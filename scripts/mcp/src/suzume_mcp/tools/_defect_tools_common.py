"""Shared helpers for defect store MCP tools."""

from ..core import bug_store
from ..core.json_utils import json_result as _json_result


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
