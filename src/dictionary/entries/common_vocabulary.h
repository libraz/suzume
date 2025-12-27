#ifndef SUZUME_DICTIONARY_ENTRIES_COMMON_VOCABULARY_H_
#define SUZUME_DICTIONARY_ENTRIES_COMMON_VOCABULARY_H_

// =============================================================================
// Layer 1: Common Vocabulary - Tokenization Critical
// =============================================================================
// High-frequency vocabulary that MUST be in Layer 1 to prevent incorrect
// tokenization. Without these entries, the analyzer incorrectly splits them.
//
// Examples of bugs this fixes:
//   飲み会 → 飲 + み + 会 (wrong) → 飲み会 (correct)
//   楽しみ → 楽 + しみ (wrong) → 楽しみ (correct)
//   お願い → お + 願 + い (wrong) → お願い (correct)
//
// Layer 1 Criteria:
//   ✅ Tokenization critical: Prevents incorrect splitting
//   ✅ High frequency: Common in everyday Japanese
//   ✅ Essential for WASM: Analysis accuracy requires these
//
// Note: These are open-class vocabulary, but included in L1 for correctness.
// =============================================================================

#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get common vocabulary entries for core dictionary
 *
 * These are high-frequency words that would be incorrectly tokenized
 * without dictionary entries.
 *
 * @return Vector of dictionary entries for common vocabulary
 */
inline std::vector<DictionaryEntry> getCommonVocabularyEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // ========================================
      // Compound nouns (複合名詞)
      // ========================================
      {"飲み会", POS::Noun, 0.3F, "", false, false, false, CT::None, "のみかい"},
      {"楽しみ", POS::Noun, 0.3F, "", false, false, false, CT::None, "たのしみ"},
      {"食べ物", POS::Noun, 0.3F, "", false, false, false, CT::None, "たべもの"},
      {"飲み物", POS::Noun, 0.3F, "", false, false, false, CT::None, "のみもの"},
      {"買い物", POS::Noun, 0.3F, "", false, false, false, CT::None, "かいもの"},

      // ========================================
      // お + verb stem patterns (polite expressions)
      // These are common polite nouns derived from お + verb stem
      // ========================================
      {"お願い", POS::Noun, 0.5F, "", false, false, false, CT::None, "おねがい"},
      {"お疲れ", POS::Noun, 0.5F, "", false, false, false, CT::None, "おつかれ"},
      {"お疲れ様", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おつかれさま"},
      {"お問い合わせ", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おといあわせ"},
      {"お待ち", POS::Noun, 0.5F, "", false, false, false, CT::None, "おまち"},
      {"お知らせ", POS::Noun, 0.5F, "", false, true, false, CT::None,
       "おしらせ"},
      {"お申し込み", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おもうしこみ"},
      {"お支払い", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おしはらい"},
      {"お届け", POS::Noun, 0.5F, "", false, false, false, CT::None, "おとどけ"},
      {"お受け取り", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おうけとり"},
      {"お買い上げ", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おかいあげ"},
      {"お預かり", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おあずかり"},
      {"お見舞い", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おみまい"},
      {"お迎え", POS::Noun, 0.5F, "", false, false, false, CT::None, "おむかえ"},
      {"お答え", POS::Noun, 0.5F, "", false, false, false, CT::None, "おこたえ"},
      {"お断り", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おことわり"},
      {"お帰り", POS::Noun, 0.5F, "", false, false, false, CT::None, "おかえり"},
      {"お呼び", POS::Noun, 0.5F, "", false, false, false, CT::None, "およびだし"},
      {"お引き取り", POS::Noun, 0.5F, "", false, false, false, CT::None,
       "おひきとり"},

      // ========================================
      // Lexicalized ご + noun
      // ========================================
      {"ご飯", POS::Noun, 0.5F, "", false, false, false, CT::None, "ごはん"},
      {"ご覧", POS::Noun, 0.5F, "", false, false, false, CT::None, "ごらん"},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_COMMON_VOCABULARY_H_
