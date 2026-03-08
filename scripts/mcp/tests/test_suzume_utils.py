"""Integration tests for get_expected_tokens (requires MeCab)."""

import shutil

import pytest

from suzume_mcp.core.suzume_utils import format_expected, get_expected_tokens, tokens_match

pytestmark = pytest.mark.skipif(
    shutil.which("mecab") is None,
    reason="MeCab not installed",
)


class TestGetExpectedTokens:
    """Test get_expected_tokens with representative inputs."""

    def test_simple_sentence(self):
        tokens, source, rule = get_expected_tokens("食べる")
        surfaces = [t["surface"] for t in tokens]
        assert "食べる" in surfaces

    def test_number_counter(self):
        tokens, source, rule = get_expected_tokens("3人")
        assert len(tokens) == 1
        assert tokens[0]["surface"] == "3人"

    def test_kanji_compound(self):
        tokens, source, rule = get_expected_tokens("経済成長")
        assert any(t["surface"] == "経済成長" for t in tokens)

    def test_date(self):
        tokens, source, rule = get_expected_tokens("2024年12月23日")
        assert any(t["surface"] == "2024年12月23日" for t in tokens)

    def test_slang_adjective(self):
        tokens, source, rule = get_expected_tokens("エモい")
        surfaces = [t["surface"] for t in tokens]
        assert "エモい" in surfaces or "エモ" in surfaces

    def test_janai_split(self):
        tokens, source, rule = get_expected_tokens("嫌じゃない")
        surfaces = [t["surface"] for t in tokens]
        assert "じゃ" in surfaces
        assert "ない" in surfaces

    def test_symbols_filtered(self):
        tokens, source, rule = get_expected_tokens("（テスト）")
        surfaces = [t["surface"] for t in tokens]
        assert "（" not in surfaces
        assert "）" not in surfaces

    def test_fullwidth_normalized(self):
        tokens, source, rule = get_expected_tokens("１２３")
        for t in tokens:
            s = t["surface"]
            assert "１" not in s  # Should be half-width


class TestTokensMatch:
    def test_match(self):
        a = [{"surface": "食べ", "pos": "Verb"}, {"surface": "た", "pos": "Auxiliary"}]
        b = [{"surface": "食べ", "pos": "Verb"}, {"surface": "た", "pos": "Auxiliary"}]
        assert tokens_match(a, b)

    def test_pos_normalization(self):
        a = [{"surface": "食べ", "pos": "VERB"}]
        b = [{"surface": "食べ", "pos": "Verb"}]
        assert tokens_match(a, b)

    def test_mismatch_surface(self):
        a = [{"surface": "食べ", "pos": "Verb"}]
        b = [{"surface": "食", "pos": "Verb"}]
        assert not tokens_match(a, b)

    def test_mismatch_length(self):
        a = [{"surface": "食べ", "pos": "Verb"}]
        b = [{"surface": "食べ", "pos": "Verb"}, {"surface": "た", "pos": "Auxiliary"}]
        assert not tokens_match(a, b)


class TestFormatExpected:
    def test_basic(self):
        tokens = [{"surface": "食べ", "pos": "Verb", "lemma": "食べる"}]
        result = format_expected(tokens)
        assert result[0]["surface"] == "食べ"
        assert result[0]["pos"] == "Verb"
        assert result[0]["lemma"] == "食べる"

    def test_no_lemma_when_same(self):
        tokens = [{"surface": "食べる", "pos": "Verb", "lemma": "食べる"}]
        result = format_expected(tokens)
        assert "lemma" not in result[0]
