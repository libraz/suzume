#ifndef SUZUME_ANALYSIS_CATEGORY_COST_H_
#define SUZUME_ANALYSIS_CATEGORY_COST_H_

#include <array>
#include <cstddef>

#include "core/types.h"

namespace suzume::analysis {

using core::ExtendedPOS;

/**
 * @brief Category-based word cost table
 *
 * Design principles:
 *   1. All costs are positive (0.2 ~ 2.0)
 *   2. Function words (particles, auxiliaries) have lower costs
 *   3. Content words (nouns, verbs, adjectives) have moderate costs
 *   4. Unknown/rare categories have higher costs
 *   5. No per-entry cost adjustment - category determines cost entirely
 *
 * Cost ranges:
 *   - 0.2: Very common function words (は, が, た, ます)
 *   - 0.3: Common function words (て, で, ない)
 *   - 0.4: Verb/adjective forms (連用形, 音便形)
 *   - 0.5: Content words (nouns, dictionary-form verbs)
 *   - 0.6: Less common forms
 *   - 0.8: Rare categories
 *   - 2.0: Unknown
 */

namespace detail {

constexpr std::array<float, static_cast<size_t>(ExtendedPOS::Count_)> kCategoryCostTable = []() constexpr {
  std::array<float, static_cast<size_t>(ExtendedPOS::Count_)> table{};

  // Initialize all to moderate cost
  for (auto& cost : table) {
    cost = 0.5F;
  }

  // Unknown: high cost to discourage
  table[static_cast<size_t>(ExtendedPOS::Unknown)] = 2.0F;

  // ===========================================================================
  // Verb forms (0.4-0.6) - moderate costs
  // ===========================================================================
  table[static_cast<size_t>(ExtendedPOS::VerbShuushikei)] = 0.5F;  // 終止形
  table[static_cast<size_t>(ExtendedPOS::VerbRenyokei)] = 0.4F;    // 連用形
  table[static_cast<size_t>(ExtendedPOS::VerbMizenkei)] = 0.4F;    // 未然形
  table[static_cast<size_t>(ExtendedPOS::VerbOnbinkei)] = 0.4F;    // 音便形
  table[static_cast<size_t>(ExtendedPOS::VerbTeForm)] = 0.4F;      // て形
  table[static_cast<size_t>(ExtendedPOS::VerbKateikei)] = 0.5F;    // 仮定形
  table[static_cast<size_t>(ExtendedPOS::VerbMeireikei)] = 0.5F;   // 命令形
  table[static_cast<size_t>(ExtendedPOS::VerbRentaikei)] = 0.5F;   // 連体形
  table[static_cast<size_t>(ExtendedPOS::VerbTaForm)] = 0.5F;      // た形
  table[static_cast<size_t>(ExtendedPOS::VerbTaraForm)] = 0.5F;    // たら形

  // ===========================================================================
  // Adjective forms (0.4-0.5)
  // ===========================================================================
  table[static_cast<size_t>(ExtendedPOS::AdjBasic)] = 0.5F;     // 終止形
  table[static_cast<size_t>(ExtendedPOS::AdjRenyokei)] = 0.4F;  // 連用形(く)
  table[static_cast<size_t>(ExtendedPOS::AdjStem)] = 0.4F;      // 語幹
  table[static_cast<size_t>(ExtendedPOS::AdjKatt)] = 0.4F;      // かっ形
  table[static_cast<size_t>(ExtendedPOS::AdjKeForm)] = 0.5F;    // け形
  table[static_cast<size_t>(ExtendedPOS::AdjMizenkei)] = 0.5F;  // 未然形(かろ)
  table[static_cast<size_t>(ExtendedPOS::AdjNaAdj)] = 0.5F;     // ナ形容詞

  // ===========================================================================
  // Auxiliaries (0.2-0.4) - low costs for function words
  // ===========================================================================
  // Tense (very common)
  table[static_cast<size_t>(ExtendedPOS::AuxTenseTa)] = 0.2F;       // た
  table[static_cast<size_t>(ExtendedPOS::AuxTenseMasu)] = 0.2F;     // ます
  table[static_cast<size_t>(ExtendedPOS::AuxKuruwaPolite)] = 0.2F;  // なんし

  // Negation
  table[static_cast<size_t>(ExtendedPOS::AuxNegativeNai)] = 0.3F;  // ない
  table[static_cast<size_t>(ExtendedPOS::AuxNegativeNu)] = 0.4F;   // ぬ (archaic)
  table[static_cast<size_t>(ExtendedPOS::AuxNegativeMai)] = 0.4F;  // まい (negative volitional)

  // Classical/literary auxiliaries (archaic, closed class)
  // AuxClassicalNari is deliberately costed higher than other closed-class
  // auxiliaries: なり alone competes with the common modern VerbRenyokei(なる)
  // reading (それなり, 大人なり) and should lose that competition by default.
  // It only wins when followed by けり, where the AuxClassicalNari->AuxClassicalKeri
  // bigram bonus (see bigram_table.cpp) easily outweighs this extra cost.
  table[static_cast<size_t>(ExtendedPOS::AuxClassicalNari)] = 0.7F;  // なり (文語断定)
  table[static_cast<size_t>(ExtendedPOS::AuxClassicalKeri)] = 0.4F;  // けり (文語過去/詠嘆)
  // AuxClassicalTari (連体形 たる) is adnominal and only valid before a nominal, so it is
  // costed high enough that standalone (sentence-final) it LOSES to a single-kanji verb
  // reading (当たる, 隔たる) — those verbs carry a +1.0 penalty that would otherwise let
  // 当|たる sneak past. The AuxClassicalTari->Noun/Particle bigram bonus (bigram_table.cpp)
  // more than repays this cost when a nominal actually follows (堂々たる態度).
  table[static_cast<size_t>(ExtendedPOS::AuxClassicalTari)] = 0.5F;     // たる (文語断定連体)
  table[static_cast<size_t>(ExtendedPOS::AuxClassicalPerfect)] = 0.4F;  // たり/り (文語完了・存続)
  // AuxClassicalBeshi (連体形 べき) preserves べき's prior word cost (it used to ride
  // AuxVolitional's 0.3); the adnominal べき→Noun and 終止形/受身→べき bigram bonuses in
  // bigram_table.cpp carry its placement, so the category cost stays neutral at 0.3.
  table[static_cast<size_t>(ExtendedPOS::AuxClassicalKi)] = 0.4F;     // し/しか (文語過去キ)
  table[static_cast<size_t>(ExtendedPOS::AuxClassicalBeshi)] = 0.3F;  // べき (文語当為)

  // Desire/Volition
  table[static_cast<size_t>(ExtendedPOS::AuxDesireTai)] = 0.3F;   // たい
  table[static_cast<size_t>(ExtendedPOS::AuxVolitional)] = 0.3F;  // う/よう

  // Voice
  table[static_cast<size_t>(ExtendedPOS::AuxPassive)] = 0.3F;      // れる/られる
  table[static_cast<size_t>(ExtendedPOS::AuxCausative)] = 0.3F;    // せる/させる
  table[static_cast<size_t>(ExtendedPOS::AuxPotential)] = 0.3F;    // れる/られる
  table[static_cast<size_t>(ExtendedPOS::AuxInability)] = 0.3F;    // かねる
  table[static_cast<size_t>(ExtendedPOS::AuxBenefactive)] = 0.3F;  // あげる

  // Aspect
  table[static_cast<size_t>(ExtendedPOS::AuxAspectIru)] = 0.3F;       // いる
  table[static_cast<size_t>(ExtendedPOS::AuxAspectShimau)] = 0.3F;    // しまう
  table[static_cast<size_t>(ExtendedPOS::AuxAspectOku)] = 0.3F;       // おく
  table[static_cast<size_t>(ExtendedPOS::AuxAspectMiru)] = 0.3F;      // みる
  table[static_cast<size_t>(ExtendedPOS::AuxAspectIku)] = 0.3F;       // いく
  table[static_cast<size_t>(ExtendedPOS::AuxAspectKuru)] = 0.3F;      // くる
  table[static_cast<size_t>(ExtendedPOS::AuxAspectHajimeru)] = 0.3F;  // はじめる

  // Appearance/Conjecture
  table[static_cast<size_t>(ExtendedPOS::AuxAppearanceSou)] = 0.4F;     // そう
  table[static_cast<size_t>(ExtendedPOS::AuxConjectureRashii)] = 0.3F;  // らしい
  table[static_cast<size_t>(ExtendedPOS::AuxConjectureMitai)] = 0.4F;   // みたい
  table[static_cast<size_t>(ExtendedPOS::AuxSimilitudeYou)] = 0.4F;     // よう

  // Copula
  table[static_cast<size_t>(ExtendedPOS::AuxCopulaDa)] = 0.2F;    // だ
  table[static_cast<size_t>(ExtendedPOS::AuxCopulaDesu)] = 0.2F;  // です

  // Other auxiliaries
  table[static_cast<size_t>(ExtendedPOS::AuxHonorific)] = 0.3F;  // れる/られる
  table[static_cast<size_t>(ExtendedPOS::AuxGozaru)] = 0.4F;     // ございます

  // ===========================================================================
  // Particles (0.2-0.4) - low costs for function words
  // ===========================================================================
  table[static_cast<size_t>(ExtendedPOS::ParticleCase)] = 0.2F;       // が,を,に
  table[static_cast<size_t>(ExtendedPOS::ParticleTopic)] = 0.2F;      // は,も
  table[static_cast<size_t>(ExtendedPOS::ParticleFinal)] = 0.3F;      // ね,よ,わ
  table[static_cast<size_t>(ExtendedPOS::ParticleConj)] = 0.3F;       // て,ば
  table[static_cast<size_t>(ExtendedPOS::ParticleQuote)] = 0.3F;      // と(引用)
  table[static_cast<size_t>(ExtendedPOS::ParticleAdverbial)] = 0.3F;  // ばかり,だけ
  table[static_cast<size_t>(ExtendedPOS::ParticleNo)] = 0.2F;         // の
  table[static_cast<size_t>(ExtendedPOS::ParticleBinding)] = 0.3F;    // こそ,さえ
  // Same closed-class cost as the rest of the conjunctive particles; only the
  // form it attaches to differs.
  table[static_cast<size_t>(ExtendedPOS::ParticleConjFinite)] =
      table[static_cast<size_t>(ExtendedPOS::ParticleConj)];  // が(逆接)

  // ===========================================================================
  // Nouns (0.5-0.6)
  // ===========================================================================
  table[static_cast<size_t>(ExtendedPOS::Noun)] = 0.5F;              // 普通名詞
  table[static_cast<size_t>(ExtendedPOS::NounFormal)] = 0.4F;        // 形式名詞
  table[static_cast<size_t>(ExtendedPOS::NounVerbal)] = 0.5F;        // 連用形転成
  table[static_cast<size_t>(ExtendedPOS::NounProper)] = 0.5F;        // 固有名詞
  table[static_cast<size_t>(ExtendedPOS::NounProperFamily)] = 0.3F;  // 固有名詞(姓)
  table[static_cast<size_t>(ExtendedPOS::NounProperGiven)] = 0.3F;   // 固有名詞(名)
  table[static_cast<size_t>(ExtendedPOS::NounNumber)] = 0.4F;        // 数詞

  // ===========================================================================
  // Pronouns (0.4-0.5)
  // ===========================================================================
  table[static_cast<size_t>(ExtendedPOS::Pronoun)] = 0.4F;               // 代名詞
  table[static_cast<size_t>(ExtendedPOS::PronounInterrogative)] = 0.4F;  // 疑問詞

  // ===========================================================================
  // Others (0.4-0.8)
  // ===========================================================================
  table[static_cast<size_t>(ExtendedPOS::Adverb)] = 0.5F;  // 副詞
  // A quotative adverb is a semantic subtype, not an intrinsically cheaper
  // candidate. Contextual connection rules select it over homographs.
  table[static_cast<size_t>(ExtendedPOS::AdverbQuotative)] = table[static_cast<size_t>(ExtendedPOS::Adverb)];
  table[static_cast<size_t>(ExtendedPOS::Conjunction)] = 0.4F;   // 接続詞
  table[static_cast<size_t>(ExtendedPOS::Determiner)] = 0.4F;    // 連体詞
  table[static_cast<size_t>(ExtendedPOS::Prefix)] = 0.5F;        // 接頭辞
  table[static_cast<size_t>(ExtendedPOS::Suffix)] = 0.5F;        // 接尾辞
  table[static_cast<size_t>(ExtendedPOS::Symbol)] = 0.3F;        // 記号
  table[static_cast<size_t>(ExtendedPOS::Interjection)] = 0.5F;  // 感動詞
  table[static_cast<size_t>(ExtendedPOS::Other)] = 0.8F;         // その他

  return table;
}();

}  // namespace detail

/**
 * @brief Get the category cost for an ExtendedPOS
 *
 * @param epos The ExtendedPOS category
 * @return Cost value (0.2 ~ 2.0)
 */
inline float getCategoryCost(ExtendedPOS epos) {
  const auto index = static_cast<size_t>(epos);
  if (index < detail::kCategoryCostTable.size()) {
    return detail::kCategoryCostTable[index];
  }
  return 2.0F;  // Unknown category
}

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_CATEGORY_COST_H_
