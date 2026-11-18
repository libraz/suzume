"""Server bootstrap coverage for complete MCP tool registration."""

from __future__ import annotations

import asyncio
import subprocess
import sys
from pathlib import Path

import pytest

from suzume_mcp import config, server

_TOOL_MODULES = sorted(path.stem for path in Path(server.__file__).with_name("tools").glob("*.py"))


def test_project_root_is_repository_root() -> None:
    assert (config.PROJECT_ROOT / "CMakeLists.txt").is_file()


@pytest.mark.parametrize("module", _TOOL_MODULES)
def test_tool_module_imports_without_the_server_first(module: str) -> None:
    completed = subprocess.run(
        [sys.executable, "-c", f"import suzume_mcp.tools.{module}"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert completed.returncode == 0, completed.stderr


def test_every_decorated_tool_module_is_loaded() -> None:
    registered = {tool.name for tool in asyncio.run(server.mcp.list_tools())}
    tool_dir = Path(server.__file__).with_name("tools")
    for module_path in tool_dir.glob("*.py"):
        source = module_path.read_text(encoding="utf-8")
        if "@mcp.tool()" not in source:
            continue
        assert module_path.stem in __import__("sys").modules[f"suzume_mcp.tools.{module_path.stem}"].__name__
    assert {"test_show", "dict_add", "defect_list", "thread_scan"} <= registered
