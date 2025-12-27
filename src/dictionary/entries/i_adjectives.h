#ifndef SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_
#define SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_

// =============================================================================
// I-Adjectives (形容詞) - Layer 1 Hardcoded Entries
// =============================================================================
// Single-kanji + い pattern adjectives that could be confused with Godan verb
// renyokei. Pattern: 寒い (adjective) vs 伴い (verb renyokei)
//
// Without these entries, the unknown word generator skips single-kanji + い
// patterns (to avoid generating false adjectives from verb renyokei like 伴い).
// Real single-kanji i-adjectives must be registered here.
//
// Layer 1 Criteria:
//   - Closed class: Single-kanji + い adjectives are limited in number
//   - Prevents tokenization errors: 寒い → 寒 + い (wrong)
//
// NOT included here (handled by inflection analyzer or L2):
//   - Multi-character patterns: 難しい, 美しい, 楽しい (no confusion risk)
//   - Hiragana adjectives: やばい, すごい (L2 or other entries)
// =============================================================================

#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get i-adjective entries for core dictionary
 *
 * Single-kanji + い adjectives that need explicit dictionary registration
 * to prevent confusion with verb renyokei forms.
 *
 * @return Vector of dictionary entries for i-adjectives
 */
inline std::vector<DictionaryEntry> getIAdjectiveEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  // Only 漢字+い patterns (potential verb renyokei confusion)
  return {
      // ===== Temperature/Sensation =====
      {"寒い", POS::Adjective, 0.3F, "寒い", false, false, false, CT::IAdjective, "さむい"},
      {"暑い", POS::Adjective, 0.3F, "暑い", false, false, false, CT::IAdjective, "あつい"},
      {"熱い", POS::Adjective, 0.3F, "熱い", false, false, false, CT::IAdjective, "あつい"},
      {"痛い", POS::Adjective, 0.3F, "痛い", false, false, false, CT::IAdjective, "いたい"},
      {"怖い", POS::Adjective, 0.3F, "怖い", false, false, false, CT::IAdjective, "こわい"},
      {"辛い", POS::Adjective, 0.3F, "辛い", false, false, false, CT::IAdjective, "からい"},
      {"甘い", POS::Adjective, 0.3F, "甘い", false, false, false, CT::IAdjective, "あまい"},
      {"苦い", POS::Adjective, 0.3F, "苦い", false, false, false, CT::IAdjective, "にがい"},
      {"酸い", POS::Adjective, 0.3F, "酸い", false, false, false, CT::IAdjective, "すい"},
      {"臭い", POS::Adjective, 0.3F, "臭い", false, false, false, CT::IAdjective, "くさい"},
      {"眠い", POS::Adjective, 0.3F, "眠い", false, false, false, CT::IAdjective, "ねむい"},
      {"硬い", POS::Adjective, 0.3F, "硬い", false, false, false, CT::IAdjective, "かたい"},

      // ===== Colors =====
      {"赤い", POS::Adjective, 0.3F, "赤い", false, false, false, CT::IAdjective, "あかい"},
      {"青い", POS::Adjective, 0.3F, "青い", false, false, false, CT::IAdjective, "あおい"},
      {"白い", POS::Adjective, 0.3F, "白い", false, false, false, CT::IAdjective, "しろい"},
      {"黒い", POS::Adjective, 0.3F, "黒い", false, false, false, CT::IAdjective, "くろい"},

      // ===== Size/Shape =====
      {"長い", POS::Adjective, 0.3F, "長い", false, false, false, CT::IAdjective, "ながい"},
      {"高い", POS::Adjective, 0.3F, "高い", false, false, false, CT::IAdjective, "たかい"},
      {"低い", POS::Adjective, 0.3F, "低い", false, false, false, CT::IAdjective, "ひくい"},
      {"広い", POS::Adjective, 0.3F, "広い", false, false, false, CT::IAdjective, "ひろい"},
      {"狭い", POS::Adjective, 0.3F, "狭い", false, false, false, CT::IAdjective, "せまい"},
      {"太い", POS::Adjective, 0.3F, "太い", false, false, false, CT::IAdjective, "ふとい"},
      {"細い", POS::Adjective, 0.3F, "細い", false, false, false, CT::IAdjective, "ほそい"},
      {"深い", POS::Adjective, 0.3F, "深い", false, false, false, CT::IAdjective, "ふかい"},
      {"浅い", POS::Adjective, 0.3F, "浅い", false, false, false, CT::IAdjective, "あさい"},
      {"丸い", POS::Adjective, 0.3F, "丸い", false, false, false, CT::IAdjective, "まるい"},
      {"厚い", POS::Adjective, 0.3F, "厚い", false, false, false, CT::IAdjective, "あつい"},
      {"薄い", POS::Adjective, 0.3F, "薄い", false, false, false, CT::IAdjective, "うすい"},
      {"重い", POS::Adjective, 0.3F, "重い", false, false, false, CT::IAdjective, "おもい"},
      {"軽い", POS::Adjective, 0.3F, "軽い", false, false, false, CT::IAdjective, "かるい"},
      {"固い", POS::Adjective, 0.3F, "固い", false, false, false, CT::IAdjective, "かたい"},
      {"濃い", POS::Adjective, 0.3F, "濃い", false, false, false, CT::IAdjective, ""},  // No hiragana entry: こい conflicts with 来い (imperative)
      {"短い", POS::Adjective, 0.3F, "短い", false, false, false, CT::IAdjective, "みじかい"},

      // ===== Time/Speed =====
      {"早い", POS::Adjective, 0.3F, "早い", false, false, false, CT::IAdjective, "はやい"},
      {"速い", POS::Adjective, 0.3F, "速い", false, false, false, CT::IAdjective, "はやい"},
      {"遅い", POS::Adjective, 0.3F, "遅い", false, false, false, CT::IAdjective, "おそい"},
      {"近い", POS::Adjective, 0.3F, "近い", false, false, false, CT::IAdjective, "ちかい"},
      {"遠い", POS::Adjective, 0.3F, "遠い", false, false, false, CT::IAdjective, "とおい"},
      {"古い", POS::Adjective, 0.3F, "古い", false, false, false, CT::IAdjective, "ふるい"},
      {"若い", POS::Adjective, 0.3F, "若い", false, false, false, CT::IAdjective, "わかい"},

      // ===== Quality/Evaluation =====
      {"良い", POS::Adjective, 0.3F, "良い", false, false, false, CT::IAdjective, "よい"},
      {"悪い", POS::Adjective, 0.3F, "悪い", false, false, false, CT::IAdjective, "わるい"},
      {"強い", POS::Adjective, 0.3F, "強い", false, false, false, CT::IAdjective, "つよい"},
      {"弱い", POS::Adjective, 0.3F, "弱い", false, false, false, CT::IAdjective, "よわい"},
      {"安い", POS::Adjective, 0.3F, "安い", false, false, false, CT::IAdjective, "やすい"},
      {"汚い", POS::Adjective, 0.3F, "汚い", false, false, false, CT::IAdjective, "きたない"},
      {"醜い", POS::Adjective, 0.3F, "醜い", false, false, false, CT::IAdjective, "みにくい"},
      {"鋭い", POS::Adjective, 0.3F, "鋭い", false, false, false, CT::IAdjective, "するどい"},
      {"鈍い", POS::Adjective, 0.3F, "鈍い", false, false, false, CT::IAdjective, "にぶい"},

      // ===== Quantity =====
      {"多い", POS::Adjective, 0.3F, "多い", false, false, false, CT::IAdjective, "おおい"},

      // ===== Other single-kanji + い =====
      {"暗い", POS::Adjective, 0.3F, "暗い", false, false, false, CT::IAdjective, "くらい"},
      {"狡い", POS::Adjective, 0.3F, "狡い", false, false, false, CT::IAdjective, "ずるい"},
      {"旨い", POS::Adjective, 0.3F, "旨い", false, false, false, CT::IAdjective, "うまい"},

      // ===== Special irregular adjective =====
      {"いい", POS::Adjective, 0.3F, "いい", false, false, false, CT::IAdjective, ""},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_
