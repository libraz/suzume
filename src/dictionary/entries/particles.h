#ifndef SUZUME_DICTIONARY_ENTRIES_PARTICLES_H_
#define SUZUME_DICTIONARY_ENTRIES_PARTICLES_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Particles (助詞) - ~35 entries
//   - Case particles (格助詞): が, を, に, で, と, から, まで, より, へ
//   - Binding particles (係助詞): は, も, こそ, さえ, でも, しか
//   - Conjunctive particles (接続助詞): て, ば, たら, ながら, のに, etc.
//   - Final particles (終助詞): か, な, ね, よ, わ, の
//   - Adverbial particles (副助詞): ばかり, だけ, ほど, くらい, など
//
// DO NOT add lexical items (nouns, verbs, adjectives) here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get particle entries for core dictionary
 * @return Vector of dictionary entries for particles
 */
std::vector<DictionaryEntry> getParticleEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_PARTICLES_H_
