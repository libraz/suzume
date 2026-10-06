"""Facade for the focused merge post-processing passes."""

from . import (
    merge_postprocessor_affixes,
    merge_postprocessor_classical_adjectives,
    merge_postprocessor_classical_verbs,
    merge_postprocessor_common,
    merge_postprocessor_grammar,
    merge_postprocessor_spelling,
)

# Exports from merge_postprocessor_affixes.
_postprocess_bound_prefix_adjective = merge_postprocessor_affixes._postprocess_bound_prefix_adjective
_postprocess_bound_suffix_noun_cell = merge_postprocessor_affixes._postprocess_bound_suffix_noun_cell
_postprocess_bound_voiced_suffix = merge_postprocessor_affixes._postprocess_bound_voiced_suffix
_postprocess_distributive_quantity = merge_postprocessor_affixes._postprocess_distributive_quantity
_postprocess_nominal_zukeru = merge_postprocessor_affixes._postprocess_nominal_zukeru
_postprocess_search_unit_split = merge_postprocessor_affixes._postprocess_search_unit_split

# Exports from merge_postprocessor_classical_adjectives.
KARI_MIZENKEI_CELL = merge_postprocessor_classical_adjectives.KARI_MIZENKEI_CELL
_kari_cell_analysis = merge_postprocessor_classical_adjectives._kari_cell_analysis
_postprocess_adj_bungo = merge_postprocessor_classical_adjectives._postprocess_adj_bungo
_postprocess_adj_kari = merge_postprocessor_classical_adjectives._postprocess_adj_kari
_postprocess_predicate_shi = merge_postprocessor_classical_adjectives._postprocess_predicate_shi
_postprocess_ku_nominalization = merge_postprocessor_classical_adjectives._postprocess_ku_nominalization
classical_adjective_lemma = merge_postprocessor_classical_adjectives.classical_adjective_lemma

# Exports from merge_postprocessor_classical_verbs.
NIDAN_TERMINAL_KANA = merge_postprocessor_classical_verbs.NIDAN_TERMINAL_KANA
_postprocess_classical_kemu = merge_postprocessor_classical_verbs._postprocess_classical_kemu
_postprocess_classical_ki = merge_postprocessor_classical_verbs._postprocess_classical_ki
_postprocess_classical_continuative_host = merge_postprocessor_classical_verbs._postprocess_classical_continuative_host
_postprocess_classical_mu = merge_postprocessor_classical_verbs._postprocess_classical_mu
_postprocess_classical_shimu = merge_postprocessor_classical_verbs._postprocess_classical_shimu
_postprocess_ha_row_godan = merge_postprocessor_classical_verbs._postprocess_ha_row_godan
_postprocess_historical_kana_word = merge_postprocessor_classical_verbs._postprocess_historical_kana_word
_postprocess_plain_verb_e_row_lemma = merge_postprocessor_classical_verbs._postprocess_plain_verb_e_row_lemma
_postprocess_kakari_pronoun_split = merge_postprocessor_classical_verbs._postprocess_kakari_pronoun_split
_postprocess_nidan_cell = merge_postprocessor_classical_verbs._postprocess_nidan_cell
_postprocess_nominal_classical_copula = merge_postprocessor_classical_verbs._postprocess_nominal_classical_copula
_postprocess_nominal_copula_naru = merge_postprocessor_classical_verbs._postprocess_nominal_copula_naru
_postprocess_tomo_particle = merge_postprocessor_classical_verbs._postprocess_tomo_particle
nidan_cell = merge_postprocessor_classical_verbs.nidan_cell

# Exports from merge_postprocessor_common.
_probe_continuative = merge_postprocessor_common._probe_continuative
reads_as_continuative = merge_postprocessor_common.reads_as_continuative

# Exports from merge_postprocessor_grammar.
_postprocess_atode = merge_postprocessor_grammar._postprocess_atode
_postprocess_decomposable_adverb = merge_postprocessor_grammar._postprocess_decomposable_adverb
_postprocess_demo_copula = merge_postprocessor_grammar._postprocess_demo_copula
_postprocess_derivational_nominal_suffix = merge_postprocessor_grammar._postprocess_derivational_nominal_suffix
_postprocess_epenthetic_sa = merge_postprocessor_grammar._postprocess_epenthetic_sa
_postprocess_filler_split = merge_postprocessor_grammar._postprocess_filler_split
_postprocess_gamashii = merge_postprocessor_grammar._postprocess_gamashii
_postprocess_honorific_split = merge_postprocessor_grammar._postprocess_honorific_split
_postprocess_ichidan_imperative_yo = merge_postprocessor_grammar._postprocess_ichidan_imperative_yo
_postprocess_kamo = merge_postprocessor_grammar._postprocess_kamo
_postprocess_kuruwa = merge_postprocessor_grammar._postprocess_kuruwa
_postprocess_nde_split = merge_postprocessor_grammar._postprocess_nde_split
_postprocess_nominal_before_conjunctive_te = merge_postprocessor_grammar._postprocess_nominal_before_conjunctive_te
_postprocess_noni = merge_postprocessor_grammar._postprocess_noni
_postprocess_prefix_split = merge_postprocessor_grammar._postprocess_prefix_split
_postprocess_totomoni = merge_postprocessor_grammar._postprocess_totomoni
_postprocess_word_internal_honorific_prefix = merge_postprocessor_grammar._postprocess_word_internal_honorific_prefix

# Exports from merge_postprocessor_spelling.
_postprocess_ascii_joiner_merge = merge_postprocessor_spelling._postprocess_ascii_joiner_merge
_postprocess_dialectal = merge_postprocessor_spelling._postprocess_dialectal
_postprocess_kanji_merge = merge_postprocessor_spelling._postprocess_kanji_merge
_postprocess_nickname_merge = merge_postprocessor_spelling._postprocess_nickname_merge
_postprocess_onomatopoeia_tto_merge = merge_postprocessor_spelling._postprocess_onomatopoeia_tto_merge
_postprocess_productive_mimetics = merge_postprocessor_spelling._postprocess_productive_mimetics
_postprocess_small_kana_head_merge = merge_postprocessor_spelling._postprocess_small_kana_head_merge
_postprocess_stranded_lengthening_vowel = merge_postprocessor_spelling._postprocess_stranded_lengthening_vowel
_postprocess_stranded_okurigana = merge_postprocessor_spelling._postprocess_stranded_okurigana
_postprocess_variation_selector_merge = merge_postprocessor_spelling._postprocess_variation_selector_merge


def apply_merge_postprocessors(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Apply merge post-processors in the historical order."""
    result = _postprocess_kamo(result, applied_rule)
    result, applied_rule = _postprocess_totomoni(result, applied_rule)
    result, applied_rule = _postprocess_noni(result, applied_rule)
    result, applied_rule = _postprocess_atode(result, applied_rule)
    _postprocess_epenthetic_sa(result)
    result, applied_rule = _postprocess_honorific_split(result, applied_rule)
    result, applied_rule = _postprocess_prefix_split(result, applied_rule)
    result, applied_rule = _postprocess_nde_split(result, applied_rule)
    result, applied_rule = _postprocess_filler_split(result, applied_rule)
    result, applied_rule = _postprocess_kuruwa(result, applied_rule)
    result, applied_rule = _postprocess_demo_copula(result, applied_rule)
    result, applied_rule = _postprocess_gamashii(result, applied_rule)
    result, applied_rule = _postprocess_adj_bungo(result, applied_rule)
    result, applied_rule = _postprocess_predicate_shi(result, applied_rule)
    result, applied_rule = _postprocess_adj_kari(result, applied_rule)
    result, applied_rule = _postprocess_ha_row_godan(result, applied_rule)
    result, applied_rule = _postprocess_nidan_cell(result, applied_rule)
    result, applied_rule = _postprocess_nominal_classical_copula(result, applied_rule)
    result, applied_rule = _postprocess_historical_kana_word(result, applied_rule)
    result, applied_rule = _postprocess_kakari_pronoun_split(result, applied_rule)
    result, applied_rule = _postprocess_classical_mu(result, applied_rule)
    result, applied_rule = _postprocess_ku_nominalization(result, applied_rule)
    result, applied_rule = _postprocess_classical_shimu(result, applied_rule)
    result, applied_rule = _postprocess_classical_kemu(result, applied_rule)
    result, applied_rule = _postprocess_classical_ki(result, applied_rule)
    result, applied_rule = _postprocess_classical_continuative_host(result, applied_rule)
    result, applied_rule = _postprocess_nominal_copula_naru(result, applied_rule)
    result, applied_rule = _postprocess_nominal_before_conjunctive_te(result, applied_rule)
    result, applied_rule = _postprocess_derivational_nominal_suffix(result, applied_rule)
    result, applied_rule = _postprocess_ichidan_imperative_yo(result, applied_rule)
    result, applied_rule = _postprocess_stranded_okurigana(result, applied_rule)
    result, applied_rule = _postprocess_decomposable_adverb(result, applied_rule)
    result, applied_rule = _postprocess_bound_prefix_adjective(result, applied_rule)
    result, applied_rule = _postprocess_word_internal_honorific_prefix(result, applied_rule)
    result, applied_rule = _postprocess_variation_selector_merge(result, applied_rule)
    result, applied_rule = _postprocess_plain_verb_e_row_lemma(result, applied_rule)
    result, applied_rule = _postprocess_tomo_particle(result, applied_rule)
    result, applied_rule = _postprocess_bound_voiced_suffix(result, applied_rule)
    result, applied_rule = _postprocess_bound_suffix_noun_cell(result, applied_rule)
    result, applied_rule = _postprocess_kanji_merge(result, applied_rule)
    result, applied_rule = _postprocess_nickname_merge(result, applied_rule)
    result, applied_rule = _postprocess_search_unit_split(result, applied_rule)
    result, applied_rule = _postprocess_onomatopoeia_tto_merge(result, applied_rule)
    result, applied_rule = _postprocess_productive_mimetics(result, applied_rule)
    result, applied_rule = _postprocess_distributive_quantity(result, applied_rule)
    result, applied_rule = _postprocess_nominal_zukeru(result, applied_rule)
    result, applied_rule = _postprocess_ascii_joiner_merge(result, applied_rule)
    result, applied_rule = _postprocess_small_kana_head_merge(result, applied_rule)
    result, applied_rule = _postprocess_stranded_lengthening_vowel(result, applied_rule)
    _postprocess_dialectal(result)
    return result, applied_rule
