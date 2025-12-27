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

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

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
inline std::vector<DictionaryEntry> getCompoundParticleEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // Compound particles: grammaticalized forms functioning as single units
  // Hiragana-only forms are strongly grammaticalized and should remain as units
  // Forms with kanji (に基づいて, に限って, etc.) should be analyzed as
  // particle + verb to preserve verb conjugation information
  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // Relation (関連) - hiragana only
      {"について", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Cause/Means (原因・手段) - hiragana only
      {"によって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"により", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"によると", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Place/Situation (場所・状況) - hiragana only
      {"において", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にて", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Capacity/Viewpoint (資格・観点) - hiragana only
      {"として", POS::Particle, 0.1F, "", false, false, false, CT::None, ""},
      {"にとって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Duration/Scope (範囲・期間) - hiragana only
      {"にわたって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にわたり", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にあたって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にあたり", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Topic/Means (話題・手段) - hiragana only
      {"をめぐって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"をめぐり", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"をもって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_COMPOUND_PARTICLES_H_
