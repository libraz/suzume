/**
 * @file inflection_scorer.cpp
 * @brief Confidence scoring orchestration for inflection analysis candidates
 */

#include <algorithm>

#include "inflection_scorer_constants.h"
#include "inflection_scorer_internal.h"

namespace suzume::grammar {

float calculateConfidence(VerbType type, std::string_view stem, size_t aux_total_len, size_t aux_count,
                          uint16_t required_conn, size_t suffix_len, std::string_view first_aux,
                          const InflectionScorerOptions* opts) {
  float base = GET_OPT(base_confidence, inflection::kBaseConfidence);

  SUZUME_DEBUG_LOG_TRACE("[INFL_SCORE] stem=\"" << stem << "\" type=" << static_cast<int>(type)
                                                << " aux_len=" << aux_total_len << " aux_count=" << aux_count
                                                << " conn=" << required_conn << " suffix_len=" << suffix_len
                                                << ": base=" << base << "\n");

  const inflection_score_detail::InflectionScoreContext context{type,          stem,       aux_total_len, aux_count,
                                                                required_conn, suffix_len, first_aux,     opts};
  base = inflection_score_detail::scoreStemAndIchidan(base, context);
  base = inflection_score_detail::scoreGodan(base, context);
  base = inflection_score_detail::scoreAdjectiveAndForm(base, context);
  base = inflection_score_detail::scorePotentialAndSuru(base, context);

  float ceiling = GET_OPT(confidence_ceiling, inflection::kConfidenceCeiling);
  float floor = GET_OPT(confidence_floor, inflection::kConfidenceFloor);
  float result = std::min(ceiling, std::max(floor, base));
  SUZUME_DEBUG_LOG_TRACE("[INFL_SCORE] → confidence=" << result << "\n");
  return result;
}

}  // namespace suzume::grammar
