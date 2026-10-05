"""Tests for POS mapping logic."""

import pytest

from suzume_mcp.core.pos_mapping import correct_mecab_pos, map_mecab_pos, normalize_pos


def test_particle_correction_accepts_raw_japanese_noun_tag():
    tokens = [{"surface": "ぞ", "pos": "名詞", "lemma": "ぞ"}]
    correct_mecab_pos(tokens)
    assert tokens[0]["pos"] == "Particle"


def test_particle_correction_preserves_suffix_subclassification():
    tokens = [{"surface": "さ", "pos": "名詞", "pos_sub1": "接尾", "lemma": "さ"}]
    correct_mecab_pos(tokens)
    assert tokens[0]["pos"] == "名詞"
    assert map_mecab_pos(tokens[0]) == "Suffix"


class TestMapMecabPos:
    """Test map_mecab_pos with representative patterns."""

    def test_basic_noun(self):
        assert map_mecab_pos("名詞") == "Noun"

    def test_basic_verb(self):
        assert map_mecab_pos("動詞") == "Verb"

    def test_basic_adjective(self):
        assert map_mecab_pos("形容詞") == "Adjective"

    def test_basic_particle(self):
        assert map_mecab_pos("助詞") == "Particle"

    def test_basic_auxiliary(self):
        assert map_mecab_pos("助動詞") == "Auxiliary"

    def test_already_english(self):
        assert map_mecab_pos("Noun") == "Noun"
        assert map_mecab_pos("Verb") == "Verb"

    def test_unknown_pos(self):
        assert map_mecab_pos("未知") == "Other"

    def test_adverb_override_izure(self):
        token = {"surface": "いずれ", "pos": "名詞", "pos_sub1": "副詞可能", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Adverb"

    def test_lexical_mamonaku_stays_adverb(self):
        token = {"surface": "間もなく", "pos": "副詞", "lemma": "間もなく"}
        correct_mecab_pos([token])
        assert token == {"surface": "間もなく", "pos": "副詞", "lemma": "間もなく"}
        assert map_mecab_pos(token) == "Adverb"

    def test_fixed_adverb_overrides(self):
        for surface in ("一切", "一切合切", "いっさい", "いま", "このほど", "たかだか", "むしろ"):
            token = {"surface": surface, "pos": "名詞", "pos_sub1": "", "pos_sub2": ""}
            assert map_mecab_pos(token) == "Adverb"

    def test_pronoun_override_anata(self):
        token = {"surface": "あなた", "pos": "名詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Pronoun"

    def test_na_adj_suki(self):
        token = {"surface": "好き", "pos": "名詞", "pos_sub1": "形容動詞語幹", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Adjective"

    def test_na_adj_exception_maji(self):
        token = {"surface": "マジ", "pos": "名詞", "pos_sub1": "形容動詞語幹", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Noun"

    def test_n_noun_to_particle(self):
        token = {"surface": "ん", "pos": "名詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Particle"
        assert token["lemma"] == "の"

    def test_n_auxiliary(self):
        token = {"surface": "ん", "pos": "助動詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Auxiliary"

    def test_nara_to_particle(self):
        token = {"surface": "なら", "pos": "助動詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Particle"

    def test_symbol_bracket(self):
        token = {"surface": "（", "pos": "記号", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Symbol"

    def test_verb_hijiritu_iru(self):
        token = {"surface": "いる", "pos": "動詞", "pos_sub1": "非自立", "pos_sub2": "", "lemma": "いる"}
        assert map_mecab_pos(token) == "Verb"

    def test_verb_hijiritu_shimau(self):
        token = {"surface": "しまう", "pos": "動詞", "pos_sub1": "非自立", "pos_sub2": "", "lemma": "しまう"}
        assert map_mecab_pos(token) == "Auxiliary"

    def test_suffix_tachi(self):
        token = {"surface": "たち", "pos": "名詞", "pos_sub1": "接尾", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Suffix"

    def test_suffix_naka_noun(self):
        token = {"surface": "中", "pos": "名詞", "pos_sub1": "接尾", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Noun"

    @pytest.mark.parametrize("surface", ["という", "といった"])
    def test_quotative_determiner(self, surface):
        token = {"surface": surface, "pos": "助詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Determiner"

    def test_yoku_adjective(self):
        token = {"surface": "よく", "pos": "副詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Adjective"
        assert token["lemma"] == "よい"

    def test_dou_stays_adverb(self):
        token = {"surface": "どう", "pos": "副詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Adverb"

    def test_cho_noun(self):
        token = {"surface": "超", "pos": "接頭詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Noun"

    def test_kirai_adjective(self):
        token = {"surface": "嫌い", "pos": "動詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Adjective"


class TestSymbolMisclassification:
    """IPADIC files anything it does not know under 記号.

    A 記号 token is dropped by the symbol filter, so leaving real text labelled
    that way loses the character from the expected tokens entirely.
    """

    def test_supplementary_plane_kanji_becomes_a_noun(self):
        tokens = [{"surface": "𠮟", "pos": "記号", "pos_sub1": "一般"}]
        correct_mecab_pos(tokens)
        assert tokens[0]["pos"] == "名詞"

    @pytest.mark.parametrize("codepoint", [0x2B820, 0x2CEB0, 0x2EBF0, 0x2F800, 0x30000, 0x31350, 0x323B0])
    def test_modern_cjk_extensions_become_nouns(self, codepoint):
        surface = chr(codepoint)
        tokens = [{"surface": surface, "pos": "記号", "pos_sub1": "一般"}]
        correct_mecab_pos(tokens)
        assert tokens[0]["pos"] == "名詞"

    @pytest.mark.parametrize("surface", ["café", "тест", "τεστ", "테스트", "ทดสอบ", "ا"])
    def test_unicode_text_becomes_a_noun(self, surface):
        tokens = [{"surface": surface, "pos": "記号", "pos_sub1": "一般"}]
        correct_mecab_pos(tokens)
        assert tokens[0]["pos"] == "名詞"

    def test_standalone_letter_becomes_a_noun(self):
        tokens = [{"surface": "Ａ", "pos": "記号", "pos_sub1": "アルファベット"}]
        correct_mecab_pos(tokens)
        assert tokens[0]["pos"] == "名詞"

    @pytest.mark.parametrize("surface", ["￥", "€", "＄", "℃", "°", "№", "℡", "§", "±", "™", "©"])
    def test_meaningful_symbols_become_other(self, surface):
        tokens = [{"surface": surface, "pos": "記号", "pos_sub1": "一般"}]
        correct_mecab_pos(tokens)
        assert tokens[0]["pos"] == "その他"

    def test_real_punctuation_stays_a_symbol(self):
        tokens = [
            {"surface": "。", "pos": "記号", "pos_sub1": "句点"},
            {"surface": "、", "pos": "記号", "pos_sub1": "読点"},
            {"surface": "（", "pos": "記号", "pos_sub1": "括弧開"},
        ]
        correct_mecab_pos(tokens)
        assert [t["pos"] for t in tokens] == ["記号", "記号", "記号"]


class TestNormalizePos:
    def test_uppercase(self):
        assert normalize_pos("NOUN") == "Noun"
        assert normalize_pos("VERB") == "Verb"

    def test_already_normalized(self):
        assert normalize_pos("Noun") == "Noun"

    def test_adnominal_to_determiner(self):
        assert normalize_pos("Adnominal") == "Determiner"

    def test_filler_to_other(self):
        assert normalize_pos("Filler") == "Other"

    def test_short_forms(self):
        assert normalize_pos("ADJ") == "Adjective"
        assert normalize_pos("ADV") == "Adverb"
        assert normalize_pos("PART") == "Particle"
        assert normalize_pos("AUX") == "Auxiliary"
        assert normalize_pos("DET") == "Determiner"
        assert normalize_pos("PRON") == "Pronoun"


def test_contracted_sou_before_ka_is_the_demonstrative_adverb():
    tokens = [
        {"surface": "そっ", "pos": "名詞", "pos_sub1": "代名詞", "lemma": "そっ"},
        {"surface": "か", "pos": "助詞", "lemma": "か"},
    ]
    correct_mecab_pos(tokens)
    assert (tokens[0]["pos"], tokens[0]["lemma"]) == ("副詞", "そう")
