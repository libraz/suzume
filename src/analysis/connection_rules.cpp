#include "analysis/connection_rules.h"

#include "analysis/connection_rules_internal.h"

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
  if (surface.size() < core::kJapaneseCharBytes) {
    return false;
  }
  return surface.substr(surface.size() - core::kJapaneseCharBytes) == "く";
}

bool startsWithTe(std::string_view surface) {
  if (surface.size() < core::kJapaneseCharBytes) {
    return false;
  }
  std::string_view first3 = surface.substr(0, core::kJapaneseCharBytes);
  return first3 == "て" || first3 == "で";
}

bool endsWithSou(std::string_view surface) {
  if (surface.size() < core::kTwoJapaneseCharBytes) {
    return false;
  }
  return surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == "そう";
}

bool endsWithYou(std::string_view surface) {
  if (surface.size() < core::kTwoJapaneseCharBytes) {
    return false;
  }
  return surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == "よう";
}

bool endsWithNodaBase(std::string_view surface) {
  if (surface.size() < core::kJapaneseCharBytes) {
    return false;
  }
  std::string_view last = surface.substr(surface.size() - core::kJapaneseCharBytes);
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

ConnectionRuleResult evaluateConnectionRules(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next,
                                             const ConnectionOptions& opts) {
  // Evaluate ALL rules and accumulate adjustments
  // Multiple rules can apply simultaneously

  using namespace connection_rules;
  ConnectionRuleResult accumulated;

  // Verb conjugation rules
  accumulateRule(accumulated, checkCopulaAfterVerb(prev, next, opts));
  accumulateRule(accumulated, checkIchidanRenyokeiTe(prev, next, opts));
  accumulateRule(accumulated, checkTeFormSplit(prev, next, opts));
  accumulateRule(accumulated, checkTaiAfterRenyokei(prev, next, opts));
  accumulateRule(accumulated, checkYasuiAfterRenyokei(prev, next, opts));
  accumulateRule(accumulated, checkNagaraSplit(prev, next, opts));
  accumulateRule(accumulated, checkSouAfterRenyokei(prev, next, opts));

  // Other rules
  accumulateRule(accumulated, checkCharacterSpeechSplit(prev, next, opts));
  accumulateRule(accumulated, checkAdjKuNaru(prev, next, opts));

  // Verb rules (continued)
  accumulateRule(accumulated, checkCompoundAuxAfterRenyokei(prev, next, opts));
  accumulateRule(accumulated, checkTakuteAfterRenyokei(prev, next, opts));
  accumulateRule(accumulated, checkTakuTeSplit(prev, next, opts));
  accumulateRule(accumulated, checkTokuContractionSplit(prev, next, opts));

  // Auxiliary rules
  accumulateRule(accumulated, checkMasenDeSplit(prev, next, opts));

  // Other rules
  accumulateRule(accumulated, checkYoruNightAfterNi(prev, next, opts));

  // Verb rules
  accumulateRule(accumulated, checkConditionalVerbToVerb(prev, next, opts));
  accumulateRule(accumulated, checkVerbRenyokeiCompoundAux(prev, next, opts));

  // Auxiliary rules
  accumulateRule(accumulated, checkIruAuxAfterNoun(prev, next, opts));
  accumulateRule(accumulated, checkIruAuxAfterTeForm(prev, next, opts));

  // Verb rules
  accumulateRule(accumulated, checkTeFormVerbToVerb(prev, next, opts));
  accumulateRule(accumulated, checkRashiiAfterPredicate(prev, next, opts));
  accumulateRule(accumulated, checkVerbToCaseParticle(prev, next, opts));

  // Other rules
  accumulateRule(accumulated, checkFormalNounBeforeKanji(prev, next, opts));
  accumulateRule(accumulated, checkHiraganaNounStartsWithParticle(prev, next, opts));
  accumulateRule(accumulated, checkSameParticleRepeated(prev, next, opts));
  accumulateRule(accumulated, checkPrefixToShortStemHiraganaAdj(prev, next, opts));
  accumulateRule(accumulated, checkSuffixAfterSymbol(prev, next, opts));

  // Verb rules
  accumulateRule(accumulated, checkPrefixBeforeVerb(prev, next, opts));

  // Auxiliary rules
  accumulateRule(accumulated, checkNounBeforeVerbAux(prev, next, opts));
  accumulateRule(accumulated, checkMaiAfterNoun(prev, next, opts));
  accumulateRule(accumulated, checkAuxAfterParticle(prev, next, opts));
  accumulateRule(accumulated, checkInvalidTeFormAux(prev, next, opts));

  // Other rules
  accumulateRule(accumulated, checkParticleBeforeHiraganaOther(prev, next, opts));

  // Auxiliary rules
  accumulateRule(accumulated, checkMitaiAfterNounOrVerb(prev, next, opts));

  // Particle rules
  accumulateRule(accumulated, checkShiParticleConnection(prev, next, opts));

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
