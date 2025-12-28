#ifndef SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_
#define SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Conjunctions (接続詞) - ~35 entries
//   - Sequential (順接): そして, それから, すると, だから, したがって, etc.
//   - Adversative (逆接): しかし, だが, けれども, ところが, それでも
//   - Parallel/Addition (並列・添加): また, および, ならびに, かつ, etc.
//   - Alternative (選択): または, あるいは, もしくは, それとも
//   - Explanation (説明・補足): つまり, すなわち, たとえば, なぜなら, etc.
//   - Topic change (転換): さて, ところで, では, それでは
//
// Registration Rules:
//   - Register kanji form with reading; hiragana is auto-generated
//   - For hiragana-only entries, leave reading empty
//   - Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
//
// DO NOT add lexical items here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

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
  using CT = ConjugationType;

  return {
      // Sequential (順接) - kanji with reading
      {"従って", POS::Conjunction, 0.5F, "", false, false, false, CT::None, "したがって"},
      {"故に", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "ゆえに"},
      // Sequential (順接) - hiragana only
      {"そして", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それから", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"すると", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"だから", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"そのため", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},

      // Adversative (逆接) - all hiragana
      {"しかし", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"だが", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"けれども", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"ところが", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それでも", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"でも", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},
      {"だって", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},
      {"にもかかわらず", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},

      // Parallel/Addition (並列・添加) - kanji with reading
      {"又", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "また"},
      {"及び", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "および"},
      {"並びに", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "ならびに"},
      {"且つ", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "かつ"},
      {"更に", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "さらに"},
      // Parallel/Addition - hiragana only
      {"しかも", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"そのうえ", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},

      // Alternative (選択) - kanji with reading
      {"或いは", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "あるいは"},
      {"若しくは", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "もしくは"},
      // Alternative - hiragana only
      {"または", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それとも", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},

      // Explanation/Supplement (説明・補足) - kanji with reading
      {"即ち", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "すなわち"},
      {"例えば", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "たとえば"},
      {"但し", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "ただし"},
      {"尚", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "なお"},
      // Explanation/Supplement - hiragana only
      {"つまり", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"なぜなら", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"ちなみに", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},

      // Topic change (転換) - all hiragana
      {"さて", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"ところで", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"では", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それでは", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
  };
}

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_CONJUNCTIONS_H_
