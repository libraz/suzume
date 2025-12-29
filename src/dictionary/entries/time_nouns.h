#ifndef SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_
#define SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_

// =============================================================================
// Time Noun Entries (Layer 1 Fallback)
// =============================================================================
// This file contains time-related nouns that are essential for basic
// tokenization when the external dictionary (core.dic) is not available.
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
//   - Time nouns (時間名詞): 今日, 明日, 昨日, 今週, 来月, 今年, etc.
//   - These have high priority (low cost) to ensure proper segmentation
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get time noun entries for core dictionary
 *
 * Time nouns (時間名詞) are words that indicate time periods
 * (今日, 明日, 昨日, 今週, etc.).
 *
 * Used as fallback when core.dic is not available (e.g., WASM builds).
 * These have high priority (low cost) to ensure proper segmentation.
 *
 * @return Vector of dictionary entries for time nouns
 */
std::vector<DictionaryEntry> getTimeNounEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_TIME_NOUNS_H_
