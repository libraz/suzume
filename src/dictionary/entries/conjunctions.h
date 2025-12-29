#ifndef SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_
#define SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Conjunctions (接続詞) - ~35 entries
//   - Sequential (順接): そして, それから, すると, だから, したがって, etc.
//   - Adversative (逆接): しかし, だが, けれども, ところが, それでも
//   - Parallel/Addition (並列・添加): また, および, ならびに, かつ, etc.
//   - Alternative (選択): または, あるいは, もしくは, それとも
//   - Explanation (説明・補足): つまり, すなわち, たとえば, なぜなら, etc.
//   - Topic change (転換): さて, ところで, では, それでは
//
// Registration Rules:
//   - Register kanji form with reading; hiragana is auto-generated
//   - For hiragana-only entries, leave reading empty
//   - Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
//
// DO NOT add lexical items here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get conjunction entries for core dictionary
 * @return Vector of dictionary entries for conjunctions
 */
std::vector<DictionaryEntry> getConjunctionEntries();

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_
