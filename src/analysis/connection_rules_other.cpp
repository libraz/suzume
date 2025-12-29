#include "analysis/connection_rules_internal.h"

namespace suzume::analysis {
namespace connection_rules {

// =============================================================================
// Other Connection Rules
// =============================================================================

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
      (next.surface.size() >= core::kTwoJapaneseCharBytes &&
       next.surface.substr(0, core::kTwoJapaneseCharBytes) == "なり");

  if (!is_naru) {
    return {};
  }

  // Bonus (negative value)
  return {ConnectionPattern::AdjKuNaru, -scorer::kBonusAdjKuNaru,
          "adj-ku + naru pattern"};
}

// Rule: PREFIX → short-stem pure hiragana adjective
// E.g., お + いしい is likely misanalysis (should be おいしい)
// Valid short hiragana adjectives (すごい, うまい, やばい) are in dictionary
// Unknown short-stem (≤2 chars) hiragana adjectives after PREFIX are penalized
ConnectionRuleResult checkPrefixToShortStemHiraganaAdj(
    const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // Previous must be PREFIX
  if (prev.pos != core::PartOfSpeech::Prefix) {
    return {};
  }

  // Next must be unknown adjective
  if (next.pos != core::PartOfSpeech::Adjective || next.fromDictionary()) {
    return {};
  }

  // Check if lemma is at least valid length (2 chars for い-adj)
  if (next.lemma.size() < core::kTwoJapaneseCharBytes) {
    return {};
  }

  // Check stem length: lemma minus final い
  size_t stem_bytes = next.lemma.size() - core::kJapaneseCharBytes;
  size_t stem_chars = stem_bytes / core::kJapaneseCharBytes;

  // Only penalize short stems (≤2 chars like いしい, but not おいしい)
  if (stem_chars > 2) {
    return {};
  }

  // Check if lemma is pure hiragana
  for (size_t i = 0; i < next.lemma.size(); i += core::kJapaneseCharBytes) {
    if (i + 2 >= next.lemma.size()) break;
    auto b0 = static_cast<unsigned char>(next.lemma[i]);
    auto b1 = static_cast<unsigned char>(next.lemma[i + 1]);
    // Hiragana range: E3 81 80 - E3 82 9F
    if (b0 != 0xE3 || (b1 != 0x81 && b1 != 0x82)) {
      return {};  // Not pure hiragana
    }
  }

  return {ConnectionPattern::PrefixToShortStemHiraganaAdj,
          scorer::kPenaltyShortStemHiraganaAdj,
          "prefix to short-stem hiragana adj"};
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

// Rule 14: に (PARTICLE) + よる (NOUN, lemma 夜) split penalty
// Discourages parsing に + よる(夜) when compound particle によると is available
// E.g., 報告によると should use によると compound particle
ConnectionRuleResult checkYoruNightAfterNi(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Particle ||
      next.pos != core::PartOfSpeech::Noun) {
    return {};
  }

  if (prev.surface != "に") {
    return {};
  }

  // Check if next is よる with lemma 夜 (night)
  if (next.surface != "よる" || next.lemma != "夜") {
    return {};
  }

  return {ConnectionPattern::YoruNightAfterNi, scorer::kPenaltyYoruNightAfterNi,
          "yoru(night) after ni (prefer compound particle)"};
}

// Check for formal noun followed by kanji (should be compound word)
// e.g., 所 + 在する → should be 所在する
ConnectionRuleResult checkFormalNounBeforeKanji(const core::LatticeEdge& prev,
                                                const core::LatticeEdge& next) {
  // Check if prev is a formal noun (single kanji)
  // Note: Also check centralized formal noun set for edges without the flag
  bool is_formal =
      prev.isFormalNoun() ||
      (prev.pos == core::PartOfSpeech::Noun &&
       prev.surface.size() == core::kJapaneseCharBytes &&
       normalize::isFormalNounSurface(prev.surface));

  if (!is_formal) {
    return {};
  }

  // Check if next starts with kanji (0xE4-0xE9 range in UTF-8)
  if (next.surface.empty()) {
    return {};
  }
  auto first_byte = static_cast<unsigned char>(next.surface[0]);
  if (first_byte < 0xE4 || first_byte > 0xE9) {
    return {};
  }

  // Penalty for formal noun + kanji pattern
  return {ConnectionPattern::FormalNounBeforeKanji, 3.0F,
          "formal noun before kanji (should be compound)"};
}

// Rule: Same particle repeated (も + も, の + の, etc.)
// This is grammatically rare - usually different particles or NOUN between them
// Exception: と + と can occur in quotation patterns
ConnectionRuleResult checkSameParticleRepeated(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Particle ||
      next.pos != core::PartOfSpeech::Particle) {
    return {};
  }

  // Same single-character particle repeated
  if (prev.surface.size() == core::kJapaneseCharBytes &&
      next.surface.size() == core::kJapaneseCharBytes &&
      prev.surface == next.surface) {
    // Exception: と + と in quotation (〜と言ったとか)
    if (prev.surface == "と") {
      return {};
    }
    return {ConnectionPattern::SameParticleRepeated, 2.0F,
            "same particle repeated"};
  }

  return {};
}

// Rule: Hiragana noun starting with particle character after NOUN
// Japanese grammar: NOUN is very likely to be followed by PARTICLE
// If a hiragana noun starts with common particle (も、の、が、を、に、は、で、と、へ、か),
// prefer splitting off the particle.
// Example: すもも(NOUN) + もも(NOUN) should prefer すもも + も(PARTICLE) + もも
ConnectionRuleResult checkHiraganaNounStartsWithParticle(
    const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // Previous must be NOUN (content word likely followed by particle)
  if (prev.pos != core::PartOfSpeech::Noun) {
    return {};
  }

  // Next must be NOUN (we're penalizing noun-noun when particle split is better)
  if (next.pos != core::PartOfSpeech::Noun) {
    return {};
  }

  // Next surface must be pure hiragana (check first byte range: 0xE3 8X XX)
  if (next.surface.size() < core::kJapaneseCharBytes) {
    return {};
  }
  auto b0 = static_cast<unsigned char>(next.surface[0]);
  auto b1 = static_cast<unsigned char>(next.surface[1]);
  // Hiragana range in UTF-8: E3 81 80 - E3 82 9F
  if (b0 != 0xE3 || (b1 != 0x81 && b1 != 0x82)) {
    return {};
  }

  // Check if first character is a common particle
  // も、の、が、を、に、は、で、と、へ、か、や
  std::string_view first3 = next.surface.substr(0, core::kJapaneseCharBytes);
  if (first3 == "も" || first3 == "の" || first3 == "が" ||
      first3 == "を" || first3 == "に" || first3 == "は" ||
      first3 == "で" || first3 == "と" || first3 == "へ" ||
      first3 == "か" || first3 == "や") {
    // Penalty to prefer NOUN + PARTICLE over NOUN + NOUN(starts with particle)
    return {ConnectionPattern::HiraganaNounStartsWithParticle, 1.5F,
            "hiragana noun starts with particle char"};
  }

  return {};
}

// Rule: SYMBOL + SUFFIX penalty
// After punctuation (、。etc.), a word is unlikely to be a suffix
// E.g., 、家 should be 家(NOUN), not 家(SUFFIX meaning "-ist" as in 作家)
ConnectionRuleResult checkSuffixAfterSymbol(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Symbol ||
      next.pos != core::PartOfSpeech::Suffix) {
    return {};
  }

  return {ConnectionPattern::SuffixAfterSymbol,
          scorer::kPenaltySuffixAfterSymbol, "suffix after punctuation"};
}

// Check for PARTICLE + hiragana OTHER pattern
// Hiragana OTHER after particle is often a split error in reading contexts
// e.g., と + うきょう in とうきょう should not be split
ConnectionRuleResult checkParticleBeforeHiraganaOther(
    const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  if (prev.pos != core::PartOfSpeech::Particle ||
      next.pos != core::PartOfSpeech::Other) {
    return {};
  }

  // Check if it starts with hiragana
  if (next.surface.size() < core::kJapaneseCharBytes) {
    return {};
  }
  auto b0 = static_cast<unsigned char>(next.surface[0]);
  auto b1 = static_cast<unsigned char>(next.surface[1]);
  if (b0 != 0xE3 || (b1 != 0x81 && b1 != 0x82)) {
    return {};
  }

  // Penalty based on length: single char = 2.5, multi-char = 1.0
  float penalty = (next.surface.size() == core::kJapaneseCharBytes) ? 2.5F : 1.0F;
  return {ConnectionPattern::ParticleBeforeAux, penalty,
          "hiragana other after particle (likely split)"};
}

}  // namespace connection_rules
}  // namespace suzume::analysis
