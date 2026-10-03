"""Ordered context postprocessors and compatibility exports for normalization."""

from collections.abc import Callable

from . import (
    postprocessor_boundaries,
    postprocessor_classical,
    postprocessor_common,
    postprocessor_function_words,
    postprocessor_mecab,
    postprocessor_nominals,
    postprocessor_predicates,
    postprocessor_subsidiaries,
)

# Exports from postprocessor_boundaries.
_PRODUCTIVE_COMPOUND_V2 = postprocessor_boundaries._PRODUCTIVE_COMPOUND_V2
postprocess_exclusion_suffix = postprocessor_boundaries.postprocess_exclusion_suffix
postprocess_productive_search_unit_boundaries = postprocessor_boundaries.postprocess_productive_search_unit_boundaries
postprocess_productive_verb_suffix_stem = postprocessor_boundaries.postprocess_productive_verb_suffix_stem
postprocess_quantity_bound_suffix = postprocessor_boundaries.postprocess_quantity_bound_suffix
postprocess_state_suffix = postprocessor_boundaries.postprocess_state_suffix

# Exports from postprocessor_classical.
_CLASSICAL_ATTRIBUTIVE_AUX_LEMMAS = postprocessor_classical._CLASSICAL_ATTRIBUTIVE_AUX_LEMMAS
_CLASSICAL_PAST_KERI_CELLS = postprocessor_classical._CLASSICAL_PAST_KERI_CELLS
_CLASSICAL_RAMU = postprocessor_classical._CLASSICAL_RAMU
_KARI_HOST_POS = postprocessor_classical._KARI_HOST_POS
_KARI_RENYOKEI_CELL = postprocessor_classical._KARI_RENYOKEI_CELL
_KU_TERMINAL_CELL = postprocessor_classical._KU_TERMINAL_CELL
_PERFECT_NU_CELLS = postprocessor_classical._PERFECT_NU_CELLS
_RAMU_HOST_POS = postprocessor_classical._RAMU_HOST_POS
_classical_past_starts_at = postprocessor_classical._classical_past_starts_at
_na_adjective_base_of_ku_terminal = postprocessor_classical._na_adjective_base_of_ku_terminal
_nominal_plus_continuative = postprocessor_classical._nominal_plus_continuative
_spells_verb_continuative = postprocessor_classical._spells_verb_continuative
_verb_continuative_reading = postprocessor_classical._verb_continuative_reading
postprocess_classical_b_row_moteiku = postprocessor_classical.postprocess_classical_b_row_moteiku
postprocess_classical_conjecture_aux = postprocessor_classical.postprocess_classical_conjecture_aux
postprocess_classical_copula_nari = postprocessor_classical.postprocess_classical_copula_nari
postprocess_classical_desiderative_aux = postprocessor_classical.postprocess_classical_desiderative_aux
postprocess_classical_focus_namu = postprocessor_classical.postprocess_classical_focus_namu
postprocess_classical_ha_row_past = postprocessor_classical.postprocess_classical_ha_row_past
postprocess_classical_honorific_aux = postprocessor_classical.postprocess_classical_honorific_aux
postprocess_classical_kere_aux = postprocessor_classical.postprocess_classical_kere_aux
postprocess_classical_ku_terminal = postprocessor_classical.postprocess_classical_ku_terminal
postprocess_classical_nari_after_attributive = postprocessor_classical.postprocess_classical_nari_after_attributive
postprocess_classical_nari_kateikei = postprocessor_classical.postprocess_classical_nari_kateikei
postprocess_classical_past_izenkei_shika = postprocessor_classical.postprocess_classical_past_izenkei_shika
postprocess_classical_past_keri = postprocessor_classical.postprocess_classical_past_keri
postprocess_classical_past_shi = postprocessor_classical.postprocess_classical_past_shi
postprocess_classical_perfect_aux = postprocessor_classical.postprocess_classical_perfect_aux
postprocess_classical_perfect_ni = postprocessor_classical.postprocess_classical_perfect_ni
postprocess_classical_perfect_nu = postprocessor_classical.postprocess_classical_perfect_nu
postprocess_classical_ramu_boundary = postprocessor_classical.postprocess_classical_ramu_boundary

# Exports from postprocessor_common.
_raw_analysis = postprocessor_common._raw_analysis
reports_mutation = postprocessor_common.reports_mutation

# Exports from postprocessor_function_words.
_HELD_FINAL_PARTICLES = postprocessor_function_words._HELD_FINAL_PARTICLES
_HELD_VOWELS = postprocessor_function_words._HELD_VOWELS
_INTERJECTION_PREDICATE_HOMOGRAPHS = postprocessor_function_words._INTERJECTION_PREDICATE_HOMOGRAPHS
postprocess_closed_function_words = postprocessor_function_words.postprocess_closed_function_words
postprocess_compound_case_particle_aru = postprocessor_function_words.postprocess_compound_case_particle_aru
postprocess_copula_neg = postprocessor_function_words.postprocess_copula_neg
postprocess_dai_final_particle = postprocessor_function_words.postprocess_dai_final_particle
postprocess_de_after_nominal = postprocessor_function_words.postprocess_de_after_nominal
postprocess_de_aru = postprocessor_function_words.postprocess_de_aru
postprocess_de_particle = postprocessor_function_words.postprocess_de_particle
postprocess_demo = postprocessor_function_words.postprocess_demo
postprocess_dewa_aru_boundary = postprocessor_function_words.postprocess_dewa_aru_boundary
postprocess_final_particle_quotative_tte = postprocessor_function_words.postprocess_final_particle_quotative_tte
postprocess_held_final_particle = postprocessor_function_words.postprocess_held_final_particle
postprocess_kamo_before_final_particle = postprocessor_function_words.postprocess_kamo_before_final_particle
postprocess_indefinite_ka = postprocessor_function_words.postprocess_indefinite_ka
postprocess_interjection_after_te = postprocessor_function_words.postprocess_interjection_after_te
postprocess_interjection_before_copula = postprocessor_function_words.postprocess_interjection_before_copula
postprocess_kiri_limited_particle = postprocessor_function_words.postprocess_kiri_limited_particle
postprocess_monono_conjunction = postprocessor_function_words.postprocess_monono_conjunction
postprocess_nan_copula_nominalizer = postprocessor_function_words.postprocess_nan_copula_nominalizer
postprocess_nanka_particle = postprocessor_function_words.postprocess_nanka_particle
postprocess_now_final_particle = postprocessor_function_words.postprocess_now_final_particle
postprocess_nano_quotative = postprocessor_function_words.postprocess_nano_quotative
postprocess_negative_conjunctive_de = postprocessor_function_words.postprocess_negative_conjunctive_de
postprocess_nominal_conjunction_homograph = postprocessor_function_words.postprocess_nominal_conjunction_homograph
postprocess_quotative_determiner_spelling = postprocessor_function_words.postprocess_quotative_determiner_spelling
postprocess_renyokei_compound_particle = postprocessor_function_words.postprocess_renyokei_compound_particle
postprocess_te_form_contraction = postprocessor_function_words.postprocess_te_form_contraction
postprocess_to_areba_conditional = postprocessor_function_words.postprocess_to_areba_conditional
postprocess_tteba_emphatic_particle = postprocessor_function_words.postprocess_tteba_emphatic_particle

# Exports from postprocessor_mecab.
_ADJECTIVE_INFLECTION_KANA = postprocessor_mecab._ADJECTIVE_INFLECTION_KANA
_EMPHATIC_SOKUON_MARKS = postprocessor_mecab._EMPHATIC_SOKUON_MARKS
_NAI_NEGATIVE_HEAD = postprocessor_mecab._NAI_NEGATIVE_HEAD
SLANG_ADJ_SUBSTITUTE = postprocessor_mecab.SLANG_ADJ_SUBSTITUTE
_accept_slang_match = postprocessor_mecab._accept_slang_match
_emphatic_final_sokuon = postprocessor_mecab._emphatic_final_sokuon
_invents_a_word_for = postprocessor_mecab._invents_a_word_for
_is_emphatic_spelling = postprocessor_mecab._is_emphatic_spelling
_non_overlapping_replacements = postprocessor_mecab._non_overlapping_replacements
_reads_as_adjective = postprocessor_mecab._reads_as_adjective
_stranded_adjective_stems = postprocessor_mecab._stranded_adjective_stems
postprocess_mecab_tokens = postprocessor_mecab.postprocess_mecab_tokens
preprocess_for_mecab = postprocessor_mecab.preprocess_for_mecab
repair_kanji_prefix_before_kana_noun = postprocessor_mecab.repair_kanji_prefix_before_kana_noun
repair_kko_nominalizer = postprocessor_mecab.repair_kko_nominalizer
repair_productive_causative = postprocessor_mecab.repair_productive_causative
repair_assimilated_koto_copula = postprocessor_mecab.repair_assimilated_koto_copula
repair_contracted_quotative = postprocessor_mecab.repair_contracted_quotative
merge_conjunction_with_rashii = postprocessor_mecab.merge_conjunction_with_rashii
repair_continuative_before_manner_suffix = postprocessor_mecab.repair_continuative_before_manner_suffix
repair_adjective_stem_before_suffix = postprocessor_mecab.repair_adjective_stem_before_suffix
repair_interrogative_nande = postprocessor_mecab.repair_interrogative_nande
merge_reason_nde = postprocessor_mecab.merge_reason_nde
repair_contracted_rareru = postprocessor_mecab.repair_contracted_rareru
repair_contracted_iika = postprocessor_mecab.repair_contracted_iika
repair_contracted_volitional = postprocessor_mecab.repair_contracted_volitional
repair_euphonic_adjective_adverb = postprocessor_mecab.repair_euphonic_adjective_adverb
repair_regional_imperative = postprocessor_mecab.repair_regional_imperative
split_transparent_suru_te_adverb = postprocessor_mecab.split_transparent_suru_te_adverb

# Exports from postprocessor_nominals.
_is_irrealis_before_negative = postprocessor_nominals._is_irrealis_before_negative
postprocess_adjective_nominalizer = postprocessor_nominals.postprocess_adjective_nominalizer
postprocess_adverb_nominal_context = postprocessor_nominals.postprocess_adverb_nominal_context
postprocess_adverbial_temporal_prefix = postprocessor_nominals.postprocess_adverbial_temporal_prefix
postprocess_attributive_mamonaku = postprocessor_nominals.postprocess_attributive_mamonaku
postprocess_determiner_wake_noun = postprocessor_nominals.postprocess_determiner_wake_noun
postprocess_deverbal_noun_context = postprocessor_nominals.postprocess_deverbal_noun_context
postprocess_formal_noun_lemma = postprocessor_nominals.postprocess_formal_noun_lemma
postprocess_fuu_formal_noun = postprocessor_nominals.postprocess_fuu_formal_noun
postprocess_hiragana_purpose_noun = postprocessor_nominals.postprocess_hiragana_purpose_noun
postprocess_ikaga = postprocessor_nominals.postprocess_ikaga
postprocess_ka_suru_noun = postprocessor_nominals.postprocess_ka_suru_noun
postprocess_kadouka_adverb = postprocessor_nominals.postprocess_kadouka_adverb
postprocess_l2_noun_context = postprocessor_nominals.postprocess_l2_noun_context
postprocess_prolonged_sound_noun = postprocessor_nominals.postprocess_prolonged_sound_noun
postprocess_tada = postprocessor_nominals.postprocess_tada
postprocess_taihen = postprocessor_nominals.postprocess_taihen
postprocess_temporal_nao = postprocessor_nominals.postprocess_temporal_nao
postprocess_tsuke_noun = postprocessor_nominals.postprocess_tsuke_noun
postprocess_verbal_nominalizer_mi = postprocessor_nominals.postprocess_verbal_nominalizer_mi
postprocess_yoshi_formal_noun = postprocessor_nominals.postprocess_yoshi_formal_noun
postprocess_you_noun = postprocessor_nominals.postprocess_you_noun

# Exports from postprocessor_predicates.
_EASE_ADJECTIVE_HEADS = postprocessor_predicates._EASE_ADJECTIVE_HEADS
_GODAN_ERO_TO_BASE = postprocessor_predicates._GODAN_ERO_TO_BASE
postprocess_adjective_garu = postprocessor_predicates.postprocess_adjective_garu
postprocess_adverbial_na_adjective = postprocessor_predicates.postprocess_adverbial_na_adjective
postprocess_bound_derived_adjective = postprocessor_predicates.postprocess_bound_derived_adjective
postprocess_chigai_negative_adjective = postprocessor_predicates.postprocess_chigai_negative_adjective
postprocess_difficulty_adjective_stem = postprocessor_predicates.postprocess_difficulty_adjective_stem
postprocess_hiragana_godan_wa_terminal = postprocessor_predicates.postprocess_hiragana_godan_wa_terminal
postprocess_hiragana_yaka_adverbial = postprocessor_predicates.postprocess_hiragana_yaka_adverbial
postprocess_honorific_i_adjective = postprocessor_predicates.postprocess_honorific_i_adjective
postprocess_i_adjective_upper_bound = postprocessor_predicates.postprocess_i_adjective_upper_bound
postprocess_ii = postprocessor_predicates.postprocess_ii
postprocess_kusai_garu = postprocessor_predicates.postprocess_kusai_garu
postprocess_modifier_godan_imperative = postprocessor_predicates.postprocess_modifier_godan_imperative
postprocess_na_adj_noun = postprocessor_predicates.postprocess_na_adj_noun
postprocess_onaji_predicate = postprocessor_predicates.postprocess_onaji_predicate
postprocess_short_hiragana_onbin = postprocessor_predicates.postprocess_short_hiragana_onbin
postprocess_sou = postprocessor_predicates.postprocess_sou
postprocess_sou_aux = postprocessor_predicates.postprocess_sou_aux
postprocess_teki_na_adjective = postprocessor_predicates.postprocess_teki_na_adjective
postprocess_verb_ease_adjective = postprocessor_predicates.postprocess_verb_ease_adjective

# Exports from postprocessor_subsidiaries.
_TE_CONTINUATION_HEADS = postprocessor_subsidiaries._TE_CONTINUATION_HEADS
_TOKU_FABRICATED_TAILS = postprocessor_subsidiaries._TOKU_FABRICATED_TAILS
postprocess_binding_negative_aux = postprocessor_subsidiaries.postprocess_binding_negative_aux
postprocess_closed_subsidiary_aux = postprocessor_subsidiaries.postprocess_closed_subsidiary_aux
postprocess_contracted_iku_lemma = postprocessor_subsidiaries.postprocess_contracted_iku_lemma
postprocess_contracted_progressive_aux = postprocessor_subsidiaries.postprocess_contracted_progressive_aux
postprocess_contracted_toku_aux = postprocessor_subsidiaries.postprocess_contracted_toku_aux
postprocess_excessive_after_verb = postprocessor_subsidiaries.postprocess_excessive_after_verb
postprocess_giving_aux = postprocessor_subsidiaries.postprocess_giving_aux
postprocess_honorific_oki_aux = postprocessor_subsidiaries.postprocess_honorific_oki_aux
postprocess_honorific_request = postprocessor_subsidiaries.postprocess_honorific_request
postprocess_humble_o_itasu = postprocessor_subsidiaries.postprocess_humble_o_itasu
postprocess_iru_aux = postprocessor_subsidiaries.postprocess_iru_aux
postprocess_itadakeru_aux = postprocessor_subsidiaries.postprocess_itadakeru_aux
postprocess_koto_suru_te = postprocessor_subsidiaries.postprocess_koto_suru_te
postprocess_kuru_causative = postprocessor_subsidiaries.postprocess_kuru_causative
postprocess_miru_aux = postprocessor_subsidiaries.postprocess_miru_aux
postprocess_mu_verb_desiderative = postprocessor_subsidiaries.postprocess_mu_verb_desiderative
postprocess_n_kuruwa = postprocessor_subsidiaries.postprocess_n_kuruwa
postprocess_nai_context = postprocessor_subsidiaries.postprocess_nai_context
postprocess_nara_verb = postprocessor_subsidiaries.postprocess_nara_verb
postprocess_obligation_nan_naru = postprocessor_subsidiaries.postprocess_obligation_nan_naru
postprocess_shimau_aux = postprocessor_subsidiaries.postprocess_shimau_aux
postprocess_shortened_causative_passive = postprocessor_subsidiaries.postprocess_shortened_causative_passive
postprocess_subsidiary_yuku = postprocessor_subsidiaries.postprocess_subsidiary_yuku
postprocess_tagaru_aux = postprocessor_subsidiaries.postprocess_tagaru_aux
postprocess_teru_te_kureru = postprocessor_subsidiaries.postprocess_teru_te_kureru
postprocess_teru_te_order = postprocessor_subsidiaries.postprocess_teru_te_order
postprocess_verb_negative_aux = postprocessor_subsidiaries.postprocess_verb_negative_aux
postprocess_copula_negative_nee = postprocessor_subsidiaries.postprocess_copula_negative_nee


# Keep the semantic invocation order in one place: the first mutation supplies
# the public applied-rule label.
POSTPROCESSORS: tuple[tuple[str, Callable[[list[dict]], bool]], ...] = (
    ("nan-copula-nominalizer", postprocess_nan_copula_nominalizer),
    ("now-final-particle", postprocess_now_final_particle),
    ("sou-context", postprocess_sou),
    ("ikaga-adverb", postprocess_ikaga),
    ("tada-context", postprocess_tada),
    ("demo-particle", postprocess_demo),
    ("hiragana-yaka-adverbial", postprocess_hiragana_yaka_adverbial),
    ("closed-function-word-pos", postprocess_closed_function_words),
    ("closed-subsidiary-aux", postprocess_closed_subsidiary_aux),
    ("classical-focus-namu", postprocess_classical_focus_namu),
    ("classical-copula-nari", postprocess_classical_copula_nari),
    ("classical-past-izenkei-shika", postprocess_classical_past_izenkei_shika),
    ("honorific-i-adjective", postprocess_honorific_i_adjective),
    ("i-adjective-upper-bound", postprocess_i_adjective_upper_bound),
    ("kadouka-adverb", postprocess_kadouka_adverb),
    ("ii-adjective", postprocess_ii),
    ("iru-aux", postprocess_iru_aux),
    ("giving-receiving-aux", postprocess_giving_aux),
    ("teru-te-kureru", postprocess_teru_te_kureru),
    ("teru-te-order", postprocess_teru_te_order),
    ("contracted-progressive-aux", postprocess_contracted_progressive_aux),
    ("itadakeru-aux", postprocess_itadakeru_aux),
    ("miru-aux", postprocess_miru_aux),
    ("monono-conjunction", postprocess_monono_conjunction),
    ("formal-noun-lemma", postprocess_formal_noun_lemma),
    ("adjective-nominalizer", postprocess_adjective_nominalizer),
    ("verbal-nominalizer-mi", postprocess_verbal_nominalizer_mi),
    ("mu-verb-desiderative", postprocess_mu_verb_desiderative),
    ("shortened-causative-passive", postprocess_shortened_causative_passive),
    ("modifier-godan-imperative", postprocess_modifier_godan_imperative),
    ("contracted-shimau-aux", postprocess_shimau_aux),
    ("contracted-toku-aux", postprocess_contracted_toku_aux),
    ("koto-suru-te", postprocess_koto_suru_te),
    ("quantity-bound-suffix", postprocess_quantity_bound_suffix),
    ("exclusion-suffix", postprocess_exclusion_suffix),
    ("state-suffix", postprocess_state_suffix),
    ("productive-verb-suffix-stem", postprocess_productive_verb_suffix_stem),
    ("teki-na-adjective", postprocess_teki_na_adjective),
    ("difficulty-adjective-stem", postprocess_difficulty_adjective_stem),
    ("renyokei-compound-particle", postprocess_renyokei_compound_particle),
    ("compound-case-particle-aru", postprocess_compound_case_particle_aru),
    ("to-areba-conditional", postprocess_to_areba_conditional),
    ("tagaru-search-unit", postprocess_tagaru_aux),
    ("l2-noun-context", postprocess_l2_noun_context),
    ("adjective-garu-pos", postprocess_adjective_garu),
    ("kusai-garu", postprocess_kusai_garu),
    ("verb-ease-adjective", postprocess_verb_ease_adjective),
    ("excessive-after-verb", postprocess_excessive_after_verb),
    ("fuu-formal-noun", postprocess_fuu_formal_noun),
    ("indefinite-ka", postprocess_indefinite_ka),
    ("subsidiary-yuku", postprocess_subsidiary_yuku),
    ("contracted-iku-lemma", postprocess_contracted_iku_lemma),
    ("hiragana-purpose-noun", postprocess_hiragana_purpose_noun),
    ("short-hiragana-onbin", postprocess_short_hiragana_onbin),
    ("hiragana-godan-wa-terminal", postprocess_hiragana_godan_wa_terminal),
    ("honorific-request-renyokei", postprocess_honorific_request),
    ("honorific-oki-aux", postprocess_honorific_oki_aux),
    ("humble-o-itasu", postprocess_humble_o_itasu),
    # Ordered before the copular readings below, which name the environments
    # that genuinely select the copula and must have the last word.
    ("de-after-nominal", postprocess_de_after_nominal),
    ("de-particle", postprocess_de_particle),
    ("te-form-contraction-particle", postprocess_te_form_contraction),
    ("dai-final-particle", postprocess_dai_final_particle),
    ("tteba-emphatic-particle", postprocess_tteba_emphatic_particle),
    ("held-final-particle", postprocess_held_final_particle),
    ("kamo-final-particle", postprocess_kamo_before_final_particle),
    ("nano-quotative", postprocess_nano_quotative),
    ("final-particle-quotative-tte", postprocess_final_particle_quotative_tte),
    ("chigai-negative-adjective", postprocess_chigai_negative_adjective),
    ("nanka-colloquial-particle", postprocess_nanka_particle),
    ("kiri-limiting-particle", postprocess_kiri_limited_particle),
    ("kuru-causative-lemma", postprocess_kuru_causative),
    ("onaji-predicative-na-adjective", postprocess_onaji_predicate),
    ("de-aru", postprocess_de_aru),
    ("dewa-aru-boundary", postprocess_dewa_aru_boundary),
    ("ka-suru-noun", postprocess_ka_suru_noun),
    ("taihen-context", postprocess_taihen),
    ("na-adjective-noun-use", postprocess_na_adj_noun),
    ("deverbal-noun-context", postprocess_deverbal_noun_context),
    ("attributive-mamonaku", postprocess_attributive_mamonaku),
    ("adverb-nominal-context", postprocess_adverb_nominal_context),
    ("nominal-conjunction-homograph", postprocess_nominal_conjunction_homograph),
    ("interjection-before-copula", postprocess_interjection_before_copula),
    ("interjection-after-te", postprocess_interjection_after_te),
    ("negative-conjunctive-de", postprocess_negative_conjunctive_de),
    ("temporal-nao-adverb", postprocess_temporal_nao),
    ("tsuke-noun", postprocess_tsuke_noun),
    ("copular-negative-pos", postprocess_copula_neg),
    ("you-noun", postprocess_you_noun),
    ("classical-ramu-boundary", postprocess_classical_ramu_boundary),
    ("classical-desiderative-aux", postprocess_classical_desiderative_aux),
    ("classical-honorific-aux", postprocess_classical_honorific_aux),
    ("classical-conjecture-aux", postprocess_classical_conjecture_aux),
    ("classical-kere-aux", postprocess_classical_kere_aux),
    ("classical-ha-row-past", postprocess_classical_ha_row_past),
    ("classical-b-row-moteiku", postprocess_classical_b_row_moteiku),
    ("classical-perfect-aux", postprocess_classical_perfect_aux),
    ("classical-perfect-ni", postprocess_classical_perfect_ni),
    ("classical-past-keri", postprocess_classical_past_keri),
    ("classical-past-shi", postprocess_classical_past_shi),
    ("classical-perfect-nu", postprocess_classical_perfect_nu),
    ("classical-ku-terminal", postprocess_classical_ku_terminal),
    ("adverbial-temporal-prefix", postprocess_adverbial_temporal_prefix),
    ("prolonged-sound-noun", postprocess_prolonged_sound_noun),
    ("yoshi-formal-noun", postprocess_yoshi_formal_noun),
    ("sou-aux", postprocess_sou_aux),
    ("nara-verb", postprocess_nara_verb),
    ("classical-nari-kateikei", postprocess_classical_nari_kateikei),
    ("classical-nari-after-attributive", postprocess_classical_nari_after_attributive),
    ("n-kuruwa", postprocess_n_kuruwa),
    ("determiner-wake-noun", postprocess_determiner_wake_noun),
    ("nai-context", postprocess_nai_context),
    ("obligation-nan-naru", postprocess_obligation_nan_naru),
    ("binding-negative-aux", postprocess_binding_negative_aux),
    ("verb-negative-aux", postprocess_verb_negative_aux),
    ("copula-negative-nee", postprocess_copula_negative_nee),
    ("productive-search-unit-boundaries", postprocess_productive_search_unit_boundaries),
    ("bound-derived-adjective", postprocess_bound_derived_adjective),
    ("quotative-determiner-spelling", postprocess_quotative_determiner_spelling),
    ("adverbial-na-adjective", postprocess_adverbial_na_adjective),
)


POSTPROCESSOR_MODULES = (
    postprocessor_common,
    postprocessor_mecab,
    postprocessor_function_words,
    postprocessor_predicates,
    postprocessor_nominals,
    postprocessor_subsidiaries,
    postprocessor_classical,
    postprocessor_boundaries,
)


def postprocessor_rules() -> tuple[tuple[str, Callable[[list[dict]], bool]], ...]:
    """Return the validated, ordered context-dependent postprocessors."""
    labels = [label for label, _ in POSTPROCESSORS]
    if len(labels) != len(set(labels)):
        raise RuntimeError("duplicate postprocessor rule label")

    processors = [processor for _, processor in POSTPROCESSORS]
    registered = set(processors)
    if len(processors) != len(registered):
        raise RuntimeError("duplicate postprocessor function")

    defined = {
        processor
        for module in POSTPROCESSOR_MODULES
        for name, processor in vars(module).items()
        if name.startswith("postprocess_")
        and name != "postprocess_mecab_tokens"
        and callable(processor)
        and processor.__module__ == module.__name__
    }
    if missing := defined - registered:
        names = ", ".join(sorted(processor.__name__ for processor in missing))
        raise RuntimeError(f"unregistered postprocessor: {names}")
    if extra := registered - defined:
        names = ", ".join(sorted(processor.__name__ for processor in extra))
        raise RuntimeError(f"unknown postprocessor: {names}")
    return POSTPROCESSORS
