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

      // 見つける: Ichidan (見つけて → 見つける, not 見つく)
      {"見つける", POS::Verb, 0.3F, "見つける", false, false, false, CT::Ichidan,
       "みつける"},

      // 見つかる: GodanRa (intransitive pair of 見つける)
      {"見つかる", POS::Verb, 0.3F, "見つかる", false, false, false, CT::GodanRa,
       "みつかる"},

      // 上がる: GodanRa (intransitive pair of 上げる)
      // Critical for preventing "上がらない" → "上" + "が" + "らない"
      {"上がる", POS::Verb, 0.3F, "上がる", false, false, false, CT::GodanRa,
       "あがる"},

      // 下がる: GodanRa (intransitive pair of 下げる)
      {"下がる", POS::Verb, 0.3F, "下がる", false, false, false, CT::GodanRa,
       "さがる"},

      // ========================================
      // Common Godan verbs (frequently misidentified)
      // ========================================
      // 喜ぶ: GodanBa (喜んで → 喜ぶ, not 喜む)
      {"喜ぶ", POS::Verb, 0.3F, "喜ぶ", false, false, false, CT::GodanBa,
       "よろこぶ"},

      // 学ぶ: GodanBa (学んで → 学ぶ, not 学む)
      {"学ぶ", POS::Verb, 0.3F, "学ぶ", false, false, false, CT::GodanBa,
       "まなぶ"},

      // 遊ぶ: GodanBa (遊んで → 遊ぶ, not 遊む)
      {"遊ぶ", POS::Verb, 0.3F, "遊ぶ", false, false, false, CT::GodanBa,
       "あそぶ"},

      // 飛ぶ: GodanBa (飛んで → 飛ぶ, not 飛む)
      {"飛ぶ", POS::Verb, 0.3F, "飛ぶ", false, false, false, CT::GodanBa,
       "とぶ"},

      // 呼ぶ: GodanBa (呼んで → 呼ぶ, not 呼む)
      {"呼ぶ", POS::Verb, 0.3F, "呼ぶ", false, false, false, CT::GodanBa,
       "よぶ"},

      // 選ぶ: GodanBa (選んで → 選ぶ, not 選む)
      {"選ぶ", POS::Verb, 0.3F, "選ぶ", false, false, false, CT::GodanBa,
       "えらぶ"},

      // 並ぶ: GodanBa (並んで → 並ぶ, not 並む)
      {"並ぶ", POS::Verb, 0.3F, "並ぶ", false, false, false, CT::GodanBa,
       "ならぶ"},

      // 結ぶ: GodanBa (結んで → 結ぶ, not 結む)
      {"結ぶ", POS::Verb, 0.3F, "結ぶ", false, false, false, CT::GodanBa,
       "むすぶ"},

      // ========================================
      // Godan verbs with special patterns
      // ========================================
      // 行う: GodanWa (行います→行う, conflicts with 行 suffix "ゆき")
      // Higher cost than 行く (0.3) so 行く wins for ambiguous forms like 行った
      // 行われた (passive) is unambiguous and still works
      {"行う", POS::Verb, 0.5F, "行う", false, false, false, CT::GodanWa,
       "おこなう"},

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
      // 行く: GodanKa but has special っ音便 (行って→いって, not 行いて)
      // Need explicit entries for っ音便 forms since GodanKa uses い音便 by default
      {"行く", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いく"},
      {"行った", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いった"},
      {"行って", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いって"},

      // ========================================
      // Common GodanKa verbs (き-renyoukei may be confused with i-adj stems)
      // ========================================
      // These verbs have き as renyoukei. When followed by そう, they could be
      // incorrectly analyzed as i-adjectives (e.g., 書きそう → 書きい is invalid).
      // Dictionary lookup prevents false adjective candidates.
      //
      // 書く (to write) - 書きそう should be verb+aux, not adj
      {"書く", POS::Verb, 0.3F, "書く", false, false, false, CT::GodanKa, "かく"},

      // 聞く (to hear/listen) - 聞きそう should be verb+aux
      {"聞く", POS::Verb, 0.3F, "聞く", false, false, false, CT::GodanKa, "きく"},

      // 効く (to be effective) - 効きそう should be verb+aux
      {"効く", POS::Verb, 0.3F, "効く", false, false, false, CT::GodanKa, "きく"},

      // 利く (to work/function) - 利きそう should be verb+aux
      {"利く", POS::Verb, 0.3F, "利く", false, false, false, CT::GodanKa, "きく"},

      // 描く (to draw/depict) - 描きそう should be verb+aux
      {"描く", POS::Verb, 0.3F, "描く", false, false, false, CT::GodanKa, "えがく"},

      // 弾く (to play instrument) - 弾きそう should be verb+aux
      {"弾く", POS::Verb, 0.3F, "弾く", false, false, false, CT::GodanKa, "ひく"},

      // 引く (to pull) - 引きそう should be verb+aux
      {"引く", POS::Verb, 0.3F, "引く", false, false, false, CT::GodanKa, "ひく"},

      // 敷く (to spread/lay) - 敷きそう should be verb+aux
      {"敷く", POS::Verb, 0.3F, "敷く", false, false, false, CT::GodanKa, "しく"},

      // 拭く (to wipe) - 拭きそう should be verb+aux
      {"拭く", POS::Verb, 0.3F, "拭く", false, false, false, CT::GodanKa, "ふく"},

      // 吹く (to blow) - 吹きそう should be verb+aux
      {"吹く", POS::Verb, 0.3F, "吹く", false, false, false, CT::GodanKa, "ふく"},

      // 置く (to place) - 置きそう should be verb+aux
      {"置く", POS::Verb, 0.3F, "置く", false, false, false, CT::GodanKa, "おく"},

      // 開く (to open) - 開きそう should be verb+aux
      {"開く", POS::Verb, 0.3F, "開く", false, false, false, CT::GodanKa, "あく"},

      // 空く (to become empty) - 空きそう should be verb+aux
      {"空く", POS::Verb, 0.3F, "空く", false, false, false, CT::GodanKa, "あく"},

      // 着く (to arrive) - 着きそう should be verb+aux
      {"着く", POS::Verb, 0.3F, "着く", false, false, false, CT::GodanKa, "つく"},

      // 付く (to stick/attach) - 付きそう should be verb+aux
      {"付く", POS::Verb, 0.3F, "付く", false, false, false, CT::GodanKa, "つく"},

      // 続く (to continue) - 続きそう should be verb+aux
      {"続く", POS::Verb, 0.3F, "続く", false, false, false, CT::GodanKa, "つづく"},

      // 届く (to reach/arrive) - 届きそう should be verb+aux
      {"届く", POS::Verb, 0.3F, "届く", false, false, false, CT::GodanKa, "とどく"},

      // 動く (to move) - 動きそう should be verb+aux
      {"動く", POS::Verb, 0.3F, "動く", false, false, false, CT::GodanKa, "うごく"},

      // 働く (to work) - 働きそう should be verb+aux
      {"働く", POS::Verb, 0.3F, "働く", false, false, false, CT::GodanKa, "はたらく"},

      // 泣く (to cry) - 泣きそう should be verb+aux
      {"泣く", POS::Verb, 0.3F, "泣く", false, false, false, CT::GodanKa, "なく"},

      // 鳴く (to chirp/cry animal) - 鳴きそう should be verb+aux
      {"鳴く", POS::Verb, 0.3F, "鳴く", false, false, false, CT::GodanKa, "なく"},

      // 咲く (to bloom) - 咲きそう should be verb+aux
      {"咲く", POS::Verb, 0.3F, "咲く", false, false, false, CT::GodanKa, "さく"},

      // 焼く (to bake/grill) - 焼きそう should be verb+aux
      {"焼く", POS::Verb, 0.3F, "焼く", false, false, false, CT::GodanKa, "やく"},

      // 歩く (to walk) - 歩きそう should be verb+aux
      {"歩く", POS::Verb, 0.3F, "歩く", false, false, false, CT::GodanKa, "あるく"},

      // 抱く (to hold/embrace) - 抱きそう should be verb+aux
      {"抱く", POS::Verb, 0.3F, "抱く", false, false, false, CT::GodanKa, "だく"},

      // 叩く (to hit/knock) - 叩きそう should be verb+aux
      {"叩く", POS::Verb, 0.3F, "叩く", false, false, false, CT::GodanKa, "たたく"},

      // 輝く (to shine) - 輝きそう should be verb+aux
      {"輝く", POS::Verb, 0.3F, "輝く", false, false, false, CT::GodanKa, "かがやく"},

      // 解く (to solve/untie) - 解きそう should be verb+aux
      {"解く", POS::Verb, 0.3F, "解く", false, false, false, CT::GodanKa, "とく"},

      // 突く (to poke/stab) - 突きそう should be verb+aux
      {"突く", POS::Verb, 0.3F, "突く", false, false, false, CT::GodanKa, "つく"},

      // 招く (to invite) - 招きそう should be verb+aux
      {"招く", POS::Verb, 0.3F, "招く", false, false, false, CT::GodanKa, "まねく"},

      // 導く (to guide/lead) - 導きそう should be verb+aux
      {"導く", POS::Verb, 0.3F, "導く", false, false, false, CT::GodanKa, "みちびく"},

      // 築く (to build/construct) - 築きそう should be verb+aux
      {"築く", POS::Verb, 0.3F, "築く", false, false, false, CT::GodanKa, "きずく"},

      // ========================================
      // Basic auxiliary verbs (補助動詞) - in L1 with auto-conjugation
      // ========================================
      // Note: あれ (ある imperative) conflicts with pronoun あれ,
      // but pronoun has lower cost (0.2) so it wins in normal contexts.
      {"ある", POS::Verb, 0.3F, "ある", false, false, true, CT::GodanRa, "ある"},
      {"いる", POS::Verb, 0.3F, "いる", false, false, true, CT::Ichidan, "いる"},
      {"おる", POS::Verb, 0.3F, "おる", false, false, true, CT::GodanRa, "おる"},
      {"くる", POS::Verb, 0.3F, "くる", false, false, true, CT::Kuru, "くる"},
      {"くれる", POS::Verb, 0.3F, "くれる", false, false, true, CT::Ichidan, "くれる"},
      {"あげる", POS::Verb, 0.3F, "あげる", false, false, true, CT::Ichidan, "あげる"},
      {"みる", POS::Verb, 0.3F, "みる", false, false, true, CT::Ichidan, "みる"},
      {"おく", POS::Verb, 0.3F, "おく", false, false, true, CT::GodanKa, "おく"},
      {"しまう", POS::Verb, 0.3F, "しまう", false, false, true, CT::GodanWa, "しまう"},

      // ========================================
      // Common GodanWa verbs (ワ行五段) - from L2 dictionary
      // ========================================
      {"買う", POS::Verb, 1.0F, "買う", false, false, false, CT::GodanWa, "かう"},
      {"言う", POS::Verb, 1.0F, "言う", false, false, false, CT::GodanWa, "いう"},
      {"思う", POS::Verb, 1.0F, "思う", false, false, false, CT::GodanWa, "おもう"},
      {"使う", POS::Verb, 1.0F, "使う", false, false, false, CT::GodanWa, "つかう"},
      {"会う", POS::Verb, 1.0F, "会う", false, false, false, CT::GodanWa, "あう"},
      {"払う", POS::Verb, 1.0F, "払う", false, false, false, CT::GodanWa, "はらう"},
      {"洗う", POS::Verb, 1.0F, "洗う", false, false, false, CT::GodanWa, "あらう"},
      {"歌う", POS::Verb, 1.0F, "歌う", false, false, false, CT::GodanWa, "うたう"},
      {"習う", POS::Verb, 1.0F, "習う", false, false, false, CT::GodanWa, "ならう"},
      {"笑う", POS::Verb, 1.0F, "笑う", false, false, false, CT::GodanWa, "わらう"},
      {"違う", POS::Verb, 1.0F, "違う", false, false, false, CT::GodanWa, "ちがう"},
      {"追う", POS::Verb, 1.0F, "追う", false, false, false, CT::GodanWa, "おう"},
      {"誘う", POS::Verb, 1.0F, "誘う", false, false, false, CT::GodanWa, "さそう"},
      {"拾う", POS::Verb, 1.0F, "拾う", false, false, false, CT::GodanWa, "ひろう"},
      {"願う", POS::Verb, 0.5F, "願う", false, false, false, CT::GodanWa, "ねがう"},

      // ========================================
      // Common GodanRa verbs (ラ行五段) - from L2 dictionary
      // ========================================
      {"取る", POS::Verb, 1.0F, "取る", false, false, false, CT::GodanRa, "とる"},
      {"乗る", POS::Verb, 1.0F, "乗る", false, false, false, CT::GodanRa, "のる"},
      {"送る", POS::Verb, 1.0F, "送る", false, false, false, CT::GodanRa, "おくる"},
      {"作る", POS::Verb, 1.0F, "作る", false, false, false, CT::GodanRa, "つくる"},
      {"知る", POS::Verb, 1.0F, "知る", false, false, false, CT::GodanRa, "しる"},
      {"座る", POS::Verb, 1.0F, "座る", false, false, false, CT::GodanRa, "すわる"},
      {"帰る", POS::Verb, 1.0F, "帰る", false, false, false, CT::GodanRa, "かえる"},
      {"入る", POS::Verb, 1.0F, "入る", false, false, false, CT::GodanRa, "はいる"},
      {"走る", POS::Verb, 1.0F, "走る", false, false, false, CT::GodanRa, "はしる"},
      {"売る", POS::Verb, 1.0F, "売る", false, false, false, CT::GodanRa, "うる"},
      {"切る", POS::Verb, 1.0F, "切る", false, false, false, CT::GodanRa, "きる"},

      // ========================================
      // Common GodanTa verbs (タ行五段) - from L2 dictionary
      // ========================================
      {"持つ", POS::Verb, 1.0F, "持つ", false, false, false, CT::GodanTa, "もつ"},
      {"待つ", POS::Verb, 1.0F, "待つ", false, false, false, CT::GodanTa, "まつ"},
      {"立つ", POS::Verb, 1.0F, "立つ", false, false, false, CT::GodanTa, "たつ"},
      {"打つ", POS::Verb, 1.0F, "打つ", false, false, false, CT::GodanTa, "うつ"},
      {"勝つ", POS::Verb, 1.0F, "勝つ", false, false, false, CT::GodanTa, "かつ"},
      {"育つ", POS::Verb, 1.0F, "育つ", false, false, false, CT::GodanTa, "そだつ"},

      // ========================================
      // Common Ichidan verbs (一段動詞) - from L2 dictionary
      // ========================================
      {"伝える", POS::Verb, 0.5F, "伝える", false, false, false, CT::Ichidan, "つたえる"},
      {"知らせる", POS::Verb, 0.5F, "知らせる", false, false, false, CT::Ichidan, "しらせる"},
      {"届ける", POS::Verb, 0.5F, "届ける", false, false, false, CT::Ichidan, "とどける"},
      {"答える", POS::Verb, 0.5F, "答える", false, false, false, CT::Ichidan, "こたえる"},
      {"教える", POS::Verb, 0.5F, "教える", false, false, false, CT::Ichidan, "おしえる"},
      {"見せる", POS::Verb, 0.5F, "見せる", false, false, false, CT::Ichidan, "みせる"},

      // ========================================
      // ~する Godan verbs (サ変ラ行) - from L2 dictionary
      // ========================================
      // These are NOT suru-verbs, but godan-ra verbs with する suffix
      {"対する", POS::Verb, 0.3F, "対する", false, false, false, CT::GodanRa, "たいする"},
      {"関する", POS::Verb, 0.3F, "関する", false, false, false, CT::GodanRa, "かんする"},
      {"属する", POS::Verb, 0.3F, "属する", false, false, false, CT::GodanRa, "ぞくする"},
      {"発する", POS::Verb, 0.3F, "発する", false, false, false, CT::GodanRa, "はっする"},
      {"接する", POS::Verb, 0.3F, "接する", false, false, false, CT::GodanRa, "せっする"},
      {"反する", POS::Verb, 0.3F, "反する", false, false, false, CT::GodanRa, "はんする"},
      {"達する", POS::Verb, 0.3F, "達する", false, false, false, CT::GodanRa, "たっする"},
      {"適する", POS::Verb, 0.3F, "適する", false, false, false, CT::GodanRa, "てきする"},
      {"課する", POS::Verb, 0.3F, "課する", false, false, false, CT::GodanRa, "かする"},
      {"解する", POS::Verb, 0.3F, "解する", false, false, false, CT::GodanRa, "かいする"},
      {"介する", POS::Verb, 0.3F, "介する", false, false, false, CT::GodanRa, "かいする"},
      {"帰する", POS::Verb, 0.3F, "帰する", false, false, false, CT::GodanRa, "きする"},
      {"欲する", POS::Verb, 0.3F, "欲する", false, false, false, CT::GodanRa, "ほっする"},
      {"資する", POS::Verb, 0.3F, "資する", false, false, false, CT::GodanRa, "しする"},

      // ========================================
      // Other common verbs - from L2 dictionary
      // ========================================
      {"引っ越す", POS::Verb, 0.3F, "引っ越す", false, false, false, CT::GodanSa, "ひっこす"},
  };
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_ESSENTIAL_VERBS_H_
