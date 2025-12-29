#ifndef SUZUME_DICTIONARY_ENTRIES_NA_ADJECTIVES_H_
#define SUZUME_DICTIONARY_ENTRIES_NA_ADJECTIVES_H_

// =============================================================================
// Na-Adjectives (形容動詞) - Layer 1 Hardcoded Entries
// =============================================================================
// High-frequency na-adjectives that form adverbs with 〜に suffix.
// Without these, patterns like "丁寧に" would be incorrectly analyzed.
//
// These are common na-adjectives used in everyday Japanese:
//   - Essential for preventing tokenization errors with 〜に adverb forms
//   - Marked with NaAdjective conjugation type for proper lemmatization
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
 * @brief Get na-adjective entries for core dictionary
 *
 * Na-adjectives can form adverbs with 〜に suffix (e.g., 丁寧に, 慎重に).
 *
 * @return Vector of dictionary entries for na-adjectives
 */
std::vector<DictionaryEntry> getNaAdjectiveEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_NA_ADJECTIVES_H_
