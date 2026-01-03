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
// Penalty/Bonus magnitudes (see scale:: namespace for formal constants):
//   scale::kTrivial (0.2F)     - Almost no impact
//   scale::kMinor (0.5F)       - Small adjustment, tips the scale
//   scale::kModerate (1.0F)    - Standard penalty/bonus
//   scale::kStrong (1.5F)      - Strong preference/discouragement
//   scale::kSevere (2.5F)      - Severe violation
//   scale::kProhibitive (3.5F) - Near-prohibition of pattern
//
// Base connection costs (from scorer.cpp):
//   NOUN→NOUN: 0.0, VERB→VERB: 0.8, NOUN→VERB: 0.2, etc.
// =============================================================================

namespace suzume::analysis::scorer {

// =============================================================================
// Score Scale Constants
// =============================================================================
// Formal scale definitions for consistent penalty/bonus magnitude.
// All penalty/bonus constants should reference these scale values.
namespace scale {

// Penalty scale (positive values - higher discourages pattern)
constexpr float kTrivial = 0.2F;      // Almost no impact
constexpr float kMinor = 0.5F;        // Slight unnaturalness
constexpr float kModerate = 1.0F;     // Moderate penalty
constexpr float kStrong = 1.5F;       // Strong grammatical violation
constexpr float kSevere = 2.5F;       // Severe violation
constexpr float kProhibitive = 3.5F;  // Near prohibition

// Bonus scale (negative values - lower encourages pattern)
constexpr float kSlightBonus = -0.2F;
constexpr float kModerateBonus = -0.5F;
constexpr float kStrongBonus = -1.0F;
constexpr float kVeryStrongBonus = -1.5F;

}  // namespace scale

// =============================================================================
// Edge Costs (Unigram penalties for invalid patterns)
// =============================================================================

// Note: kPenaltyVerbSou and kPenaltyVerbSouDesu were removed
// to unify verb+そう as single token (走りそう → 走る, like 食べそう → 食べる)
// See: backup/technical_debt_action_plan.md section 3.3

// Unknown adjective ending with そう but invalid lemma
// Valid: おいしそう (lemma おいしい), Invalid: 食べそう (lemma 食べい)
constexpr float kPenaltyInvalidAdjSou = scale::kStrong;

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

// たい adjective after verb renyokei - this is valid
// E.g., 食べたい, 読みたい (positive value subtracted as bonus)
constexpr float kBonusTaiAfterRenyokei = 0.8F;

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
constexpr float kPenaltyCharacterSpeechSplit = scale::kModerate;

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
constexpr float kPenaltyTaiAfterAux = scale::kModerate;

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
constexpr float kBonusVerbRenyokeiCompoundAux = scale::kModerate;

// Verb renyokei + と (PARTICLE) pattern
// E.g., 食べ + と is likely part of 食べといた/食べとく contraction
// This split should be penalized to prefer the single token interpretation
constexpr float kPenaltyTokuContractionSplit = scale::kStrong;

// NOUN + いる/います/いません (AUX) penalty
// いる auxiliary should only follow te-form verbs (食べている), not nouns
// E.g., 手伝 + います should be 手伝います (single verb), not noun + aux
constexpr float kPenaltyIruAuxAfterNoun = scale::kStrong + scale::kMinor;  // 2.0

// Te-form VERB + いる/います/いません (AUX) bonus
// E.g., 食べて + いる, 走って + います - progressive aspect pattern
constexpr float kBonusIruAuxAfterTeForm = scale::kMinor;

// Te-form VERB + しまう/しまった (AUX) bonus
// E.g., 食べて + しまった, 忘れて + しまう - completive/regretful aspect pattern
constexpr float kBonusShimauAuxAfterTeForm = scale::kModerate;  // 1.5F

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
constexpr float kPenaltySuffixAfterSymbol = scale::kModerate;

// Prefix before verb/auxiliary penalty
// Prefixes should attach to nouns/suffixes, not verbs (e.g., 何してる - 何 is PRON)
constexpr float kPenaltyPrefixBeforeVerb = scale::kStrong + scale::kMinor;  // 2.0

// Noun before verb-specific auxiliary penalty
// Verb auxiliaries (ます/ましょう/たい/ない) require verb stem, not nouns
// E.g., 行き(NOUN) + ましょう is invalid - should be 行き(VERB) + ましょう
constexpr float kPenaltyNounBeforeVerbAux = scale::kStrong + scale::kMinor;  // 2.0

// =============================================================================
// Auxiliary Connection Rules (extracted from inline literals)
// =============================================================================

// Invalid single-char aux (る) after te-form
// E.g., して + る should be してる (contraction), not split
// P7-1: Normalized to scale::kProhibitive (was 5.0F)
constexpr float kPenaltyInvalidSingleCharAux = scale::kProhibitive;

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
constexpr float kBonusVerbMitai = scale::kModerate;

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
constexpr float kPenaltyParticleBeforeMultiHiraganaOther = scale::kModerate;

// Particle before hiragana VERB penalty
// E.g., し + まる in しまる split - likely an erroneous split of a hiragana verb
// This prevents splits like し(PARTICLE) + まる(VERB) when しまる should be single VERB
// Also handles は + ちみつ in はちみつ - particle bonus (-0.4) requires strong penalty
constexpr float kPenaltyParticleBeforeHiraganaVerb = scale::kProhibitive;

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
constexpr float kPenaltySuruRenyokeiToTeVerb = scale::kModerate;  // 1.5

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
// Pattern String Constants
// =============================================================================
// These string constants are used for pattern matching in scoring.
// Centralizing them improves maintainability and makes patterns discoverable.

// Suffix patterns for auxiliary detection
constexpr const char* kSuffixSou = "そう";       // conjecture/hearsay
constexpr const char* kSuffixTai = "たい";       // desire
constexpr const char* kSuffixNai = "ない";       // negation
constexpr const char* kSuffixTaiRashii = "たいらしい";  // desire + conjecture
constexpr const char* kSuffixSan = "さん";       // contracted negative (さ+ん) or honorific
constexpr const char* kSuffixN = "ん";           // contracted negative (〜ない→〜ん)
constexpr const char* kSuffixShi = "し";         // suru renyokei ending
constexpr const char* kLemmaSuru = "する";       // suru verb lemma suffix

// Verb conjugation form markers
constexpr const char* kFormTe = "て";            // te-form (unvoiced)
constexpr const char* kFormDe = "で";            // te-form (voiced)
constexpr const char* kFormKu = "く";            // ku-form (adverbial)
constexpr const char* kFormYou = "よう";         // volitional form
constexpr const char* kFormTa = "た";            // past tense
constexpr const char* kFormRu = "る";            // terminal form suffix

// Common particles
constexpr const char* kParticleNo = "の";        // genitive/nominalizer
constexpr const char* kParticleGa = "が";        // nominative
constexpr const char* kParticleWo = "を";        // accusative
constexpr const char* kParticleNi = "に";        // dative/locative
constexpr const char* kParticleHa = "は";        // topic marker
constexpr const char* kParticleMo = "も";        // also/even
constexpr const char* kParticleTo = "と";        // quotative/comitative
constexpr const char* kParticleHe = "へ";        // directional
constexpr const char* kParticleKa = "か";        // question marker
constexpr const char* kParticleYa = "や";        // listing marker
constexpr const char* kParticleNa = "な";        // na-adjective copula/prohibition

// Auxiliary lemmas
constexpr const char* kLemmaIru = "いる";        // progressive auxiliary
constexpr const char* kLemmaOru = "おる";        // humble progressive
constexpr const char* kLemmaShimau = "しまう";   // completive auxiliary
constexpr const char* kLemmaMiru = "みる";       // try doing
constexpr const char* kLemmaOku = "おく";        // preparatory
constexpr const char* kLemmaIku = "いく";        // continuing/going
constexpr const char* kLemmaKuru = "くる";       // coming/becoming
constexpr const char* kLemmaAgeru = "あげる";    // giving (up)
constexpr const char* kLemmaMorau = "もらう";    // receiving
constexpr const char* kLemmaKureru = "くれる";   // receiving (favor)
constexpr const char* kLemmaAru = "ある";        // existence/state
constexpr const char* kLemmaNaru = "なる";       // become
constexpr const char* kLemmaMasu = "ます";       // polite suffix
constexpr const char* kLemmaMai = "まい";        // negative volitional

// Copula and sentence-final expressions
constexpr const char* kCopulaDa = "だ";          // plain copula
constexpr const char* kCopulaDesu = "です";      // polite copula
constexpr const char* kSuffixMasen = "ません";   // polite negative

// Valid i-adjective lemma endings (non-verb derived)
// しい: おいしい, 難しい, 美しい (productive pattern)
// さい: 小さい
// きい: 大きい (validated at candidate generation)
constexpr const char* kAdjEndingShii = "しい";
constexpr const char* kAdjEndingSai = "さい";
constexpr const char* kAdjEndingKii = "きい";

// Verb contraction patterns that should not be adjectives
// んどい/んとい: verb onbin + とく contraction (読んどく→読んどい)
// とい: verb renyokei + とく contraction (見とく→見とい)
constexpr const char* kPatternNdoi = "んどい";
constexpr const char* kPatternNtoi = "んとい";
constexpr const char* kPatternToi = "とい";

// Verb+auxiliary patterns in surface (should not be adjectives)
constexpr const char* kPatternTeShima = "てしま";  // て+しまう
constexpr const char* kPatternDeShima = "でしま";  // で+しまう (voiced)
constexpr const char* kPatternTeIru = "ている";    // て+いる
constexpr const char* kPatternDeIru = "でいる";    // で+いる (voiced)
constexpr const char* kPatternTeMora = "てもら";   // て+もらう
constexpr const char* kPatternDeMora = "でもら";   // で+もらう (voiced)
constexpr const char* kPatternTeOku = "ておく";    // て+おく
constexpr const char* kPatternDeOku = "でおく";    // で+おく (voiced)
constexpr const char* kPatternTeAge = "てあげ";    // て+あげる
constexpr const char* kPatternDeAge = "であげ";    // で+あげる (voiced)
constexpr const char* kPatternTeKure = "てくれ";   // て+くれる
constexpr const char* kPatternDeKure = "でくれ";   // で+くれる (voiced)

// P4-6: Additional auxiliary verb patterns
constexpr const char* kPatternTeMiru = "てみる";   // て+みる (試行: try to)
constexpr const char* kPatternDeMiru = "でみる";   // で+みる (voiced)
constexpr const char* kPatternTeIku = "ていく";    // て+いく (方向: going)
constexpr const char* kPatternDeIku = "でいく";    // で+いく (voiced)
constexpr const char* kPatternTeKuru = "てくる";   // て+くる (方向: coming)
constexpr const char* kPatternDeKuru = "でくる";   // で+くる (voiced)
constexpr const char* kPatternTeAru = "てある";    // て+ある (状態: resultative)
constexpr const char* kPatternDeAru = "である";    // で+ある (voiced)
constexpr const char* kPatternTeOru = "ておる";    // て+おる (敬語: formal progressive)
constexpr const char* kPatternDeOru = "でおる";    // で+おる (voiced)

// Specific surfaces that are verb forms, not adjectives
constexpr const char* kSurfaceShimai = "しまい";   // しまう renyokei
constexpr const char* kSurfaceJimai = "じまい";    // じまう renyokei (voiced)

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
