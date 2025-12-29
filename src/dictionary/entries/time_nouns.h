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

#include <vector>

#include "dictionary/dictionary.h"

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
std::vector<DictionaryEntry> getTimeNounEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_
