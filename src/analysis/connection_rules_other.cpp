#include "analysis/connection_rules_internal.h"

namespace suzume::analysis {
namespace connection_rules {

// =============================================================================
// Other Connection Rules
// =============================================================================

// Rule 9: Adjective く + なる (bonus)
ConnectionRuleResult checkAdjKuNaru(const core::LatticeEdge& prev,
                                    const core::LatticeEdge& next,
                                    const ConnectionOptions& opts) {
  if (!isAdjToVerb(prev, next)) return {};

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
  return {ConnectionPattern::AdjKuNaru, -opts.bonus_adj_ku_naru,
          "adj-ku + naru pattern"};
}

// Rule: PREFIX → short-stem pure hiragana adjective
// E.g., お + いしい is likely misanalysis (should be おいしい)
// Valid short hiragana adjectives (すごい, うまい, やばい) are in dictionary
// Unknown short-stem (≤2 chars) hiragana adjectives after PREFIX are penalized
ConnectionRuleResult checkPrefixToShortStemHiraganaAdj(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& opts) {
  if (!isPrefixToAdj(prev, next)) return {};

  // Next must be unknown adjective (dictionary adjectives are valid)
  if (next.fromDictionary()) return {};

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
  if (!grammar::isPureHiragana(next.lemma)) return {};

  return {ConnectionPattern::PrefixToShortStemHiraganaAdj,
          opts.penalty_prefix_short_stem_hiragana_adj,
          "prefix to short-stem hiragana adj"};
}

// Rule 8: だ/です + character speech suffix split penalty
ConnectionRuleResult checkCharacterSpeechSplit(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isAuxToAux(prev, next)) return {};

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
          opts.penalty_character_speech_split, "split character speech pattern"};
}

// Rule 14: に (PARTICLE) + よる (NOUN, lemma 夜) split penalty
// Discourages parsing に + よる(夜) when compound particle によると is available
// E.g., 報告によると should use によると compound particle
ConnectionRuleResult checkYoruNightAfterNi(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (!isParticleToNoun(prev, next)) return {};

  if (prev.surface != "に") return {};

  // Check if next is よる with lemma 夜 (night)
  if (next.surface != "よる" || next.lemma != "夜") {
    return {};
  }

  return {ConnectionPattern::YoruNightAfterNi, opts.penalty_yoru_night_after_ni,
          "yoru(night) after ni (prefer compound particle)"};
}

// Check for formal noun followed by kanji (should be compound word)
// e.g., 所 + 在する → should be 所在する
ConnectionRuleResult checkFormalNounBeforeKanji(const core::LatticeEdge& prev,
                                                const core::LatticeEdge& next,
                                                const ConnectionOptions& opts) {
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
  return {ConnectionPattern::FormalNounBeforeKanji,
          opts.penalty_formal_noun_before_kanji,
          "formal noun before kanji (should be compound)"};
}

// Rule: Same particle repeated (も + も, の + の, etc.)
// This is grammatically rare - usually different particles or NOUN between them
// Exception: と + と can occur in quotation patterns
ConnectionRuleResult checkSameParticleRepeated(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isParticleToParticle(prev, next)) return {};

  // Same single-character particle repeated
  if (prev.surface.size() == core::kJapaneseCharBytes &&
      next.surface.size() == core::kJapaneseCharBytes &&
      prev.surface == next.surface) {
    // Exception: と + と in quotation (〜と言ったとか)
    if (prev.surface == "と") {
      return {};
    }
    return {ConnectionPattern::SameParticleRepeated,
            opts.penalty_same_particle_repeated, "same particle repeated"};
  }

  return {};
}

// Rule: Hiragana noun starting with particle character after NOUN
// Japanese grammar: NOUN is very likely to be followed by PARTICLE
// If a hiragana noun starts with common particle (も、の、が、を、に、は、で、と、へ、か),
// prefer splitting off the particle.
// Example: すもも(NOUN) + もも(NOUN) should prefer すもも + も(PARTICLE) + もも
ConnectionRuleResult checkHiraganaNounStartsWithParticle(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& opts) {
  if (!isNounToNoun(prev, next)) return {};

  // Next surface must start with hiragana
  if (!grammar::startsWithHiragana(next.surface)) return {};

  // Check if first character is a common particle
  // も、の、が、を、に、は、で、と、へ、か、や
  std::string_view first3 = next.surface.substr(0, core::kJapaneseCharBytes);
  if (first3 == "も" || first3 == "の" || first3 == "が" ||
      first3 == "を" || first3 == "に" || first3 == "は" ||
      first3 == "で" || first3 == "と" || first3 == "へ" ||
      first3 == "か" || first3 == "や") {
    // Penalty to prefer NOUN + PARTICLE over NOUN + NOUN(starts with particle)
    return {ConnectionPattern::HiraganaNounStartsWithParticle,
            opts.penalty_hiragana_noun_starts_with_particle,
            "hiragana noun starts with particle char"};
  }

  return {};
}

// Rule: SYMBOL + SUFFIX penalty
// After punctuation (、。etc.), a word is unlikely to be a suffix
// E.g., 、家 should be 家(NOUN), not 家(SUFFIX meaning "-ist" as in 作家)
ConnectionRuleResult checkSuffixAfterSymbol(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& opts) {
  if (!isSymbolToSuffix(prev, next)) return {};

  return {ConnectionPattern::SuffixAfterSymbol,
          opts.penalty_suffix_after_symbol, "suffix after punctuation"};
}

// Check for PARTICLE + hiragana OTHER pattern
// Hiragana OTHER after particle is often a split error in reading contexts
// e.g., と + うきょう in とうきょう should not be split
ConnectionRuleResult checkParticleBeforeHiraganaOther(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& opts) {
  if (!isParticleToOther(prev, next)) return {};

  // Check if it starts with hiragana
  if (!grammar::startsWithHiragana(next.surface)) return {};

  // Penalty based on length: single char = 2.5, multi-char = 1.0
  float penalty = (next.surface.size() == core::kJapaneseCharBytes)
                      ? opts.penalty_particle_before_single_hiragana_other
                      : opts.penalty_particle_before_multi_hiragana_other;
  return {ConnectionPattern::ParticleBeforeAux, penalty,
          "hiragana other after particle (likely split)"};
}

// Check for PARTICLE + hiragana VERB pattern
// Short particles followed by unknown hiragana verbs are often erroneous splits
// e.g., し + まる in しまる should be single VERB しまる
// e.g., た + よる in たよる should be single VERB たよる
// Exception: Te-forms (ending with て/で) are valid verb forms after particles
// e.g., に + つけて (te-form of つける) is valid
ConnectionRuleResult checkParticleBeforeHiraganaVerb(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& opts) {
  if (!isParticleToVerb(prev, next)) return {};

  // Only apply to single-char particles (most prone to false splits)
  if (prev.surface.size() > core::kJapaneseCharBytes) return {};

  // Only penalize if the verb is unknown (not from dictionary)
  // Dictionary verbs after particles are often valid (e.g., が + 見える)
  if (next.fromDictionary()) return {};

  // Only apply to pure hiragana verbs
  if (!grammar::startsWithHiragana(next.surface)) return {};

  // Don't penalize te-forms - they are valid verb forms after particles
  // e.g., に + つけて, を + 食べて, が + 見えて
  if (next.surface.size() >= core::kJapaneseCharBytes) {
    std::string_view last = next.surface.substr(next.surface.size() - core::kJapaneseCharBytes);
    if (last == "て" || last == "で") {
      return {};  // Valid te-form, no penalty
    }
  }

  // Don't penalize ている/でいる progressive forms
  if (next.surface.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view last3 = next.surface.substr(next.surface.size() - core::kThreeJapaneseCharBytes);
    if (last3 == "ている" || last3 == "でいる") {
      return {};  // Valid progressive form, no penalty
    }
  }

  return {ConnectionPattern::ParticleBeforeAux,
          opts.penalty_particle_before_hiragana_verb,
          "single-char particle before unknown hiragana verb (likely split)"};
}

// Rule: Conjunctive particle し after predicate (ADJ/VERB/AUX)
// Valid: 上手いし, 食べるし, 高いし, 行くし, だし
// Invalid: 本し (noun cannot directly connect to し)
ConnectionRuleResult checkShiParticleConnection(const core::LatticeEdge& prev,
                                                const core::LatticeEdge& next,
                                                const ConnectionOptions& opts) {
  // Only applies to し particle
  if (next.pos != core::PartOfSpeech::Particle || next.surface != "し") {
    return {};
  }

  // Check previous POS
  switch (prev.pos) {
    case core::PartOfSpeech::Adjective:
      // ADJ + し: valid (上手いし, 高いし)
      // Must end with い for i-adjective shuushikei
      if (prev.surface.size() >= core::kJapaneseCharBytes &&
          prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes) ==
              "い") {
        return {ConnectionPattern::ShiParticleAfterPredicate,
                -opts.bonus_shi_after_i_adj, "i-adj + shi particle (valid)"};
      }
      // Na-adj needs だ/な before し, so no bonus for bare na-adj
      return {};

    case core::PartOfSpeech::Verb:
      // VERB + し: valid if shuushikei (終止形)
      // Shuushikei ends with う段 (う、く、す、つ、ぬ、ふ、む、ゆ、る)
      // or る for ichidan verbs
      if (!prev.surface.empty()) {
        return {ConnectionPattern::ShiParticleAfterPredicate,
                -opts.bonus_shi_after_verb, "verb + shi particle (valid)"};
      }
      return {};

    case core::PartOfSpeech::Auxiliary:
      // AUX + し: valid (だし, ないし, たし)
      return {ConnectionPattern::ShiParticleAfterPredicate,
              -opts.bonus_shi_after_aux, "aux + shi particle (valid)"};

    case core::PartOfSpeech::Noun:
      // NOUN + し: invalid (本し - should be 本だし with copula)
      return {ConnectionPattern::ShiParticleAfterNoun,
              opts.penalty_shi_after_noun,
              "noun + shi particle (invalid, needs copula)"};

    default:
      return {};
  }
}

}  // namespace connection_rules
}  // namespace suzume::analysis
