"""Integration tests for get_expected_tokens (requires MeCab)."""

import shutil
from unittest.mock import patch

import pytest

from suzume_mcp.core.postprocessors import POSTPROCESSORS
from suzume_mcp.core.suzume_utils import _oracle_text, format_expected, get_expected_tokens, tokens_match

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
        assert source == "MeCab+SuzumeRules"
        assert rule == "slang-adjective"

    @pytest.mark.parametrize(
        ("text", "expected_rule"),
        [
            ("バズった", "slang-verb"),
            ("打ち合わせをする", "word-exception"),
            ("ですっ", "word-exception"),
        ],
    )
    def test_preprocessed_mecab_input_reports_its_actual_rule(self, text, expected_rule):
        _tokens, source, rule = get_expected_tokens(text)
        assert source == "MeCab+SuzumeRules"
        assert expected_rule in rule.split("+")

    def test_janai_split(self):
        tokens, source, rule = get_expected_tokens("嫌じゃない")
        surfaces = [t["surface"] for t in tokens]
        assert "じゃ" in surfaces
        assert "ない" in surfaces

    @pytest.mark.parametrize(
        ("text", "expected_rule"),
        [
            ("ただで入る", "tada-context"),
            ("確認でも問題ない", "demo-adverbial-particle"),
        ],
    )
    def test_context_postprocessors_report_their_rule(self, text, expected_rule):
        _tokens, source, rule = get_expected_tokens(text)
        assert source == "MeCab+SuzumeRules"
        assert rule == expected_rule

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

    @pytest.mark.parametrize(
        ("text", "surface", "rule"),
        [
            ("１，２３４円", "1,234円", "number+unit"),
            ("ｔｅｓｔ＠ｅｘａｍｐｌｅ．ｊｐ", "test@example.jp", "email"),
        ],
    )
    def test_fullwidth_pretokenizer_units_share_core_normalization(self, text, surface, rule):
        tokens, _, applied_rule = get_expected_tokens(text)
        assert [token["surface"] for token in tokens] == [surface]
        assert applied_rule == rule

    def test_keycap_emoji_stays_one_search_unit(self):
        tokens, _, rule = get_expected_tokens("1️⃣です")
        assert [token["surface"] for token in tokens] == ["1️⃣", "です"]
        assert rule == "keycap-emoji"

    def test_word_exception_restoration_is_offset_scoped(self):
        tokens, _, _ = get_expected_tokens("確認を再確認する")
        assert "".join(token["surface"] for token in tokens) == "確認を再確認する"

    def test_word_exception_does_not_strand_a_verb_ending(self):
        tokens, _, rule = get_expected_tokens("日程を打ち合わせる")
        assert [token["surface"] for token in tokens] == ["日程", "を", "打ち合わせる"]
        assert rule != "word-exception"

    def test_word_exception_does_not_cut_a_quotative(self):
        tokens, _, rule = get_expected_tokens("そうですって")
        assert [token["surface"] for token in tokens] == ["そう", "です", "って"]
        assert rule != "word-exception"

    def test_whitespace_does_not_shift_merge_rule_anchor(self):
        tokens, _, rule = get_expected_tokens("彼は そんなら行く")
        assert [token["surface"] for token in tokens] == ["彼", "は", "そんなら", "行く"]
        assert rule == "fixed-function-search-unit"


class TestSurfaceIsNeverLost:
    """The expected tokens must cover every non-punctuation character.

    MeCab labels anything outside IPADIC as 記号, and the symbol filter drops
    記号 tokens, so an unknown character can silently vanish and leave a test
    asserting a segmentation of text that is not the input.
    """

    def test_supplementary_plane_kanji_survives(self):
        tokens, _, _ = get_expected_tokens("𩸽を焼く")
        assert [t["surface"] for t in tokens] == ["𩸽", "を", "焼く"]

    def test_fullwidth_letter_survives(self):
        tokens, _, _ = get_expected_tokens("Ａさんに聞く")
        assert "A" in [t["surface"] for t in tokens]

    def test_punctuation_is_still_dropped(self):
        tokens, _, _ = get_expected_tokens("東京。")
        assert [t["surface"] for t in tokens] == ["東京"]

    def test_declared_prolonged_sound_normalization_is_allowed(self):
        tokens, _, rule = get_expected_tokens("長いーー音を入力する")
        assert "".join(token["surface"] for token in tokens) == "長いー音を入力する"
        assert rule == "prolonged-sound-merge"

    @pytest.mark.parametrize("text", ["3枚あのーー", "そうそうあのーー"])
    def test_prolonged_sound_normalization_does_not_depend_on_the_first_rule(self, text):
        # Emphatic lengthening is kept: the tokenizer only collapses a repeated
        # mark directly before a kanji, which is what the case above covers.
        tokens, _, _ = get_expected_tokens(text)
        assert "".join(token["surface"] for token in tokens).endswith("あのーー")

    @pytest.mark.parametrize(
        ("source", "normalized"),
        [("ﾀﾞｻい服", "ダサい服"), ("ﾊﾞｽに乗る", "バスに乗る")],
    )
    def test_halfwidth_voiced_marks_share_the_core_coordinate_space(self, source, normalized):
        assert _oracle_text(source) == normalized

    @pytest.mark.parametrize(
        ("source", "normalized"),
        [
            ("１，２３４円", "1,234円"),
            ("ｔｅｓｔ＠ｅｘａｍｐｌｅ．ｊｐ", "test@example.jp"),
            ("ｖ１．２．３", "v1.2.3"),
            ("テ\u200bスト", "テスト"),
            ("1️⃣です", "1️⃣です"),
        ],
    )
    def test_oracle_text_matches_core_width_and_symbol_coordinates(self, source, normalized):
        assert _oracle_text(source) == normalized

    def test_unicode_text_is_reclassified_instead_of_lost(self):
        tokens, _, _ = get_expected_tokens("테스트を見る")
        assert "".join(token["surface"] for token in tokens) == "테스트を見る"
        assert tokens[0]["pos"] == "Noun"

    @pytest.mark.parametrize("symbol", ["￥", "€", "＄", "℃", "°", "№", "℡", "§", "±", "™", "©"])
    def test_meaningful_symbols_survive_the_filter(self, symbol):
        tokens, _, _ = get_expected_tokens(f"価格は{symbol}1です")
        expected_symbol = "$" if symbol == "＄" else symbol
        assert expected_symbol in [token["surface"] for token in tokens]

    @pytest.mark.parametrize(
        ("source", "normalized"),
        [
            ("か\u3099く", "がく"),
            ("か\u309bく", "がく"),
            ("は\u309aん", "ぱん"),
            ("は\u309cん", "ぱん"),
            ("カ\u3099ク", "ガク"),
        ],
    )
    def test_combining_kana_is_nfc_normalized(self, source, normalized):
        tokens, _, _ = get_expected_tokens(source)
        assert "".join(token["surface"] for token in tokens) == normalized

    def test_emoji_family_is_retained_like_the_cpp_default(self):
        tokens, _, rule = get_expected_tokens("👨‍👩‍👧")
        assert [token["surface"] for token in tokens] == ["👨‍👩‍👧"]
        assert tokens[0]["pos"] == "Other"
        assert rule == ""

    def test_unknown_non_punctuation_symbol_is_retained_as_other(self):
        raw_symbol = [{"surface": "↯", "pos": "記号", "pos_sub1": "一般", "lemma": "↯"}]
        with patch("suzume_mcp.core.suzume_utils.mecab_analyze", return_value=raw_symbol):
            tokens, _, _ = get_expected_tokens("↯")
        assert tokens[0]["surface"] == "↯"
        assert tokens[0]["pos"] == "Other"

    def test_duplicate_surface_raises_instead_of_poisoning_the_oracle(self):
        duplicated = [
            {"surface": "東京", "pos": "名詞", "lemma": "東京"},
            {"surface": "東京", "pos": "名詞", "lemma": "東京"},
        ]
        with (
            patch("suzume_mcp.core.suzume_utils.apply_suzume_split", return_value=(duplicated, "broken-rule")),
            pytest.raises(RuntimeError, match="do not reconstruct"),
        ):
            get_expected_tokens("東京")

    def test_postprocessor_surface_corruption_is_rejected(self):
        def corrupt_surface(tokens):
            tokens[0]["surface"] = "大阪"
            return True

        corrupting_rules = tuple(
            (label, corrupt_surface if label == "adverbial-na-adjective" else processor)
            for label, processor in POSTPROCESSORS
        )
        with (
            patch("suzume_mcp.core.suzume_utils.postprocessor_rules", return_value=corrupting_rules),
            pytest.raises(RuntimeError, match="do not reconstruct"),
        ):
            get_expected_tokens("東京")


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

    def test_lemma_included_when_same(self):
        """Lemma is always included, even when same as surface."""
        tokens = [{"surface": "食べる", "pos": "Verb", "lemma": "食べる"}]
        result = format_expected(tokens)
        assert result[0]["lemma"] == "食べる"
