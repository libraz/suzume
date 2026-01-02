#ifndef SUZUME_ANALYSIS_CONNECTION_RULE_OPTIONS_H_
#define SUZUME_ANALYSIS_CONNECTION_RULE_OPTIONS_H_

#include "scorer_constants.h"

// =============================================================================
// Connection Rule Options
// =============================================================================
// This file defines a struct that holds all adjustable parameters for
// connection rule scoring. Default values reference the constexpr values from
// scorer_constants.h to eliminate duplicate definitions.
//
// These options can be loaded from JSON for parameter tuning without rebuild.
// =============================================================================

namespace suzume::analysis {

/// Options for edge (unigram) scoring penalties
struct EdgeOptions {
  // Unknown adjective ending with そう but invalid lemma
  float penalty_invalid_adj_sou = scorer::kPenaltyInvalidAdjSou;

  // Unknown adjective with invalid たい pattern
  float penalty_invalid_tai_pattern = scorer::kPenaltyInvalidTaiPattern;

  // Unknown adjective containing verb+auxiliary patterns
  float penalty_verb_aux_in_adj = scorer::kPenaltyVerbAuxInAdj;

  // しまい/じまい parsed as adjective
  float penalty_shimai_as_adj = scorer::kPenaltyShimaiAsAdj;

  // Adjective lemma containing verb onbin + contraction patterns
  float penalty_verb_onbin_as_adj = scorer::kPenaltyVerbOnbinAsAdj;

  // Pure hiragana unknown adjective (unused, kept for config compatibility)
  float penalty_short_stem_hiragana_adj = scorer::kPenaltyHiraganaAdj;

  // Verb ending with たいらしい (should be split)
  float penalty_verb_tai_rashii = scorer::kPenaltyVerbTaiRashii;

  // Unknown adjective with verb+ない pattern
  float penalty_verb_nai_pattern = scorer::kPenaltyVerbNaiPattern;

  // Bonus for unified verb forms containing auxiliary patterns (てしまった, てもらった, etc.)
  // This helps unified forms beat split paths when the te-form has a dictionary entry
  float bonus_unified_verb_aux = scorer::kBonusUnifiedVerbAux;

  // Verb ending with さん where stem looks nominal (田中さん, おねえさん)
  // These should be NOUN + SUFFIX, not VERB with contracted negative
  float penalty_verb_san_honorific = scorer::kPenaltyVerbSanHonorific;

  // Verb ending with ん (contracted negative) with very short stem (いん)
  // Short contracted forms are often misanalysis
  float penalty_verb_contracted_neg_short_stem = scorer::kPenaltyVerbContractedNegShortStem;
};

/// Options for connection (bigram) scoring penalties/bonuses
struct ConnectionOptions {
  // === Verb Connection Rules ===

  // Copula after verb without そう pattern
  float penalty_copula_after_verb = scorer::kPenaltyCopulaAfterVerb;

  // Ichidan renyokei + て split
  float penalty_ichidan_renyokei_te = scorer::kPenaltyIchidanRenyokeiTe;

  // たい adjective after verb renyokei (bonus = positive value subtracted)
  float bonus_tai_after_renyokei = scorer::kBonusTaiAfterRenyokei;

  // やすい (cheap) after renyokei-like noun
  float penalty_yasui_after_renyokei = scorer::kPenaltyYasuiAfterRenyokei;

  // VERB + ながら split
  float penalty_nagara_split = scorer::kPenaltyNagaraSplit;

  // VERB renyokei + 方 (should be nominalized)
  float penalty_kata_after_renyokei = scorer::kPenaltyKataAfterRenyokei;

  // NOUN + そう when noun looks like verb renyokei
  float penalty_sou_after_renyokei = scorer::kPenaltySouAfterRenyokei;

  // Te-form split penalty
  float penalty_te_form_split = scorer::kPenaltyTeFormSplit;

  // VERB + て split when verb ends with たく
  float penalty_taku_te_split = scorer::kPenaltyTakuTeSplit;

  // VERB renyokei + たくて split
  float penalty_takute_after_renyokei = scorer::kPenaltyTakuteAfterRenyokei;

  // Conditional verb + result verb bonus
  float bonus_conditional_verb_to_verb = scorer::kBonusConditionalVerbToVerb;

  // Verb renyokei + compound auxiliary bonus
  float bonus_verb_renyokei_compound_aux = scorer::kBonusVerbRenyokeiCompoundAux;

  // Verb renyokei + と contraction split
  float penalty_toku_contraction_split = scorer::kPenaltyTokuContractionSplit;

  // Te-form VERB + VERB bonus
  float bonus_te_form_verb_to_verb = scorer::kBonusTeFormVerbToVerb;

  // らしい after verb/adjective bonus
  float bonus_rashii_after_predicate = scorer::kBonusRashiiAfterPredicate;

  // Verb (renyokei/base) + case particle (を/が/に/で/から/まで/へ)
  // Penalizes patterns like 打ち合わせ(VERB)+を which should be NOUN+を
  float penalty_verb_to_case_particle = scorer::kPenaltyVerbToCaseParticle;

  // === Auxiliary Connection Rules ===

  // AUX + たい pattern
  float penalty_tai_after_aux = scorer::kPenaltyTaiAfterAux;

  // AUX(ません形) + で split
  float penalty_masen_de_split = scorer::kPenaltyMasenDeSplit;

  // Invalid single-char aux after te-form
  float penalty_invalid_single_char_aux = scorer::kPenaltyInvalidSingleCharAux;

  // Te-form + た contraction
  float penalty_te_form_ta_contraction = scorer::kPenaltyTeFormTaContraction;

  // NOUN + まい
  float penalty_noun_mai = scorer::kPenaltyNounMai;

  // NOUN (i-row ending) + る/て/た(AUX) - likely ichidan verb split
  float penalty_noun_irow_to_verb_aux = scorer::scale::kStrong;

  // Short/unknown aux after particle
  float penalty_short_aux_after_particle = scorer::kPenaltyShortAuxAfterParticle;

  // NOUN + みたい bonus
  float bonus_noun_mitai = scorer::kBonusNounMitai;

  // VERB + みたい bonus
  float bonus_verb_mitai = scorer::kBonusVerbMitai;

  // NOUN + いる/います/いません
  float penalty_iru_aux_after_noun = scorer::kPenaltyIruAuxAfterNoun;

  // Te-form + いる bonus
  float bonus_iru_aux_after_te_form = scorer::kBonusIruAuxAfterTeForm;

  // Te-form + しまう bonus
  float bonus_shimau_aux_after_te_form = scorer::kBonusShimauAuxAfterTeForm;

  // === Other Connection Rules ===

  // AUX だ/です + character speech suffix split
  float penalty_character_speech_split = scorer::kPenaltyCharacterSpeechSplit;

  // ADJ(く) + なる bonus
  float bonus_adj_ku_naru = scorer::kBonusAdjKuNaru;

  // Compound verb aux after renyokei-like noun
  float penalty_compound_aux_after_renyokei = scorer::kPenaltyCompoundAuxAfterRenyokei;

  // に + よる (夜) split
  float penalty_yoru_night_after_ni = scorer::kPenaltyYoruNightAfterNi;

  // Formal noun + kanji
  float penalty_formal_noun_before_kanji = scorer::kPenaltyFormalNounBeforeKanji;

  // Same particle repeated
  float penalty_same_particle_repeated = scorer::kPenaltySameParticleRepeated;

  // Suspicious particle sequence (different particles in unlikely pattern)
  float penalty_suspicious_particle_sequence = scorer::kPenaltySuspiciousParticleSequence;

  // Hiragana noun starts with particle char
  float penalty_hiragana_noun_starts_with_particle = scorer::kPenaltyHiraganaNounStartsWithParticle;

  // Particle before single hiragana OTHER
  float penalty_particle_before_single_hiragana_other = scorer::kPenaltyParticleBeforeSingleHiraganaOther;

  // Particle before multi hiragana OTHER
  float penalty_particle_before_multi_hiragana_other = scorer::kPenaltyParticleBeforeMultiHiraganaOther;

  // Particle before hiragana VERB (likely split of hiragana verb)
  float penalty_particle_before_hiragana_verb = scorer::kPenaltyParticleBeforeHiraganaVerb;

  // し after i-adjective bonus
  float bonus_shi_after_i_adj = scorer::kBonusShiAfterIAdj;

  // し after verb bonus
  float bonus_shi_after_verb = scorer::kBonusShiAfterVerb;

  // し after auxiliary bonus
  float bonus_shi_after_aux = scorer::kBonusShiAfterAux;

  // し after noun penalty
  float penalty_shi_after_noun = scorer::kPenaltyShiAfterNoun;

  // な particle after kanji noun penalty
  // Kanji noun + な(PARTICLE) is almost always na-adjective pattern
  float penalty_na_particle_after_kanji_noun =
      scorer::kPenaltyNaParticleAfterKanjiNoun;

  // Suffix at sentence start
  float penalty_suffix_at_start = scorer::kPenaltySuffixAtStart;

  // Suffix after punctuation/symbol
  float penalty_suffix_after_symbol = scorer::kPenaltySuffixAfterSymbol;

  // Prefix before verb/auxiliary
  float penalty_prefix_before_verb = scorer::kPenaltyPrefixBeforeVerb;

  // Noun before verb-specific auxiliary
  float penalty_noun_before_verb_aux = scorer::kPenaltyNounBeforeVerbAux;

  // PREFIX + pure hiragana adjective
  // Used in connection context (お + こがましい is likely misanalysis)
  float penalty_prefix_hiragana_adj = scorer::kPenaltyHiraganaAdj;

  // PARTICLE + pure hiragana adjective
  // Used in connection context (は + なはだしい is likely misanalysis)
  float penalty_particle_before_hiragana_adj = scorer::kPenaltyHiraganaAdj;
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
