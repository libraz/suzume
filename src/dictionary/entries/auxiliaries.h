#ifndef SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_
#define SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Auxiliary Verbs (助動詞) - ~25 entries
//   - Assertion (断定): だ, です, である
//   - Polite (丁寧): ます, ました, ません
//   - Negation (否定): ない, ぬ, なかった
//   - Past/Completion (過去・完了): た
//   - Conjecture (推量): う, よう, だろう, でしょう
//   - Desire (願望): たい, たがる
//   - Potential/Passive/Causative: れる, られる, せる, させる
//
// DO NOT add lexical verbs (食べる, 書く, etc.) here.
// For vocabulary, use Layer 2 (core.dic) or Layer 3 (user.dic).
// =============================================================================

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
  using CT = ConjugationType;

  // All auxiliaries are hiragana-only; reading field is empty
  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // Assertion (断定) - Very low cost to prioritize over particle + verb splits
      // でした must beat で(particle) + した(verb) combination
      // であった must beat である + た or other splits
      {"だ", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"だった", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"だったら", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"です", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"でした", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"である", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"であった", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"であれば", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"でしたら", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},

      // Polite (丁寧)
      {"ます", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"ました", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"ません", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Negation (否定)
      {"ない", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"ぬ", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"なかった", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Past/Completion (過去・完了)
      {"た", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Conjecture (推量)
      {"う", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"よう", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"だろう", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"でしょう", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Possibility/Uncertainty (可能性・不確実) - かもしれない forms
      // Without these, "もしれません" is incorrectly parsed as verb "もしれる"
      {"かもしれない", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},
      {"かもしれません", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},
      {"かもしれなかった", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},

      // Desire (願望)
      {"たい", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"たがる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Potential/Passive/Causative (可能・受身・使役)
      {"れる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"られる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"せる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"させる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Polite existence (丁寧存在) - ございます conjugations
      {"ございます", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございました", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございましたら", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございません", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},

      // Request (依頼) - ください
      {"ください", POS::Auxiliary, 0.3F, "ください", false, false, false, CT::None, ""},
      {"くださいませ", POS::Auxiliary, 0.3F, "ください", false, false, false, CT::None, ""},

      // Explanatory (説明) - のだ/んだ forms
      {"のだ", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"のです", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"のでした", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んだ", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んです", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んでした", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
  };
}

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_AUXILIARIES_H_
