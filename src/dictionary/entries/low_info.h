#ifndef SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_
#define SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_

// =============================================================================
// Low Information Word Entries (Layer 1 Fallback)
// =============================================================================
// This file contains common verbs and suffixes that are essential for
// basic tokenization when the external dictionary (core.dic) is not available.
//
// Usage:
//   - WASM builds: Primary source (core.dic file access not available)
//   - Native builds: Fallback (same entries also in core.dic, no conflict)
//
// Canonical data source: data/core/dictionary.tsv
//   - Both this file and core.dic are generated from the same TSV source
//   - Duplicate entries have identical costs, causing no scoring issues
//
// Contents:
//   - Verbs (動詞): Common low-information verbs (する, いる, ある, etc.)
//   - Suffixes (接尾語): Common productive suffixes
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get low information word entries for core dictionary
 *
 * Low information words are common verbs and suffixes that
 * carry little semantic meaning on their own (する, いる, ある, etc.).
 *
 * Used as fallback when core.dic is not available (e.g., WASM builds).
 *
 * @return Vector of dictionary entries for low information words
 */
std::vector<DictionaryEntry> getLowInfoEntries();

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_
