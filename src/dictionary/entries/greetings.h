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

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get greeting entries
 *
 * These are closed-class expressions that should always be recognized
 * as single tokens. They cannot be productively formed or decomposed.
 *
 * @return Vector of dictionary entries for greetings
 */
inline std::vector<DictionaryEntry> getGreetingEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // All greetings are hiragana-only; reading field is empty
  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // ===== Basic Greetings (基本挨拶) =====
      {"こんにちは", POS::Other, 0.3F, "こんにちは", false, false, false, CT::None, ""},
      {"こんばんは", POS::Other, 0.3F, "こんばんは", false, false, false, CT::None, ""},
      {"おはよう", POS::Other, 0.3F, "おはよう", false, false, false, CT::None, ""},
      {"おやすみ", POS::Other, 0.3F, "おやすみ", false, false, false, CT::None, ""},
      {"さようなら", POS::Other, 0.3F, "さようなら", false, false, false, CT::None, ""},

      // ===== Thanks and Apologies (感謝・謝罪) =====
      {"ありがとう", POS::Other, 0.3F, "ありがとう", false, false, false, CT::None, ""},
      {"すみません", POS::Other, 0.3F, "すみません", false, false, false, CT::None, ""},
      {"ごめんなさい", POS::Other, 0.3F, "ごめんなさい", false, false, false, CT::None, ""},
      {"ごめん", POS::Other, 0.3F, "ごめん", false, false, false, CT::None, ""},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_GREETINGS_H_
