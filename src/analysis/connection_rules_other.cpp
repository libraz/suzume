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
      next.lemma == scorer::kLemmaNaru ||
      (next.surface.size() >= core::kTwoJapaneseCharBytes &&
       next.surface.substr(0, core::kTwoJapaneseCharBytes) == "なり");

  if (!is_naru) {
    return {};
  }

  // Bonus (negative value)
  return {ConnectionPattern::AdjKuNaru, -opts.bonus_adj_ku_naru,
          "adj-ku + naru pattern"};
}

// Rule: PREFIX → pure hiragana adjective (unknown)
// E.g., お + いしい is likely misanalysis (should be おいしい)
// E.g., お + こがましい is likely misanalysis (should be おこがましい)
// Valid hiragana adjectives (すごい, うまい, やばい) are in dictionary
// Honorific prefix お typically goes with kanji adjectives (お美しい, お高い)
// Unknown pure hiragana adjectives after PREFIX are penalized
ConnectionRuleResult checkPrefixToHiraganaAdj(const core::LatticeEdge& prev,
                                              const core::LatticeEdge& next,
                                              const ConnectionOptions& opts) {
  if (!isPrefixToAdj(prev, next)) return {};

  // Next must be unknown adjective (dictionary adjectives are valid)
  if (next.fromDictionary()) return {};

  // Check if lemma is at least valid length (2 chars for い-adj)
  if (next.lemma.size() < core::kTwoJapaneseCharBytes) {
    return {};
  }

  // Check if lemma is pure hiragana
  // Kanji-containing adjectives after PREFIX are valid (お美しい, お高い)
  if (!grammar::isPureHiragana(next.lemma)) return {};

  return {ConnectionPattern::PrefixToHiraganaAdj,
          opts.penalty_prefix_hiragana_adj, "prefix to hiragana adj"};
}

// Note: PARTICLE → ADJ penalty was removed because:
// 1. Particles like が, を, に before adjectives are grammatically valid
// 2. The penalty for は + なはだしい at start causes worse fragmentation
// 3. The proper fix is in adjective candidate generation (C2 task) to not break
//    at particle characters within hiragana adjectives like はなはだしい
// The PREFIX → ADJ rule (checkPrefixToHiraganaAdj) is kept since お/ご prefixes
// before unknown hiragana adjectives are almost always misanalysis.
ConnectionRuleResult checkParticleBeforeHiraganaAdj(
    const core::LatticeEdge& /*prev*/, const core::LatticeEdge& /*next*/,
    const ConnectionOptions& /*opts*/) {
  return {};  // Disabled - see comment above
}

// Rule 8: だ/です + character speech suffix split penalty
ConnectionRuleResult checkCharacterSpeechSplit(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isAuxToAux(prev, next)) return {};

  if (prev.surface != scorer::kCopulaDa && prev.surface != scorer::kCopulaDesu) {
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

  if (prev.surface != scorer::kParticleNi) return {};

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

  // Exception: formal noun + adjective is a valid grammatical construction
  // e.g., こと無く (without doing), 時妙な (strange at that time)
  // Formal nouns can naturally precede adjectives when the adjective is a
  // separate word, not part of a compound
  if (next.pos == core::PartOfSpeech::Adjective) {
    return {};  // No penalty for formal noun + ADJ pattern
  }

  // Exception: formal noun + pronoun (interrogatives)
  // e.g., 時何だか (when something/what), 所誰 (where someone)
  // Interrogative pronouns (何, 誰) are standalone words, not compound parts
  if (next.pos == core::PartOfSpeech::Pronoun) {
    return {};  // No penalty for formal noun + PRON pattern
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
    if (prev.surface == scorer::kParticleTo) {
      return {};
    }
    return {ConnectionPattern::SameParticleRepeated,
            opts.penalty_same_particle_repeated, "same particle repeated"};
  }

  return {};
}

// Rule: Suspicious particle sequence (different particles in unlikely pattern)
// This catches cases where a hiragana noun was split into particles
// E.g., は + し + が likely means はし (noun) was split incorrectly
//
// Suspicious patterns:
// - し after short particle: し is listing particle, should follow predicates
// - が/を after short particle: case particles usually follow nouns, not particles
//
// Valid compound patterns (exceptions):
// - には, では, からは, へは - case + topic
// - にも, でも, からも, へも - case + も
// - とは - quotative + topic
// - からの, への, での - case + の
ConnectionRuleResult checkSuspiciousParticleSequence(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& opts) {
  if (!isParticleToParticle(prev, next)) return {};

  // Both must be single-character particles
  if (prev.surface.size() != core::kJapaneseCharBytes ||
      next.surface.size() != core::kJapaneseCharBytes) {
    return {};
  }

  // Check for valid compound particle patterns
  std::string_view p = prev.surface;
  std::string_view n = next.surface;

  // Valid compounds: case + topic marker (は/も/の)
  // には, では, とは, へは, からは etc.
  if ((p == scorer::kParticleNi || p == scorer::kFormDe ||
       p == scorer::kParticleTo || p == scorer::kParticleHe) &&
      (n == scorer::kParticleHa || n == scorer::kParticleMo ||
       n == scorer::kParticleNo)) {
    return {};  // Valid compound
  }

  // し/な after short particle is suspicious
  // し as listing particle (し接続助詞) should follow predicates, not particles
  // な as prohibition/emphasis particle (するな, 来たな) should follow verbs
  if (n == scorer::kSuffixShi || n == scorer::kParticleNa) {
    return {ConnectionPattern::SuspiciousParticleSequence,
            opts.penalty_suspicious_particle_sequence,
            "particle after short particle (likely split)"};
  }

  // が/を after certain particles is suspicious
  // These case particles usually follow nouns, not other particles
  // Exceptions: のが, ので are valid
  if ((n == scorer::kParticleGa || n == scorer::kParticleWo) &&
      p != scorer::kParticleNo) {
    return {ConnectionPattern::SuspiciousParticleSequence,
            opts.penalty_suspicious_particle_sequence,
            "case particle after short particle (likely split)"};
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
  if (first3 == scorer::kParticleMo || first3 == scorer::kParticleNo ||
      first3 == scorer::kParticleGa || first3 == scorer::kParticleWo ||
      first3 == scorer::kParticleNi || first3 == scorer::kParticleHa ||
      first3 == scorer::kFormDe || first3 == scorer::kParticleTo ||
      first3 == scorer::kParticleHe || first3 == scorer::kParticleKa ||
      first3 == scorer::kParticleYa) {
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

// Rule: PARTICLE + SUFFIX penalty
// After particles, SUFFIX is usually wrong - NOUN is expected
// E.g., 大切な人 should be 人(NOUN), not 人(SUFFIX)
// E.g., いつもの店 should be 店(NOUN), not 店(SUFFIX)
// SUFFIX is for counters like 三人 where 人 follows a number, not particle
ConnectionRuleResult checkSuffixAfterNaParticle(const core::LatticeEdge& prev,
                                                const core::LatticeEdge& next,
                                                const ConnectionOptions& /*opts*/) {
  if (prev.pos != core::PartOfSpeech::Particle) return {};
  if (next.pos != core::PartOfSpeech::Suffix) return {};

  // Moderate penalty - particle should typically be followed by NOUN, not SUFFIX
  return {ConnectionPattern::SuffixAfterSymbol,
          scorer::scale::kModerate, "suffix after particle (should be noun)"};
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

  // Only penalize if the verb is unknown (not from dictionary or recognized by
  // inflection system) Dictionary verbs after particles are often valid (e.g.,
  // が + 見える) Also exempt verbs recognized by inflection system (have lemma)
  // e.g., たべる is recognized as ichidan verb with lemma "たべる"
  if (next.fromDictionary()) return {};
  if (!next.lemma.empty()) return {};  // Recognized by inflection system

  // Only apply to pure hiragana verbs
  if (!grammar::startsWithHiragana(next.surface)) return {};

  // Don't penalize te-forms - they are valid verb forms after particles
  // e.g., に + つけて, を + 食べて, が + 見えて
  // Exception: Very short te-forms (2 chars like けて) are often erroneous splits
  // e.g., が + けて in 心がけて should be single verb 心がける
  if (next.surface.size() >= core::kJapaneseCharBytes) {
    std::string_view last = next.surface.substr(next.surface.size() - core::kJapaneseCharBytes);
    if (last == scorer::kFormTe || last == scorer::kFormDe) {
      // Short te-forms (2 chars) get moderate penalty - often erroneous splits
      // e.g., けて, して (from 1-char stem verbs) are rare and usually wrong
      if (next.surface.size() <= core::kTwoJapaneseCharBytes) {
        return {ConnectionPattern::ParticleBeforeAux,
                scorer::scale::kModerate,
                "single-char particle before short te-form (likely split)"};
      }
      return {};  // Valid te-form (3+ chars), no penalty
    }
  }

  // Don't penalize ている/でいる progressive forms
  if (next.surface.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view last3 = next.surface.substr(next.surface.size() - core::kThreeJapaneseCharBytes);
    if (last3 == scorer::kPatternTeIru || last3 == scorer::kPatternDeIru) {
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
  if (next.pos != core::PartOfSpeech::Particle || next.surface != scorer::kSuffixShi) {
    return {};
  }

  // Check previous POS
  switch (prev.pos) {
    case core::PartOfSpeech::Adjective:
      // ADJ + し: valid (上手いし, 高いし)
      // Must end with い for i-adjective shuushikei
      if (prev.surface.size() >= core::kJapaneseCharBytes &&
          prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes) ==
              "い") {  // i-adjective ending
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

// Rule: Kanji NOUN + な(PARTICLE) penalty
// When a kanji compound noun is followed by な particle, it's almost always
// a na-adjective pattern (獰猛な, 静かな, 便利な)
// The な particle (prohibition/emphasis) is rare after nouns.
// Penalty shifts preference to NOUN + AUX(な) or registered ADJ patterns.
ConnectionRuleResult checkNaParticleAfterKanjiNoun(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& opts) {
  // Check if prev is NOUN
  if (prev.pos != core::PartOfSpeech::Noun) return {};

  // Check if next is PARTICLE with surface な
  if (next.pos != core::PartOfSpeech::Particle ||
      next.surface != scorer::kParticleNa) {
    return {};
  }

  // Check if prev surface is kanji (potential na-adjective stem)
  // At least 2 characters for typical na-adjective stems
  if (prev.surface.size() < core::kTwoJapaneseCharBytes) return {};

  // Check if prev starts with kanji (0xE4-0xE9 range in UTF-8)
  auto first_byte = static_cast<unsigned char>(prev.surface[0]);
  if (first_byte < 0xE4 || first_byte > 0xE9) return {};

  // Apply penalty to shift preference to na-adjective pattern
  return {ConnectionPattern::NaParticleAfterKanjiNoun,
          opts.penalty_na_particle_after_kanji_noun,
          "kanji noun + na particle (likely na-adjective)"};
}

// Rule: VERB/ADJ/AUX + くらい(ADJ) penalty
// When くらい follows a predicate, it's usually the particle (extent/degree),
// not the adjective 暗い (dark). E.g., いられぬくらいだ → くらい is PARTICLE
ConnectionRuleResult checkKuraiAdjectiveAfterPredicate(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& /* opts */) {
  // Only apply to くらい as ADJ
  if (next.pos != core::PartOfSpeech::Adjective) return {};
  if (next.surface != "くらい" && next.lemma != "暗い") return {};

  // Check if prev is a predicate (VERB, ADJ, AUX)
  bool is_predicate = (prev.pos == core::PartOfSpeech::Verb ||
                       prev.pos == core::PartOfSpeech::Adjective ||
                       prev.pos == core::PartOfSpeech::Auxiliary);
  if (!is_predicate) return {};

  // Strong penalty to prefer PARTICLE interpretation
  return {ConnectionPattern::KuraiAdjAfterPredicate,
          scorer::scale::kStrong,
          "kurai adjective after predicate (prefer particle)"};
}

}  // namespace connection_rules
}  // namespace suzume::analysis
