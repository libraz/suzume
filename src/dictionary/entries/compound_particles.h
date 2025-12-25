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

  // Compound particles have low cost (0.3F) to prefer them over split alternatives
  // e.g., "として" should win over "と" + "して" (verb)
  // All are hiragana-only; reading field is empty
  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // Relation (関連)
      {"について", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"に関して", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"に対して", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Cause/Means (原因・手段)
      {"によって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"により", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"によると", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Place/Situation (場所・状況)
      {"において", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にて", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Capacity/Viewpoint (資格・観点)
      {"として", POS::Particle, 0.1F, "", false, false, false, CT::None, ""},
      {"にとって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Range/Limitation (範囲・限定)
      {"に限って", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"に限り", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Basis (根拠)
      {"に基づいて", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"に基づき", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Comparison (比較)
      {"に比べて", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_COMPOUND_PARTICLES_H_
