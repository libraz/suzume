"""Tests for MeCab interface (requires MeCab installed)."""

import shutil

import pytest

from suzume_mcp.core.mecab import mecab_analyze

pytestmark = pytest.mark.skipif(
    shutil.which("mecab") is None,
    reason="MeCab not installed",
)


def test_basic_sentence():
    tokens = mecab_analyze("食べる")
    assert len(tokens) >= 1
    assert tokens[0]["surface"] == "食べる"
    assert tokens[0]["pos"] == "動詞"


def test_multi_token():
    tokens = mecab_analyze("猫が走る")
    surfaces = [t["surface"] for t in tokens]
    assert "猫" in surfaces
    assert "走る" in surfaces


def test_has_all_fields():
    tokens = mecab_analyze("食べる")
    t = tokens[0]
    assert "surface" in t
    assert "pos" in t
    assert "pos_sub1" in t
    assert "lemma" in t


def test_empty_input():
    tokens = mecab_analyze("")
    assert tokens == []


def test_ascii_input():
    tokens = mecab_analyze("hello")
    assert len(tokens) >= 1
