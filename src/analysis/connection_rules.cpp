#include "analysis/connection_rules.h"

#include "analysis/scorer_constants.h"
#include "normalize/exceptions.h"

namespace suzume::analysis {

// =============================================================================
// Stem Ending Pattern Detection
// =============================================================================

// i-row hiragana characters (godan renyokei markers)
// UTF-8: each hiragana is 3 bytes
static constexpr std::string_view kIRowChars[] = {
    "み", "き", "ぎ", "し", "ち", "に", "び", "り", "い"};
static constexpr size_t kIRowCount = sizeof(kIRowChars) / sizeof(kIRowChars[0]);

// e-row hiragana characters (ichidan renyokei markers)
static constexpr std::string_view kERowChars[] = {
    "べ", "め", "せ", "け", "げ", "て", "ね", "れ", "え",
    "で", "ぜ", "へ", "ぺ"};
static constexpr size_t kERowCount = sizeof(kERowChars) / sizeof(kERowChars[0]);

// Onbin markers for godan te/ta-form stems
static constexpr std::string_view kOnbinChars[] = {"い", "っ", "ん"};
static constexpr size_t kOnbinCount =
    sizeof(kOnbinChars) / sizeof(kOnbinChars[0]);

bool endsWithIRow(std::string_view surface) {
  if (surface.size() < 3) {
    return false;
  }
  std::string_view last3 = surface.substr(surface.size() - 3);
  for (size_t i = 0; i < kIRowCount; ++i) {
    if (last3 == kIRowChars[i]) {
      return true;
    }
  }
  return false;
}

bool endsWithERow(std::string_view surface) {
  if (surface.size() < 3) {
    return false;
  }
  std::string_view last3 = surface.substr(surface.size() - 3);
  for (size_t i = 0; i < kERowCount; ++i) {
    if (last3 == kERowChars[i]) {
      return true;
    }
  }
  return false;
}

bool endsWithRenyokeiMarker(std::string_view surface) {
  return endsWithIRow(surface) || endsWithERow(surface);
}

bool endsWithOnbinMarker(std::string_view surface) {
  if (surface.size() < 3) {
    return false;
  }
  std::string_view last3 = surface.substr(surface.size() - 3);
  for (size_t i = 0; i < kOnbinCount; ++i) {
    if (last3 == kOnbinChars[i]) {
      return true;
    }
  }
  return false;
}

bool endsWithKuForm(std::string_view surface) {
  if (surface.size() < 3) {
    return false;
  }
  return surface.substr(surface.size() - 3) == "く";
}

bool startsWithTe(std::string_view surface) {
  if (surface.size() < 3) {
    return false;
  }
  std::string_view first3 = surface.substr(0, 3);
  return first3 == "て" || first3 == "で";
}

bool endsWithSou(std::string_view surface) {
  if (surface.size() < 6) {
    return false;
  }
  return surface.substr(surface.size() - 6) == "そう";
}

// =============================================================================
// Individual Rule Evaluators
// =============================================================================

namespace {

// Rule 1: Copula だ/です cannot follow verbs (except そう pattern)
ConnectionRuleResult checkCopulaAfterVerb(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Verb ||
      next.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }

  if (next.surface != "だ" && next.surface != "です") {
    return {};
  }

  // Exception: 〜そう + です is valid
  if (endsWithSou(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::CopulaAfterVerb, scorer::kPenaltyCopulaAfterVerb,
          "copula after verb"};
}

// Rule 2: Ichidan renyokei + て/てV split should be avoided
ConnectionRuleResult checkIchidanRenyokeiTe(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  // Check if next starts with て
  bool is_te_pattern = false;
  if (next.pos == core::PartOfSpeech::Particle && next.surface == "て") {
    is_te_pattern = true;
  } else if (next.pos == core::PartOfSpeech::Verb && startsWithTe(next.surface)) {
    is_te_pattern = true;
  }

  if (!is_te_pattern) {
    return {};
  }

  // Check if prev ends with e-row (ichidan renyokei)
  if (!endsWithERow(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::IchidanRenyokeiTe, scorer::kPenaltyIchidanRenyokeiTe,
          "ichidan renyokei + te pattern"};
}

// Rule 3: Te-form split (音便形 or 一段形 → て/で)
ConnectionRuleResult checkTeFormSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next) {
  if ((prev.pos != core::PartOfSpeech::Noun &&
       prev.pos != core::PartOfSpeech::Verb) ||
      next.pos != core::PartOfSpeech::Particle) {
    return {};
  }

  if (next.surface != "て" && next.surface != "で") {
    return {};
  }

  // Check for godan onbin or ichidan endings
  if (!endsWithOnbinMarker(prev.surface) && !endsWithERow(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::TeFormSplit, scorer::kPenaltyTeFormSplit,
          "te-form split pattern"};
}

// Rule 4: Verb renyokei + たい adjective handling
// Two cases:
// 1. Short forms (たくて, たくない, etc.): No bonus - should be unified as single token
// 2. Long forms (たくなってきた, etc.): Give bonus for proper connection
// Also penalizes AUX + たい patterns (e.g., なり(だ) + たかった)
ConnectionRuleResult checkTaiAfterRenyokei(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next) {
  if (next.pos != core::PartOfSpeech::Adjective || next.lemma != "たい") {
    return {};
  }

  // Penalize AUX + たい pattern (e.g., なり(だ) + たかった)
  if (prev.pos == core::PartOfSpeech::Auxiliary) {
    return {ConnectionPattern::TaiAfterRenyokei, scorer::kPenaltyTaiAfterAux,
            "tai-pattern after auxiliary (unnatural)"};
  }

  // Only process VERB + たい
  if (prev.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  // Short たい forms (たくて, たくない, たかった, たければ, etc.)
  // These are <= 12 bytes (4 hiragana chars) and should be unified with verb
  // Don't give bonus - let inflection analyzer handle as single token
  if (next.surface.size() <= 12) {
    return {};
  }

  // Long たい forms (たくなってきた, たくてたまらない, etc.)
  // These are complex compound patterns that benefit from bonus
  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  // Bonus (negative value) for long compound patterns
  return {ConnectionPattern::TaiAfterRenyokei, -scorer::kBonusTaiAfterRenyokei,
          "tai-pattern after verb renyokei"};
}

// Rule 5: Renyokei-like noun + やすい (安い) penalty
ConnectionRuleResult checkYasuiAfterRenyokei(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Noun ||
      next.pos != core::PartOfSpeech::Adjective) {
    return {};
  }

  if (next.surface != "やすい" || next.lemma != "安い") {
    return {};
  }

  if (!endsWithIRow(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::YasuiAfterRenyokei, scorer::kPenaltyYasuiAfterRenyokei,
          "yasui adj after renyokei-like noun"};
}

// Rule 6: Verb renyokei + ながら split penalty
ConnectionRuleResult checkNagaraSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Verb ||
      next.pos != core::PartOfSpeech::Particle) {
    return {};
  }

  if (next.surface != "ながら") {
    return {};
  }

  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::NagaraSplit, scorer::kPenaltyNagaraSplit,
          "nagara split after renyokei verb"};
}

// Rule 7: Renyokei-like noun + そう (adverb) penalty
ConnectionRuleResult checkSouAfterRenyokei(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Noun ||
      next.pos != core::PartOfSpeech::Adverb) {
    return {};
  }

  if (next.surface != "そう") {
    return {};
  }

  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::SouAfterRenyokei, scorer::kPenaltySouAfterRenyokei,
          "sou aux after renyokei-like noun"};
}

// Rule 8: だ/です + character speech suffix split penalty
ConnectionRuleResult checkCharacterSpeechSplit(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Auxiliary ||
      next.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }

  if (prev.surface != "だ" && prev.surface != "です") {
    return {};
  }

  // Character speech suffixes
  if (next.surface != "にゃ" && next.surface != "にゃん" &&
      next.surface != "わ" && next.surface != "のだ" &&
      next.surface != "よ" && next.surface != "ね" &&
      next.surface != "ぞ" && next.surface != "さ") {
    return {};
  }

  return {ConnectionPattern::CharacterSpeechSplit,
          scorer::kPenaltyCharacterSpeechSplit, "split character speech pattern"};
}

// Rule 9: Adjective く + なる (bonus)
ConnectionRuleResult checkAdjKuNaru(const core::LatticeEdge& prev,
                                    const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Adjective ||
      next.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  if (!endsWithKuForm(prev.surface)) {
    return {};
  }

  // Check if next is なる or starts with なり
  bool is_naru =
      next.lemma == "なる" ||
      (next.surface.size() >= 6 && next.surface.substr(0, 6) == "なり");

  if (!is_naru) {
    return {};
  }

  // Bonus (negative value)
  return {ConnectionPattern::AdjKuNaru, -scorer::kBonusAdjKuNaru,
          "adj-ku + naru pattern"};
}

// Rule 10: Renyokei-like noun + compound verb auxiliary penalty
ConnectionRuleResult checkCompoundAuxAfterRenyokei(
    const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Noun ||
      next.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  if (next.surface.size() < 3) {
    return {};
  }

  // Check if next starts with compound verb auxiliary kanji
  std::string_view first_char = next.surface.substr(0, 3);
  if (!normalize::isCompoundVerbAuxStart(first_char)) {
    return {};
  }

  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::CompoundAuxAfterRenyokei,
          scorer::kPenaltyCompoundAuxAfterRenyokei,
          "compound aux after renyokei-like noun"};
}

// Rule 11: VERB renyokei + たくて (ADJ) split penalty
// Prevents 飲み + たくて from being preferred over 飲みたくて
ConnectionRuleResult checkTakuteAfterRenyokei(const core::LatticeEdge& prev,
                                              const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Verb ||
      next.pos != core::PartOfSpeech::Adjective) {
    return {};
  }

  // Check if next is たくて form (ADJ with lemma たい)
  if (next.lemma != "たい" || next.surface != "たくて") {
    return {};
  }

  // Check if prev ends with renyokei marker
  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::TakuteAfterRenyokei,
          scorer::kPenaltyTakuteAfterRenyokei,
          "takute adj after renyokei verb"};
}

// Rule 12: Verb/Adjective たく + て split penalty
// Prevents 食べたく + て from being preferred over 食べたくて
// Also handles ADJ case: 見たく (ADJ) + て should be 見たくて
ConnectionRuleResult checkTakuTeSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next) {
  if ((prev.pos != core::PartOfSpeech::Verb &&
       prev.pos != core::PartOfSpeech::Adjective) ||
      next.pos != core::PartOfSpeech::Particle) {
    return {};
  }

  if (next.surface != "て") {
    return {};
  }

  // Check if prev ends with たく (desire adverbial form)
  // UTF-8: たく = 6 bytes
  if (prev.surface.size() < 6) {
    return {};
  }
  std::string_view last6 = prev.surface.substr(prev.surface.size() - 6);
  if (last6 != "たく") {
    return {};
  }

  return {ConnectionPattern::TakuTeSplit, scorer::kPenaltyTakuTeSplit,
          "taku + te split (should be takute)"};
}

}  // namespace

// =============================================================================
// Main Rule Evaluation Function
// =============================================================================

ConnectionRuleResult evaluateConnectionRules(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next) {
  // Evaluate all rules in order
  // Currently returns the first matching rule for simplicity
  // Future: could accumulate all adjustments

  ConnectionRuleResult result;

  result = checkCopulaAfterVerb(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkIchidanRenyokeiTe(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTeFormSplit(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTaiAfterRenyokei(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkYasuiAfterRenyokei(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkNagaraSplit(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkSouAfterRenyokei(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkCharacterSpeechSplit(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkAdjKuNaru(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkCompoundAuxAfterRenyokei(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTakuteAfterRenyokei(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  result = checkTakuTeSplit(prev, next);
  if (result.pattern != ConnectionPattern::None) {
    return result;
  }

  return {};  // No pattern matched
}

}  // namespace suzume::analysis
