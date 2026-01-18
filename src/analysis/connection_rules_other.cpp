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
  if (!isPosMatch<POS::Adj, POS::Verb>(prev, next)) return {};

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

// Rule: ADJ(く形) → て(PARTICLE) bonus
// MeCab-compatible kute split: 美しくて → 美しく + て, しんどくて → しんどく + て
ConnectionRuleResult checkAdjKuToTeParticle(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& /* opts */) {
  // Check if prev is adjective
  if (prev.pos != core::PartOfSpeech::Adjective) return {};

  // Check if prev ends with く (adverbial form)
  if (!endsWithKuForm(prev.surface)) {
    return {};
  }

  // Check if next is て particle
  if (next.pos != core::PartOfSpeech::Particle) return {};
  if (next.surface != scorer::kFormTe) return {};

  // Strong bonus to prefer this split over unified くて form
  return {ConnectionPattern::AdjKuToTeParticle, -scorer::kBonusAdjKuToTeParticle,
          "adj-ku + te particle (MeCab-compatible kute split)"};
}

// Rule: ADJ(く形) → ない(AUX) bonus
// MeCab-compatible adjective negation split: 高くない → 高く + ない
ConnectionRuleResult checkAdjKuToNai(const core::LatticeEdge& prev,
                                     const core::LatticeEdge& next,
                                     const ConnectionOptions& /* opts */) {
  // Check if prev is adjective
  if (prev.pos != core::PartOfSpeech::Adjective) return {};

  // Check if prev ends with く (adverbial form)
  if (!endsWithKuForm(prev.surface)) {
    return {};
  }

  // Check if next is ない auxiliary
  if (next.pos != core::PartOfSpeech::Auxiliary) return {};
  if (next.lemma != "ない") return {};

  // Strong bonus to prefer split over unified くない form
  return {ConnectionPattern::AdjKuToNai, -scorer::kBonusAdjKuToNai,
          "adj-ku + nai (MeCab-compatible adjective negation split)"};
}

// Rule: I-ADJ(基本形) → です(AUX) bonus
// MeCab-compatible polite form: 美味しいです → 美味しい + です
// Strong bonus needed to beat で(AUX)+す(VERB) split path
ConnectionRuleResult checkIAdjToDesu(const core::LatticeEdge& prev,
                                     const core::LatticeEdge& next,
                                     const ConnectionOptions& /* opts */) {
  // Check if prev is adjective
  if (prev.pos != core::PartOfSpeech::Adjective) return {};

  // Check if prev ends with い (basic form) or た (past form: よかった)
  // Note: く form is handled by checkAdjKuToTeParticle and checkAdjKuToNai
  if (prev.surface.size() < core::kJapaneseCharBytes) return {};
  std::string_view last_char = prev.surface.substr(
      prev.surface.size() - core::kJapaneseCharBytes);
  if (last_char != "い" && last_char != "た") return {};

  // Check if next is です (polite copula)
  if (next.pos != core::PartOfSpeech::Auxiliary) return {};
  if (next.surface != "です") return {};

  // Strong bonus to prefer this split over で+す path
  return {ConnectionPattern::IAdjToDesu, -scorer::kBonusIAdjToDesu,
          "i-adj + desu (MeCab-compatible polite form)"};
}

// Rule: ADJ stem (ガル接続) → すぎる/すぎ/がる (VERB) bonus
// MeCab-compatible splitting for i-adjective stems followed by すぎる/がる
// MeCab output for 高すぎる:
//   高    形容詞,自立,*,*,形容詞・アウオ段,ガル接続,高い,タカ,タカ
//   すぎる 動詞,非自立,*,*,一段,基本形,すぎる,スギル,スギル
// Strong bonus needed to beat single-token 高すぎる(ADJ) path
ConnectionRuleResult checkAdjStemToSugiruVerb(const core::LatticeEdge& prev,
                                              const core::LatticeEdge& next,
                                              const ConnectionOptions& /* opts */) {
  // Check if prev is an adjective
  if (prev.pos != core::PartOfSpeech::Adjective) return {};

  // Check if next is a verb starting with すぎ or がる
  if (next.pos != core::PartOfSpeech::Verb) return {};
  // Check if next verb starts with すぎ/がる patterns
  if (!utf8::startsWith(next.surface, "すぎ") &&
      !utf8::startsWith(next.surface, "がる") &&
      !utf8::startsWith(next.surface, "がり") &&
      !utf8::startsWith(next.surface, "がっ")) {
    return {};
  }

  // Check if prev is an adjective stem (kanji only, no い ending)
  // Adjective stems like 高, 尊, 寒 are generated by generateAdjectiveStemCandidates
  // They have lemma like "高い" but surface is just "高"
  if (prev.surface.size() < core::kJapaneseCharBytes) return {};
  std::string_view last_char = prev.surface.substr(
      prev.surface.size() - core::kJapaneseCharBytes);
  // If it ends with い, it's not a stem form
  if (last_char == "い") return {};

  // Additional check: lemma should end with い (base form of i-adjective)
  if (prev.lemma.size() < core::kJapaneseCharBytes) return {};
  std::string_view lemma_last = prev.lemma.substr(
      prev.lemma.size() - core::kJapaneseCharBytes);
  if (lemma_last != "い") return {};

  // Strong bonus to prefer this split over single-token adjective
  return {ConnectionPattern::AdjStemToSugiruVerb, -1.5F,
          "adj-stem + sugiru/garu (MeCab-compatible garu-connection split)"};
}

// Appearance auxiliary そう (様態助動詞) splitting for i-adjective stems
// MeCab outputs 名詞,接尾,助動詞語幹 but semantically it's auxiliary
// Strong bonus needed to beat ADV そう path
ConnectionRuleResult checkAdjStemToSouAux(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next,
                                          const ConnectionOptions& /* opts */) {
  // Check if prev is an adjective
  if (prev.pos != core::PartOfSpeech::Adjective) return {};

  // Check if next is そう as Auxiliary (appearance suffix)
  if (next.pos != core::PartOfSpeech::Auxiliary) return {};
  if (next.surface != "そう") return {};

  // Check if prev is an adjective stem (no い ending)
  // Adjective stems like 楽し, 悲し, 美味し are generated by generateAdjectiveStemCandidates
  // They have lemma like "楽しい" but surface is just "楽し"
  if (prev.surface.size() < core::kJapaneseCharBytes) return {};
  std::string_view last_char = prev.surface.substr(
      prev.surface.size() - core::kJapaneseCharBytes);
  // If it ends with い, it's not a stem form
  if (last_char == "い") return {};

  // Additional check: lemma should end with い (base form of i-adjective)
  if (prev.lemma.size() < core::kJapaneseCharBytes) return {};
  std::string_view lemma_last = prev.lemma.substr(
      prev.lemma.size() - core::kJapaneseCharBytes);
  if (lemma_last != "い") return {};

  // Strong bonus to prefer ADJ-stem + そう(AUX) over single-token dictionary path
  // Need large bonus because dictionary conjugated forms (美味しそう) have very low cost
  // and we want MeCab-compatible split (美味し + そう)
  return {ConnectionPattern::AdjStemToSouAux, -4.0F,
          "adj-stem + sou (appearance auxiliary)"};
}

// Appearance auxiliary そう (様態助動詞) splitting for verb renyokei
// MeCab outputs 名詞,接尾,助動詞語幹 but semantically it's auxiliary
// Strong bonus needed to beat ADV そう path
ConnectionRuleResult checkVerbRenyokeiToSouAux(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& /* opts */) {
  // Check if prev is a verb
  if (prev.pos != core::PartOfSpeech::Verb) return {};

  // Check if next is そう as Auxiliary (appearance suffix)
  if (next.pos != core::PartOfSpeech::Auxiliary) return {};
  if (next.surface != "そう") return {};

  // Check if prev is a verb renyokei (ends with i-row hiragana or renyokei pattern)
  // Renyokei endings: い, き, し, ち, に, び, み, り (i-row) for Godan
  // Or e-row (え, け, せ, て, ね, へ, め, れ) for Ichidan
  std::string_view last_char = utf8::lastChar(prev.surface);
  if (last_char.empty()) return {};

  // Check for renyokei endings (i-row and e-row)
  if (!utf8::equalsAny(last_char, {
          "い", "き", "し", "ち", "に", "び", "み", "り",  // i-row (Godan)
          "え", "け", "せ", "て", "ね", "べ", "め", "れ"   // e-row (Ichidan)
      })) {
    return {};
  }

  // Strong bonus to prefer VERB-renyokei + そう(AUX) over VERB + そう(ADV)
  // AUX entry cost is 2.5F, so we need at least -2.0F bonus to beat ADV path
  return {ConnectionPattern::VerbRenyokeiToSouAux, -2.5F,
          "verb-renyokei + sou (appearance auxiliary)"};
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
  if (!isPosMatch<POS::Prefix, POS::Adj>(prev, next)) return {};

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

// Rule 8: だ/です + character speech suffix split penalty
ConnectionRuleResult checkCharacterSpeechSplit(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isPosMatch<POS::Aux, POS::Aux>(prev, next)) return {};

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
  if (!isPosMatch<POS::Particle, POS::Noun>(prev, next)) return {};

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

  // Check if next starts with kanji
  if (!startsWithKanji(next.surface)) {
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
  if (!isPosMatch<POS::Particle, POS::Particle>(prev, next)) return {};

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
  if (!isPosMatch<POS::Particle, POS::Particle>(prev, next)) return {};

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

// Rule: 終助詞 + 終助詞 (sentence-final particle sequence) bonus
// E.g., よ + ね, よ + わ are extremely common Japanese sentence endings
// This rule prevents ね from being parsed as verb (ねる) after よ
ConnectionRuleResult checkSentenceFinalParticleSeq(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& /* opts */) {
  if (!isPosMatch<POS::Particle, POS::Particle>(prev, next)) return {};

  // Both must be single-character particles
  if (prev.surface.size() != core::kJapaneseCharBytes ||
      next.surface.size() != core::kJapaneseCharBytes) {
    return {};
  }

  std::string_view p = prev.surface;
  std::string_view n = next.surface;

  // Check for sentence-final particle sequences
  // よ + ね/わ, ね + え patterns
  bool prev_is_final = (p == scorer::kParticleYo || p == scorer::kParticleNe);
  bool next_is_final = (n == scorer::kParticleNe || n == scorer::kParticleWa);

  if (prev_is_final && next_is_final) {
    return {ConnectionPattern::SentenceFinalParticleSeq,
            -scorer::kBonusSentenceFinalParticleSeq,
            "sentence-final particle sequence (よね, よわ, etc.)"};
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
  if (!isPosMatch<POS::Noun, POS::Noun>(prev, next)) return {};

  // Next surface must start with hiragana
  if (!grammar::startsWithHiragana(next.surface)) return {};

  // Exception: known nouns that legitimately start with particle characters
  // のち (後): temporal noun "after/later", common in weather forecasts
  // E.g., 晴れのち曇り = sunny, later cloudy
  if (next.surface == "のち" || next.surface == "のちほど") {
    return {};
  }

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
  if (!isPosMatch<POS::Symbol, POS::Suffix>(prev, next)) return {};

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
  if (!isPosMatch<POS::Particle, POS::Other>(prev, next)) return {};

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
  if (!isPosMatch<POS::Particle, POS::Verb>(prev, next)) return {};

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
  if (utf8::endsWith(next.surface, scorer::kFormTe) ||
      utf8::endsWith(next.surface, scorer::kFormDe)) {
    // Short te-forms (2 chars) get moderate penalty - often erroneous splits
    // e.g., けて, して (from 1-char stem verbs) are rare and usually wrong
    if (next.surface.size() <= core::kTwoJapaneseCharBytes) {
      return {ConnectionPattern::ParticleBeforeAux,
              scorer::scale::kModerate,
              "single-char particle before short te-form (likely split)"};
    }
    return {};  // Valid te-form (3+ chars), no penalty
  }

  // Don't penalize ている/でいる progressive forms
  if (utf8::endsWith(next.surface, scorer::kPatternTeIru) ||
      utf8::endsWith(next.surface, scorer::kPatternDeIru)) {
    return {};  // Valid progressive form, no penalty
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
      if (utf8::endsWith(prev.surface, "い")) {
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

  // Check if prev starts with kanji
  if (!startsWithKanji(prev.surface)) return {};

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

// Rule: に(PARTICLE) + いる/いた(VERB/AUX) bonus
// Session 31 removed に from isNeverVerbStemAtStart to support verbs like
// にげる (逃げる), にる (煮る), にぎる (握る). However, this causes
// "にいた" to be incorrectly analyzed as verb にぐ past form.
// This rule gives bonus to the correct pattern: に(PARTICLE) + いた(VERB)
// E.g., 家にいた → 家 + に + いた (not 家 + にいた)
// IMPORTANT: Exclude っ-onbin forms (いっ, いって, いった) which are from いく not いる
ConnectionRuleResult checkParticleNiToIruVerb(const core::LatticeEdge& prev,
                                              const core::LatticeEdge& next,
                                              const ConnectionOptions& opts) {
  // Check if prev is PARTICLE with surface に
  if (prev.pos != core::PartOfSpeech::Particle) return {};
  if (prev.surface != scorer::kParticleNi) return {};

  // Check if next is VERB or AUX with lemma いる
  if (next.pos != core::PartOfSpeech::Verb &&
      next.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }

  // Check if lemma is いる (progressive auxiliary or existential verb)
  if (next.lemma != scorer::kLemmaIru) return {};

  // Exclude っ-onbin forms from いく (行く):
  // いっ, いった, いって are godan-ka onbin forms, not いる forms
  // This prevents "旅行にいった" from being misparsed
  if (next.surface.size() >= core::kTwoJapaneseCharBytes) {
    std::string_view prefix = next.surface.substr(0, core::kTwoJapaneseCharBytes);
    if (prefix == "いっ") {
      return {};  // Don't give bonus for いっ* forms (from いく)
    }
  }

  // Strong bonus (negative value) to prefer this pattern over にいた(VERB)
  return {ConnectionPattern::ParticleNiToIruVerb,
          -opts.bonus_ni_particle_to_iru_verb,
          "ni particle + iru verb (existential/progressive pattern)"};
}

// Rule: ADV(quotative) + いっ(VERB, lemma=いう) bonus
// MeCab: そういって → そう + いっ(lemma=いう) + て
// Quotative adverbs (そう, こう, ああ, どう) + いっ should use いう (to say)
// NOT いく (to go) or いる (to be)
ConnectionRuleResult checkQuotativeAdvToIu(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& /* opts */) {
  // Check if prev is ADV
  if (prev.pos != core::PartOfSpeech::Adverb) return {};

  // Check if prev is a quotative adverb (そう, こう, ああ, どう)
  bool is_quotative = (prev.surface == "そう" || prev.surface == "こう" ||
                       prev.surface == "ああ" || prev.surface == "どう");
  if (!is_quotative) return {};

  // Check if next is VERB with surface いっ and lemma いう
  if (next.pos != core::PartOfSpeech::Verb) return {};
  if (next.surface != "いっ") return {};
  if (next.lemma != "いう") return {};

  // Strong bonus (negative value) to prefer いう over いく after quotative ADV
  return {ConnectionPattern::QuotativeAdvToIu, -3.0F,
          "quotative adv + iu verb (to say)"};
}

// Rule: PARTICLE(に) + いって(VERB, lemma=いく) bonus
// MeCab: 旅行にいった → 旅行 + に + いっ(lemma=いく) + た
// After に particle, いっ should prefer lemma=いく (to go), not いう (to say)
ConnectionRuleResult checkNiParticleToIku(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next,
                                          const ConnectionOptions& /* opts */) {
  // Check if prev is PARTICLE に
  if (prev.pos != core::PartOfSpeech::Particle) return {};
  if (prev.surface != scorer::kParticleNi) return {};

  // Check if next is VERB with surface いっ (onbinkei) and lemma いく
  if (next.pos != core::PartOfSpeech::Verb) return {};
  if (next.surface != "いっ") return {};
  if (next.lemma != "いく") return {};

  // Strong bonus to prefer いく after に particle
  return {ConnectionPattern::NiParticleToIku, -2.0F,
          "ni particle + iku onbinkei (to go)"};
}

}  // namespace connection_rules
}  // namespace suzume::analysis
