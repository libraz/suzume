#ifndef SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_
#define SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get auxiliary verb entries for core dictionary
 * @return Vector of dictionary entries for auxiliary verbs
 */
inline std::vector<DictionaryEntry> getAuxiliaryEntries() {
  using POS = core::PartOfSpeech;

  return {
      // Assertion (断定)
      {"だ", POS::Auxiliary, 1.0F, "", false, false, false},
      {"です", POS::Auxiliary, 1.0F, "", false, false, false},
      {"である", POS::Auxiliary, 1.0F, "", false, false, false},

      // Polite (丁寧)
      {"ます", POS::Auxiliary, 1.0F, "", false, false, false},
      {"ました", POS::Auxiliary, 1.0F, "", false, false, false},
      {"ません", POS::Auxiliary, 1.0F, "", false, false, false},

      // Negation (否定)
      {"ない", POS::Auxiliary, 1.0F, "", false, false, false},
      {"ぬ", POS::Auxiliary, 1.0F, "", false, false, false},
      {"なかった", POS::Auxiliary, 1.0F, "", false, false, false},

      // Past/Completion (過去・完了)
      {"た", POS::Auxiliary, 1.0F, "", false, false, false},

      // Conjecture (推量)
      {"う", POS::Auxiliary, 1.0F, "", false, false, false},
      {"よう", POS::Auxiliary, 1.0F, "", false, false, false},
      {"だろう", POS::Auxiliary, 1.0F, "", false, false, false},
      {"でしょう", POS::Auxiliary, 1.0F, "", false, false, false},

      // Desire (願望)
      {"たい", POS::Auxiliary, 1.0F, "", false, false, false},
      {"たがる", POS::Auxiliary, 1.0F, "", false, false, false},

      // Potential/Passive/Causative (可能・受身・使役)
      {"れる", POS::Auxiliary, 1.0F, "", false, false, false},
      {"られる", POS::Auxiliary, 1.0F, "", false, false, false},
      {"せる", POS::Auxiliary, 1.0F, "", false, false, false},
      {"させる", POS::Auxiliary, 1.0F, "", false, false, false},
  };
}

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_
