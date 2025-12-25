#ifndef SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_
#define SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_

// =============================================================================
// I-Adjectives (形容詞) - Layer 1 Hardcoded Entries
// =============================================================================
// Single-kanji i-adjectives that could be confused with Godan verb renyokei.
// Pattern: 寒い vs 伴い (adjective vs verb renyokei)
//
// Without these entries, the unknown word generator skips single-kanji + い
// patterns (to avoid generating false adjectives from verb renyokei like 伴い).
// Real single-kanji i-adjectives must be registered here.
//
// Layer 1 Criteria:
//   - Closed class: Single-kanji i-adjectives are limited in number
//   - High frequency: Essential for everyday Japanese
//   - Prevents tokenization errors: 寒い → 寒 + い (wrong)
// =============================================================================

#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get i-adjective entries for core dictionary
 *
 * Single-kanji i-adjectives that need explicit dictionary registration.
 *
 * @return Vector of dictionary entries for i-adjectives
 */
inline std::vector<DictionaryEntry> getIAdjectiveEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // ===== Temperature/Sensation =====
      {"寒い", POS::Adjective, 0.3F, "寒い", false, false, false, CT::IAdjective, "さむい"},
      {"暑い", POS::Adjective, 0.3F, "暑い", false, false, false, CT::IAdjective, "あつい"},
      {"熱い", POS::Adjective, 0.3F, "熱い", false, false, false, CT::IAdjective, "あつい"},
      {"冷たい", POS::Adjective, 0.3F, "冷たい", false, false, false, CT::IAdjective, "つめたい"},
      {"温かい", POS::Adjective, 0.3F, "温かい", false, false, false, CT::IAdjective, "あたたかい"},
      {"痛い", POS::Adjective, 0.3F, "痛い", false, false, false, CT::IAdjective, "いたい"},
      {"怖い", POS::Adjective, 0.3F, "怖い", false, false, false, CT::IAdjective, "こわい"},
      {"辛い", POS::Adjective, 0.3F, "辛い", false, false, false, CT::IAdjective, "からい"},  // spicy
      {"甘い", POS::Adjective, 0.3F, "甘い", false, false, false, CT::IAdjective, "あまい"},
      {"苦い", POS::Adjective, 0.3F, "苦い", false, false, false, CT::IAdjective, "にがい"},
      {"酸い", POS::Adjective, 0.3F, "酸い", false, false, false, CT::IAdjective, "すい"},
      {"臭い", POS::Adjective, 0.3F, "臭い", false, false, false, CT::IAdjective, "くさい"},
      {"眠い", POS::Adjective, 0.3F, "眠い", false, false, false, CT::IAdjective, "ねむい"},
      {"硬い", POS::Adjective, 0.3F, "硬い", false, false, false, CT::IAdjective, "かたい"},
      {"柔らかい", POS::Adjective, 0.3F, "柔らかい", false, false, false, CT::IAdjective, "やわらかい"},

      // ===== Colors =====
      {"赤い", POS::Adjective, 0.3F, "赤い", false, false, false, CT::IAdjective, "あかい"},
      {"青い", POS::Adjective, 0.3F, "青い", false, false, false, CT::IAdjective, "あおい"},
      {"白い", POS::Adjective, 0.3F, "白い", false, false, false, CT::IAdjective, "しろい"},
      {"黒い", POS::Adjective, 0.3F, "黒い", false, false, false, CT::IAdjective, "くろい"},
      {"黄色い", POS::Adjective, 0.3F, "黄色い", false, false, false, CT::IAdjective, "きいろい"},

      // ===== Size/Shape =====
      {"大きい", POS::Adjective, 0.3F, "大きい", false, false, false, CT::IAdjective, "おおきい"},
      {"小さい", POS::Adjective, 0.3F, "小さい", false, false, false, CT::IAdjective, "ちいさい"},
      {"長い", POS::Adjective, 0.3F, "長い", false, false, false, CT::IAdjective, "ながい"},
      {"短い", POS::Adjective, 0.3F, "短い", false, false, false, CT::IAdjective, "みじかい"},
      {"高い", POS::Adjective, 0.3F, "高い", false, false, false, CT::IAdjective, "たかい"},
      {"低い", POS::Adjective, 0.3F, "低い", false, false, false, CT::IAdjective, "ひくい"},
      {"広い", POS::Adjective, 0.3F, "広い", false, false, false, CT::IAdjective, "ひろい"},
      {"狭い", POS::Adjective, 0.3F, "狭い", false, false, false, CT::IAdjective, "せまい"},
      {"太い", POS::Adjective, 0.3F, "太い", false, false, false, CT::IAdjective, "ふとい"},
      {"細い", POS::Adjective, 0.3F, "細い", false, false, false, CT::IAdjective, "ほそい"},
      {"深い", POS::Adjective, 0.3F, "深い", false, false, false, CT::IAdjective, "ふかい"},
      {"浅い", POS::Adjective, 0.3F, "浅い", false, false, false, CT::IAdjective, "あさい"},
      {"丸い", POS::Adjective, 0.3F, "丸い", false, false, false, CT::IAdjective, "まるい"},
      {"厚い", POS::Adjective, 0.3F, "厚い", false, false, false, CT::IAdjective, "あつい"},  // thick
      {"薄い", POS::Adjective, 0.3F, "薄い", false, false, false, CT::IAdjective, "うすい"},
      {"重い", POS::Adjective, 0.3F, "重い", false, false, false, CT::IAdjective, "おもい"},
      {"軽い", POS::Adjective, 0.3F, "軽い", false, false, false, CT::IAdjective, "かるい"},
      {"固い", POS::Adjective, 0.3F, "固い", false, false, false, CT::IAdjective, "かたい"},
      {"濃い", POS::Adjective, 0.3F, "濃い", false, false, false, CT::IAdjective, "こい"},

      // ===== Time/Speed =====
      {"早い", POS::Adjective, 0.3F, "早い", false, false, false, CT::IAdjective, "はやい"},
      {"速い", POS::Adjective, 0.3F, "速い", false, false, false, CT::IAdjective, "はやい"},
      {"遅い", POS::Adjective, 0.3F, "遅い", false, false, false, CT::IAdjective, "おそい"},
      {"近い", POS::Adjective, 0.3F, "近い", false, false, false, CT::IAdjective, "ちかい"},
      {"遠い", POS::Adjective, 0.3F, "遠い", false, false, false, CT::IAdjective, "とおい"},
      {"古い", POS::Adjective, 0.3F, "古い", false, false, false, CT::IAdjective, "ふるい"},
      {"新しい", POS::Adjective, 0.3F, "新しい", false, false, false, CT::IAdjective, "あたらしい"},
      {"若い", POS::Adjective, 0.3F, "若い", false, false, false, CT::IAdjective, "わかい"},

      // ===== Quality/Evaluation =====
      {"面白い", POS::Adjective, 0.3F, "面白い", false, false, false, CT::IAdjective, "おもしろい"},
      {"良い", POS::Adjective, 0.3F, "良い", false, false, false, CT::IAdjective, "よい"},
      {"いい", POS::Adjective, 0.3F, "いい", false, false, false, CT::IAdjective, ""},
      {"悪い", POS::Adjective, 0.3F, "悪い", false, false, false, CT::IAdjective, "わるい"},
      {"強い", POS::Adjective, 0.3F, "強い", false, false, false, CT::IAdjective, "つよい"},
      {"弱い", POS::Adjective, 0.3F, "弱い", false, false, false, CT::IAdjective, "よわい"},
      {"易しい", POS::Adjective, 0.3F, "易しい", false, false, false, CT::IAdjective, "やさしい"},
      {"難しい", POS::Adjective, 0.3F, "難しい", false, false, false, CT::IAdjective, "むずかしい"},
      {"安い", POS::Adjective, 0.3F, "安い", false, false, false, CT::IAdjective, "やすい"},
      {"汚い", POS::Adjective, 0.3F, "汚い", false, false, false, CT::IAdjective, "きたない"},
      {"正しい", POS::Adjective, 0.3F, "正しい", false, false, false, CT::IAdjective, "ただしい"},
      {"美しい", POS::Adjective, 0.3F, "美しい", false, false, false, CT::IAdjective, "うつくしい"},
      {"醜い", POS::Adjective, 0.3F, "醜い", false, false, false, CT::IAdjective, "みにくい"},
      {"鋭い", POS::Adjective, 0.3F, "鋭い", false, false, false, CT::IAdjective, "するどい"},
      {"鈍い", POS::Adjective, 0.3F, "鈍い", false, false, false, CT::IAdjective, "にぶい"},
      {"珍しい", POS::Adjective, 0.3F, "珍しい", false, false, false, CT::IAdjective, "めずらしい"},
      {"素晴らしい", POS::Adjective, 0.3F, "素晴らしい", false, false, false, CT::IAdjective, "すばらしい"},

      // ===== Quantity =====
      {"多い", POS::Adjective, 0.3F, "多い", false, false, false, CT::IAdjective, "おおい"},
      {"少ない", POS::Adjective, 0.3F, "少ない", false, false, false, CT::IAdjective, "すくない"},

      // ===== Emotions/Mental states =====
      {"可愛い", POS::Adjective, 0.3F, "可愛い", false, false, false, CT::IAdjective, "かわいい"},
      {"嬉しい", POS::Adjective, 0.3F, "嬉しい", false, false, false, CT::IAdjective, "うれしい"},
      {"悲しい", POS::Adjective, 0.3F, "悲しい", false, false, false, CT::IAdjective, "かなしい"},
      {"楽しい", POS::Adjective, 0.3F, "楽しい", false, false, false, CT::IAdjective, "たのしい"},
      {"苦しい", POS::Adjective, 0.3F, "苦しい", false, false, false, CT::IAdjective, "くるしい"},
      {"寂しい", POS::Adjective, 0.3F, "寂しい", false, false, false, CT::IAdjective, "さびしい"},
      {"淋しい", POS::Adjective, 0.3F, "淋しい", false, false, false, CT::IAdjective, "さびしい"},
      {"恥ずかしい", POS::Adjective, 0.3F, "恥ずかしい", false, false, false, CT::IAdjective, "はずかしい"},
      {"懐かしい", POS::Adjective, 0.3F, "懐かしい", false, false, false, CT::IAdjective, "なつかしい"},
      {"嫌い", POS::Adjective, 0.3F, "嫌い", false, false, false, CT::NaAdjective, "きらい"},  // na-adjective
      {"羨ましい", POS::Adjective, 0.3F, "羨ましい", false, false, false, CT::IAdjective, "うらやましい"},
      {"恐ろしい", POS::Adjective, 0.3F, "恐ろしい", false, false, false, CT::IAdjective, "おそろしい"},
      {"激しい", POS::Adjective, 0.3F, "激しい", false, false, false, CT::IAdjective, "はげしい"},
      {"優しい", POS::Adjective, 0.3F, "優しい", false, false, false, CT::IAdjective, "やさしい"},
      {"厳しい", POS::Adjective, 0.3F, "厳しい", false, false, false, CT::IAdjective, "きびしい"},
      {"忙しい", POS::Adjective, 0.3F, "忙しい", false, false, false, CT::IAdjective, "いそがしい"},
      {"親しい", POS::Adjective, 0.3F, "親しい", false, false, false, CT::IAdjective, "したしい"},

      // ===== Other common adjectives =====
      {"明るい", POS::Adjective, 0.3F, "明るい", false, false, false, CT::IAdjective, "あかるい"},
      {"暗い", POS::Adjective, 0.3F, "暗い", false, false, false, CT::IAdjective, "くらい"},
      {"狡い", POS::Adjective, 0.3F, "狡い", false, false, false, CT::IAdjective, "ずるい"},
      {"等しい", POS::Adjective, 0.3F, "等しい", false, false, false, CT::IAdjective, "ひとしい"},
      {"旨い", POS::Adjective, 0.3F, "旨い", false, false, false, CT::IAdjective, "うまい"},
      {"不味い", POS::Adjective, 0.3F, "不味い", false, false, false, CT::IAdjective, "まずい"},

      // ===== Common hiragana adjectives =====
      // Note: Adjectives with kanji + reading auto-generate hiragana entries
      // Only pure-hiragana adjectives need explicit registration here
      {"おいしい", POS::Adjective, 0.3F, "おいしい", false, false, false, CT::IAdjective, ""},
      {"まずい", POS::Adjective, 0.3F, "まずい", false, false, false, CT::IAdjective, ""},
      {"つまらない", POS::Adjective, 0.3F, "つまらない", false, false, false, CT::IAdjective, ""},
      {"かわいい", POS::Adjective, 0.3F, "可愛い", false, false, false, CT::IAdjective, ""},
      {"うるさい", POS::Adjective, 0.3F, "うるさい", false, false, false, CT::IAdjective, ""},

      // ===== Colloquial/Slang adjectives =====
      {"やばい", POS::Adjective, 0.3F, "やばい", false, false, false, CT::IAdjective, ""},
      {"うざい", POS::Adjective, 0.5F, "うざい", false, false, false, CT::IAdjective, ""},
      {"だるい", POS::Adjective, 0.5F, "だるい", false, false, false, CT::IAdjective, ""},
      {"えらい", POS::Adjective, 0.3F, "えらい", false, false, false, CT::IAdjective, ""},
      {"すごい", POS::Adjective, 0.3F, "すごい", false, false, false, CT::IAdjective, ""},
      {"ひどい", POS::Adjective, 0.3F, "ひどい", false, false, false, CT::IAdjective, ""},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_
