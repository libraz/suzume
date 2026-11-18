"""Tests for the stale-CLI guard shared by the CLI-running tools."""

from __future__ import annotations

import asyncio
import itertools
import json

import pytest

from suzume_mcp.tools import _cli_freshness
from suzume_mcp.tools._cli_freshness import stale_cli_guard, with_stale_notice

_STALENESS = {
    "message": "stale",
    "newest_source": "src/core/types.cpp",
    "source_modified": "2026-11-19T04:00:01",
    "cli": "build/bin/suzume-cli",
    "cli_built": "2026-11-19T04:00:00",
}


def _guarded_tool(persists: bool, uses_cli: bool, calls: list[dict]):
    @stale_cli_guard(persists=lambda args: persists and args["write"], uses_cli=lambda args: uses_cli)
    async def tool(text: str, write: bool = True) -> str:
        calls.append({"text": text, "write": write})
        return json.dumps({"status": "ok", "text": text})

    return tool


# Every combination of the four inputs that decide the outcome; the space is small
# enough to enumerate rather than sample.
_CASES = list(itertools.product([True, False], repeat=4))


@pytest.mark.parametrize(("stale", "persists", "uses_cli", "allowed"), _CASES)
def test_guard_outcome(
    monkeypatch: pytest.MonkeyPatch, stale: bool, persists: bool, uses_cli: bool, allowed: bool
) -> None:
    monkeypatch.setattr(_cli_freshness, "cli_staleness", lambda: _STALENESS if stale else None)
    if allowed:
        monkeypatch.setenv("SUZUME_ALLOW_STALE_CLI", "1")
    else:
        monkeypatch.delenv("SUZUME_ALLOW_STALE_CLI", raising=False)
    calls: list[dict] = []

    result = json.loads(asyncio.run(_guarded_tool(persists, uses_cli, calls)("東京")))

    refused = stale and persists and uses_cli and not allowed
    if refused:
        assert calls == []
        assert result["status"] == "error"
        assert "SUZUME_ALLOW_STALE_CLI=1" in result["message"]
        assert result["stale_cli"] == _STALENESS
        return
    assert calls == [{"text": "東京", "write": True}]
    assert result["status"] == "ok"
    if stale and uses_cli:
        assert next(iter(result)) == "stale_cli"
        assert result["stale_cli"] == _STALENESS
    else:
        assert "stale_cli" not in result


def test_guard_reads_defaults_and_keywords_when_deciding(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(_cli_freshness, "cli_staleness", lambda: _STALENESS)
    monkeypatch.delenv("SUZUME_ALLOW_STALE_CLI", raising=False)
    calls: list[dict] = []
    tool = _guarded_tool(persists=True, uses_cli=True, calls=calls)

    assert json.loads(asyncio.run(tool("東京")))["status"] == "error"
    assert json.loads(asyncio.run(tool("東京", write=False)))["stale_cli"] == _STALENESS
    assert calls == [{"text": "東京", "write": False}]


def test_notice_leaves_a_non_object_result_unchanged() -> None:
    assert with_stale_notice("[1, 2]", _STALENESS) == "[1, 2]"
    assert with_stale_notice("plain text", _STALENESS) == "plain text"
    assert with_stale_notice('{"status": "ok"}', None) == '{"status": "ok"}'


def test_guard_keeps_the_tool_signature_for_registration() -> None:
    import inspect

    tool = _guarded_tool(persists=False, uses_cli=True, calls=[])

    assert list(inspect.signature(tool).parameters) == ["text", "write"]
    assert inspect.iscoroutinefunction(tool)


def _refused_calls():
    from suzume_mcp.tools.defect_tools import defect_add, defect_dismiss, defect_recheck, defect_resolve
    from suzume_mcp.tools.dict_tools import dict_cleanup
    from suzume_mcp.tools.thread_tools import thread_next, thread_scan

    return {
        "defect_add": lambda: defect_add(text="東京"),
        "defect_dismiss": lambda: defect_dismiss(text="東京", resolution="ambiguous", reason="r", pattern="p"),
        "defect_recheck": lambda: defect_recheck(apply=True),
        "defect_resolve": lambda: defect_resolve(ids="1"),
        "thread_scan": lambda: thread_scan(dry_run=False),
        "thread_next": lambda: thread_next(dry_run=False),
        "dict_cleanup": lambda: dict_cleanup(input_file="data/core/sample.tsv", dry_run=False),
    }


@pytest.mark.parametrize("name", list(_refused_calls()))
def test_persisting_tools_refuse_a_stale_cli(monkeypatch: pytest.MonkeyPatch, name: str) -> None:
    monkeypatch.setattr(_cli_freshness, "cli_staleness", lambda: _STALENESS)
    monkeypatch.delenv("SUZUME_ALLOW_STALE_CLI", raising=False)

    result = json.loads(asyncio.run(_refused_calls()[name]()))

    assert result["status"] == "error"
    assert result["message"].startswith("Refusing to persist output from a stale Suzume CLI")


def test_written_report_opens_with_the_stale_banner(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(_cli_freshness, "cli_staleness", lambda: _STALENESS)

    banner = _cli_freshness.stale_banner()

    assert banner.startswith("> **Stale CLI**")
    assert "`src/core/types.cpp`" in banner and banner.endswith("\n\n")
