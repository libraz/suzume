#ifndef SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_
#define SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Determiners (連体詞) - ~10 entries
//   - Words that modify nouns and cannot be conjugated
//   - Demonstrative determiners: この, その, あの, どの
//   - Other determiners: ある, あらゆる, いわゆる, 大きな, 小さな, おかしな
//
// DO NOT add adjectives or other modifiers here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get determiner entries for core dictionary
 *
 * Determiners (連体詞) are words that modify nouns and cannot
 * be conjugated. They always appear before nouns.
 *
 * @return Vector of dictionary entries for determiners
 */
std::vector<DictionaryEntry> getDeterminerEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_
