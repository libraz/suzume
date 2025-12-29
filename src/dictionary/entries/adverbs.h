#ifndef SUZUME_DICTIONARY_ENTRIES_ADVERBS_H_
#define SUZUME_DICTIONARY_ENTRIES_ADVERBS_H_

// =============================================================================
// Common Adverbs (副詞) - Layer 1 Hardcoded Entries
// =============================================================================
// High-frequency adverbs that are essential for proper tokenization.
// Without these, hiragana sequences like "とても" would be incorrectly split.
//
// These are CLOSED CLASS for the purposes of morphological analysis:
//   - Core adverbs that rarely change
//   - Essential for preventing tokenization errors
//
// Registration Rules:
//   - Register kanji form with reading; hiragana is auto-generated
//   - For hiragana-only entries, leave reading empty
//   - Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get common adverb entries for core dictionary
 *
 * @return Vector of dictionary entries for common adverbs
 */
std::vector<DictionaryEntry> getAdverbEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_ADVERBS_H_
