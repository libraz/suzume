#ifndef SUZUME_DICTIONARY_ENTRIES_COMMON_VOCABULARY_H_
#define SUZUME_DICTIONARY_ENTRIES_COMMON_VOCABULARY_H_

// =============================================================================
// Layer 1: Common Vocabulary - Tokenization Critical
// =============================================================================
// High-frequency vocabulary that MUST be in Layer 1 to prevent incorrect
// tokenization. Without these entries, the analyzer incorrectly splits them.
//
// Examples of bugs this fixes:
//   飲み会 → 飲 + み + 会 (wrong) → 飲み会 (correct)
//   楽しみ → 楽 + しみ (wrong) → 楽しみ (correct)
//
// Note: お/ご + noun patterns are now handled as PREFIX + NOUN
// (e.g., お茶 → お(PREFIX) + 茶(NOUN))
//
// Layer 1 Criteria:
//   ✅ Tokenization critical: Prevents incorrect splitting
//   ✅ High frequency: Common in everyday Japanese
//   ✅ Essential for WASM: Analysis accuracy requires these
//
// Note: These are open-class vocabulary, but included in L1 for correctness.
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get common vocabulary entries for core dictionary
 *
 * These are high-frequency words that would be incorrectly tokenized
 * without dictionary entries.
 *
 * @return Vector of dictionary entries for common vocabulary
 */
std::vector<DictionaryEntry> getCommonVocabularyEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_COMMON_VOCABULARY_H_
