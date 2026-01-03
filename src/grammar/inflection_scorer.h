/**
 * @file inflection_scorer.h
 * @brief Confidence scoring for inflection analysis candidates
 */

#ifndef SUZUME_GRAMMAR_INFLECTION_SCORER_H_
#define SUZUME_GRAMMAR_INFLECTION_SCORER_H_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "conjugation.h"
#include "inflection_scorer_constants.h"

namespace suzume::grammar {

/**
 * @brief Options for inflection scorer (runtime overrides for constants)
 *
 * All values default to NaN, meaning "use the constexpr value from
 * inflection_scorer_constants.h". Set a value to override the constant.
 */
struct InflectionScorerOptions {
  // Base configuration
  float base_confidence = std::numeric_limits<float>::quiet_NaN();
  float confidence_floor = std::numeric_limits<float>::quiet_NaN();
  float confidence_ceiling = std::numeric_limits<float>::quiet_NaN();

  // Stem length adjustments
  float penalty_stem_very_long = std::numeric_limits<float>::quiet_NaN();
  float penalty_stem_long = std::numeric_limits<float>::quiet_NaN();
  float bonus_stem_two_char = std::numeric_limits<float>::quiet_NaN();
  float bonus_aux_length_per_byte = std::numeric_limits<float>::quiet_NaN();

  // Ichidan validation
  float penalty_ichidan_potential_ambiguity = std::numeric_limits<float>::quiet_NaN();
  float bonus_ichidan_e_row = std::numeric_limits<float>::quiet_NaN();
  float penalty_ichidan_looks_godan = std::numeric_limits<float>::quiet_NaN();
  float penalty_ichidan_kanji_i = std::numeric_limits<float>::quiet_NaN();
  float penalty_ichidan_kanji_hiragana_stem = std::numeric_limits<float>::quiet_NaN();
  float penalty_ichidan_irregular_stem = std::numeric_limits<float>::quiet_NaN();

  // I-Adjective validation
  float penalty_i_adj_single_kanji = std::numeric_limits<float>::quiet_NaN();
  float penalty_i_adj_verb_aux_pattern = std::numeric_limits<float>::quiet_NaN();
  float bonus_i_adj_compound_yasui_nikui = std::numeric_limits<float>::quiet_NaN();
  float penalty_i_adj_e_row_stem = std::numeric_limits<float>::quiet_NaN();
  float penalty_i_adj_verb_rashii_pattern = std::numeric_limits<float>::quiet_NaN();

  // Suru vs GodanSa disambiguation
  float bonus_suru_two_kanji = std::numeric_limits<float>::quiet_NaN();
  float penalty_godan_sa_two_kanji = std::numeric_limits<float>::quiet_NaN();
  float bonus_godan_sa_single_kanji = std::numeric_limits<float>::quiet_NaN();
  float penalty_suru_single_kanji = std::numeric_limits<float>::quiet_NaN();

  // Single hiragana stem penalties
  float penalty_ichidan_single_hiragana_particle = std::numeric_limits<float>::quiet_NaN();
  float penalty_pure_hiragana_stem = std::numeric_limits<float>::quiet_NaN();
  float penalty_godan_single_hiragana_stem = std::numeric_limits<float>::quiet_NaN();
  float penalty_godan_non_ra_pure_hiragana = std::numeric_limits<float>::quiet_NaN();
  float penalty_godan_te_stem = std::numeric_limits<float>::quiet_NaN();

  /// Helper to get value with fallback to constant
  template<typename T>
  static float getOrDefault(float opt_val, T const_val) {
    return std::isnan(opt_val) ? static_cast<float>(const_val) : opt_val;
  }
};

/**
 * @brief Calculate confidence score for an inflection candidate
 *
 * The confidence score reflects how likely the parsed inflection is correct.
 * Higher scores indicate more reliable matches.
 *
 * @param type The detected verb type
 * @param stem The extracted verb stem
 * @param aux_total_len Total length of matched auxiliary suffixes
 * @param aux_count Number of matched auxiliary suffixes
 * @param required_conn The required connection ID from the auxiliary chain
 * @param suffix_len Total length of suffix (verb ending + auxiliaries)
 * @param opts Optional overrides for scoring constants (nullptr = use defaults)
 * @return Confidence score in range [0.5, 0.95]
 *
 * Scoring factors include:
 * - Stem length (very long or very short stems get penalties)
 * - Auxiliary chain length (longer chains = more confidence)
 * - Verb type validation (e.g., Ichidan stems should end in e-row)
 * - Context-specific patterns (onbin, mizenkei, renyokei)
 * - Disambiguation between similar patterns (Suru vs GodanSa)
 */
float calculateConfidence(VerbType type, std::string_view stem,
                          size_t aux_total_len, size_t aux_count,
                          uint16_t required_conn, size_t suffix_len,
                          const InflectionScorerOptions* opts = nullptr);

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_INFLECTION_SCORER_H_
