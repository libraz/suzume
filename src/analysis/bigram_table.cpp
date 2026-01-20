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

  // VerbRenyokei → AuxHonorific (書き+なさい) - strong bonus for polite imperative
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxHonorific, cost::kStrongBonus);

  // VerbMizenkei → AuxNegativeNai (食べ+ない for ichidan) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxNegativeNai, cost::kModerateBonus);

  // VerbMizenkei → AuxNegativeNu (くだら+ん contracted negative) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxNegativeNu, cost::kModerateBonus);

  // VerbMizenkei → AuxPassive (食べ+られる) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxPassive, cost::kModerateBonus);

  // VerbMizenkei → AuxCausative (食べ+させる) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxCausative, cost::kModerateBonus);

  // VerbMizenkei → AuxVolitional (食べ+よう) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxVolitional, cost::kModerateBonus);

  // VerbOnbinkei → AuxTenseTa (書い+た, 泳い+だ) - strong bonus
  setCell(t, EPOS::VerbOnbinkei, EPOS::AuxTenseTa, cost::kStrongBonus);

  // VerbTeForm → AuxAspectIru (食べて+いる) - strong bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectIru, cost::kStrongBonus);

  // VerbTeForm → AuxAspectShimau (食べて+しまう) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectShimau, cost::kModerateBonus);

  // VerbTeForm → AuxAspectOku (食べて+おく) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectOku, cost::kModerateBonus);

  // VerbTeForm → AuxAspectMiru (食べて+みる) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectMiru, cost::kModerateBonus);

  // VerbTeForm → AuxAspectIku (食べて+いく) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectIku, cost::kModerateBonus);

  // VerbTeForm → AuxAspectKuru (食べて+くる) - moderate bonus
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectKuru, cost::kModerateBonus);

  // VerbRenyokei → AuxAspectOku (食べ+とく contraction of 食べておく) - strong bonus
  // This handles contracted forms where ておく → とく
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxAspectOku, cost::kStrongBonus);

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
  // Adjective Forms → Auxiliaries
  // =========================================================================

  // AdjRenyokei → AuxNegativeNai (美しく+ない) - very strong bonus for MeCab-compatible split
  // This needs to beat the full-form hiragana adjective bonus (e.g., しんどくない as single token)
  setCell(t, EPOS::AdjRenyokei, EPOS::AuxNegativeNai, -2.0F);

  // AdjRenyokei → ParticleConj (美しく+て, ウザく+て) - strong bonus for te-form split
  setCell(t, EPOS::AdjRenyokei, EPOS::ParticleConj, cost::kStrongBonus);

  // AdjKatt → AuxTenseTa (美しかっ+た) - strong bonus
  setCell(t, EPOS::AdjKatt, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AdjKeForm → ParticleConj (美しけれ+ば) - strong bonus for conditional splitting
  setCell(t, EPOS::AdjKeForm, EPOS::ParticleConj, cost::kStrongBonus);

  // AdjBasic → AuxCopulaDesu (美しい+です) - moderate bonus
  setCell(t, EPOS::AdjBasic, EPOS::AuxCopulaDesu, cost::kModerateBonus);

  // AdjBasic → ParticleFinal (美しい+ね) - minor bonus
  setCell(t, EPOS::AdjBasic, EPOS::ParticleFinal, cost::kMinorBonus);

  // AdjStem → AuxAppearanceSou (美し+そう) - strong bonus
  setCell(t, EPOS::AdjStem, EPOS::AuxAppearanceSou, cost::kStrongBonus);

  // =========================================================================
  // Auxiliary → Auxiliary Chains
  // =========================================================================

  // AuxTenseMasu → AuxTenseTa (まし+た) - strong bonus
  setCell(t, EPOS::AuxTenseMasu, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxNegativeNai → AuxTenseTa (なかっ+た) - strong bonus
  setCell(t, EPOS::AuxNegativeNai, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxDesireTai → AuxTenseTa (たかっ+た) - strong bonus
  setCell(t, EPOS::AuxDesireTai, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxAspectIru → AuxTenseTa (い+た) - moderate bonus
  setCell(t, EPOS::AuxAspectIru, EPOS::AuxTenseTa, cost::kModerateBonus);

  // AuxCopulaDa → AuxTenseTa (だっ+た) - strong bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxTenseTa → AuxCopulaDesu (た+です) - moderate bonus for polite past
  // e.g., 長かっ+た+です, 美しかっ+た+です (adjective past polite)
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxCopulaDesu, cost::kModerateBonus);

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

  // NounFormal → ParticleConj (こと+が) - neutral
  setCell(t, EPOS::NounFormal, EPOS::ParticleCase, cost::kNeutral);

  // =========================================================================
  // Noun → Copula/Negative
  // =========================================================================

  // Noun → AuxCopulaDa (学生+だ) - moderate bonus
  setCell(t, EPOS::Noun, EPOS::AuxCopulaDa, cost::kModerateBonus);

  // Noun → AuxCopulaDesu (学生+です) - moderate bonus
  setCell(t, EPOS::Noun, EPOS::AuxCopulaDesu, cost::kModerateBonus);

  // Noun → AuxNegativeNai (間違い+ない, 違い+ない) - moderate bonus
  // For idiomatic patterns meaning "certain" or "no doubt"
  setCell(t, EPOS::Noun, EPOS::AuxNegativeNai, cost::kModerateBonus);

  // NaAdj → AuxCopulaDa (静か+だ) - strong bonus
  setCell(t, EPOS::AdjNaAdj, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // NaAdj → AuxCopulaDesu (静か+です) - strong bonus
  setCell(t, EPOS::AdjNaAdj, EPOS::AuxCopulaDesu, cost::kStrongBonus);

  // AdjStem → Suffix (な+さ in なさそう) - strong bonus for nominalization
  // This favors な(ADJ stem of ない) + さ(nominalization suffix) over さ(する mizenkei)
  setCell(t, EPOS::AdjStem, EPOS::Suffix, cost::kStrongBonus);

  // =========================================================================
  // Particle → Various (Particles can connect to many things)
  // =========================================================================

  // ParticleAdverbial → VerbRenyokei (かも+しれ in かもしれない) - strong bonus
  // This favors かも+しれ+ない over か+もし+れない
  setCell(t, EPOS::ParticleAdverbial, EPOS::VerbRenyokei, cost::kStrongBonus);

  // ParticleCase → Adverb (か+もし) - moderate penalty
  // This discourages splitting かもしれない as か+もし+れない
  setCell(t, EPOS::ParticleCase, EPOS::Adverb, cost::kModeratePenalty);

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

  // AdjBasic → AuxConjectureRashii (美しい+らしい) - strong bonus
  setCell(t, EPOS::AdjBasic, EPOS::AuxConjectureRashii, cost::kStrongBonus);

  // VerbShuushikei → AuxConjectureRashii (食べる+らしい) - moderate bonus
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxConjectureRashii, cost::kModerateBonus);

  // VerbShuushikei → AuxConjectureMitai (食べる+みたい) - strong bonus
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxConjectureMitai, cost::kStrongBonus);

  // VerbShuushikei → AuxVolitional (食べる+べき) - strong bonus for obligation
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxVolitional, cost::kStrongBonus);

  // AdjBasic → AuxConjectureMitai (美しい+みたい) - moderate bonus
  setCell(t, EPOS::AdjBasic, EPOS::AuxConjectureMitai, cost::kModerateBonus);

  // Noun → AuxConjectureMitai (学生+みたい) - moderate bonus
  setCell(t, EPOS::Noun, EPOS::AuxConjectureMitai, cost::kModerateBonus);

  // Noun → AuxConjectureRashii (春+らしい) - strong bonus
  setCell(t, EPOS::Noun, EPOS::AuxConjectureRashii, cost::kStrongBonus);

  // AuxConjectureMitai → AuxCopulaDa (みたい+な) - strong bonus for MeCab-compatible split
  // MeCab splits みたいな as みたい + な(連体形 of だ)
  setCell(t, EPOS::AuxConjectureMitai, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // =========================================================================
  // Prohibited/Penalized Connections (Grammatically Invalid or Unlikely)
  // =========================================================================

  // Note: VerbRenyokei → VerbRenyokei is NOT prohibited because:
  // - Legitimate: 食べ+すぎる (auxiliary verb compound)
  // - Spurious cases like 食べ→るみ are handled by scorer penalty for
  //   non-dictionary kanji+hiragana verb renyokei candidates

  // AdjStem → AuxConjectureMitai: unnatural (美し+みたい should be 美しい+みたい)
  setCell(t, EPOS::AdjStem, EPOS::AuxConjectureMitai, cost::kProhibitive);

  // AdjStem → AuxConjectureRashii: unnatural (美し+らしい should be 美しい+らしい)
  setCell(t, EPOS::AdjStem, EPOS::AuxConjectureRashii, cost::kProhibitive);

  // =========================================================================
  // Particle → Other penalties (prevents over-segmentation of hiragana words)
  // =========================================================================
  // Patterns like も+ちろん, と+にかく are not valid Japanese morphology
  // Single-char particles followed by unknown hiragana are usually misanalyses

  // ParticleTopic → Other: penalty (も+ちろん is invalid)
  setCell(t, EPOS::ParticleTopic, EPOS::Other, cost::kModeratePenalty);

  // ParticleCase → Other: penalty (と+にかく, に+かく are invalid)
  setCell(t, EPOS::ParticleCase, EPOS::Other, cost::kModeratePenalty);

  // ParticleFinal → Other: penalty (ね+random, よ+random are invalid)
  setCell(t, EPOS::ParticleFinal, EPOS::Other, cost::kModeratePenalty);

  // ParticleConj → Other: minor penalty (て+random at sentence start is unlikely)
  // Less penalty than others because て+noun/verb is valid in some contexts
  setCell(t, EPOS::ParticleConj, EPOS::Other, cost::kMinorPenalty);

  return t;
}

// Initialize the static table
const std::array<std::array<float, BigramTable::kSize>, BigramTable::kSize>
    BigramTable::table_ = BigramTable::initTable();

}  // namespace suzume::analysis
