"""Tests for postprocessor functions."""

import pytest

from suzume_mcp.core import postprocessors
from suzume_mcp.core.postprocessors import (
    POSTPROCESSORS,
    _non_overlapping_replacements,
    postprocess_adjective_garu,
    postprocess_adjective_nominalizer,
    postprocess_adverb_nominal_context,
    postprocess_attributive_mamonaku,
    postprocess_classical_focus_namu,
    postprocess_classical_perfect_aux,
    postprocess_closed_function_words,
    postprocess_closed_subsidiary_aux,
    postprocess_compound_case_particle_aru,
    postprocess_copula_neg,
    postprocess_de_particle,
    postprocess_demo,
    postprocess_deverbal_noun_context,
    postprocess_dewa_aru_boundary,
    postprocess_formal_noun_lemma,
    postprocess_fuu_formal_noun,
    postprocess_giving_aux,
    postprocess_hiragana_godan_wa_terminal,
    postprocess_hiragana_purpose_noun,
    postprocess_hiragana_yaka_adverbial,
    postprocess_honorific_i_adjective,
    postprocess_honorific_request,
    postprocess_i_adjective_upper_bound,
    postprocess_ii,
    postprocess_ikaga,
    postprocess_indefinite_ka,
    postprocess_iru_aux,
    postprocess_katakana_godan_te_form,
    postprocess_katakana_ichidan_continuative,
    postprocess_l2_noun_context,
    postprocess_mecab_tokens,
    postprocess_miru_aux,
    postprocess_modifier_godan_imperative,
    postprocess_na_adj_noun,
    postprocess_nara_verb,
    postprocess_negative_host,
    postprocess_onaji_predicate,
    postprocess_predicate_cell_lemmas,
    postprocess_productive_search_unit_boundaries,
    postprocess_productive_verb_suffix_stem,
    postprocess_quantity_bound_suffix,
    postprocess_renyokei_compound_particle,
    postprocess_shimau_aux,
    postprocess_short_hiragana_onbin,
    postprocess_shortened_causative_passive,
    postprocess_sou,
    postprocess_sou_host,
    postprocess_state_suffix,
    postprocess_subsidiary_yuku,
    postprocess_tagaru_aux,
    postprocess_teki_na_adjective,
    postprocess_temporal_nao,
    postprocess_to_areba_conditional,
    postprocess_tsuke_noun,
    postprocess_you_noun,
    preprocess_for_mecab,
)


def _tok(surface, pos, **kw):
    t = {"surface": surface, "pos": pos, "lemma": surface}
    t.update(kw)
    return t


def test_postprocessor_registry_is_complete_and_has_unique_labels():
    """Every context-dependent postprocessor must have one ordered rule entry."""
    rules = postprocessors.postprocessor_rules()
    labels = [label for label, _ in rules]
    defined = {
        processor
        for name, processor in vars(postprocessors).items()
        if name.startswith("postprocess_") and name != "postprocess_mecab_tokens" and callable(processor)
    }

    assert len(labels) == len(set(labels))
    assert {processor for _, processor in rules} == defined
    assert rules == POSTPROCESSORS


def test_postprocessor_registry_rejects_unexported_rule(monkeypatch):
    """A new owner-module rule must be registered even before facade export."""
    owner = postprocessors.postprocessor_nominals

    def postprocess_unregistered(tokens):
        return False

    postprocess_unregistered.__module__ = owner.__name__
    monkeypatch.setattr(owner, "postprocess_unregistered", postprocess_unregistered, raising=False)

    with pytest.raises(RuntimeError, match="unregistered postprocessor: postprocess_unregistered"):
        postprocessors.postprocessor_rules()


def test_postprocessor_registry_rejects_duplicate_labels(monkeypatch):
    monkeypatch.setattr(postprocessors, "POSTPROCESSORS", (*POSTPROCESSORS, POSTPROCESSORS[0]))

    with pytest.raises(RuntimeError, match="duplicate postprocessor rule label"):
        postprocessors.postprocessor_rules()


def test_postprocessor_registry_rejects_duplicate_functions(monkeypatch):
    rules = (*POSTPROCESSORS, ("second-label-for-same-processor", POSTPROCESSORS[0][1]))
    monkeypatch.setattr(postprocessors, "POSTPROCESSORS", rules)

    with pytest.raises(RuntimeError, match="duplicate postprocessor function"):
        postprocessors.postprocessor_rules()


class TestModifierGodanImperative:
    def test_clause_final_e_row_after_modifier_is_godan_imperative(self):
        tokens = [_tok("少し", "Adverb"), _tok("待て", "Verb", lemma="待てる")]

        assert postprocess_modifier_godan_imperative(tokens)
        assert tokens[1]["lemma"] == "待つ"

    def test_auxiliary_continuation_keeps_ichidan_lemma(self):
        tokens = [
            _tok("必ずしも", "Adverb"),
            _tok("食べ", "Verb", lemma="食べる"),
            _tok("ない", "Auxiliary"),
        ]

        assert not postprocess_modifier_godan_imperative(tokens)
        assert tokens[1]["lemma"] == "食べる"

    def test_connective_particle_keeps_ichidan_lemma(self):
        tokens = [
            _tok("すっかり", "Adverb"),
            _tok("忘れ", "Verb", lemma="忘れる"),
            _tok("て", "Particle"),
            _tok("しまっ", "Auxiliary", lemma="しまう"),
        ]

        assert not postprocess_modifier_godan_imperative(tokens)
        assert tokens[1]["lemma"] == "忘れる"


class TestKatakanaIchidanContinuative:
    def test_katakana_noun_before_masu_is_ichidan_stem(self):
        tokens = [_tok("バテ", "Noun"), _tok("まし", "Auxiliary", lemma="ます"), _tok("た", "Auxiliary")]

        assert postprocess_katakana_ichidan_continuative(tokens)
        assert tokens[0]["pos"] == "Verb"
        assert tokens[0]["lemma"] == "バテる"

    def test_loanword_with_long_vowel_stays_noun(self):
        tokens = [_tok("ケーキ", "Noun"), _tok("た", "Auxiliary", lemma="た")]

        assert not postprocess_katakana_ichidan_continuative(tokens)
        assert tokens[0]["pos"] == "Noun"

    def test_other_rows_stay_noun(self):
        tokens = [_tok("メモ", "Noun"), _tok("て", "Particle")]

        assert not postprocess_katakana_ichidan_continuative(tokens)


class TestKatakanaGodanTeForm:
    def test_quotative_before_masu_is_te_form(self):
        tokens = [
            _tok("メモ", "Noun"),
            _tok("って", "Particle"),
            _tok("まし", "Auxiliary", lemma="ます"),
            _tok("た", "Auxiliary"),
        ]

        assert postprocess_katakana_godan_te_form(tokens)
        assert [t["surface"] for t in tokens] == ["メモっ", "て", "まし", "た"]
        assert tokens[0]["lemma"] == "メモる"

    def test_quotative_before_predicate_stays(self):
        tokens = [_tok("カレー", "Noun"), _tok("って", "Particle"), _tok("何", "Pronoun")]

        assert not postprocess_katakana_godan_te_form(tokens)


class TestPreprocessForMecab:
    def test_overlapping_replacements_choose_the_leftmost_longest_span(self):
        candidates = {
            (0, "short"): {"original": "ab", "replacement": "X", "length": 2},
            (0, "long"): {"original": "abc", "replacement": "Y", "length": 3},
            (2, "overlap"): {"original": "cd", "replacement": "Z", "length": 2},
            (3, "next"): {"original": "de", "replacement": "W", "length": 2},
        }

        selected = _non_overlapping_replacements(candidates)

        assert list(selected) == [(0, "long"), (3, "next")]

    def test_slang_adj(self):
        text, reps, rules = preprocess_for_mecab("エモい")
        assert "エモ" not in text
        assert len(reps) > 0
        assert rules == ("slang-adjective",)

    def test_slang_verb(self):
        text, reps, rules = preprocess_for_mecab("バズった")
        assert "バズ" not in text
        assert rules == ("slang-verb",)

    def test_word_exception(self):
        text, reps, rules = preprocess_for_mecab("小供")
        assert text == "供給"
        assert rules == ("word-exception",)

    def test_word_exception_leaves_inflected_word_intact(self):
        text, replacements, rules = preprocess_for_mecab("日程を打ち合わせる")
        assert text == "日程を打ち合わせる"
        assert replacements == {}
        assert rules == ()

    def test_word_exception_leaves_quotative_intact(self):
        text, replacements, rules = preprocess_for_mecab("そうですって")
        assert text == "そうですって"
        assert replacements == {}
        assert rules == ()

    def test_no_change(self):
        text, reps, rules = preprocess_for_mecab("食べる")
        assert text == "食べる"
        assert len(reps) == 0
        assert rules == ()

    def test_word_exception_restores_only_recorded_offset(self):
        original = "確認を再確認する"
        processed, replacements, _ = preprocess_for_mecab(original)
        assert processed == "確認を確認する"
        tokens = [
            _tok("確認", "名詞"),
            _tok("を", "助詞"),
            _tok("確認", "名詞"),
            _tok("する", "動詞"),
        ]

        postprocess_mecab_tokens(tokens, original, replacements)

        assert [token["surface"] for token in tokens] == ["確認", "を", "再確認", "する"]


class TestPostprocessSou:
    def test_katakana_stem_before_sou(self):
        tokens = [_tok("キモ", "Noun"), _tok("そう", "Auxiliary")]
        postprocess_sou(tokens)
        assert tokens[0]["pos"] == "Adjective"
        assert tokens[0]["lemma"] == "キモい"


class TestPostprocessSouHost:
    def test_demonstrative_before_copula_is_adverb(self):
        tokens = [_tok("そう", "Adjective"), _tok("だ", "Auxiliary")]
        assert postprocess_sou_host(tokens)
        assert tokens[0]["pos"] == "Adverb"

    def test_demonstrative_before_verb_is_adverb(self):
        tokens = [_tok("そう", "Adverb"), _tok("食べる", "Verb")]
        assert not postprocess_sou_host(tokens)

    def test_hearsay_after_copula_is_auxiliary(self):
        tokens = [_tok("教師", "Noun"), _tok("だ", "Auxiliary"), _tok("そう", "Adjective"), _tok("だ", "Auxiliary")]
        assert postprocess_sou_host(tokens)
        assert tokens[2]["pos"] == "Auxiliary"

    def test_appearance_after_adjective_stem_is_auxiliary(self):
        tokens = [_tok("おいし", "Adjective"), _tok("そう", "Adjective"), _tok("だ", "Auxiliary")]
        assert postprocess_sou_host(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_na_adjective_noun_stem_is_auxiliary(self):
        tokens = [_tok("不安", "Noun"), _tok("そう", "Adverb"), _tok("だ", "Auxiliary")]
        assert postprocess_sou_host(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_temporal_noun_before_verb_keeps_adverb(self):
        tokens = [_tok("明日", "Noun"), _tok("そう", "Adverb"), _tok("する", "Verb")]
        assert not postprocess_sou_host(tokens)


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

    def test_demo_after_noun(self):
        tokens = [_tok("雨", "Noun"), _tok("でも", "Conjunction")]
        postprocess_demo(tokens)
        assert tokens[1]["pos"] == "Particle"

    def test_sentence_initial_demo_is_conjunction(self):
        tokens = [_tok("でも", "Particle"), _tok("始め", "Verb")]
        postprocess_demo(tokens)
        assert tokens[0]["pos"] == "Conjunction"

    def test_demo_after_symbol_is_conjunction(self):
        tokens = [_tok("。", "Symbol"), _tok("でも", "Particle"), _tok("始め", "Verb")]
        postprocess_demo(tokens)
        assert tokens[1]["pos"] == "Conjunction"


class TestPostprocessDeParticleNoop:
    def test_copula_before_binding_particle(self):
        tokens = [_tok("本", "Noun"), _tok("で", "Particle"), _tok("しか", "Particle")]
        postprocess_de_particle(tokens)
        assert tokens[1] == _tok("で", "Auxiliary", lemma="だ")

    def test_case_particle_without_binding_particle(self):
        tokens = [_tok("本", "Noun"), _tok("で", "Particle"), _tok("学ぶ", "Verb")]
        postprocess_de_particle(tokens)
        assert tokens[1] == _tok("で", "Particle")


class TestPostprocessClosedFunctionWords:
    def test_literary_conjunction(self):
        tokens = [_tok("しかるに", "Adverb"), _tok("確認", "Noun")]
        assert postprocess_closed_function_words(tokens)
        assert tokens[0] == _tok("しかるに", "Conjunction")

    def test_distributive_pronoun(self):
        tokens = [_tok("各々", "Noun"), _tok("判断", "Noun")]
        assert postprocess_closed_function_words(tokens)
        assert tokens[0] == _tok("各々", "Pronoun")

    def test_ambiguous_function_word_uses_adverbial_policy(self):
        tokens = [_tok("また", "Conjunction"), _tok("確認", "Noun")]
        assert postprocess_closed_function_words(tokens)
        assert tokens[0] == _tok("また", "Adverb")

    def test_formal_adverb_and_following_content_noun(self):
        tokens = [_tok("つとめて", "Noun"), _tok("水", "Suffix"), _tok("を", "Particle")]
        assert postprocess_closed_function_words(tokens)
        assert tokens == [_tok("つとめて", "Adverb"), _tok("水", "Noun"), _tok("を", "Particle")]

    def test_existing_l1_adverbs_override_reference_pos(self):
        for surface, source_pos in (("やや", "Noun"), ("およそ", "Adnominal")):
            tokens = [_tok(surface, source_pos)]
            assert postprocess_closed_function_words(tokens)
            assert tokens == [_tok(surface, "Adverb")]

    def test_ordinary_noun_is_unchanged(self):
        tokens = [_tok("確認", "Noun")]
        assert not postprocess_closed_function_words(tokens)


class TestPostprocessClosedSubsidiaryAux:
    def test_retags_finite_subsidiary_forms_after_renyokei(self):
        cases = (
            ("たまえ", "たまう"),
            ("あぐね", "あぐねる"),
            ("そこね", "そこねる"),
            ("そこなっ", "そこなう"),
        )
        for surface, lemma in cases:
            tokens = [_tok("読み", "Verb", lemma="読む"), _tok(surface, "Verb")]
            assert postprocess_closed_subsidiary_aux(tokens)
            assert tokens[1] == _tok(surface, "Auxiliary", lemma=lemma)

    def test_sentence_initial_lexical_homograph_is_unchanged(self):
        tokens = [_tok("そこなう", "Verb")]
        assert not postprocess_closed_subsidiary_aux(tokens)
        assert tokens == [_tok("そこなう", "Verb")]


class TestPostprocessStructuralFunctionWords:
    def test_classical_namu_before_quote(self):
        tokens = [_tok("なむ", "Interjection"), _tok("と", "Particle"), _tok("思う", "Verb")]
        assert postprocess_classical_focus_namu(tokens)
        assert tokens[0] == _tok("なむ", "Particle")

    def test_honorific_i_adjective(self):
        tokens = [_tok("お", "Prefix"), _tok("忙しい", "Noun")]
        assert postprocess_honorific_i_adjective(tokens)
        assert tokens[1] == _tok("忙しい", "Adjective")

    def test_renyokei_before_upper_bound_particle(self):
        tokens = [_tok("多く", "Noun"), _tok("とも", "Particle")]
        assert postprocess_i_adjective_upper_bound(tokens)
        assert tokens[0] == _tok("多く", "Adjective", lemma="多い")


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

    def test_iru_after_focused_te_form(self):
        tokens = [_tok("で", "Particle"), _tok("しか", "Particle", pos_sub1="係助詞"), _tok("い", "Verb", lemma="いる")]
        postprocess_iru_aux(tokens)
        assert tokens[2]["pos"] == "Auxiliary"

    @pytest.mark.parametrize(("particle", "sub"), [("は", "係助詞"), ("など", "副助詞"), ("ばかり", "副助詞")])
    def test_iru_after_topic_or_adverbial_particle_in_te_frame(self, particle, sub):
        tokens = [_tok("て", "Particle"), _tok(particle, "Particle", pos_sub1=sub), _tok("いる", "Verb", lemma="いる")]
        postprocess_iru_aux(tokens)
        assert tokens[2]["pos"] == "Auxiliary"

    def test_iru_after_conjunctive_particle_stays_verb(self):
        tokens = [
            _tok("て", "Particle"),
            _tok("から", "Particle", pos_sub1="接続助詞"),
            _tok("いる", "Verb", lemma="いる"),
        ]
        postprocess_iru_aux(tokens)
        assert tokens[2]["pos"] == "Verb"

    def test_conditional_iru_after_focus_retains_reference_pos(self):
        tokens = [_tok("で", "Particle"), _tok("さえ", "Particle"), _tok("いれ", "Verb", lemma="いる")]
        postprocess_iru_aux(tokens)
        assert tokens[2]["pos"] == "Verb"


class TestPostprocessGivingAux:
    def test_receiving_verb_after_te_is_auxiliary(self):
        tokens = [_tok("て", "Particle"), _tok("もらっ", "Verb", lemma="もらう")]
        assert postprocess_giving_aux(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_lexical_giving_verb_stays_verb(self):
        tokens = [_tok("本", "Noun"), _tok("を", "Particle"), _tok("あげる", "Verb", lemma="あげる")]
        assert not postprocess_giving_aux(tokens)
        assert tokens[2]["pos"] == "Verb"


class TestPostprocessClosedAuxiliaryParadigms:
    def test_dialectal_copula_has_canonical_lemma(self):
        tokens = [_tok("そう", "Adverb"), _tok("じゃろ", "Auxiliary", lemma="じゃ")]
        assert postprocess_closed_subsidiary_aux(tokens)
        assert tokens[1] == _tok("じゃろ", "Auxiliary", lemma="だろ")

    def test_gozaru_after_copular_de_is_auxiliary(self):
        tokens = [_tok("で", "Auxiliary", lemma="だ"), _tok("ござら", "Verb", lemma="ござる")]
        assert postprocess_closed_subsidiary_aux(tokens)
        assert tokens[1]["pos"] == "Auxiliary"


class TestPostprocessClassicalPerfectAux:
    def test_deverbal_noun_reading_is_restored_before_tari_keri(self):
        tokens = [
            _tok("語り", "Noun"),
            _tok("たり", "Auxiliary"),
            _tok("けり", "Auxiliary"),
        ]
        assert postprocess_classical_perfect_aux(tokens)
        assert tokens[0] == _tok("語り", "Verb", lemma="語る")

    def test_ordinary_deverbal_noun_is_unchanged(self):
        tokens = [_tok("語り", "Noun"), _tok("を", "Particle")]
        assert not postprocess_classical_perfect_aux(tokens)

    def test_deverbal_noun_reading_is_restored_before_adnominal_taru(self):
        tokens = [
            _tok("咲き", "Noun"),
            _tok("たる", "Auxiliary", lemma="たり"),
        ]
        assert postprocess_classical_perfect_aux(tokens)
        assert tokens[0] == _tok("咲き", "Verb", lemma="咲く")

    def test_non_continuative_noun_before_adnominal_taru_is_unchanged(self):
        tokens = [
            _tok("役割", "Noun"),
            _tok("たる", "Auxiliary", lemma="たり"),
        ]
        assert not postprocess_classical_perfect_aux(tokens)


class TestPostprocessMiruAux:
    def test_miru_after_te(self):
        tokens = [_tok("て", "Particle"), _tok("み", "Verb", lemma="みる")]
        postprocess_miru_aux(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_miru_after_emphatic_mo(self):
        tokens = [
            _tok("て", "Particle"),
            _tok("も", "Particle"),
            _tok("み", "Verb", lemma="みる"),
        ]
        postprocess_miru_aux(tokens)
        assert tokens[2]["pos"] == "Auxiliary"

    def test_miru_standalone(self):
        tokens = [_tok("花", "Noun"), _tok("を", "Particle"), _tok("みる", "Verb")]
        postprocess_miru_aux(tokens)
        assert tokens[2]["pos"] == "Verb"


class TestPostprocessShimauAux:
    def test_kanji_shimau_after_te(self):
        tokens = [_tok("て", "Particle"), _tok("仕舞っ", "Verb", lemma="仕舞う"), _tok("た", "Auxiliary")]
        postprocess_shimau_aux(tokens)
        assert tokens[1] == _tok("仕舞っ", "Auxiliary", lemma="しまう")

    def test_kanji_shimau_standalone(self):
        tokens = [_tok("物", "Noun"), _tok("を", "Particle"), _tok("仕舞う", "Verb")]
        assert not postprocess_shimau_aux(tokens)
        assert tokens[2] == _tok("仕舞う", "Verb")

    def test_split_juu_after_nasal_onbin_is_merged(self):
        tokens = [
            _tok("遊ん", "Verb", lemma="遊ぶ"),
            _tok("じゃ", "Auxiliary", lemma="だ"),
            _tok("う", "Interjection"),
        ]
        assert postprocess_shimau_aux(tokens)
        assert tokens == [
            _tok("遊ん", "Verb", lemma="遊ぶ"),
            _tok("じゃう", "Auxiliary", lemma="じゃう"),
        ]

    def test_whole_chau_inflection_is_auxiliary_after_verb(self):
        tokens = [_tok("食べ", "Verb", lemma="食べる"), _tok("ちゃっ", "Verb", lemma="ちゃう")]
        assert postprocess_shimau_aux(tokens)
        assert tokens[1] == _tok("ちゃっ", "Auxiliary", lemma="ちゃう")

    def test_chau_after_voice_auxiliary_is_still_completive(self):
        tokens = [
            _tok("壊さ", "Verb", lemma="壊す"),
            _tok("れ", "Auxiliary", lemma="れる"),
            _tok("ちゃう", "Verb", lemma="ちゃう"),
        ]
        assert postprocess_shimau_aux(tokens)
        assert tokens[2] == _tok("ちゃう", "Auxiliary", lemma="ちゃう")

    def test_split_chau_imperative_is_merged(self):
        tokens = [
            _tok("食べ", "Verb", lemma="食べる"),
            _tok("ちゃ", "Particle"),
            _tok("え", "Interjection"),
        ]
        assert postprocess_shimau_aux(tokens)
        assert tokens == [
            _tok("食べ", "Verb", lemma="食べる"),
            _tok("ちゃえ", "Auxiliary", lemma="ちゃう"),
        ]

    def test_split_chai_before_polite_auxiliary_is_merged(self):
        tokens = [
            _tok("食べ", "Verb", lemma="食べる"),
            _tok("ちゃ", "Particle"),
            _tok("い", "Verb"),
            _tok("ます", "Auxiliary"),
        ]
        assert postprocess_shimau_aux(tokens)
        assert tokens[1] == _tok("ちゃい", "Auxiliary", lemma="ちゃう")

    def test_copula_and_utterance_are_not_merged_without_onbin_verb(self):
        tokens = [
            _tok("それ", "Pronoun"),
            _tok("じゃ", "Auxiliary", lemma="だ"),
            _tok("う", "Interjection"),
        ]
        assert not postprocess_shimau_aux(tokens)


class TestPostprocessQuantityBoundSuffix:
    def test_quantity_counter_is_split_from_bound_suffix(self):
        tokens = [_tok("二階建て", "Noun"), _tok("三本立て", "Noun")]
        assert postprocess_quantity_bound_suffix(tokens)
        assert [(token["surface"], token["pos"]) for token in tokens] == [
            ("二階", "Noun"),
            ("建て", "Suffix"),
            ("三本", "Noun"),
            ("立て", "Suffix"),
        ]

    def test_non_quantity_verb_is_not_split(self):
        tokens = [_tok("建てる", "Verb")]
        assert not postprocess_quantity_bound_suffix(tokens)
        assert tokens == [_tok("建てる", "Verb")]

    def test_separate_homographic_verb_stem_becomes_suffix(self):
        tokens = [_tok("二本", "Noun"), _tok("立て", "Verb", lemma="立てる")]
        assert postprocess_quantity_bound_suffix(tokens)
        assert tokens[1] == _tok("立て", "Suffix")


class TestPostprocessTsureteParticle:
    def test_hiragana_compound_particle(self):
        tokens = [_tok("年", "Noun"), _tok("に", "Particle"), _tok("つれ", "Verb"), _tok("て", "Particle")]
        assert postprocess_renyokei_compound_particle(tokens)
        assert tokens == [_tok("年", "Noun"), _tok("につれて", "Particle")]

    def test_kanji_verb_is_not_compound_particle(self):
        tokens = [_tok("年", "Noun"), _tok("に", "Particle"), _tok("連れ", "Verb"), _tok("て", "Particle")]
        assert not postprocess_renyokei_compound_particle(tokens)


class TestPostprocessCompoundCaseParticleAru:
    def test_pre_nominal_aru_becomes_determiner(self):
        tokens = [
            _tok("について", "Particle", pos_sub1="格助詞", pos_sub2="連語"),
            _tok("ある", "Verb"),
            _tok("議論", "Noun"),
        ]

        assert postprocess_compound_case_particle_aru(tokens)
        assert tokens[1] == _tok("ある", "Determiner")

    def test_pre_nominal_aru_after_adverb_becomes_determiner(self):
        tokens = [
            _tok("かえって", "Adverb"),
            _tok("ある", "Verb"),
            _tok("本", "Noun"),
        ]

        assert postprocess_compound_case_particle_aru(tokens)
        assert tokens[1] == _tok("ある", "Determiner")

    def test_inflected_aru_after_compound_particle_becomes_verb(self):
        tokens = [
            _tok("について", "Particle", pos_sub1="格助詞", pos_sub2="連語"),
            _tok("あっ", "Auxiliary", lemma="ある"),
            _tok("た", "Auxiliary", lemma="た"),
        ]

        assert postprocess_compound_case_particle_aru(tokens)
        assert tokens[1] == _tok("あっ", "Verb", lemma="ある")

    def test_productive_te_form_restores_resultative_verb(self):
        tokens = [
            _tok("て", "Particle"),
            _tok("ある", "Determiner"),
            _tok("本", "Noun"),
        ]

        assert postprocess_compound_case_particle_aru(tokens)
        assert tokens[1] == _tok("ある", "Verb")

    def test_inflected_resultative_keeps_auxiliary(self):
        tokens = [
            _tok("て", "Particle", pos_sub1="格助詞", pos_sub2="連語"),
            _tok("あり", "Auxiliary", lemma="ある"),
            _tok("ます", "Auxiliary", lemma="ます"),
        ]

        assert not postprocess_compound_case_particle_aru(tokens)
        assert tokens[1] == _tok("あり", "Auxiliary", lemma="ある")

    def test_topic_in_copula_chain_keeps_auxiliary(self):
        tokens = [
            _tok("は", "Particle"),
            _tok("ある", "Auxiliary", lemma="ある"),
            _tok("まい", "Auxiliary", lemma="まい"),
        ]

        assert not postprocess_compound_case_particle_aru(tokens)
        assert tokens[1] == _tok("ある", "Auxiliary", lemma="ある")

    def test_nominative_particle_keeps_existence_verb(self):
        tokens = [
            _tok("が", "Particle", pos_sub1="格助詞"),
            _tok("ある", "Verb"),
            _tok("人", "Noun"),
        ]

        assert not postprocess_compound_case_particle_aru(tokens)
        assert tokens[1] == _tok("ある", "Verb")

    def test_topic_before_pre_nominal_aru_does_not_supply_its_subject(self):
        tokens = [
            _tok("は", "Particle"),
            _tok("ある", "Verb"),
            _tok("人", "Noun"),
        ]

        assert postprocess_compound_case_particle_aru(tokens)
        assert tokens[1] == _tok("ある", "Determiner")


class TestPostprocessToArebaConditional:
    def test_splits_reference_analyzer_compound(self):
        tokens = [_tok("必要", "Noun"), _tok("と", "Particle"), _tok("あれば", "Verb", lemma="ある")]

        assert postprocess_to_areba_conditional(tokens)
        assert tokens == [
            _tok("必要", "Noun"),
            _tok("と", "Particle"),
            _tok("あれ", "Verb", lemma="ある"),
            _tok("ば", "Particle"),
        ]

    def test_keeps_existing_boundary(self):
        tokens = [
            _tok("と", "Particle"),
            _tok("あれ", "Verb", lemma="ある"),
            _tok("ば", "Particle"),
        ]

        assert not postprocess_to_areba_conditional(tokens)


class TestPostprocessProductiveVerbSuffixStem:
    def test_noun_homograph_before_gachi_becomes_godan_renyokei(self):
        tokens = [_tok("読み", "Noun"), _tok("がち", "Suffix")]
        assert postprocess_productive_verb_suffix_stem(tokens)
        assert tokens[0] == _tok("読み", "Verb", lemma="読む")

    def test_noun_homograph_before_ppanashi_becomes_godan_renyokei(self):
        tokens = [_tok("出し", "Noun"), _tok("っぱなし", "Suffix")]
        assert postprocess_productive_verb_suffix_stem(tokens)
        assert tokens[0] == _tok("出し", "Verb", lemma="出す")

    def test_ordinary_noun_before_particle_is_unchanged(self):
        tokens = [_tok("読み", "Noun"), _tok("を", "Particle")]
        assert not postprocess_productive_verb_suffix_stem(tokens)

    def test_makuri_is_a_productive_suffix(self):
        tokens = [_tok("走り", "Verb", lemma="走る"), _tok("まくり", "Auxiliary", lemma="まくる")]
        assert postprocess_productive_verb_suffix_stem(tokens)
        assert tokens[1] == _tok("まくり", "Suffix", lemma="まくり")


class TestPostprocessNaraVerb:
    def test_nara_before_classical_negative(self):
        tokens = [_tok("なら", "Auxiliary"), _tok("ぬ", "Auxiliary")]
        postprocess_nara_verb(tokens)
        assert tokens[0] == _tok("なら", "Verb", lemma="なる")


class TestPostprocessHonorificRequest:
    def test_standalone_honorific_continuative_is_nominal(self):
        tokens = [_tok("お", "Prefix"), _tok("預かり", "Verb", lemma="預かる")]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("預かり", "Noun", lemma="預かり")

    def test_topic_marked_honorific_continuative_is_nominal(self):
        tokens = [_tok("お", "Prefix"), _tok("振込み", "Verb", lemma="振込む"), _tok("は", "Particle")]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("振込み", "Noun", lemma="振込み")

    def test_classical_hiragana_verb_is_not_nominalized(self):
        tokens = [_tok("お", "Prefix"), _tok("はす", "Verb", lemma="はする")]
        assert not postprocess_honorific_request(tokens)
        assert tokens[1]["pos"] == "Verb"

    def test_godan_renyokei_before_kudasaru(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("立ち", "Noun"),
            _tok("ください", "Verb", lemma="くださる"),
        ]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("立ち", "Verb", lemma="立つ")

    def test_ichidan_renyokei_before_kudasaru(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("答え", "Noun"),
            _tok("ください", "Verb", lemma="くださる"),
        ]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("答え", "Verb", lemma="答える")

    def test_godan_renyokei_before_itasu(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("願い", "Noun"),
            _tok("いたし", "Verb", lemma="いたす"),
        ]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("願い", "Verb", lemma="願う")

    def test_godan_renyokei_before_itadaku(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("使い", "Noun"),
            _tok("いただく", "Verb", lemma="いただく"),
        ]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("使い", "Verb", lemma="使う")

    def test_godan_renyokei_before_potential_itadaku(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("待ち", "Noun"),
            _tok("いただけ", "Auxiliary", lemma="いただける"),
        ]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("待ち", "Verb", lemma="待つ")

    def test_godan_renyokei_before_honorific_naru_auxiliary_reading(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("読み", "Noun"),
            _tok("に", "Particle"),
            _tok("なる", "Auxiliary", lemma="なる"),
        ]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("読み", "Verb", lemma="読む")

    def test_godan_renyokei_before_moushiageru(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("願い", "Noun"),
            _tok("申し上げ", "Verb", lemma="申し上げる"),
        ]
        assert postprocess_honorific_request(tokens)
        assert tokens[1] == _tok("願い", "Verb", lemma="願う")

    def test_object_noun_is_not_changed(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("茶", "Noun"),
            _tok("ください", "Verb", lemma="くださる"),
        ]
        assert not postprocess_honorific_request(tokens)
        assert tokens[1]["pos"] == "Noun"


class TestTokenizerSearchUnitNormalizers:
    def test_tagaru_forms_one_auxiliary(self):
        tokens = [_tok("食べ", "Verb"), _tok("た", "Auxiliary"), _tok("がる", "Verb")]
        assert postprocess_tagaru_aux(tokens)
        assert tokens == [_tok("食べ", "Verb"), _tok("たがる", "Auxiliary", lemma="たがる")]

    def test_productive_garu_after_adjective_stem_is_verb(self):
        for form in ("がら", "がり", "がる", "がれ", "がろ", "がっ"):
            tokens = [_tok("恥ずかし", "Adjective", lemma="恥ずかしい"), _tok(form, "Auxiliary")]
            assert postprocess_adjective_garu(tokens)
            assert tokens[1] == _tok(form, "Verb", lemma="がる")

    def test_l2_na_adjective_garu_is_split_and_typed(self):
        tokens = [_tok("嫌がる", "Verb")]
        assert postprocess_adjective_garu(tokens)
        assert tokens == [
            _tok("嫌", "Adjective"),
            _tok("がる", "Verb", lemma="がる"),
        ]

    def test_registered_noun_host_keeps_noun_pos_before_garu(self, monkeypatch):
        from suzume_mcp.core import postprocessor_predicates

        real_headwords = postprocessor_predicates.core_headwords
        monkeypatch.setattr(
            postprocessor_predicates,
            "core_headwords",
            lambda name: frozenset({"心配"}) if name == "nouns.tsv" else real_headwords(name),
        )
        tokens = [_tok("心配", "Noun"), _tok("がる", "Auxiliary")]
        assert postprocess_adjective_garu(tokens)
        assert tokens == [
            _tok("心配", "Noun"),
            _tok("がる", "Verb", lemma="がる"),
        ]

    def test_registered_na_adjective_host_is_adjective_before_garu(self):
        tokens = [_tok("不安", "Noun"), _tok("がる", "Auxiliary")]
        assert postprocess_adjective_garu(tokens)
        assert tokens[0]["pos"] == "Adjective"

    def test_unregistered_noun_does_not_license_garu(self):
        tokens = [_tok("ころ", "Noun"), _tok("がる", "Auxiliary")]
        assert not postprocess_adjective_garu(tokens)

    def test_lexical_garu_verb_is_not_split_without_adjective_evidence(self):
        tokens = [_tok("つながる", "Verb")]
        assert not postprocess_adjective_garu(tokens)

    def test_registered_lexical_garu_verb_beats_adjective_homograph(self):
        tokens = [_tok("広がる", "Verb")]
        assert not postprocess_adjective_garu(tokens)

    def test_l2_noun_homograph_before_copula_is_nominal(self, monkeypatch):
        monkeypatch.setattr(
            "suzume_mcp.core.postprocessor_nominals.core_headwords",
            lambda filename: frozenset({"終わり"}),
        )
        tokens = [_tok("終わり", "Verb", lemma="終わる"), _tok("だ", "Auxiliary", lemma="だ")]

        assert postprocess_l2_noun_context(tokens)
        assert tokens[0] == _tok("終わり", "Noun")

    def test_l2_noun_homograph_before_verbal_auxiliary_stays_verb(self, monkeypatch):
        monkeypatch.setattr(
            "suzume_mcp.core.postprocessor_nominals.core_headwords",
            lambda filename: frozenset({"終わり"}),
        )
        tokens = [_tok("終わり", "Verb", lemma="終わる"), _tok("ます", "Auxiliary", lemma="ます")]

        assert not postprocess_l2_noun_context(tokens)
        assert tokens[0] == _tok("終わり", "Verb", lemma="終わる")

    def test_past_auxiliary_before_case_particle_is_not_tagaru(self):
        tokens = [_tok("い", "Auxiliary", lemma="いる"), _tok("た", "Auxiliary"), _tok("が", "Particle")]
        assert not postprocess_tagaru_aux(tokens)
        assert [token["surface"] for token in tokens] == ["い", "た", "が"]

    def test_demonstrative_fuu_is_split(self):
        tokens = [_tok("そんなふうに", "Auxiliary")]
        assert postprocess_fuu_formal_noun(tokens)
        assert [token["surface"] for token in tokens] == ["そんな", "ふう", "に"]
        assert tokens[1]["pos"] == "Noun"

    def test_indefinite_pronoun_and_existential(self):
        tokens = [_tok("なにか", "Adverb"), _tok("いる", "Auxiliary")]
        assert postprocess_indefinite_ka(tokens)
        assert [(token["surface"], token["pos"]) for token in tokens] == [
            ("なに", "Pronoun"),
            ("か", "Particle"),
            ("いる", "Verb"),
        ]

    def test_subsidiary_yuku_is_verbal(self):
        tokens = [_tok("散り", "Verb", lemma="散る"), _tok("ゆく", "Auxiliary")]
        assert postprocess_subsidiary_yuku(tokens)
        assert tokens[1]["pos"] == "Verb"

    def test_te_form_yuku_is_auxiliary(self):
        tokens = [_tok("て", "Particle"), _tok("いこ", "Verb", lemma="行く")]
        assert postprocess_subsidiary_yuku(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_case_de_keeps_independent_motion_verb(self):
        tokens = [_tok("三人", "Noun"), _tok("で", "Particle"), _tok("行く", "Verb", lemma="行く")]
        assert not postprocess_subsidiary_yuku(tokens)
        assert tokens[2]["pos"] == "Verb"

    def test_hiragana_purpose_is_nominal_search_unit(self):
        tokens = [_tok("およぎ", "Verb", lemma="およぐ"), _tok("に", "Particle"), _tok("行く", "Verb")]
        assert postprocess_hiragana_purpose_noun(tokens)
        assert tokens[0] == _tok("およぎ", "Noun")

    def test_short_hiragana_onbin(self):
        tokens = [_tok("かん", "Noun"), _tok("で", "Particle")]
        assert postprocess_short_hiragana_onbin(tokens)
        assert tokens[0] == _tok("かん", "Verb", lemma="かむ")

    def test_valid_hatsuonbin_lemma_is_preserved(self):
        tokens = [_tok("とん", "Verb", lemma="とぶ"), _tok("だ", "Auxiliary")]
        assert not postprocess_short_hiragana_onbin(tokens)
        assert tokens[0] == _tok("とん", "Verb", lemma="とぶ")

    def test_suffix_before_copula_is_not_onbin(self):
        tokens = [_tok("さん", "Suffix"), _tok("で", "Auxiliary")]
        assert not postprocess_short_hiragana_onbin(tokens)

    def test_hiragana_godan_wa_terminal(self):
        tokens = [_tok("つか", "Verb"), _tok("う", "Auxiliary")]
        assert postprocess_hiragana_godan_wa_terminal(tokens)
        assert tokens == [_tok("つかう", "Verb")]

    def test_o_row_volitional_is_not_merged(self):
        tokens = [_tok("むかお", "Verb", lemma="むかう"), _tok("う", "Auxiliary")]
        assert not postprocess_hiragana_godan_wa_terminal(tokens)

    def test_adjective_volitional_is_not_merged(self):
        tokens = [_tok("うれしかろ", "Adjective", lemma="うれしい"), _tok("う", "Auxiliary")]
        assert not postprocess_hiragana_godan_wa_terminal(tokens)


class TestPostprocessDeParticle:
    def test_de_before_ha_stays_auxiliary(self):
        """で before は stays Auxiliary (no-op: MeCab distinguishes copula/particle)."""
        tokens = [_tok("で", "Auxiliary"), _tok("は", "Particle")]
        postprocess_de_particle(tokens)
        assert tokens[0]["pos"] == "Auxiliary"

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

    def test_bare_na_adjective_stem_before_wo_is_nominal(self):
        tokens = [_tok("平静", "Adjective"), _tok("を", "Particle"), _tok("保っ", "Verb", lemma="保つ")]
        assert postprocess_na_adj_noun(tokens)
        assert tokens[0] == _tok("平静", "Noun")

    def test_na_adjective_stem_after_relative_clause_is_nominal_head(self):
        tokens = [
            _tok("春めい", "Verb", lemma="春めく"),
            _tok("た", "Auxiliary", lemma="た"),
            _tok("陽気", "Adjective"),
        ]
        assert postprocess_na_adj_noun(tokens)
        assert tokens[-1] == _tok("陽気", "Noun")

    def test_past_clause_before_particle_is_not_inferred_across_removed_punctuation(self):
        tokens = [
            _tok("基本的", "Adjective"),
            _tok("だっ", "Auxiliary", lemma="だ"),
            _tok("た", "Auxiliary", lemma="た"),
            _tok("基本的", "Adjective"),
            _tok("なら", "Particle"),
        ]
        assert not postprocess_na_adj_noun(tokens)
        assert tokens[3] == _tok("基本的", "Adjective")

    def test_appearance_auxiliary_after_past_clause_stays_adjective(self):
        tokens = [
            _tok("読ん", "Verb", lemma="読む"),
            _tok("だ", "Auxiliary", lemma="た"),
            _tok("そう", "Adjective"),
            _tok("だ", "Auxiliary"),
        ]
        assert not postprocess_na_adj_noun(tokens)
        assert tokens[2] == _tok("そう", "Adjective")

    def test_past_sentence_does_not_retag_next_adjective_before_appearance_auxiliary(self):
        tokens = [
            _tok("高", "Adjective", lemma="高い"),
            _tok("そう", "Auxiliary"),
            _tok("だっ", "Auxiliary", lemma="だ"),
            _tok("た", "Auxiliary", lemma="た"),
            _tok("静か", "Adjective"),
            _tok("そう", "Auxiliary"),
            _tok("でし", "Auxiliary", lemma="です"),
        ]
        assert not postprocess_na_adj_noun(tokens)
        assert tokens[4] == _tok("静か", "Adjective")

    def test_predicative_na_adjective_stays_adjective(self):
        tokens = [_tok("彼", "Pronoun"), _tok("は", "Particle"), _tok("元気", "Adjective"), _tok("だ", "Auxiliary")]
        assert not postprocess_na_adj_noun(tokens)
        assert tokens[-2] == _tok("元気", "Adjective")


class TestCopulaParadigmNormalization:
    copula_surfaces = ("だ", "だっ", "で", "です", "でし", "でしょ", "な", "なら")

    def test_onaji_is_predicative_adjective_across_copula_cells_and_clause_end(self):
        for following in (*self.copula_surfaces, None):
            tokens = [_tok("同じ", "Determiner")]
            if following is not None:
                tokens.append(_tok(following, "Auxiliary"))
            assert postprocess_onaji_predicate(tokens)
            assert tokens[0] == _tok("同じ", "Adjective")

    def test_teki_is_adjective_across_copula_cells_and_clause_end(self):
        for following in (*self.copula_surfaces, None):
            tokens = [_tok("基本的", "Noun")]
            if following is not None:
                tokens.append(_tok(following, "Auxiliary"))
            assert postprocess_teki_na_adjective(tokens)
            assert tokens[0]["pos"] == "Adjective"

    def test_state_suffix_is_suffix_across_copula_cells_and_clause_end(self):
        for following in (*self.copula_surfaces, None):
            tokens = [_tok("作業", "Noun"), _tok("中", "Noun")]
            if following is not None:
                tokens.append(_tok(following, "Auxiliary"))
            assert postprocess_state_suffix(tokens)
            assert tokens[1]["pos"] == "Suffix"


class TestPostprocessHiraganaYakaAdverbial:
    def test_repairs_reference_word_internal_split(self):
        tokens = [_tok("みや", "Noun"), _tok("びやかに", "Adverb"), _tok("話し", "Verb")]
        assert postprocess_hiragana_yaka_adverbial(tokens)
        assert tokens[:2] == [_tok("みやびやか", "Adjective"), _tok("に", "Particle")]

    def test_unrelated_hiragana_sequence_is_unchanged(self):
        tokens = [_tok("しずか", "Adjective"), _tok("に", "Particle")]
        assert not postprocess_hiragana_yaka_adverbial(tokens)


class TestPostprocessDewaAruBoundary:
    def test_splits_copula_and_binding_particle(self):
        tokens = [_tok("では", "Particle"), _tok("ある", "Verb"), _tok("まい", "Auxiliary")]
        assert postprocess_dewa_aru_boundary(tokens)
        assert tokens[:3] == [_tok("で", "Auxiliary", lemma="だ"), _tok("は", "Particle"), _tok("ある", "Verb")]

    def test_discourse_conjunction_is_unchanged(self):
        tokens = [_tok("では", "Conjunction"), _tok("始める", "Verb")]
        assert not postprocess_dewa_aru_boundary(tokens)


class TestPostprocessDeverbalNounContext:
    def test_renyokei_before_accusative_is_noun(self):
        tokens = [_tok("やすらぎ", "Verb", lemma="やすらぐ"), _tok("を", "Particle"), _tok("求め", "Verb")]
        assert postprocess_deverbal_noun_context(tokens)
        assert tokens[0] == _tok("やすらぎ", "Noun")

    def test_compound_renyokei_before_no_is_noun(self):
        tokens = [_tok("書きかけ", "Verb", lemma="書きかける"), _tok("の", "Particle"), _tok("紙", "Noun")]
        assert postprocess_deverbal_noun_context(tokens)
        assert tokens[0] == _tok("書きかけ", "Noun")

    def test_renyokei_before_polite_conjecture_is_noun(self):
        tokens = [_tok("曇り", "Verb", lemma="曇る"), _tok("でしょ", "Auxiliary"), _tok("う", "Auxiliary")]
        assert postprocess_deverbal_noun_context(tokens)
        assert tokens[0] == _tok("曇り", "Noun")

    def test_renyokei_before_independent_naku_is_noun(self):
        tokens = [_tok("たゆみ", "Verb", lemma="たゆむ"), _tok("なく", "Adjective", lemma="ない")]
        assert postprocess_deverbal_noun_context(tokens)
        assert tokens[0] == _tok("たゆみ", "Noun")

    def test_renyokei_before_negative_auxiliary_is_unchanged(self):
        tokens = [_tok("食べ", "Verb", lemma="食べる"), _tok("なく", "Auxiliary", lemma="ない")]
        assert not postprocess_deverbal_noun_context(tokens)

    def test_finite_verb_before_nominalizer_is_unchanged(self):
        tokens = [_tok("読む", "Verb", lemma="読む"), _tok("の", "Particle")]
        assert not postprocess_deverbal_noun_context(tokens)

    def test_renyokei_before_binding_particle_is_unchanged(self):
        tokens = [_tok("減り", "Verb", lemma="減る"), _tok("は", "Particle"), _tok("し", "Verb")]
        assert not postprocess_deverbal_noun_context(tokens)


class TestPostprocessAttributiveMamonaku:
    def test_splits_after_finite_predicate(self):
        tokens = [_tok("休む", "Verb"), _tok("間もなく", "Adverb"), _tok("働い", "Verb")]
        assert postprocess_attributive_mamonaku(tokens)
        assert tokens[1:4] == [
            _tok("間", "Noun"),
            _tok("も", "Particle"),
            _tok("なく", "Adjective", lemma="ない"),
        ]

    def test_clause_initial_adverb_is_unchanged(self):
        tokens = [_tok("間もなく", "Adverb"), _tok("到着", "Noun")]
        assert not postprocess_attributive_mamonaku(tokens)


class TestPostprocessAdverbNominalContext:
    def test_accusative_restores_noun(self):
        tokens = [_tok("一切", "Adverb"), _tok("を", "Particle")]
        assert postprocess_adverb_nominal_context(tokens)
        assert tokens[0] == _tok("一切", "Noun")

    def test_genitive_can_follow_adverb(self):
        tokens = [_tok("まったく", "Adverb"), _tok("の", "Particle")]
        assert not postprocess_adverb_nominal_context(tokens)

    def test_homograph_before_genitive_restores_noun(self):
        tokens = [_tok("一切", "Adverb"), _tok("の", "Particle")]
        assert postprocess_adverb_nominal_context(tokens)
        assert tokens[0] == _tok("一切", "Noun")

    def test_predicate_modifier_stays_adverb(self):
        tokens = [_tok("一切", "Adverb"), _tok("確認", "Noun")]
        assert not postprocess_adverb_nominal_context(tokens)


class TestPostprocessTemporalNao:
    def test_nao_after_temporal_adverb(self):
        tokens = [_tok("いま", "Adverb"), _tok("なお", "Conjunction")]
        assert postprocess_temporal_nao(tokens)
        assert tokens[1] == _tok("なお", "Adverb")

    def test_clause_initial_conjunction_is_unchanged(self):
        tokens = [_tok("なお", "Conjunction"), _tok("確認", "Noun")]
        assert not postprocess_temporal_nao(tokens)


class TestPostprocessNegativeHost:
    def test_bare_nominal_negative_is_adjective(self):
        tokens = [_tok("問題", "Noun"), _tok("ない", "Auxiliary")]
        assert postprocess_negative_host(tokens)
        assert tokens[1] == _tok("ない", "Adjective")

    def test_after_particle_is_adjective(self):
        tokens = [_tok("しか", "Particle"), _tok("ない", "Auxiliary")]
        assert postprocess_negative_host(tokens)
        assert tokens[1]["pos"] == "Adjective"

    def test_after_copula_is_adjective(self):
        tokens = [_tok("じゃ", "Auxiliary", lemma="だ"), _tok("なく", "Auxiliary", lemma="ない")]
        assert postprocess_negative_host(tokens)
        assert tokens[1]["pos"] == "Adjective"

    def test_after_adjective_continuative_is_adjective(self):
        tokens = [_tok("高く", "Adjective", lemma="高い"), _tok("ない", "Auxiliary")]
        assert postprocess_negative_host(tokens)
        assert tokens[1]["pos"] == "Adjective"

    def test_after_verb_irrealis_is_auxiliary(self):
        tokens = [_tok("食べ", "Verb", lemma="食べる"), _tok("ない", "Adjective")]
        assert postprocess_negative_host(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_after_verbal_auxiliary_is_auxiliary(self):
        tokens = [_tok("させ", "Auxiliary", lemma="させる"), _tok("ない", "Auxiliary")]
        assert not postprocess_negative_host(tokens)

    def test_after_dropped_iru_te_form_is_auxiliary(self):
        tokens = [_tok("見", "Verb", lemma="見る"), _tok("て", "Particle"), _tok("ない", "Adjective")]
        assert postprocess_negative_host(tokens)
        assert tokens[2]["pos"] == "Auxiliary"


class TestPostprocessTsukeNoun:
    def test_tsuke(self):
        tokens = [_tok("付け", "Suffix")]
        postprocess_tsuke_noun(tokens)
        assert tokens[0]["pos"] == "Noun"


class TestPostprocessYouNoun:
    def test_mizenkei_you_remains_volitional_auxiliary(self):
        tokens = [_tok("見", "Verb", lemma="見る", conj_form="未然形"), _tok("よう", "Suffix")]
        assert postprocess_you_noun(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_renyokei_you_is_formal_noun(self):
        tokens = [_tok("読み", "Verb", lemma="読む"), _tok("よう", "Suffix")]
        postprocess_you_noun(tokens)
        assert tokens[1]["pos"] == "Noun"

    def test_terminal_no_you_is_formal_noun(self):
        tokens = [_tok("の", "Particle"), _tok("よう", "Auxiliary")]
        postprocess_you_noun(tokens)
        assert tokens[1]["pos"] == "Noun"

    def test_no_you_before_copula_is_formal_noun(self):
        tokens = [_tok("の", "Particle"), _tok("よう", "Auxiliary"), _tok("だ", "Auxiliary")]
        postprocess_you_noun(tokens)
        assert tokens[1]["pos"] == "Noun"


class TestClosedGrammarNormalizers:
    def test_formal_noun_lemma_after_auxiliary(self):
        tokens = [_tok("た", "Auxiliary"), _tok("物", "Noun")]
        assert postprocess_formal_noun_lemma(tokens)
        assert tokens[1]["lemma"] == "もの"

    def test_standalone_hoka_takes_kana_lemma(self):
        tokens = [_tok("に", "Particle"), _tok("他", "Noun", lemma="他"), _tok("なら", "Verb", lemma="なる")]
        assert postprocess_formal_noun_lemma(tokens)
        assert tokens[1]["lemma"] == "ほか"

    def test_lexical_thing_keeps_kanji_lemma(self):
        tokens = [_tok("物", "Noun")]
        assert not postprocess_formal_noun_lemma(tokens)

    def test_ga_tame_is_formal_noun_not_verb(self):
        tokens = [_tok("が", "Particle"), _tok("ため", "Verb", lemma="ためる")]
        assert postprocess_formal_noun_lemma(tokens)
        assert tokens[1] == {"surface": "ため", "pos": "Noun", "lemma": "ため"}

    def test_adjective_sa_is_suffix(self):
        tokens = [_tok("らし", "Auxiliary", lemma="らしい"), _tok("さ", "Particle")]
        assert postprocess_adjective_nominalizer(tokens)
        assert tokens[1]["pos"] == "Suffix"

    def test_causative_sa_is_not_nominalizer(self):
        tokens = [
            _tok("余儀なく", "Adjective", lemma="余儀ない"),
            _tok("さ", "Verb", lemma="する"),
            _tok("れる", "Auxiliary"),
        ]
        assert not postprocess_adjective_nominalizer(tokens)

    def test_shortened_causative_sa_is_auxiliary(self):
        tokens = [
            _tok("読ま", "Verb", lemma="読む"),
            _tok("さ", "Verb", lemma="する"),
            _tok("れ", "Auxiliary", lemma="れる"),
        ]
        assert postprocess_shortened_causative_passive(tokens)
        assert tokens[1] == {"surface": "さ", "pos": "Auxiliary", "lemma": "す"}

    def test_suru_passive_sa_stays_verbal(self):
        tokens = [
            _tok("反映", "Noun"),
            _tok("さ", "Verb", lemma="する"),
            _tok("れ", "Auxiliary", lemma="れる"),
        ]
        assert not postprocess_shortened_causative_passive(tokens)
        assert tokens[1]["pos"] == "Verb"

    def test_honorific_naru_stem_is_not_nominalized(self):
        tokens = [
            _tok("お", "Prefix"),
            _tok("読み", "Verb", lemma="読む"),
            _tok("に", "Particle"),
            _tok("なる", "Verb", lemma="なる"),
        ]
        assert not postprocess_deverbal_noun_context(tokens)
        assert tokens[1] == _tok("読み", "Verb", lemma="読む")


class TestProductiveSearchUnitBoundaries:
    def test_hiragana_alignment_compound_before_particle_is_nominal(self):
        tokens = [
            _tok("つめあわ", "Verb", lemma="つめあう"),
            _tok("せ", "Auxiliary", lemma="せる"),
            _tok("を", "Particle"),
        ]
        assert postprocess_productive_search_unit_boundaries(tokens)
        assert tokens == [
            {"surface": "つめあわせ", "pos": "Noun", "lemma": "つめあわせ"},
            _tok("を", "Particle"),
        ]

    def test_hiragana_alignment_stem_without_nominal_particle_stays_split(self):
        tokens = [
            _tok("つめあわ", "Verb", lemma="つめあう"),
            _tok("せ", "Auxiliary", lemma="せる"),
            _tok("た", "Auxiliary"),
        ]
        assert not postprocess_productive_search_unit_boundaries(tokens)

    def test_formal_noun_you_is_not_volitional(self):
        tokens = [_tok("読む", "Verb", lemma="読む"), _tok("よう", "Noun"), _tok("だ", "Auxiliary")]
        assert not postprocess_productive_search_unit_boundaries(tokens)
        assert [token["surface"] for token in tokens] == ["読む", "よう", "だ"]

    def test_volitional_auxiliary_keeps_morpheme_boundary(self):
        tokens = [_tok("見", "Verb", lemma="見る"), _tok("よう", "Auxiliary", lemma="よう")]
        assert postprocess_productive_search_unit_boundaries(tokens)
        assert tokens == [
            {"surface": "見よ", "pos": "Verb", "lemma": "見る"},
            {"surface": "う", "pos": "Auxiliary", "lemma": "う"},
        ]

    def test_sentence_final_yo_is_not_imperative(self):
        tokens = [_tok("帰る", "Verb", lemma="帰る"), _tok("よ", "Particle")]
        assert not postprocess_productive_search_unit_boundaries(tokens)

    def test_regular_sa_row_passive_is_not_shortened_causative(self):
        tokens = [_tok("果たさ", "Verb", lemma="果たす"), _tok("れ", "Auxiliary", lemma="れる")]
        assert not postprocess_productive_search_unit_boundaries(tokens)

    def test_shortened_causative_is_split_by_inflectional_mismatch(self):
        tokens = [_tok("やらさ", "Verb", lemma="やる"), _tok("れ", "Auxiliary", lemma="れる")]
        assert postprocess_productive_search_unit_boundaries(tokens)
        assert tokens == [
            {"surface": "やら", "pos": "Verb", "lemma": "やる"},
            {"surface": "さ", "pos": "Auxiliary", "lemma": "す"},
            {"surface": "れ", "pos": "Auxiliary", "lemma": "れる"},
        ]

    def test_terminal_verb_before_closed_v2_is_not_compounded(self):
        tokens = [_tok("あれ", "Verb", lemma="ある"), _tok("続ける", "Verb", lemma="続ける")]
        assert not postprocess_productive_search_unit_boundaries(tokens)

    def test_i_adjective_negative_chain_stays_adjective(self):
        tokens = [_tok("高く", "Adjective", lemma="高い"), _tok("なく", "Adjective", lemma="ない")]
        assert not postprocess_productive_search_unit_boundaries(tokens)

    def test_indefinite_ari_before_copula_is_nominal(self):
        tokens = [_tok("でも", "Particle"), _tok("あり", "Auxiliary", lemma="ある"), _tok("だ", "Auxiliary")]
        assert postprocess_productive_search_unit_boundaries(tokens)
        assert tokens[1] == _tok("あり", "Noun")

    def test_oide_before_kuruwa_polite_auxiliary_is_formal_noun(self):
        tokens = [_tok("おいで", "Adverb"), _tok("なんし", "Auxiliary", lemma="ます")]
        assert postprocess_productive_search_unit_boundaries(tokens)
        assert tokens[0] == _tok("おいで", "Noun")


class TestPostprocessPredicateCellLemmas:
    def test_ta_before_appearance_sou_is_desiderative_stem(self):
        tokens = [_tok("読み", "Verb", lemma="読む"), _tok("た", "Auxiliary"), _tok("そう", "Auxiliary")]
        assert postprocess_predicate_cell_lemmas(tokens)
        assert tokens[1]["lemma"] == "たい"

    def test_ease_stem_before_sou_is_adjective(self):
        tokens = [_tok("書き", "Verb", lemma="書く"), _tok("にく", "Noun"), _tok("そう", "Auxiliary")]
        assert postprocess_predicate_cell_lemmas(tokens)
        assert tokens[1] == _tok("にく", "Adjective", lemma="にくい")

    def test_continuative_toru_contraction_is_auxiliary(self):
        tokens = [_tok("食べ", "Verb", lemma="食べる"), _tok("とれ", "Verb", lemma="とる"), _tok("ば", "Particle")]
        assert postprocess_predicate_cell_lemmas(tokens)
        assert tokens[1]["pos"] == "Auxiliary"

    def test_clause_initial_suffix_is_nominal(self):
        tokens = [_tok("がち", "Suffix"), _tok("で", "Auxiliary", lemma="だ")]
        assert postprocess_predicate_cell_lemmas(tokens)
        assert tokens[0]["pos"] == "Noun"


class TestPostprocessCopulaNeg:
    def test_de_before_topic_negative_is_copula(self):
        tokens = [_tok("本", "Noun"), _tok("で", "Particle"), _tok("は", "Particle"), _tok("ない", "Auxiliary")]
        assert postprocess_copula_neg(tokens)
        assert tokens[1] == _tok("で", "Auxiliary", lemma="だ")


class TestQuotativeDeterminerHead:
    @staticmethod
    def _surfaces(tokens: list[dict]) -> list[str]:
        return [token["surface"] for token in tokens]

    def test_toiu_before_a_noun_head_is_the_determiner(self):
        tokens = [
            {"surface": "慣れろ", "pos": "Verb"},
            {"surface": "と", "pos": "Particle"},
            {"surface": "いう", "pos": "Verb"},
            {"surface": "わけ", "pos": "Noun"},
        ]
        assert postprocessors.postprocess_quotative_determiner_head(tokens)
        assert self._surfaces(tokens) == ["慣れろ", "という", "わけ"]
        assert tokens[1]["pos"] == "Determiner"

    def test_headless_determiner_is_the_quotation(self):
        tokens = [{"surface": "降る", "pos": "Verb"}, {"surface": "という", "pos": "Determiner"}]
        assert postprocessors.postprocess_quotative_determiner_head(tokens)
        assert self._surfaces(tokens) == ["降る", "と", "いう"]

    def test_headless_family_members_split_the_same_way(self):
        tokens = [{"surface": "読む", "pos": "Verb"}, {"surface": "っていう", "pos": "Determiner"}]
        assert postprocessors.postprocess_quotative_determiner_head(tokens)
        assert self._surfaces(tokens) == ["読む", "って", "いう"]
        tokens = [{"surface": "行く", "pos": "Verb"}, {"surface": "といった", "pos": "Determiner"}]
        assert postprocessors.postprocess_quotative_determiner_head(tokens)
        assert self._surfaces(tokens) == ["行く", "と", "いっ", "た"]

    def test_hostless_and_nominalizer_contexts_are_left_alone(self):
        bare = [{"surface": "という", "pos": "Determiner"}]
        before_no = [
            {"surface": "行く", "pos": "Verb"},
            {"surface": "という", "pos": "Determiner"},
            {"surface": "の", "pos": "Particle"},
        ]
        assert not postprocessors.postprocess_quotative_determiner_head(bare)
        assert not postprocessors.postprocess_quotative_determiner_head(before_no)


class TestContractedIkuTteNa:
    def test_splits_tte_na_after_te_form_into_iku_cell(self):
        tokens = [
            {"surface": "持っ", "pos": "Verb", "lemma": "持つ"},
            {"surface": "て", "pos": "Particle", "lemma": "て"},
            {"surface": "ってな", "pos": "Particle", "lemma": "ってな"},
        ]
        assert postprocessors.postprocess_contracted_iku_tte_na(tokens)
        assert [(t["surface"], t["pos"]) for t in tokens] == [
            ("持っ", "Verb"),
            ("て", "Particle"),
            ("っ", "Auxiliary"),
            ("て", "Particle"),
            ("な", "Particle"),
        ]

    def test_splits_tte_na_elsewhere_into_quotative_and_na(self):
        tokens = [
            {"surface": "行く", "pos": "Verb", "lemma": "行く"},
            {"surface": "ってな", "pos": "Particle", "lemma": "ってな"},
        ]
        assert postprocessors.postprocess_contracted_iku_tte_na(tokens)
        assert [t["surface"] for t in tokens] == ["行く", "って", "な"]
