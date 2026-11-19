"""Integration tests for automatic placement during test additions."""

import json
from importlib import import_module

import pytest

from suzume_mcp.tools import test_tools as _test_tools  # noqa: F401
from tests.helpers import CORPUS_DIR, parse, run, write_suite

mutation = import_module("suzume_mcp.tools._test_tools_mutation")


@pytest.fixture(autouse=True)
def passing_cli(monkeypatch):
    """Placement is under test here, not the pin verdict: Suzume echoes the expectation."""

    async def fake_cli(text, cli_path=None, skip_user_dict=False):
        return expected_tokens()

    monkeypatch.setattr("suzume_mcp.core.suzume_cli.get_suzume_tokens_async", fake_cli)


def expected_tokens():
    return [{"surface": "テスト", "pos": "Noun", "lemma": "テスト"}]


def existing_cases(case_count: int) -> list[dict]:
    return [
        {
            "id": f"existing_{index}",
            "description": f"existing {index}",
            "input": f"既存{index}",
            "expected": expected_tokens(),
        }
        for index in range(case_count)
    ]


def test_add_splits_full_logical_file(tmp_path, monkeypatch):
    write_suite(tmp_path, "general", existing_cases(100))
    monkeypatch.setattr(mutation, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(mutation, "get_expected_tokens", lambda _text: (expected_tokens(), "reference", "test"))

    data = parse(run(mutation.test_add("新規テスト", file="general")))

    test_dir = tmp_path / CORPUS_DIR
    assert data["status"] == "ok"
    assert data["requested_file"] == "general"
    assert data["file"] == "general_02"
    assert data["split"] is True
    assert not (test_dir / "general.json").exists()
    assert sum(len(json.loads(path.read_text(encoding="utf-8"))["cases"]) for path in test_dir.glob("*.json")) == 101


def test_add_uses_suggested_logical_file_when_omitted(tmp_path, monkeypatch):
    adjective_tokens = [{"surface": "明るい", "pos": "Adjective", "lemma": "明るい"}]
    monkeypatch.setattr(mutation, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(mutation, "get_expected_tokens", lambda _text: (adjective_tokens, "reference", "test"))

    data = parse(run(mutation.test_add("明るい")))

    test_dir = tmp_path / CORPUS_DIR
    assert data["status"] == "ok"
    assert data["requested_file"] == "adjective_i_basic"
    assert data["placement_source"] == "suggested"
    assert (test_dir / "adjective_i_basic.json").exists()


def test_batch_add_reports_and_applies_partition_placements(tmp_path, monkeypatch):
    write_suite(tmp_path, "general", existing_cases(99))
    monkeypatch.setattr(mutation, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(
        mutation,
        "get_expected_tokens_batch_subprocess",
        lambda texts: [(expected_tokens(), "reference", "test") for _text in texts],
    )
    inputs = ["追加テスト一", "追加テスト二", "追加テスト三"]

    preview = parse(run(mutation.test_batch_add("general", inputs, apply=False)))
    applied = parse(run(mutation.test_batch_add("general", inputs, apply=True)))

    test_dir = tmp_path / CORPUS_DIR
    assert preview["applied"] is False
    assert [item["file"] for item in preview["to_add"]] == ["general_01", "general_02", "general_02"]
    assert applied["applied"] is True
    assert [item["file"] for item in applied["to_add"]] == ["general_01", "general_02", "general_02"]
    assert not (test_dir / "general.json").exists()
    assert sorted(len(json.loads(path.read_text(encoding="utf-8"))["cases"]) for path in test_dir.glob("*.json")) == [
        2,
        100,
    ]


def test_batch_add_groups_automatic_suggestions(tmp_path, monkeypatch):
    inputs = ["追加名詞", "追加動詞"]
    token_results = [
        ([{"surface": "名詞", "pos": "Noun", "lemma": "名詞"}], "reference", "test"),
        ([{"surface": "試す", "pos": "Verb", "lemma": "試す"}], "reference", "test"),
    ]
    monkeypatch.setattr(mutation, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(mutation, "get_expected_tokens_batch_subprocess", lambda _texts: token_results)

    data = parse(run(mutation.test_batch_add("", inputs, apply=True)))

    test_dir = tmp_path / CORPUS_DIR
    assert data["applied"] is True
    assert [item["file"] for item in data["to_add"]] == ["noun_general", "verb_godan_misc"]
    assert (test_dir / "noun_general.json").exists()
    assert (test_dir / "verb_godan_misc.json").exists()
