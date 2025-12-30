#ifndef SUZUME_ANALYSIS_CONNECTION_RULE_OPTIONS_H_
#define SUZUME_ANALYSIS_CONNECTION_RULE_OPTIONS_H_

// =============================================================================
// Connection Rule Options
// =============================================================================
// This file defines a struct that holds all adjustable parameters for
// connection rule scoring. Default values match the constexpr values from
// scorer_constants.h for backward compatibility.
//
// These options can be loaded from JSON for parameter tuning without rebuild.
// =============================================================================

namespace suzume::analysis {

/// Options for edge (unigram) scoring penalties
struct EdgeOptions {
  // Unknown adjective ending with そう but invalid lemma
  float penalty_invalid_adj_sou = 1.5F;

  // Unknown adjective with invalid たい pattern
  float penalty_invalid_tai_pattern = 2.0F;

  // Unknown adjective containing verb+auxiliary patterns
  float penalty_verb_aux_in_adj = 2.0F;

  // しまい/じまい parsed as adjective
  float penalty_shimai_as_adj = 3.0F;

  // Adjective lemma containing verb onbin + contraction patterns
  float penalty_verb_onbin_as_adj = 2.0F;

  // Short-stem pure hiragana unknown adjective
  float penalty_short_stem_hiragana_adj = 3.0F;

  // Verb ending with たいらしい (should be split)
  float penalty_verb_tai_rashii = 0.5F;

  // Unknown adjective with verb+ない pattern
  float penalty_verb_nai_pattern = 1.5F;

  // Bonus for unified verb forms containing auxiliary patterns (てしまった, てもらった, etc.)
  // This helps unified forms beat split paths when the te-form has a dictionary entry
  float bonus_unified_verb_aux = 0.3F;
};

/// Options for connection (bigram) scoring penalties/bonuses
struct ConnectionOptions {
  // === Verb Connection Rules ===

  // Copula after verb without そう pattern
  float penalty_copula_after_verb = 3.0F;

  // Ichidan renyokei + て split
  float penalty_ichidan_renyokei_te = 1.5F;

  // たい adjective after verb renyokei (bonus = negative)
  float bonus_tai_after_renyokei = 0.8F;

  // やすい (cheap) after renyokei-like noun
  float penalty_yasui_after_renyokei = 2.0F;

  // VERB + ながら split
  float penalty_nagara_split = 1.0F;

  // NOUN + そう when noun looks like verb renyokei
  float penalty_sou_after_renyokei = 0.5F;

  // Te-form split penalty
  float penalty_te_form_split = 1.5F;

  // VERB + て split when verb ends with たく
  float penalty_taku_te_split = 2.0F;

  // VERB renyokei + たくて split
  float penalty_takute_after_renyokei = 1.5F;

  // Conditional verb + result verb bonus
  float bonus_conditional_verb_to_verb = 0.7F;

  // Verb renyokei + compound auxiliary bonus
  float bonus_verb_renyokei_compound_aux = 1.0F;

  // Verb renyokei + と contraction split
  float penalty_toku_contraction_split = 1.5F;

  // Te-form VERB + VERB bonus
  float bonus_te_form_verb_to_verb = 0.8F;

  // らしい after verb/adjective bonus
  float bonus_rashii_after_predicate = 0.8F;

  // Verb (renyokei/base) + case particle (を/が/に/で/から/まで/へ)
  // Penalizes patterns like 打ち合わせ(VERB)+を which should be NOUN+を
  float penalty_verb_to_case_particle = 1.5F;

  // === Auxiliary Connection Rules ===

  // AUX + たい pattern
  float penalty_tai_after_aux = 1.0F;

  // AUX(ません形) + で split
  float penalty_masen_de_split = 2.0F;

  // Invalid single-char aux after te-form
  float penalty_invalid_single_char_aux = 5.0F;

  // Te-form + た contraction
  float penalty_te_form_ta_contraction = 1.5F;

  // NOUN + まい
  float penalty_noun_mai = 1.5F;

  // Short/unknown aux after particle
  float penalty_short_aux_after_particle = 3.0F;

  // NOUN + みたい bonus
  float bonus_noun_mitai = 3.0F;

  // VERB + みたい bonus
  float bonus_verb_mitai = 1.0F;

  // NOUN + いる/います/いません
  float penalty_iru_aux_after_noun = 2.0F;

  // Te-form + いる bonus
  float bonus_iru_aux_after_te_form = 0.5F;

  // === Other Connection Rules ===

  // AUX だ/です + character speech suffix split
  float penalty_character_speech_split = 1.0F;

  // ADJ(く) + なる bonus
  float bonus_adj_ku_naru = 0.5F;

  // Compound verb aux after renyokei-like noun
  float penalty_compound_aux_after_renyokei = 0.5F;

  // に + よる (夜) split
  float penalty_yoru_night_after_ni = 1.5F;

  // Formal noun + kanji
  float penalty_formal_noun_before_kanji = 3.0F;

  // Same particle repeated
  float penalty_same_particle_repeated = 2.0F;

  // Hiragana noun starts with particle char
  float penalty_hiragana_noun_starts_with_particle = 1.5F;

  // Particle before single hiragana OTHER
  float penalty_particle_before_single_hiragana_other = 2.5F;

  // Particle before multi hiragana OTHER
  float penalty_particle_before_multi_hiragana_other = 1.0F;

  // し after i-adjective bonus
  float bonus_shi_after_i_adj = 0.5F;

  // し after verb bonus
  float bonus_shi_after_verb = 0.3F;

  // し after auxiliary bonus
  float bonus_shi_after_aux = 0.3F;

  // し after noun penalty
  float penalty_shi_after_noun = 1.5F;

  // Suffix at sentence start
  float penalty_suffix_at_start = 3.0F;

  // Suffix after punctuation/symbol
  float penalty_suffix_after_symbol = 1.0F;

  // Prefix before verb/auxiliary
  float penalty_prefix_before_verb = 2.0F;

  // Noun before verb-specific auxiliary
  float penalty_noun_before_verb_aux = 2.0F;

  // PREFIX + short-stem pure hiragana adjective
  // Used in connection context (お + いしい is likely misanalysis)
  float penalty_prefix_short_stem_hiragana_adj = 3.0F;
};

/// Combined options for all connection rule scoring
struct ConnectionRuleOptions {
  EdgeOptions edge;
  ConnectionOptions connection;

  /// Create default options matching scorer_constants.h
  static ConnectionRuleOptions defaults() { return ConnectionRuleOptions{}; }
};

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_CONNECTION_RULE_OPTIONS_H_
