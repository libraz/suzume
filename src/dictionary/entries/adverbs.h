#ifndef SUZUME_DICTIONARY_ENTRIES_ADVERBS_H_
#define SUZUME_DICTIONARY_ENTRIES_ADVERBS_H_

// =============================================================================
// Common Adverbs (副詞) - Layer 1 Hardcoded Entries
// =============================================================================
// High-frequency adverbs that are essential for proper tokenization.
// Without these, hiragana sequences like "とても" would be incorrectly split.
//
// These are CLOSED CLASS for the purposes of morphological analysis:
//   - Core adverbs that rarely change
//   - Essential for preventing tokenization errors
//
// Registration Rules:
//   - Register kanji form with reading; hiragana is auto-generated
//   - For hiragana-only entries, leave reading empty
//   - Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
// =============================================================================

#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get common adverb entries for core dictionary
 *
 * @return Vector of dictionary entries for common adverbs
 */
inline std::vector<DictionaryEntry> getAdverbEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  return {
      // Degree adverbs (程度副詞) - kanji with reading
      {"全く", POS::Adverb, 0.5F, "", false, false, false, CT::None, "まったく"},
      {"全然", POS::Adverb, 0.5F, "", false, false, false, CT::None, "ぜんぜん"},
      {"少し", POS::Adverb, 0.5F, "", false, false, false, CT::None, "すこし"},
      {"結構", POS::Adverb, 0.5F, "", false, false, false, CT::None, "けっこう"},
      // Degree adverbs - hiragana only
      {"とても", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"すごく", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"めっちゃ", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},  // colloquial
      {"かなり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"もっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ずっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"さらに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"まさに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"あまり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"なかなか", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ほとんど", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ちょっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Manner adverbs (様態副詞) - hiragana only
      {"ゆっくり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"しっかり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"はっきり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"きちんと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ちゃんと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Time adverbs (時間副詞) - hiragana only
      {"すぐ", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"すぐに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      // Time adverbs (時間副詞) - kanji with reading
      {"今すぐ", POS::Adverb, 0.5F, "", false, false, false, CT::None, "いますぐ"},
      {"まだ", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"もう", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"やっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ついに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"いつも", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たまに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"よく", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たびたび", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Affirmation/Negation adverbs - kanji with reading
      {"必ず", POS::Adverb, 0.5F, "", false, false, false, CT::None, "かならず"},
      {"決して", POS::Adverb, 0.5F, "", false, false, false, CT::None, "けっして"},
      // Affirmation/Negation - hiragana only
      {"きっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たぶん", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"おそらく", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Other common adverbs - kanji with reading
      {"実は", POS::Adverb, 0.5F, "", false, false, false, CT::None, "じつは"},
      {"特に", POS::Adverb, 0.5F, "", false, false, false, CT::None, "とくに"},
      {"主に", POS::Adverb, 0.5F, "", false, false, false, CT::None, "おもに"},
      // Other - hiragana only
      {"やはり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"やっぱり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Formal/Business adverbs (敬語・ビジネス)
      {"何卒", POS::Adverb, 0.3F, "", false, false, false, CT::None, "なにとぞ"},
      {"誠に", POS::Adverb, 0.3F, "", false, false, false, CT::None, "まことに"},
      {"甚だ", POS::Adverb, 0.5F, "", false, false, false, CT::None, "はなはだ"},
      {"恐縮", POS::Noun, 0.3F, "", false, false, false, CT::None, "きょうしゅく"},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_ADVERBS_H_
