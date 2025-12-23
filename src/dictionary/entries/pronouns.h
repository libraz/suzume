#ifndef SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_
#define SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get pronoun entries for core dictionary
 *
 * Pronouns (代名詞) are words that substitute for nouns.
 * They typically have low information value for tag generation.
 *
 * @return Vector of dictionary entries for pronouns
 */
inline std::vector<DictionaryEntry> getPronounEntries() {
  using POS = core::PartOfSpeech;

  return {
      // First person (一人称)
      {"私", POS::Pronoun, 0.5F, "", false, false, true},
      {"僕", POS::Pronoun, 0.5F, "", false, false, true},
      {"俺", POS::Pronoun, 0.5F, "", false, false, true},
      {"わたくし", POS::Pronoun, 0.5F, "", false, false, true},

      // Second person (二人称)
      {"あなた", POS::Pronoun, 0.5F, "", false, false, true},
      {"君", POS::Pronoun, 0.5F, "", false, false, true},
      {"お前", POS::Pronoun, 0.5F, "", false, false, true},

      // Third person (三人称)
      {"彼", POS::Pronoun, 0.5F, "", false, false, true},
      {"彼女", POS::Pronoun, 0.5F, "", false, false, true},
      {"彼ら", POS::Pronoun, 0.5F, "", false, false, true},
      {"彼女ら", POS::Pronoun, 0.5F, "", false, false, true},

      // Demonstrative - proximal (近称)
      {"これ", POS::Pronoun, 0.5F, "", false, false, true},
      {"ここ", POS::Pronoun, 0.5F, "", false, false, true},
      {"こちら", POS::Pronoun, 0.5F, "", false, false, true},

      // Demonstrative - medial (中称)
      {"それ", POS::Pronoun, 0.5F, "", false, false, true},
      {"そこ", POS::Pronoun, 0.5F, "", false, false, true},
      {"そちら", POS::Pronoun, 0.5F, "", false, false, true},

      // Demonstrative - distal (遠称)
      {"あれ", POS::Pronoun, 0.5F, "", false, false, true},
      {"あそこ", POS::Pronoun, 0.5F, "", false, false, true},
      {"あちら", POS::Pronoun, 0.5F, "", false, false, true},

      // Demonstrative - interrogative (不定称)
      {"どれ", POS::Pronoun, 0.5F, "", false, false, true},
      {"どこ", POS::Pronoun, 0.5F, "", false, false, true},
      {"どちら", POS::Pronoun, 0.5F, "", false, false, true},

      // Indefinite (不定代名詞)
      {"誰", POS::Pronoun, 0.5F, "", false, false, true},
      {"何", POS::Pronoun, 0.5F, "", false, false, true},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_
