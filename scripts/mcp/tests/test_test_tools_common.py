"""Contract tests for shared tokenization-test tool helpers."""

from pathlib import Path

from suzume_mcp.core import native_cli, suzume_cli
from suzume_mcp.tools import _test_tools_common as common
from tests.helpers import CORPUS_DIR


def test_get_suzume_tokens_runs_without_the_bundled_user_dictionary(monkeypatch):
    observed = []

    def analyze_many(texts, cli_path, skip_user_dict):
        observed.append((texts, skip_user_dict))
        return [[{"surface": "-x", "pos": "Noun", "lemma": "-x"}]]

    monkeypatch.setattr(suzume_cli, "_analyze_many_sync", analyze_many)

    assert common._get_suzume_tokens("-x") == [{"surface": "-x", "pos": "Noun", "lemma": "-x"}]
    assert observed == [(["-x"], True)]


def test_analyze_command_puts_an_option_separator_before_the_text():
    assert native_cli.analyze_args("-x", skip_user_dict=False) == ["analyze", "--", "-x"]
    assert native_cli.analyze_args("-x", skip_user_dict=True)[-2:] == ["--", "-x"]


def test_filtered_test_files_reject_path_like_names(monkeypatch, tmp_path: Path):
    corpus = tmp_path / CORPUS_DIR
    corpus.mkdir(parents=True)
    outside = tmp_path / "outside.json"
    outside.write_text("{}", encoding="utf-8")
    monkeypatch.setattr(common, "PROJECT_ROOT", tmp_path)

    assert common._get_test_files_filtered("../outside") == []
    assert common._get_test_files_filtered(str(outside)) == []
