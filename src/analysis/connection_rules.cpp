#include "analysis/connection_rules.h"

#include "analysis/connection_rules_internal.h"
#include "analysis/scorer_constants.h"

namespace suzume::analysis {

// =============================================================================
// Stem Ending Pattern Detection (using grammar::char_patterns)
// =============================================================================

bool endsWithIRow(std::string_view surface) {
  return grammar::endsWithIRow(surface);
}

bool endsWithERow(std::string_view surface) {
  return grammar::endsWithERow(surface);
}

bool endsWithRenyokeiMarker(std::string_view surface) {
  return grammar::endsWithRenyokeiMarker(surface);
}

bool endsWithOnbinMarker(std::string_view surface) {
  return grammar::endsWithOnbin(surface);
}

bool endsWithKuForm(std::string_view surface) {
  return utf8::endsWith(surface, "く");
}

bool startsWithTe(std::string_view surface) {
  std::string_view first = utf8::firstNBytes(surface, core::kJapaneseCharBytes);
  return first == "て" || first == "で";
}

bool endsWithTeForm(std::string_view surface) {
  if (surface.size() < core::kJapaneseCharBytes) {
    return false;
  }
  std::string_view last = surface.substr(surface.size() - core::kJapaneseCharBytes);
  return last == "て" || last == "で";
}

bool endsWithSou(std::string_view surface) {
  return utf8::endsWith(surface, scorer::kSuffixSou);
}

bool endsWithYou(std::string_view surface) {
  return utf8::endsWith(surface, "よう");
}

bool endsWithNodaBase(std::string_view surface) {
  std::string_view last = utf8::lastChar(surface);
  return last == "の" || last == "ん";
}

// =============================================================================
// Main Rule Evaluation Function
// =============================================================================

namespace {

// Helper to accumulate rule results
inline void accumulateRule(ConnectionRuleResult& accumulated,
                           const ConnectionRuleResult& single) {
  if (single.pattern != ConnectionPattern::None) {
    accumulated.adjustment += single.adjustment;
    accumulated.matched_count++;
    // Keep first matched pattern for single-match case, otherwise Accumulated
    if (accumulated.pattern == ConnectionPattern::None) {
      accumulated.pattern = single.pattern;
      accumulated.description = single.description;
    } else if (accumulated.matched_count > 1) {
      accumulated.pattern = ConnectionPattern::Accumulated;
      accumulated.description = "Multiple rules matched";
    }
  }
}

}  // namespace

namespace connection_rules {

// =============================================================================
// POS-based Dispatch Implementations
// =============================================================================
// These functions group rules by prev.pos for efficient dispatch.
// Average reduction: 33 rule calls → 4-12 rule calls per evaluation.

void evaluateVerbRules(const core::LatticeEdge& prev,
                       const core::LatticeEdge& next,
                       const ConnectionOptions& opts,
                       ConnectionRuleResult& accumulated) {
  // VERB → AUX rules
  accumulateRule(accumulated, checkCopulaAfterVerb(prev, next, opts));
  accumulateRule(accumulated, checkIruAuxAfterTeForm(prev, next, opts));
  accumulateRule(accumulated, checkShimauAuxAfterTeForm(prev, next, opts));
  accumulateRule(accumulated, checkInvalidTeFormAux(prev, next, opts));

  // VERB → VERB rules
  accumulateRule(accumulated, checkIchidanRenyokeiTe(prev, next, opts));
  accumulateRule(accumulated, checkConditionalVerbToVerb(prev, next, opts));
  accumulateRule(accumulated, checkVerbRenyokeiCompoundAux(prev, next, opts));
  accumulateRule(accumulated, checkTeFormVerbToVerb(prev, next, opts));

  // VERB → PARTICLE rules
  accumulateRule(accumulated, checkTeFormSplit(prev, next, opts));
  accumulateRule(accumulated, checkNagaraSplit(prev, next, opts));
  accumulateRule(accumulated, checkTakuTeSplit(prev, next, opts));
  accumulateRule(accumulated, checkTokuContractionSplit(prev, next, opts));
  accumulateRule(accumulated, checkVerbToCaseParticle(prev, next, opts));
  accumulateRule(accumulated, checkShiParticleConnection(prev, next, opts));

  // VERB → ADJ rules
  accumulateRule(accumulated, checkTaiAfterRenyokei(prev, next, opts));
  accumulateRule(accumulated, checkTakuteAfterRenyokei(prev, next, opts));
  accumulateRule(accumulated, checkRashiiAfterPredicate(prev, next, opts));
  accumulateRule(accumulated, checkMitaiAfterNounOrVerb(prev, next, opts));

  // VERB → NOUN rules
  accumulateRule(accumulated, checkKataAfterRenyokei(prev, next, opts));
}

void evaluateNounRules(const core::LatticeEdge& prev,
                       const core::LatticeEdge& next,
                       const ConnectionOptions& opts,
                       ConnectionRuleResult& accumulated) {
  // NOUN → AUX rules
  accumulateRule(accumulated, checkIruAuxAfterNoun(prev, next, opts));
  accumulateRule(accumulated, checkNounBeforeVerbAux(prev, next, opts));
  accumulateRule(accumulated, checkMaiAfterNoun(prev, next, opts));
  accumulateRule(accumulated, checkNounIRowToVerbAux(prev, next, opts));

  // NOUN → VERB rules
  accumulateRule(accumulated, checkCompoundAuxAfterRenyokei(prev, next, opts));

  // NOUN → PARTICLE rules
  accumulateRule(accumulated, checkTeFormSplit(prev, next, opts));
  accumulateRule(accumulated, checkShiParticleConnection(prev, next, opts));
  accumulateRule(accumulated, checkNaParticleAfterKanjiNoun(prev, next, opts));

  // NOUN → ADJ rules
  accumulateRule(accumulated, checkYasuiAfterRenyokei(prev, next, opts));
  accumulateRule(accumulated, checkMitaiAfterNounOrVerb(prev, next, opts));

  // VERB/ADJ/AUX → ADJ rules
  accumulateRule(accumulated, checkKuraiAdjectiveAfterPredicate(prev, next, opts));

  // NOUN → ADV rules
  accumulateRule(accumulated, checkSouAfterRenyokei(prev, next, opts));

  // NOUN → NOUN rules
  accumulateRule(accumulated, checkHiraganaNounStartsWithParticle(prev, next, opts));

  // Check formal noun patterns (special case: requires flag or formal noun check)
  accumulateRule(accumulated, checkFormalNounBeforeKanji(prev, next, opts));
}

void evaluateAdjRules(const core::LatticeEdge& prev,
                      const core::LatticeEdge& next,
                      const ConnectionOptions& opts,
                      ConnectionRuleResult& accumulated) {
  // ADJ → VERB rules
  accumulateRule(accumulated, checkAdjKuNaru(prev, next, opts));

  // ADJ → PARTICLE rules
  accumulateRule(accumulated, checkTakuTeSplit(prev, next, opts));
  accumulateRule(accumulated, checkShiParticleConnection(prev, next, opts));

  // ADJ → ADJ rules
  accumulateRule(accumulated, checkRashiiAfterPredicate(prev, next, opts));
}

void evaluateAuxRules(const core::LatticeEdge& prev,
                      const core::LatticeEdge& next,
                      const ConnectionOptions& opts,
                      ConnectionRuleResult& accumulated) {
  // AUX → AUX rules
  accumulateRule(accumulated, checkCharacterSpeechSplit(prev, next, opts));

  // AUX → PARTICLE rules
  accumulateRule(accumulated, checkMasenDeSplit(prev, next, opts));
  accumulateRule(accumulated, checkShiParticleConnection(prev, next, opts));

  // AUX → ADJ rules (TaiAfterRenyokei handles AUX → たい penalty)
  accumulateRule(accumulated, checkTaiAfterRenyokei(prev, next, opts));
}

void evaluateParticleRules(const core::LatticeEdge& prev,
                           const core::LatticeEdge& next,
                           const ConnectionOptions& opts,
                           ConnectionRuleResult& accumulated) {
  // PARTICLE → AUX rules
  accumulateRule(accumulated, checkAuxAfterParticle(prev, next, opts));

  // PARTICLE → NOUN rules
  accumulateRule(accumulated, checkYoruNightAfterNi(prev, next, opts));

  // PARTICLE → PARTICLE rules
  accumulateRule(accumulated, checkSameParticleRepeated(prev, next, opts));
  accumulateRule(accumulated, checkSuspiciousParticleSequence(prev, next, opts));

  // PARTICLE → OTHER rules
  accumulateRule(accumulated, checkParticleBeforeHiraganaOther(prev, next, opts));

  // PARTICLE → VERB rules
  accumulateRule(accumulated, checkParticleBeforeHiraganaVerb(prev, next, opts));

  // PARTICLE → ADJ rules
  accumulateRule(accumulated, checkParticleBeforeHiraganaAdj(prev, next, opts));

  // PARTICLE → SUFFIX rules
  accumulateRule(accumulated, checkSuffixAfterNaParticle(prev, next, opts));
}

void evaluatePrefixRules(const core::LatticeEdge& prev,
                         const core::LatticeEdge& next,
                         const ConnectionOptions& opts,
                         ConnectionRuleResult& accumulated) {
  // PREFIX → VERB/AUX rules
  accumulateRule(accumulated, checkPrefixBeforeVerb(prev, next, opts));

  // PREFIX → ADJ rules
  accumulateRule(accumulated, checkPrefixToHiraganaAdj(prev, next, opts));
}

void evaluateSymbolRules(const core::LatticeEdge& prev,
                         const core::LatticeEdge& next,
                         const ConnectionOptions& opts,
                         ConnectionRuleResult& accumulated) {
  // SYMBOL → SUFFIX rules
  accumulateRule(accumulated, checkSuffixAfterSymbol(prev, next, opts));
}

}  // namespace connection_rules

ConnectionRuleResult evaluateConnectionRules(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next,
                                             const ConnectionOptions& opts) {
  // POS-based dispatch: evaluate only rules relevant to prev.pos
  // This reduces average rule evaluations from 33 to 4-12 per call.

  using namespace connection_rules;
  ConnectionRuleResult accumulated;

  switch (prev.pos) {
    case core::PartOfSpeech::Verb:
      evaluateVerbRules(prev, next, opts, accumulated);
      break;

    case core::PartOfSpeech::Noun:
      evaluateNounRules(prev, next, opts, accumulated);
      break;

    case core::PartOfSpeech::Adjective:
      evaluateAdjRules(prev, next, opts, accumulated);
      break;

    case core::PartOfSpeech::Auxiliary:
      evaluateAuxRules(prev, next, opts, accumulated);
      break;

    case core::PartOfSpeech::Particle:
      evaluateParticleRules(prev, next, opts, accumulated);
      break;

    case core::PartOfSpeech::Prefix:
      evaluatePrefixRules(prev, next, opts, accumulated);
      break;

    case core::PartOfSpeech::Symbol:
      evaluateSymbolRules(prev, next, opts, accumulated);
      break;

    default:
      // No connection rules for: Adverb, Suffix, Pronoun, Conjunction, Other
      break;
  }

  // Clamp accumulated adjustment to prevent extreme values
  if (accumulated.matched_count > 0) {
    if (accumulated.adjustment < ConnectionRuleResult::kMinAdjustment) {
      accumulated.adjustment = ConnectionRuleResult::kMinAdjustment;
    } else if (accumulated.adjustment > ConnectionRuleResult::kMaxAdjustment) {
      accumulated.adjustment = ConnectionRuleResult::kMaxAdjustment;
    }
  }

  return accumulated;
}

}  // namespace suzume::analysis
