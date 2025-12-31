#include "analysis/connection_rules_internal.h"

namespace suzume::analysis {
namespace connection_rules {

// =============================================================================
// Helper Function: Check if verb is an auxiliary verb pattern (補助動詞)
// =============================================================================

bool isAuxiliaryVerbPattern(std::string_view surface, std::string_view lemma) {
  // Check lemma for auxiliary verb patterns
  // いる/おる (progressive/state), しまう (completion), みる (try),
  // おく (preparation), いく/くる (direction), あげる/もらう/くれる (giving)
  if (lemma == "いる" || lemma == "おる" || lemma == "しまう" ||
      lemma == "みる" || lemma == "おく" || lemma == "いく" ||
      lemma == "くる" || lemma == "あげる" || lemma == "もらう" ||
      lemma == "くれる" || lemma == "ある") {
    return true;
  }

  // Check surface for polite forms
  if (surface == "います" || surface == "おります" ||
      surface == "しまいます" || surface == "みます" ||
      surface == "おきます" || surface == "いきます" ||
      surface == "きます" || surface == "あります" ||
      surface == "ございます") {
    return true;
  }

  // Check surface for negative/past forms of auxiliary verbs
  // This handles cases where lemma is empty (unknown word analysis)
  if (surface == "くれない" || surface == "くれなかった" ||
      surface == "あげない" || surface == "あげなかった" ||
      surface == "もらわない" || surface == "もらわなかった" ||
      surface == "しまわない" || surface == "しまわなかった" ||
      surface == "いない" || surface == "いなかった" ||
      surface == "おらない" || surface == "おらなかった") {
    return true;
  }

  return false;
}

// =============================================================================
// Verb Conjugation Rules
// =============================================================================

// Rule 1: Copula だ/です cannot follow verbs (except certain patterns)
// P4-1: Added のだ/んです exception
// P4-2: Added ようだ exception (そうだ already handled)
ConnectionRuleResult checkCopulaAfterVerb(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next,
                                          const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  if (next.surface != "だ" && next.surface != "です") {
    return {};
  }

  // Exception 1: 〜そう + だ/です is valid (hearsay/appearance)
  // E.g., 走りそうだ, 走りそうです
  if (endsWithSou(prev.surface)) {
    return {};
  }

  // Exception 2: 〜よう + だ/です is valid (appearance/intention)
  // E.g., 帰るようだ, 帰るようです
  if (endsWithYou(prev.surface)) {
    return {};
  }

  // Exception 3: 〜の/〜ん + だ/です is valid (explanatory copula)
  // E.g., 食べるのだ, 食べるんです (nominalized verb + copula)
  if (endsWithNodaBase(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::CopulaAfterVerb, opts.penalty_copula_after_verb,
          "copula after verb"};
}

// Rule 2: Ichidan renyokei + て/てV split should be avoided
ConnectionRuleResult checkIchidanRenyokeiTe(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& opts) {
  if (prev.pos != core::PartOfSpeech::Verb) return {};

  // Check if next starts with て
  bool is_te_pattern =
      (next.pos == core::PartOfSpeech::Particle && next.surface == "て") ||
      (next.pos == core::PartOfSpeech::Verb && startsWithTe(next.surface));

  if (!is_te_pattern) return {};

  // Check if prev ends with e-row (ichidan renyokei)
  if (!endsWithERow(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::IchidanRenyokeiTe, opts.penalty_ichidan_renyokei_te,
          "ichidan renyokei + te pattern"};
}

// Rule 3: Te-form split (音便形 or 一段形 → て/で)
// P4-4: Penalty encourages unified te-form; subsequent morphemes (から, も, etc.)
//       correctly attach to unified form (e.g., 食べて + から, not 食べ + て + から)
// NOTE: Excludes VERB + e-row + "て" which is handled by checkIchidanRenyokeiTe
ConnectionRuleResult checkTeFormSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next,
                                      const ConnectionOptions& opts) {
  // NOUN/VERB + PARTICLE pattern (can't simplify to single helper)
  if (next.pos != core::PartOfSpeech::Particle) return {};
  if (prev.pos != core::PartOfSpeech::Noun &&
      prev.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  if (next.surface != "て" && next.surface != "で") return {};

  // Check for godan onbin, ichidan endings, or godan renyokei i-row endings
  // Godan te-form patterns:
  //   - 書く → 書いて (onbin: い + て)
  //   - 読む → 読んで (onbin: ん + で)
  //   - 話す → 話して (renyokei: し + て, i-row ending)
  //   - いたす → いたして (renyokei: し + て)
  // Ichidan te-form patterns:
  //   - 食べる → 食べて (e-row ending)
  bool has_onbin = endsWithOnbinMarker(prev.surface);
  bool has_erow = endsWithERow(prev.surface);
  bool has_irow = endsWithIRow(prev.surface);

  if (!has_onbin && !has_erow && !has_irow) {
    return {};
  }

  // Skip VERB + e-row + "て" - already handled by checkIchidanRenyokeiTe
  if (prev.pos == core::PartOfSpeech::Verb && has_erow && next.surface == "て") {
    return {};
  }

  return {ConnectionPattern::TeFormSplit, opts.penalty_te_form_split,
          "te-form split pattern"};
}

// Rule 4: Verb renyokei + たい adjective handling
// P4-3: Verb-only for bonus; AUX penalty is intentional separate case
//
// Bonus cases (VERB only):
// - Short forms (たくて, たくない, etc.): No bonus - should be unified as single token
// - Long forms (たくなってきた, etc.): Give bonus for proper verb renyokei connection
//
// Penalty case (AUX):
// - AUX + たい patterns (e.g., なり(だ) + たかった): Penalize as unnatural
ConnectionRuleResult checkTaiAfterRenyokei(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (next.pos != core::PartOfSpeech::Adjective || next.lemma != "たい") {
    return {};
  }

  // Penalize AUX + たい pattern (e.g., なり(だ) + たかった)
  if (prev.pos == core::PartOfSpeech::Auxiliary) {
    return {ConnectionPattern::TaiAfterRenyokei, opts.penalty_tai_after_aux,
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
  return {ConnectionPattern::TaiAfterRenyokei, -opts.bonus_tai_after_renyokei,
          "tai-pattern after verb renyokei"};
}

// Rule 5: Renyokei-like noun + やすい (安い) penalty
ConnectionRuleResult checkYasuiAfterRenyokei(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next,
                                             const ConnectionOptions& opts) {
  if (!isNounToAdj(prev, next)) return {};

  if (next.surface != "やすい" || next.lemma != "安い") return {};

  if (!endsWithIRow(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::YasuiAfterRenyokei, opts.penalty_yasui_after_renyokei,
          "yasui adj after renyokei-like noun"};
}

// Rule 6: Verb renyokei + ながら split penalty
ConnectionRuleResult checkNagaraSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next,
                                      const ConnectionOptions& opts) {
  if (!isVerbToParticle(prev, next)) return {};

  if (next.surface != "ながら") return {};

  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::NagaraSplit, opts.penalty_nagara_split,
          "nagara split after renyokei verb"};
}

// Rule 7: Renyokei-like noun + そう (adverb) penalty
ConnectionRuleResult checkSouAfterRenyokei(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (!isNounToAdv(prev, next)) return {};

  if (next.surface != "そう") return {};

  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::SouAfterRenyokei, opts.penalty_sou_after_renyokei,
          "sou aux after renyokei-like noun"};
}

// Rule 10: Renyokei-like noun + compound verb auxiliary penalty
ConnectionRuleResult checkCompoundAuxAfterRenyokei(
    const core::LatticeEdge& prev, const core::LatticeEdge& next,
    const ConnectionOptions& opts) {
  if (!isNounToVerb(prev, next)) return {};

  if (next.surface.size() < core::kJapaneseCharBytes) return {};

  // Check if next starts with compound verb auxiliary kanji
  std::string_view first_char = next.surface.substr(0, core::kJapaneseCharBytes);
  if (!normalize::isCompoundVerbAuxStart(first_char)) {
    return {};
  }

  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::CompoundAuxAfterRenyokei,
          opts.penalty_compound_aux_after_renyokei,
          "compound aux after renyokei-like noun"};
}

// Rule 11: VERB renyokei + たくて (ADJ) split penalty
// Prevents 飲み + たくて from being preferred over 飲みたくて
ConnectionRuleResult checkTakuteAfterRenyokei(const core::LatticeEdge& prev,
                                              const core::LatticeEdge& next,
                                              const ConnectionOptions& opts) {
  if (!isVerbToAdj(prev, next)) return {};

  // Check if next is たくて form (ADJ with lemma たい)
  if (next.lemma != "たい" || next.surface != "たくて") return {};

  // Check if prev ends with renyokei marker
  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::TakuteAfterRenyokei,
          opts.penalty_takute_after_renyokei,
          "takute adj after renyokei verb"};
}

// Rule 12: Verb/Adjective たく + て split penalty
// Prevents 食べたく + て from being preferred over 食べたくて
// Also handles ADJ case: 見たく (ADJ) + て should be 見たくて
ConnectionRuleResult checkTakuTeSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next,
                                      const ConnectionOptions& opts) {
  // VERB/ADJ + PARTICLE pattern
  if (next.pos != core::PartOfSpeech::Particle) return {};
  if (prev.pos != core::PartOfSpeech::Verb &&
      prev.pos != core::PartOfSpeech::Adjective) {
    return {};
  }

  if (next.surface != "て") return {};

  // Check if prev ends with たく (desire adverbial form)
  if (prev.surface.size() < core::kTwoJapaneseCharBytes) {
    return {};
  }
  std::string_view last6 = prev.surface.substr(prev.surface.size() - core::kTwoJapaneseCharBytes);
  if (last6 != "たく") {
    return {};
  }

  return {ConnectionPattern::TakuTeSplit, opts.penalty_taku_te_split,
          "taku + te split (should be takute)"};
}

// Rule 15: Conditional verb (ending with ば) + verb (bonus)
// E.g., あれば + 手伝います - grammatically correct conditional clause
// This offsets the high VERB→VERB base cost for conditional patterns
ConnectionRuleResult checkConditionalVerbToVerb(const core::LatticeEdge& prev,
                                                const core::LatticeEdge& next,
                                                const ConnectionOptions& opts) {
  if (!isVerbToVerb(prev, next)) return {};

  // Check if prev verb ends with ば (conditional form)
  if (prev.surface.size() < core::kJapaneseCharBytes) return {};
  std::string_view last3 = prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes);
  if (last3 != "ば") {
    return {};
  }

  // Bonus (negative value) for conditional clause pattern
  return {ConnectionPattern::ConditionalVerbToVerb,
          -opts.bonus_conditional_verb_to_verb,
          "conditional verb to result verb"};
}

// Rule 16: Verb renyokei + compound auxiliary verb (bonus)
// E.g., 読み + 終わる, 書き + 始める, 走り + 続ける
// This gives bonus for proper VERB→VERB compound verb patterns
ConnectionRuleResult checkVerbRenyokeiCompoundAux(const core::LatticeEdge& prev,
                                                  const core::LatticeEdge& next,
                                                  const ConnectionOptions& opts) {
  if (!isVerbToVerb(prev, next)) return {};

  // Check if next starts with compound verb auxiliary kanji
  if (next.surface.size() < core::kJapaneseCharBytes) return {};
  std::string_view first_char = next.surface.substr(0, core::kJapaneseCharBytes);
  if (!normalize::isCompoundVerbAuxStart(first_char)) {
    return {};
  }

  // Check if prev ends with renyokei marker
  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  // Bonus (negative value) for compound verb pattern
  return {ConnectionPattern::VerbRenyokeiCompoundAux,
          -opts.bonus_verb_renyokei_compound_aux,
          "verb renyokei + compound aux verb"};
}

// Rule 17: Te-form VERB + VERB bonus
// E.g., 関して + 報告する, 調べて + わかる - te-form verb sequence
// This offsets the high VERB→VERB base cost for te-form patterns
// Excludes auxiliary verb patterns (いる, おる, しまう, etc.) which should be AUX
ConnectionRuleResult checkTeFormVerbToVerb(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (!isVerbToVerb(prev, next)) return {};

  // Check if prev verb ends with te-form (て or で)
  if (prev.surface.size() < core::kJapaneseCharBytes) return {};
  std::string_view last_char = prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes);
  if (last_char != "て" && last_char != "で") {
    return {};
  }

  // Exclude auxiliary verb patterns - these should be Auxiliary, not Verb
  // E.g., なって + おります should have おります as AUX
  if (isAuxiliaryVerbPattern(next.surface, next.lemma)) {
    return {};
  }

  // Bonus (negative value) for te-form + verb pattern
  return {ConnectionPattern::TeFormVerbToVerb, -opts.bonus_te_form_verb_to_verb,
          "te-form verb to verb"};
}

// Rule: PREFIX + VERB/AUX penalty
// P4-5: Honorific patterns work correctly because this penalty discourages
//       PREFIX→VERB, encouraging PREFIX→NOUN (renyokei as noun) instead.
//       E.g., お帰りになる → お(PREFIX)+帰り(NOUN)+に(PARTICLE)+なる(VERB)
// Prefixes should attach to nouns/suffixes, not verbs
// E.g., 何してる - 何 should be PRON, not PREFIX
ConnectionRuleResult checkPrefixBeforeVerb(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (prev.pos != core::PartOfSpeech::Prefix) return {};
  if (next.pos != core::PartOfSpeech::Verb &&
      next.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }

  return {ConnectionPattern::PrefixBeforeVerb, opts.penalty_prefix_before_verb,
          "prefix before verb"};
}

// Rule: VERB (renyokei) + と (PARTICLE) penalty
// E.g., 食べ + と is likely part of 食べといた/食べとく contraction
// This split should be penalized to prefer the single token interpretation
// Applies when: prev ends with e-row (ichidan renyokei) or onbin marker
ConnectionRuleResult checkTokuContractionSplit(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isVerbToParticle(prev, next)) return {};

  // Check if next is と particle
  if (next.surface != "と") return {};

  // Check if prev verb ends with renyokei-like pattern
  // Ichidan: ends with e-row (べ, け, て, etc.)
  // Godan onbin: ends with ん, っ, い (after te-form contraction)
  bool is_erow = grammar::endsWithERow(prev.surface);
  bool is_onbin = grammar::endsWithOnbin(prev.surface);

  if (!is_erow && !is_onbin) {
    return {};
  }

  return {ConnectionPattern::TokuContractionSplit,
          opts.penalty_toku_contraction_split, "toku contraction split"};
}

// Rule: VERB/ADJ → らしい (ADJ) bonus
// Conjecture auxiliary pattern: 帰るらしい, 美しいらしい
// This offsets the high VERB/ADJ→ADJ base cost (0.8) for valid rashii patterns
// Note: Does not apply to NOUN→らしい (男らしい should stay as single token)
ConnectionRuleResult checkRashiiAfterPredicate(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  // Only VERB/ADJ → ADJ pattern
  if (next.pos != core::PartOfSpeech::Adjective) return {};
  if (prev.pos != core::PartOfSpeech::Verb &&
      prev.pos != core::PartOfSpeech::Adjective) {
    return {};
  }

  // Check if next is らしい or its conjugated forms
  if (next.surface != "らしい" && next.surface != "らしかった" &&
      next.surface != "らしく" && next.surface != "らしくて" &&
      next.surface != "らしければ" && next.surface != "らしくない" &&
      next.surface != "らしくなかった") {
    return {};
  }

  // Bonus (negative value) for conjecture auxiliary pattern
  return {ConnectionPattern::RashiiAfterPredicate,
          -opts.bonus_rashii_after_predicate,
          "rashii conjecture after verb/adj"};
}

// Rule: VERB → case particle (を/が/で/へ) penalty
// Verb renyokei/base form cannot directly connect to case particles
// E.g., 打ち合わせ(VERB)+を is unnatural; should be 打ち合わせ(NOUN)+を
// Exception: に is excluded because 連用形+に+移動動詞 is valid (買いに行く)
// Exception: から is excluded because it's conjunctive (理由), not case particle
// Exception: まで is excluded because it's adverbial (範囲), not case particle
// Exception: te-form verbs are excluded (handled separately)
ConnectionRuleResult checkVerbToCaseParticle(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next,
                                             const ConnectionOptions& opts) {
  if (!isVerbToParticle(prev, next)) return {};

  // Only apply to true case particles (格助詞): を/が/で/へ
  // に is excluded: 連用形+に+移動動詞 is valid (買いに行く, 見に来る)
  // から is excluded: conjunctive particle for reason (疲れたから)
  // まで is excluded: adverbial particle for range (食べるまで)
  const auto& surf = next.surface;
  if (surf != "を" && surf != "が" && surf != "で" && surf != "へ") {
    return {};
  }

  // Exclude te-form verbs (て/で ending) - they have different connection patterns
  if (prev.surface.size() >= core::kJapaneseCharBytes) {
    std::string_view last = prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes);
    if (last == "て" || last == "で") {
      return {};
    }
    // Exclude classical negative ぬ + で (知らぬで = 知らないで)
    // で after ぬ-form is te-form connection, not case particle
    if (last == "ぬ" && surf == "で") {
      return {};
    }
  }

  return {ConnectionPattern::VerbToCaseParticle, opts.penalty_verb_to_case_particle,
          "verb to case particle (likely nominalized)"};
}

}  // namespace connection_rules
}  // namespace suzume::analysis
