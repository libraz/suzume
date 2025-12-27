#ifndef SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_
#define SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_

// =============================================================================
// ⚠️ DEPRECATED: Entries migrated to Layer 2 (data/core/dictionary.tsv)
// =============================================================================
// This file contains OPEN CLASS vocabulary that does not belong in Layer 1:
//   - Time nouns (時間名詞): Nouns are open class
//   - Examples: 今日, 明日, 昨日, 今週, 来月, 今年, etc.
//
// Current status: WASM fallback (entries also in dictionary.tsv)
//   - Kept for WASM minimal builds where core.dic is not available
//   - Native builds with core.dic will have duplicate entries (same cost)
//   - Canonical source: data/core/dictionary.tsv (section: time_nouns.h migration)
//
// Migration status: COMPLETED (2025-12-27)
//   - All entries copied to data/core/dictionary.tsv
//   - This file remains for WASM fallback compatibility
//   - TODO: Add #ifdef SUZUME_WASM conditional compilation
//
// Layer 1 Criteria (this file DOES NOT meet):
//   ❌ CLOSED CLASS: Nouns are open class (new words can be coined)
//   ❌ Grammatically fixed: Time expressions can be extended
//   ✅ Needed for WASM: Only reason this file exists
// =============================================================================

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get time noun entries for core dictionary (WASM fallback)
 *
 * @deprecated This function provides WASM fallback entries.
 * For native builds, use core.dic instead.
 *
 * Time nouns (時間名詞) are words that indicate time periods.
 * These have high priority (low cost) to ensure proper segmentation.
 *
 * @return Vector of dictionary entries for time nouns
 */
inline std::vector<DictionaryEntry> getTimeNounEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // Note: Time nouns use is_formal_noun=true to enable hiragana auto-expansion
  // This is a pragmatic choice - time nouns are functionally a closed class
  return {
      // Days (日)
      {"今日", POS::Noun, 0.5F, "", false, true, false, CT::None, "きょう"},
      {"明日", POS::Noun, 0.5F, "", false, true, false, CT::None, "あした"},
      {"昨日", POS::Noun, 0.5F, "", false, true, false, CT::None, "きのう"},
      {"明後日", POS::Noun, 0.5F, "", false, true, false, CT::None, "あさって"},
      {"一昨日", POS::Noun, 0.5F, "", false, true, false, CT::None, "おととい"},
      {"毎日", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいにち"},

      // Time of day (時間帯)
      {"今朝", POS::Noun, 0.5F, "", false, true, false, CT::None, "けさ"},
      {"毎朝", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいあさ"},
      {"今晩", POS::Noun, 0.5F, "", false, true, false, CT::None, "こんばん"},
      {"今夜", POS::Noun, 0.5F, "", false, true, false, CT::None, "こんや"},
      {"昨夜", POS::Noun, 0.5F, "", false, true, false, CT::None, "さくや"},
      {"朝", POS::Noun, 0.6F, "", false, true, false, CT::None, "あさ"},
      {"昼", POS::Noun, 0.6F, "", false, true, false, CT::None, "ひる"},
      {"夜", POS::Noun, 0.6F, "", false, true, false, CT::None, "よる"},
      {"夕方", POS::Noun, 0.6F, "", false, true, false, CT::None, "ゆうがた"},

      // Weeks (週)
      {"今週", POS::Noun, 0.5F, "", false, true, false, CT::None, "こんしゅう"},
      {"来週", POS::Noun, 0.5F, "", false, true, false, CT::None, "らいしゅう"},
      {"先週", POS::Noun, 0.5F, "", false, true, false, CT::None, "せんしゅう"},
      {"毎週", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいしゅう"},

      // Months (月)
      {"今月", POS::Noun, 0.5F, "", false, true, false, CT::None, "こんげつ"},
      {"来月", POS::Noun, 0.5F, "", false, true, false, CT::None, "らいげつ"},
      {"先月", POS::Noun, 0.5F, "", false, true, false, CT::None, "せんげつ"},
      {"毎月", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいつき"},

      // Years (年)
      {"今年", POS::Noun, 0.5F, "", false, true, false, CT::None, "ことし"},
      {"来年", POS::Noun, 0.5F, "", false, true, false, CT::None, "らいねん"},
      {"去年", POS::Noun, 0.5F, "", false, true, false, CT::None, "きょねん"},
      {"昨年", POS::Noun, 0.5F, "", false, true, false, CT::None, "さくねん"},
      {"毎年", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいとし"},

      // Other time expressions
      {"今", POS::Noun, 0.5F, "", false, true, false, CT::None, "いま"},
      {"現在", POS::Noun, 0.5F, "", false, true, false, CT::None, "げんざい"},
      {"最近", POS::Noun, 0.5F, "", false, true, false, CT::None, "さいきん"},
      {"将来", POS::Noun, 0.5F, "", false, true, false, CT::None, "しょうらい"},
      {"過去", POS::Noun, 0.5F, "", false, true, false, CT::None, "かこ"},
      {"未来", POS::Noun, 0.5F, "", false, true, false, CT::None, "みらい"},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_
