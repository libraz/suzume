"""Tests for test tools (MCP tool functions)."""

import asyncio
import json
import shutil

import pytest

from suzume_mcp.tools import test_tools as _tt

pytestmark = pytest.mark.skipif(
    shutil.which("mecab") is None,
    reason="MeCab not installed",
)


def run(coro):
    return asyncio.run(coro)


def parse(result: str) -> dict:
    """Parse JSON result from tool function."""
    return json.loads(result)


class TestTestShow:
    def test_default_mode(self):
        result = run(_tt.test_show("食べる"))
        data = parse(result)
        assert data["input"] == "食べる"
        assert isinstance(data["expected"], list)
        assert isinstance(data["match"], bool)
        assert "diff_type" in data
        assert "diff_details" in data
        assert "rule" in data

    def test_tsv_mode(self):
        result = run(_tt.test_show("食べる", mode="tsv"))
        # TSV mode still returns plain text
        lines = result.strip().split("\n")
        assert len(lines) >= 1
        assert "\t" in lines[0]

    def test_brief_mode(self):
        result = run(_tt.test_show("食べる", mode="brief"))
        data = parse(result)
        assert data["mode"] == "brief"
        assert "expected" in data

    def test_json_mode(self):
        result = run(_tt.test_show("食べる", mode="json"))
        data = json.loads(result)
        assert isinstance(data, list)
        assert all("surface" in t and "pos" in t for t in data)


class TestTestList:
    def test_list(self):
        result = run(_tt.test_list())
        data = parse(result)
        assert "files" in data
        assert "total" in data
        assert isinstance(data["files"], list)
        assert data["total"] > 0


class TestTestSearch:
    def test_search_found(self):
        result = run(_tt.test_search("食べ"))
        data = parse(result)
        assert data["total"] > 0
        assert len(data["matches"]) > 0

    def test_search_not_found(self):
        result = run(_tt.test_search("zzzznonexistent"))
        data = parse(result)
        assert data["total"] == 0
        assert data["matches"] == []

    def test_search_with_limit(self):
        result = run(_tt.test_search("食べ", limit=2))
        data = parse(result)
        assert len(data["matches"]) <= 2


class TestTestFailed:
    def test_no_file(self):
        result = run(_tt.test_failed(test_output_file="/nonexistent/file.txt"))
        data = parse(result)
        assert data["total"] == 0
        assert data["failures"] == []

    def test_with_file(self, tmp_path):
        output_file = tmp_path / "test.txt"
        output_file.write_text(
            "Input: 食べています\n[  FAILED  ] Tokenize/Verb.Test (0 ms) GetParam() = verb_ichidan/tabeteiru\n",
            encoding="utf-8",
        )
        result = run(_tt.test_failed(test_output_file=str(output_file)))
        data = parse(result)
        assert data["total"] == 1
        assert data["failures"][0]["input"] == "食べています"


class TestTestCompare:
    def test_compare(self, tmp_path):
        before = tmp_path / "before.txt"
        after = tmp_path / "after.txt"
        before.write_text(
            "Input: 食べる\n[  FAILED  ] Tokenize/verb.Test (0 ms)\n"
            "Input: 走った\n[  FAILED  ] Tokenize/verb2.Test (0 ms)\n",
            encoding="utf-8",
        )
        after.write_text(
            "Input: 走った\n[  FAILED  ] Tokenize/verb2.Test (0 ms)\n",
            encoding="utf-8",
        )
        result = run(_tt.test_compare(str(before), str(after)))
        data = parse(result)
        assert data["before_failures"] == 2
        assert data["after_failures"] == 1


class TestTestListPos:
    def test_list_pos(self):
        result = run(_tt.test_list_pos())
        data = parse(result)
        assert "pos_values" in data
        pos_names = [p["pos"] for p in data["pos_values"]]
        assert "Noun" in pos_names or "Verb" in pos_names


class TestTestAcceptDiff:
    def test_missing_reason(self):
        result = run(_tt.test_accept_diff(input_text="食べる"))
        data = parse(result)
        assert data["status"] == "error"
        assert "reason" in data["message"]

    def test_missing_input(self):
        result = run(_tt.test_accept_diff(reason="test"))
        data = parse(result)
        assert data["status"] == "error"
        assert "Either input_text or all_failed" in data["message"]

    def test_not_found(self):
        result = run(_tt.test_accept_diff(input_text="zzzznonexistent", reason="test"))
        data = parse(result)
        assert data["status"] == "error"
        assert "No test found" in data["message"]


class TestTestResetSuzume:
    def test_missing_args(self):
        result = run(_tt.test_reset_suzume())
        data = parse(result)
        assert data["status"] == "error"
        assert "Either input_text or all_tests" in data["message"]

    def test_not_found(self):
        result = run(_tt.test_reset_suzume(input_text="zzzznonexistent"))
        data = parse(result)
        assert data["status"] == "error"
        assert "No test found" in data["message"]


class TestTestValidateIds:
    def test_validate(self):
        result = run(_tt.test_validate_ids())
        data = parse(result)
        assert "problems" in data
        assert "total" in data


class TestTestCheckCoverage:
    def test_coverage(self):
        result = run(_tt.test_check_coverage(inputs=["食べる", "zzzznonexistent"]))
        data = parse(result)
        assert "existing" in data
        assert "missing" in data
        assert "summary" in data
        assert data["summary"]["missing"] >= 1


class TestTestSuggestFile:
    def test_verb(self):
        result = run(_tt.test_suggest_file("食べている"))
        data = parse(result)
        assert "suggestions" in data
        assert len(data["suggestions"]) > 0

    def test_adjective(self):
        result = run(_tt.test_suggest_file("美しい"))
        data = parse(result)
        assert "suggestions" in data
        assert len(data["suggestions"]) > 0
