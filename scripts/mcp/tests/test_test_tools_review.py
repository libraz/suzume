"""Focused tests for safe oracle override review tools."""

import json
from importlib import import_module

from suzume_mcp.tools import test_tools as _test_tools  # noqa: F401
from tests import helpers
from tests.helpers import CORPUS_DIR, parse, run

common_tools = import_module("suzume_mcp.tools._test_tools_common")
mutation_tools = import_module("suzume_mcp.tools._test_tools_mutation")
read_tools = import_module("suzume_mcp.tools._test_tools_read")
review = import_module("suzume_mcp.tools._test_tools_review")


def token(surface: str) -> dict:
    return {"surface": surface, "pos": "Noun", "lemma": surface}


def write_suite(root, name: str, cases: list[dict]) -> None:
    helpers.write_suite(root, name, cases, header={}, compact=True)


def make_case(case_id: str, input_text: str, **extra) -> dict:
    case = {"id": case_id, "input": input_text, "expected": [token("正解")]}
    case.update(extra)
    return case


def test_reset_refuses_ambiguous_input(tmp_path, monkeypatch):
    write_suite(tmp_path, "first", [make_case("one", "重複", accepted_diff={"reason": "x"})])
    write_suite(tmp_path, "second", [make_case("two", "重複", accepted_diff={"reason": "x"})])
    monkeypatch.setattr(review, "PROJECT_ROOT", tmp_path)

    data = parse(run(review.test_reset_suzume(input_text="重複")))

    assert data["status"] == "error"
    assert "multiple tests" in data["message"]
    assert "first/one" in data["message"]
    assert "second/two" in data["message"]


def test_reset_strips_both_banned_keys(tmp_path, monkeypatch):
    case = make_case(
        "stale",
        "対象",
        suzume_expected=[token("旧値")],
        accepted_diff={"reason": "old", "category": "tokenizer-difference"},
    )
    write_suite(tmp_path, "sample", [case])
    monkeypatch.setattr(review, "PROJECT_ROOT", tmp_path)

    data = parse(run(review.test_reset_suzume(test_id="sample/stale", apply=True)))

    assert data["total"] == 1
    assert data["reset"][0]["removed"] == ["suzume_expected", "accepted_diff"]
    stored = json.loads((tmp_path / CORPUS_DIR / "sample.json").read_text(encoding="utf-8"))
    assert "suzume_expected" not in stored["cases"][0]
    assert "accepted_diff" not in stored["cases"][0]


def test_reset_strips_even_while_suzume_still_differs(tmp_path, monkeypatch):
    """Removal must never be blocked: the override is banned even when it is 'true'."""
    case = make_case(
        "active",
        "対象",
        suzume_expected=[token("現状")],
        accepted_diff={"reason": "known limitation", "category": "pos-limitation"},
    )
    write_suite(tmp_path, "sample", [case])
    monkeypatch.setattr(review, "PROJECT_ROOT", tmp_path)

    data = parse(run(review.test_reset_suzume(test_id="sample/active", apply=True)))

    assert data["total"] == 1
    assert "test_needs_suzume_update" in data["next_step"]
    stored = json.loads((tmp_path / CORPUS_DIR / "sample.json").read_text(encoding="utf-8"))
    assert "suzume_expected" not in stored["cases"][0]


def test_reset_finds_orphan_accepted_diff(tmp_path, monkeypatch):
    write_suite(tmp_path, "sample", [make_case("orphan", "孤立", accepted_diff={"reason": "old"})])
    test_file = tmp_path / CORPUS_DIR / "sample.json"
    monkeypatch.setattr(review, "_get_test_files_filtered", lambda _file: [test_file])

    data = parse(run(review.test_reset_suzume(all_tests=True, apply=True)))

    assert data["reset"][0]["removed"] == ["accepted_diff"]
    stored = json.loads(test_file.read_text(encoding="utf-8"))
    assert "accepted_diff" not in stored["cases"][0]


def test_accept_diff_refuses_and_writes_nothing(tmp_path, monkeypatch):
    case = make_case("target", "対象")
    write_suite(tmp_path, "sample", [case])
    monkeypatch.setattr(review, "PROJECT_ROOT", tmp_path)

    data = parse(
        run(
            review.test_accept_diff(
                test_id="sample/target",
                reason="updated",
                category="grammar-boundary",
                apply=True,
            )
        )
    )

    assert data["status"] == "error"
    assert "disabled by design" in data["message"]
    stored = json.loads((tmp_path / CORPUS_DIR / "sample.json").read_text(encoding="utf-8"))
    assert stored["cases"][0] == case


def test_partial_oracle_sync_updates_only_selected_id(tmp_path, monkeypatch):
    cases = [make_case("first", "一つ"), make_case("second", "二つ")]
    write_suite(tmp_path, "sample", cases)
    test_file = tmp_path / CORPUS_DIR / "sample.json"
    monkeypatch.setattr(common_tools, "_get_test_files_filtered", lambda _file: [test_file])
    monkeypatch.setattr(
        common_tools,
        "get_expected_tokens_batch_subprocess",
        lambda texts: [([token("新値")], "MeCab+SuzumeRules", "test-rule") for _text in texts],
    )

    data = parse(run(read_tools.test_needs_suzume_update(test_ids=["sample/second"], apply=True)))

    assert data["total"] == 1
    assert data["needs_update"][0]["id"] == "sample/second"
    stored = json.loads(test_file.read_text(encoding="utf-8"))["cases"]
    assert stored[0]["expected"] == [token("正解")]
    assert stored[1]["expected"] == [token("新値")]


def test_pos_mutations_reject_aliases_that_cpp_test_loader_would_treat_as_unknown():
    replace = parse(run(mutation_tools.test_replace_pos(old_pos="Noun", new_pos="NOUN")))
    mapped = parse(run(mutation_tools.test_map_pos(surface="東京", old_pos="Noun", new_pos="VERB")))

    assert replace["status"] == "error"
    assert "Invalid new_pos" in replace["message"]
    assert mapped["status"] == "error"
    assert "Invalid new_pos" in mapped["message"]


def test_pos_mutations_refuse_case_level_oracle_overrides():
    replace = parse(run(mutation_tools.test_replace_pos(old_pos="Noun", new_pos="Verb", apply=True)))
    mapped = parse(run(mutation_tools.test_map_pos(surface="東京", old_pos="Noun", new_pos="Verb", apply=True)))

    assert replace["status"] == "error"
    assert mapped["status"] == "error"
    assert "oracle-owned" in replace["message"]
    assert "test_needs_suzume_update" in mapped["message"]


def test_diff_mecab_uses_one_raw_batch_and_reports_item_errors(tmp_path, monkeypatch):
    cases = [make_case("first", "一つ"), make_case("second", "二つ")]
    write_suite(tmp_path, "sample", cases)
    test_file = tmp_path / CORPUS_DIR / "sample.json"
    monkeypatch.setattr(read_tools, "_get_test_files_filtered", lambda _file: [test_file])
    calls = []

    def fake_mecab_batch(texts):
        calls.append(texts)
        return [
            ([token("生")], "MeCab", "intentional-rule"),
            ([], "error", "failed for second"),
        ]

    monkeypatch.setattr(read_tools, "get_mecab_tokens_batch_subprocess", fake_mecab_batch)

    data = parse(run(read_tools.test_diff_mecab()))

    assert calls == [["一つ", "二つ"]]
    assert data["summary"]["processed"] == 1
    assert data["summary"]["errors"] == 1
    assert data["categories"]["intentional"][0]["rule"] == "intentional-rule"
    assert data["errors"][0]["id"] == "sample/second"


def test_oracle_sync_skips_only_the_failed_batch_item(tmp_path, monkeypatch):
    cases = [make_case("first", "一つ"), make_case("second", "二つ")]
    write_suite(tmp_path, "sample", cases)
    test_file = tmp_path / CORPUS_DIR / "sample.json"
    monkeypatch.setattr(common_tools, "_get_test_files_filtered", lambda _file: [test_file])
    monkeypatch.setattr(
        common_tools,
        "get_expected_tokens_batch_subprocess",
        lambda _texts: [([token("新値")], "MeCab", ""), ([], "error", "second failed")],
    )

    data = parse(run(read_tools.test_needs_suzume_update(apply=False)))

    assert data["total"] == 1
    assert data["needs_update"][0]["id"] == "sample/first"
    assert data["errors"][0]["id"] == "sample/second"
