"""Store-only defect MCP tools."""

import datetime
from pathlib import Path

from ..core import bug_store
from ..core.json_utils import json_error
from ..core.json_utils import json_result as _json_result
from ..server import mcp
from ._defect_tools_common import _entry, _parse_ids


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
