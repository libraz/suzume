"""Tests for dict tools (MCP tool functions)."""

import asyncio
import json
import shutil

import pytest

from suzume_mcp.tools.dict_tools import (
    dict_check,
    dict_cleanup,
    dict_grep,
    dict_sort,
    dict_suggest,
)

pytestmark = pytest.mark.skipif(
    shutil.which("mecab") is None,
    reason="MeCab not installed",
)


def run(coro):
    return asyncio.run(coro)


def parse(result_str: str) -> dict:
    return json.loads(result_str)


class TestDictCheck:
    def test_known_word(self):
        result = parse(run(dict_check("食べる")))
        assert result["word"] == "食べる"
        assert result["mecab"]["is_single"] is True
        assert len(result["mecab"]["tokens"]) == 1

    def test_compound_word(self):
        result = parse(run(dict_check("Wi-Fi")))
        # Either MeCab splits it or it already exists
        assert "mecab" in result or result.get("in_dictionary") is True


class TestDictSuggest:
    def test_verb(self):
        result = parse(run(dict_suggest("食べる")))
        assert result["suggestion"]["pos"] == "VERB"
        assert result["suggestion"]["conj_type"] == "ICHIDAN"

    def test_noun(self):
        result = parse(run(dict_suggest("経済")))
        assert result["suggestion"]["pos"] == "NOUN"


class TestDictSort:
    def test_sort_dry_run(self):
        result = parse(run(dict_sort(file="data/core/verbs.tsv", dry_run=True)))
        assert result["total_entries"] > 0
        assert result.get("dry_run") is True
        assert any("Godan" in g["name"] or "Ichidan" in g["name"] for g in result["groups"])

    def test_sort_invalid_user(self):
        result = parse(run(dict_sort(user="nonexistent")))
        assert result["status"] == "error"
        assert "Invalid user category" in result["message"]

    def test_sort_no_args(self):
        result = parse(run(dict_sort()))
        assert result["status"] == "error"
        assert "Specify file" in result["message"]


class TestDictCleanup:
    def test_cleanup_dry_run(self):
        result = parse(run(dict_cleanup(input_file="data/core/verbs.tsv")))
        assert result["total"] > 0
        assert result.get("dry_run") is True

    def test_cleanup_not_found(self):
        result = parse(run(dict_cleanup(input_file="nonexistent.tsv")))
        assert result["status"] == "error"
        assert "not found" in result["message"]


class TestDictGrep:
    def test_grep_pattern(self):
        result = parse(run(dict_grep("^Wi")))
        assert result["total"] >= 0
        assert "matches" in result

    def test_grep_no_match(self):
        result = parse(run(dict_grep("^zzzznonexistent")))
        assert result["total"] == 0
        assert result["matches"] == []
