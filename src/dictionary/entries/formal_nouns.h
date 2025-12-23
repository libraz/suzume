#ifndef SUZUME_DICTIONARY_ENTRIES_FORMAL_NOUNS_H_
#define SUZUME_DICTIONARY_ENTRIES_FORMAL_NOUNS_H_

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get formal noun entries for core dictionary
 *
 * Formal nouns are function words that look like nouns but have
 * grammatical functions. They are typically excluded from tags.
 *
 * @return Vector of dictionary entries for formal nouns
 */
inline std::vector<DictionaryEntry> getFormalNounEntries() {
  using POS = core::PartOfSpeech;

  return {
      {"こと", POS::Noun, 2.0F, "", false, true, false},   {"もの", POS::Noun, 2.0F, "", false, true, false},
      {"ため", POS::Noun, 2.0F, "", false, true, false},   {"ところ", POS::Noun, 2.0F, "", false, true, false},
      {"よう", POS::Noun, 2.0F, "", false, true, false},   {"ほう", POS::Noun, 2.0F, "", false, true, false},
      {"わけ", POS::Noun, 2.0F, "", false, true, false},   {"はず", POS::Noun, 2.0F, "", false, true, false},
      {"とき", POS::Noun, 2.0F, "", false, true, false},   {"つもり", POS::Noun, 2.0F, "", false, true, false},
      {"まま", POS::Noun, 2.0F, "", false, true, false},   {"うち", POS::Noun, 2.0F, "", false, true, false},
      {"とおり", POS::Noun, 2.0F, "", false, true, false}, {"かぎり", POS::Noun, 2.0F, "", false, true, false},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_FORMAL_NOUNS_H_
