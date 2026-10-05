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
            ("ですっ", "emphatic-sokuon"),
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

    @pytest.mark.parametrize(
        ("text", "surfaces"),
        [
            ("Baby, I need you", ["Baby", "I", "need", "you"]),
            ("犬 猫", ["犬", "猫"]),
            ("ワタシハ ロボット デス", ["ワタシハ", "ロボット", "デス"]),
            ("今日も仕事 #社畜", ["今日", "も", "仕事", "#社畜"]),
            ("#社畜 仕事", ["#社畜", "仕事"]),
            ("今日も仕事　#社畜", ["今日", "も", "仕事", "#社畜"]),
        ],
    )
    def test_whitespace_is_a_hard_boundary(self, text, surfaces):
        tokens, _, _ = get_expected_tokens(text)
        assert [token["surface"] for token in tokens] == surfaces

    @pytest.mark.parametrize(
        ("text", "surfaces"),
        [
            ("好きになるしさ", ["好き", "に", "なる", "し", "さ"]),
            ("行きたいしさ", ["行き", "たい", "し", "さ"]),
            ("遊びたいしね", ["遊び", "たい", "し", "ね"]),
        ],
    )
    def test_predicate_and_listing_shi_are_restored(self, text, surfaces):
        tokens, _, rule = get_expected_tokens(text)
        assert [token["surface"] for token in tokens] == surfaces
        assert tokens[-1]["pos"] == "Particle"
        assert rule == "predicate-shi"

    def test_classical_terminal_without_a_modern_final_particle_is_kept(self):
        tokens, _, _ = get_expected_tokens("春はあけぼの、白し")
        assert tokens[-1]["surface"] == "白し"

    def test_postprocessors_do_not_join_across_removed_punctuation(self):
        tokens, _, _ = get_expected_tokens("書き、直して")
        assert [token["surface"] for token in tokens] == ["書き", "直し", "て"]
        tokens, _, _ = get_expected_tokens("書き直して")
        assert [token["surface"] for token in tokens] == ["書き直し", "て"]


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
        with patch("suzume_mcp.core.postprocessor_mecab.mecab_analyze", return_value=raw_symbol):
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


class TestStrandedAdjectiveStem:
    def test_finds_a_slang_adjective_no_list_names(self):
        tokens, _, _ = get_expected_tokens("ムズい問題だ")
        assert [token["surface"] for token in tokens] == ["ムズい", "問題", "だ"]
        assert tokens[0]["pos"] == "Adjective"

    def test_recovers_the_stranded_negative_continuative(self):
        tokens, _, _ = get_expected_tokens("エモくない話")
        assert [token["surface"] for token in tokens] == ["エモく", "ない", "話"]
        assert tokens[0]["pos"] == "Adjective"

    def test_leaves_a_kana_nominal_before_a_real_verb(self):
        tokens, _, _ = get_expected_tokens("ねこいる")
        assert [token["surface"] for token in tokens] == ["ねこ", "いる"]

    def test_leaves_a_kana_nominal_before_a_real_adjective(self):
        tokens, _, _ = get_expected_tokens("バリかっこいい")
        assert [token["surface"] for token in tokens] == ["バリ", "かっこいい"]


class TestEmphaticFinalSokuon:
    def test_keeps_the_adjective_the_mark_closes(self):
        tokens, _, _ = get_expected_tokens("すごっ")
        assert [token["surface"] for token in tokens] == ["すごっ"]
        assert tokens[0]["pos"] == "Adjective"
        assert tokens[0]["lemma"] == "すごい"

    def test_repairs_a_boundary_the_mark_was_glued_into(self):
        tokens, _, _ = get_expected_tokens("きれいっ")
        assert [token["surface"] for token in tokens] == ["きれいっ"]
        assert tokens[0]["pos"] == "Adjective"

    def test_keeps_the_mark_on_the_final_particle(self):
        tokens, _, _ = get_expected_tokens("やったぞっ")
        assert [token["surface"] for token in tokens] == ["やっ", "た", "ぞっ"]
        assert tokens[-1]["pos"] == "Particle"
        assert tokens[-1]["lemma"] == "ぞ"

    def test_leaves_a_word_the_dictionary_reads_whole(self):
        tokens, _, _ = get_expected_tokens("あっ")
        assert [token["surface"] for token in tokens] == ["あっ"]
        assert tokens[0]["lemma"] == "あっ"

    def test_leaves_the_copula_onbin_cell_alone(self):
        tokens, _, _ = get_expected_tokens("だめだっ")
        assert [token["surface"] for token in tokens] == ["だめ", "だっ"]


class TestClassicalContinuativeHost:
    @pytest.mark.parametrize(
        ("text", "expected"),
        [
            ("月見ぬべし", [("月", "Noun"), ("見", "Verb"), ("ぬ", "Auxiliary"), ("べし", "Auxiliary")]),
            ("水落ちぬれば", [("水", "Noun"), ("落ち", "Verb"), ("ぬれ", "Auxiliary"), ("ば", "Particle")]),
            ("日暮れぬれば", [("日", "Noun"), ("暮れ", "Verb"), ("ぬれ", "Auxiliary"), ("ば", "Particle")]),
            ("咲きければ", [("咲き", "Verb"), ("けれ", "Auxiliary"), ("ば", "Particle")]),
            ("月出づべし", [("月", "Noun"), ("出づ", "Verb"), ("べし", "Auxiliary")]),
        ],
    )
    def test_recovers_the_verb_a_continuative_cell_attaches_to(self, text, expected):
        tokens, _, _ = get_expected_tokens(text)
        assert [(token["surface"], token["pos"]) for token in tokens] == expected

    @pytest.mark.parametrize("text", ["花見に行く", "月見をする", "壁にける", "提出す", "谷深く"])
    def test_leaves_nominals_without_a_continuative_cell(self, text):
        before = {"花見に行く": "花見", "月見をする": "月見", "壁にける": "壁", "提出す": "提出", "谷深く": "谷"}[text]
        tokens, _, _ = get_expected_tokens(text)
        assert tokens[0]["surface"] == before


class TestClassicalTerminalAuxiliaryHost:
    @pytest.mark.parametrize(
        ("text", "expected"),
        [
            ("鳴くらむ", [("鳴く", "Verb"), ("らむ", "Auxiliary")]),
            ("花咲くらむ", [("花", "Noun"), ("咲く", "Verb"), ("らむ", "Auxiliary")]),
            ("見ゆらむ", [("見ゆ", "Verb"), ("らむ", "Auxiliary")]),
            ("恋ふらむ", [("恋ふ", "Verb"), ("らむ", "Auxiliary")]),
            ("見ゆめり", [("見ゆ", "Verb"), ("めり", "Auxiliary")]),
            ("漕ぐめり", [("漕ぐ", "Verb"), ("めり", "Auxiliary")]),
        ],
    )
    def test_restores_the_terminal_host(self, text, expected):
        tokens, _, _ = get_expected_tokens(text)
        assert [(token["surface"], token["pos"]) for token in tokens] == expected

    @pytest.mark.parametrize(
        ("text", "surfaces"),
        [("目がくらむ", ["目", "が", "くらむ"]), ("事故る", ["事故る"]), ("子供らむ", ["子供", "らむ"])],
    )
    def test_leaves_non_terminal_hosts(self, text, surfaces):
        tokens, _, _ = get_expected_tokens(text)
        assert [token["surface"] for token in tokens] == surfaces


class TestClassicalMuAfterAuxiliaryIrrealis:
    def test_reads_mu_after_tara_as_the_conjectural(self):
        tokens, _, _ = get_expected_tokens("来たらむ")
        assert [(token["surface"], token["pos"]) for token in tokens] == [
            ("来", "Verb"),
            ("たら", "Auxiliary"),
            ("む", "Auxiliary"),
        ]


class TestClassicalPerfectNuru:
    def test_joins_nu_and_ru_after_a_continuative(self):
        tokens, _, _ = get_expected_tokens("時ぞ過ぎぬる")
        assert [(token["surface"], token["pos"]) for token in tokens][-1] == ("ぬる", "Auxiliary")
        assert tokens[-1]["lemma"] == "ぬ"

    def test_keeps_the_negative_before_a_noun(self):
        tokens, _, _ = get_expected_tokens("見ぬ人")
        assert [token["surface"] for token in tokens] == ["見", "ぬ", "人"]


class TestClassicalPastShikaFusedHost:
    @pytest.mark.parametrize("text", ["来しかば", "来しかど", "出しかば"])
    def test_splits_the_realis_off_a_fused_host(self, text):
        tokens, _, _ = get_expected_tokens(text)
        assert [(token["surface"], token["pos"]) for token in tokens][:2] == [(text[0], "Verb"), ("しか", "Auxiliary")]

    @pytest.mark.parametrize("text", ["来すか", "話しかば"])
    def test_leaves_a_host_whose_stem_is_no_continuative(self, text):
        tokens, _, _ = get_expected_tokens(text)
        assert "しか" not in [token["surface"] for token in tokens]


class TestKanjiVerbFrame:
    @pytest.mark.parametrize(
        ("text", "verb", "lemma"),
        [("論を俟たない", "俟た", "俟つ"), ("失くさない", "失くさ", "失くす"), ("断じざるを得ない", "断じ", "断じる")],
    )
    def test_reads_an_unlisted_kanji_verb_through_a_row_frame(self, text, verb, lemma):
        tokens, _, _ = get_expected_tokens(text)
        token = next(token for token in tokens if token["surface"] == verb)
        assert (token["pos"], token["lemma"]) == ("Verb", lemma)

    def test_leaves_a_noun_before_a_kana_word(self):
        tokens, _, _ = get_expected_tokens("雪たくさん")
        assert [token["surface"] for token in tokens] == ["雪", "たくさん"]


class TestUnlistedKanaWords:
    @pytest.mark.parametrize(
        ("text", "word", "pos", "lemma"),
        [
            ("ぶっちゃけると", "ぶっちゃける", "Verb", "ぶっちゃける"),
            ("ぶっちゃけ無理です", "ぶっちゃけ", "Verb", "ぶっちゃける"),
            ("頑なに拒む", "頑な", "Adjective", "頑な"),
            ("ふとんで寝る", "ふとん", "Noun", "ふとん"),
            ("うれしくてうれぴい。", "うれぴい", "Adjective", "うれぴい"),
        ],
    )
    def test_keeps_a_word_the_reference_lacks_whole(self, text, word, pos, lemma):
        tokens, _, _ = get_expected_tokens(text)
        token = next(token for token in tokens if token["surface"] == word)
        assert (token["pos"], token["lemma"]) == (pos, lemma)

    def test_leaves_the_adverb_futo(self):
        tokens, _, _ = get_expected_tokens("ふと思った")
        assert tokens[0]["surface"] == "ふと"


class TestSlangAdjectiveStems:
    @pytest.mark.parametrize("text", ["ずっと共にいたい", "ここにいたい", "家にいたかった"])
    def test_reads_locative_ni_itai_as_iru_plus_desiderative(self, text):
        tokens, _, _ = get_expected_tokens(text)
        surfaces = [token["surface"] for token in tokens]
        assert surfaces[surfaces.index("に") + 1] == "い"
        assert tokens[surfaces.index("に") + 1]["lemma"] == "いる"

    def test_keeps_the_adjective_after_ga(self):
        tokens, _, _ = get_expected_tokens("頭がいたい")
        assert (tokens[-1]["surface"], tokens[-1]["pos"]) == ("いたい", "Adjective")

    @pytest.mark.parametrize(("text", "stem"), [("マジヤバい", "ヤバい"), ("マジヤバかった", "ヤバかっ")])
    def test_takes_the_stem_out_of_an_unknown_katakana_run(self, text, stem):
        tokens, _, _ = get_expected_tokens(text)
        assert [token["surface"] for token in tokens][:2] == ["マジ", stem]

    @pytest.mark.parametrize("text", ["ねこかわいすぎ", "ねこがかわいすぎ"])
    def test_reads_a_kana_stem_before_sugi(self, text):
        tokens, _, _ = get_expected_tokens(text)
        stem = next(token for token in tokens if token["surface"] == "かわい")
        assert (stem["pos"], stem["lemma"]) == ("Adjective", "かわいい")

    @pytest.mark.parametrize(("text", "word"), [("かわいそうな猫", "かわいそう"), ("読みやすそうだ。", "やす")])
    def test_leaves_a_lexeme_or_a_predicate_suffix(self, text, word):
        tokens, _, _ = get_expected_tokens(text)
        assert word in [token["surface"] for token in tokens]


class TestDerivedVerbSuffixSplit:
    @pytest.mark.parametrize(
        ("text", "host", "suffix", "lemma"),
        [
            ("春めいた日差し", "春", "めい", "めく"),
            ("謎めいた話", "謎", "めい", "めく"),
            ("冗談めかした言い方", "冗談", "めかし", "めかす"),
            ("大人ぶった発言が嫌い。", "大人", "ぶっ", "ぶる"),
            ("学者ぶって話す", "学者", "ぶっ", "ぶる"),
            ("謎めきたる文字", "謎", "めき", "めく"),
        ],
    )
    def test_keeps_host_and_suffix_apart(self, text, host, suffix, lemma):
        tokens, _, _ = get_expected_tokens(text)
        surfaces = [token["surface"] for token in tokens]
        index = surfaces.index(host)
        assert surfaces[index + 1] == suffix
        assert (tokens[index + 1]["pos"], tokens[index + 1]["lemma"]) == ("Verb", lemma)

    @pytest.mark.parametrize("text", ["時めく", "ときめく", "古めかしい建物", "艶めかしい"])
    def test_leaves_lexicalized_words_whole(self, text):
        tokens, _, _ = get_expected_tokens(text)
        assert len(tokens[0]["surface"]) >= 3


class TestTeAdverbAfterObject:
    def test_reads_the_adverb_after_wo_as_the_verb_te_form(self):
        tokens, _, _ = get_expected_tokens("約束を果たして帰る")
        assert [token["surface"] for token in tokens][2:4] == ["果たし", "て"]
        assert tokens[2]["lemma"] == "果たす"

    def test_keeps_the_clause_initial_adverb(self):
        tokens, _, _ = get_expected_tokens("果たして現状で十分だろうか")
        assert tokens[0]["surface"] == "果たして"


class TestMimeticRunGate:
    @pytest.mark.parametrize(
        ("text", "head"),
        [("いつかきっと再会したい", ["いつか", "きっと"]), ("もうちょっと待って", ["もう", "ちょっと"])],
    )
    def test_leaves_a_lexical_tto_adverb_after_another_word(self, text, head):
        tokens, _, _ = get_expected_tokens(text)
        assert [token["surface"] for token in tokens][:2] == head

    def test_leaves_a_run_opening_on_a_dependent_token(self):
        tokens, _, _ = get_expected_tokens("知ってるんじゃん。")
        assert [token["surface"] for token in tokens] == ["知っ", "てる", "ん", "じゃん"]

    @pytest.mark.parametrize(("text", "first"), [("えっと本", "えっと"), ("えっと驚いた", "えっ")])
    def test_reads_etto_as_the_filler_unless_a_predicate_follows(self, text, first):
        tokens, _, _ = get_expected_tokens(text)
        assert tokens[0]["surface"] == first


class TestPhraseFinalEmphaticSokuon:
    @pytest.mark.parametrize(
        ("text", "word"),
        [("来たぞっ！やったぜ", "ぞっ"), ("ちょっ…待ってください", "ちょっ"), ("ドアがバタンっ。", "バタンっ")],
    )
    def test_keeps_the_mark_on_its_host_before_punctuation(self, text, word):
        tokens, _, _ = get_expected_tokens(text)
        assert word in [token["surface"] for token in tokens]

    def test_leaves_a_sokuon_carrying_a_suffix(self):
        tokens, _, _ = get_expected_tokens("行ったって")
        assert [token["surface"] for token in tokens][:2] == ["行っ", "た"]


class TestPhraseFinalSmallVowel:
    @pytest.mark.parametrize("text", ["待ってますぅ", "行きますぅ！"])
    def test_keeps_the_drawn_out_auxiliary_with_its_lemma(self, text):
        tokens, _, _ = get_expected_tokens(text)
        assert (tokens[-1]["surface"], tokens[-1]["lemma"]) == ("ますぅ", "ます")

    def test_leaves_a_small_vowel_inside_a_word(self):
        tokens, _, _ = get_expected_tokens("ファイル")
        assert [token["surface"] for token in tokens] == ["ファイル"]


class TestVowelFusedAdjective:
    @pytest.mark.parametrize(
        ("text", "word", "lemma"),
        [("すげえな", "すげえ", "すごい"), ("これうめえ", "うめえ", "うまい"), ("ひでえ話だ", "ひでえ", "ひどい")],
    )
    def test_reads_the_fused_long_e_as_the_adjective(self, text, word, lemma):
        tokens, _, _ = get_expected_tokens(text)
        token = next(token for token in tokens if token["surface"] == word)
        assert (token["pos"], token["lemma"]) == ("Adjective", lemma)

    @pytest.mark.parametrize("text", ["知らねえ", "かもねえ", "食べるけえ"])
    def test_leaves_other_e_endings(self, text):
        tokens, _, _ = get_expected_tokens(text)
        assert all(token["pos"] != "Adjective" or token["lemma"] in ("ない",) for token in tokens)


class TestStraddlingReplacement:
    def test_drops_a_replacement_the_analysis_splits_and_reanalyzes(self):
        from suzume_mcp.core.postprocessor_mecab import _apply_replacements, analyze_preprocessed

        def fake_preprocess(text):
            replacements = {(0, "word_exception"): {"original": "にゃー", "replacement": "ねえ", "length": 3}}
            return _apply_replacements(text, replacements)

        def split_first_char(text):
            return [{"surface": text[:1]}, {"surface": text[1:]}]

        with (
            patch("suzume_mcp.core.postprocessor_mecab.preprocess_for_mecab", side_effect=fake_preprocess),
            patch("suzume_mcp.core.postprocessor_mecab.mecab_analyze", side_effect=split_first_char),
        ):
            tokens, replacements, _ = analyze_preprocessed("にゃー")
        assert replacements == {}
        assert "".join(token["surface"] for token in tokens) == "にゃー"

    @pytest.mark.parametrize(("text", "particle"), [("遊ぼうにゃーん", "にゃーん"), ("行くにゃー", "にゃー")])
    def test_keeps_the_character_speech_particle_whole(self, text, particle):
        tokens, _, _ = get_expected_tokens(text)
        assert (tokens[-1]["surface"], tokens[-1]["pos"], tokens[-1]["lemma"]) == (particle, "Particle", particle)


class TestCharacterSpeech:
    @pytest.mark.parametrize(
        ("text", "host", "particle"),
        [
            ("走るっぴ", "走る", "っぴ"),
            ("我が参加するもふ", "する", "もふ"),
            ("わかったぞい", "た", "ぞい"),
            ("遊ぶわん", "遊ぶ", "わん"),
        ],
    )
    def test_final_particle_closes_the_intact_predicate(self, text, host, particle):
        tokens, _, _ = get_expected_tokens(text)
        assert tokens[-2]["surface"] == host
        assert (tokens[-1]["surface"], tokens[-1]["pos"], tokens[-1]["lemma"]) == (particle, "Particle", particle)

    @pytest.mark.parametrize(
        ("text", "copula", "lemma"),
        [
            ("学生ざます", "ざます", "ざます"),
            ("彼も応じてくれるでやんす", "やんす", "やんす"),
            ("そうでござんす", "ござんす", "ござる"),
            ("やるっス", "っス", "です"),
        ],
    )
    def test_copula_is_one_auxiliary(self, text, copula, lemma):
        tokens, _, _ = get_expected_tokens(text)
        assert (tokens[-1]["surface"], tokens[-1]["pos"], tokens[-1]["lemma"]) == (copula, "Auxiliary", lemma)

    @pytest.mark.parametrize(
        ("text", "surfaces"),
        [
            ("それはえらいこっちゃ。", ["それ", "は", "えらい", "こっ", "ちゃ"]),
            ("ざまを見ろ", ["ざま", "を", "見ろ"]),
            ("お茶わん", ["お", "茶わん"]),
        ],
    )
    def test_leaves_homographs_outside_a_predicate_tail(self, text, surfaces):
        tokens, _, _ = get_expected_tokens(text)
        assert [token["surface"] for token in tokens] == surfaces


class TestSokuonAfterPast:
    @pytest.mark.parametrize(
        ("text", "surfaces"),
        [
            ("来たっ", ["来", "たっ"]),
            ("やったっ！すごい", ["やっ", "たっ", "すごい"]),
            ("妾も同行したっちゃ", ["妾", "も", "同行", "し", "た", "っちゃ"]),
            ("そうだったっちゃ", ["そう", "だっ", "た", "っちゃ"]),
        ],
    )
    def test_past_auxiliary_keeps_its_token(self, text, surfaces):
        tokens, _, _ = get_expected_tokens(text)
        assert [token["surface"] for token in tokens] == surfaces
        assert all(token["lemma"] == "た" for token in tokens if token["surface"] in ("た", "たっ"))


class TestPejorativeYagaru:
    @pytest.mark.parametrize(
        ("text", "host", "cell"),
        [
            ("来やがった、覚悟しろ", "来", "やがっ"),
            ("食べやがった", "食べ", "やがっ"),
            ("待たせやがって", "せ", "やがっ"),
            ("来やがれ", "来", "やがれ"),
            ("書きやがらない", "書き", "やがら"),
        ],
    )
    def test_auxiliary_follows_the_continuative(self, text, host, cell):
        tokens, _, _ = get_expected_tokens(text)
        surfaces = [token["surface"] for token in tokens]
        at = surfaces.index(cell)
        assert surfaces[at - 1] == host
        assert (tokens[at]["pos"], tokens[at]["lemma"]) == ("Auxiliary", "やがる")

    @pytest.mark.parametrize("text", ["寒がる", "やがて", "嫌やがな"])
    def test_leaves_other_ya_ga_sequences(self, text):
        tokens, _, _ = get_expected_tokens(text)
        assert all(token["lemma"] != "やがる" for token in tokens)


class TestColloquialNegative:
    @pytest.mark.parametrize(
        ("text", "host"),
        [("変わんねえ。", "変わん"), ("帰んねえよ", "帰ん"), ("知らねえ", "知ら"), ("変わらねえ", "変わら")],
    )
    def test_nee_after_an_irrealis_is_the_negative(self, text, host):
        tokens, _, _ = get_expected_tokens(text)
        surfaces = [token["surface"] for token in tokens]
        at = surfaces.index(host)
        assert (tokens[at + 1]["pos"], tokens[at + 1]["lemma"]) == ("Auxiliary", "ない")

    def test_final_particle_nee_stays_after_a_finite_form(self):
        tokens, _, _ = get_expected_tokens("いいねえ")
        assert tokens[-1]["pos"] == "Particle"


class TestNominalSuffixAfterHost:
    @pytest.mark.parametrize(
        ("text", "suffix"),
        [
            ("お声がけする", "がけ"),
            ("一日がけの仕事", "がけ"),
            ("飾りっけのない態度", "っけ"),
            ("塩っけがない", "っけ"),
        ],
    )
    def test_suffix_after_a_nominal(self, text, suffix):
        tokens, _, _ = get_expected_tokens(text)
        assert next(token for token in tokens if token["surface"] == suffix)["pos"] == "Suffix"

    @pytest.mark.parametrize("text", ["何だったっけ", "行ったっけ", "どこだっけ"])
    def test_recollective_particle_after_a_predicate(self, text):
        tokens, _, _ = get_expected_tokens(text)
        assert tokens[-1]["pos"] == "Particle"


class TestGeNaiAdjective:
    @pytest.mark.parametrize(("text", "tail"), [("危なげない", "ない"), ("危なげなく勝つ", "なく")])
    def test_splits_when_the_ge_noun_stands_alone(self, text, tail):
        tokens, _, _ = get_expected_tokens(text)
        assert [(token["surface"], token["pos"]) for token in tokens[:2]] == [("危なげ", "Noun"), (tail, "Adjective")]


class TestRateQuantities:
    @pytest.mark.parametrize(
        ("text", "quantities"),
        [("1泊5000円", ["1泊", "5000円"]), ("1日3回", ["1日", "3回"]), ("1個300円", ["1個", "300円"])],
    )
    def test_rate_splits_where_the_dimension_changes(self, text, quantities):
        tokens, _, _ = get_expected_tokens(text)
        assert [token["surface"] for token in tokens] == quantities

    @pytest.mark.parametrize("text", ["1泊2日", "2泊3日", "1時間30分", "3割5分", "3分の1"])
    def test_one_dimension_stays_one_quantity(self, text):
        tokens, _, _ = get_expected_tokens(text)
        assert [token["surface"] for token in tokens] == [text]
