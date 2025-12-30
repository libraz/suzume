#ifndef SUZUME_ANALYSIS_CONNECTION_RULES_H_
#define SUZUME_ANALYSIS_CONNECTION_RULES_H_

// =============================================================================
// Connection Rules
// =============================================================================
// This module defines grammatical connection rules between morphemes.
// It replaces hardcoded pattern matching in scorer.cpp with a structured
// rule-based approach for better maintainability and extensibility.
//
// Each rule defines:
// - Conditions: what prev/next morpheme patterns match
// - Cost adjustment: penalty (positive) or bonus (negative)
// - Description: for debugging and documentation
// =============================================================================

#include <string_view>

#include "analysis/connection_rule_options.h"
#include "core/lattice.h"
#include "core/types.h"

namespace suzume::analysis {

// =============================================================================
// Stem Ending Pattern Detection
// =============================================================================

/**
 * @brief Check if surface ends with i-row hiragana (godan renyokei marker)
 * Matches: み, き, ぎ, し, ち, に, び, り, い
 */
bool endsWithIRow(std::string_view surface);

/**
 * @brief Check if surface ends with e-row hiragana (ichidan renyokei marker)
 * Matches: べ, め, せ, け, げ, て, ね, れ, え, で, ぜ, へ, ぺ
 */
bool endsWithERow(std::string_view surface);

/**
 * @brief Check if surface ends with renyokei marker (either i-row or e-row)
 * Used to detect verb renyokei forms for auxiliary attachment
 */
bool endsWithRenyokeiMarker(std::string_view surface);

/**
 * @brief Check if surface ends with onbin marker (い, っ, ん)
 * Used to detect godan verb te/ta-form stems (音便形)
 */
bool endsWithOnbinMarker(std::string_view surface);

/**
 * @brief Check if surface ends with ku form (く)
 * Used to detect i-adjective adverbial form
 */
bool endsWithKuForm(std::string_view surface);

/**
 * @brief Check if surface ends with te-form marker (て or で)
 */
bool startsWithTe(std::string_view surface);

/**
 * @brief Check if surface ends with そう (auxiliary pattern)
 */
bool endsWithSou(std::string_view surface);

// =============================================================================
// Connection Pattern Enumeration
// =============================================================================

/**
 * @brief Identifiers for connection patterns
 * Used for debugging and rule identification
 */
enum class ConnectionPattern {
  None,                    // No special pattern matched
  Accumulated,             // Multiple rules matched (cumulative result)
  CopulaAfterVerb,         // VERB → だ/です (invalid unless そう pattern)
  IchidanRenyokeiTe,       // 一段連用形 → て/てV
  TeFormSplit,             // 音便形/一段形 → て/で (split should be avoided)
  TaiAfterRenyokei,        // 動詞連用形 → たい (valid auxiliary)
  YasuiAfterRenyokei,      // 連用形的名詞 → 安い (should be やすい aux)
  NagaraSplit,             // 動詞連用形 → ながら (split should be avoided)
  SouAfterRenyokei,        // 連用形的名詞 → そう (auxiliary pattern)
  CharacterSpeechSplit,    // だ/です → キャラ発話接尾辞
  AdjKuNaru,               // 形容詞く → なる (become pattern)
  CompoundAuxAfterRenyokei,// 連用形的名詞 → 複合動詞補助
  TakuTeSplit,             // 動詞たく形 → て (should be たくて as single token)
  TakuteAfterRenyokei,     // 動詞連用形 → たくて (should be single token)
  MasenDeSplit,            // AUXません形 → で (should be でした as single token)
  YoruNightAfterNi,        // に → よる(夜) (should prefer compound particle によると)
  ConditionalVerbToVerb,   // 条件形動詞 → 動詞 (e.g., あれば + 手伝う)
  VerbRenyokeiCompoundAux, // 動詞連用形 → 補助動詞 (e.g., 読み + 終わる)
  IruAuxAfterNoun,         // 名詞 → いる/います (AUX) (invalid pattern)
  IruAuxAfterTeForm,       // テ形動詞 → いる/います (AUX) (progressive aspect)
  FormalNounBeforeKanji,   // 形式名詞 → 漢字で始まる語 (invalid: 所+在する should be 所在する)
  TeFormVerbToVerb,        // テ形動詞 → 動詞 (e.g., 関して + 報告する)
  HiraganaNounStartsWithParticle,  // ひらがな名詞が助詞で始まる (e.g., もも after すもも)
  SameParticleRepeated,    // 同じ助詞が連続 (e.g., も + も)
  PrefixToShortStemHiraganaAdj,  // PREFIX → 短語幹純ひらがなADJ (e.g., お + いしい)
  SuffixAfterSymbol,       // SYMBOL → SUFFIX (invalid: 、家 should be NOUN, not SUFFIX)
  PrefixBeforeVerb,        // PREFIX → VERB (invalid: 何してる - 何 should be PRON)
  NounBeforeVerbAux,       // NOUN → verb-specific AUX (invalid: 行き(NOUN) + ましょう)
  ParticleBeforeAux,       // PARTICLE → AUX (invalid: と + う should not happen)
  NounBeforeNaAdj,         // NOUN → みたい etc (valid: 猫みたい)
  VerbBeforeNaAdj,         // VERB → みたい etc (valid: 食べるみたい)
  InvalidTeFormAux,        // テ形VERB → 無効単独AUX (e.g., して+る - should be してる)
  ShiParticleAfterPredicate,  // 活用語 → し接続助詞 (valid: 上手いし, 食べるし)
  ShiParticleAfterNoun,       // 名詞 → し接続助詞 (invalid: 本し - noun can't directly connect)
  TokuContractionSplit,       // VERB(連用形) → と (invalid: 食べ + と should be 食べといた)
  RashiiAfterPredicate,       // VERB/ADJ → らしい(ADJ) (conjecture auxiliary: 帰るらしい)
  VerbToCaseParticle          // VERB → case particle (を/が/に/で) (likely nominalized verb)
};

/**
 * @brief Result of connection rule evaluation
 *
 * When multiple rules apply, adjustment contains the clamped sum of all
 * individual adjustments. The matched_count field tracks how many rules
 * contributed to the final adjustment.
 */
struct ConnectionRuleResult {
  ConnectionPattern pattern{ConnectionPattern::None};
  float adjustment{0.0F};        // Positive = penalty, negative = bonus
  const char* description{nullptr};
  int matched_count{0};          // Number of rules that contributed

  // Accumulation limits (prevents extreme penalty stacking)
  static constexpr float kMinAdjustment = -3.0F;  // Maximum bonus
  static constexpr float kMaxAdjustment = 5.0F;   // Maximum penalty
};

// =============================================================================
// Rule Evaluation
// =============================================================================

/**
 * @brief Evaluate all connection rules for a morpheme pair
 * @param prev Previous morpheme
 * @param next Next morpheme
 * @param opts Connection options (penalty/bonus values)
 * @return Combined adjustment value and matched pattern info
 *
 * Returns the sum of all applicable rule adjustments.
 * Each rule is evaluated independently; multiple rules can apply.
 */
ConnectionRuleResult evaluateConnectionRules(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next,
                                             const ConnectionOptions& opts);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_CONNECTION_RULES_H_
