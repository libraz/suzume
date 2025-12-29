#ifndef SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_
#define SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_

// =============================================================================
// ⚠️ DEPRECATED: Entries migrated to Layer 2 (data/core/dictionary.tsv)
// =============================================================================
// This file contains OPEN CLASS vocabulary that does not belong in Layer 1:
//   - Verbs (動詞): Open class - new verbs can be added to the language
//   - Suffixes (接尾語): Open class - productive and extensible
//
// Current status: WASM fallback (entries also in dictionary.tsv)
//   - Kept for WASM minimal builds where core.dic is not available
//   - Native builds with core.dic will have duplicate entries (same cost)
//   - Canonical source: data/core/dictionary.tsv (section: low_info.h migration)
//
// Migration status: COMPLETED (2025-12-27)
//   - All entries copied to data/core/dictionary.tsv
//   - This file remains for WASM fallback compatibility
//   - TODO: Add #ifdef SUZUME_WASM conditional compilation
//
// Layer 1 Criteria (this file DOES NOT meet):
//   ❌ CLOSED CLASS: Verbs and suffixes are open, productive classes
//   ❌ Grammatically fixed: New verbs/suffixes appear regularly
//   ✅ Needed for WASM: Only reason this file exists
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get low information word entries for core dictionary
 *
 * @deprecated This function provides WASM fallback entries.
 * For native builds, use core.dic instead.
 *
 * Low information words are common verbs and suffixes that
 * carry little semantic meaning on their own.
 *
 * @return Vector of dictionary entries for low information words
 */
std::vector<DictionaryEntry> getLowInfoEntries();

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_
