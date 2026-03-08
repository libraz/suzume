#ifndef SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
#define SUZUME_ANALYSIS_SCORER_CONSTANTS_H_

#include <cstddef>

#include "analysis/bigram_table.h"

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
// Penalty/Bonus magnitudes (see bigram_cost namespace in bigram_table.h):
//   kNegligible (0.2F) - Almost no impact
//   kMinor (0.5F)      - Small adjustment, tips the scale
//   kRare (1.0F)       - Rare occurrence
//   kStrong (1.5F)     - Strong preference/discouragement
//   kSevere (2.5F)     - Severe violation
//   kNever (3.5F)      - Near-prohibition of pattern
//
// Base connection costs (from scorer.cpp):
//   NOUN→NOUN: 0.0, VERB→VERB: 0.8, NOUN→VERB: 0.2, etc.
// =============================================================================

namespace suzume::analysis::scorer {

// Use bigram_cost constants via this alias
namespace scale = bigram_cost;

// =============================================================================
// Edge Costs (Unigram penalties for invalid patterns)
// =============================================================================

// Note: kPenaltyVerbSou and kPenaltyVerbSouDesu were removed
// to unify verb+そう as single token (走りそう → 走る, like 食べそう → 食べる)
// See: backup/technical_debt_action_plan.md section 3.3

// Unknown adjective ending with そう but invalid lemma
// Valid: おいしそう (lemma おいしい), Invalid: 食べそう (lemma 食べい)
constexpr float kPenaltyInvalidAdjSou = scale::kStrong;

// Bonus for unknown i-adjective in くない form (negative conjugation)
// E.g., しんどくない, エモくない - these are strong indicators of i-adjective
// くない pattern is highly reliable for adjective detection
// Needs to compete with fragmented paths containing dictionary verbs
constexpr float kBonusIAdjKunai = scale::kExtraStrongBonus;  // -1.0

// Unknown adjective with たい pattern where stem is invalid
// E.g., りたかった is invalid (り is not a valid verb stem)
constexpr float kPenaltyInvalidTaiPattern = scale::kStrong + scale::kMinor;  // 2.0

// Unknown adjective containing verb+auxiliary patterns
// E.g., 食べすぎてしまい should be verb+しまう, not adjective
constexpr float kPenaltyVerbAuxInAdj = scale::kStrong + scale::kMinor;  // 2.0

// しまい/じまい parsed as adjective (should be しまう renyokei)
constexpr float kPenaltyShimaiAsAdj = scale::kSevere + scale::kMinor;  // 3.0

// Adjective lemma containing verb onbin + contraction patterns (んどい, んとい)
// E.g., 読んどい from 読んどく - invalid adjective, should be verb とく contraction
constexpr float kPenaltyVerbOnbinAsAdj = scale::kStrong + scale::kMinor;  // 2.0

// Pure hiragana unknown adjective penalty (after PREFIX or single PARTICLE)
// Valid hiragana adjectives (すごい, うまい, おこがましい) are in dictionary
// Unknown pure hiragana adjectives after PREFIX/PARTICLE are likely misanalysis
// E.g., お+こがましい should be おこがましい, は+なはだしい should be はなはだしい
constexpr float kPenaltyHiraganaAdj = scale::kSevere + scale::kMinor;  // 3.0

// Bonus for unified verb forms containing auxiliary patterns
// E.g., 食べてしまった (unified) beats 食べて + しまった (split)
// When te-form has a dictionary entry, unified forms need this bonus to compete
constexpr float kBonusUnifiedVerbAux = 0.3F;

// =============================================================================
// Connection Costs (Bigram penalties/bonuses)
// =============================================================================

// Copula (だ/です) after verb without そう pattern
// This is grammatically invalid in most cases
constexpr float kPenaltyCopulaAfterVerb = scale::kSevere + scale::kMinor;  // 3.0

// Ichidan renyokei + て/てV split (should be te-form)
// E.g., 教え + て should be 教えて
constexpr float kPenaltyIchidanRenyokeiTe = scale::kStrong;

// たい adjective/auxiliary after verb renyokei - this is valid
// E.g., 食べたい, 読みたい (positive value subtracted as bonus)
// Must be stronger than kBonusTaAfterRenyokei (2.5) to prevent た+い split
// when たい auxiliary is the correct parse
constexpr float kBonusTaiAfterRenyokei = scale::kSevere + 0.1F;  // 2.6

// 安い (やすい) interpreted as cheap after renyokei-like noun
// Should be verb + やすい (easy to do)
constexpr float kPenaltyYasuiAfterRenyokei = scale::kStrong + scale::kMinor;  // 2.0

// VERB + ながら split when verb is in renyokei
// Should be single token like 飲みながら, 歩きながら
// Strong penalty needed because dictionary renyokei entries (e.g., 歩き) have low cost
constexpr float kPenaltyNagaraSplit = scale::kStrong;

// VERB renyokei + 方 when verb should be nominalized
// 解き方, 読み方, 書き方 - the verb renyokei is used as a nominalized noun
// Strong penalty to force nominalized noun candidate
constexpr float kPenaltyKataAfterRenyokei = scale::kStrong;

// NOUN + そう when noun looks like verb renyokei
// Should be verb renyokei + そう auxiliary
constexpr float kPenaltySouAfterRenyokei = scale::kMinor;

// AUX だ/です + character speech suffix split
// E.g., だ + にゃ should be だにゃ (single token)
constexpr float kPenaltyCharacterSpeechSplit = scale::kRare;

// ADJ(連用形・く) + VERB(なる) pattern
// E.g., 美しく + なる - very common grammatical pattern (positive value subtracted)
constexpr float kBonusAdjKuNaru = scale::kMinor;

// Compound verb auxiliary after renyokei-like noun
// E.g., 読み + 終わる should be verb renyokei + auxiliary
constexpr float kPenaltyCompoundAuxAfterRenyokei = scale::kMinor;

// Unknown adjective with lemma ending in ない where stem looks like verb mizenkei
// E.g., 走らなければ with lemma 走らない is likely verb+aux, not true adjective
// True adjectives: 少ない, 危ない (stem doesn't end in あ段)
// Verb patterns: 走らない, 書かない (stem ends in あ段 = verb mizenkei)
constexpr float kPenaltyVerbNaiPattern = scale::kStrong;

// Noun/Verb + て/で split when prev ends with Godan onbin or Ichidan ending
// E.g., 書い + て should be 書いて (te-form), not split
// E.g., 教え + て should be 教えて (te-form), not split
constexpr float kPenaltyTeFormSplit = scale::kStrong;

// VERB + て split when verb ends with たく (desire adverbial form)
// E.g., 食べたく + て should be 食べたくて (single token)
// This prevents splitting たくて into たく + て
constexpr float kPenaltyTakuTeSplit = scale::kStrong + scale::kMinor;  // 2.0

// VERB renyokei + たくて (ADJ) split
// E.g., 飲み + たくて should be 飲みたくて (single token)
// This prevents splitting at the boundary before たくて
constexpr float kPenaltyTakuteAfterRenyokei = scale::kStrong;

// AUX + たい adjective pattern
// E.g., なり(AUX, だ) + たかった is unnatural
// Should be なり(VERB, なる) + たかった
constexpr float kPenaltyTaiAfterAux = scale::kRare;

// AUX(ません形) + で(PARTICLE) split
// E.g., ございません + で + した should be ございません + でした
// The で after negative polite forms is part of でした (copula past), not a particle
constexpr float kPenaltyMasenDeSplit = scale::kStrong + scale::kMinor;  // 2.0

// に (PARTICLE) + よる (NOUN, lemma 夜) split
// When followed by と, should prefer compound particle によると
// E.g., 報告によると should use compound particle, not に + 夜 + と
constexpr float kPenaltyYoruNightAfterNi = scale::kStrong;

// Conditional verb (ending with ば) + result verb
// E.g., あれば + 手伝います - very common grammatical pattern
// Offsets the high VERB→VERB base cost (0.8) for conditional clauses
constexpr float kBonusConditionalVerbToVerb = 0.7F;

// Verb renyokei + compound auxiliary verb
// E.g., 読み + 終わる, 書き + 始める, 走り + 続ける
// Offsets the VERB→VERB base cost (0.8) for compound verb patterns
// Must be >= 0.8 to make VERB→VERB cheaper than NOUN→NOUN (0.0)
constexpr float kBonusVerbRenyokeiCompoundAux = scale::kRare;

// Verb renyokei + と (PARTICLE) pattern
// E.g., 食べ + と is likely part of 食べといた/食べとく contraction
// This split should be penalized to prefer the single token interpretation
constexpr float kPenaltyTokuContractionSplit = scale::kStrong;

// てく/ってく + れ* mis-segmentation penalty
// When てく (colloquial ていく) is followed by れ-starting AUX, it's almost
// always a mis-segmentation of てくれる pattern.
// E.g., つけてくれない → つけ + てく + れない is wrong; should be つけて + くれない
constexpr float kPenaltyTekuReMissegmentation = scale::kNever;

// NOUN + いる/います/いません (AUX) penalty
// いる auxiliary should only follow te-form verbs (食べている), not nouns
// E.g., 手伝 + います should be 手伝います (single verb), not noun + aux
constexpr float kPenaltyIruAuxAfterNoun = scale::kStrong + scale::kMinor;  // 2.0

// Te-form VERB + いる/います/いません (AUX) bonus
// E.g., 食べて + いる, 走って + います - progressive aspect pattern
constexpr float kBonusIruAuxAfterTeForm = scale::kMinor;

// Te-form VERB + しまう/しまった (AUX) bonus
// E.g., 食べて + しまった, 忘れて + しまう - completive/regretful aspect pattern
constexpr float kBonusShimauAuxAfterTeForm = scale::kRare;  // 1.5F

// Verb renyokei + そう (AUX) bonus
// E.g., 降り + そう, 切れ + そう - appearance auxiliary pattern
// Helps AUX beat ADV when preceded by verb renyokei form
// Value compensates for higher AUX cost (1.0F) to ensure VERB→AUX wins over VERB→ADV
constexpr float kBonusSouAuxAfterRenyokei = 1.3F;

// Te-form VERB + VERB bonus
// E.g., 関して + 報告する, 調べて + わかる - te-form continuation pattern
// Offsets the high VERB→VERB base cost (0.8) when prev verb ends with て/で
constexpr float kBonusTeFormVerbToVerb = 0.8F;

// Suffix at sentence start penalty
// Suffix should only follow nouns/pronouns, not appear at sentence start
constexpr float kPenaltySuffixAtStart = scale::kSevere + scale::kMinor;  // 3.0

// Suffix after punctuation/symbol penalty
// After 、。etc., a word is unlikely to be a suffix (e.g., 、家 should be NOUN)
constexpr float kPenaltySuffixAfterSymbol = scale::kRare;

// Prefix before verb/auxiliary penalty
// Prefixes should attach to nouns/suffixes, not verbs (e.g., 何してる - 何 is PRON)
constexpr float kPenaltyPrefixBeforeVerb = scale::kStrong + scale::kMinor;  // 2.0

// Noun before verb-specific auxiliary penalty
// Verb auxiliaries (ます/ましょう/たい/ない) require verb stem, not nouns
// E.g., 行き(NOUN) + ましょう is invalid - should be 行き(VERB) + ましょう
constexpr float kPenaltyNounBeforeVerbAux = scale::kStrong + scale::kMinor;  // 2.0

// =============================================================================
// Verb Connection Rules (Phase 5 normalization)
// =============================================================================

// しれる verb + ます/ない pattern bonus
// E.g., かもしれません → かも + しれ + ませ + ん
// Strong bonus to compensate for し+れ path getting passive-aux bonuses
constexpr float kBonusShireruToMasuNai = scale::kNever;  // 3.5F

// Verb renyokei + contracted verb (てる/とく/etc.) bonus
// E.g., し + てる should beat し(PARTICLE) + てる
constexpr float kBonusRenyokeiToContractedVerb = scale::kSevere;  // 2.5

// Verb renyokei + standalone て/で (renyokei of てる/でる) bonus
// E.g., 食べ + て(VERB, lemma=てる) for contracted progressive 食べてた
// Moderate bonus to keep path competitive while allowing て(PARTICLE) to win
// for full progressive forms like 食べている
constexpr float kBonusStandaloneTeDeRenyokei = 0.6F;

// Verb renyokei/onbinkei + て/で particle (MeCab-compatible te-form split)
// E.g., 食べ + て, 書い + て - strong bonus to prefer this split
constexpr float kBonusRenyokeiToTeParticle = scale::kSevere;  // 2.5

// て/で particle + auxiliary verb bonus
// E.g., て + いる, て + しまう - supports MeCab-compatible patterns
constexpr float kBonusTeParticleToAuxVerb = scale::kStrong + scale::kMinor;  // 2.0

// てる renyokei + た (contracted progressive past)
// E.g., て(VERB, lemma=てる) + た → 見てた (MeCab-compatible)
constexpr float kBonusTeruRenyokeiToTa = scale::kStrong;  // 1.5

// Verb onbinkei + だ (voiced past tense)
// E.g., 泳い(onbin) + だ → 泳いだ (encourages verb + voiced た interpretation)
// Without this, the split path (泳い NOUN + だ AUX) wins due to dictionary bonus on だ
constexpr float kBonusOnbinkeiToVoicedTa = scale::kMinor;  // 0.5

// Verb onbinkei + たら/だら (conditional past)
// E.g., 書い(onbin) + たら → 書いたら (encourages verb + conditional interpretation)
// Without this, 書いたら(VERB dict) wins over 書い + たら split
constexpr float kBonusOnbinkeiToTara = scale::kMinor;  // 0.5

// Verb onbinkei + た (past tense)
// E.g., 書い(onbin) + た → 書いた (encourages verb + past aux interpretation)
// Without this, 書いた(VERB unk) wins over 書い + た split
// Note: Larger bonus needed because onbin VERB candidates have higher base cost
constexpr float kBonusOnbinkeiToTa = scale::kRare;  // 1.5

// Adjective く form + て particle bonus
// E.g., 美しく + て - very common pattern
constexpr float kBonusAdjKuToTeParticle = scale::kSevere;  // 2.5

// Adjective く form + ない (AUX) bonus
// E.g., 高く + ない - MeCab-compatible split for adjective negation
// Strong bonus needed to beat unknown adjective single-token candidates (美しくない etc.)
constexpr float kBonusAdjKuToNai = scale::kSevere;  // 2.5

// I-adjective (basic form) + です (polite copula) bonus
// E.g., 美味しい + です, いい + です - very common polite pattern
// Strong bonus needed to beat で(AUX)+す(VERB) split path
constexpr float kBonusIAdjToDesu = scale::kSevere;  // 2.5

// ADJ(かっ形) → た(AUX) bonus for MeCab-compatible split (BUG-036)
// E.g., よかったです → よかっ + た + です
// Small bonus to make the path viable (main bonus is at た→です)
constexpr float kBonusIAdjKattToTa = 0.3F;

// た(AUX) → です(AUX) bonus for MeCab-compatible split (BUG-036)
// Helps split paths like よかっ+た+です beat full form paths like よかった+です
// Also helps verb past + です patterns (食べたです)
constexpr float kBonusTaAuxToDesu = scale::kSevere;  // 2.5

// に(PARTICLE) + いる/いた(VERB/AUX) bonus
// E.g., 家にいた → 家 + に + いた (not にいた as verb)
// Session 31 allowed に as verb stem start for verbs like にげる, にる
// This bonus helps the particle + verb path beat the incorrect にぐ analysis
// Strong bonus needed because にいた (verb) has very low cost while いた has high cost
constexpr float kBonusNiParticleToIruVerb = scale::kNever;  // 3.5

// 終助詞連続 (sentence-final particle sequence) bonus
// E.g., よ + ね, よ + わ are extremely common final particle combinations
// Strong bonus to prefer ね(PARTICLE) over ね(VERB, lemma=ねる)
constexpr float kBonusSentenceFinalParticleSeq = scale::kStrong;  // 2.5

// そう auxiliary small bonus after renyokei (fine-tuning)
// Balanced to allow ADJ一体 (難しそう→難しい) while boosting VERB+そう slightly
// Note: Perfect MeCab compat (VERB/ADJ語幹+そう split) needs ADJ stem generation
constexpr float kBonusSouAfterRenyokeiSmall = 0.25F;

// ADJ stem + そう (appearance auxiliary) bonus
// E.g., 美味し + そう - MeCab-compatible adjective stem split
// Very large bonus needed because dictionary forms (美味しそう) have low cost
constexpr float kBonusAdjStemToSouAux = 4.0F;

// Quotative + いう (to say) bonus
// E.g., そう + いっ, と + いっ - prefer いう over いく after quotative contexts
constexpr float kBonusQuotativeToIu = scale::kSevere + scale::kMinor;  // 3.0

// に(PARTICLE) + いく(to go) onbinkei bonus
// E.g., 旅行にいった → に + いっ (prefer いく over いう after に)
constexpr float kBonusNiParticleToIku = scale::kStrong + scale::kMinor;  // 2.0

// =============================================================================
// Auxiliary Connection Rules (extracted from inline literals)
// =============================================================================

// Invalid single-char aux (る) after te-form
// E.g., して + る should be してる (contraction), not split
// P7-1: Normalized to scale::kNever (was 5.0F)
constexpr float kPenaltyInvalidSingleCharAux = scale::kNever;

// Te-form + た (likely contracted -ていた form)
// E.g., 見て + た should be 見てた (見ていた contraction)
constexpr float kPenaltyTeFormTaContraction = scale::kStrong;

// NOUN + まい (negative conjecture) penalty
// まい attaches to verb stems, not nouns
constexpr float kPenaltyNounMai = scale::kStrong;

// Short/unknown aux after particle
// PARTICLE + short AUX is grammatically invalid
constexpr float kPenaltyShortAuxAfterParticle = scale::kSevere + scale::kMinor;  // 3.0

// NOUN + みたい (resemblance pattern) bonus
// E.g., 猫みたい (like a cat) - very common pattern
// P7-2: Large bonus (3.0F) intentional - required to override unknown verb analysis
// Without this, みたい tends to be parsed as verb rather than auxiliary
constexpr float kBonusNounMitai = scale::kSevere + scale::kMinor;  // 3.0

// VERB + みたい (hearsay/appearance) bonus
// E.g., 食べるみたい (seems like eating)
constexpr float kBonusVerbMitai = scale::kRare;

// で(PARTICLE/AUX) + くる活用形 penalty (できる misparse prevention)
// E.g., できます → で + きます is wrong; should be でき + ます
// Applied to both PARTICLE→AUX and AUX(だ)→AUX patterns
constexpr float kPenaltyDeToKuruAux = scale::kNever;  // 3.5

// ADJ + で(AUX, lemma=だ) bonus for na-adjective copula pattern
// E.g., 嫌でない → 嫌 + で(AUX) + ない (copula negation)
// E.g., 静かで美しい → 静か(ADJ) + で(AUX) + 美しい
// Note: Only applies to ADJ, not NOUN. Regular nouns use particle で.
// E.g., 秒速で → 秒速(NOUN) + で(PARTICLE)
constexpr float kBonusAdjToCopulaDe = -scale::kSevere;  // -2.5

// NOUN/ADJ + でない(VERB, lemma=できる) penalty
// Prevents na-adj copula from being misparsed as できる negation
// E.g., 嫌でない should be copula pattern, not できる negative form
constexpr float kPenaltyNaAdjToDekinaiVerb = scale::kNever;  // 3.5

// で(AUX, lemma=だ) + ない(AUX) bonus for na-adjective copula negation
// E.g., 嫌でない → 嫌 + で + ない, でない → で + ない (copula negation pattern)
// Strong bonus (kProhibitive) to beat で(VERB,でる)+ない path
constexpr float kBonusCopulaDeToNai = scale::kNever;  // 3.5 (used as negative bonus: -3.5)

// で(AUX, lemma=だ) + ござる(AUX) bonus for classical copula pattern
// E.g., 侍でござる, これでござる - need to beat で(PARTICLE) + ござる(VERB) path
// で(PARTICLE) has cost -0.8, で(AUX) has cost 0.2 → diff 1.0
// Need ~1.6+ bonus to overcome total difference
constexpr float kBonusCopulaDeToGozaru = 1.8F;

// で(AUX, lemma=だ) + は/も(PARTICLE) bonus for copula negation patterns
// E.g., ではない, でもない - prefer copula interpretation over particle
// で(PARTICLE) has cost 0.1, で(AUX) has cost 1.0 → diff 0.9
// Need bonus > 0.9 to overcome word cost difference
constexpr float kBonusCopulaDeToHaMo = scale::kStrong;  // 1.5

// =============================================================================
// Other Connection Rules (extracted from inline literals)
// =============================================================================

// Formal noun + kanji penalty
// E.g., 所 + 在する should be 所在する (compound)
constexpr float kPenaltyFormalNounBeforeKanji = scale::kSevere + scale::kMinor;  // 3.0

// Same particle repeated penalty
// E.g., も + も is grammatically rare
constexpr float kPenaltySameParticleRepeated = scale::kStrong + scale::kMinor;  // 2.0

// Suspicious particle sequence penalty (different particles in unlikely sequence)
// E.g., は + し + が suggests a noun like はし was split incorrectly
// し as listing particle should follow predicates, not particles
constexpr float kPenaltySuspiciousParticleSequence = scale::kStrong;  // 2.5

// Hiragana noun starts with particle char penalty
// E.g., もも after NOUN should prefer も(PARTICLE) + もも
constexpr float kPenaltyHiraganaNounStartsWithParticle = scale::kStrong;

// Particle before hiragana OTHER penalty (single char)
// E.g., と + う in とうきょう split
constexpr float kPenaltyParticleBeforeSingleHiraganaOther = scale::kSevere;

// Particle before hiragana OTHER penalty (multi char)
constexpr float kPenaltyParticleBeforeMultiHiraganaOther = scale::kRare;

// Particle before hiragana VERB penalty
// E.g., し + まる in しまる split - likely an erroneous split of a hiragana verb
// This prevents splits like し(PARTICLE) + まる(VERB) when しまる should be single VERB
// Also handles は + ちみつ in はちみつ - particle bonus (-0.4) requires strong penalty
constexpr float kPenaltyParticleBeforeHiraganaVerb = scale::kNever;

// し particle after i-adjective (valid pattern)
// E.g., 上手いし, 高いし (positive value subtracted)
constexpr float kBonusShiAfterIAdj = scale::kMinor;

// し particle after verb (valid pattern)
// E.g., 食べるし, 行くし (positive value subtracted)
constexpr float kBonusShiAfterVerb = 0.3F;

// し particle after auxiliary (valid pattern)
// E.g., だし, ないし, たし (positive value subtracted)
constexpr float kBonusShiAfterAux = 0.3F;

// し particle after noun (invalid, needs copula)
// E.g., 本し should be 本だし
constexpr float kPenaltyShiAfterNoun = scale::kStrong;

// な particle after kanji noun (likely na-adjective pattern)
// E.g., 獰猛な should prefer ADJ interpretation over NOUN + PARTICLE
// When a 2+ kanji noun is followed by な(PARTICLE), it's almost always
// a na-adjective stem. Penalty shifts preference away from PARTICLE.
// Using severe penalty because PARTICLE → NOUN connection cost is 0,
// so we need sufficient penalty to prefer other paths.
constexpr float kPenaltyNaParticleAfterKanjiNoun = scale::kSevere;  // 2.5

// NOUN(し ending) + VERB(て starting) penalty
// Penalizes patterns like 説明し(NOUN) + てくれます(VERB)
// This suggests suru-verb te-form that should be single VERB
constexpr float kPenaltySuruRenyokeiToTeVerb = scale::kRare;  // 1.5

// らしい (conjecture) after verb/adjective (valid pattern)
// E.g., 帰るらしい, 美しいらしい
// Offset VERB/ADJ→ADJ base cost (0.8) to encourage proper split
constexpr float kBonusRashiiAfterPredicate = 0.8F;

// Verb ending with たいらしい should be split (帰りたいらしい → 帰りたい + らしい)
constexpr float kPenaltyVerbTaiRashii = scale::kMinor;

// Verb ending with さん (contracted negative) where stem looks nominal
// E.g., 田中さん should be NOUN + SUFFIX, not VERB (田中する + contracted negative)
// Applied when: surface ends with さん AND (lemma ends with する OR stem ends with kanji)
constexpr float kPenaltyVerbSanHonorific = scale::kSevere;  // 2.5

// Verb ending with ん (contracted negative) with very short stem
// E.g., いん (from いる) is rare and often misanalysis in patterns like ないんだ
// Applied when: surface ends with ん AND surface is 2 chars AND pure hiragana
constexpr float kPenaltyVerbContractedNegShortStem = scale::kStrong + scale::kMinor;  // 2.0

// Verb (renyokei/base) + case particle (を/が/に/で/から/まで/へ)
// Penalizes patterns like 打ち合わせ(VERB)+を which should be NOUN+を
// Verbal nouns used as objects should be NOUN, not VERB
constexpr float kPenaltyVerbToCaseParticle = scale::kStrong;  // 1.5

// =============================================================================
// Surface-Based Cost Adjustments (scorer.cpp)
// =============================================================================
// These constants are used for word-cost adjustments based on surface properties.

// Bonus for compound particles from dictionary (について, によって, として, etc.)
// Multi-char particles that should not be split into verb+て patterns
constexpr float kBonusCompoundParticle = -1.0F;

// Bonus for みたい (conjecture auxiliary) from dictionary
constexpr float kBonusMitaiDict = -1.0F;

// Bonus for hiragana+kanji mixed nouns from dictionary (なし崩し, みじん切り, お茶)
constexpr float kBonusMixedNoun = -0.5F;

// Bonus for long all-kanji nouns from dictionary (4+ chars)
// Without this, split path wins due to dict+dict connection bonus (-0.5) and
// split_candidates both-in-dict bonus (-0.2), totaling -0.7 advantage
constexpr float kBonusLongKanjiNounBase = -1.0F;
constexpr float kBonusLongKanjiNounPerChar = -0.3F;

// Bonus for multi-char hiragana suffixes from dictionary (まみれ, だらけ, ごと)
constexpr float kBonusLongSuffix = -1.5F;

// Bonus for short hiragana verbs from dictionary (なる, ある, いる, する)
constexpr float kBonusShortHiraganaVerb = -0.3F;

// Penalty for spurious kanji+hiragana verb renyokei not in dictionary
// E.g., 学生み (学生みる doesn't exist) - false positive
constexpr float kPenaltySpuriousVerbRenyokei = scale::kStrong;

// Penalty for short/long pure-hiragana hatsuonbin verb forms
constexpr float kPenaltyHatsuonbinShort = scale::kRare;    // 2-4 chars
constexpr float kPenaltyHatsuonbinLong = scale::kSevere;   // 5+ chars

// Penalty for pure-hiragana verb forms containing さん pattern
constexpr float kPenaltySanPatternVerb = scale::kSevere;

// Penalty for pure-hiragana ichidan verb renyokei starting with に
constexpr float kPenaltyNiPrefixVerb = scale::kStrong + scale::kMinor;  // 2.0

// Penalty for very long pure-hiragana verb candidates not in dictionary
constexpr float kPenaltyVeryLongHiraganaVerb = scale::kNever;

// Penalty for kanji+hiragana verb renyokei ending in いし pattern
constexpr float kPenaltyIshiVerbRenyokei = scale::kSevere;

// Penalty for kanji 中 compound patterns (過剰分割防止)
constexpr float kPenaltyKanjiChuuCompound = scale::kMinor;

// =============================================================================
// Pattern String Constants
// =============================================================================

// Suffix pattern for auxiliary detection
constexpr const char* kSuffixSou = "そう";       // conjecture/hearsay

// =============================================================================
// Pattern Arrays for Auxiliary Verb Detection
// =============================================================================

// Te-form + auxiliary patterns for verb candidate penalty (16 patterns)
// Used in: verb_candidates_helpers.cpp (containsTeFormAuxPattern)
// Excludes てある/である/ておる/でおる/ていく/でいく/であげ (rare in compound verbs)
constexpr const char* kTeFormAuxPenaltyPatterns[] = {
    "てくる", "でくる", "てくれ", "でくれ", "ている", "でいる",
    "てしま", "でしま", "てもら", "でもら", "てあげ",
    "ておく", "でおく", "ておい", "てみる", "でみる",
};
constexpr size_t kTeFormAuxPenaltyPatternsSize =
    sizeof(kTeFormAuxPenaltyPatterns) / sizeof(kTeFormAuxPenaltyPatterns[0]);

// Causative auxiliary patterns for verb candidate penalty
// Used in: verb_candidates_helpers.cpp (containsCausativeAuxPattern)
// Pattern: verb mizenkei + せ/させ + auxiliary (ない/て/た/ず/る/ろ/よ/なく)
constexpr const char* kCausativeAuxPenaltyPatterns[] = {
    "せない", "せなく", "せなかっ", "せて", "せた", "せず", "せる", "せろ", "せよ",
    "させない", "させなく", "させて", "させた", "させず", "させる", "させろ", "させよ",
};
constexpr size_t kCausativeAuxPenaltyPatternsSize =
    sizeof(kCausativeAuxPenaltyPatterns) / sizeof(kCausativeAuxPenaltyPatterns[0]);

// I-adjective conjugation suffixes (standalone, not verb candidates)
// These patterns are conjugation endings for i-adjectives:
// - か行: past (高かった), conditional past (高かったら)
// - く行: te-form (高くて), negative (高くない)
// - け行: conditional (高ければ)
// When appearing standalone without a stem, these should NOT be verb candidates.
constexpr const char* kIAdjPastKatta = "かった";      // i-adj past: 高い→高かった
constexpr const char* kIAdjPastKattara = "かったら";  // i-adj conditional past
constexpr const char* kIAdjStemKa = "かっ";           // i-adj past stem
constexpr const char* kIAdjTeKute = "くて";           // i-adj te-form: 高い→高くて
constexpr const char* kIAdjNegKunai = "くない";       // i-adj negative: 高い→高くない
constexpr const char* kIAdjNegStemKuna = "くな";      // i-adj negative stem
constexpr const char* kIAdjCondKereba = "ければ";     // i-adj conditional: 高い→高ければ
constexpr const char* kIAdjCondStemKere = "けれ";     // i-adj conditional stem

}  // namespace suzume::analysis::scorer

#endif  // SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
