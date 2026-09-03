#include "analysis/scorer.h"
#include "analysis/scorer_constants.h"
#include "core/types.h"
#include "grammar/char_patterns.h"

namespace sc = suzume::analysis::scorer;

namespace suzume::analysis {

float Scorer::bosCost(const core::LatticeEdge& edge) const {
  if (edge.extended_pos == core::ExtendedPOS::Conjunction && grammar::isCopulaFusedConjunction(edge.surface)) {
    return sc::kBosDemoConjunctionBonus;
  }
  if (edge.extended_pos == core::ExtendedPOS::Conjunction && grammar::isConditionalToConjunction(edge.surface)) {
    return sc::kBosConditionalToConjunctionBonus;
  }
  if (edge.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isFormalNounConjunctiveParticle(edge.surface)) {
    return sc::scale::kAlmostNever;
  }
  return sc::getBoundaryCost(edge.extended_pos).bos;
}

float Scorer::eosCost(const core::LatticeEdge& edge, core::ExtendedPOS prev_extended_pos) const {
  if (edge.extended_pos == core::ExtendedPOS::NounFormal && prev_extended_pos == core::ExtendedPOS::VerbRenyokei &&
      !grammar::isSubstantiveFormalNoun(edge.surface)) {
    return sc::kEosRenyokeiFormalNounPenalty;
  }

  const sc::BoundaryCost boundary_cost = sc::getBoundaryCost(edge.extended_pos);

  switch (boundary_cost.eos_gate) {
    case sc::EosBoundaryGate::Always:
      return boundary_cost.eos;
    case sc::EosBoundaryGate::SingleCodepoint:
      // The bare renyokei き needs a following た/て/ます, while the
      // 終止形 くる/くれる legitimately ends a sentence.
      return edge.end - edge.start == 1 ? boundary_cost.eos : sc::scale::kNeutral;
    case sc::EosBoundaryGate::ListingParticle:
      return grammar::isListingParticleTariSurface(edge.surface) ? boundary_cost.eos : sc::scale::kNeutral;
    case sc::EosBoundaryGate::NonDictionary:
      return edge.fromDictionary() ? sc::scale::kNeutral : boundary_cost.eos;
    case sc::EosBoundaryGate::IzenkeiNegative:
      // Only the 已然形 ね is barred: it needs ば or ど(も) after it, while the
      // other cells of the same auxiliary close a clause (読ま+ず, 知ら+ぬ).
      return grammar::isSingleHiragana(edge.surface, U'ね') ? boundary_cost.eos : sc::scale::kNeutral;
    case sc::EosBoundaryGate::AfterContent:
      // The sentence start arrives here as Unknown, and a punctuation mark
      // opens a fragment; in both the token introduces what follows instead.
      return prev_extended_pos == core::ExtendedPOS::Unknown || prev_extended_pos == core::ExtendedPOS::Symbol
                 ? sc::scale::kNeutral
                 : boundary_cost.eos;
  }

  return sc::scale::kNeutral;
}

}  // namespace suzume::analysis
