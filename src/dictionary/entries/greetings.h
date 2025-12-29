#ifndef SUZUME_DICTIONARY_ENTRIES_GREETINGS_H_
#define SUZUME_DICTIONARY_ENTRIES_GREETINGS_H_

// =============================================================================
// Layer 1: Greetings (挨拶) - Closed Class
// =============================================================================
// Status: LAYER 1 (Hardcoded)
//
// This file contains ONLY greetings - fixed expressions that:
//   - Cannot be decomposed meaningfully
//   - Are grammatically frozen
//   - Form a closed class
//
// Layer 1 Criteria:
//   ✅ CLOSED CLASS: Greetings are fixed, non-productive expressions
//   ✅ Grammatically fixed: Cannot be conjugated or modified
//   ✅ Essential for WASM: Basic communication requires these
//
// NOT included here (use Layer 2 - data/core/dictionary.tsv):
//   ❌ お/ご prefix words (お願い, ご連絡, etc.)
//   ❌ Compound nouns (飲み会, 楽しみ, etc.)
//   ❌ Compound verbs (恐れ入る, 差し上げる, etc.)
//   ❌ Business vocabulary
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get greeting entries
 *
 * These are closed-class expressions that should always be recognized
 * as single tokens. They cannot be productively formed or decomposed.
 *
 * @return Vector of dictionary entries for greetings
 */
std::vector<DictionaryEntry> getGreetingEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_GREETINGS_H_
