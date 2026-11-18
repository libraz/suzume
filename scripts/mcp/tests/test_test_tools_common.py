"""Contract tests for shared tokenization-test tool helpers."""

from pathlib import Path
from types import SimpleNamespace

from suzume_mcp import server as _server  # noqa: F401
from suzume_mcp.tools import _test_tools_common as common


def test_get_suzume_tokens_uses_explicit_analyze_command_and_option_separator(monkeypatch, tmp_path: Path):
    cli = tmp_path / "suzume-cli"
    cli.touch()
    observed: list[str] = []

    def run(command, **_kwargs):
        observed.extend(command)
        return SimpleNamespace(returncode=0, stdout="-x\tNoun\t-x\nEOS\n", stderr="")

    monkeypatch.setattr(common, "existing_cli", lambda: cli)
    monkeypatch.setattr(common.subprocess, "run", run)

    assert common._get_suzume_tokens("-x") == [{"surface": "-x", "pos": "Noun", "lemma": "-x"}]
    assert observed == [str(cli), "analyze", "--no-user-dict", "--", "-x"]


def test_filtered_test_files_reject_path_like_names(monkeypatch, tmp_path: Path):
    corpus = tmp_path / "tests/data/tokenization"
    corpus.mkdir(parents=True)
    outside = tmp_path / "outside.json"
    outside.write_text("{}", encoding="utf-8")
    monkeypatch.setattr(common, "PROJECT_ROOT", tmp_path)

    assert common._get_test_files_filtered("../outside") == []
    assert common._get_test_files_filtered(str(outside)) == []
