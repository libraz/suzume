#ifndef SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_
#define SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_

// =============================================================================
// I-Adjectives (形容詞) - Layer 1 Hardcoded Entries
// =============================================================================
// Single-kanji + い pattern adjectives that could be confused with Godan verb
// renyokei. Pattern: 寒い (adjective) vs 伴い (verb renyokei)
//
// Without these entries, the unknown word generator skips single-kanji + い
// patterns (to avoid generating false adjectives from verb renyokei like 伴い).
// Real single-kanji i-adjectives must be registered here.
//
// Layer 1 Criteria:
//   - Closed class: Single-kanji + い adjectives are limited in number
//   - Prevents tokenization errors: 寒い → 寒 + い (wrong)
//
// NOT included here (handled by inflection analyzer or L2):
//   - Multi-character patterns: 難しい, 美しい, 楽しい (no confusion risk)
//   - Hiragana adjectives: やばい, すごい (L2 or other entries)
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get i-adjective entries for core dictionary
 *
 * Single-kanji + い adjectives that need explicit dictionary registration
 * to prevent confusion with verb renyokei forms.
 *
 * @return Vector of dictionary entries for i-adjectives
 */
std::vector<DictionaryEntry> getIAdjectiveEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_I_ADJECTIVES_H_
