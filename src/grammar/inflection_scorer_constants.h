#ifndef SUZUME_GRAMMAR_INFLECTION_SCORER_CONSTANTS_H_
#define SUZUME_GRAMMAR_INFLECTION_SCORER_CONSTANTS_H_

// =============================================================================
// Inflection Scorer Constants
// =============================================================================
// Confidence adjustment values for morphological analysis.
// Positive values increase confidence, negative values decrease it.
//
// Naming convention:
//   kBonus*   - positive adjustment (encourages pattern)
//   kPenalty* - negative adjustment (discourages pattern)
//   kBase*    - base/threshold values
// =============================================================================

namespace suzume::grammar::inflection {

// =============================================================================
// Base Configuration
// =============================================================================

// Starting confidence for all analysis candidates
constexpr float kBaseConfidence = 0.6F;

// Confidence bounds (floor and ceiling)
constexpr float kConfidenceFloor = 0.3F;
constexpr float kConfidenceCeiling = 0.95F;

// =============================================================================
// Stem Length Adjustments
// =============================================================================

// Very long stems (12+ bytes / 4+ chars) are suspicious
constexpr float kPenaltyStemVeryLong = 0.25F;

// Long stems (9-11 bytes / 3 chars) are slightly suspicious
constexpr float kPenaltyStemLong = 0.15F;

// 2-char stems (6 bytes) are common - small boost
constexpr float kBonusStemTwoChar = 0.02F;

// 1-char stems (3 bytes) are possible but less common
constexpr float kBonusStemOneChar = 0.01F;

// Bonus per byte of auxiliary chain matched
constexpr float kBonusAuxLengthPerByte = 0.02F;

// =============================================================================
// Ichidan Validation
// =============================================================================

// 2-char potential form pattern (kanji + e-row) in potential context
// E.g., 読め could be 読める (Ichidan) or 読む potential
constexpr float kPenaltyIchidanPotentialAmbiguity = 0.35F;

// E-row ending (食べ, 見せ) confirms Ichidan
constexpr float kBonusIchidanERow = 0.12F;

// Stem matches Godan conjugation pattern in this context
constexpr float kPenaltyIchidanLooksGodan = 0.15F;

// Ichidan stem ending in kanji + い in renyokei context
// Pattern: 手伝い+ます → 手伝いる (wrong) should be 手伝+います → 手伝う
// This is much more suspicious than generic "looks godan" pattern
// Real ichidan verbs ending in い (用いる) are very rare
constexpr float kPenaltyIchidanKanjiI = 0.35F;

// Single kanji stem with single long aux (causative-passive pattern)
// E.g., 見させられた (legitimate Ichidan)
constexpr float kBonusIchidanCausativePassive = 0.10F;

// Single kanji stem with multiple aux or short single aux
// Likely wrong match via される pattern
constexpr float kPenaltyIchidanSingleKanjiMultiAux = 0.30F;

// Kanji + single hiragana stem pattern (人い, 玉い)
// Real Ichidan verbs have kanji-only stems (見る) or pure hiragana (いる)
// This pattern is likely NOUN + verb misanalysis
constexpr float kPenaltyIchidanKanjiHiraganaStem = 0.50F;

// く/す/こ as Ichidan stem - these are irregular verbs
constexpr float kPenaltyIchidanIrregularStem = 0.60F;

// =============================================================================
// GodanRa Validation
// =============================================================================

// Single hiragana stem (み, き, に) - likely Ichidan, not GodanRa
constexpr float kPenaltyGodanRaSingleHiragana = 0.30F;

// In kVerbKatei context, i-row ending stems like 起き (from 起きる) are Ichidan
// GodanRa verbs typically have kanji-only stems in this context (走れば → 走)
constexpr float kBonusIchidanKateiIRow = 0.12F;
constexpr float kPenaltyGodanRaKateiIRow = 0.10F;

// =============================================================================
// GodanWa/Ra/Ta Disambiguation
// =============================================================================

// Multi-kanji stem with っ-onbin - boost GodanWa
constexpr float kBonusGodanWaMultiKanji = 0.10F;

// Multi-kanji stem with っ-onbin - slight penalty for GodanRa/Ta
constexpr float kPenaltyGodanRaTaMultiKanji = 0.05F;

// =============================================================================
// Kuru Validation
// =============================================================================

// Any stem other than 来 or empty is invalid for Kuru
constexpr float kPenaltyKuruInvalidStem = 0.25F;

// =============================================================================
// I-Adjective Validation
// =============================================================================

// Single-kanji I-adjective stems are very rare
constexpr float kPenaltyIAdjSingleKanji = 0.25F;

// Stem contains verb+auxiliary patterns (てしま, ている, etc.)
constexpr float kPenaltyIAdjVerbAuxPattern = 0.45F;

// Stem ends with し (難しい, 美しい) with auxiliaries
constexpr float kBonusIAdjShiiStem = 0.15F;

// Verb renyokei + やすい/にくい compound pattern
constexpr float kBonusIAdjCompoundYasuiNikui = 0.35F;

// 3+ kanji stem - likely サ変名詞 misanalysis
constexpr float kPenaltyIAdjAllKanji = 0.40F;

// E-row ending - typical of Ichidan verb, not I-adjective
constexpr float kPenaltyIAdjERowStem = 0.35F;

// Verb mizenkei + a-row pattern (食べな, 読ま)
constexpr float kPenaltyIAdjMizenkeiPattern = 0.30F;

// Kanji + i-row pattern (godan verb renyokei)
constexpr float kPenaltyIAdjGodanRenyokeiPattern = 0.30F;

// Single-kanji + な that's NOT a known adjective (少な, 危な)
constexpr float kPenaltyIAdjVerbNegativeNa = 0.35F;

// =============================================================================
// Onbinkei (音便) Context Validation
// =============================================================================

// a-row ending in onbinkei context - suspicious for most Godan verbs
constexpr float kPenaltyOnbinkeiARowStem = 0.30F;

// E-row ending in onbinkei context for non-Ichidan - likely Ichidan stem
constexpr float kPenaltyOnbinkeiERowNonIchidan = 0.35F;

// =============================================================================
// All-Kanji Stem Validation
// =============================================================================

// Multi-kanji stem with non-Suru type in conditional form
constexpr float kPenaltyAllKanjiNonSuruKatei = 0.10F;

// Multi-kanji stem with non-Suru type in other contexts
constexpr float kPenaltyAllKanjiNonSuruOther = 0.40F;

// =============================================================================
// Godan Potential Validation
// =============================================================================

// Boost for Godan potential interpretation
constexpr float kBonusGodanPotential = 0.10F;

// GodanBa potential - very rare compared to Ichidan べる verbs
constexpr float kPenaltyGodanBaPotential = 0.25F;

// Compound pattern (aux_count >= 2) - likely Ichidan, not Godan potential
constexpr float kPenaltyPotentialCompoundBase = 0.15F;
constexpr float kPenaltyPotentialCompoundPerAux = 0.05F;
constexpr float kPenaltyPotentialCompoundMax = 0.35F;

// =============================================================================
// Te-Form Validation
// =============================================================================

// Short te-form (て/で alone) with な-adjective-like stem
constexpr float kPenaltyTeFormNaAdjective = 0.40F;

// =============================================================================
// Suru vs GodanSa Disambiguation
// =============================================================================

// 2-kanji stem with Suru in し-context
constexpr float kBonusSuruTwoKanji = 0.20F;

// 2-kanji stem with GodanSa in し-context
constexpr float kPenaltyGodanSaTwoKanji = 0.30F;

// 3+ kanji stem with Suru in し-context
constexpr float kBonusSuruLongStem = 0.05F;

// Single-kanji stem - prefer GodanSa (出す, 消す)
constexpr float kBonusGodanSaSingleKanji = 0.10F;

// Single-kanji stem with Suru - penalize
constexpr float kPenaltySuruSingleKanji = 0.15F;

// =============================================================================
// Single Hiragana Stem Particle Penalty
// =============================================================================

// Single-hiragana stem (も, は, が, etc.) in mizenkei context
// These are common particles, not verb stems
// E.g., もない → も(PARTICLE) + ない(AUX), not もる(VERB)
constexpr float kPenaltyIchidanSingleHiraganaParticleStem = 0.45F;

// Pure hiragana verb stems (multiple chars) are rare
// Most real verbs have kanji stems or are in dictionary
// E.g., もな(う), なまむ are not real verbs
// Exception: some valid hiragana verbs like いる, ある are in dictionary
constexpr float kPenaltyPureHiraganaStem = 0.35F;

// Single-hiragana Godan stem penalty
// E.g., まむ has stem ま which is not a real verb
// Real verbs like み(る), き(る) are Ichidan, handled by dictionary
constexpr float kPenaltyGodanSingleHiraganaStem = 0.40F;

// Godan (non-Ra) pure hiragana multi-char stem penalty
// Coined verbs use GodanRa (ググる, ディスる), never other types
// Real Godan verbs are written in kanji (読む, 泳ぐ, 話す, etc.)
// Pure hiragana stems like なま(む), まむ(ぐ) are almost never real
constexpr float kPenaltyGodanNonRaPureHiraganaStem = 0.45F;

// =============================================================================
// Suru/Kuru Imperative Boost
// =============================================================================

// Empty stem with Suru/Kuru imperative (しろ, せよ, こい)
// These must win over competing Ichidan/Godan interpretations
constexpr float kBonusSuruKuruImperative = 0.05F;

// =============================================================================
// Volitional Form Validation
// =============================================================================

// Ichidan volitional with godan-like stem ending (く, す, etc.)
// E.g., 続く + よう → 続くる (wrong) - should be 続こう
// True ichidan volitional: 食べ + よう = 食べよう (e-row ending)
// Godan volitional: 書 + こ + う = 書こう (o-row stem)
constexpr float kPenaltyIchidanVolitionalGodanStem = 0.50F;

// =============================================================================
// Additional Inline Penalties (Extracted from inflection_scorer.cpp)
// =============================================================================

// Small kana (拗音) cannot start a verb stem - grammatically impossible
// ょ, ゃ, ゅ, ぁ, ぃ, ぅ, ぇ, ぉ are always part of compound sounds
constexpr float kPenaltySmallKanaStemInvalid = 1.0F;

// Ichidan verbs do NOT have onbin (音便) forms
// Ichidan te-form uses renyokei + て, Godan uses onbinkei + て/で
constexpr float kPenaltyIchidanOnbinInvalid = 0.50F;

// Ichidan stems ending in て as base form (来て→来てる) is usually wrong
// Exception: 捨てる, 棄てる have legitimate て-ending stems
constexpr float kPenaltyIchidanTeStemBaseInvalid = 0.50F;

// All-kanji + で patterns are usually copula, not verb stems
// e.g., 公園で = NOUN + copula, 嫌でない = 嫌 + で + ない
constexpr float kPenaltyIchidanCopulaDePattern = 0.70F;

// Ichidan stems cannot end in u-row hiragana (う, く, す, つ, etc.)
// U-row endings are Godan dictionary forms (読む, 書く, 話す, etc.)
constexpr float kPenaltyIchidanURowStemInvalid = 0.50F;

// Single-kanji Ichidan stem with onbinkei context (侍で as Ichidan) is wrong
constexpr float kPenaltyIchidanSingleKanjiOnbinInvalid = 0.60F;

// Particle + な stem pattern for GodanWa (もな, はな, etc.)
// These are likely PARTICLE + ない misparse
constexpr float kPenaltyGodanWaParticleNaStem = 0.45F;

// I-adjective stems ending with "づ" are invalid (verb onbin pattern)
constexpr float kPenaltyIAdjZuStemInvalid = 0.50F;

// Ichidan stem that looks like noun + い pattern in mizenkei context
// 間違いない → 間違い(NOUN) + ない(AUX), not 間違いる(VERB)
constexpr float kPenaltyIchidanNounIMizenkei = 0.30F;

// Suru stems containing te-form markers (て/で) are invalid
// E.g., "基づいて処理" should be verb te-form + noun
constexpr float kPenaltySuruTeFormStemInvalid = 0.80F;

}  // namespace suzume::grammar::inflection

#endif  // SUZUME_GRAMMAR_INFLECTION_SCORER_CONSTANTS_H_
