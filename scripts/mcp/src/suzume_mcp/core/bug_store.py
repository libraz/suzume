"""File-backed defect store shared by the quality-check skills.

One record is one JSON file. A record never leaves the store by deletion: an
open record moves to ``resolved/`` when its defect is fixed, and a bulk retire
moves records to a timestamped directory under ``_archive/``. There is no code
path here that unlinks a record, so a mistaken bulk call is always recoverable.

Sub-directories of a store are never scanned, which keeps snapshots and the
archive out of listing, numbering, and retirement.
"""

import datetime
import json
import re
import shutil
from pathlib import Path

from ..config import PROJECT_ROOT
from .diff_utils import classify_surface_diff
from .file_utils import atomic_write_text

DEFAULT_SOURCE = "defect"
RESOLVED_DIRNAME = "resolved"
ARCHIVE_DIRNAME = "_archive"
METADATA_DIRNAME = "_metadata"
PROBE_HISTORY_FILENAME = "probe_history.json"

# Explicit per-source mapping. "defect" has no "-quality-check" suffix, so the
# name-pattern fallback below would resolve it to the wrong directory.
_SOURCE_DIRS = {
    "thread": PROJECT_ROOT / ".claude" / "skills" / "thread-quality-check" / "bugs",
    "literary": PROJECT_ROOT / ".claude" / "skills" / "literary-quality-check" / "bugs",
    "defect": PROJECT_ROOT / ".claude" / "skills" / "defect-sweep" / "bugs",
}

KNOWN_SOURCES = tuple(_SOURCE_DIRS)

# Field order used when writing a record, so a rewrite produces a stable diff.
_FIELD_ORDER = (
    "id",
    "text",
    "expected",
    "suzume",
    "diff_type",
    "kind",
    "check",
    "priority",
    "pattern",
    "description",
    "status",
    "created",
    "line_num",
    "resolution",
    "resolved_at",
    "resolved_note",
    "resolved_output",
)

PRIORITIES = ("high", "medium", "low")

# A record whose expected and observed token lists are identical differs only in
# POS/lemma, which surface comparison cannot see. Such a record is never closed
# automatically.
CHECK_SURFACE = "surface"
CHECK_MANUAL = "manual"

# Which side of the comparison is wrong. `expected` is the oracle's output and
# `suzume` is the tokenizer's, so `kind` says which of the two to believe --
# nothing else in the record needs to change.
KIND_TOKENIZER = "tokenizer"  # Suzume violates the specification; fix src/
KIND_ORACLE = "oracle"  # the normalization pipeline is wrong; fix core/
KIND_BOTH = "both"
KINDS = (KIND_TOKENIZER, KIND_ORACLE, KIND_BOTH)

# Why a closed record was closed. Only `fixed` can resurface: the others record
# a judgment about the language, which a code change does not invalidate.
RESOLUTION_FIXED = "fixed"
RESOLUTIONS = (
    RESOLUTION_FIXED,
    "conformant",  # Suzume and the oracle are both right; the diff is intended
    "known-limit",  # documented irreducible trade-off
    "ambiguous",  # several valid readings, not decidable
    "duplicate",  # folded into another root
)
DISMISSALS = tuple(res for res in RESOLUTIONS if res != RESOLUTION_FIXED)


class BugStoreError(Exception):
    """Raised for a rejected store operation (unknown source, bad id, …)."""


# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------


def store_dir(source: str = DEFAULT_SOURCE) -> Path:
    """Resolve the open-record directory for a source."""
    if source not in _SOURCE_DIRS:
        raise BugStoreError(f"Unknown source: {source!r}. Valid sources: {', '.join(KNOWN_SOURCES)}")
    return _SOURCE_DIRS[source]


def resolved_dir(source: str = DEFAULT_SOURCE) -> Path:
    """Resolve the retired-record directory for a source."""
    return store_dir(source) / RESOLVED_DIRNAME


def archive_dir(source: str = DEFAULT_SOURCE) -> Path:
    """Resolve the bulk-retirement directory for a source."""
    return store_dir(source) / ARCHIVE_DIRNAME


# ---------------------------------------------------------------------------
# Token normalization
# ---------------------------------------------------------------------------


def normalize_tokens(text: str) -> list[str]:
    """Split a token string written with either ``/`` or whitespace separators."""
    return [tok for tok in re.split(r"\s*/\s*|\s+", (text or "").strip()) if tok]


def join_tokens(tokens: list[str]) -> str:
    """Join tokens into the store's canonical space-separated form."""
    return " ".join(tokens)


def canonical_tokens(text: str) -> str:
    """Rewrite a token string into the canonical space-separated form."""
    return join_tokens(normalize_tokens(text))


def detect_check(expected: str, suzume: str) -> str:
    """Pick the resolution mode for a record from its two token strings."""
    if normalize_tokens(expected) == normalize_tokens(suzume):
        return CHECK_MANUAL
    return CHECK_SURFACE


def today() -> str:
    """Return the local date as ``YYYY-MM-DD``."""
    return datetime.date.today().isoformat()


# ---------------------------------------------------------------------------
# Reading
# ---------------------------------------------------------------------------


def _id_from_name(name: str) -> int | None:
    match = re.match(r"^(\d+)", name)
    return int(match.group(1)) if match else None


def _apply_defaults(data: dict, path: Path, status: str) -> dict:
    """Fill in v2 fields for a record, including ones written by older tools."""
    record = dict(data)
    if not isinstance(record.get("id"), int):
        fallback = _id_from_name(path.stem)
        if fallback is None:
            raise BugStoreError(f"Record has no usable id: {path.name}")
        record["id"] = fallback
    record["text"] = record.get("text", "")
    record["expected"] = canonical_tokens(record.get("expected", ""))
    record["suzume"] = canonical_tokens(record.get("suzume", ""))
    record["diff_type"] = record.get("diff_type") or classify_surface_diff(
        normalize_tokens(record["expected"]), normalize_tokens(record["suzume"])
    )
    record["check"] = record.get("check") or detect_check(record["expected"], record["suzume"])
    record["kind"] = record.get("kind") or KIND_TOKENIZER
    record["priority"] = (record.get("priority") or "").lower()
    record["pattern"] = record.get("pattern", "")
    record["description"] = record.get("description", "")
    record["status"] = status
    if status == "resolved":
        record["resolution"] = record.get("resolution") or RESOLUTION_FIXED
    record["_file"] = path.name
    record["_path"] = str(path)
    return record


def _read_record(path: Path, status: str) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as exc:
        raise BugStoreError(f"Unreadable record: {path}") from exc
    if not isinstance(data, dict):
        raise BugStoreError(f"Unreadable record: {path} (root must be an object)")
    try:
        return _apply_defaults(data, path, status)
    except BugStoreError as exc:
        raise BugStoreError(f"Unreadable record: {path} ({exc})") from exc


def _scan(directory: Path, status: str) -> list[dict]:
    """Read every record directly inside a directory. Sub-directories are skipped."""
    if not directory.is_dir():
        return []
    records = []
    unreadable: list[str] = []
    for path in directory.glob("*.json"):
        if not path.is_file():
            continue
        try:
            records.append(_read_record(path, status))
        except BugStoreError as exc:
            unreadable.append(str(exc))
    if unreadable:
        raise BugStoreError("; ".join(unreadable))
    records.sort(key=lambda rec: rec["id"])
    return records


def load_open(source: str = DEFAULT_SOURCE) -> list[dict]:
    """Load every open record, ordered by id."""
    return _scan(store_dir(source), "open")


def load_resolved(source: str = DEFAULT_SOURCE) -> list[dict]:
    """Load every retired record, ordered by id."""
    return _scan(resolved_dir(source), "resolved")


def load_one(source: str, bug_id: int, include_resolved: bool = True) -> dict | None:
    """Find a single record by id."""
    loaders = (load_open, load_resolved) if include_resolved else (load_open,)
    return next((record for load in loaders for record in load(source) if record["id"] == bug_id), None)


def find_by_text(source: str, text: str, include_resolved: bool = True) -> dict | None:
    """Find a record registered for the same input text."""
    needle = (text or "").strip()
    if not needle:
        return None
    pool = load_open(source) + (load_resolved(source) if include_resolved else [])
    for record in pool:
        if (record.get("text") or "").strip() == needle:
            return record
    return None


def next_id(source: str = DEFAULT_SOURCE) -> int:
    """Return the next free id.

    Numbering reads the ``id`` field of every record the store has ever held --
    open, resolved, and archived. Deriving it from sorted file names instead
    breaks as soon as ids reach a wider digit count, because ``1000_x.json``
    sorts before ``999_x.json``.
    """
    highest = 0
    directories = [store_dir(source), resolved_dir(source)]
    archive = archive_dir(source)
    if archive.is_dir():
        directories.extend(path for path in archive.iterdir() if path.is_dir())
    for directory in directories:
        if not directory.is_dir():
            continue
        for path in directory.glob("*.json"):
            record_id = None
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
                if isinstance(data, dict) and isinstance(data.get("id"), int):
                    record_id = data["id"]
            except (json.JSONDecodeError, OSError):
                record_id = None
            if record_id is None:
                record_id = _id_from_name(path.stem)
            if record_id is not None:
                highest = max(highest, record_id)
    return highest + 1


# ---------------------------------------------------------------------------
# Writing
# ---------------------------------------------------------------------------


def record_filename(record: dict) -> str:
    """Build the on-disk name for a record."""
    diff_type = record.get("diff_type") or "unknown"
    return f"{record['id']:04d}_{diff_type}.json"


def _serialize(record: dict) -> str:
    ordered = {key: record[key] for key in _FIELD_ORDER if key in record and record[key] not in ("", None)}
    # Preserve any field a future tool added without dropping it on rewrite.
    for key, value in record.items():
        if key.startswith("_") or key in ordered:
            continue
        if value in ("", None):
            continue
        ordered[key] = value
    return json.dumps(ordered, ensure_ascii=False, indent=2) + "\n"


def write_record(source: str, record: dict, directory: Path | None = None) -> Path:
    """Write a record, replacing any earlier file for the same id."""
    target_dir = directory or store_dir(source)
    target_dir.mkdir(parents=True, exist_ok=True)
    path = target_dir / record_filename(record)
    stale = target_dir / record.get("_file", "")
    atomic_write_text(path, _serialize(record))
    if record.get("_file") and stale.is_file() and stale != path:
        stale.unlink()
    return path


def _persist(source: str, record: dict, directory: Path | None = None) -> dict:
    """Write a record and point its private location fields at the written file."""
    path = write_record(source, record, directory)
    record["_file"] = path.name
    record["_path"] = str(path)
    return record


def _move(source: str, record: dict, moved: dict, directory: Path) -> Path:
    """Write the moved copy of a record, then remove the file it came from."""
    path = write_record(source, moved, directory)
    origin = Path(record["_path"])
    if origin.is_file() and origin != path:
        origin.unlink()
    return path


def _build(
    source: str,
    text: str,
    expected: str,
    suzume: str,
    diff_type: str,
    check: str,
    kind: str,
    priority: str,
    pattern: str,
    description: str,
    line_num: int,
) -> dict:
    expected_canonical = canonical_tokens(expected)
    suzume_canonical = canonical_tokens(suzume)
    record = {
        "id": next_id(source),
        "text": text,
        "expected": expected_canonical,
        "suzume": suzume_canonical,
        "diff_type": diff_type
        or classify_surface_diff(normalize_tokens(expected_canonical), normalize_tokens(suzume_canonical)),
        "kind": kind or KIND_TOKENIZER,
        "check": check or detect_check(expected_canonical, suzume_canonical),
        "priority": (priority or "").lower(),
        "pattern": pattern,
        "description": description,
        "created": today(),
    }
    if line_num > 0:
        record["line_num"] = line_num
    return record


def create(
    source: str,
    text: str,
    expected: str,
    suzume: str,
    diff_type: str = "",
    check: str = "",
    kind: str = "",
    priority: str = "",
    pattern: str = "",
    description: str = "",
    line_num: int = 0,
    record_id: int | None = None,
) -> dict:
    """Build and persist a new open record, assigning its id."""
    record = _build(source, text, expected, suzume, diff_type, check, kind, priority, pattern, description, line_num)
    if record_id is not None:
        record["id"] = record_id
    record["status"] = "open"
    return _persist(source, record)


def dismiss(
    source: str,
    text: str,
    expected: str,
    suzume: str,
    resolution: str,
    reason: str,
    pattern: str,
    diff_type: str = "",
    kind: str = "",
) -> dict:
    """Persist an adjudicated non-defect straight into ``resolved/``.

    A dismissal is an ordinary record, which is what lets the store double as
    the ledger of "already judged, not a defect" examples: a later probe of the
    same text finds it and never sends it back for judgment.
    """
    if resolution not in DISMISSALS:
        raise BugStoreError(f"resolution must be one of {DISMISSALS} (use resolve() for {RESOLUTION_FIXED})")
    if not (reason or "").strip():
        raise BugStoreError("reason is required — it is the whole value of a dismissal")
    if not (pattern or "").strip():
        raise BugStoreError("pattern is required — dismissals are the denominator of the per-family yield")

    record = _build(source, text, expected, suzume, diff_type, "", kind, "", pattern, reason, 0)
    record["status"] = "resolved"
    record["resolution"] = resolution
    record["resolved_at"] = today()
    record["resolved_note"] = reason
    return _persist(source, record, resolved_dir(source))


def update(source: str, record: dict, changes: dict) -> dict:
    """Apply field changes to an existing record and rewrite it."""
    updated = dict(record)
    updated.update({key: value for key, value in changes.items() if value is not None})
    if changes.get("expected") is not None or changes.get("suzume") is not None:
        expected = canonical_tokens(updated["expected"])
        suzume = canonical_tokens(updated["suzume"])
        updated["expected"] = expected
        updated["suzume"] = suzume
        if changes.get("diff_type") is None:
            updated["diff_type"] = classify_surface_diff(normalize_tokens(expected), normalize_tokens(suzume))
        if changes.get("check") is None:
            updated["check"] = detect_check(expected, suzume)
    return _persist(source, updated, Path(record["_path"]).parent)


def resolve(source: str, record: dict, note: str, output: str) -> Path:
    """Move a record into ``resolved/`` as fixed, recording how it was closed."""
    retired = dict(record)
    retired["status"] = "resolved"
    retired["resolution"] = RESOLUTION_FIXED
    retired["resolved_at"] = today()
    if note:
        retired["resolved_note"] = note
    if output:
        retired["resolved_output"] = canonical_tokens(output)
    return _move(source, record, retired, resolved_dir(source))


def reopen(source: str, record: dict) -> Path:
    """Move a retired record back into the open set."""
    restored = dict(record)
    restored["status"] = "open"
    for key in ("resolution", "resolved_at", "resolved_note", "resolved_output"):
        restored.pop(key, None)
    return _move(source, record, restored, store_dir(source))


def archive(source: str, stamp: str) -> tuple[Path, int]:
    """Move every open record into a timestamped archive directory."""
    records = load_open(source)
    target = archive_dir(source) / stamp
    target.mkdir(parents=True, exist_ok=True)
    for record in records:
        shutil.move(record["_path"], str(target / Path(record["_path"]).name))
    return target, len(records)


# ---------------------------------------------------------------------------
# Probe history
# ---------------------------------------------------------------------------


def _probe_history_path(source: str) -> Path:
    """Return the metadata path that records completed family probes."""
    return store_dir(source) / METADATA_DIRNAME / PROBE_HISTORY_FILENAME


def _load_probe_history(source: str) -> dict[str, str]:
    """Read per-family probe timestamps, rejecting malformed scheduler state."""
    path = _probe_history_path(source)
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as exc:
        raise BugStoreError(f"Unreadable probe history: {path}") from exc
    if not isinstance(data, dict) or not all(
        isinstance(key, str) and isinstance(value, str) for key, value in data.items()
    ):
        raise BugStoreError(f"Unreadable probe history: {path}")
    return data


def record_probe(source: str, pattern: str, timestamp: str | None = None) -> None:
    """Persist the time a labelled probe family was checked, even when it matched."""
    label = (pattern or "").strip()
    if not label:
        return
    history = _load_probe_history(source)
    history[label] = timestamp or datetime.datetime.now().astimezone().isoformat(timespec="seconds")
    atomic_write_text(
        _probe_history_path(source), json.dumps(history, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    )


# ---------------------------------------------------------------------------
# Summaries and report rendering
# ---------------------------------------------------------------------------


def yield_by_pattern(open_records: list[dict], resolved_records: list[dict], source: str | None = None) -> dict:
    """Report the per-family hit rate that drives the next round's allocation.

    Both halves of a judgment land in the store — a defect as an open record, a
    non-defect as a dismissal — so the denominator needs no separate ledger.
    Records closed as ``fixed`` were defects and count toward ``filed``.
    """
    families: dict[str, dict] = {}

    def bucket(pattern: str) -> dict:
        return families.setdefault(
            pattern or "(none)",
            {
                "judged": 0,
                "filed": 0,
                "open": 0,
                "dismissed": 0,
                "kinds": {},
                "resolutions": {},
                "last_probed": "",
            },
        )

    for record in open_records:
        entry = bucket(record.get("pattern", ""))
        entry["judged"] += 1
        entry["filed"] += 1
        entry["open"] += 1
        kind = record.get("kind") or KIND_TOKENIZER
        entry["kinds"][kind] = entry["kinds"].get(kind, 0) + 1
        entry["last_probed"] = max(entry["last_probed"], record.get("created", ""))

    for record in resolved_records:
        entry = bucket(record.get("pattern", ""))
        entry["judged"] += 1
        resolution = record.get("resolution") or RESOLUTION_FIXED
        entry["resolutions"][resolution] = entry["resolutions"].get(resolution, 0) + 1
        if resolution == RESOLUTION_FIXED:
            entry["filed"] += 1
            kind = record.get("kind") or KIND_TOKENIZER
            entry["kinds"][kind] = entry["kinds"].get(kind, 0) + 1
        else:
            entry["dismissed"] += 1
        entry["last_probed"] = max(entry["last_probed"], record.get("created", ""))

    if source is not None:
        for pattern, timestamp in _load_probe_history(source).items():
            if pattern in families:
                families[pattern]["last_probed"] = max(families[pattern]["last_probed"], timestamp)

    for entry in families.values():
        entry["hit_rate"] = round(entry["filed"] / entry["judged"], 3) if entry["judged"] else 0.0

    return dict(sorted(families.items(), key=lambda item: (-item[1]["hit_rate"], item[0])))


def summarize(records: list[dict]) -> dict:
    """Count records by pattern, diff type, priority, and resolution mode."""
    patterns: dict[str, int] = {}
    diff_types: dict[str, int] = {}
    priorities: dict[str, int] = {}
    kinds: dict[str, int] = {}
    manual = 0
    for record in records:
        kind = record.get("kind") or KIND_TOKENIZER
        kinds[kind] = kinds.get(kind, 0) + 1
        pattern = record.get("pattern") or "(none)"
        patterns[pattern] = patterns.get(pattern, 0) + 1
        diff_type = record.get("diff_type") or "unknown"
        diff_types[diff_type] = diff_types.get(diff_type, 0) + 1
        priority = record.get("priority") or "(none)"
        priorities[priority] = priorities.get(priority, 0) + 1
        if record.get("check") == CHECK_MANUAL:
            manual += 1
    return {
        "total": len(records),
        "patterns": dict(sorted(patterns.items())),
        "diff_types": dict(sorted(diff_types.items())),
        "priorities": dict(sorted(priorities.items())),
        "kinds": dict(sorted(kinds.items())),
        "manual": manual,
    }


def _cell(text: str, limit: int = 0) -> str:
    """Escape a value for a Markdown table cell."""
    value = (text or "").replace("|", "\\|").replace("\n", " ").strip()
    if limit and len(value) > limit:
        value = value[: limit - 1] + "…"
    return value or "-"


def _first_line(text: str, limit: int = 80) -> str:
    return _cell((text or "").split("\n")[0], limit)


# States a recheck can assign, in the order the report presents them: what needs
# acting on first comes first.
REPORT_STATE_ORDER = ("error", "resolved", "stale", "needs-manual", "open")

_STATE_NOTE = {
    "error": "the CLI or the oracle failed — investigate before planning around it",
    "resolved": "current output matches the oracle — close with `defect_resolve` and a note",
    "stale": "still wrong, but the output moved since it was filed — re-triage from `current`",
    "needs-manual": "POS/lemma only — surface comparison cannot judge it, reproduce by hand",
    "open": "still wrong, unchanged since it was filed",
}


def render_report(
    source: str,
    records: list[dict],
    live: dict[int, dict],
    resolved_count: int,
    date: str,
) -> str:
    """Render the deterministic Markdown report for a set of records.

    ``live`` maps a record id to ``{"current": str, "state": str}`` as produced
    by a recheck. Records sort by pattern name then id, so the same store always
    renders byte-identical output.
    """
    ordered = sorted(records, key=lambda rec: ((rec.get("pattern") or "~"), rec["id"]))
    summary = summarize(ordered)

    def state_of(record: dict) -> str:
        return live.get(record["id"], {}).get("state") or "open"

    lines: list[str] = []
    lines.append(f"# Defect store report ({date}) — open {len(ordered)} / resolved {resolved_count}")
    lines.append("")
    lines.append(f'Generated by `defect_report(source="{source}")`. Overwritten on every run — do not hand-edit.')
    lines.append("Every record was re-run for this report, so `current` supersedes the output stored with it.")
    lines.append("")
    lines.append("## State on the current build")
    lines.append("")
    lines.append("| state | n | ids | meaning |")
    lines.append("|---|---|---|---|")
    for state in REPORT_STATE_ORDER:
        group = [rec for rec in ordered if state_of(rec) == state]
        if not group:
            continue
        ids = " ".join(f"#{record_id}" for record_id in sorted(rec["id"] for rec in group))
        lines.append(f"| {state} | {len(group)} | {_cell(ids, 120)} | {_STATE_NOTE[state]} |")
    lines.append("")
    lines.append("## Breakdown")
    lines.append("")
    lines.append("| axis | counts |")
    lines.append("|---|---|")
    priority_counts = " / ".join(f"{level} {summary['priorities'].get(level, 0)}" for level in (*PRIORITIES, "(none)"))
    lines.append(f"| priority | {priority_counts} |")
    lines.append("| kind | " + " / ".join(f"{key} {value}" for key, value in summary["kinds"].items()) + " |")
    lines.append("| diff type | " + " / ".join(f"{key} {value}" for key, value in summary["diff_types"].items()) + " |")
    shared = {key: value for key, value in summary["patterns"].items() if value > 1}
    lines.append(
        "| patterns holding more than one record | "
        + (" / ".join(f"{key} {value}" for key, value in shared.items()) if shared else "none")
        + " |"
    )
    lines.append(f"| singleton patterns | {len(summary['patterns']) - len(shared)} |")

    oracle_side = [rec for rec in ordered if rec.get("kind") in (KIND_ORACLE, KIND_BOTH)]
    if oracle_side:
        lines.append("")
        lines.append("## Oracle-side records")
        lines.append("")
        lines.append(
            "These are fixed in `scripts/mcp/src/suzume_mcp/core/`, not in `src/`, so they belong in their own "
            "work package. `both` needs a change on each side."
        )
        lines.append("")
        lines.append("| ID | kind | pattern | text | oracle says | Suzume says | note |")
        lines.append("|---|---|---|---|---|---|---|")
        for record in oracle_side:
            current = live.get(record["id"], {}).get("current") or record.get("suzume", "")
            lines.append(
                f"| #{record['id']} | {record.get('kind')} | {_cell(record.get('pattern', ''))} | "
                f"{_cell(record.get('text', ''), 40)} | {_cell(record.get('expected', ''), 60)} | "
                f"{_cell(current, 60)} | {_first_line(record.get('description', ''))} |"
            )

    lines.append("")
    lines.append("## Open records")
    lines.append("")
    lines.append("`chk`: s = a surface comparison can close it, **m** = POS/lemma only, never closed automatically.")
    lines.append("`kind`: which side is wrong — `tokenizer` (fix `src/`), `oracle` (fix `core/`), or `both`.")

    current_pattern = None
    for record in ordered:
        pattern = record.get("pattern") or "(none)"
        if pattern != current_pattern:
            current_pattern = pattern
            group_size = sum(1 for rec in ordered if (rec.get("pattern") or "(none)") == pattern)
            lines.append("")
            lines.append(f"### {pattern} ({group_size})")
            lines.append("")
            lines.append("| ID | pri | kind | state | text | expected | current | diff | chk | note |")
            lines.append("|---|---|---|---|---|---|---|---|---|---|")
        info = live.get(record["id"], {})
        current = info.get("current") or record.get("suzume", "")
        check_mark = "**m**" if record.get("check") == CHECK_MANUAL else "s"
        priority = (record.get("priority") or "-")[:3]
        kind = record.get("kind") or KIND_TOKENIZER
        kind_cell = "tok" if kind == KIND_TOKENIZER else f"**{kind}**"
        lines.append(
            f"| #{record['id']} | {priority} | {kind_cell} | {state_of(record)} | "
            f"{_cell(record.get('text', ''), 40)} | "
            f"{_cell(record.get('expected', ''), 60)} | {_cell(current, 60)} | "
            f"{_cell(record.get('diff_type', ''))} | {check_mark} | {_first_line(record.get('description', ''))} |"
        )

    lines.append("")
    return "\n".join(lines) + "\n"
