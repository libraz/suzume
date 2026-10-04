"""Defect store MCP tools.

These tools own the backlog of confirmed tokenizer defects. None of them delete
a record: closing one moves it to ``resolved/`` and retiring a whole store moves
its records to ``_archive/<timestamp>/``, so any mistaken bulk call stays
recoverable. See ``core/bug_store.py`` for the on-disk contract.
"""

import asyncio
import datetime
from pathlib import Path

from ..core import bug_store
from ..core.file_utils import atomic_write_text
from ..core.json_utils import json_error
from ..core.json_utils import json_result as _json_result
from ..core.suzume_cli import get_expected_tokens_batch_subprocess, get_suzume_surfaces_async
from ..server import PROJECT_ROOT, mcp

# Number of CLI processes to keep in flight while rechecking a store.
_RECHECK_CONCURRENCY = 8


def _parse_ids(ids: str) -> list[int]:
    """Parse an explicit id list. Wildcards and bulk keywords are rejected."""
    raw = (ids or "").strip()
    if not raw:
        raise ValueError('ids is required — list the record ids explicitly (e.g. "219,942")')
    if raw.lower() in ("all", "*", "any"):
        raise ValueError('ids must name individual records; "all" is not accepted')
    parsed = []
    for chunk in raw.replace(",", " ").split():
        chunk = chunk.lstrip("#")
        if not chunk.isdigit():
            raise ValueError(f"Not a record id: {chunk!r}")
        parsed.append(int(chunk))
    if not parsed:
        raise ValueError("ids is required — list the record ids explicitly")
    return parsed


async def _current_outputs(texts: list[str]) -> list[list[str]]:
    """Run the CLI over many texts with bounded concurrency."""
    semaphore = asyncio.Semaphore(_RECHECK_CONCURRENCY)

    async def one(text: str) -> list[str]:
        async with semaphore:
            # user.dic stays loaded so dict_add fixes count as resolutions.
            return await get_suzume_surfaces_async(text, skip_user_dict=False)

    return await asyncio.gather(*(one(text) for text in texts), return_exceptions=True)


async def _compare_texts(texts: list[str]) -> list[dict]:
    """Run the oracle once and the CLI concurrently; one {error} or {current, expected} per text."""
    oracle = get_expected_tokens_batch_subprocess(texts)
    outputs = await _current_outputs(texts)
    rows = []
    for (tokens, source, rule), output in zip(oracle, outputs, strict=True):
        if source == "error":
            rows.append({"error": rule})
        elif isinstance(output, BaseException):
            rows.append({"error": str(output)})
        else:
            rows.append(
                {
                    "current": bug_store.join_tokens(list(output)),
                    "expected": bug_store.join_tokens([tok["surface"] for tok in tokens]),
                }
            )
    return rows


async def _recheck_records(records: list[dict]) -> dict[int, dict]:
    """Re-run every record and classify its current state.

    States:
        resolved     — current output matches the current oracle
        needs-manual — POS/lemma-only record; surface comparison cannot close it
        stale        — still wrong, but the output moved since the record was filed
        open         — still wrong, unchanged
        error        — the CLI or the oracle failed for this text
    """
    if not records:
        return {}
    try:
        compared = await _compare_texts([record.get("text", "") for record in records])
    except Exception as exc:
        return {record["id"]: {"state": "error", "error": str(exc)} for record in records}

    results: dict[int, dict] = {}
    for record, row in zip(records, compared, strict=True):
        if "error" in row:
            results[record["id"]] = {"state": "error", "error": row["error"]}
            continue
        current = row["current"]
        if record.get("check") == bug_store.CHECK_MANUAL:
            state = "needs-manual"
        elif current == row["expected"]:
            state = "resolved"
        elif current != record.get("suzume", ""):
            state = "stale"
        else:
            state = "open"
        results[record["id"]] = {"state": state, "current": current, "oracle": row["expected"]}
    return results


def _entry(record: dict, compact: bool = True) -> dict:
    """Shape a record for a tool response."""
    entry = {
        "id": record["id"],
        "text": record.get("text", ""),
        "expected": record.get("expected", ""),
        "suzume": record.get("suzume", ""),
        "diff_type": record.get("diff_type", ""),
        "kind": record.get("kind", bug_store.KIND_TOKENIZER),
        "check": record.get("check", ""),
        "status": record.get("status", "open"),
    }
    for key in ("priority", "pattern"):
        if record.get(key):
            entry[key] = record[key]
    if compact:
        description = record.get("description", "")
        if description:
            entry["note"] = description.split("\n")[0][:120]
    else:
        for key in (
            "description",
            "status",
            "created",
            "line_num",
            "resolution",
            "resolved_at",
            "resolved_note",
        ):
            if record.get(key):
                entry[key] = record[key]
        entry["file"] = record.get("_file", "")
    return entry


async def _screen(texts: list[str], source: str) -> list[dict]:
    """Compare many texts at once and say what the store already knows.

    One oracle subprocess plus concurrent CLI runs, so a whole family of
    generated sentences costs a single call.
    """
    compared = await _compare_texts(texts)

    open_by_text = {rec.get("text", ""): rec for rec in bug_store.load_open(source)}
    closed_by_text = {rec.get("text", ""): rec for rec in bug_store.load_resolved(source)}

    rows: list[dict] = []
    for text, compare in zip(texts, compared, strict=True):
        if "error" in compare:
            rows.append({"text": text, "state": "error", "error": compare["error"]})
            continue
        current, expected = compare["current"], compare["expected"]
        if current == expected:
            rows.append({"text": text, "state": "match"})
            continue

        row = {"text": text, "state": "candidate", "expected": expected, "suzume": current}
        row["token_delta"] = len(bug_store.normalize_tokens(current)) - len(bug_store.normalize_tokens(expected))

        known = open_by_text.get(text)
        closed = closed_by_text.get(text)
        if known is not None:
            row.update({"state": "known", "id": known["id"], "kind": known.get("kind", "")})
        elif closed is not None:
            resolution = closed.get("resolution", bug_store.RESOLUTION_FIXED)
            if resolution == bug_store.RESOLUTION_FIXED:
                # It was fixed once and differs again: worth judging, not skipping.
                row.update({"state": "regression", "id": closed["id"], "fixed_at": closed.get("resolved_at", "")})
            else:
                row.update(
                    {
                        "state": "dismissed",
                        "id": closed["id"],
                        "resolution": resolution,
                        "reason": (closed.get("resolved_note") or "")[:160],
                    }
                )
        rows.append(row)
    return rows


async def _derive_tokenizations(text: str, expected: str, suzume: str) -> tuple[str, str]:
    """Fill an omitted oracle or observed tokenization from the pipeline and the CLI."""
    if not expected:
        tokens, oracle_source, rule = get_expected_tokens_batch_subprocess([text])[0]
        if oracle_source == "error":
            raise RuntimeError(rule)
        expected = bug_store.join_tokens([tok["surface"] for tok in tokens])
    if not suzume:
        suzume = bug_store.join_tokens(list(await get_suzume_surfaces_async(text, skip_user_dict=False)))
    return expected, suzume


def _duplicate(existing: dict, verb: str) -> str:
    return _json_result(
        {
            "status": "duplicate",
            "message": f"Already {verb} as #{existing['id']} ({existing['status']})",
            "existing": _entry(existing, compact=False),
        }
    )


def _load_by_ids(source: str, wanted: list[int], include_resolved: bool = True) -> tuple[list[dict], list[int]]:
    """Load the named records in request order, listing the ids that do not exist."""
    records = []
    missing = []
    for bug_id in wanted:
        record = bug_store.load_one(source, bug_id, include_resolved=include_resolved)
        if record is None:
            missing.append(bug_id)
        else:
            records.append(record)
    return records, missing


# ---------------------------------------------------------------------------
# MCP tools
# ---------------------------------------------------------------------------


@mcp.tool()
async def defect_probe(
    texts: list[str],
    source: str = "defect",
    pattern: str = "",
    include_match: bool = False,
) -> str:
    """Run many sentences through the oracle and the tokenizer in one call.

    This is the cheap end of the sweep: generate a family of sentences, probe
    them here, and pass only what comes back as `candidate` or `regression` on
    for judgment. Everything the store has already seen is filtered out.

    States: `candidate` (differs, unknown), `regression` (differs again after
    being fixed), `known` (an open record exists), `dismissed` (judged and
    closed as a non-defect), `match` (no difference), `error`.

    Args:
        texts: Sentences to probe. At most 200 per call.
        source: Store to screen against.
        pattern: Grammar family these sentences came from; echoed back on every
            row so the caller can file without re-tagging.
        include_match: Also return the sentences that showed no difference.
    """
    if not texts:
        return json_error("texts is required")
    if len(texts) > 200:
        return json_error(f"{len(texts)} texts exceeds the 200-per-call limit — split the batch")

    try:
        rows = await _screen([text for text in texts if (text or "").strip()], source)
    except Exception as exc:
        return json_error(str(exc))

    if pattern:
        try:
            bug_store.record_probe(source, pattern)
        except bug_store.BugStoreError as exc:
            return json_error(str(exc))
        for row in rows:
            row["pattern"] = pattern
    counts: dict[str, int] = {}
    for row in rows:
        counts[row["state"]] = counts.get(row["state"], 0) + 1

    returned = [row for row in rows if include_match or row["state"] != "match"]
    return _json_result(
        {
            "status": "ok",
            "source": source,
            "probed": len(rows),
            "counts": counts,
            "to_judge": counts.get("candidate", 0) + counts.get("regression", 0),
            "rows": returned,
        }
    )


@mcp.tool()
async def defect_dismiss(
    text: str,
    resolution: str,
    reason: str,
    pattern: str,
    expected: str = "",
    suzume: str = "",
    kind: str = "",
    source: str = "defect",
) -> str:
    """Record an adjudicated non-defect so it is never re-judged.

    The record goes straight into `resolved/` without ever being open, which is
    what lets the store double as the ledger of already-judged examples: a later
    `defect_probe` of the same text comes back as `dismissed`.

    Args:
        text: The sentence that was judged.
        resolution: conformant / known-limit / ambiguous / duplicate. A fixed
            defect is closed with `defect_resolve`, not here.
        reason: Why it is not a defect. Required — it is the whole value of the record.
        pattern: Grammar family. Required: dismissals are the denominator of the
            per-family yield that drives the next round's allocation.
        expected: Oracle tokenization. Derived when omitted.
        suzume: Observed tokenization. Derived when omitted.
        kind: Optional, when the judgment still identified a side.
        source: Store to write to.
    """
    if not (text or "").strip():
        return json_error("text is required")
    if kind and kind not in bug_store.KINDS:
        return json_error(f"kind must be one of {bug_store.KINDS}")
    try:
        existing = bug_store.find_by_text(source, text)
        if existing is not None:
            return _duplicate(existing, "recorded")
        expected, suzume = await _derive_tokenizations(text, expected, suzume)
        record = bug_store.dismiss(
            source,
            text=text,
            expected=expected,
            suzume=suzume,
            resolution=resolution,
            reason=reason,
            pattern=pattern,
            kind=kind,
        )
    except Exception as exc:
        return json_error(str(exc))

    return _json_result(
        {
            "status": "ok",
            "id": record["id"],
            "file": record["_file"],
            "resolution": record["resolution"],
            "message": "Recorded as judged; defect_probe will skip this text from now on. defect_reopen undoes it.",
        }
    )


@mcp.tool()
async def defect_yield(source: str = "defect", min_judged: int = 0) -> str:
    """Report the per-family hit rate that should drive the next round.

    Both outcomes of a judgment are in the store — a defect as an open record, a
    non-defect as a dismissal — so this needs no separate bookkeeping.

    Allocate the next round from this: most of it to the families with the
    highest `hit_rate` that are not already saturated with open records, and a
    standing minority to the families whose latest stored record is oldest and
    the families with no rows at all, because a mined-out family's rate falls
    on its own.

    Args:
        source: Store to read.
        min_judged: Hide families with fewer than this many judged sentences.
    """
    try:
        families = bug_store.yield_by_pattern(bug_store.load_open(source), bug_store.load_resolved(source), source)
    except Exception as exc:
        return json_error(str(exc))

    if min_judged > 0:
        families = {key: value for key, value in families.items() if value["judged"] >= min_judged}

    judged = sum(entry["judged"] for entry in families.values())
    filed = sum(entry["filed"] for entry in families.values())
    return _json_result(
        {
            "status": "ok",
            "source": source,
            "families": families,
            "totals": {
                "families": len(families),
                "judged": judged,
                "filed": filed,
                "dismissed": sum(entry["dismissed"] for entry in families.values()),
                "hit_rate": round(filed / judged, 3) if judged else 0.0,
            },
        }
    )


@mcp.tool()
async def defect_add(
    text: str,
    expected: str = "",
    suzume: str = "",
    pattern: str = "",
    description: str = "",
    priority: str = "",
    kind: str = "tokenizer",
    line_num: int = 0,
    source: str = "defect",
    force: bool = False,
) -> str:
    """Register a defect root in the store.

    Duplicates are rejected: a record already filed for the same text is
    returned instead of a second one being created.

    Args:
        text: Input sentence that reproduces the defect.
        expected: Oracle tokenization. Derived from the pipeline when omitted.
        suzume: Observed tokenization. Derived from the CLI when omitted.
        pattern: Grammar-family label used for retrieval, report grouping, and yield.
        description: Priority reasoning, inferred layer, distinction from nearby roots.
        priority: high / medium / low.
        kind: Which side is wrong — "tokenizer" (fix src/), "oracle" (fix the
            normalization pipeline in core/), or "both".
        line_num: Corpus line number, when the record came from a scan.
        source: Store to write to — "defect", "thread", or "literary".
        force: Register even when a record already exists for this text.
    """
    if not (text or "").strip():
        return json_error("text is required")
    if kind and kind not in bug_store.KINDS:
        return json_error(f"kind must be one of {bug_store.KINDS}")
    try:
        existing = bug_store.find_by_text(source, text)
        if existing and not force:
            return _duplicate(existing, "filed")

        expected, suzume = await _derive_tokenizations(text, expected, suzume)
        if priority and priority.lower() not in bug_store.PRIORITIES:
            return json_error(f"priority must be one of {bug_store.PRIORITIES}")

        record = bug_store.create(
            source,
            text=text,
            expected=expected,
            suzume=suzume,
            kind=kind,
            priority=priority,
            pattern=pattern,
            description=description,
            line_num=line_num,
        )
    except Exception as exc:
        return json_error(str(exc))

    return _json_result(
        {
            "status": "ok",
            "id": record["id"],
            "file": record["_file"],
            "diff_type": record["diff_type"],
            "kind": record["kind"],
            "check": record["check"],
            "open_total": len(bug_store.load_open(source)),
        }
    )


@mcp.tool()
async def defect_list(
    source: str = "defect",
    pattern: str = "",
    diff_type: str = "",
    priority: str = "",
    check: str = "",
    kind: str = "",
    status: str = "open",
    resolution: str = "",
    limit: int = 50,
    offset: int = 0,
    detail: bool = False,
) -> str:
    """List defect records, ordered by id.

    Counts always cover the whole store; filters and paging affect the returned
    rows only.

    Args:
        source: Store to read — "defect", "thread", or "literary".
        pattern: Filter by grammar-family label.
        diff_type: Filter by over-split / under-split / boundary / minor.
        priority: Filter by high / medium / low.
        check: Filter by "surface" or "manual".
        kind: Filter by "tokenizer", "oracle", or "both".
        status: "open" (default), "resolved", or "all".
        resolution: For closed records — fixed / conformant / known-limit /
            ambiguous / duplicate. Implies status="resolved".
        limit: Maximum rows to return, counted from `offset`.
        offset: Number of matching rows to skip.
        detail: Return full records instead of compact rows.
    """
    try:
        open_records = bug_store.load_open(source)
        closed_records = bug_store.load_resolved(source)
    except Exception as exc:
        return json_error(str(exc))

    if resolution:
        status = "resolved"
    if status == "resolved":
        records = closed_records
    elif status == "all":
        records = sorted(open_records + closed_records, key=lambda record: record["id"])
    else:
        records = open_records

    summary = bug_store.summarize(records)
    filtered = records
    if pattern:
        filtered = [rec for rec in filtered if rec.get("pattern", "") == pattern]
    if diff_type:
        filtered = [rec for rec in filtered if rec.get("diff_type", "") == diff_type]
    if priority:
        filtered = [rec for rec in filtered if rec.get("priority", "") == priority.lower()]
    if check:
        filtered = [rec for rec in filtered if rec.get("check", "") == check]
    if kind:
        filtered = [rec for rec in filtered if rec.get("kind", bug_store.KIND_TOKENIZER) == kind]
    if resolution:
        filtered = [rec for rec in filtered if rec.get("resolution", "") == resolution]

    window = filtered[offset : offset + limit] if limit > 0 else filtered[offset:]
    result = {
        "source": source,
        "status": status,
        "summary": summary,
        "open_total": len(open_records),
        "resolved_total": len(closed_records),
        "matched": len(filtered),
        "offset": offset,
        "returned": len(window),
        "records": [_entry(rec, compact=not detail) for rec in window],
    }
    return _json_result(result)


@mcp.tool()
async def defect_get(ids: str, source: str = "defect", live: bool = True) -> str:
    """Show full records by id, optionally with their current behavior.

    Args:
        ids: Record ids, comma- or space-separated (e.g. "219,942").
        source: Store to read.
        live: Re-run each record and include its current output and state.
    """
    try:
        wanted = _parse_ids(ids)
    except ValueError as exc:
        return json_error(str(exc))

    records, missing = _load_by_ids(source, wanted)
    states = await _recheck_records([rec for rec in records if rec["status"] == "open"]) if live else {}
    entries = []
    for record in records:
        entry = _entry(record, compact=False)
        if record["id"] in states:
            entry.update(states[record["id"]])
        entries.append(entry)

    result: dict = {"source": source, "records": entries}
    if missing:
        result["missing"] = missing
    return _json_result(result)


@mcp.tool()
async def defect_update(
    id: int,
    source: str = "defect",
    pattern: str = "",
    description: str = "",
    priority: str = "",
    kind: str = "",
    check: str = "",
    expected: str = "",
) -> str:
    """Amend an open record. `id` and `text` are immutable.

    Args:
        id: Record id.
        source: Store to read.
        pattern: New grammar-family label.
        description: New description (replaces the previous one).
        priority: high / medium / low.
        kind: tokenizer / oracle / both.
        check: "surface" or "manual" — force the resolution mode.
        expected: Corrected expected tokenization.
    """
    record = bug_store.load_one(source, id, include_resolved=False)
    if record is None:
        return json_error(f"No open record #{id} in {source}")
    if priority and priority.lower() not in bug_store.PRIORITIES:
        return json_error(f"priority must be one of {bug_store.PRIORITIES}")
    if kind and kind not in bug_store.KINDS:
        return json_error(f"kind must be one of {bug_store.KINDS}")
    if check and check not in (bug_store.CHECK_SURFACE, bug_store.CHECK_MANUAL):
        return json_error('check must be "surface" or "manual"')

    changes = {
        "pattern": pattern or None,
        "description": description or None,
        "priority": priority.lower() if priority else None,
        "kind": kind or None,
        "check": check or None,
        "expected": bug_store.canonical_tokens(expected) if expected else None,
    }
    if not any(value is not None for value in changes.values()):
        return json_error("No fields to update")

    updated = bug_store.update(source, record, changes)
    return _json_result({"status": "ok", "record": _entry(updated, compact=False)})


@mcp.tool()
async def defect_recheck(ids: str = "", source: str = "defect", apply: bool = False) -> str:
    """Re-run open records against the current build and report their state.

    Read-only unless `apply` is set. With `apply=True`, records whose surfaces
    now match the oracle move to `resolved/`; records marked `check="manual"`
    are never closed this way, because surface comparison cannot see a POS or
    lemma defect.

    Args:
        ids: Restrict to these ids. Empty means the whole store.
        source: Store to recheck.
        apply: Retire the records that came back resolved.
    """
    try:
        records = bug_store.load_open(source)
        missing: list[int] = []
        if ids:
            wanted = _parse_ids(ids)
            by_id = {record["id"]: record for record in records}
            missing = [bug_id for bug_id in wanted if bug_id not in by_id]
            records = [by_id[bug_id] for bug_id in wanted if bug_id in by_id]
    except (ValueError, bug_store.BugStoreError) as exc:
        return json_error(str(exc))

    states = await _recheck_records(records)
    buckets: dict[str, list[dict]] = {}
    for record in records:
        info = states.get(record["id"], {"state": "error", "error": "not evaluated"})
        row = {"id": record["id"], "text": record.get("text", "")}
        if info.get("current") is not None:
            row["current"] = info.get("current", "")
        if info["state"] in ("stale", "open"):
            row["filed_as"] = record.get("suzume", "")
            row["oracle"] = info.get("oracle", "")
        if info.get("error"):
            row["error"] = info["error"]
        buckets.setdefault(info["state"], []).append(row)

    retired: list[int] = []
    if apply:
        for record in records:
            if states.get(record["id"], {}).get("state") != "resolved":
                continue
            bug_store.resolve(
                source,
                record,
                note="defect_recheck(apply=True)",
                output=states[record["id"]].get("current", ""),
            )
            retired.append(record["id"])

    result = {
        "status": "ok",
        "source": source,
        "checked": len(records),
        "counts": {state: len(rows) for state, rows in sorted(buckets.items())},
        "states": {state: rows for state, rows in sorted(buckets.items())},
        "applied": apply,
        "retired": retired,
        "open_total": len(bug_store.load_open(source)),
    }
    if not apply and buckets.get("resolved"):
        ready = ",".join(str(row["id"]) for row in buckets["resolved"])
        result["hint"] = f'Close them with defect_resolve(ids="{ready}", source="{source}", note="…")'
    if missing:
        result["missing"] = missing
    return _json_result(result)


@mcp.tool()
async def defect_resolve(ids: str, source: str = "defect", note: str = "", force: bool = False) -> str:
    """Close named records, moving them to `resolved/`.

    Each record is re-run first; one that still reproduces is refused unless
    `force` is set with a `note` explaining the decision. Records are moved, not
    deleted.

    Args:
        ids: Record ids, comma- or space-separated. Required, no wildcards.
        source: Store to update.
        note: Why the record is being closed (e.g. the plan and package that fixed it).
        force: Close even when the record still reproduces. Requires `note`.
    """
    try:
        wanted = _parse_ids(ids)
    except ValueError as exc:
        return json_error(str(exc))
    if force and not note.strip():
        return json_error("force=True requires note explaining why the record is closed while still failing")

    records, missing = _load_by_ids(source, wanted, include_resolved=False)
    states = await _recheck_records(records)
    resolved: list[dict] = []
    refused: list[dict] = []
    for record in records:
        info = states.get(record["id"], {"state": "error", "error": "not evaluated"})
        if info["state"] == "resolved" or force:
            path = bug_store.resolve(
                source,
                record,
                note=note or f"defect_resolve (state={info['state']})",
                output=info.get("current", ""),
            )
            resolved.append({"id": record["id"], "state": info["state"], "file": Path(path).name})
        else:
            refused.append(
                {
                    "id": record["id"],
                    "state": info["state"],
                    "current": info.get("current", ""),
                    "oracle": info.get("oracle", ""),
                    "reason": (
                        "POS/lemma-only record — confirm the fix by hand, then re-run with force=True and a note"
                        if info["state"] == "needs-manual"
                        else "still reproduces"
                    ),
                }
            )

    result = {
        "status": "ok" if resolved and not refused else ("error" if refused and not resolved else "partial"),
        "source": source,
        "resolved": resolved,
        "refused": refused,
        "open_total": len(bug_store.load_open(source)),
    }
    if missing:
        result["missing"] = missing
    return _json_result(result)


@mcp.tool()
async def defect_reopen(ids: str, source: str = "defect") -> str:
    """Move retired records back into the open set.

    Args:
        ids: Record ids, comma- or space-separated.
        source: Store to update.
    """
    try:
        wanted = _parse_ids(ids)
    except ValueError as exc:
        return json_error(str(exc))

    reopened = []
    missing = []
    for bug_id in wanted:
        record = next((rec for rec in bug_store.load_resolved(source) if rec["id"] == bug_id), None)
        if record is None:
            missing.append(bug_id)
            continue
        path = bug_store.reopen(source, record)
        reopened.append({"id": bug_id, "file": Path(path).name})

    result = {"status": "ok", "source": source, "reopened": reopened, "open_total": len(bug_store.load_open(source))}
    if missing:
        result["missing"] = missing
    return _json_result(result)


@mcp.tool()
async def defect_report(source: str = "defect", out: str = "", include_resolved: bool = False) -> str:
    """Render the deterministic Markdown report for a store.

    Every record is re-run so the report shows current behavior rather than the
    output filed with it. Ordering is pattern name then id, so the same store
    always produces byte-identical output. The report is a generated artifact:
    regenerate it instead of editing it.

    Args:
        source: Store to report on.
        out: Path below ``backup/`` to write to. Absolute paths and ``..`` are rejected.
            Empty returns the text only.
        include_resolved: Append the retired records as a closed-history section.
    """
    try:
        records = bug_store.load_open(source)
        retired = bug_store.load_resolved(source)
    except Exception as exc:
        return json_error(str(exc))

    live = await _recheck_records(records)
    text = bug_store.render_report(
        source=source,
        records=records,
        live=live,
        resolved_count=len(retired),
        date=bug_store.today(),
    )
    if include_resolved and retired:
        lines = ["", "## Resolved history", "", "| ID | text | closed | note |", "|---|---|---|---|"]
        for record in retired:
            lines.append(
                f"| #{record['id']} | {record.get('text', '')} | {record.get('resolved_at', '-')} | "
                f"{(record.get('resolved_note') or '-')[:80]} |"
            )
        text += "\n".join(lines) + "\n"

    result: dict = {
        "status": "ok",
        "source": source,
        "open": len(records),
        "resolved": len(retired),
        "states": {
            state: sum(1 for info in live.values() if info["state"] == state)
            for state in sorted({info["state"] for info in live.values()})
        },
    }
    if out:
        requested = Path(out)
        backup_root = (PROJECT_ROOT / "backup").resolve()
        if requested.is_absolute() or ".." in requested.parts:
            return json_error("out must be a relative path below backup/ without '..'")
        path = (PROJECT_ROOT / requested).resolve()
        try:
            path.relative_to(backup_root)
        except ValueError:
            return json_error("out must be a relative path below backup/")
        atomic_write_text(path, text)
        result["written"] = str(path)
    else:
        result["report"] = text
    return _json_result(result)


@mcp.tool()
async def defect_archive(source: str = "defect", confirm: str = "") -> str:
    """Retire every open record of a store into a timestamped archive directory.

    Records are moved, never deleted, and sub-directories of the store are left
    untouched. `confirm` must equal "<source>:<open count>" so a stale or
    guessed call cannot retire a store.

    Args:
        source: Store to retire.
        confirm: Exactly "<source>:<open count>", e.g. "defect:39".
    """
    try:
        records = bug_store.load_open(source)
    except Exception as exc:
        return json_error(str(exc))

    token = f"{source}:{len(records)}"
    if confirm != token:
        return json_error(f'confirm must be exactly "{token}" to archive {len(records)} open record(s) of {source}')
    if not records:
        return _json_result({"status": "ok", "archived": 0, "message": "Nothing to archive"})

    stamp = datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S")
    target, count = bug_store.archive(source, stamp)
    return _json_result(
        {
            "status": "ok",
            "archived": count,
            "directory": str(target),
            "open_total": len(bug_store.load_open(source)),
            "message": "Records were moved, not deleted. Move them back to restore.",
        }
    )
