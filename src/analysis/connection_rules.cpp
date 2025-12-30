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

// =============================================================================
// Main Rule Evaluation Function
// =============================================================================

ConnectionRuleResult evaluateConnectionRules(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next,
                                             const ConnectionOptions& opts) {
  // Evaluate all rules in order
  // Currently returns the first matching rule for simplicity
  // Future: could accumulate all adjustments

  using namespace connection_rules;
  ConnectionRuleResult result;

  // Verb conjugation rules
  result = checkCopulaAfterVerb(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkIchidanRenyokeiTe(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTeFormSplit(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTaiAfterRenyokei(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkYasuiAfterRenyokei(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkNagaraSplit(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkSouAfterRenyokei(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Other rules
  result = checkCharacterSpeechSplit(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkAdjKuNaru(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Verb rules (continued)
  result = checkCompoundAuxAfterRenyokei(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTakuteAfterRenyokei(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTakuTeSplit(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTokuContractionSplit(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Auxiliary rules
  result = checkMasenDeSplit(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Other rules
  result = checkYoruNightAfterNi(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Verb rules
  result = checkConditionalVerbToVerb(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkVerbRenyokeiCompoundAux(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Auxiliary rules
  result = checkIruAuxAfterNoun(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkIruAuxAfterTeForm(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Verb rules
  result = checkTeFormVerbToVerb(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkRashiiAfterPredicate(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkVerbToCaseParticle(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Other rules
  result = checkFormalNounBeforeKanji(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkHiraganaNounStartsWithParticle(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkSameParticleRepeated(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkPrefixToShortStemHiraganaAdj(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkSuffixAfterSymbol(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Verb rules
  result = checkPrefixBeforeVerb(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Auxiliary rules
  result = checkNounBeforeVerbAux(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkMaiAfterNoun(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkAuxAfterParticle(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkInvalidTeFormAux(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Other rules
  result = checkParticleBeforeHiraganaOther(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Auxiliary rules
  result = checkMitaiAfterNounOrVerb(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  // Particle rules
  result = checkShiParticleConnection(prev, next, opts);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  return {};  // No pattern matched
}

}  // namespace suzume::analysis
