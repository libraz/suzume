#ifndef SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_
#define SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Determiners (連体詞) - ~10 entries
//   - Words that modify nouns and cannot be conjugated
//   - Demonstrative determiners: この, その, あの, どの
//   - Other determiners: ある, あらゆる, いわゆる, 大きな, 小さな, おかしな
//
// DO NOT add adjectives or other modifiers here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

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
  using CT = ConjugationType;

  // Registration: kanji + reading format; hiragana auto-generated
  // For hiragana-only entries, leave reading empty
  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // Demonstrative determiners (指示連体詞) - hiragana only
      {"この", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"その", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"あの", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"どの", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},

      // Other determiners (連体詞) - hiragana only
      {"ある", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"あらゆる", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"いわゆる", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"おかしな", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},

      // Demonstrative manner determiners (指示様態連体詞)
      {"こういう", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"そういう", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"ああいう", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"どういう", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},

      // Determiners with kanji - register kanji with reading
      {"大きな", POS::Determiner, 1.0F, "", false, false, false, CT::None, "おおきな"},
      {"小さな", POS::Determiner, 1.0F, "", false, false, false, CT::None, "ちいさな"},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_DETERMINERS_H_
