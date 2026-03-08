"""Tests for POS mapping logic."""

from suzume_mcp.core.pos_mapping import map_mecab_pos, normalize_pos


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

    def test_toiu_determiner(self):
        token = {"surface": "という", "pos": "助詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Determiner"

    def test_yoku_adjective(self):
        token = {"surface": "よく", "pos": "副詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Adjective"
        assert token["lemma"] == "よい"

    def test_dou_adjective(self):
        token = {"surface": "どう", "pos": "副詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Adjective"

    def test_cho_noun(self):
        token = {"surface": "超", "pos": "接頭詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Noun"

    def test_kirai_adjective(self):
        token = {"surface": "嫌い", "pos": "動詞", "pos_sub1": "", "pos_sub2": ""}
        assert map_mecab_pos(token) == "Adjective"


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
