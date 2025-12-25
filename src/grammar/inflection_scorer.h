/**
 * @file inflection_scorer.h
 * @brief Confidence scoring for inflection analysis candidates
 */

#ifndef SUZUME_GRAMMAR_INFLECTION_SCORER_H_
#define SUZUME_GRAMMAR_INFLECTION_SCORER_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "conjugation.h"

namespace suzume::grammar {

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
                          uint16_t required_conn);

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_INFLECTION_SCORER_H_
