#ifndef SUZUME_GRAMMAR_INFLECTION_SCORER_INTERNAL_H_
#define SUZUME_GRAMMAR_INFLECTION_SCORER_INTERNAL_H_

#include <string_view>

#include "core/debug.h"
#include "core/utf8_constants.h"
#include "inflection_scorer.h"

namespace suzume::grammar::inflection_score_detail {

struct InflectionScoreContext {
  VerbType type;
  std::string_view stem;
  size_t aux_total_len;
  size_t aux_count;
  uint16_t required_conn;
  size_t suffix_len;
  std::string_view first_aux;
  const InflectionScorerOptions* opts;
};

inline void logConfidenceAdjustment(float amount, [[maybe_unused]] const char* reason) {
  if (amount != 0.0F) {
    SUZUME_DEBUG_LOG_TRACE("  " << reason << ": " << (amount > 0 ? "+" : "") << amount << "\n");
  }
}

using ::utf8::equalsAny;

/// Bind every InflectionScoreContext field, plus the stem length every scoring
/// body needs, as locals so the bodies below read as plain expressions.
/// Adding a field to the context is then a single edit here, and no body can
/// drift by binding a different subset.
#define SUZUME_UNPACK_INFLECTION_CONTEXT(ctx)                          \
  [[maybe_unused]] const VerbType type = (ctx).type;                   \
  [[maybe_unused]] const std::string_view stem = (ctx).stem;           \
  [[maybe_unused]] const size_t aux_total_len = (ctx).aux_total_len;   \
  [[maybe_unused]] const size_t aux_count = (ctx).aux_count;           \
  [[maybe_unused]] const uint16_t required_conn = (ctx).required_conn; \
  [[maybe_unused]] const size_t suffix_len = (ctx).suffix_len;         \
  [[maybe_unused]] const std::string_view first_aux = (ctx).first_aux; \
  [[maybe_unused]] const InflectionScorerOptions* opts = (ctx).opts;   \
  [[maybe_unused]] const size_t stem_len = stem.size()

float scoreStemAndIchidan(float base, const InflectionScoreContext& context);
float scoreGodan(float base, const InflectionScoreContext& context);
float scoreAdjectiveAndForm(float base, const InflectionScoreContext& context);
float scorePotentialAndSuru(float base, const InflectionScoreContext& context);

}  // namespace suzume::grammar::inflection_score_detail

#endif  // SUZUME_GRAMMAR_INFLECTION_SCORER_INTERNAL_H_
