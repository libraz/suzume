#ifndef SUZUME_DICTIONARY_ENTRIES_COMPOUND_PARTICLES_H_
#define SUZUME_DICTIONARY_ENTRIES_COMPOUND_PARTICLES_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Compound Particles (複合助詞) - ~15 entries
//   - Multi-character particles functioning as single units
//   - Grammaticalized from particle + verb/noun combinations
//   - Examples: について, によって, において, として, にとって, etc.
//
// DO NOT add lexical items here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get compound particle entries for core dictionary
 *
 * Compound particles are multi-character particles that function as a unit.
 * They typically derive from particle + verb/noun combinations but are
 * now grammaticalized as single units.
 *
 * @return Vector of dictionary entries for compound particles
 */
std::vector<DictionaryEntry> getCompoundParticleEntries();

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_COMPOUND_PARTICLES_H_
