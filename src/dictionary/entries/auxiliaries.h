#ifndef SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_
#define SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Auxiliary Verbs (助動詞) - ~25 entries
//   - Assertion (断定): だ, です, である
//   - Polite (丁寧): ます, ました, ません
//   - Negation (否定): ない, ぬ, なかった
//   - Past/Completion (過去・完了): た
//   - Conjecture (推量): う, よう, だろう, でしょう
//   - Desire (願望): たい, たがる
//   - Potential/Passive/Causative: れる, られる, せる, させる
//
// DO NOT add lexical verbs (食べる, 書く, etc.) here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get auxiliary verb entries for core dictionary
 * @return Vector of dictionary entries for auxiliary verbs
 */
inline std::vector<DictionaryEntry> getAuxiliaryEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // All auxiliaries are hiragana-only; reading field is empty
  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // Assertion (断定) - Copula conjugations are hardcoded because:
      // 1. だった cannot be split as だ+った (った is not a valid suffix)
      // 2. でした/であった would incorrectly split as で+した/で+あった
      // 3. These are high-frequency forms that require reliable recognition
      // Very low cost to prioritize over particle + verb splits
      {"だ", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"だった", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"だったら", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"です", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"でした", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"でしたら", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"である", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"であった", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"であれば", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},

      // Polite (丁寧)
      {"ます", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"ました", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"ません", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Negation (否定) - ない conjugations (i-adjective pattern)
      // Use higher cost (1.0F) to not break compound verb forms like 説明しない
      {"ない", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なかった", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なくて", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なければ", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"ぬ", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Past/Completion (過去・完了)
      {"た", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Conjecture (推量)
      {"う", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"よう", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"だろう", POS::Auxiliary, 0.5F, "", false, false, false, CT::None, ""},
      {"でしょう", POS::Auxiliary, 0.5F, "", false, false, false, CT::None, ""},

      // Possibility/Uncertainty (可能性・不確実) - かもしれない forms
      // Without these, "もしれません" is incorrectly parsed as verb "もしれる"
      {"かもしれない", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},
      {"かもしれません", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},
      {"かもしれなかった", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},

      // Desire (願望) - たい conjugations (i-adjective pattern)
      // Base form is Auxiliary, conjugated forms are Adjective (i-adjective conjugation)
      {"たい", POS::Auxiliary, 0.3F, "たい", false, false, false, CT::None, ""},
      {"たかった", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくない", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくなかった", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくて", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たければ", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たがる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Potential/Passive/Causative (可能・受身・使役)
      {"れる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"られる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"せる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"させる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Polite existence (丁寧存在) - ございます conjugations
      {"ございます", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございました", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございましたら", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございません", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},

      // Request (依頼) - ください
      {"ください", POS::Auxiliary, 0.3F, "ください", false, false, false, CT::None, ""},
      {"くださいませ", POS::Auxiliary, 0.3F, "ください", false, false, false, CT::None, ""},

      // Progressive/Continuous (進行・継続) - いる conjugations
      // Used after te-form verbs: 食べている (is eating), 見ていた (was watching)
      // Splits te-form + auxiliary for grammatically accurate analysis
      // Note: いた is NOT included to avoid breaking いたす (致す) verb
      {"いる", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"います", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いました", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いません", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いない", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いなかった", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いれば", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},

      // Explanatory (説明) - のだ/んだ forms
      {"のだ", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"のです", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"のでした", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んだ", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んです", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んでした", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},

      // Kuruwa-kotoba (廓言葉) - Yoshiwara courtesan speech
      // ありんす series (polite existence, from あります)
      {"ありんす", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ありんした", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ありんせん", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      // ざんす series (polite existence, from ございます)
      {"ざんす", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ざました", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ざんせん", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      // でありんす (copula + ありんす)
      {"でありんす", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"でありんした", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},

      // =========================================================================
      // Character speech patterns (キャラクター語尾/役割語)
      // Stylistic variants used in anime/games/literature.
      // For katakana entries, hiragana versions are also registered.
      // =========================================================================

      // Cat-like (猫系) - にゃ語尾
      {"にゃ", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, ""},
      {"にゃん", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, ""},
      {"にゃー", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, ""},
      {"ニャ", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, "にゃ"},
      {"ニャン", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, "にゃん"},
      {"ニャー", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, "にゃー"},
      // Compound forms (だ/です + にゃ) - very low cost to beat verb misrecognition
      // lemma is だよ/ですよ because にゃ functions as よ (sentence-ending particle)
      {"だにゃ", POS::Auxiliary, 0.01F, "だよ", false, false, false, CT::None, ""},
      {"だにゃん", POS::Auxiliary, 0.01F, "だよ", false, false, false, CT::None, ""},
      {"ですにゃ", POS::Auxiliary, 0.01F, "ですよ", false, false, false, CT::None, ""},
      {"ですにゃん", POS::Auxiliary, 0.01F, "ですよ", false, false, false, CT::None, ""},

      // Squid character (イカ娘) - ゲソ語尾
      {"ゲソ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "げそ"},
      {"げそ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"でゲソ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "でげそ"},
      {"でげそ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},

      // Ojou-sama/Lady speech (お嬢様言葉)
      // Lower cost to beat です+わ(particle) split
      {"ですわ", POS::Auxiliary, 0.1F, "です", false, false, false, CT::None, ""},
      {"ましたわ", POS::Auxiliary, 0.1F, "ました", false, false, false, CT::None, ""},
      {"ませんわ", POS::Auxiliary, 0.1F, "ません", false, false, false, CT::None, ""},
      {"ですの", POS::Auxiliary, 0.1F, "です", false, false, false, CT::None, ""},
      {"ますの", POS::Auxiliary, 0.1F, "ます", false, false, false, CT::None, ""},
      {"だわ", POS::Auxiliary, 0.1F, "だ", false, false, false, CT::None, ""},

      // Youth slang (若者言葉)
      {"っす", POS::Auxiliary, 0.3F, "です", false, false, false, CT::None, ""},
      {"っした", POS::Auxiliary, 0.3F, "でした", false, false, false, CT::None, ""},
      {"っすか", POS::Auxiliary, 0.3F, "ですか", false, false, false, CT::None, ""},

      // Rabbit-like (兎系)
      {"ぴょん", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"ピョン", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "ぴょん"},

      // Ninja/Old-fashioned (忍者・古風)
      {"ござる", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"でござる", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"ござった", POS::Auxiliary, 0.3F, "だった", false, false, false, CT::None, ""},
      {"でござった", POS::Auxiliary, 0.3F, "だった", false, false, false, CT::None, ""},
      {"ござらぬ", POS::Auxiliary, 0.3F, "ではない", false, false, false, CT::None, ""},
      {"ござらん", POS::Auxiliary, 0.3F, "ではない", false, false, false, CT::None, ""},
      {"でございます", POS::Auxiliary, 0.3F, "です", false, false, false, CT::None, ""},
      {"ナリ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "なり"},
      {"なり", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"でナリ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "でなり"},
      {"でなり", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},

      // Elderly/Archaic (老人・古風) - じゃ語尾
      {"じゃ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"じゃな", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"のじゃ", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"じゃろう", POS::Auxiliary, 0.3F, "だろう", false, false, false, CT::None, ""},

      // Regional dialects as character speech (方言系)
      // Higher cost (1.0) to avoid false positives like やばい → まじや+ばい
      {"ぜよ", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"だべ", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"やんけ", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"やで", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"やねん", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"だっちゃ", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"ばい", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},

      // Robot/Mechanical (ロボット・機械)
      {"デス", POS::Auxiliary, 0.3F, "です", false, false, false, CT::None, "です"},
      {"マス", POS::Auxiliary, 0.3F, "ます", false, false, false, CT::None, "ます"},
  };
}

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_
