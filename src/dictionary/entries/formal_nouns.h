#ifndef SUZUME_DICTIONARY_ENTRIES_FORMAL_NOUNS_H_
#define SUZUME_DICTIONARY_ENTRIES_FORMAL_NOUNS_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Formal Nouns (形式名詞) - ~15 entries
//   - Grammaticalized nouns with function word behavior
//   - Examples: こと, もの, ため, ところ, よう, ほう, わけ, はず, etc.
//   - Typically excluded from tag generation (low semantic content)
//
// Registration Rules:
//   - Register kanji form with reading; hiragana is auto-generated
//   - For hiragana-only entries, leave reading empty
//   - Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
//
// DO NOT add regular nouns (山, 川, 車, etc.) here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

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
  using CT = ConjugationType;

  return {
      // Kanji forms with reading (hiragana auto-generated)
      // Note: Formal nouns need lower cost than particle combinations
      // e.g., "もの" must beat "も"(0.5) + "の"(1.0) after Viterbi scoring
      // Low cost (0.3) compensates for connection costs in Viterbi
      {"事", POS::Noun, 0.3F, "", false, true, false, CT::None, "こと"},
      {"物", POS::Noun, 0.3F, "", false, true, false, CT::None, "もの"},
      {"為", POS::Noun, 2.0F, "", false, true, false, CT::None, "ため"},
      {"所", POS::Noun, 2.0F, "", false, true, false, CT::None, "ところ"},
      {"時", POS::Noun, 2.0F, "", false, true, false, CT::None, "とき"},
      {"内", POS::Noun, 2.0F, "", false, true, false, CT::None, "うち"},
      {"通り", POS::Noun, 2.0F, "", false, true, false, CT::None, "とおり"},
      {"限り", POS::Noun, 2.0F, "", false, true, false, CT::None, "かぎり"},
      // Suffix-like formal nouns (prevent merging with preceding nouns)
      {"付け", POS::Noun, 0.3F, "", false, true, false, CT::None, "つけ"},
      {"付", POS::Noun, 0.5F, "", false, true, false, CT::None, "つけ"},
      // Hiragana-only forms (no common kanji equivalent)
      {"よう", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"ほう", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"わけ", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"はず", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"つもり", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"まま", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_FORMAL_NOUNS_H_
