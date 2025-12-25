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
  using CT = ConjugationType;

  // All particles are hiragana-only; reading field is empty
  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // Case particles (格助詞) - low cost to ensure recognition
      {"が", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"を", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"に", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"で", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"と", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"から", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"まで", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"より", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"へ", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},

      // Binding particles (係助詞) - low cost for common particles
      {"は", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"も", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"こそ", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"さえ", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"でも", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"しか", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},

      // Conjunctive particles (接続助詞)
      {"て", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"ば", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"たら", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"ながら", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"のに", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"ので", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},  // Reason
      {"けれど", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"けど", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"けれども", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},

      // Final particles (終助詞)
      {"か", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"な", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"ね", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"よ", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"わ", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"の", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},

      // Adverbial particles (副助詞)
      {"ばかり", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"だけ", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"ほど", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"くらい", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"など", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_PARTICLES_H_
