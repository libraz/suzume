#ifndef SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_
#define SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get conjunction entries for core dictionary
 * @return Vector of dictionary entries for conjunctions
 */
inline std::vector<DictionaryEntry> getConjunctionEntries() {
  using POS = core::PartOfSpeech;

  return {
      // Sequential (順接)
      {"そして", POS::Conjunction, 1.0F, "", false, false, false},
      {"それから", POS::Conjunction, 1.0F, "", false, false, false},
      {"すると", POS::Conjunction, 1.0F, "", false, false, false},
      {"だから", POS::Conjunction, 1.0F, "", false, false, false},
      {"したがって", POS::Conjunction, 1.0F, "", false, false, false},
      {"ゆえに", POS::Conjunction, 1.0F, "", false, false, false},
      {"そのため", POS::Conjunction, 1.0F, "", false, false, false},

      // Adversative (逆接)
      {"しかし", POS::Conjunction, 1.0F, "", false, false, false},
      {"だが", POS::Conjunction, 1.0F, "", false, false, false},
      {"けれども", POS::Conjunction, 1.0F, "", false, false, false},
      {"ところが", POS::Conjunction, 1.0F, "", false, false, false},
      {"それでも", POS::Conjunction, 1.0F, "", false, false, false},

      // Parallel/Addition (並列・添加)
      {"また", POS::Conjunction, 1.0F, "", false, false, false},
      {"および", POS::Conjunction, 1.0F, "", false, false, false},
      {"ならびに", POS::Conjunction, 1.0F, "", false, false, false},
      {"かつ", POS::Conjunction, 1.0F, "", false, false, false},
      {"さらに", POS::Conjunction, 1.0F, "", false, false, false},
      {"しかも", POS::Conjunction, 1.0F, "", false, false, false},
      {"そのうえ", POS::Conjunction, 1.0F, "", false, false, false},

      // Alternative (選択)
      {"または", POS::Conjunction, 1.0F, "", false, false, false},
      {"あるいは", POS::Conjunction, 1.0F, "", false, false, false},
      {"もしくは", POS::Conjunction, 1.0F, "", false, false, false},
      {"それとも", POS::Conjunction, 1.0F, "", false, false, false},

      // Explanation/Supplement (説明・補足)
      {"つまり", POS::Conjunction, 1.0F, "", false, false, false},
      {"すなわち", POS::Conjunction, 1.0F, "", false, false, false},
      {"たとえば", POS::Conjunction, 1.0F, "", false, false, false},
      {"なぜなら", POS::Conjunction, 1.0F, "", false, false, false},
      {"ただし", POS::Conjunction, 1.0F, "", false, false, false},
      {"なお", POS::Conjunction, 1.0F, "", false, false, false},
      {"ちなみに", POS::Conjunction, 1.0F, "", false, false, false},

      // Topic change (転換)
      {"さて", POS::Conjunction, 1.0F, "", false, false, false},
      {"ところで", POS::Conjunction, 1.0F, "", false, false, false},
      {"では", POS::Conjunction, 1.0F, "", false, false, false},
      {"それでは", POS::Conjunction, 1.0F, "", false, false, false},
  };
}

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_
