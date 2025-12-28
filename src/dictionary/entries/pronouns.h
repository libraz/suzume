#ifndef SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_
#define SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_

// =============================================================================
// Layer 1: Hardcoded Dictionary Entry (entries/*.h)
// =============================================================================
// Classification Criteria:
//   - CLOSED CLASS: Grammatically fixed set with known upper bound
//   - Rarely changes (tied to language structure, not vocabulary)
//   - Required for WASM minimal builds
//
// This file: Pronouns (代名詞) - ~30 entries
//   - Personal pronouns (人称代名詞): 私, 僕, 俺, あなた, 君, 彼, 彼女, etc.
//   - Demonstrative pronouns (指示代名詞): これ, それ, あれ, ここ, そこ, etc.
//   - Interrogative pronouns (疑問代名詞): 誰, 何, どこ, どれ, いつ, etc.
//   - Typically excluded from tag generation (low semantic content)
//
// Registration Rules:
//   - Register kanji form with reading; hiragana is auto-generated
//   - For hiragana-only entries, leave reading empty
//   - Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
//
// DO NOT add nouns here. For vocabulary, use Layer 2 or Layer 3.
// =============================================================================

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
  using CT = ConjugationType;

  return {
      // First person (一人称) - kanji with reading
      {"私", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "わたし"},
      {"僕", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "ぼく"},
      {"俺", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "おれ"},
      // First person - hiragana only
      {"わたくし", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // First person plural (一人称複数)
      {"私たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "わたしたち"},
      {"僕たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "ぼくたち"},
      {"僕ら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "ぼくら"},
      {"俺たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "おれたち"},
      {"俺ら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "おれら"},
      {"我々", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "われわれ"},

      // Second person (二人称) - kanji with reading
      {"貴方", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "あなた"},
      {"君", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "きみ"},
      // Second person - hiragana/mixed only
      {"あなた", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"お前", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "おまえ"},

      // Second person plural (二人称複数)
      {"あなたたち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"君たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "きみたち"},

      // Third person (三人称) - kanji with reading
      {"彼", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かれ"},
      {"彼女", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かのじょ"},
      {"彼ら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かれら"},
      {"彼女ら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かのじょら"},
      {"彼女たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かのじょたち"},

      // Archaic/Samurai (武家・古風)
      {"拙者", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "せっしゃ"},
      {"貴殿", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "きでん"},
      {"某", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "それがし"},
      {"我輩", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "わがはい"},
      {"吾輩", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "わがはい"},

      // Collective pronouns (集合代名詞)
      {"皆", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "みな"},
      {"皆さん", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "みなさん"},
      {"みんな", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - proximal (近称) - hiragana only
      {"これ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"ここ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"こちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - medial (中称) - hiragana only
      {"それ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"そこ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"そちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - distal (遠称) - hiragana only
      // あれ cost tuning: 0.8 + low_info=false balances standalone vs conditional
      // - Standalone: pronoun word_cost=-0.1 < verb word_cost=0 (pronoun wins)
      // - With ば: pronoun+ば path > verb あれば path (verb wins)
      {"あれ", POS::Pronoun, 0.8F, "", false, false, false, CT::None, ""},
      {"あそこ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"あちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - interrogative (不定称) - hiragana only
      {"どれ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どこ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Indefinite (不定代名詞) - kanji with reading
      {"誰", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "だれ"},
      {"何", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "なに"},

      // Interrogatives (疑問詞) - hiragana only
      {"いつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"いくつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"いくら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どう", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
      {"どうして", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
      {"なぜ", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
      {"どんな", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_PRONOUNS_H_
