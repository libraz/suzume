#include "bigram_table.h"

namespace suzume::analysis {

using EPOS = core::ExtendedPOS;
namespace cost = bigram_cost;

// =============================================================================
// Bigram Table Implementation
// =============================================================================

float BigramTable::getCost(core::ExtendedPOS prev, core::ExtendedPOS next) {
  size_t prev_idx = static_cast<size_t>(prev);
  size_t next_idx = static_cast<size_t>(next);
  if (prev_idx >= kSize || next_idx >= kSize) {
    return 0.0F;
  }
  return table_[prev_idx][next_idx];
}

const std::array<std::array<float, BigramTable::kSize>, BigramTable::kSize>&
BigramTable::getTable() {
  return table_;
}

float BigramTable::getCostByIndex(size_t prev_idx, size_t next_idx) {
  if (prev_idx >= kSize || next_idx >= kSize) {
    return 0.0F;
  }
  return table_[prev_idx][next_idx];
}

// =============================================================================
// Table Initialization Helper
// =============================================================================

namespace {

// Helper to set a single cell
inline void setCell(std::array<std::array<float, BigramTable::kSize>,
                               BigramTable::kSize>& table,
                    EPOS prev, EPOS next, float value) {
  table[static_cast<size_t>(prev)][static_cast<size_t>(next)] = value;
}

// Helper to set a row (all connections FROM a specific EPOS)
inline void setRow(std::array<std::array<float, BigramTable::kSize>,
                              BigramTable::kSize>& table,
                   EPOS prev, float value) {
  for (size_t i = 0; i < BigramTable::kSize; ++i) {
    table[static_cast<size_t>(prev)][i] = value;
  }
}

// Helper to set a column (all connections TO a specific EPOS)
inline void setCol(std::array<std::array<float, BigramTable::kSize>,
                              BigramTable::kSize>& table,
                   EPOS next, float value) {
  for (size_t i = 0; i < BigramTable::kSize; ++i) {
    table[i][static_cast<size_t>(next)] = value;
  }
}

}  // namespace

std::array<std::array<float, BigramTable::kSize>, BigramTable::kSize>
BigramTable::initTable() {
  std::array<std::array<float, kSize>, kSize> t{};

  // Initialize all cells to neutral
  for (auto& row : t) {
    row.fill(cost::kNeutral);
  }

  // =========================================================================
  // Verb Forms → Auxiliaries (Core Grammar)
  // =========================================================================

  // VerbRenyokei → AuxTenseMasu (食べ+ます) - strong bonus
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxTenseMasu, cost::kStrongBonus);

  // VerbRenyokei → AuxDesireTai (食べ+たい) - strong bonus
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxDesireTai, cost::kStrongBonus);

  // VerbMizenkei → AuxNegativeNai (食べ+ない for ichidan) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxNegativeNai, cost::kModerateBonus);

  // VerbRenyokei → AuxNegativeNai (すぎ+ない for ichidan renyokei)
  // For ichidan verbs, renyokei and mizenkei are the same form (e.g., 食べ, すぎ)
  // The dictionary stores these as VerbRenyokei, so we need a bonus for this path too
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxNegativeNai, cost::kModerateBonus);

  // VerbMizenkei → AuxPassive (食べ+られる) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxPassive, cost::kModerateBonus);

  // VerbRenyokei → AuxPassive (知らせ+られ) - moderate bonus
  // For ichidan verbs, renyokei = mizenkei, so both forms need bonus
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxPassive, cost::kModerateBonus);

  // AuxPassive → AuxTenseTa (られ+た) - MeCab compatibility
  setCell(t, EPOS::AuxPassive, EPOS::AuxTenseTa, cost::kStrongBonus);

  // VerbMizenkei → AuxCausative (食べ+させる) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxCausative, cost::kModerateBonus);

  // VerbMizenkei → AuxVolitional (食べ+よう) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxVolitional, cost::kModerateBonus);

  // VerbOnbinkei → AuxTenseTa (書い+た, 泳い+だ) - strong bonus
  setCell(t, EPOS::VerbOnbinkei, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxNegativeNu → VerbOnbinkei (ん+かっ) - strong bonus
  // MeCab compatibility: くだらんかった → くだら|ん|かっ|た
  // MeCab analyzes かっ as verb かる (五段ラ行連用タ接続)
  setCell(t, EPOS::AuxNegativeNu, EPOS::VerbOnbinkei, cost::kStrongBonus);

  // VerbRenyokei → AuxTenseTa (食べ+た, すぎ+た for ichidan) - moderate bonus
  // Ichidan verb renyokei connects directly to た without onbin
  // Note: Using moderate bonus to allow dictionary adverbs (どうして) to win
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxTenseTa, cost::kModerateBonus);

  // VerbTeForm → AuxAspectIru (食べて+いる) - strong bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectIru, cost::kStrongBonus);

  // ParticleConj → AuxAspectIru (て+いる) - MeCab compatibility: 食べ+て+いる
  // Strong bonus to beat VerbTeForm→AuxAspectIru path (食べて+いる)
  setCell(t, EPOS::ParticleConj, EPOS::AuxAspectIru, cost::kStrongBonus);

  // VerbTeForm → AuxAspectShimau (食べて+しまう) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectShimau, cost::kModerateBonus);

  // VerbTeForm → AuxAspectOku (食べて+おく) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectOku, cost::kModerateBonus);

  // VerbRenyokei → AuxAspectOku (見+とく, 食べ+とく colloquial) - strong bonus
  // MeCab compatibility: 見とく → 見|とく
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxAspectOku, cost::kStrongBonus);

  // VerbOnbinkei → AuxAspectOku (書い+とく godan verbs) - strong bonus
  // MeCab compatibility: 書いとく → 書い|とく
  setCell(t, EPOS::VerbOnbinkei, EPOS::AuxAspectOku, cost::kStrongBonus);

  // AuxAspectOku → AuxTenseTa (とい+た) - MeCab compatibility
  setCell(t, EPOS::AuxAspectOku, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxAspectOku → ParticleConj (とい+て) - MeCab compatibility
  setCell(t, EPOS::AuxAspectOku, EPOS::ParticleConj, cost::kStrongBonus);

  // VerbTeForm → AuxAspectMiru (食べて+みる) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectMiru, cost::kModerateBonus);

  // VerbTeForm → AuxAspectIku (食べて+いく) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectIku, cost::kModerateBonus);

  // VerbTeForm → AuxAspectKuru (食べて+くる) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectKuru, cost::kModerateBonus);

  // =========================================================================
  // Verb Forms → Particles
  // =========================================================================

  // VerbShuushikei → ParticleFinal (食べる+ね) - minor bonus
  setCell(t, EPOS::VerbShuushikei, EPOS::ParticleFinal, cost::kMinorBonus);

  // VerbShuushikei → ParticleQuote (食べる+と言う) - neutral
  setCell(t, EPOS::VerbShuushikei, EPOS::ParticleQuote, cost::kNeutral);

  // VerbKateikei → ParticleConj (食べれ+ば) - strong bonus
  setCell(t, EPOS::VerbKateikei, EPOS::ParticleConj, cost::kStrongBonus);

  // VerbOnbinkei → ParticleConj (書い+て, 読ん+で) - strong bonus for te-form split
  setCell(t, EPOS::VerbOnbinkei, EPOS::ParticleConj, cost::kStrongBonus);

  // VerbRenyokei → ParticleConj (食べ+て, 見+て) - moderate bonus for ichidan te-form split
  // Reduced from kStrongBonus to prevent す+ば splitting すばらしい
  setCell(t, EPOS::VerbRenyokei, EPOS::ParticleConj, cost::kModerateBonus);

  // =========================================================================
  // Adjective Forms → Auxiliaries/Particles
  // =========================================================================

  // AdjRenyokei → ParticleConj (ウザく+て, 美しく+て) - strong bonus
  // MeCab compatibility: ウザくて → ウザく|て
  setCell(t, EPOS::AdjRenyokei, EPOS::ParticleConj, cost::kStrongBonus);

  // AdjRenyokei → AuxNegativeNai (美しく+ない) - strong bonus
  setCell(t, EPOS::AdjRenyokei, EPOS::AuxNegativeNai, cost::kStrongBonus);

  // AdjKatt → AuxTenseTa (美しかっ+た) - strong bonus
  setCell(t, EPOS::AdjKatt, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AdjBasic → AuxCopulaDesu (美しい+です) - moderate bonus
  setCell(t, EPOS::AdjBasic, EPOS::AuxCopulaDesu, cost::kModerateBonus);

  // AdjBasic → ParticleFinal (美しい+ね) - minor bonus
  setCell(t, EPOS::AdjBasic, EPOS::ParticleFinal, cost::kMinorBonus);

  // AdjBasic → NounFormal (高い+ん in 高いんだ) - strong bonus for MeCab split
  setCell(t, EPOS::AdjBasic, EPOS::NounFormal, cost::kStrongBonus);

  // AdjBasic → ParticleConj (よけれ+ば) - strong bonus for hypothetical split
  setCell(t, EPOS::AdjBasic, EPOS::ParticleConj, cost::kStrongBonus);

  // AdjStem → AuxAppearanceSou (美し+そう) - strong bonus
  setCell(t, EPOS::AdjStem, EPOS::AuxAppearanceSou, cost::kStrongBonus);

  // AdjStem → Suffix (な+さ nominalization) - strong bonus for なさそう
  setCell(t, EPOS::AdjStem, EPOS::Suffix, cost::kStrongBonus);
  setCell(t, EPOS::AdjRenyokei, EPOS::Suffix, cost::kStrongBonus);

  // Suffix → AuxAppearanceSou (さ+そう) - strong bonus for なさそう/よさそう
  setCell(t, EPOS::Suffix, EPOS::AuxAppearanceSou, cost::kStrongBonus);

  // =========================================================================
  // Auxiliary → Auxiliary Chains
  // =========================================================================

  // AuxTenseMasu → AuxTenseTa (まし+た) - strong bonus
  setCell(t, EPOS::AuxTenseMasu, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxNegativeNai → AuxTenseTa (なかっ+た) - strong bonus
  setCell(t, EPOS::AuxNegativeNai, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxNegativeNai → NounFormal (ない+ん in ないんだ) - strong bonus for MeCab split
  setCell(t, EPOS::AuxNegativeNai, EPOS::NounFormal, cost::kStrongBonus);

  // AuxDesireTai → AuxTenseTa (たかっ+た) - strong bonus
  setCell(t, EPOS::AuxDesireTai, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxAspectIru → AuxTenseTa (い+た) - moderate bonus
  setCell(t, EPOS::AuxAspectIru, EPOS::AuxTenseTa, cost::kModerateBonus);

  // AuxCopulaDa → AuxTenseTa (だっ+た) - strong bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxTenseTa, cost::kStrongBonus);

  // =========================================================================
  // Auxiliary → Particle
  // =========================================================================

  // AuxTenseTa → ParticleFinal (た+ね/よ) - minor bonus
  setCell(t, EPOS::AuxTenseTa, EPOS::ParticleFinal, cost::kMinorBonus);

  // AuxTenseMasu → ParticleFinal (ます+ね/よ) - minor bonus
  setCell(t, EPOS::AuxTenseMasu, EPOS::ParticleFinal, cost::kMinorBonus);

  // AuxCopulaDesu → ParticleFinal (です+ね/よ) - minor bonus
  setCell(t, EPOS::AuxCopulaDesu, EPOS::ParticleFinal, cost::kMinorBonus);

  // AuxCopulaDa → ParticleFinal (だ+ね/よ) - minor bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::ParticleFinal, cost::kMinorBonus);

  // =========================================================================
  // Noun → Particles
  // =========================================================================

  // Noun → ParticleCase (机+が/を/に) - neutral (very common)
  setCell(t, EPOS::Noun, EPOS::ParticleCase, cost::kNeutral);

  // Noun → ParticleTopic (机+は/も) - neutral (very common)
  setCell(t, EPOS::Noun, EPOS::ParticleTopic, cost::kNeutral);

  // Noun → ParticleAdverbial (雨+でも/ばかり/だけ) - strong bonus
  // to prevent で+も split in 雨でも (needs to beat Noun→AuxCopulaDa→ParticleTopic)
  setCell(t, EPOS::Noun, EPOS::ParticleAdverbial, cost::kStrongBonus);

  // Adverb → ParticleConj (どう+し) - penalty to prevent oversplitting
  // Adverbs like どうして should not be split as どう+し(particle)+て
  setCell(t, EPOS::Adverb, EPOS::ParticleConj, cost::kModeratePenalty);

  // NounFormal → ParticleConj (こと+が) - neutral
  setCell(t, EPOS::NounFormal, EPOS::ParticleCase, cost::kNeutral);

  // NounFormal → AuxCopulaDa (ん+だ) - strong bonus for MeCab-compatible split
  setCell(t, EPOS::NounFormal, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // =========================================================================
  // Noun → Copula
  // =========================================================================

  // Noun → AuxCopulaDa (学生+だ) - moderate bonus
  setCell(t, EPOS::Noun, EPOS::AuxCopulaDa, cost::kModerateBonus);

  // Noun → AuxCopulaDesu (学生+です) - moderate bonus
  setCell(t, EPOS::Noun, EPOS::AuxCopulaDesu, cost::kModerateBonus);

  // NaAdj → AuxCopulaDa (静か+だ) - strong bonus
  setCell(t, EPOS::AdjNaAdj, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // NaAdj → AuxCopulaDesu (静か+です) - strong bonus
  setCell(t, EPOS::AdjNaAdj, EPOS::AuxCopulaDesu, cost::kStrongBonus);

  // =========================================================================
  // Particle → Various (Particles can connect to many things)
  // =========================================================================

  // ParticleCase → Noun (が+学生) - neutral
  setCell(t, EPOS::ParticleCase, EPOS::Noun, cost::kNeutral);

  // ParticleCase → VerbShuushikei (を+食べる) - neutral
  setCell(t, EPOS::ParticleCase, EPOS::VerbShuushikei, cost::kNeutral);

  // ParticleTopic → VerbShuushikei (は+食べる) - neutral
  setCell(t, EPOS::ParticleTopic, EPOS::VerbShuushikei, cost::kNeutral);

  // ParticleConj → VerbShuushikei (て+食べる for compound verbs) - minor penalty
  // (te-form usually followed by auxiliary, not new verb)
  setCell(t, EPOS::ParticleConj, EPOS::VerbShuushikei, cost::kMinorPenalty);

  // =========================================================================
  // Penalties: Invalid or Rare Connections
  // =========================================================================

  // VerbShuushikei → AuxTenseMasu (食べる+ます) - prohibitive
  // (ます attaches to renyokei, not shuushikei)
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxTenseMasu, cost::kProhibitive);

  // VerbShuushikei → AuxDesireTai (食べる+たい) - prohibitive
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxDesireTai, cost::kProhibitive);

  // VerbShuushikei → AuxTenseTa (食べる+た) - prohibitive
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxTenseTa, cost::kProhibitive);

  // AuxTenseTa → AuxTenseTa (た+た) - prohibitive
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxTenseTa, cost::kProhibitive);

  // AuxTenseTa → AuxTenseMasu (た+ます) - prohibitive
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxTenseMasu, cost::kProhibitive);

  // ParticleFinal → VerbShuushikei (ね+食べる) - strong penalty
  // (sentence-final particles rarely continue to verbs)
  setCell(t, EPOS::ParticleFinal, EPOS::VerbShuushikei, cost::kStrongPenalty);

  // ParticleFinal → ParticleFinal (よ+ね) - minor bonus (common pattern)
  setCell(t, EPOS::ParticleFinal, EPOS::ParticleFinal, cost::kMinorBonus);

  // =========================================================================
  // Copula → Negation (ではない pattern)
  // =========================================================================

  // AuxCopulaDa (で form) → ParticleTopic (では) - minor bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::ParticleTopic, cost::kMinorBonus);

  // AuxCopulaDa (で form) → AuxNegativeNai (で+ない in ではない) - minor bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxNegativeNai, cost::kMinorBonus);

  // AuxCopulaDa → AuxGozaru (で+ございます) - moderate bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxGozaru, cost::kModerateBonus);

  // =========================================================================
  // Appearance/Conjecture connections
  // =========================================================================

  // VerbRenyokei → AuxAppearanceSou (食べ+そう) - strong bonus
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxAppearanceSou, cost::kStrongBonus);

  // AdjBasic → AuxConjectureRashii (美しい+らしい) - moderate bonus
  setCell(t, EPOS::AdjBasic, EPOS::AuxConjectureRashii, cost::kModerateBonus);

  // VerbShuushikei → AuxConjectureRashii (食べる+らしい) - moderate bonus
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxConjectureRashii, cost::kModerateBonus);

  // VerbShuushikei → AuxObligation (食べる+べき) - strong bonus for classical obligation
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxObligation, cost::kStrongBonus);

  // AuxObligation → AuxCopulaDa (べき+だ, べき+だっ) - MeCab compatibility
  setCell(t, EPOS::AuxObligation, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // Noun → AuxConjectureMitai (学生+みたい) - moderate bonus
  setCell(t, EPOS::Noun, EPOS::AuxConjectureMitai, cost::kModerateBonus);

  // VerbShuushikei → AuxConjectureMitai (食べる+みたい, 行く+みたい) - strong bonus
  // MeCab compatibility: V+みたい should be split
  // Must be strong to beat VerbRenyokei+AuxDesireTai path (行くみ+たい)
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxConjectureMitai, cost::kStrongBonus);

  // Noun → AuxConjectureRashii (春+らしい, 学生+らしい) - moderate bonus
  // MeCab compatibility: N+らしい should be split (except lexicalized forms like 男らしい)
  // Use moderate (not strong) to let dictionary entries (男らしい) win over split path
  setCell(t, EPOS::Noun, EPOS::AuxConjectureRashii, cost::kModerateBonus);

  // =========================================================================
  // Compound auxiliary adjectives (やすい/にくい patterns)
  // =========================================================================

  // VerbRenyokei → AdjBasic (使い+にくい, 読み+やすい) - strong bonus
  // MeCab compatibility: compound verb+auxiliary adjective should be split
  setCell(t, EPOS::VerbRenyokei, EPOS::AdjBasic, cost::kStrongBonus);

  // VerbOnbinkei → AdjBasic (also covers 使い+にくい patterns)
  // Note: い-ending godan renyokei (使い) may be detected as VerbOnbinkei
  // because detectVerbForm can't distinguish 使い (renyokei) from 書い (onbin)
  setCell(t, EPOS::VerbOnbinkei, EPOS::AdjBasic, cost::kStrongBonus);

  return t;
}

// Initialize the static table
const std::array<std::array<float, BigramTable::kSize>, BigramTable::kSize>
    BigramTable::table_ = BigramTable::initTable();

}  // namespace suzume::analysis
