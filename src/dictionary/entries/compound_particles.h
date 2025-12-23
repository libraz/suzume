#ifndef SUZUME_DICTIONARY_ENTRIES_COMPOUND_PARTICLES_H_
#define SUZUME_DICTIONARY_ENTRIES_COMPOUND_PARTICLES_H_

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

  return {
      // Relation (関連)
      {"について", POS::Particle, 1.0F, "", false, false, false},
      {"に関して", POS::Particle, 1.0F, "", false, false, false},
      {"に対して", POS::Particle, 1.0F, "", false, false, false},

      // Cause/Means (原因・手段)
      {"によって", POS::Particle, 1.0F, "", false, false, false},
      {"により", POS::Particle, 1.0F, "", false, false, false},
      {"によると", POS::Particle, 1.0F, "", false, false, false},

      // Place/Situation (場所・状況)
      {"において", POS::Particle, 1.0F, "", false, false, false},
      {"にて", POS::Particle, 1.0F, "", false, false, false},

      // Capacity/Viewpoint (資格・観点)
      {"として", POS::Particle, 1.0F, "", false, false, false},
      {"にとって", POS::Particle, 1.0F, "", false, false, false},

      // Range/Limitation (範囲・限定)
      {"に限って", POS::Particle, 1.0F, "", false, false, false},
      {"に限り", POS::Particle, 1.0F, "", false, false, false},

      // Basis (根拠)
      {"に基づいて", POS::Particle, 1.0F, "", false, false, false},
      {"に基づき", POS::Particle, 1.0F, "", false, false, false},

      // Comparison (比較)
      {"に比べて", POS::Particle, 1.0F, "", false, false, false},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_COMPOUND_PARTICLES_H_
