#ifndef SUZUME_DICTIONARY_ENTRIES_INTERJECTIONS_H_
#define SUZUME_DICTIONARY_ENTRIES_INTERJECTIONS_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Interjections (感動詞) - ~20 entries
//   - Exclamations and emotional expressions
//   - Examples: ああ, えっ, おい, うわ, へえ, etc.
//   - Standalone words that express emotions or reactions
//
// DO NOT add regular words here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get interjection entries for core dictionary
 *
 * Interjections are standalone emotional expressions that don't
 * connect grammatically to other words.
 *
 * @return Vector of dictionary entries for interjections
 */
std::vector<DictionaryEntry> getInterjectionEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_INTERJECTIONS_H_
