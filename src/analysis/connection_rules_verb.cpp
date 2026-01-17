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
  if (lemma == scorer::kLemmaIru || lemma == scorer::kLemmaOru ||
      lemma == scorer::kLemmaShimau || lemma == scorer::kLemmaMiru ||
      lemma == scorer::kLemmaOku || lemma == scorer::kLemmaIku ||
      lemma == scorer::kLemmaKuru || lemma == scorer::kLemmaAgeru ||
      lemma == scorer::kLemmaMorau || lemma == scorer::kLemmaKureru ||
      lemma == scorer::kLemmaAru) {
    return true;
  }

  // Check surface for polite forms (conjugated forms not covered by lemma constants)
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

  if (next.surface != scorer::kCopulaDa && next.surface != scorer::kCopulaDesu) {
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
      (next.pos == core::PartOfSpeech::Particle && next.surface == scorer::kFormTe) ||
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

  if (next.surface != scorer::kFormTe && next.surface != scorer::kFormDe) return {};

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

  // Skip VERB + e-row + て - already handled by checkIchidanRenyokeiTe
  if (prev.pos == core::PartOfSpeech::Verb && has_erow &&
      next.surface == scorer::kFormTe) {
    return {};
  }

  // Skip dictionary NOUN + e-row + で - these are legitimate splits
  // e.g., 付け(NOUN) + で(PARTICLE) in "日付けで"
  if (prev.pos == core::PartOfSpeech::Noun && prev.fromDictionary() &&
      has_erow && next.surface == scorer::kFormDe) {
    return {};
  }

  // Skip suru-verb stem し(VERB, lemma=する) + て - MeCab-compatible split
  // e.g., 勉強して → 勉強 + し(VERB) + て(PARTICLE)
  if (prev.pos == core::PartOfSpeech::Verb && prev.surface == scorer::kSuffixShi &&
      prev.lemma == "する" && next.surface == scorer::kFormTe) {
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
  if (next.pos != core::PartOfSpeech::Adjective || next.lemma != scorer::kSuffixTai) {
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

// Rule 4b: Verb renyokei + た auxiliary handling (MeCab compatibility)
// Give bonus to split pattern: verb renyokei + た
// This enables 入れた → 入れ + た, 論じた → 論じ + た, etc.
// Does NOT apply to 音便形 (書い+た, 買っ+た, 飛ん+た should stay unified)
ConnectionRuleResult checkTaAfterRenyokei(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next,
                                          const ConnectionOptions& opts) {
  // Only process VERB → AUX(た)
  if (prev.pos != core::PartOfSpeech::Verb) {
    return {};
  }
  if (next.pos != core::PartOfSpeech::Auxiliary || next.surface != "た") {
    return {};
  }

  // Check if prev is a valid verb renyokei (連用形)
  // Valid patterns:
  // - Ends with i-row hiragana (五段連用形: 書き, 読み, 話し) or e-row (一段連用形: 食べ, 入れ)
  // EXCLUDE 音便形 (い, っ, ん) - these should stay unified with た
  // 書いた should NOT split (イ音便)
  // 買った should NOT split (促音便)
  // 飛んだ should NOT split (撥音便)
  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }
  if (endsWithOnbinMarker(prev.surface)) {
    return {};  // 音便形 - do not split
  }

  // Give bonus (negative value) to prefer this split
  return {ConnectionPattern::TaAfterRenyokei, -opts.bonus_ta_after_renyokei,
          "ta-aux after verb renyokei (MeCab compatible split)"};
}

// Rule 4c: Verb mizenkei + ない auxiliary handling (MeCab compatibility)
// Give bonus to split pattern: verb mizenkei + ない
// This enables 食べない → 食べ + ない, しれない → しれ + ない, etc.
// Mizenkei patterns:
// - 一段: え段 (食べ, しれ) - same as renyokei
// - 五段: あ段 (書か, 読ま)
// Note: 音便形 + ない doesn't exist (ない attaches to mizenkei, not onbin)
ConnectionRuleResult checkNaiAfterVerbMizenkei(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  // Only process VERB → AUX(ない)
  if (prev.pos != core::PartOfSpeech::Verb) {
    return {};
  }
  if (next.pos != core::PartOfSpeech::Auxiliary || next.surface != "ない") {
    return {};
  }

  // Check if prev is a valid verb mizenkei (未然形)
  // Valid patterns:
  // - 一段: ends with e-row hiragana (食べ, しれ, 見)
  // - 五段: ends with a-row hiragana (書か, 読ま, 話さ)
  // Note: 五段 mizenkei ends with あ段, not い段
  if (!endsWithRenyokeiMarker(prev.surface) && !endsWithARow(prev.surface)) {
    return {};
  }

  // Give bonus (negative value) to prefer this split
  return {ConnectionPattern::NaiAfterVerbMizenkei, -opts.bonus_nai_after_verb_mizenkei,
          "nai-aux after verb mizenkei (MeCab compatible split)"};
}

// Rule 4d: Verb mizenkei + れる/られる auxiliary handling (MeCab compatibility)
// Give bonus to split pattern: verb mizenkei + passive auxiliary
// This enables 言われる → 言わ + れる, 食べられる → 食べ + られる, etc.
// Passive patterns:
// - 五段: stem + あ段 + れる (書か+れる, 言わ+れる)
// - 一段: stem + られる (食べ+られる)
// - サ変: stem + される (処理+される)
ConnectionRuleResult checkPassiveAfterVerbMizenkei(const core::LatticeEdge& prev,
                                                    const core::LatticeEdge& next,
                                                    const ConnectionOptions& opts) {
  // Only process VERB → AUX patterns
  if (prev.pos != core::PartOfSpeech::Verb) {
    return {};
  }
  if (next.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }

  // Check if next is passive auxiliary (れる, られる, or conjugated forms)
  // れる patterns: れる, れた, れて, れない, れます, れべき, etc.
  // られる patterns: られる, られた, られて, られない, られます, られべき, etc.
  bool is_passive_aux = false;
  if (next.lemma == "れる" || next.lemma == "られる") {
    is_passive_aux = true;
  }
  // Check specific surface patterns for passive auxiliary conjugations
  // Godan passive (れる conjugations)
  if (next.surface == "れる" || next.surface == "れた" || next.surface == "れて" ||
      next.surface == "れない" || next.surface == "れます" || next.surface == "れません" ||
      next.surface == "れべき") {
    is_passive_aux = true;
  }
  // Ichidan passive (られる conjugations)
  if (next.surface == "られる" || next.surface == "られた" || next.surface == "られて" ||
      next.surface == "られない" || next.surface == "られます" || next.surface == "られません" ||
      next.surface == "られべき") {
    is_passive_aux = true;
  }

  if (!is_passive_aux) {
    return {};
  }

  // Check if prev is a valid verb mizenkei (未然形)
  // Valid patterns:
  // - 五段: ends with a-row hiragana (書か, 言わ, 読ま)
  // - 一段: ends with e-row hiragana (食べ, 見) or single kanji + られる
  // - サ変: さ (処理+さ+れる)
  bool is_valid_mizenkei = (endsWithARow(prev.surface) ||
                            endsWithRenyokeiMarker(prev.surface) ||
                            prev.surface == "さ");
  // Also accept single-kanji ichidan verbs followed by られる
  // E.g., 見 + られる, 寝 + られる, 着 + られる
  // For these, the verb surface is just kanji (no hiragana ending)
  if (!is_valid_mizenkei &&
      prev.conj_type == dictionary::ConjugationType::Ichidan &&
      (next.surface == "られる" || next.lemma == "られる")) {
    is_valid_mizenkei = true;
  }
  if (!is_valid_mizenkei) {
    return {};
  }

  // Exclude short katakana + け patterns from passive bonus
  // Pattern: KATAKANA(1-2chars) + け (e.g., エモけ, キモけ, ダサけ)
  // These are likely i-adjective misanalyses (エモければ = エモい + ければ)
  // Valid katakana verbs like ウケる form ウケ+られる (not ウケけ+れ)
  if (prev.surface.size() >= 9 &&  // 2+ katakana (6+ bytes) + け (3 bytes)
      utf8::endsWith(prev.surface, "け")) {
    // Check if the part before け is short katakana (1-2 chars = 3-6 bytes)
    std::string_view before_ke = prev.surface.substr(0, prev.surface.size() - 3);
    if (before_ke.size() <= 6 &&  // 1-2 katakana characters
        grammar::isPureKatakana(before_ke)) {
      return {};
    }
  }

  // Give bonus (negative value) to prefer this split
  return {ConnectionPattern::PassiveAfterVerbMizenkei, -opts.bonus_passive_after_verb_mizenkei,
          "passive-aux after verb mizenkei (MeCab compatible split)"};
}

// Rule 4e: しれる verb + ます/ない auxiliary bonus
// For かもしれません pattern: しれ(VERB) + ませ(AUX) should beat し(VERB) + れ(AUX) + ませ(AUX)
// The し+れ path gets passive-aux bonuses, so we need to compensate with a strong bonus
ConnectionRuleResult checkShireruToMasuNai(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  // Check if prev is しれる verb (lemma = しれる)
  if (prev.lemma != "しれる") return {};

  // Check if next is ます/ない type auxiliary
  bool is_masu_nai = (next.lemma == scorer::kLemmaMasu ||
                      next.lemma == "ない" ||
                      next.surface == "ない" || next.surface == "ます" ||
                      next.surface == "ませ" || next.surface == "まし");

  if (!is_masu_nai) return {};

  // Give strong bonus to compensate for し+れ path getting multiple bonuses
  // This ensures しれ + ませ beats し + れ + ませ in かもしれません
  (void)opts;  // Unused - using fixed bonus value
  return {ConnectionPattern::ShireruToMasuNai, -scorer::kBonusShireruToMasuNai,
          "shireru verb + masu/nai (prefer over suru+reru split)"};
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

// Rule 6b: Verb renyokei + 方 penalty (should be nominalized)
// 解き方, 読み方, 書き方 - the verb should be nominalized, not VERB
ConnectionRuleResult checkKataAfterRenyokei(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& opts) {
  if (!isVerbToNoun(prev, next)) return {};

  if (next.surface != "方") return {};

  if (!endsWithRenyokeiMarker(prev.surface)) {
    return {};
  }

  return {ConnectionPattern::KataAfterRenyokei, opts.penalty_kata_after_renyokei,
          "kata after renyokei verb (should be nominalized)"};
}

// Rule 7: Renyokei-like noun + そう (adverb) penalty
ConnectionRuleResult checkSouAfterRenyokei(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (!isNounToAdv(prev, next)) return {};

  if (next.surface != scorer::kSuffixSou) return {};

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
  if (next.lemma != scorer::kSuffixTai || next.surface != "たくて") return {};

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

  if (next.surface != scorer::kFormTe) return {};

  // Check if prev ends with たく (desire adverbial form)
  if (prev.surface.size() < core::kTwoJapaneseCharBytes) {
    return {};
  }
  std::string_view last6 = prev.surface.substr(prev.surface.size() - core::kTwoJapaneseCharBytes);
  if (last6 != "たく") {  // desire adverbial form - not a common constant
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
// Also handles hiragana: 食べ + すぎる, 食べ + はじめる
// This gives bonus for proper VERB→VERB compound verb patterns
ConnectionRuleResult checkVerbRenyokeiCompoundAux(const core::LatticeEdge& prev,
                                                  const core::LatticeEdge& next,
                                                  const ConnectionOptions& opts) {
  if (!isVerbToVerb(prev, next)) return {};

  // Check if next is a compound verb auxiliary
  bool is_compound_aux = false;

  // Check kanji pattern: first char is compound verb auxiliary kanji
  if (next.surface.size() >= core::kJapaneseCharBytes) {
    std::string_view first_char = next.surface.substr(0, core::kJapaneseCharBytes);
    if (normalize::isCompoundVerbAuxStart(first_char)) {
      is_compound_aux = true;
    }
  }

  // Check hiragana pattern: surface matches or starts with hiragana compound aux
  if (!is_compound_aux) {
    if (normalize::isHiraganaCompoundVerbAux(next.surface) ||
        normalize::startsWithHiraganaCompoundVerbAux(next.surface)) {
      is_compound_aux = true;
    }
  }

  if (!is_compound_aux) {
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
// Exception: Bare suru te-form "して" should NOT get bonus - MeCab splits as し+て
ConnectionRuleResult checkTeFormVerbToVerb(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (!isVerbToVerb(prev, next)) return {};

  if (!endsWithTeForm(prev.surface)) return {};

  // Exclude auxiliary verb patterns - these should be Auxiliary, not Verb
  // E.g., なって + おります should have おります as AUX
  if (isAuxiliaryVerbPattern(next.surface, next.lemma)) {
    return {};
  }

  // Don't give bonus for bare suru te-form "して" - should be split as し+て
  // MeCab: してみる → し + て + みる (3 tokens)
  if (prev.surface == "して" && prev.lemma == scorer::kLemmaSuru) {
    return {};  // No bonus for bare suru te-form
  }

  // Bonus (negative value) for te-form + verb pattern
  return {ConnectionPattern::TeFormVerbToVerb, -opts.bonus_te_form_verb_to_verb,
          "te-form verb to verb"};
}

// Rule: Verb renyokei + てる/でる/とく/どく (VERB) bonus
// Contracted progressive/preparative forms: してる = している, しとく = しておく
// These are now VERB (MeCab compatible) but need a connection bonus
ConnectionRuleResult checkRenyokeiToContractedVerb(const core::LatticeEdge& prev,
                                                   const core::LatticeEdge& next,
                                                   const ConnectionOptions& opts) {
  if (!isVerbToVerb(prev, next)) return {};

  // Check if next is contracted progressive/preparative verb (てる/でる/とく/どく etc.)
  bool is_contracted = (next.lemma == "てる" || next.lemma == "でる" ||
                        next.lemma == "とく" || next.lemma == "どく");
  if (!is_contracted) return {};

  // Exclude standalone て/で (single-char renyokei of てる/でる)
  // These should only get bonus when followed by た (handled by TeruRenyokeiToTa rule)
  // Without this check, 食べ + て would incorrectly get the bonus and beat 食べて (te-form)
  if (next.surface == scorer::kFormTe || next.surface == scorer::kFormDe) {
    return {};
  }

  // Check if prev ends with renyokei marker (し, き, り, etc.) or e-row (ichidan)
  bool is_renyokei = (endsWithRenyokeiMarker(prev.surface) ||
                      endsWithERow(prev.surface) ||
                      prev.surface.size() == core::kJapaneseCharBytes);  // single-char verbs (見, 着, etc.)
  if (!is_renyokei) return {};

  // Strong bonus to prefer し + てる over し(PARTICLE) + てる
  (void)opts;
  return {ConnectionPattern::RenyokeiToContractedVerb, -scorer::kBonusRenyokeiToContractedVerb,
          "verb renyokei + contracted progressive/preparative verb"};
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
  if (next.surface != scorer::kParticleTo) return {};

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

// Rule: てく/ってく + れ* pattern (colloquial ていく mis-segmentation)
// When "てく" (colloquial form of ていく) is followed by れ-starting auxiliary,
// it's almost always a mis-segmentation of てくれる pattern.
// E.g., つけてくれない → つけ + てく + れない is wrong
//       should be → つけて + くれない or つけてくれない
// This rule penalizes VERB "てく"/"ってく" followed by AUX starting with "れ"
ConnectionRuleResult checkTekuReMissegmentation(const core::LatticeEdge& prev,
                                                 const core::LatticeEdge& next,
                                                 const ConnectionOptions& opts) {
  // Check if prev is VERB
  if (prev.pos != core::PartOfSpeech::Verb) return {};

  // Check if prev surface is てく or ってく (colloquial ていく)
  if (prev.surface != "てく" && prev.surface != "ってく") return {};

  // Check if next is AUX starting with れ
  if (next.pos != core::PartOfSpeech::Auxiliary) return {};
  if (next.surface.size() < core::kJapaneseCharBytes) return {};
  if (next.surface.substr(0, core::kJapaneseCharBytes) != "れ") return {};

  // Apply strong penalty - this pattern is almost always wrong
  return {ConnectionPattern::TekuReMissegmentation,
          opts.penalty_teku_re_missegmentation,
          "teku + re mis-segmentation (should be te + kureru)"};
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
  if (surf != scorer::kParticleWo && surf != scorer::kParticleGa &&
      surf != scorer::kFormDe && surf != scorer::kParticleHe) {
    return {};
  }

  // Exclude te-form verbs (て/で ending) - they have different connection patterns
  if (prev.surface.size() >= core::kJapaneseCharBytes) {
    std::string_view last = prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes);
    if (last == scorer::kFormTe || last == scorer::kFormDe) {
      return {};
    }
    // Exclude classical negative ぬ + で (知らぬで = 知らないで)
    // で after ぬ-form is te-form connection, not case particle
    if (last == "ぬ" && surf == scorer::kFormDe) {  // ぬ is not a common constant
      return {};
    }
  }

  return {ConnectionPattern::VerbToCaseParticle, opts.penalty_verb_to_case_particle,
          "verb to case particle (likely nominalized)"};
}

// Rule: NOUN(ending with し) + VERB(starting with て) penalty
// Penalizes patterns like 説明し(NOUN) + てくれます(VERB)
// This is likely a suru-verb te-form split that should be parsed as:
//   説明して(VERB) + くれます(VERB) or 説明してくれます(VERB)
// The し ending suggests suru-verb renyokei, not a true noun
ConnectionRuleResult checkSuruRenyokeiToTeVerb(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isNounToVerb(prev, next)) return {};

  // Check if prev (NOUN) ends with し (suru-verb renyokei pattern)
  if (prev.surface.size() < core::kJapaneseCharBytes) return {};
  std::string_view last = prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes);
  if (last != scorer::kSuffixShi) return {};

  // Check if prev has 2+ kanji before し (typical suru-verb pattern)
  // e.g., 説明し, 確認し, 対応し, 検討し
  // Single kanji + し could be valid nouns (貸し, 押し, etc.)
  if (prev.surface.size() < core::kThreeJapaneseCharBytes) return {};

  // Check if next (VERB) starts with て or で (te-form pattern)
  if (!startsWithTe(next.surface)) return {};

  return {ConnectionPattern::SuruRenyokeiToTeVerb,
          opts.penalty_suru_renyokei_to_te_verb,
          "suru renyokei noun + te-verb (should be suru te-form)"};
}

// Rule: VERB(renyokei/onbinkei) → て/で(PARTICLE) bonus
// MeCab-compatible te-form split: 食べて → 食べ + て, 読んで → 読ん + で
// Conditions:
// - 一段: prev ends with e-row (食べ, 見, 入れ) → て
// - 五段音便: prev ends with い/ん/っ → て/で
//   - イ音便(書い, 泳い) → て
//   - 撥音便(読ん, 遊ん) → で
//   - 促音便(走っ, 待っ) → て
// - 五段renyokei: prev ends with i-row (話し, 書き) → て (for さ行 etc.)
ConnectionRuleResult checkRenyokeiToTeParticle(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isVerbToParticle(prev, next)) return {};

  // Check if next is て/で particle
  if (next.surface != scorer::kFormTe && next.surface != scorer::kFormDe) {
    return {};
  }

  // Check if prev is a valid verb form for te-form connection
  bool is_erow = endsWithERow(prev.surface);       // ichidan renyokei: 食べ, 見
  bool is_onbin = endsWithOnbinMarker(prev.surface);  // godan onbin: 書い, 読ん, 走っ
  bool is_irow = endsWithIRow(prev.surface);       // godan renyokei: 話し, 書き

  if (!is_erow && !is_onbin && !is_irow) {
    return {};
  }

  // Validate て/で match with onbin type
  // 撥音便(ん) requires で, others use て
  if (is_onbin) {
    std::string_view last = prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes);
    if (last == scorer::kSuffixN) {  // 撥音便: 読ん, 遊ん → で
      if (next.surface != scorer::kFormDe) {
        return {};  // 撥音便 + て is invalid
      }
    } else {  // イ音便/促音便: 書い, 走っ → て
      if (next.surface != scorer::kFormTe) {
        return {};  // イ音便/促音便 + で is invalid
      }
    }
  }

  // For e-row and i-row, only て is valid
  if ((is_erow || is_irow) && !is_onbin) {
    if (next.surface != scorer::kFormTe) {
      return {};
    }
  }

  // Skip single-char し (lemma=する) + て - too ambiguous
  // This prevents breaking dictionary words like どうして, そうして
  // Multi-char suru renyokei like 話し, 直し still get the bonus
  if (prev.surface == scorer::kSuffixShi && prev.lemma == scorer::kLemmaSuru) {
    return {};
  }

  // Strong bonus to prefer this split over unified te-form
  // This needs to offset the penalty from checkTeFormSplit
  (void)opts;  // Using fixed value for now
  return {ConnectionPattern::RenyokeiToTeParticle, -scorer::kBonusRenyokeiToTeParticle,
          "verb renyokei/onbinkei + te/de particle (MeCab-compatible te-form split)"};
}

// Rule: て/で(PARTICLE) → auxiliary verb bonus
// Give bonus when て/で particle is followed by auxiliary verbs (いる, しまう, etc.)
// This supports patterns like: 食べ + て + いる → 食べている
ConnectionRuleResult checkTeParticleToAuxVerb(const core::LatticeEdge& prev,
                                              const core::LatticeEdge& next,
                                              const ConnectionOptions& opts) {
  // Check if prev is て/で particle
  if (prev.pos != core::PartOfSpeech::Particle) return {};
  if (prev.surface != scorer::kFormTe && prev.surface != scorer::kFormDe) {
    return {};
  }

  // Check if next is an auxiliary verb pattern
  // Include both AUX and VERB for auxiliary patterns like いる, しまう
  if (next.pos != core::PartOfSpeech::Auxiliary &&
      next.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  // Check if next is a te-form auxiliary verb
  // いる, いた, いて, います etc. (progressive)
  // しまう, しまった, しまって, しまいます etc. (completive)
  // おく, おいた, おいて, おきます etc. (preparative)
  // くる, きた, きて, きます etc. (direction)
  // みる, みた, みて, みます etc. (attemptive)
  // もらう, もらった, もらって etc. (benefactive-receive)
  // くれる, くれた, くれて etc. (benefactive-give)
  // あげる, あげた, あげて etc. (benefactive-give-up)
  // ある, あった, あります etc. (resultative)
  bool is_te_aux = false;
  if (next.lemma == scorer::kLemmaIru || next.lemma == scorer::kLemmaOru ||
      next.lemma == scorer::kLemmaShimau || next.lemma == scorer::kLemmaOku ||
      next.lemma == scorer::kLemmaKuru || next.lemma == scorer::kLemmaMiru ||
      next.lemma == scorer::kLemmaMorau || next.lemma == scorer::kLemmaKureru ||
      next.lemma == scorer::kLemmaAgeru || next.lemma == scorer::kLemmaAru ||
      next.lemma == scorer::kLemmaIku) {
    is_te_aux = true;
  }

  // Also check surface for specific patterns
  if (isAuxiliaryVerbPattern(next.surface, next.lemma)) {
    is_te_aux = true;
  }

  if (!is_te_aux) {
    return {};
  }

  // Bonus for valid te-form + auxiliary pattern
  (void)opts;
  return {ConnectionPattern::TeParticleToAuxVerb, -scorer::kBonusTeParticleToAuxVerb,
          "te/de particle + auxiliary verb"};
}

}  // namespace connection_rules
}  // namespace suzume::analysis
