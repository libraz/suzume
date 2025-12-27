#ifndef SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
#define SUZUME_ANALYSIS_SCORER_CONSTANTS_H_

// =============================================================================
// Scorer Constants
// =============================================================================
// This file centralizes all penalty and bonus values used in scoring.
// Rationale and tuning notes are documented alongside each constant.
//
// Naming convention:
//   kPenalty* - increases cost (discourages pattern)
//   kBonus*   - decreases cost (encourages pattern)
// =============================================================================

namespace suzume::analysis::scorer {

// =============================================================================
// Edge Costs (Unigram penalties for invalid patterns)
// =============================================================================

// Note: kPenaltyVerbSou and kPenaltyVerbSouDesu were removed
// to unify verb+そう as single token (走りそう → 走る, like 食べそう → 食べる)
// See: backup/technical_debt_action_plan.md section 3.3

// Unknown adjective ending with そう but invalid lemma
// Valid: おいしそう (lemma おいしい), Invalid: 食べそう (lemma 食べい)
constexpr float kPenaltyInvalidAdjSou = 1.5F;

// Unknown adjective with たい pattern where stem is invalid
// E.g., りたかった is invalid (り is not a valid verb stem)
constexpr float kPenaltyInvalidTaiPattern = 2.0F;

// Unknown adjective containing verb+auxiliary patterns
// E.g., 食べすぎてしまい should be verb+しまう, not adjective
constexpr float kPenaltyVerbAuxInAdj = 2.0F;

// しまい/じまい parsed as adjective (should be しまう renyokei)
constexpr float kPenaltyShimaiAsAdj = 3.0F;

// =============================================================================
// Connection Costs (Bigram penalties/bonuses)
// =============================================================================

// Copula (だ/です) after verb without そう pattern
// This is grammatically invalid in most cases
constexpr float kPenaltyCopulaAfterVerb = 3.0F;

// Ichidan renyokei + て/てV split (should be te-form)
// E.g., 教え + て should be 教えて
constexpr float kPenaltyIchidanRenyokeiTe = 1.5F;

// たい adjective after verb renyokei - this is valid
// E.g., 食べたい, 読みたい
constexpr float kBonusTaiAfterRenyokei = 0.8F;

// 安い (やすい) interpreted as cheap after renyokei-like noun
// Should be verb + やすい (easy to do)
constexpr float kPenaltyYasuiAfterRenyokei = 2.0F;

// VERB + ながら split when verb is in renyokei
// Should be single token like 飲みながら
constexpr float kPenaltyNagaraSplit = 1.0F;

// NOUN + そう when noun looks like verb renyokei
// Should be verb renyokei + そう auxiliary
constexpr float kPenaltySouAfterRenyokei = 0.5F;

// AUX だ/です + character speech suffix split
// E.g., だ + にゃ should be だにゃ (single token)
constexpr float kPenaltyCharacterSpeechSplit = 1.0F;

// ADJ(連用形・く) + VERB(なる) pattern
// E.g., 美しく + なる - very common grammatical pattern
constexpr float kBonusAdjKuNaru = 0.5F;

// Compound verb auxiliary after renyokei-like noun
// E.g., 読み + 終わる should be verb renyokei + auxiliary
constexpr float kPenaltyCompoundAuxAfterRenyokei = 0.5F;

// Unknown adjective with lemma ending in ない where stem looks like verb mizenkei
// E.g., 走らなければ with lemma 走らない is likely verb+aux, not true adjective
// True adjectives: 少ない, 危ない (stem doesn't end in あ段)
// Verb patterns: 走らない, 書かない (stem ends in あ段 = verb mizenkei)
constexpr float kPenaltyVerbNaiPattern = 1.5F;

// Noun/Verb + て/で split when prev ends with Godan onbin or Ichidan ending
// E.g., 書い + て should be 書いて (te-form), not split
// E.g., 教え + て should be 教えて (te-form), not split
constexpr float kPenaltyTeFormSplit = 1.5F;

// VERB + て split when verb ends with たく (desire adverbial form)
// E.g., 食べたく + て should be 食べたくて (single token)
// This prevents splitting たくて into たく + て
constexpr float kPenaltyTakuTeSplit = 2.0F;

// VERB renyokei + たくて (ADJ) split
// E.g., 飲み + たくて should be 飲みたくて (single token)
// This prevents splitting at the boundary before たくて
constexpr float kPenaltyTakuteAfterRenyokei = 1.5F;

// AUX + たい adjective pattern
// E.g., なり(AUX, だ) + たかった is unnatural
// Should be なり(VERB, なる) + たかった
constexpr float kPenaltyTaiAfterAux = 1.0F;

}  // namespace suzume::analysis::scorer

#endif  // SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
