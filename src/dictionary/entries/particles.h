#ifndef SUZUME_DICTIONARY_ENTRIES_PARTICLES_H_
#define SUZUME_DICTIONARY_ENTRIES_PARTICLES_H_

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get particle entries for core dictionary
 * @return Vector of dictionary entries for particles
 */
inline std::vector<DictionaryEntry> getParticleEntries() {
  using POS = core::PartOfSpeech;

  return {
      // Case particles (格助詞) - low cost to ensure recognition
      {"が", POS::Particle, 0.5F, "", false, false, false},
      {"を", POS::Particle, 0.5F, "", false, false, false},
      {"に", POS::Particle, 0.5F, "", false, false, false},
      {"で", POS::Particle, 0.5F, "", false, false, false},
      {"と", POS::Particle, 0.5F, "", false, false, false},
      {"から", POS::Particle, 0.5F, "", false, false, false},
      {"まで", POS::Particle, 0.5F, "", false, false, false},
      {"より", POS::Particle, 0.8F, "", false, false, false},
      {"へ", POS::Particle, 0.5F, "", false, false, false},

      // Binding particles (係助詞) - low cost for common particles
      {"は", POS::Particle, 0.5F, "", false, false, false},
      {"も", POS::Particle, 0.5F, "", false, false, false},
      {"こそ", POS::Particle, 0.8F, "", false, false, false},
      {"さえ", POS::Particle, 0.8F, "", false, false, false},
      {"でも", POS::Particle, 0.8F, "", false, false, false},
      {"しか", POS::Particle, 0.8F, "", false, false, false},

      // Conjunctive particles (接続助詞)
      {"て", POS::Particle, 0.8F, "", false, false, false},
      {"ば", POS::Particle, 0.8F, "", false, false, false},
      {"たら", POS::Particle, 0.8F, "", false, false, false},
      {"ながら", POS::Particle, 0.8F, "", false, false, false},
      {"のに", POS::Particle, 0.8F, "", false, false, false},
      {"ので", POS::Particle, 0.5F, "", false, false, false},  // Reason (because)
      {"けれど", POS::Particle, 0.8F, "", false, false, false},
      {"けど", POS::Particle, 0.8F, "", false, false, false},
      {"けれども", POS::Particle, 0.8F, "", false, false, false},

      // Final particles (終助詞)
      {"か", POS::Particle, 1.0F, "", false, false, false},
      {"な", POS::Particle, 1.0F, "", false, false, false},
      {"ね", POS::Particle, 1.0F, "", false, false, false},
      {"よ", POS::Particle, 1.0F, "", false, false, false},
      {"わ", POS::Particle, 1.0F, "", false, false, false},
      {"の", POS::Particle, 1.0F, "", false, false, false},

      // Adverbial particles (副助詞)
      {"ばかり", POS::Particle, 1.0F, "", false, false, false},
      {"だけ", POS::Particle, 1.0F, "", false, false, false},
      {"ほど", POS::Particle, 1.0F, "", false, false, false},
      {"くらい", POS::Particle, 1.0F, "", false, false, false},
      {"など", POS::Particle, 1.0F, "", false, false, false},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_PARTICLES_H_
