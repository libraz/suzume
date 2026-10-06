#ifndef SUZUME_ANALYSIS_SCORER_CONNECTION_RULES_H_
#define SUZUME_ANALYSIS_SCORER_CONNECTION_RULES_H_

#include "core/debug.h"
#include "core/lattice.h"

namespace suzume::analysis::connection_rules::detail {

#ifdef SUZUME_DEBUG_INFO
inline void traceConnectionContribution(const core::LatticeEdge& prev, const core::LatticeEdge& next,
                                        const char* function, int line, float contribution) {
  if (contribution == float{}) {
    return;
  }
  SUZUME_DEBUG_LOG_TRACE("[CONNECTION] \"" << prev.surface << "\" -> \"" << next.surface << "\" " << function << ":"
                                           << line << " contribution=" << contribution << "\n");
}
#endif

}  // namespace suzume::analysis::connection_rules::detail

// `prev` and `next` are the two lattice edges accepted by every connection
// rule helper. In release/WASM builds this is exactly the original addition.
#ifdef SUZUME_DEBUG_INFO
#define SUZUME_CONNECTION_ADD(total, value)                                                                  \
  do {                                                                                                       \
    const float suzume_connection_contribution = (value);                                                    \
    (total) += suzume_connection_contribution;                                                               \
    suzume::analysis::connection_rules::detail::traceConnectionContribution(prev, next, __func__, __LINE__,  \
                                                                            suzume_connection_contribution); \
  } while (false)
#else
#define SUZUME_CONNECTION_ADD(total, value) ((total) += (value))
#endif

namespace suzume::analysis::connection_rules {

float computeVerbRenyokeiEarlyBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computePassiveCausativeBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeTaFormVolitionalBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeNegativeAndNounVerbBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeParticleDeterminerBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computePrefixSymbolBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);

/** @brief Waives the particle→polite-auxiliary bar for a continuative-based compound particle. */
float computeCompoundParticlePoliteBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);

/** @brief Whether an edge is a compound case particle in its attributive る cell (による, に関する). */
bool isAttributiveCompoundParticle(const core::LatticeEdge& edge);

/** @brief Bars an attributive compound particle from closing its modifier (before a particle, symbol or auxiliary). */
float computeAttributiveCompoundParticlePenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);

/** @brief Penalizes the case particle に after the universal quantifier とも, which opens ともに. */
float computeQuantifierTomoCasePenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);

/** @brief Bars a multi-mora conjunctive particle ending in で from governing the copular ある. */
float computeConjunctiveParticleCopulaPenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);

/** @brief Bars a finished predicate from introducing an adverb ending in the adverbializing に. */
float computeAdverbialNiAfterPredicatePenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeSuffixShortVerbBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeParticleQuoteBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeCompoundNominalizationBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeProgressiveHonorificBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeSugiFinalParticleBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeCopulaConditionalBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computePastConditionalVerbBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeExistentialAruNominalPredicateBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeCompletionAuxiliaryBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeBarePotentialRenyokeiPenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);

/** @brief Penalizes the recollective final particle け directly after a bare common noun. */
float computeBareNounRecollectiveKePenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeAdjectiveTePredicatePenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeClassicalNegativeBoundaryPenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);
float computeAdjectiveDerivationHostPenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next);

}  // namespace suzume::analysis::connection_rules

#endif  // SUZUME_ANALYSIS_SCORER_CONNECTION_RULES_H_
