#ifndef SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_
#define SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Pronouns (代名詞) - ~30 entries
//   - Personal pronouns (人称代名詞): 私, 僕, 俺, あなた, 君, 彼, 彼女, etc.
//   - Demonstrative pronouns (指示代名詞): これ, それ, あれ, ここ, そこ, etc.
//   - Interrogative pronouns (疑問代名詞): 誰, 何, どこ, どれ, いつ, etc.
//   - Typically excluded from tag generation (low semantic content)
//
// Registration Rules:
//   - Register kanji form with reading; hiragana is auto-generated
//   - For hiragana-only entries, leave reading empty
//   - Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
//
// DO NOT add nouns here. For vocabulary, use Layer 2 or Layer 3.
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get pronoun entries for core dictionary
 *
 * Pronouns (代名詞) are words that substitute for nouns.
 * They typically have low information value for tag generation.
 *
 * @return Vector of dictionary entries for pronouns
 */
std::vector<DictionaryEntry> getPronounEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_
