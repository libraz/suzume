"""MCP server for Suzume Japanese morphological analysis tools."""

from importlib import import_module
from pathlib import Path

from mcp.server.mcpserver import MCPServer

from .config import PROJECT_ROOT  # noqa: F401 — re-exported for tools

mcp = MCPServer("suzume")

# Import every tool module to register its decorated functions. Keeping this
# discovery next to the server eliminates a second, manually maintained list:
# an import error intentionally aborts startup rather than making only part of
# the MCP API silently disappear.
_TOOLS_PACKAGE = f"{__package__}.tools"
for _tool_file in sorted(Path(__file__).with_name("tools").glob("*.py")):
    if not _tool_file.stem.startswith("_"):
        import_module(f"{_TOOLS_PACKAGE}.{_tool_file.stem}")


def main():
    """Entry point for the MCP server."""
    mcp.run()


if __name__ == "__main__":
    main()
