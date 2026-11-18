"""Shared helpers for defect store MCP tools."""

from ..core import bug_store
from ..core.json_utils import json_error
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


def _text_error(text: str) -> str | None:
    """Return the error JSON for a missing text, or None when it is present."""
    return None if (text or "").strip() else json_error("text is required")


def _kind_error(kind: str) -> str | None:
    """Return the error JSON for an unknown kind; an empty kind is accepted."""
    if kind and kind not in bug_store.KINDS:
        return json_error(f"kind must be one of {bug_store.KINDS}")
    return None


def _priority_error(priority: str) -> str | None:
    """Return the error JSON for an unknown priority; an empty priority is accepted."""
    if priority and priority.lower() not in bug_store.PRIORITIES:
        return json_error(f"priority must be one of {bug_store.PRIORITIES}")
    return None


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
    by_id: dict[int, dict] = {}
    for load in (bug_store.load_open, bug_store.load_resolved) if include_resolved else (bug_store.load_open,):
        for record in load(source):
            by_id.setdefault(record["id"], record)
    records = []
    missing = []
    for bug_id in wanted:
        record = by_id.get(bug_id)
        if record is None:
            missing.append(bug_id)
        else:
            records.append(record)
    return records, missing
