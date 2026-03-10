"""MCP server for Suzume Japanese morphological analysis tools."""

from mcp.server.fastmcp import FastMCP

from .config import PROJECT_ROOT  # noqa: F401 — re-exported for tools

mcp = FastMCP("suzume")

# Import tool modules to register them with the MCP server.
# These must be imported after `mcp` is defined since they reference it.
from .tools import dict_tools, test_tools, thread_tools  # noqa: E402, F401


def main():
    """Entry point for the MCP server."""
    mcp.run()


if __name__ == "__main__":
    main()
