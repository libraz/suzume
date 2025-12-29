#ifndef SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_
#define SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Auxiliary Verbs (助動詞) - ~25 entries
//   - Assertion (断定): だ, です, である
//   - Polite (丁寧): ます, ました, ません
//   - Negation (否定): ない, ぬ, なかった
//   - Past/Completion (過去・完了): た
//   - Conjecture (推量): う, よう, だろう, でしょう
//   - Desire (願望): たい, たがる
//   - Potential/Passive/Causative: れる, られる, せる, させる
//
// DO NOT add lexical verbs (食べる, 書く, etc.) here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get auxiliary verb entries for core dictionary
 * @return Vector of dictionary entries for auxiliary verbs
 */
std::vector<DictionaryEntry> getAuxiliaryEntries();

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_
