#ifndef SUZUME_DICTIONARY_ENTRIES_ESSENTIAL_VERBS_H_
#define SUZUME_DICTIONARY_ENTRIES_ESSENTIAL_VERBS_H_

// =============================================================================
// Layer 1: Essential Verbs - Tokenization Critical
// =============================================================================
// Verbs that MUST be in Layer 1 to ensure correct lemmatization.
// Without these entries, the inflection analyzer produces incorrect base forms.
//
// Examples of bugs this fixes:
//   用いて → 用く (wrong, Godan) → 用いる (correct, Ichidan)
//   上げます → 上ぐ (wrong, Godan) → 上げる (correct, Ichidan)
//
// Layer 1 Criteria:
//   ✅ Tokenization critical: Prevents incorrect lemmatization
//   ✅ Ambiguous conjugation: Would be misidentified without dictionary
//   ✅ Essential for WASM: Analysis accuracy requires these
//
// Note: These are NOT hiragana verbs - they have kanji but need L1 registration
// because their conjugation patterns are ambiguous.
// =============================================================================

#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume::dictionary::entries {

/**
 * @brief Get essential verb entries for core dictionary
 *
 * These verbs have ambiguous conjugation patterns and must be registered
 * to ensure correct lemmatization.
 *
 * @return Vector of dictionary entries for essential verbs
 */
inline std::vector<DictionaryEntry> getEssentialVerbEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  // Only base forms registered - conjugation handled by inflection analyzer
  return {
      // ========================================
      // Ichidan verbs often confused with Godan
      // ========================================
      // 用いる: Ichidan, not GodanKa (用いて → 用いる, not 用く)
      {"用いる", POS::Verb, 0.3F, "用いる", false, false, false, CT::Ichidan,
       "もちいる"},

      // 上げる: Ichidan, not GodanGa (上げます → 上げる, not 上ぐ)
      // Used frequently as auxiliary verb (〜てあげる)
      {"上げる", POS::Verb, 0.3F, "上げる", false, false, false, CT::Ichidan,
       "あげる"},

      // 下げる: Ichidan, pair with 上げる
      {"下げる", POS::Verb, 0.3F, "下げる", false, false, false, CT::Ichidan,
       "さげる"},

      // 上がる: GodanRa (intransitive pair of 上げる)
      // Critical for preventing "上がらない" → "上" + "が" + "らない"
      {"上がる", POS::Verb, 0.3F, "上がる", false, false, false, CT::GodanRa,
       "あがる"},

      // 下がる: GodanRa (intransitive pair of 下げる)
      {"下がる", POS::Verb, 0.3F, "下がる", false, false, false, CT::GodanRa,
       "さがる"},

      // ========================================
      // Godan verbs with special patterns
      // ========================================
      // 伴う: GodanWa (伴い is commonly used in "〜に伴い" pattern)
      {"伴う", POS::Verb, 0.3F, "伴う", false, false, false, CT::GodanWa,
       "ともなう"},
      // Renyokei used as conjunction-like (similar to compound particles)
      {"伴い", POS::Verb, 0.3F, "伴う", false, false, false, CT::GodanWa,
       "ともない"},

      // ========================================
      // Compound verbs (敬語複合動詞)
      // ========================================
      // 恐れ入る: GodanRa (入る as いる follows Godan: 恐れ入ります)
      {"恐れ入る", POS::Verb, 0.3F, "恐れ入る", false, false, false, CT::GodanRa,
       "おそれいる"},

      // 申し上げる: Ichidan (humble form of 言う)
      {"申し上げる", POS::Verb, 0.3F, "申し上げる", false, false, false,
       CT::Ichidan, "もうしあげる"},

      // 差し上げる: Ichidan (humble form of あげる/やる)
      {"差し上げる", POS::Verb, 0.3F, "差し上げる", false, false, false,
       CT::Ichidan, "さしあげる"},

      // ========================================
      // Special Godan verbs with irregular euphonic changes
      // ========================================
      // 行く: GodanKa but has special イ音便 (行って→いって, not いいて)
      {"行く", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いく"},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_ESSENTIAL_VERBS_H_
