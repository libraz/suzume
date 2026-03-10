"""Tests for postprocessor functions."""

from suzume_mcp.core.postprocessors import (
    postprocess_copula_neg,
    postprocess_de_particle,
    postprocess_demo,
    postprocess_ii,
    postprocess_ikaga,
    postprocess_iru_aux,
    postprocess_na_adj_noun,
    postprocess_sou,
    postprocess_te,
    postprocess_tsuke_noun,
    preprocess_for_mecab,
)


def _tok(surface, pos, **kw):
    t = {"surface": surface, "pos": pos, "lemma": surface}
    t.update(kw)
    return t


class TestPreprocessForMecab:
    def test_slang_adj(self):
        text, reps = preprocess_for_mecab("エモい")
        assert "エモ" not in text
        assert len(reps) > 0

    def test_slang_verb(self):
        text, reps = preprocess_for_mecab("バズった")
        assert "バズ" not in text

    def test_word_exception(self):
        text, reps = preprocess_for_mecab("小供")
        assert text == "供給"

    def test_no_change(self):
        text, reps = preprocess_for_mecab("食べる")
        assert text == "食べる"
        assert len(reps) == 0


class TestPostprocessSou:
    def test_sou_before_copula(self):
        tokens = [_tok("そう", "Adverb"), _tok("だ", "Auxiliary")]
        postprocess_sou(tokens)
        assert tokens[0]["pos"] == "Adjective"

    def test_sou_standalone(self):
        tokens = [_tok("そう", "Adverb"), _tok("食べる", "Verb")]
        postprocess_sou(tokens)
        assert tokens[0]["pos"] == "Adverb"

    def test_katakana_stem_before_sou(self):
        tokens = [_tok("キモ", "Noun"), _tok("そう", "Auxiliary")]
        postprocess_sou(tokens)
        assert tokens[0]["pos"] == "Adjective"
        assert tokens[0]["lemma"] == "キモい"


class TestPostprocessIkaga:
    def test_ikaga_with_copula(self):
        tokens = [_tok("いかが", "Adjective"), _tok("ですか", "Auxiliary")]
        postprocess_ikaga(tokens)
        assert tokens[0]["pos"] == "Adjective"  # Keep as-is

    def test_ikaga_standalone(self):
        tokens = [_tok("いかが", "Adjective"), _tok("食べ", "Verb")]
        postprocess_ikaga(tokens)
        assert tokens[0]["pos"] == "Adverb"


class TestPostprocessDemo:
    def test_demo_after_interrogative(self):
        tokens = [_tok("何", "Noun"), _tok("でも", "Particle")]
        postprocess_demo(tokens)
        assert tokens[1]["pos"] == "Particle"

    def test_demo_conjunction(self):
        tokens = [_tok("でも", "Particle"), _tok("始め", "Verb")]
        postprocess_demo(tokens)
        assert tokens[0]["pos"] == "Conjunction"


class TestPostprocessIi:
    def test_ii_adjective(self):
        tokens = [_tok("いい", "Verb", lemma="いう")]
        postprocess_ii(tokens)
        assert tokens[0]["pos"] == "Adjective"

    def test_ii_before_verb(self):
        tokens = [_tok("いい", "Verb", lemma="いう"), _tok("出す", "Verb")]
        postprocess_ii(tokens)
        assert tokens[0]["pos"] == "Verb"  # Keep as verb


class TestPostprocessIruAux:
    def test_iru_after_te(self):
        tokens = [_tok("て", "Particle"), _tok("いる", "Verb")]
        postprocess_iru_aux(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_iru_standalone(self):
        tokens = [_tok("猫", "Noun"), _tok("いる", "Verb")]
        postprocess_iru_aux(tokens)
        assert tokens[1]["pos"] == "Verb"


class TestPostprocessDeParticle:
    def test_de_before_ha(self):
        tokens = [_tok("で", "Auxiliary"), _tok("は", "Particle")]
        postprocess_de_particle(tokens)
        assert tokens[0]["pos"] == "Particle"

    def test_de_after_adjective_stays_auxiliary(self):
        """で after Adjective stays Auxiliary (copula て-form)."""
        tokens = [_tok("好き", "Adjective"), _tok("で", "Auxiliary")]
        postprocess_de_particle(tokens)
        assert tokens[1]["pos"] == "Auxiliary"


class TestPostprocessNaAdjNoun:
    def test_adj_before_sugiru_stays_adjective(self):
        """Adjective stems before すぎる stay Adjective (no-op)."""
        tokens = [_tok("複雑", "Adjective"), _tok("すぎる", "Verb")]
        postprocess_na_adj_noun(tokens)
        assert tokens[0]["pos"] == "Adjective"


class TestPostprocessTsukeNoun:
    def test_tsuke(self):
        tokens = [_tok("付け", "Suffix")]
        postprocess_tsuke_noun(tokens)
        assert tokens[0]["pos"] == "Noun"


class TestPostprocessCopulaNeg:
    def test_naku_after_ja(self):
        tokens = [_tok("じゃ", "Auxiliary"), _tok("なく", "Auxiliary")]
        postprocess_copula_neg(tokens)
        assert tokens[1]["pos"] == "Adjective"


class TestPostprocessTe:
    def test_te_after_verb(self):
        tokens = [_tok("食べ", "Verb"), _tok("て", "Particle")]
        postprocess_te(tokens)
        assert tokens[1]["pos"] == "Auxiliary"
