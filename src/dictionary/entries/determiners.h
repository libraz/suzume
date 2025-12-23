#ifndef SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_
#define SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get determiner entries for core dictionary
 *
 * Determiners (連体詞) are words that modify nouns and cannot
 * be conjugated. They always appear before nouns.
 *
 * @return Vector of dictionary entries for determiners
 */
inline std::vector<DictionaryEntry> getDeterminerEntries() {
  using POS = core::PartOfSpeech;

  return {
      // Demonstrative determiners (指示連体詞)
      {"この", POS::Determiner, 1.0F, "", false, false, false},
      {"その", POS::Determiner, 1.0F, "", false, false, false},
      {"あの", POS::Determiner, 1.0F, "", false, false, false},
      {"どの", POS::Determiner, 1.0F, "", false, false, false},

      // Other determiners (連体詞)
      {"ある", POS::Determiner, 1.0F, "", false, false, false},
      {"あらゆる", POS::Determiner, 1.0F, "", false, false, false},
      {"いわゆる", POS::Determiner, 1.0F, "", false, false, false},
      {"大きな", POS::Determiner, 1.0F, "", false, false, false},
      {"小さな", POS::Determiner, 1.0F, "", false, false, false},
      {"おかしな", POS::Determiner, 1.0F, "", false, false, false},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_
