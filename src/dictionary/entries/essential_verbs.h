#ifndef SUZUME_DICTIONARY_ENTRIES_ESSENTIAL_VERBS_H_
#define SUZUME_DICTIONARY_ENTRIES_ESSENTIAL_VERBS_H_

// =============================================================================
// Layer 1: Essential Verbs - Tokenization Critical
// =============================================================================
// Verbs that MUST be in Layer 1 to ensure correct lemmatization.
// Without these entries, the inflection analyzer produces incorrect base forms.
//
// Examples of bugs this fixes:
//   用いて → 用く (wrong, Godan) → 用いる (correct, Ichidan)
//   上げます → 上ぐ (wrong, Godan) → 上げる (correct, Ichidan)
//
// Layer 1 Criteria:
//   ✅ Tokenization critical: Prevents incorrect lemmatization
//   ✅ Ambiguous conjugation: Would be misidentified without dictionary
//   ✅ Essential for WASM: Analysis accuracy requires these
//
// Note: These are NOT hiragana verbs - they have kanji but need L1 registration
// because their conjugation patterns are ambiguous.
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get essential verb entries for core dictionary
 *
 * These verbs have ambiguous conjugation patterns and must be registered
 * to ensure correct lemmatization.
 *
 * @return Vector of dictionary entries for essential verbs
 */
std::vector<DictionaryEntry> getEssentialVerbEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_ESSENTIAL_VERBS_H_
