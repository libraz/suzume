"""Tests for native Suzume CLI discovery."""

import asyncio
import io
import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

from suzume_mcp import __main__ as normalization_cli
from suzume_mcp.core import native_cli, suzume_cli
from suzume_mcp.core.suzume_cli import get_cli_path

CLI_NAME = "suzume-cli.exe" if os.name == "nt" else "suzume-cli"


@pytest.fixture(autouse=True)
def clear_cli_override(monkeypatch: pytest.MonkeyPatch) -> None:
    """Keep each discovery test independent of the developer environment."""
    monkeypatch.delenv("SUZUME_CLI_PATH", raising=False)


def _touch_cli(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.touch()
    return path


def test_cli_path_env_override_is_authoritative(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    override = tmp_path / "custom" / "suzume-cli"
    monkeypatch.setenv("SUZUME_CLI_PATH", str(override))

    assert get_cli_path(tmp_path) == override


def test_cli_path_relative_override_uses_project_root(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("SUZUME_CLI_PATH", "artifacts/suzume-cli")

    assert get_cli_path(tmp_path) == tmp_path / "artifacts" / "suzume-cli"


def test_cli_path_prefers_flat_build(tmp_path: Path) -> None:
    flat_cli = _touch_cli(tmp_path / "build" / "bin" / CLI_NAME)
    _touch_cli(tmp_path / "build" / "bin" / "Debug" / CLI_NAME)

    assert get_cli_path(tmp_path) == flat_cli


@pytest.mark.parametrize("configuration", ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"])
def test_cli_path_finds_multi_config_build(tmp_path: Path, configuration: str) -> None:
    configured_cli = _touch_cli(tmp_path / "build" / "bin" / configuration / CLI_NAME)

    assert get_cli_path(tmp_path) == configured_cli


def test_cli_path_supports_alternate_multi_config_layout(tmp_path: Path) -> None:
    configured_cli = _touch_cli(tmp_path / "build" / "Release" / "bin" / CLI_NAME)

    assert get_cli_path(tmp_path) == configured_cli


def test_cli_path_returns_flat_candidate_when_unbuilt(tmp_path: Path) -> None:
    assert get_cli_path(tmp_path) == tmp_path / "build" / "bin" / CLI_NAME


def test_normalization_batch_preserves_per_item_errors(monkeypatch: pytest.MonkeyPatch, capsys) -> None:
    monkeypatch.setattr("sys.stdin", io.StringIO(json.dumps([1, None])))

    normalization_cli.cmd_normalize(["--batch"])

    results = json.loads(capsys.readouterr().out)
    assert len(results) == 2
    assert "Expected text string" in results[0]["error"]
    assert "Expected text string" in results[1]["error"]


def test_batch_wrapper_keeps_successes_around_an_error(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(
        suzume_cli,
        "_run_normalize_cli",
        lambda _texts, **_kwargs: [
            {"tokens": [{"surface": "東京", "pos": "Noun"}], "source": "MeCab", "rule": ""},
            {"error": "bad input"},
        ],
    )

    results = suzume_cli.get_expected_tokens_batch_subprocess(["東京", "失敗"])

    assert results[0][0][0]["surface"] == "東京"
    assert results[1] == ([], "error", "normalization failed for '失敗': bad input")


def test_mecab_batch_wrapper_selects_raw_mode(monkeypatch: pytest.MonkeyPatch) -> None:
    calls = []

    def fake_run(texts, *, raw_mecab=False):
        calls.append((texts, raw_mecab))
        return [{"tokens": [], "source": "MeCab", "rule": "intentional-rule"}]

    monkeypatch.setattr(suzume_cli, "_run_normalize_cli", fake_run)

    assert suzume_cli.get_mecab_tokens_batch_subprocess(["東京"]) == [([], "MeCab", "intentional-rule")]
    assert calls == [(["東京"], True)]


def _write(path: Path, modified: int) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.touch()
    os.utime(path, ns=(modified, modified))
    return path


@pytest.fixture
def fresh_source_scan(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(suzume_cli, "_source_scan", {})
    monkeypatch.delenv("SUZUME_ALLOW_STALE_CLI", raising=False)


_BUILT_AT = 2_000_000_000_000_000_000
_ONE_SECOND = 1_000_000_000


@pytest.mark.parametrize("source", ["src/analysis/scorer.cpp", "include/suzume/suzume_c.h", "tools/cli/main.cpp"])
def test_staleness_names_the_source_changed_after_the_build(tmp_path: Path, fresh_source_scan, source: str) -> None:
    _write(tmp_path / "build" / "bin" / CLI_NAME, _BUILT_AT)
    _write(tmp_path / source, _BUILT_AT + _ONE_SECOND)

    staleness = suzume_cli.cli_staleness(project_root=tmp_path)

    assert staleness is not None
    assert staleness["newest_source"] == str(Path(source))
    assert staleness["cli"] == str(Path("build") / "bin" / CLI_NAME)
    assert staleness["source_modified"] > staleness["cli_built"]


@pytest.mark.parametrize("source", ["src/analysis/scorer.cpp", "include/suzume/suzume_c.h", "tools/cli/main.cpp"])
def test_staleness_is_none_for_a_binary_built_after_its_sources(tmp_path: Path, fresh_source_scan, source: str) -> None:
    _write(tmp_path / "build" / "bin" / CLI_NAME, _BUILT_AT)
    _write(tmp_path / source, _BUILT_AT - 1)

    assert suzume_cli.cli_staleness(project_root=tmp_path) is None


def test_staleness_ignores_files_outside_the_compiled_directories(tmp_path: Path, fresh_source_scan) -> None:
    _write(tmp_path / "build" / "bin" / CLI_NAME, _BUILT_AT)
    _write(tmp_path / "scripts" / "check.py", _BUILT_AT + 1)

    assert suzume_cli.cli_staleness(project_root=tmp_path) is None


def test_staleness_is_none_without_a_binary(tmp_path: Path, fresh_source_scan) -> None:
    _write(tmp_path / "src" / "core" / "types.cpp", _BUILT_AT)

    assert suzume_cli.cli_staleness(project_root=tmp_path) is None


def test_refusal_names_the_override_and_yields_to_it(fresh_source_scan, monkeypatch: pytest.MonkeyPatch) -> None:
    staleness = {"newest_source": "src/core/types.cpp", "cli": "build/bin/suzume-cli"}

    refusal = suzume_cli.stale_cli_refusal(staleness)
    assert refusal is not None and "src/core/types.cpp" in refusal and "SUZUME_ALLOW_STALE_CLI=1" in refusal
    assert suzume_cli.stale_cli_refusal(None) is None

    monkeypatch.setenv("SUZUME_ALLOW_STALE_CLI", "1")
    assert suzume_cli.stale_cli_refusal(staleness) is None


def test_existing_cli_reports_a_missing_binary(tmp_path: Path) -> None:
    with pytest.raises(RuntimeError, match="not found"):
        suzume_cli.existing_cli(tmp_path / "build" / "bin" / CLI_NAME)


def _hanging_cli(tmp_path: Path) -> Path:
    cli = tmp_path / "hanging-cli"
    cli.write_text("#!/bin/sh\nexec sleep 30\n", encoding="utf-8")
    cli.chmod(0o755)
    return cli


@pytest.mark.skipif(os.name == "nt", reason="uses a shell script as the child")
def test_a_hung_cli_times_out_in_the_blocking_call(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(suzume_cli, "_ANALYZE_TIMEOUT_SECONDS", 0.2)

    with pytest.raises(RuntimeError, match=r"timed out after 0\.2s"):
        suzume_cli.get_suzume_surfaces("東京", cli_path=_hanging_cli(tmp_path))


@pytest.mark.skipif(os.name == "nt", reason="uses a shell script as the child")
def test_a_hung_cli_times_out_and_is_killed_in_the_async_call(tmp_path: Path) -> None:
    with pytest.raises(RuntimeError, match=r"timed out after 0\.2s"):
        asyncio.run(native_cli.run_cli(_hanging_cli(tmp_path), ["analyze"], timeout=0.2))


@pytest.mark.skipif(os.name == "nt", reason="uses a shell script as the child")
def test_one_failed_text_keeps_the_rest_of_the_batch(tmp_path: Path) -> None:
    cli = tmp_path / "picky-cli"
    cli.write_text(
        '#!/bin/sh\nfor arg; do last="$arg"; done\n[ "$last" = bad ] && exit 3\nprintf "%s\\tNOUN\\nEOS\\n" "$last"\n'
    )
    cli.chmod(0o755)

    results = asyncio.run(suzume_cli.get_suzume_tokens_many(["東京", "bad"], skip_user_dict=True, cli_path=cli))

    assert results[0] == [{"surface": "東京", "pos": "Noun", "lemma": "東京"}]
    assert isinstance(results[1], RuntimeError) and "Suzume CLI failed" in str(results[1])


def test_the_server_never_spells_the_native_cli_command_line() -> None:
    """A CLI flag rename must reach running servers on the next call, not after a reconnect.

    The server process imports every tool module once and keeps it. Any native-CLI flag or
    subcommand spelled there is frozen at server start, which is how a renamed user-dictionary
    flag once broke every live check until the servers were restarted. Only ``native_cli``,
    loaded fresh in each child, may spell it.
    """
    import ast

    probe = (
        "import json, sys; import suzume_mcp.server; "
        "print(json.dumps(sorted((n, m.__file__) for n, m in sys.modules.items() "
        "if n.startswith('suzume_mcp') and getattr(m, '__file__', None))))"
    )
    result = subprocess.run(
        [sys.executable, "-c", probe], capture_output=True, text=True, cwd=suzume_cli._MCP_PROJECT_DIR, check=True
    )
    loaded = dict(json.loads(result.stdout))

    assert "suzume_mcp.core.native_cli" not in loaded
    own_cli_flags = {"--batch", "--mecab"}
    offenders = []
    for name, path in sorted(loaded.items()):
        if name == "suzume_mcp.__main__":
            continue
        for node in ast.walk(ast.parse(Path(path).read_text(encoding="utf-8"))):
            if isinstance(node, ast.Constant) and isinstance(node.value, str):
                if node.value.startswith("--") and node.value not in own_cli_flags:
                    offenders.append(f"{name}:{node.lineno}: {node.value}")
    assert offenders == []


def test_normalize_timeout_grows_with_the_batch(monkeypatch: pytest.MonkeyPatch) -> None:
    observed: dict = {}

    def fake_run(command, **kwargs):
        observed.update(kwargs)
        return type("Result", (), {"returncode": 0, "stdout": "[]", "stderr": ""})()

    monkeypatch.setattr(suzume_cli.subprocess, "run", fake_run)
    suzume_cli._run_normalize_cli(["東京"] * 100)

    assert observed["timeout"] == pytest.approx(160.0)


def test_rule_command_prints_one_rule_per_text(monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture) -> None:
    monkeypatch.setattr("sys.stdin", io.StringIO(json.dumps(["経済成長", "りんご"], ensure_ascii=False)))

    normalization_cli.cmd_rule([])

    assert json.loads(capsys.readouterr().out) == ["kanji-compound", ""]


def test_map_pos_command_maps_raw_tokens(monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture) -> None:
    tokens = [{"surface": "東京", "pos": "名詞", "pos_sub1": "固有名詞"}, {"surface": "た", "pos": "助動詞"}]
    monkeypatch.setattr("sys.stdin", io.StringIO(json.dumps(tokens, ensure_ascii=False)))

    normalization_cli.cmd_map_pos([])

    assert json.loads(capsys.readouterr().out) == ["Noun", "Auxiliary"]


def test_rule_and_map_pos_wrappers_call_their_subcommands(monkeypatch: pytest.MonkeyPatch) -> None:
    calls = []
    monkeypatch.setattr(suzume_cli, "_run_module_cli", lambda args, payload: calls.append((args, payload)) or ["x"])

    suzume_cli.get_suzume_rules_subprocess(["東京"])
    suzume_cli.map_mecab_pos_subprocess([{"pos": "名詞"}])

    assert calls == [(["rule"], ["東京"]), (["map-pos"], [{"pos": "名詞"}])]


ORACLE_ENTRY_POINTS = {
    "get_expected_tokens",
    "get_mecab_tokens",
    "get_suzume_rule",
    "map_mecab_pos",
    "correct_mecab_pos",
}
ORACLE_MODULE_PREFIXES = ("merge_rules", "merge_postprocessor", "split_rules", "postprocessor")


def test_tools_reach_the_oracle_only_through_a_subprocess() -> None:
    import ast

    tools_dir = Path(suzume_cli.__file__).resolve().parent.parent / "tools"
    offenders = []
    for path in sorted(tools_dir.glob("*.py")):
        for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
            if not isinstance(node, ast.ImportFrom) or not (node.module or "").startswith("core"):
                continue
            module = (node.module or "").removeprefix("core").lstrip(".")
            names = {alias.name for alias in node.names}
            if module.startswith(ORACLE_MODULE_PREFIXES) or names & ORACLE_ENTRY_POINTS:
                offenders.append(f"{path.name}:{node.lineno}")
    assert offenders == []


def test_a_large_normalize_batch_is_split_across_workers_in_order(monkeypatch: pytest.MonkeyPatch) -> None:
    chunks = []

    def fake_module_cli(args, payload):
        chunks.append(list(payload))
        return [{"tokens": [], "source": "MeCab", "rule": text} for text in payload]

    monkeypatch.setattr(suzume_cli, "_run_module_cli", fake_module_cli)
    texts = [f"t{index}" for index in range(1000)]

    results = suzume_cli._run_normalize_cli(texts)

    assert [result["rule"] for result in results] == texts
    assert len(chunks) == suzume_cli._NORMALIZE_WORKERS
    assert sorted(len(chunk) for chunk in chunks) == [250, 250, 250, 250]


def test_a_small_normalize_batch_stays_in_one_process(monkeypatch: pytest.MonkeyPatch) -> None:
    calls = []
    monkeypatch.setattr(suzume_cli, "_run_module_cli", lambda args, payload: calls.append(payload) or payload)

    suzume_cli._run_normalize_cli(["東京"] * 100)

    assert len(calls) == 1
