#ifndef SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
#define SUZUME_ANALYSIS_SCORER_CONSTANTS_H_

// =============================================================================
// Scorer Constants
// =============================================================================
// This file centralizes all penalty and bonus values used in scoring.
// Rationale and tuning notes are documented alongside each constant.
//
// Naming convention:
//   kPenalty* - increases cost (discourages pattern)
//   kBonus*   - decreases cost (encourages pattern)
//
// =============================================================================
// Cost Scale Reference
// =============================================================================
// The scoring system uses additive costs where lower = preferred.
// Typical cost ranges across the codebase:
//
//   [-2.5, -0.5] Boosted patterns (ごろ suffix, common contractions)
//   [0.0,  0.3]  Very common closed-class items (particles, copula)
//   [0.3,  0.5]  Common functional words (aux verbs, pronouns)
//   [0.5,  0.8]  Less frequent particles, binding words
//   [1.0,  1.5]  Standard open-class cost, mild penalties
//   [1.5,  2.0]  Moderate penalties for questionable patterns
//   [2.0,  3.0]  Strong penalties for grammatically invalid patterns
//
// Penalty/Bonus magnitudes:
//   0.5  - Small adjustment, tips the scale
//   0.8  - Medium adjustment, offsets base connection costs
//   1.0  - Standard penalty/bonus
//   1.5  - Strong preference/discouragement
//   2.0+ - Near-prohibition of pattern
//
// Base connection costs (from scorer.cpp):
//   NOUN→NOUN: 0.0, VERB→VERB: 0.8, NOUN→VERB: 0.2, etc.
// =============================================================================

namespace suzume::analysis::scorer {

// =============================================================================
// Edge Costs (Unigram penalties for invalid patterns)
// =============================================================================

// Note: kPenaltyVerbSou and kPenaltyVerbSouDesu were removed
// to unify verb+そう as single token (走りそう → 走る, like 食べそう → 食べる)
// See: backup/technical_debt_action_plan.md section 3.3

// Unknown adjective ending with そう but invalid lemma
// Valid: おいしそう (lemma おいしい), Invalid: 食べそう (lemma 食べい)
constexpr float kPenaltyInvalidAdjSou = 1.5F;

// Unknown adjective with たい pattern where stem is invalid
// E.g., りたかった is invalid (り is not a valid verb stem)
constexpr float kPenaltyInvalidTaiPattern = 2.0F;

// Unknown adjective containing verb+auxiliary patterns
// E.g., 食べすぎてしまい should be verb+しまう, not adjective
constexpr float kPenaltyVerbAuxInAdj = 2.0F;

// しまい/じまい parsed as adjective (should be しまう renyokei)
constexpr float kPenaltyShimaiAsAdj = 3.0F;

// Adjective lemma containing verb onbin + contraction patterns (んどい, んとい)
// E.g., 読んどい from 読んどく - invalid adjective, should be verb とく contraction
constexpr float kPenaltyVerbOnbinAsAdj = 2.0F;

// Short-stem pure hiragana unknown adjective penalty
// Valid short hiragana adjectives (すごい, うまい, やばい) are in dictionary
// Unknown short-stem (≤2 chars) hiragana adjectives are likely misanalysis
// E.g., いしい (stem いし = 2 chars) from お+いしい is not a real adjective
// But おいしい (stem おいし = 3 chars) should not be penalized
constexpr float kPenaltyShortStemHiraganaAdj = 3.0F;

// =============================================================================
// Connection Costs (Bigram penalties/bonuses)
// =============================================================================

// Copula (だ/です) after verb without そう pattern
// This is grammatically invalid in most cases
constexpr float kPenaltyCopulaAfterVerb = 3.0F;

// Ichidan renyokei + て/てV split (should be te-form)
// E.g., 教え + て should be 教えて
constexpr float kPenaltyIchidanRenyokeiTe = 1.5F;

// たい adjective after verb renyokei - this is valid
// E.g., 食べたい, 読みたい
constexpr float kBonusTaiAfterRenyokei = 0.8F;

// 安い (やすい) interpreted as cheap after renyokei-like noun
// Should be verb + やすい (easy to do)
constexpr float kPenaltyYasuiAfterRenyokei = 2.0F;

// VERB + ながら split when verb is in renyokei
// Should be single token like 飲みながら
constexpr float kPenaltyNagaraSplit = 1.0F;

// NOUN + そう when noun looks like verb renyokei
// Should be verb renyokei + そう auxiliary
constexpr float kPenaltySouAfterRenyokei = 0.5F;

// AUX だ/です + character speech suffix split
// E.g., だ + にゃ should be だにゃ (single token)
constexpr float kPenaltyCharacterSpeechSplit = 1.0F;

// ADJ(連用形・く) + VERB(なる) pattern
// E.g., 美しく + なる - very common grammatical pattern
constexpr float kBonusAdjKuNaru = 0.5F;

// Compound verb auxiliary after renyokei-like noun
// E.g., 読み + 終わる should be verb renyokei + auxiliary
constexpr float kPenaltyCompoundAuxAfterRenyokei = 0.5F;

// Unknown adjective with lemma ending in ない where stem looks like verb mizenkei
// E.g., 走らなければ with lemma 走らない is likely verb+aux, not true adjective
// True adjectives: 少ない, 危ない (stem doesn't end in あ段)
// Verb patterns: 走らない, 書かない (stem ends in あ段 = verb mizenkei)
constexpr float kPenaltyVerbNaiPattern = 1.5F;

// Noun/Verb + て/で split when prev ends with Godan onbin or Ichidan ending
// E.g., 書い + て should be 書いて (te-form), not split
// E.g., 教え + て should be 教えて (te-form), not split
constexpr float kPenaltyTeFormSplit = 1.5F;

// VERB + て split when verb ends with たく (desire adverbial form)
// E.g., 食べたく + て should be 食べたくて (single token)
// This prevents splitting たくて into たく + て
constexpr float kPenaltyTakuTeSplit = 2.0F;

// VERB renyokei + たくて (ADJ) split
// E.g., 飲み + たくて should be 飲みたくて (single token)
// This prevents splitting at the boundary before たくて
constexpr float kPenaltyTakuteAfterRenyokei = 1.5F;

// AUX + たい adjective pattern
// E.g., なり(AUX, だ) + たかった is unnatural
// Should be なり(VERB, なる) + たかった
constexpr float kPenaltyTaiAfterAux = 1.0F;

// AUX(ません形) + で(PARTICLE) split
// E.g., ございません + で + した should be ございません + でした
// The で after negative polite forms is part of でした (copula past), not a particle
constexpr float kPenaltyMasenDeSplit = 2.0F;

// に (PARTICLE) + よる (NOUN, lemma 夜) split
// When followed by と, should prefer compound particle によると
// E.g., 報告によると should use compound particle, not に + 夜 + と
constexpr float kPenaltyYoruNightAfterNi = 1.5F;

// Conditional verb (ending with ば) + result verb
// E.g., あれば + 手伝います - very common grammatical pattern
// Offsets the high VERB→VERB base cost (0.8) for conditional clauses
constexpr float kBonusConditionalVerbToVerb = 0.7F;

// Verb renyokei + compound auxiliary verb
// E.g., 読み + 終わる, 書き + 始める, 走り + 続ける
// Offsets the VERB→VERB base cost (0.8) for compound verb patterns
// Must be >= 0.8 to make VERB→VERB cheaper than NOUN→NOUN (0.0)
constexpr float kBonusVerbRenyokeiCompoundAux = 1.0F;

// Verb renyokei + と (PARTICLE) pattern
// E.g., 食べ + と is likely part of 食べといた/食べとく contraction
// This split should be penalized to prefer the single token interpretation
constexpr float kPenaltyTokuContractionSplit = 1.5F;

// NOUN + いる/います/いません (AUX) penalty
// いる auxiliary should only follow te-form verbs (食べている), not nouns
// E.g., 手伝 + います should be 手伝います (single verb), not noun + aux
constexpr float kPenaltyIruAuxAfterNoun = 2.0F;

// Te-form VERB + いる/います/いません (AUX) bonus
// E.g., 食べて + いる, 走って + います - progressive aspect pattern
constexpr float kBonusIruAuxAfterTeForm = 0.5F;

// Te-form VERB + VERB bonus
// E.g., 関して + 報告する, 調べて + わかる - te-form continuation pattern
// Offsets the high VERB→VERB base cost (0.8) when prev verb ends with て/で
constexpr float kBonusTeFormVerbToVerb = 0.8F;

// Suffix at sentence start penalty
// Suffix should only follow nouns/pronouns, not appear at sentence start
constexpr float kPenaltySuffixAtStart = 3.0F;

// Suffix after punctuation/symbol penalty
// After 、。etc., a word is unlikely to be a suffix (e.g., 、家 should be NOUN)
constexpr float kPenaltySuffixAfterSymbol = 1.0F;

// Prefix before verb/auxiliary penalty
// Prefixes should attach to nouns/suffixes, not verbs (e.g., 何してる - 何 is PRON)
constexpr float kPenaltyPrefixBeforeVerb = 2.0F;

// Noun before verb-specific auxiliary penalty
// Verb auxiliaries (ます/ましょう/たい/ない) require verb stem, not nouns
// E.g., 行き(NOUN) + ましょう is invalid - should be 行き(VERB) + ましょう
constexpr float kPenaltyNounBeforeVerbAux = 2.0F;

// =============================================================================
// Auxiliary Connection Rules (extracted from inline literals)
// =============================================================================

// Invalid single-char aux (る) after te-form
// E.g., して + る should be してる (contraction), not split
constexpr float kPenaltyInvalidSingleCharAux = 5.0F;

// Te-form + た (likely contracted -ていた form)
// E.g., 見て + た should be 見てた (見ていた contraction)
constexpr float kPenaltyTeFormTaContraction = 1.5F;

// NOUN + まい (negative conjecture) penalty
// まい attaches to verb stems, not nouns
constexpr float kPenaltyNounMai = 1.5F;

// Short/unknown aux after particle
// PARTICLE + short AUX is grammatically invalid
constexpr float kPenaltyShortAuxAfterParticle = 3.0F;

// NOUN + みたい (resemblance pattern) bonus
// E.g., 猫みたい (like a cat) - very common pattern
constexpr float kBonusNounMitai = 3.0F;

// VERB + みたい (hearsay/appearance) bonus
// E.g., 食べるみたい (seems like eating)
constexpr float kBonusVerbMitai = 1.0F;

// =============================================================================
// Other Connection Rules (extracted from inline literals)
// =============================================================================

// Formal noun + kanji penalty
// E.g., 所 + 在する should be 所在する (compound)
constexpr float kPenaltyFormalNounBeforeKanji = 3.0F;

// Same particle repeated penalty
// E.g., も + も is grammatically rare
constexpr float kPenaltySameParticleRepeated = 2.0F;

// Hiragana noun starts with particle char penalty
// E.g., もも after NOUN should prefer も(PARTICLE) + もも
constexpr float kPenaltyHiraganaNounStartsWithParticle = 1.5F;

// Particle before hiragana OTHER penalty (single char)
// E.g., と + う in とうきょう split
constexpr float kPenaltyParticleBeforeSingleHiraganaOther = 2.5F;

// Particle before hiragana OTHER penalty (multi char)
constexpr float kPenaltyParticleBeforeMultiHiraganaOther = 1.0F;

// し particle after i-adjective (valid pattern)
// E.g., 上手いし, 高いし
constexpr float kBonusShiAfterIAdj = 0.5F;

// し particle after verb (valid pattern)
// E.g., 食べるし, 行くし
constexpr float kBonusShiAfterVerb = 0.3F;

// し particle after auxiliary (valid pattern)
// E.g., だし, ないし, たし
constexpr float kBonusShiAfterAux = 0.3F;

// し particle after noun (invalid, needs copula)
// E.g., 本し should be 本だし
constexpr float kPenaltyShiAfterNoun = 1.5F;

// らしい (conjecture) after verb/adjective (valid pattern)
// E.g., 帰るらしい, 美しいらしい
// Offset VERB/ADJ→ADJ base cost (0.8) to encourage proper split
constexpr float kBonusRashiiAfterPredicate = 0.8F;

// Verb ending with たいらしい should be split (帰りたいらしい → 帰りたい + らしい)
constexpr float kPenaltyVerbTaiRashii = 0.5F;

}  // namespace suzume::analysis::scorer

#endif  // SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
