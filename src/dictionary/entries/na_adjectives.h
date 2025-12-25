#ifndef SUZUME_DICTIONARY_ENTRIES_NA_ADJECTIVES_H_
#define SUZUME_DICTIONARY_ENTRIES_NA_ADJECTIVES_H_

// =============================================================================
// Na-Adjectives (形容動詞) - Layer 1 Hardcoded Entries
// =============================================================================
// High-frequency na-adjectives that form adverbs with 〜に suffix.
// Without these, patterns like "丁寧に" would be incorrectly analyzed.
//
// These are common na-adjectives used in everyday Japanese:
//   - Essential for preventing tokenization errors with 〜に adverb forms
//   - Marked with NaAdjective conjugation type for proper lemmatization
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
 * @brief Get na-adjective entries for core dictionary
 *
 * Na-adjectives can form adverbs with 〜に suffix (e.g., 丁寧に, 慎重に).
 *
 * @return Vector of dictionary entries for na-adjectives
 */
inline std::vector<DictionaryEntry> getNaAdjectiveEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  return {
      // Common na-adjectives - kanji with reading
      {"新た", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "あらた"},
      {"明らか", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "あきらか"},
      {"静か", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "しずか"},
      {"綺麗", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "きれい"},
      {"大切", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "たいせつ"},
      {"大事", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "だいじ"},
      {"必要", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ひつよう"},
      {"簡単", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "かんたん"},
      {"丁寧", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ていねい"},
      {"正確", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "せいかく"},
      {"自然", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "しぜん"},
      {"普通", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ふつう"},
      {"特別", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "とくべつ"},
      {"便利", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "べんり"},
      {"不便", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ふべん"},
      {"有名", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ゆうめい"},
      {"複雑", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ふくざつ"},
      {"単純", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "たんじゅん"},
      {"重要", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "じゅうよう"},
      {"確か", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "たしか"},
      {"様々", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "さまざま"},
      {"勝手", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "かって"},
      {"慎重", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "しんちょう"},
      {"上手", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "じょうず"},

      // Additional na-adjectives for adverb forms (〜に)
      {"非常", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ひじょう"},
      {"本当", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ほんとう"},
      {"頻繁", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ひんぱん"},
      {"確実", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "かくじつ"},
      {"同様", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "どうよう"},
      {"大胆", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "だいたん"},
      {"果敢", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "かかん"},
      {"無理", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "むり"},
      {"強引", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "ごういん"},
      {"地味", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "じみ"},
      {"微妙", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "びみょう"},
      {"唐突", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "とうとつ"},
      {"永遠", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "えいえん"},
      {"永久", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "えいきゅう"},
      {"無限", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "むげん"},
      {"滅多", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "めった"},
      {"一緒", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "いっしょ"},

      // Adverbial na-adjectives (often used with 〜に)
      {"流石", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective,
       "さすが"},

      // Hiragana na-adjectives
      {"だめ", POS::Adjective, 0.3F, "だめ", false, false, false, CT::NaAdjective,
       ""},

      // Business/formal na-adjectives
      {"幸い", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective,
       "さいわい"},

      // Common na-adjectives - emotional/preference
      {"好き", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective,
       "すき"},
      {"嫌い", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective,
       "きらい"},
      {"大好き", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective,
       "だいすき"},
      {"大嫌い", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective,
       "だいきらい"},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_NA_ADJECTIVES_H_
