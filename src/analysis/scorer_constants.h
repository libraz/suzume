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

// Verb ending with そう should split as verb + そう auxiliary
// E.g., 降りそう → 降り + そう, not single verb
constexpr float kPenaltyVerbSou = 1.0F;

// Verb ending with そうです should split further
// E.g., 食べそうです → 食べそう + です
constexpr float kPenaltyVerbSouDesu = 1.5F;

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

}  // namespace suzume::analysis::scorer

#endif  // SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
