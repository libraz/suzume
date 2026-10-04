"""Consistent JSON responses shared by MCP tools."""

import json
from typing import Any


def json_result(value: Any) -> str:
    """Serialize an MCP result using the project's human-readable format."""
    return json.dumps(value, ensure_ascii=False, indent=2)


def json_error(message: str, **fields: Any) -> str:
    """Serialize the standard MCP error envelope, followed by any context fields."""
    return json_result({"status": "error", "message": message, **fields})
