#ifndef SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_
#define SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_

// =============================================================================
// ⚠️ DEPRECATED: Entries migrated to Layer 2 (data/core/dictionary.tsv)
// =============================================================================
// This file contains OPEN CLASS vocabulary that does not belong in Layer 1:
//   - Verbs (動詞): Open class - new verbs can be added to the language
//   - Suffixes (接尾語): Open class - productive and extensible
//
// Current status: WASM fallback (entries also in dictionary.tsv)
//   - Kept for WASM minimal builds where core.dic is not available
//   - Native builds with core.dic will have duplicate entries (same cost)
//   - Canonical source: data/core/dictionary.tsv (section: low_info.h migration)
//
// Migration status: COMPLETED (2025-12-27)
//   - All entries copied to data/core/dictionary.tsv
//   - This file remains for WASM fallback compatibility
//   - TODO: Add #ifdef SUZUME_WASM conditional compilation
//
// Layer 1 Criteria (this file DOES NOT meet):
//   ❌ CLOSED CLASS: Verbs and suffixes are open, productive classes
//   ❌ Grammatically fixed: New verbs/suffixes appear regularly
//   ✅ Needed for WASM: Only reason this file exists
// =============================================================================

#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

/**
 * @brief Get low information word entries for core dictionary
 *
 * @deprecated This function provides WASM fallback entries.
 * For native builds, use core.dic instead.
 *
 * Low information words are common verbs and suffixes that
 * carry little semantic meaning on their own.
 *
 * @return Vector of dictionary entries for low information words
 */
inline std::vector<DictionaryEntry> getLowInfoEntries() {
  using POS = core::PartOfSpeech;
  using CT = ConjugationType;

  // Note: Verbs don't need hiragana auto-expansion (conjugation handles it)
  // Format: {surface, POS, cost, lemma, prefix, formal, low_info, conj, reading}
  return {
      // Honorific pattern verbs: お + renyokei + いたす/します
      {"伝え", POS::Verb, 0.5F, "伝える", false, false, false, CT::Ichidan, ""},
      {"伝える", POS::Verb, 0.5F, "伝える", false, false, false, CT::Ichidan, ""},
      {"知らせ", POS::Verb, 0.5F, "知らせる", false, false, false, CT::Ichidan, ""},
      {"知らせる", POS::Verb, 0.5F, "知らせる", false, false, false, CT::Ichidan, ""},
      {"届け", POS::Verb, 0.5F, "届ける", false, false, false, CT::Ichidan, ""},
      {"届ける", POS::Verb, 0.5F, "届ける", false, false, false, CT::Ichidan, ""},
      {"答え", POS::Verb, 0.5F, "答える", false, false, false, CT::Ichidan, ""},
      {"答える", POS::Verb, 0.5F, "答える", false, false, false, CT::Ichidan, ""},
      {"教え", POS::Verb, 0.5F, "教える", false, false, false, CT::Ichidan, ""},
      {"教える", POS::Verb, 0.5F, "教える", false, false, false, CT::Ichidan, ""},
      {"見せ", POS::Verb, 0.5F, "見せる", false, false, false, CT::Ichidan, ""},
      {"見せる", POS::Verb, 0.5F, "見せる", false, false, false, CT::Ichidan, ""},
      {"聞かせ", POS::Verb, 0.5F, "聞かせる", false, false, false, CT::Ichidan, ""},
      {"待ち", POS::Verb, 0.5F, "待つ", false, false, false, CT::GodanTa, ""},
      {"願い", POS::Verb, 0.5F, "願う", false, false, false, CT::GodanWa, ""},
      {"願う", POS::Verb, 0.5F, "願う", false, false, false, CT::GodanWa, ""},

      // Low information verbs (低情報量動詞)
      // Note: する, なる, できる, もらう are in hiragana_verbs.h with lower cost
      {"ある", POS::Verb, 2.0F, "ある", false, false, true, CT::GodanRa, ""},
      {"いる", POS::Verb, 2.0F, "いる", false, false, true, CT::Ichidan, ""},
      {"おる", POS::Verb, 2.0F, "おる", false, false, true, CT::GodanRa, ""},
      {"くる", POS::Verb, 2.0F, "くる", false, false, true, CT::Kuru, ""},
      {"いく", POS::Verb, 2.0F, "いく", false, false, true, CT::GodanKa, ""},
      {"くれる", POS::Verb, 2.0F, "くれる", false, false, true, CT::Ichidan, ""},
      {"あげる", POS::Verb, 2.0F, "あげる", false, false, true, CT::Ichidan, ""},
      // やる is now in hiragana_verbs.h with lower cost for compound verb support
      {"みる", POS::Verb, 2.0F, "みる", false, false, true, CT::Ichidan, ""},
      {"おく", POS::Verb, 2.0F, "おく", false, false, true, CT::GodanKa, ""},
      {"しまう", POS::Verb, 2.0F, "しまう", false, false, true, CT::GodanWa, ""},

      // Common verbs with っ-onbin ambiguity
      {"買う", POS::Verb, 1.0F, "買う", false, false, false, CT::GodanWa, ""},
      {"言う", POS::Verb, 1.0F, "言う", false, false, false, CT::GodanWa, ""},
      {"思う", POS::Verb, 1.0F, "思う", false, false, false, CT::GodanWa, ""},
      {"使う", POS::Verb, 1.0F, "使う", false, false, false, CT::GodanWa, ""},
      {"会う", POS::Verb, 1.0F, "会う", false, false, false, CT::GodanWa, ""},
      {"払う", POS::Verb, 1.0F, "払う", false, false, false, CT::GodanWa, ""},
      {"洗う", POS::Verb, 1.0F, "洗う", false, false, false, CT::GodanWa, ""},
      {"歌う", POS::Verb, 1.0F, "歌う", false, false, false, CT::GodanWa, ""},
      {"習う", POS::Verb, 1.0F, "習う", false, false, false, CT::GodanWa, ""},
      {"笑う", POS::Verb, 1.0F, "笑う", false, false, false, CT::GodanWa, ""},
      {"違う", POS::Verb, 1.0F, "違う", false, false, false, CT::GodanWa, ""},
      {"追う", POS::Verb, 1.0F, "追う", false, false, false, CT::GodanWa, ""},
      {"誘う", POS::Verb, 1.0F, "誘う", false, false, false, CT::GodanWa, ""},
      {"拾う", POS::Verb, 1.0F, "拾う", false, false, false, CT::GodanWa, ""},
      // Common GodanRa verbs
      {"取る", POS::Verb, 1.0F, "取る", false, false, false, CT::GodanRa, ""},
      {"乗る", POS::Verb, 1.0F, "乗る", false, false, false, CT::GodanRa, ""},
      {"送る", POS::Verb, 1.0F, "送る", false, false, false, CT::GodanRa, ""},
      {"作る", POS::Verb, 1.0F, "作る", false, false, false, CT::GodanRa, ""},
      {"知る", POS::Verb, 1.0F, "知る", false, false, false, CT::GodanRa, ""},
      {"座る", POS::Verb, 1.0F, "座る", false, false, false, CT::GodanRa, ""},
      {"帰る", POS::Verb, 1.0F, "帰る", false, false, false, CT::GodanRa, ""},
      {"入る", POS::Verb, 1.0F, "入る", false, false, false, CT::GodanRa, ""},
      {"走る", POS::Verb, 1.0F, "走る", false, false, false, CT::GodanRa, ""},
      {"売る", POS::Verb, 1.0F, "売る", false, false, false, CT::GodanRa, ""},
      {"切る", POS::Verb, 1.0F, "切る", false, false, false, CT::GodanRa, ""},
      // Common GodanTa verbs
      {"持つ", POS::Verb, 1.0F, "持つ", false, false, false, CT::GodanTa, ""},
      {"待つ", POS::Verb, 1.0F, "待つ", false, false, false, CT::GodanTa, ""},
      {"立つ", POS::Verb, 1.0F, "立つ", false, false, false, CT::GodanTa, ""},
      {"打つ", POS::Verb, 1.0F, "打つ", false, false, false, CT::GodanTa, ""},
      {"勝つ", POS::Verb, 1.0F, "勝つ", false, false, false, CT::GodanTa, ""},
      {"育つ", POS::Verb, 1.0F, "育つ", false, false, false, CT::GodanTa, ""},

      // Irregular adjective よい (良い) conjugations
      // Note: いい and 良い are in i_adjectives.h
      {"よい", POS::Adjective, 0.3F, "よい", false, false, false, CT::IAdjective, ""},
      {"よく", POS::Adverb, 0.5F, "よい", false, false, false, CT::IAdjective, ""},
      {"よくない", POS::Adjective, 0.5F, "よい", false, false, false, CT::IAdjective, ""},
      {"よくて", POS::Adjective, 0.5F, "よい", false, false, false, CT::IAdjective, ""},
      {"よかった", POS::Adjective, 0.5F, "よい", false, false, false, CT::IAdjective, ""},
      {"よければ", POS::Adjective, 0.5F, "よい", false, false, false, CT::IAdjective, ""},

      // Suffixes (接尾語)
      {"的", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"化", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"性", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"率", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"法", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"論", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"者", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"家", POS::Suffix, 1.5F, "か", false, false, true, CT::None, ""},
      {"員", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"式", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"感", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"力", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"度", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},

      // Plural suffixes (複数接尾語) - essential for proper tokenization
      // Low cost (0.5) to ensure NOUN+suffix is preferred over VERB interpretation
      {"たち", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"ら", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"ども", POS::Suffix, 0.8F, "", false, false, true, CT::None, ""},
      {"がた", POS::Suffix, 0.8F, "", false, false, true, CT::None, ""},

      // Honorific suffixes (敬称接尾語)
      {"さん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"様", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"ちゃん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"くん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"氏", POS::Suffix, 0.8F, "", false, false, true, CT::None, ""},
  };
}

}  // namespace suzume::entries::dictionary
#endif  // SUZUME_DICTIONARY_ENTRIES_LOW_INFO_H_
