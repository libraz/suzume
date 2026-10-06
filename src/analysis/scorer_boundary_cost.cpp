#include "analysis/scorer.h"
#include "analysis/scorer_connection_rules.h"
#include "analysis/scorer_constants.h"
#include "core/types.h"
#include "grammar/char_patterns.h"
#include "normalize/utf8.h"

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
  // The geminate the copula assimilated to cannot precede the sentence.
  if (edge.extended_pos == core::ExtendedPOS::AuxCopulaDa && grammar::isGeminateAssimilatedCopula(edge.surface)) {
    return sc::scale::kAlmostNever;
  }
  return sc::getBoundaryCost(edge.extended_pos).bos;
}

float Scorer::eosCost(const core::LatticeEdge& edge, core::ExtendedPOS prev_extended_pos, size_t prev_length) const {
  if (edge.extended_pos == core::ExtendedPOS::NounFormal && prev_extended_pos == core::ExtendedPOS::VerbRenyokei &&
      !grammar::isSubstantiveFormalNoun(edge.surface)) {
    return sc::kEosRenyokeiFormalNounPenalty;
  }

  if (edge.extended_pos == core::ExtendedPOS::VerbOnbinkei && utf8::endsWith(edge.surface, "っ")) {
    return sc::kEosSokuonOnbinPenalty;
  }

  // Only after content: a lone adverb (the whole input or a fragment after a
  // punctuation mark) is the clause opener itself.
  if (edge.pos == core::PartOfSpeech::Adverb && edge.fromDictionary() &&
      (utf8::endsWith(edge.surface, "て") || utf8::endsWith(edge.surface, "で")) &&
      prev_extended_pos != core::ExtendedPOS::Unknown && prev_extended_pos != core::ExtendedPOS::Symbol) {
    return sc::kEosTeEndingAdverbPenalty;
  }

  // A fabricated continuative after a one-character noun that closes the
  // utterance is the okurigana tail of a nominalized verb compound (日暮れ).
  if (edge.extended_pos == core::ExtendedPOS::VerbRenyokei && !edge.fromDictionary() &&
      prev_extended_pos == core::ExtendedPOS::Noun && prev_length == 1) {
    return sc::kEosNounContinuativePenalty;
  }

  // A 連体詞 modifies the nominal after it, so after a conjunctive て/で, which
  // asks for a predicate, closing the utterance on a listed one means its
  // homographic predicate reading instead (罰されて+しかる+べき).
  if (edge.extended_pos == core::ExtendedPOS::Determiner && edge.fromDictionary() &&
      prev_extended_pos == core::ExtendedPOS::ParticleConj) {
    return sc::kEosDeterminerPenalty;
  }

  // A subsidiary verb after the te-form is an imperative (読んで+くれ), not a noun.
  if (edge.extended_pos == core::ExtendedPOS::VerbRenyokei && edge.end - edge.start > 1 &&
      prev_extended_pos != core::ExtendedPOS::ParticleConj && grammar::endsWithERow(edge.surface)) {
    return sc::kEosBareERowRenyokeiPenalty;
  }

  // An attributive compound particle needs the noun it modifies; closing the
  // utterance on it means the case particle plus the predicate (場合+に+よる).
  if (connection_rules::isDependenceIdiomCompoundParticle(edge)) {
    return sc::kPenaltyAttributiveCompoundParticleClose;
  }

  const sc::BoundaryCost boundary_cost = sc::getBoundaryCost(edge.extended_pos);

  switch (boundary_cost.eos_gate) {
    case sc::EosBoundaryGate::Always:
      return boundary_cost.eos;
    case sc::EosBoundaryGate::SingleCodepoint:
      // The bare renyokei き needs a following た/て/ます, while the
      // 終止形 くる/くれる legitimately ends a sentence.
      return edge.end - edge.start == 1 ? boundary_cost.eos : sc::scale::kNeutral;
    case sc::EosBoundaryGate::ClauseDependentParticle:
      return grammar::isClauseDependentConjunctiveParticle(edge.surface) ? boundary_cost.eos : sc::scale::kNeutral;
    case sc::EosBoundaryGate::NonDictionary:
      return edge.fromDictionary() ? sc::scale::kNeutral : boundary_cost.eos;
    case sc::EosBoundaryGate::IzenkeiNegative:
      // Only the 已然形 ね is barred: it needs ば or ど(も) after it, while the
      // other cells of the same auxiliary close a clause (読ま+ず, 知ら+ぬ).
      return grammar::isSingleHiragana(edge.surface, U'ね') ? boundary_cost.eos : sc::scale::kNeutral;
    case sc::EosBoundaryGate::AttributiveCopula:
      // Only after a pronoun or the quotative: a trailing みたいな/静かな is an
      // elided-head attributive, while それな and 行けってな are the final particle.
      return grammar::isAttributiveCopulaNa(edge.surface) && (prev_extended_pos == core::ExtendedPOS::Pronoun ||
                                                              prev_extended_pos == core::ExtendedPOS::ParticleQuote)
                 ? boundary_cost.eos
                 : sc::scale::kNeutral;
    case sc::EosBoundaryGate::NonVolitionalStem:
      return grammar::endsWithORow(edge.surface) ? sc::scale::kNeutral : boundary_cost.eos;
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
