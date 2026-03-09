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

  // VerbRenyokei → AuxNegativeNai (しれ+ない for ichidan same-form mizen/renyokei)
  // Ichidan verbs have same form for mizen and renyokei (e.g., しれ from しれる)
  // This helps かもしれない → かも+しれ+ない over かも+し+れ+ない
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxNegativeNai, cost::kStrongBonus);

  // VerbMizenkei → AuxNegativeNu (くだら+ん contracted negative) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxNegativeNu, cost::kModerateBonus);

  // VerbRenyokei → AuxNegativeNu (消え+ぬ classical negative)
  // Ichidan verbs have same form for mizen and renyokei (e.g., 消え from 消える)
  // This helps 消えぬ炎 → 消え+ぬ+炎 over 消えぬ+炎
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxNegativeNu, cost::kModerateBonus);

  // VerbMizenkei → AuxPassive (食べ+られる) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxPassive, cost::kModerateBonus);

  // VerbRenyokei → AuxPassive (知らせ+られ) - strong bonus
  // Ensures 知らせられた → 知らせ+られ+た over 知ら+せ+られ+た
  // The ichidan causative verb (知らせる) renyokei should connect to passive
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxPassive, cost::kStrongBonus);

  // VerbMizenkei → AuxCausative (食べ+させる) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxCausative, cost::kModerateBonus);

  // VerbMizenkei → AuxVolitional (食べ+よう) - moderate bonus
  setCell(t, EPOS::VerbMizenkei, EPOS::AuxVolitional, cost::kModerateBonus);

  // Note: VerbRenyokei → AuxTenseTa is NOT set here because it would incorrectly
  // favor て as AUX over Particle. Ichidan た-form split (食べ+た) needs surface-based
  // handling in scorer.cpp to distinguish た from て.

  // VerbOnbinkei → AuxTenseTa (書い+た, 泳い+だ) - strong bonus
  setCell(t, EPOS::VerbOnbinkei, EPOS::AuxTenseTa, cost::kStrongBonus);

  // VerbOnbinkei → AuxAspectOku (読ん+どい) - moderate bonus for contracted ~ておく split
  setCell(t, EPOS::VerbOnbinkei, EPOS::AuxAspectOku, cost::kModerateBonus);

  // VerbOnbinkei → AuxAspectShimau (行っ+ちゃっ) - strong bonus for contracted ~てしまう split
  setCell(t, EPOS::VerbOnbinkei, EPOS::AuxAspectShimau, cost::kStrongBonus);

  // VerbTeForm → AuxAspectIru (食べて+いる) - penalty to prefer 食べ+て+いる split
  // MeCab splits as 食べ+て+いる, not 食べて+いる
  setCell(t, EPOS::VerbTeForm, EPOS::AuxAspectIru, cost::kUncommon);

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

  // VerbRenyokei → AuxAspectShimau (食べ+ちゃっ contraction of 食べてしまう) - strong bonus
  // This handles contracted forms where てしまう → ちゃう
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxAspectShimau, cost::kStrongBonus);

  // =========================================================================
  // Verb Forms → Particles
  // =========================================================================

  // VerbShuushikei → ParticleFinal (食べる+ね) - minor bonus
  setCell(t, EPOS::VerbShuushikei, EPOS::ParticleFinal, cost::kMinorBonus);

  // VerbShuushikei → ParticleNo (食べる+の+だ for のだ/んだ) - strong bonus
  setCell(t, EPOS::VerbShuushikei, EPOS::ParticleNo, cost::kStrongBonus);

  // Verb → ParticleAdverbial (できる+だけ, 食べる+だけ, 行く+だけ) - minor bonus
  setCell(t, EPOS::VerbShuushikei, EPOS::ParticleAdverbial, cost::kMinorBonus);
  setCell(t, EPOS::VerbRenyokei, EPOS::ParticleAdverbial, cost::kMinorBonus);

  // VerbShuushikei → ParticleQuote (食べる+と言う) - neutral
  setCell(t, EPOS::VerbShuushikei, EPOS::ParticleQuote, cost::kNeutral);

  // VerbKateikei → ParticleConj (食べれ+ば) - strong bonus
  setCell(t, EPOS::VerbKateikei, EPOS::ParticleConj, cost::kStrongBonus);

  // VerbKateikei → AdjBasic (滅びれば+いい) - strong bonus for 〜ればいい pattern
  // This helps beat the split path 滅び+れ+ば+いい where れ is misanalyzed as passive
  setCell(t, EPOS::VerbKateikei, EPOS::AdjBasic, cost::kStrongBonus);

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

  // AdjRenyokei → VerbRenyokei (美しく+なり) - strong bonus for adjective + become pattern
  // This is a very common Japanese pattern (美しくなる, 大きくなる, etc.)
  setCell(t, EPOS::AdjRenyokei, EPOS::VerbRenyokei, cost::kStrongBonus);

  // AdjKatt → AuxTenseTa (美しかっ+た) - strong bonus
  setCell(t, EPOS::AdjKatt, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AdjKeForm → ParticleConj (美しけれ+ば) - strong bonus for conditional splitting
  setCell(t, EPOS::AdjKeForm, EPOS::ParticleConj, cost::kStrongBonus);

  // AdjBasic → AuxCopulaDesu (美しい+です) - moderate bonus
  setCell(t, EPOS::AdjBasic, EPOS::AuxCopulaDesu, cost::kModerateBonus);

  // AdjBasic → ParticleFinal (美しい+ね, エロい+よ) - moderate bonus
  // Adjective + sentence-final particle is a very common pattern
  setCell(t, EPOS::AdjBasic, EPOS::ParticleFinal, cost::kModerateBonus);

  // AdjBasic → ParticleConj (美しい+し, 高い+けど) - minor bonus
  // Helps i-adjective+conjunctive particle beat ADJ_NA+い(verb)+し path
  setCell(t, EPOS::AdjBasic, EPOS::ParticleConj, cost::kMinorBonus);

  // AdjBasic → ParticleNo (少ない+の, 美味しい+の) - minor bonus
  // Without this, NOUN+ない(AUX)+の path beats adjective+の path
  // because AuxNeg→ParticleNo has a strong bonus (-0.8)
  setCell(t, EPOS::AdjBasic, EPOS::ParticleNo, cost::kMinorBonus);

  // AdjStem → AuxAppearanceSou (美し+そう) - very strong bonus
  // Must beat adverb bonus (-1.0 for 2-char hiragana) to prefer auxiliary
  setCell(t, EPOS::AdjStem, EPOS::AuxAppearanceSou, cost::kVeryStrongBonus);

  // AdjStem/AdjNaAdj → AuxExcessive (高+すぎる, シンプル+すぎる) - moderate bonus
  // Helps AUX_過度 beat VERB interpretation when both have same cost
  setCell(t, EPOS::AdjStem, EPOS::AuxExcessive, cost::kModerateBonus);
  setCell(t, EPOS::AdjNaAdj, EPOS::AuxExcessive, cost::kModerateBonus);

  // AdjStem → AuxGaru (怖+がる, 可愛+がる) - moderate bonus
  setCell(t, EPOS::AdjStem, EPOS::AuxGaru, cost::kModerateBonus);

  // Suffix → AuxAppearanceSou (さ+そう in なさそう) - moderate bonus
  // This completes the な+さ+そう chain for ない nominalization + appearance
  setCell(t, EPOS::Suffix, EPOS::AuxAppearanceSou, cost::kModerateBonus);

  // Suffix → AuxCopulaDa (的+な in 論理的な) - strong bonus
  // 的 suffix followed by な (copula attributive) is very common
  setCell(t, EPOS::Suffix, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // Suffix → ParticleCase (的+に in 感情的になる) - moderate bonus
  // Helps split 的+に+なる instead of 的+になる (as single verb)
  setCell(t, EPOS::Suffix, EPOS::ParticleCase, cost::kModerateBonus);

  // Suffix → Verb (中+働く in 一日中働く) - moderate bonus
  // Temporal suffix 中 followed by verb is natural
  setCell(t, EPOS::Suffix, EPOS::VerbRenyokei, cost::kModerateBonus);
  setCell(t, EPOS::Suffix, EPOS::VerbShuushikei, cost::kModerateBonus);

  // Suffix → Noun - minor bonus
  // Suffixes naturally precede nouns (e.g., honorifics before noun phrases)
  setCell(t, EPOS::Suffix, EPOS::Noun, cost::kModerateBonus);

  // =========================================================================
  // Auxiliary → Auxiliary Chains
  // =========================================================================

  // AuxTenseMasu → AuxTenseTa (まし+た) - strong bonus
  setCell(t, EPOS::AuxTenseMasu, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxTenseMasu → AuxNegativeNu (ませ+ん for polite negative) - strong bonus
  // Ensures ません → ませ+ん (aux) over ませ+ん (particle の)
  setCell(t, EPOS::AuxTenseMasu, EPOS::AuxNegativeNu, cost::kStrongBonus);

  // AuxTenseMasu → AuxVolitional (ましょ+う) - strong bonus for MeCab-compatible split
  setCell(t, EPOS::AuxTenseMasu, EPOS::AuxVolitional, cost::kStrongBonus);

  // AuxCopulaDesu → AuxVolitional (でしょ+う) - strong bonus for MeCab-compatible split
  setCell(t, EPOS::AuxCopulaDesu, EPOS::AuxVolitional, cost::kStrongBonus);

  // AuxCopulaDa → AuxVolitional (だろ+う) - strong bonus for MeCab-compatible split
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxVolitional, cost::kStrongBonus);

  // AuxCausative → AuxPassive (せ+られ in causative-passive) - strong bonus
  // Ensures 聞かせられた → 聞か+せ+られ+た over 聞か+せられた
  setCell(t, EPOS::AuxCausative, EPOS::AuxPassive, cost::kStrongBonus);

  // AuxPassive → AuxTenseMasu (れ+ます in passive polite) - strong bonus
  // Ensures 言われます → 言わ+れ+ます over 言われ+ます
  setCell(t, EPOS::AuxPassive, EPOS::AuxTenseMasu, cost::kStrongBonus);

  // AuxPassive → AuxNegativeNai (れ+ない in passive negative) - strong bonus
  // Ensures 言われない → 言わ+れ+ない over 言われ+ない
  setCell(t, EPOS::AuxPassive, EPOS::AuxNegativeNai, cost::kStrongBonus);

  // AuxPassive → AuxTenseTa (れ+た in passive past) - strong bonus
  // Ensures 言われた → 言わ+れ+た over 言われ+た
  setCell(t, EPOS::AuxPassive, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxPassive → AuxDesireTai (れ+たい in passive desiderative) - strong bonus
  // Ensures 見られたい → 見+られ+たい over 見+られ+た+い
  setCell(t, EPOS::AuxPassive, EPOS::AuxDesireTai, cost::kStrongBonus);

  // AuxPassive → AuxVolitional (れる+べき in passive obligation) - strong bonus
  // Ensures 書かれるべき → 書か+れる+べき(dict) over char_speech べき(AUX_過去) path
  setCell(t, EPOS::AuxPassive, EPOS::AuxVolitional, cost::kStrongBonus);

  // AuxPassive → ParticleConj (れ+ながら, れ+ば in passive+conjunctive) - moderate bonus
  // Ensures 揉まれながら → 揉ま+れ+ながら over 揉まれ+ながら
  setCell(t, EPOS::AuxPassive, EPOS::ParticleConj, cost::kModerateBonus);

  // AuxNegativeNai → AuxTenseTa (なかっ+た) - strong bonus
  setCell(t, EPOS::AuxNegativeNai, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxNegativeNu → AuxTenseTa (んかっ+た for contracted negative past)
  // Ensures くだらんかった → くだら+ん+かっ+た over くだ+らんかっ+た
  setCell(t, EPOS::AuxNegativeNu, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxNegativeNai → ParticleNo (ない+ん for のだ/んだ) - strong bonus
  // Ensures ないんだ → ない+ん+だ over な+いん+だ
  setCell(t, EPOS::AuxNegativeNai, EPOS::ParticleNo, cost::kStrongBonus);

  // ParticleNo → AuxCopulaDesu (ん+です/でし for んです/んでした) - strong bonus
  // Ensures んでした → ん+でし+た over ん+で+し+た
  setCell(t, EPOS::ParticleNo, EPOS::AuxCopulaDesu, cost::kStrongBonus);

  // AuxNegativeNu → AuxCopulaDesu (ん+でし for ませんでした) - strong bonus
  // Ensures ませんでした → ませ+ん+でし+た (negative aux ん)
  setCell(t, EPOS::AuxNegativeNu, EPOS::AuxCopulaDesu, cost::kStrongBonus);

  // AuxNegativeNu → ParticleTopic (ずに+は for ずにはいられない) - strong bonus
  // Ensures ずには → ずに+は(topic) over ずに+はいられ(verb)
  setCell(t, EPOS::AuxNegativeNu, EPOS::ParticleTopic, cost::kStrongBonus);

  // ParticleNo → AuxCopulaDa (ん+だ for んだ) - strong bonus
  // Ensures んだ → ん+だ over ん+だ(VERB)
  setCell(t, EPOS::ParticleNo, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // ParticleNo → Noun (の+学生, の+画像) - strong bonus
  // Genitive の + noun is fundamental Japanese grammar
  setCell(t, EPOS::ParticleNo, EPOS::Noun, cost::kStrongBonus);

  // AuxDesireTai → AuxTenseTa (たかっ+た) - strong bonus
  setCell(t, EPOS::AuxDesireTai, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxTenseTa → verb forms - prohibit
  // Past tense cannot be directly followed by verb forms
  // Prevents した+いん+だ / した+いんだ from beating し+たい+ん+だ
  setCell(t, EPOS::AuxTenseTa, EPOS::VerbOnbinkei, cost::kAlmostNever);
  setCell(t, EPOS::AuxTenseTa, EPOS::VerbTaForm, cost::kAlmostNever);
  setCell(t, EPOS::AuxTenseTa, EPOS::VerbTaraForm, cost::kAlmostNever);

  // AuxAspectIru → AuxTenseTa (い+た) - moderate bonus
  setCell(t, EPOS::AuxAspectIru, EPOS::AuxTenseTa, cost::kModerateBonus);

  // AuxAspectOku → AuxTenseTa (とい+た, どい+た) - strong bonus
  // Contracted ~ておく form + past tense: 見とい+た, 読んどい+た
  setCell(t, EPOS::AuxAspectOku, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxAspectIru → AuxTenseMasu (い+ます) - strong bonus for MeCab-compatible split
  // Ensures 学んで+い+ます uses AuxAspectIru (auxiliary) not VerbRenyokei
  setCell(t, EPOS::AuxAspectIru, EPOS::AuxTenseMasu, cost::kStrongBonus);

  // AuxAspectIru → AuxPassive (い+られ in potential/passive) - moderate bonus
  // いられる = いる + られる (potential: can stay/be)
  // E.g., はいられない → は + い + られ + ない
  setCell(t, EPOS::AuxAspectIru, EPOS::AuxPassive, cost::kModerateBonus);

  // AuxAspectIru → VerbShuushikei (い+ける) - penalty
  // Progressive auxiliary い cannot be followed by a new verb
  // Prevents て+い+ける from beating て+いける (potential of いく)
  setCell(t, EPOS::AuxAspectIru, EPOS::VerbShuushikei, cost::kRare);

  // AuxCopulaDa → AuxTenseTa (だっ+た) - strong bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxCopulaDa → AuxTenseMasu (あり+ます in であります) - strong bonus
  // Ensures で+あり+ます uses AuxCopulaDa for both で and あり
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxTenseMasu, cost::kStrongBonus);

  // AuxCopulaDesu → AuxTenseTa (でし+た) - strong bonus for polite past copula
  // Ensures 本でした → 本+でし+た over 本+で+し+た
  setCell(t, EPOS::AuxCopulaDesu, EPOS::AuxTenseTa, cost::kStrongBonus);

  // AuxTenseTa → AuxCopulaDesu (た+です) - moderate bonus for polite past
  // e.g., 長かっ+た+です, 美しかっ+た+です (adjective past polite)
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxCopulaDesu, cost::kModerateBonus);

  // =========================================================================
  // Auxiliary → Particle
  // =========================================================================

  // AuxCopulaDa → ParticleConj (な+ので, な+のに) - strong bonus
  // Ensures なので → な+ので over な+の+で
  // Without this, PART_準体→AUX_断定 bonus makes の+で(AUX) path win
  setCell(t, EPOS::AuxCopulaDa, EPOS::ParticleConj, cost::kStrongBonus);

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

  // Noun → ParticleTopic (机+は/も) - minor bonus
  // Helps サイズ+は+あり win over サイズ+はあり (particle-starting verb)
  setCell(t, EPOS::Noun, EPOS::ParticleTopic, cost::kMinorBonus);

  // Noun → ParticleAdverbial (そん+だけ, あん+だけ) - strong bonus
  // Ensures そんだけ → そん+だけ over そん+だ+け
  setCell(t, EPOS::Noun, EPOS::ParticleAdverbial, cost::kStrongBonus);

  // Noun → AdjNaAdj (一番+獰猛, とても+大切) - strong bonus
  // Prevents long kanji noun from absorbing na-adjective stem
  // (e.g., 一番獰猛+な → 一番+獰猛+な)
  setCell(t, EPOS::Noun, EPOS::AdjNaAdj, cost::kStrongBonus);

  // NounFormal → ParticleConj (こと+が) - neutral
  setCell(t, EPOS::NounFormal, EPOS::ParticleCase, cost::kNeutral);

  // NounFormal → AuxCopulaDa (はず+だ, つもり+だ, ところ+だ) - moderate bonus
  // Ensures formal noun + だ split over verb candidate (e.g., はずだ as VERB)
  setCell(t, EPOS::NounFormal, EPOS::AuxCopulaDa, cost::kModerateBonus);

  // NounFormal → AuxCopulaDesu (はず+です, つもり+です) - strong bonus
  // Ensures はずです → はず+です over はず+で+す
  setCell(t, EPOS::NounFormal, EPOS::AuxCopulaDesu, cost::kStrongBonus);

  // NounFormal → AuxNegativeNai (こと+ない) - moderate bonus
  // Ensures こと+ない over こと+な+い (な=AuxCopulaDa連用形)
  setCell(t, EPOS::NounFormal, EPOS::AuxNegativeNai, cost::kModerateBonus);

  // NounFormal → AdjBasic (こと+ない adjective) - very strong bonus
  // Ensures そんなこと+ない(ADJ) wins over そんなこと+ない(AUX)
  // When ない follows a formal noun, it's the existence-negation adjective
  // Needs to overcome: AUX word cost (0.3) + AuxNegativeNai bonus (-0.5) = -0.2
  // vs ADJ word cost (0.5) + this bonus = must be < -0.2
  setCell(t, EPOS::NounFormal, EPOS::AdjBasic, cost::kVeryStrongBonus);

  // NounFormal → ParticleAdverbial (こと+だけ, はず+だけ) - strong bonus
  // Prevents ことだけ → こと+だ+け split
  setCell(t, EPOS::NounFormal, EPOS::ParticleAdverbial, cost::kStrongBonus);

  // =========================================================================
  // Pronoun → Particles
  // =========================================================================

  // Pronoun → ParticleCase (あれ+が, これ+を, それ+に) - moderate bonus
  // Pronouns naturally take case particles; beats VERB_連用 interpretation
  // E.g., あれが欲しい → あれ(PRON)+が, not あれ(VERB ある)+が
  setCell(t, EPOS::Pronoun, EPOS::ParticleCase, cost::kModerateBonus);

  // Pronoun → AuxCopulaDesu (何+です, これ+です) - moderate bonus
  // Pronouns naturally take polite copula; matches Noun→AuxCopulaDesu bonus
  setCell(t, EPOS::Pronoun, EPOS::AuxCopulaDesu, cost::kModerateBonus);

  // Pronoun → ParticleAdverbial (あれ+だけ, それ+だけ)
  setCell(t, EPOS::Pronoun, EPOS::ParticleAdverbial, cost::kStrongBonus);

  // =========================================================================
  // Determiner → Noun (連体詞は名詞を修飾)
  // =========================================================================

  // Determiner → Noun (そんな+こと, こんな+話) - very strong bonus
  // Ensures そんなことない → そんな+こと+ない over そん+な+こと+ない
  // Ensures あんな+人 over あん+な+人 (NOUN→AUX_断定→NOUN chain has -2.5 total)
  constexpr float kDeterminerNounBonus = -2.5F;
  setCell(t, EPOS::Determiner, EPOS::Noun, kDeterminerNounBonus);
  setCell(t, EPOS::Determiner, EPOS::NounFormal, kDeterminerNounBonus);
  setCell(t, EPOS::Determiner, EPOS::NounProper, kDeterminerNounBonus);

  // =========================================================================
  // Noun → Verb (サ変動詞パターン)
  // =========================================================================

  // Noun → VerbRenyokei (得+し for サ変動詞 得する) - moderate bonus
  // This favors 名詞+し split over 名詞し as single token
  setCell(t, EPOS::Noun, EPOS::VerbRenyokei, cost::kModerateBonus);

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

  // Noun → AuxAspectIru (驚+い) - moderate penalty
  // Nouns don't directly connect to いる auxiliary; need particle (彼が+いる)
  // Prevents 驚+い+た from beating 驚い+た for verb onbin form
  setCell(t, EPOS::Noun, EPOS::AuxAspectIru, cost::kRare);

  // NaAdj → AuxCopulaDa (静か+だ) - strong bonus
  setCell(t, EPOS::AdjNaAdj, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // NaAdj → AuxCopulaDesu (静か+です) - strong bonus
  setCell(t, EPOS::AdjNaAdj, EPOS::AuxCopulaDesu, cost::kStrongBonus);

  // Adverb → AuxCopulaDa/Desu - penalty (adverbs don't directly take copula)
  // E.g., そうです: そう should be na-adjective, not adverb
  // Adverbs modify verbs/adjectives, not replace them before copula
  setCell(t, EPOS::Adverb, EPOS::AuxCopulaDa, cost::kRare);
  setCell(t, EPOS::Adverb, EPOS::AuxCopulaDesu, cost::kRare);

  // AuxCopulaDa → Noun (さすがな+人, 静かな+部屋) - strong bonus
  // Copula な(連体形 of だ) + Noun is the na-adjective attributive pattern
  setCell(t, EPOS::AuxCopulaDa, EPOS::Noun, cost::kStrongBonus);

  // AdjStem → Suffix (な+さ in なさそう) - strong bonus for nominalization
  // This favors な(ADJ stem of ない) + さ(nominalization suffix) over さ(する mizenkei)
  setCell(t, EPOS::AdjStem, EPOS::Suffix, cost::kStrongBonus);

  // VerbRenyokei → Suffix (遅れ+がち, 忘れ+がち) - strong bonus
  // This favors verb renyokei + suffix pattern over merged tokens
  setCell(t, EPOS::VerbRenyokei, EPOS::Suffix, cost::kStrongBonus);

  // =========================================================================
  // Particle → Various (Particles can connect to many things)
  // =========================================================================

  // ParticleAdverbial → ParticleCase (だけ+で, ばかり+に, ほど+に) - very strong bonus
  // Without this, PART_副→VERB_連用 bonus makes で(出る) beat で(格助詞) after だけ
  // Needs to overcome: base bigram diff (0.5-0.2=0.3) + VERB_連用 bonus (-0.8)
  setCell(t, EPOS::ParticleAdverbial, EPOS::ParticleCase, cost::kVeryStrongBonus);

  // ParticleAdverbial → ParticleNo (など+の, まで+の, ばかり+の)
  // Very strong bonus: adverbial particle + の is extremely natural.
  // Needs to overcome DET→NOUN bonus (-2.5) when competing with な+どの path.
  setCell(t, EPOS::ParticleAdverbial, EPOS::ParticleNo, cost::kVeryStrongBonus);

  // ParticleAdverbial → VerbRenyokei (かも+しれ in かもしれない) - strong bonus
  // This favors かも+しれ+ない over か+もし+れない
  setCell(t, EPOS::ParticleAdverbial, EPOS::VerbRenyokei, cost::kStrongBonus);

  // ParticleAdverbial → VerbShuushikei (でも+行く) - strong bonus
  // This favors でも+行く over で+も+行く
  setCell(t, EPOS::ParticleAdverbial, EPOS::VerbShuushikei, cost::kStrongBonus);

  // ParticleCase → Adverb (か+もし) - moderate penalty
  // This discourages splitting かもしれない as か+もし+れない
  setCell(t, EPOS::ParticleCase, EPOS::Adverb, cost::kRare);

  // ParticleCase → Noun (が+学生) - neutral
  setCell(t, EPOS::ParticleCase, EPOS::Noun, cost::kNeutral);

  // ParticleCase → VerbShuushikei (を+食べる) - neutral
  setCell(t, EPOS::ParticleCase, EPOS::VerbShuushikei, cost::kNeutral);

  // ParticleTopic → VerbShuushikei (は+食べる) - neutral
  setCell(t, EPOS::ParticleTopic, EPOS::VerbShuushikei, cost::kNeutral);

  // ParticleTopic → VerbRenyokei (は+あり in はありますか) - minor bonus
  // Helps は+あり+ます beat はあり+ます (particle-starting verb)
  // Note: ParticleCase → VerbRenyokei is intentionally not added to avoid
  // breaking という patterns (と is ParticleCase, いう is VerbRenyokei)
  setCell(t, EPOS::ParticleTopic, EPOS::VerbRenyokei, cost::kMinorBonus);

  // ParticleTopic → AdjBasic (は+良い, も+美しい) - minor bonus
  // Common pattern: 係助詞 followed by i-adjective
  setCell(t, EPOS::ParticleTopic, EPOS::AdjBasic, cost::kMinorBonus);

  // ParticleConj → VerbShuushikei (て+食べる for compound verbs) - minor penalty
  // (te-form usually followed by auxiliary, not new verb)
  setCell(t, EPOS::ParticleConj, EPOS::VerbShuushikei, cost::kUncommon);

  // ParticleConj → AuxAspectIru (て+いる) - strong bonus for MeCab-compatible split
  // Allows 食べ+て+いる to beat unified 食べて+いる path
  setCell(t, EPOS::ParticleConj, EPOS::AuxAspectIru, cost::kStrongBonus);

  // ParticleConj → AuxAspectShimau (て+しまう) - strong bonus
  setCell(t, EPOS::ParticleConj, EPOS::AuxAspectShimau, cost::kStrongBonus);

  // ParticleConj → AuxAspectOku (て+おく) - strong bonus
  setCell(t, EPOS::ParticleConj, EPOS::AuxAspectOku, cost::kStrongBonus);

  // ParticleConj → AuxAspectMiru (て+みる) - strong bonus
  setCell(t, EPOS::ParticleConj, EPOS::AuxAspectMiru, cost::kStrongBonus);

  // ParticleConj → AuxAspectIku (て+いく) - strong bonus
  setCell(t, EPOS::ParticleConj, EPOS::AuxAspectIku, cost::kStrongBonus);

  // ParticleConj → AuxAspectKuru (て+くる) - strong bonus
  setCell(t, EPOS::ParticleConj, EPOS::AuxAspectKuru, cost::kStrongBonus);

  // =========================================================================
  // Penalties: Invalid or Rare Connections
  // =========================================================================

  // Suffix → Adverb (さ+そう) - strong penalty
  // Prevents なさそう → な + さ(SUFFIX) + そう(ADV) over correct な + さ + そう(AUX)
  // Suffix + Adverb is grammatically unusual; そう after さ is AuxAppearance
  setCell(t, EPOS::Suffix, EPOS::Adverb, cost::kVeryRare);

  // AuxAspectOku → AuxVolitional (とい+う) - strong penalty
  // Prevents とい+う from beating という (quotative determiner)
  // とい (contracted ておく form) + う (volitional) is grammatically invalid
  setCell(t, EPOS::AuxAspectOku, EPOS::AuxVolitional, cost::kVeryRare);

  // AuxAspectOku → ParticleQuote (とい+って) - strong penalty
  // Prevents とい+って from beating と+いっ+て (と言って)
  // とい (contracted ておく form) + って (quote particle) is unlikely in this context
  setCell(t, EPOS::AuxAspectOku, EPOS::ParticleQuote, cost::kVeryRare);

  // VerbShuushikei → AuxTenseMasu (食べる+ます) - prohibitive
  // (ます attaches to renyokei, not shuushikei)
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxTenseMasu, cost::kAlmostNever);

  // VerbShuushikei → AuxDesireTai (食べる+たい) - prohibitive
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxDesireTai, cost::kAlmostNever);

  // VerbShuushikei → AuxTenseTa (食べる+た) - prohibitive
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxTenseTa, cost::kAlmostNever);

  // AuxTenseTa → AuxTenseTa (た+た) - prohibitive
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxTenseTa, cost::kAlmostNever);

  // AuxTenseTa → AuxTenseMasu (た+ます) - prohibitive
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxTenseMasu, cost::kAlmostNever);

  // AuxTenseTa → AuxNegative (た+ない/ん) - prohibitive
  // Past tense cannot be followed by negation; correct order is negation→past
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxNegativeNai, cost::kAlmostNever);
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxNegativeNu, cost::kAlmostNever);

  // AuxTenseTa → AuxAspectKuru (た+き) - prohibitive
  // Prevents いただき → い+た+だ+き (きた split creates standalone き entry)
  setCell(t, EPOS::AuxTenseTa, EPOS::AuxAspectKuru, cost::kAlmostNever);

  // AuxCopulaDa → AuxAspectKuru (だ+き) - prohibitive
  // Prevents いただき → い+た+だ+き
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxAspectKuru, cost::kAlmostNever);

  // ParticleNo → AuxTenseTa (ん/の+た) - prohibitive
  // Nominalizer の/ん is followed by copula (のだ/のです), not past tense
  setCell(t, EPOS::ParticleNo, EPOS::AuxTenseTa, cost::kAlmostNever);

  // ParticleFinal → VerbShuushikei (ね+食べる) - strong penalty
  // (sentence-final particles rarely continue to verbs)
  setCell(t, EPOS::ParticleFinal, EPOS::VerbShuushikei, cost::kVeryRare);

  // ParticleFinal → VerbOnbinkei (な+いん) - prohibit
  // (prevents ないんだ → な+いん+だ over ない+ん+だ)
  setCell(t, EPOS::ParticleFinal, EPOS::VerbOnbinkei, cost::kAlmostNever);

  // ParticleFinal → VerbMizenkei (な+さ) - strong penalty
  // (prevents なさそう → な(終助詞)+さ(未然)+そう over な(形容詞)+さ(接尾辞)+そう)
  setCell(t, EPOS::ParticleFinal, EPOS::VerbMizenkei, cost::kVeryRare);

  // AuxCopulaDa → VerbOnbinkei (な+いん) - prohibit
  // (prevents ないんだ → な+いん+だ over ない+ん+だ)
  setCell(t, EPOS::AuxCopulaDa, EPOS::VerbOnbinkei, cost::kAlmostNever);

  // AuxCopulaDa → VerbMizenkei (だ+くさ) - strong penalty
  // Copula followed by verb mizenkei is grammatically unusual
  // Prevents 盛りだくさん → 盛り+だ+くさ+ん over dictionary entry
  setCell(t, EPOS::AuxCopulaDa, EPOS::VerbMizenkei, cost::kVeryRare);

  // AuxCopulaDa → VerbRenyokei/VerbShuushikei - penalty for copula + general verb
  // E.g., 公園で遊ぶ should be NOUN+PART_格+VERB, not NOUN+AUX_断定+VERB
  // Copula 「で」 rarely followed by general verbs (usually followed by ある/ない/ございます)
  // This helps PART_格(で)+VERB win over AUX_断定(で)+VERB
  setCell(t, EPOS::AuxCopulaDa, EPOS::VerbRenyokei, cost::kMinor);
  setCell(t, EPOS::AuxCopulaDa, EPOS::VerbShuushikei, cost::kMinor);

  // ParticleFinal → ParticleFinal (よ+ね) - minor bonus (common pattern)
  setCell(t, EPOS::ParticleFinal, EPOS::ParticleFinal, cost::kMinorBonus);

  // ParticleFinal → ParticleNo (か+の) - moderate bonus (indefinite pronoun pattern)
  // いくつかの, 何かの, 誰かの, どれかの - か functions as indefinite marker, not sentence-ender
  setCell(t, EPOS::ParticleFinal, EPOS::ParticleNo, cost::kModerateBonus);

  // =========================================================================
  // Copula → Negation (ではない pattern)
  // =========================================================================

  // AuxCopulaDa (で form) → ParticleTopic (で+は/も in ではない/でもない)
  // Moderate bonus to promote 彼女|で|も|ない over 彼女|でも|ない
  setCell(t, EPOS::AuxCopulaDa, EPOS::ParticleTopic, cost::kModerateBonus);

  // AuxCopulaDa → AuxNegativeNai (じゃ+ない, で+ない) - moderate bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxNegativeNai, cost::kModerateBonus);

  // AuxCopulaDa → AuxGozaru (で+ございます) - moderate bonus
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxGozaru, cost::kModerateBonus);

  // AuxGozaru → AuxTenseMasu (ござい+ます) - strong bonus to prevent verb candidate win
  // Without this, verb_candidates generates "ございる" which beats dictionary "ござる"
  setCell(t, EPOS::AuxGozaru, EPOS::AuxTenseMasu, cost::kStrongBonus);

  // AuxCopulaDa → AuxCopulaDa (で+ある/あれ) - strong bonus for である pattern
  // MeCab splits である as で(だ連用形) + ある(助動詞), not で(出る連用形) + ある
  setCell(t, EPOS::AuxCopulaDa, EPOS::AuxCopulaDa, cost::kStrongBonus);

  // =========================================================================
  // Appearance/Conjecture connections
  // =========================================================================

  // VerbRenyokei → AuxAppearanceSou (食べ+そう) - very strong bonus
  // Must beat adverb bonus (-1.0 for 2-char hiragana) to prefer auxiliary
  setCell(t, EPOS::VerbRenyokei, EPOS::AuxAppearanceSou, cost::kVeryStrongBonus);

  // Other → AuxAppearanceSou - penalty (様態そう shouldn't appear at BOS)
  // At sentence start, そう should be demonstrative na-adjective, not appearance aux
  setCell(t, EPOS::Other, EPOS::AuxAppearanceSou, cost::kMinor);

  // Other → AuxAspectIku - penalty (いく as aspect aux shouldn't appear at BOS)
  // At sentence start, いく should be verb (行く) or part of pronoun (いくつ)
  // AuxAspectIku is only valid after て-form (食べていく, 走っていく)
  setCell(t, EPOS::Other, EPOS::AuxAspectIku, cost::kRare);

  // Particle → AuxAppearanceSou - penalty (様態そう shouldn't follow particles)
  // E.g., そうかもしれません: そう is demonstrative, not appearance auxiliary
  setCell(t, EPOS::ParticleCase, EPOS::AuxAppearanceSou, cost::kMinor);
  setCell(t, EPOS::ParticleTopic, EPOS::AuxAppearanceSou, cost::kMinor);
  setCell(t, EPOS::ParticleAdverbial, EPOS::AuxAppearanceSou, cost::kMinor);
  setCell(t, EPOS::ParticleQuote, EPOS::AuxAppearanceSou, cost::kMinor);

  // AdjBasic → AuxConjectureRashii (美しい+らしい) - strong bonus
  setCell(t, EPOS::AdjBasic, EPOS::AuxConjectureRashii, cost::kStrongBonus);

  // VerbShuushikei → AuxConjectureRashii (食べる+らしい) - moderate bonus
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxConjectureRashii, cost::kModerateBonus);

  // VerbShuushikei → AuxAppearanceSou (食べる+そう hearsay) - strong bonus
  // Hearsay そう (伝聞) attaches to 終止形: 食べる+そうだ, する+そうです
  // Different from appearance そう (様態) which attaches to 連用形: 食べ+そう, し+そう
  setCell(t, EPOS::VerbShuushikei, EPOS::AuxAppearanceSou, cost::kStrongBonus);

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

  // VerbTaForm → VerbMizenkei (盛りだ+くさ) - strong penalty
  // Two verbs in sequence without auxiliary/particle is grammatically unusual
  // Prevents 盛りだくさん → 盛りだ+くさ+ん over dictionary entry
  setCell(t, EPOS::VerbTaForm, EPOS::VerbMizenkei, cost::kVeryRare);

  // VerbRenyokei → VerbMizenkei (盛り+だくさ) - strong penalty
  // Renyokei followed by mizenkei is grammatically unusual
  // Legitimate patterns like 食べ+すぎ use Renyokei→Renyokei (すぎ is renyokei)
  setCell(t, EPOS::VerbRenyokei, EPOS::VerbMizenkei, cost::kVeryRare);

  // AdjBasic → VerbMizenkei (盛りだく+さ) - strong penalty
  // Adjective 終止形 followed by verb 未然形 is grammatically unusual
  // Prevents 盛りだくさん → 盛りだく+さ+ん over dictionary entry
  setCell(t, EPOS::AdjBasic, EPOS::VerbMizenkei, cost::kVeryRare);

  // AdjStem → AuxConjectureMitai: unnatural (美し+みたい should be 美しい+みたい)
  setCell(t, EPOS::AdjStem, EPOS::AuxConjectureMitai, cost::kAlmostNever);

  // AdjStem → AuxConjectureRashii: unnatural (美し+らしい should be 美しい+らしい)
  setCell(t, EPOS::AdjStem, EPOS::AuxConjectureRashii, cost::kAlmostNever);

  // AdjStem → AdjBasic: prohibit (好+みらしい should be 好み+らしい)
  // Adjective stems don't connect to unrelated i-adjective endings
  setCell(t, EPOS::AdjStem, EPOS::AdjBasic, cost::kAlmostNever);

  // AdjStem → Verb/Aux: prohibit (な+い should not split ない as な(AdjStem)+い)
  // な(AdjStem of ない) should only connect to さ(nominalization) or そう(appearance)
  // Also prevents 高+すぎた winning over 高+すぎ+た
  setCell(t, EPOS::AdjStem, EPOS::VerbRenyokei, cost::kAlmostNever);
  setCell(t, EPOS::AdjStem, EPOS::VerbShuushikei, cost::kAlmostNever);
  setCell(t, EPOS::AdjStem, EPOS::VerbMizenkei, cost::kAlmostNever);
  setCell(t, EPOS::AdjStem, EPOS::VerbTaForm, cost::kAlmostNever);
  setCell(t, EPOS::AdjStem, EPOS::VerbTaraForm, cost::kAlmostNever);
  setCell(t, EPOS::AdjStem, EPOS::AuxAspectIru, cost::kAlmostNever);  // な+い(いる)
  setCell(t, EPOS::AdjNaAdj, EPOS::AuxAspectIru, cost::kAlmostNever); // 性的+い(いる)
  setCell(t, EPOS::AdjStem, EPOS::AuxNegativeNai, cost::kAlmostNever);   // な+ない
  setCell(t, EPOS::AdjStem, EPOS::Other, cost::kAlmostNever);           // な+い(OTHER)

  // Note: Particle → AdjStem is allowed for patterns like やる気がなさそう (が+な+さ+そう)

  // =========================================================================
  // Particle → Particle penalties (unnatural adjacent particle chains)
  // =========================================================================
  // These particle combinations never occur adjacent in valid Japanese.
  // Penalizing them helps hiragana words (はし, もも, かし) compete against
  // false particle-chain interpretations (は+し, も+も, か+し).

  // PART_係 → PART_接続 (は+し, も+て): topic particle directly followed by
  // conjunctive particle is grammatically invalid (need content between them)
  setCell(t, EPOS::ParticleTopic, EPOS::ParticleConj, cost::kRare);

  // PART_係 → PART_格 (は+が, は+を, も+に): topic+case markers never stack
  // adjacent on the same phrase (は...が with content between is fine)
  setCell(t, EPOS::ParticleTopic, EPOS::ParticleCase, cost::kRare);

  // PART_係 → PART_係 (は+も, も+は): double topic marking never adjacent
  setCell(t, EPOS::ParticleTopic, EPOS::ParticleTopic, cost::kVeryRare);

  // PART_格 → PART_格 (が+を, を+に, に+で): case particles never stack
  setCell(t, EPOS::ParticleCase, EPOS::ParticleCase, cost::kVeryRare);

  // Note: PART_格 → PART_係 (に+は, で+は, と+は) is valid Japanese,
  // so we intentionally do NOT penalize ParticleCase → ParticleTopic.

  // =========================================================================
  // Particle → Other penalties (prevents over-segmentation of hiragana words)
  // =========================================================================
  // Patterns like も+ちろん, と+にかく are not valid Japanese morphology
  // Single-char particles followed by unknown hiragana are usually misanalyses

  // ParticleTopic → Other: penalty (も+ちろん is invalid)
  setCell(t, EPOS::ParticleTopic, EPOS::Other, cost::kRare);

  // ParticleCase → Other: penalty (と+にかく, に+かく are invalid)
  setCell(t, EPOS::ParticleCase, EPOS::Other, cost::kRare);

  // ParticleFinal → Other: penalty (ね+random, よ+random are invalid)
  setCell(t, EPOS::ParticleFinal, EPOS::Other, cost::kRare);

  // ParticleConj → Other: minor penalty (て+random at sentence start is unlikely)
  // Less penalty than others because て+noun/verb is valid in some contexts
  setCell(t, EPOS::ParticleConj, EPOS::Other, cost::kUncommon);

  // =========================================================================
  // Conjunction → Auxiliary penalties
  // =========================================================================
  // Conjunctions like でも/だって typically don't directly precede auxiliaries
  // 彼女でもない should be 彼女|で|も|ない (copula+particle) not 彼女|でも(CONJ)|ない

  // Conjunction → AuxNegativeNai: strong penalty (でも+ない is invalid as CONJ+AUX)
  setCell(t, EPOS::Conjunction, EPOS::AuxNegativeNai, cost::kVeryRare);

  // Conjunction → ParticleFinal: moderate penalty (でも+な is invalid)
  // Conjunctions don't typically connect to final particles mid-sentence
  setCell(t, EPOS::Conjunction, EPOS::ParticleFinal, cost::kRare);

  // Conjunction → VerbShuushikei/VerbRenyokei: strong bonus (でも+行く)
  // This favors でも+行く as single CONJ+VERB over で+も+行く
  // Note: PART_副 path is preferred but Viterbi may prune it,
  // so CONJ path serves as fallback for demo+verb patterns
  setCell(t, EPOS::Conjunction, EPOS::VerbShuushikei, cost::kStrongBonus);
  setCell(t, EPOS::Conjunction, EPOS::VerbRenyokei, cost::kStrongBonus);

  // Conjunction → Adjective: strong bonus (でも高い, それでも安い)
  // Adjectives after conjunctions are natural; without this, VERB candidates win
  setCell(t, EPOS::Conjunction, EPOS::AdjBasic, cost::kStrongBonus);
  setCell(t, EPOS::Conjunction, EPOS::AdjStem, cost::kStrongBonus);
  setCell(t, EPOS::Conjunction, EPOS::AdjRenyokei, cost::kStrongBonus);

  // =========================================================================
  // Interjection connections
  // =========================================================================

  // Adverb → Interjection (いったい+何だ) - strong bonus
  // いったい何だ should tokenize as いったい + 何だ, not いったい + 何 + だ
  setCell(t, EPOS::Adverb, EPOS::Interjection, cost::kStrongBonus);

  // Interjection → AuxGozaru (おはよう+ござい+ます) - strong bonus
  // Greetings like おはようございます need this to prefer dict AuxGozaru over verb candidate
  setCell(t, EPOS::Interjection, EPOS::AuxGozaru, cost::kStrongBonus);

  // Adverb → Noun (俄然+注目) - moderate bonus
  // Adverb modifying noun is natural and should beat kanji compound analysis
  setCell(t, EPOS::Adverb, EPOS::Noun, cost::kModerateBonus);

  // Adverb → Adjective (とても+面白い, 非常に+難しい) - strong bonus
  // Adverbs very commonly modify adjectives; should prefer ADJ over NOUN
  setCell(t, EPOS::Adverb, EPOS::AdjBasic, cost::kStrongBonus);
  setCell(t, EPOS::Adverb, EPOS::AdjRenyokei, cost::kStrongBonus);
  setCell(t, EPOS::Adverb, EPOS::AdjNaAdj, cost::kStrongBonus);
  setCell(t, EPOS::Adverb, EPOS::AdjKatt, cost::kStrongBonus);

  // Adverb → AdjStem (さすが+な) - strong penalty
  // Prevents さすが(ADV)+な(AdjStem of ない); should be ADV+な(AuxCopulaDa連体形)
  setCell(t, EPOS::Adverb, EPOS::AdjStem, cost::kVeryRare);

  // Adverb → Verb (たまたま+見つけ, すぐ+食べ) - moderate bonus
  // Adverb modifying verb is natural; prefer dictionary compound over split
  setCell(t, EPOS::Adverb, EPOS::VerbRenyokei, cost::kModerateBonus);
  setCell(t, EPOS::Adverb, EPOS::VerbShuushikei, cost::kModerateBonus);
  setCell(t, EPOS::Adverb, EPOS::VerbTaForm, cost::kModerateBonus);

  // Prefix → Noun (お+待ち, ご+確認) - strong bonus
  // Honorific prefix + noun is very common and should beat combined forms
  setCell(t, EPOS::Prefix, EPOS::Noun, cost::kStrongBonus);

  // Prefix → VerbRenyokei (お+待ち as verb renyokei) - strong bonus
  // お待ち can be verb renyokei (待つ) as well as noun
  setCell(t, EPOS::Prefix, EPOS::VerbRenyokei, cost::kStrongBonus);

  return t;
}

// Initialize the static table
const std::array<std::array<float, BigramTable::kSize>, BigramTable::kSize>
    BigramTable::table_ = BigramTable::initTable();

}  // namespace suzume::analysis
