"""A data/core edit through a dict tool reports exactly what a full oracle recompute changes.

Each test builds a throwaway project with its own one-entry lexicon and test
file, points the oracle subprocess at it, and compares the tool's report with
test_needs_suzume_update's dry run over the same tree.
"""

import json
from importlib import import_module

import pytest

from suzume_mcp.tools import test_tools as _test_tools  # noqa: F401
from tests.helpers import CORPUS_DIR, parse, run

dict_common = import_module("suzume_mcp.tools._dict_tools_common")
operations = import_module("suzume_mcp.tools._dict_tools_operations")
test_common = import_module("suzume_mcp.tools._test_tools_common")
read_tools = import_module("suzume_mcp.tools._test_tools_read")

LEXICON_NOUN = "いりぐち"
ORACLE_LEXICON_FILES = ("nouns.tsv", "verbs.tsv", "adjectives.tsv", "expressions.tsv")
INPUTS = {"lexicon_noun": "ここはいりぐちだ", "unrelated": "りんごを食べる"}


async def recompile_ok() -> str:
    return "ok"


@pytest.fixture
def project(tmp_path, monkeypatch):
    (tmp_path / "data/core").mkdir(parents=True)
    for name in ORACLE_LEXICON_FILES:
        (tmp_path / "data/core" / name).write_text("", encoding="utf-8")
    (tmp_path / "data/core/nouns.tsv").write_text(f"{LEXICON_NOUN}\tNOUN\n", encoding="utf-8")
    test_dir = tmp_path / CORPUS_DIR
    test_dir.mkdir(parents=True)
    cases = [
        {"id": case_id, "input": text, "expected": [{"surface": text, "pos": "Noun"}]}
        for case_id, text in INPUTS.items()
    ]
    (test_dir / "sample.json").write_text(
        json.dumps({"description": "sample", "cases": cases}, ensure_ascii=False), encoding="utf-8"
    )

    monkeypatch.setenv("SUZUME_PROJECT_ROOT", str(tmp_path))
    for module in (dict_common, operations, test_common):
        monkeypatch.setattr(module, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(dict_common, "oracle_drift", test_common.oracle_drift)
    monkeypatch.setattr(operations, "_recompile_core_dic", recompile_ok)

    settled = parse(run(read_tools.test_needs_suzume_update(apply=True)))
    assert settled["errors"] == []
    return tmp_path


def full_recompute_ids() -> list[str]:
    report = parse(run(read_tools.test_needs_suzume_update()))
    assert report["errors"] == []
    return [row["id"] for row in report["needs_update"]]


def test_removing_a_lexicon_noun_reports_the_changed_expectations(project):
    result = parse(run(operations.dict_remove(LEXICON_NOUN)))

    check = result["oracle_check"]
    assert check["needs_update"] == full_recompute_ids()
    assert check["needs_update"] == ["sample/lexicon_noun"]
    assert check["apply_with"] == "test_needs_suzume_update(apply=True)"


def test_an_edit_no_expectation_reads_reports_nothing(project):
    result = parse(run(operations.dict_add("ぞうきんがけ", "NOUN", force=True)))

    check = result["oracle_check"]
    assert check["needs_update"] == full_recompute_ids() == []
    assert check["total"] == 0


def test_a_chained_edit_can_skip_the_recompute(project):
    result = parse(run(operations.dict_remove(LEXICON_NOUN, oracle_check=False)))

    assert result["oracle_check"]["skipped"] is True


def test_a_user_dictionary_edit_runs_no_recompute(project, monkeypatch):
    monkeypatch.setattr(dict_common, "oracle_drift", lambda *_args, **_kwargs: pytest.fail("recomputed"))
    monkeypatch.setattr(operations, "_recompile_user_dic", recompile_ok)

    result = parse(run(operations.dict_add("テスト語", "NOUN", user="common", force=True)))

    assert result["status"] == "ok"
    assert "oracle_check" not in result
