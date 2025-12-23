#ifndef SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_
#define SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get time noun entries for core dictionary (WASM fallback)
 *
 * Time nouns (時間名詞) are words that indicate time periods.
 * These have high priority (low cost) to ensure proper segmentation.
 *
 * NOTE: This is a WASM fallback. The canonical source of truth is:
 *   data/core/basic.tsv
 *
 * Per the dictionary layer design (backup/dictionary_layers.md):
 * - Layer 1 (hardcoded): Only for closed grammatical classes (particles, etc.)
 * - Layer 2 (core.dic): Vocabulary including time nouns
 *
 * Time nouns are included here temporarily for WASM minimal builds.
 * Native builds should load from core.dic when implemented.
 *
 * @return Vector of dictionary entries for time nouns
 */
inline std::vector<DictionaryEntry> getTimeNounEntries() {
  using POS = core::PartOfSpeech;

  return {
      // Days (日)
      {"今日", POS::Noun, 0.5F, "きょう", false, false, false},
      {"明日", POS::Noun, 0.5F, "あした", false, false, false},
      {"昨日", POS::Noun, 0.5F, "きのう", false, false, false},
      {"明後日", POS::Noun, 0.5F, "あさって", false, false, false},
      {"一昨日", POS::Noun, 0.5F, "おととい", false, false, false},
      {"毎日", POS::Noun, 0.5F, "まいにち", false, false, false},

      // Time of day (時間帯)
      {"今朝", POS::Noun, 0.5F, "けさ", false, false, false},
      {"今晩", POS::Noun, 0.5F, "こんばん", false, false, false},
      {"今夜", POS::Noun, 0.5F, "こんや", false, false, false},
      {"昨夜", POS::Noun, 0.5F, "さくや", false, false, false},
      {"朝", POS::Noun, 0.6F, "あさ", false, false, false},
      {"昼", POS::Noun, 0.6F, "ひる", false, false, false},
      {"夜", POS::Noun, 0.6F, "よる", false, false, false},
      {"夕方", POS::Noun, 0.6F, "ゆうがた", false, false, false},

      // Weeks (週)
      {"今週", POS::Noun, 0.5F, "こんしゅう", false, false, false},
      {"来週", POS::Noun, 0.5F, "らいしゅう", false, false, false},
      {"先週", POS::Noun, 0.5F, "せんしゅう", false, false, false},
      {"毎週", POS::Noun, 0.5F, "まいしゅう", false, false, false},

      // Months (月)
      {"今月", POS::Noun, 0.5F, "こんげつ", false, false, false},
      {"来月", POS::Noun, 0.5F, "らいげつ", false, false, false},
      {"先月", POS::Noun, 0.5F, "せんげつ", false, false, false},
      {"毎月", POS::Noun, 0.5F, "まいつき", false, false, false},

      // Years (年)
      {"今年", POS::Noun, 0.5F, "ことし", false, false, false},
      {"来年", POS::Noun, 0.5F, "らいねん", false, false, false},
      {"去年", POS::Noun, 0.5F, "きょねん", false, false, false},
      {"昨年", POS::Noun, 0.5F, "さくねん", false, false, false},
      {"毎年", POS::Noun, 0.5F, "まいとし", false, false, false},

      // Other time expressions
      {"今", POS::Noun, 0.5F, "いま", false, false, false},
      {"現在", POS::Noun, 0.5F, "げんざい", false, false, false},
      {"最近", POS::Noun, 0.5F, "さいきん", false, false, false},
      {"将来", POS::Noun, 0.5F, "しょうらい", false, false, false},
      {"過去", POS::Noun, 0.5F, "かこ", false, false, false},
      {"未来", POS::Noun, 0.5F, "みらい", false, false, false},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_
