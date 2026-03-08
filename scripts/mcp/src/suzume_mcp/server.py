"""MCP server for Suzume Japanese morphological analysis tools."""

import os
from pathlib import Path

from mcp.server.fastmcp import FastMCP

# Resolve project root (scripts/mcp/../../ = project root)
_SERVER_DIR = Path(__file__).resolve().parent.parent.parent  # src/suzume_mcp -> mcp/
PROJECT_ROOT = _SERVER_DIR.parent.parent  # mcp/ -> scripts/ -> project root

# Allow override via environment variable
if env_root := os.environ.get("SUZUME_PROJECT_ROOT"):
    PROJECT_ROOT = Path(env_root)

mcp = FastMCP("suzume")

# Import tool modules to register them with the MCP server.
# These must be imported after `mcp` is defined since they reference it.
from .tools import dict_tools, test_tools, thread_tools  # noqa: E402, F401


def main():
    """Entry point for the MCP server."""
    mcp.run()


if __name__ == "__main__":
    main()
