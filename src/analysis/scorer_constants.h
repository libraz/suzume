#ifndef SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
#define SUZUME_ANALYSIS_SCORER_CONSTANTS_H_

#include <array>
#include <cstddef>
#include <string_view>

#include "analysis/bigram_table.h"
#include "core/types.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"

// =============================================================================
// Scorer Constants
// =============================================================================
// Named constants for surface-based word-cost adjustments and pattern tables used by
// scorer.cpp. Category costs live in category_cost.h; POS-pair connection costs in
// bigram_table.cpp. Prefer a named constant here over a raw literal in the .cpp
// (the CI ratchet blocks new raw score literals).
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
// Base POS connection costs are canonical in scorer.cpp's kBigramCostTable.
// Its rows are previous POS and columns are next POS; NOUN→VERB and
// VERB→NOUN are intentionally asymmetric.
// =============================================================================

namespace suzume::analysis::scorer {

// Use bigram_cost constants via this alias
namespace scale = bigram_cost;

// =============================================================================
// Surface-Based Cost Adjustments (scorer.cpp)
// =============================================================================
// These constants are used for word-cost adjustments based on surface properties.

// Bonus for compound particles from dictionary (について, によって, として, etc.)
// Multi-char particles that should not be split into verb+て patterns.
// The bonus grows with length for the same reason the dictionary adverb's does:
// the longer the registered unit, the less likely its span is a coincidence of
// shorter readings. Without the growth an adverb ending in the particle's head
// mora outbids it and strands the tail as a predicate (ことに + 関して).
constexpr float kBonusCompoundParticle = -3.2F;
constexpr float kBonusCompoundParticlePerChar = 0.85F;

// Bonus for みたい (conjecture auxiliary) from dictionary
constexpr float kBonusMitaiDict = -1.0F;

// Bonus for hiragana+kanji mixed nouns from dictionary (なし崩し, みじん切り, お茶)
constexpr float kBonusMixedNoun = -2.5F;
// Closed two-mora adverbial particles such as だけ must outrank the
// accidental copula+final-particle path (だ+け).
constexpr float kBonusTwoMoraAdverbialParticle = -2.0F;
// A multi-mora interjection is a closed fixed expression and must outrank the
// homographic predicate-plus-final-particle path (いいえ vs いい+え).
constexpr float kBonusClosedInterjection = -2.0F;
// Length-scaled bonus for long mixed nouns (4+ chars, e.g. お兄ちゃん, お父さん)
// Split paths accumulate PREFIX→NOUN→SUFFIX connection bonuses (~-1.7 advantage)
constexpr float kBonusLongMixedNounBase = -1.8F;
constexpr float kBonusLongMixedNounPerChar = 0.5F;

// Bonus for long all-kanji nouns from dictionary (4+ chars)
// Without this, split path wins due to dict+dict connection bonus (-0.5) and
// split_candidates both-in-dict bonus (-0.2), totaling -0.7 advantage
constexpr float kBonusLongKanjiNounBase = -1.0F;
constexpr float kBonusLongKanjiNounPerChar = 0.3F;

// Bonus for multi-char hiragana suffixes from dictionary (まみれ, だらけ, ごと)
constexpr float kBonusLongSuffix = -1.5F;

// Bonus for short hiragana verbs from dictionary (なる, ある, いる, する)
constexpr float kBonusShortHiraganaVerb = -0.3F;
// A pure-hiragana sokuonbin stem of this length has enough lexical evidence
// to attach its following te-form particle without reopening a shorter stem.
constexpr size_t kLongPureHiraganaOnbinMinChars = 4;

// Penalty for spurious kanji+hiragana verb renyokei not in dictionary
// E.g., 学生み (学生みる doesn't exist) - false positive
constexpr float kPenaltySpuriousVerbRenyokei = scale::kStrong;

// A kanji-containing 音便 candidate without a dictionary-verified lemma is
// especially prone to absorbing a preceding noun (本あっ from 本+あっ). Unlike
// ordinary kanji stems, an 音便 surface leaves too little evidence to license
// this fallback without lexical support.
constexpr float kPenaltySpuriousKanjiOnbin = scale::kSevere;
constexpr float kPenaltyShiiConditionalVerb = scale::kNever;

// Penalty for short/long pure-hiragana hatsuonbin verb forms
constexpr float kPenaltyHatsuonbinShort = scale::kRare;   // 2-4 chars
constexpr float kPenaltyHatsuonbinLong = scale::kSevere;  // 5+ chars

// Penalty for an unknown-verb candidate whose lemma (base form) is not
// dictionary-verified. A bare godan renyokei with no auxiliary chain and an
// unattested base is rarely a genuine verb usage, so the competing noun/suffix
// split should win (こども+たち over こど+もたち).
constexpr float kPenaltyUnverifiedVerbLemma = scale::kRare;

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

// Shared connection-level adjustments. These name the grammatical evidence
// used by multiple rule families, keeping individual scorer files focused on
// the predicate that establishes that evidence.
constexpr float kPenaltyClosedClassBoundary = scale::kAlmostNever;
constexpr float kPenaltyGeneratedInternalBoundary = scale::kVeryRare;
constexpr float kPenaltyIncompatibleInflection = scale::kSevere;
constexpr float kPenaltyInvalidConjunctiveSequence = scale::kProhibitive;
constexpr float kBonusClosedInflectionalChain = scale::kVeryStrongBonus;
constexpr float kBonusContractedNominalizer = scale::kExtremeBonus;
constexpr float kBonusConditionalPredicate = scale::kDoubleVeryStrongBonus;
constexpr float kBonusFormalParticleBinding = scale::kStrongBonus;
constexpr float kPenaltyAmbiguousSuffixBoundary = scale::kStrong;
constexpr float kPenaltyConjunctionInternalNoun = scale::kMinor;
constexpr float kBonusHonorificGeneratedRenyokei = scale::kMinorBonus;
// Behind a verb's terminal form the concessive とも competes with the quotative
// と plus the focus も, among the most productive sequences in the language, so
// the closed particle needs more than the preference its other hosts give it.
constexpr float kBonusConcessiveTomoAfterTerminal = scale::kDoubleVeryStrongBonus;
// The 連用形→意志 bigram cell is a penalty because the modern volitional
// selects the irrealis. The classical past conjecture けむ is the one member of
// that category which does take the continuative, so its rule both cancels the
// cell and supplies the ordinary continuative bonus.
constexpr float kBonusClassicalPastConjecture = scale::kStrongBonus - scale::kStrong;
constexpr float kBonusVerifiedTerminalAfterObject = scale::kModerateBonus;

// The Kuruwa polite auxiliary is a closed-class inflectional marker. Its
// dedicated category must remain available against noun-plus-suru fragments.
constexpr float kBonusKuruwaPoliteAuxiliary = scale::kExtraStrongBonus;

// =============================================================================
// Pattern String Constants
// =============================================================================

// Suffix pattern for auxiliary detection
constexpr const char* kSuffixSou = "そう";  // conjecture/hearsay

// =============================================================================
// Pattern Arrays for Auxiliary Verb Detection
// =============================================================================

// Te-form + auxiliary patterns for verb candidate penalty (17 patterns)
// Used in: verb_candidates_helpers.cpp (containsTeFormAuxPattern)
// Excludes てある/である/ておる/でおる/ていく/でいく/であげ (rare in compound verbs)
constexpr std::string_view kTeFormAuxPenaltyPatterns[] = {
    "てくる",
    "でくる",
    "てくれ",
    "でくれ",
    "ている",
    "でいる",
    "てしま",
    "でしま",
    "てもら",
    "でもら",
    "てあげ",
    "ておく",
    "でおく",
    "ておい",
    "てみる",
    "でみる",
    // Conjugated forms of くる after て: てきた, てきて, etc.
    // き is kuru renyokei, so てき covers てきた/てきて/てきている
    // Note: でき is NOT included — it conflicts with できる (suru potential form)
    "てき",
};

// Causative auxiliary patterns for verb candidate penalty
// Used in: verb_candidates_helpers.cpp (containsCausativeAuxPattern)
// Pattern: verb mizenkei + せ/させ + auxiliary (ない/て/た/ず/る/ろ/よ/なく)
constexpr std::string_view kCausativeAuxPenaltyPatterns[] = {
    "せない",   "せなく",   "せなかっ", "せて",   "せた",   "せず",   "せる",   "せろ",   "せよ",   "せれ",
    "させない", "させなく", "させて",   "させた", "させず", "させる", "させろ", "させよ", "させれ",
};

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

// =============================================================================
// Recently Extracted Constants
// =============================================================================

// VerbRenyokei (A-row ending) → VerbMizenkei(さ) causative bonus
// E.g., やら+さ+れ+た — overcome VerbRenyokei→VerbMizenkei bigram penalty (1.8)
constexpr float kBonusVerbCausativePattern = -3.0F;

// Compound particle (≥3 chars) → topic/binding particle (は, も, が)
// E.g., にとって+も, について+は — overcome ADV→NOUN bonus advantage
constexpr float kBonusCompoundParticleToTopic = -2.7F;

// Compound adjective length-scaled bonus (男らしい, 女らしい)
// Formula: base - per_char * (char_len - 4) for char_len > 4
constexpr float kBonusCompoundAdjBase = -2.5F;
constexpr float kBonusCompoundAdjPerChar = 0.5F;

// Pure-hiragana dict NOUN → し(suru renyokei) gap adjustment
// Tips balance: はなし(gap=0.013) vs なんし(gap=0.102)
constexpr float kPenaltyHiraganaNounToSuruTip = 0.08F;

// =============================================================================
// Word-Cost Length-Scaled Surface Bonuses (wordCost)
// =============================================================================
// Dictionary-entry bonuses applied in Scorer::wordCost. Most follow the pattern
// base + per_char * (char_len - min_len): the *Base term is the bonus for the
// shortest matching entry, the *PerChar term is the additional bonus subtracted
// per character beyond the threshold (positive value = stronger bonus per char).

// Pure-hiragana i-adjective from dictionary (つめたい, はなはだしい)
// Base for ≤3 chars, plus per-char beyond 3 (prevents verb+たい / adv+verb+aux splits)
constexpr float kBonusHiraganaAdjBase = -2.5F;
constexpr float kBonusHiraganaAdjPerChar = 0.6F;

// Kanji+okurigana i-adjective from dictionary (情けない), 4+ chars
constexpr float kBonusKanjiOkuriganaAdjBase = -1.5F;
constexpr float kBonusKanjiOkuriganaAdjPerChar = 0.3F;
constexpr float kBonusDictionaryNaAdjective = -1.0F;

// Closed pronouns of three or more morae can otherwise lose to a pronoun plus
// particle sequence (何かしら, あれこれ). Keep the registered lexical unit.
constexpr float kBonusLongPronoun = -3.5F;

// Pure-hiragana adverb from dictionary (たくさん, どうして)
// Short (≤2 chars) gets weaker bonus; longer uses base + per-char beyond 2
constexpr float kBonusHiraganaAdverbShort = -1.0F;
// Two-mora adverbs ending in う (そう, こう, どう) are closed demonstrative
// forms. Their lexical reading must outrank a fabricated stem plus volitional
// auxiliary path before a quotation particle.
constexpr float kBonusHiraganaUFinalAdverb = -1.6F;
constexpr float kBonusHiraganaAdverbBase = -3.0F;
constexpr float kBonusHiraganaAdverbPerChar = 0.85F;

// Non-hiragana (kanji-containing) adverb from dictionary (初めて, 大して)
// A two-character adverb is weakened the same way the hiragana branch weakens
// its own short entries: at that length the surface is short enough to be some
// other word's reading, and the full split-fighting bonus buries it.
constexpr float kBonusNonHiraganaAdverbShort = -2.5F;
constexpr float kBonusNonHiraganaAdverbBase = -3.5F;
constexpr float kBonusNonHiraganaAdverbPerChar = 0.3F;

// Kanji-containing determiner/adnominal from dictionary (小さな, 大きな), 3+ chars
constexpr float kBonusKanjiDeterminerBase = -2.3F;
constexpr float kBonusKanjiDeterminerPerChar = 0.3F;
// Closed pure-hiragana determiners (こういう, そういう) compete with a
// demonstrative adverb plus the lexical verb いう.
constexpr float kBonusHiraganaDeterminer = -3.5F;

// Multi-mora pure-hiragana noun from dictionary (向こう, かすみ, ふともも).
constexpr float kBonusLongHiraganaNoun = -3.0F;

// Pure-hiragana interjection/greeting from dictionary (さようなら, ありがとう)
// Tiered: ≤2 chars, ≤3 chars, then base + per-char beyond 3
constexpr float kBonusHiraganaInterjectionShort = -0.5F;
constexpr float kBonusHiraganaInterjectionMid = -1.5F;
constexpr float kBonusHiraganaInterjectionBase = -2.0F;
constexpr float kBonusHiraganaInterjectionPerChar = 0.5F;

// Non-hiragana (mixed-script) interjection from dictionary (お疲れ様)
constexpr float kBonusNonHiraganaInterjectionBase = -0.5F;
constexpr float kBonusNonHiraganaInterjectionPerChar = 0.3F;

// Pure-hiragana conjunction from dictionary (たとえば, それから)
// Short (≤3 chars) uses fixed bonus; longer uses base + per-char beyond 3
constexpr float kBonusHiraganaConjunctionShort = -2.0F;
constexpr float kBonusHiraganaConjunctionBase = -3.5F;
constexpr float kBonusHiraganaConjunctionPerChar = 0.5F;

// Dictionary entry starting with negation prefix (非/不/無/未), 2+ chars
constexpr float kBonusNegationPrefixBase = -3.0F;
constexpr float kBonusNegationPrefixPerChar = 0.5F;

// Dictionary NOUN starting with honorific prefix kanji 御 (御者, 御所), 2+ chars
constexpr float kBonusHonorificGoNounBase = -1.0F;
constexpr float kBonusHonorificGoNounPerChar = 0.3F;

// =============================================================================
// Connection-Cost Composed Surface Bonuses (connectionCost)
// =============================================================================
// Arithmetic-composed magnitudes kept as expressions to stay bit-identical.

// Double very-strong bonus to overcome an AdjStem→Verb / strong compound penalty
// Used by AdjStem→すぎ, all-kanji NOUN→すぎ, で→ある patterns
constexpr float kBonusDoubleVeryStrong = scale::kVeryStrongBonus * 2;  // -3.2

// Bonus for a split productive kanji V1連用 + kanji V2連用 compound verb
// (読み+終え, 書き+始め). Two kanji content-verb 連用形 halves that already split
// mark a genuine V1+V2 compound (MeCab splits 読み終える), so V1 must stay
// VerbRenyokei instead of collapsing to a deverbal NOUN (読み as 名詞, which wins
// via NOUN→VerbRenyokei after an adverb). The global no-bonus policy for
// VerbRenyokei→VerbRenyokei (bigram_table.cpp) guards hiragana-tail lexicalized
// compounds (抱き+しめ); single-token lexical compounds (受け入れ) never expose
// this junction, so gating on kanji in BOTH surfaces only refines the POS of
// already-split pairs. Returns 0 when the pattern does not apply.
inline float compoundVerbSplitBonus(core::ExtendedPOS prev_epos, std::string_view prev_surface,
                                    core::ExtendedPOS next_epos, std::string_view next_surface) {
  if (prev_epos != core::ExtendedPOS::VerbRenyokei || next_epos != core::ExtendedPOS::VerbRenyokei) {
    return 0.0F;
  }
  if (!grammar::containsKanji(prev_surface) || !grammar::containsKanji(next_surface)) {
    return 0.0F;
  }
  return scale::kModerateBonus;
}

// =============================================================================
// Negation-Prefix Kanji Predicates
// =============================================================================
// The negation prefixes 非/不/無/未 form single lexical items (不可能, 非常,
// 無理, 未定). A dictionary entry beginning with one gets a length-scaled bonus
// so the whole word beats the PREFIX+NOUN split, and a standalone prefix from
// this set must not attach to a following single-kanji noun.
constexpr std::string_view kNegationPrefixKanji[] = {"非", "不", "無", "未"};

/// True if the surface begins with a negation-prefix kanji (非/不/無/未).
[[nodiscard]] inline bool startsWithNegationPrefix(std::string_view surface) {
  return utf8::startsWithAny(surface, kNegationPrefixKanji);
}

/// True if the surface is exactly a negation-prefix kanji (非/不/無/未).
[[nodiscard]] inline bool isNegationPrefix(std::string_view surface) {
  return utf8::equalsAny(surface, kNegationPrefixKanji);
}

// =============================================================================
// Sentence-Boundary Costs (scorer_boundary_cost.cpp)
// =============================================================================
// BOS (beginning-of-sentence) connection-cost adjustments. A morpheme that
// cannot naturally start a sentence is penalized; a conjunction is rewarded.
constexpr float kBosSuffixPenalty = 3.0F;      // Suffix cannot lead a sentence
constexpr float kBosConjunctionBonus = -0.5F;  // でも / しかし are natural at BOS
constexpr float kBosDemoConjunctionBonus = scale::kDoubleVeryStrongBonus;
constexpr float kBosAppearanceSouPenalty = 0.5F;             // 様態そう should be demonstrative at BOS
constexpr float kBosAspectIruPenalty = scale::kRare;         // いる aspect needs a preceding て-form
constexpr float kBosAspectShimauPenalty = scale::kVeryRare;  // しまう aspect needs a preceding て-form
constexpr float kBosAspectOkuPenalty = scale::kVeryRare;     // おく aspect needs a preceding て-form
constexpr float kBosAspectIkuPenalty = 1.0F;                 // いく aspect needs a preceding て-form
constexpr float kBosAspectKuruPenalty = 3.0F;                // くる aspect (き) needs a preceding て-form
constexpr float kBosTensePenalty = 2.0F;                     // た/だ needs a preceding verb/adj stem
constexpr float kBosFinalParticlePenalty = 2.0F;             // Sentence-final particle cannot lead
constexpr float kBosTopicParticlePenalty = 1.0F;             // 係助詞 は/も cannot lead a sentence
constexpr float kBosConjunctiveParticlePenalty = 1.0F;       // 接続助詞 joins clauses, so it rarely leads one
constexpr float kBosBindingParticlePenalty = scale::kRare;   // 係結び has no host at sentence start
// A 格助詞 marks the case of the nominal in front of it, so it is bound leftward
// exactly as the topic and binding particles are. At sentence start there is no
// such nominal, and without the penalty the particle reading of a noun's first
// mora costs nothing at all (となり read as と + なり).
constexpr float kBosCaseParticlePenalty = scale::kMinor;
// の binds leftward on both of its readings: the genitive selects the nominal it
// modifies with, and the nominalizer selects the clause it turns into one. Either
// way there is nothing to its left at sentence start, and the first mora of a
// kana verb otherwise reads as the particle for free (のばす as の + ば + す).
constexpr float kBosNominalizerParticlePenalty = scale::kMinor;
constexpr float kBosHonorificAuxPenalty = 0.3F;  // Honorific auxiliary needs a preceding renyokei
// A polite auxiliary selects a continuative, so it cannot lead a sentence
// either. Its regional members spell ordinary kana runs, and at sentence start
// there is nothing for them to attach to (なんしよっと read as なんし + よっと).
constexpr float kBosPoliteAuxPenalty = scale::kRare;
// A classical perfect り / negative ん attaches to a preceding 連用形 or 未然形,
// so neither can lead a sentence. Without this the two auxiliaries chain into a
// cheap closed-class fragment run that outscores an unregistered hiragana noun
// occupying the whole input (りんご read as り + ん + ご).
constexpr float kBosClassicalPerfectPenalty = kBosTensePenalty;
constexpr float kBosClassicalNegativePenalty = kBosTensePenalty;
// The copula predicates over a nominal, so it needs one before it. At sentence
// start there is none, and without a penalty its one-mora attributive form opens
// a nominalized clause out of nothing (なんだ read as な + ん + だ, なんしよう as
// な + ん + しよう). The penalty stops short of the grammatical tiers because a
// fragment may genuinely open on the copula's own continuative, its nominal
// supplied by the preceding context (ではあるまいか).
constexpr float kBosCopulaPenalty = scale::kRare;

// EOS (end-of-sentence) adjustments share the table below with BOS. The two
// columns are intentionally asymmetric: a final particle can naturally close a
// sentence, while a prefix cannot.
constexpr float kEosAspectKuruPenalty = 3.0F;  // き (来 aspect) needs a following stem (ひこうき → ひこう+き)
constexpr float kEosListingParticlePenalty = 2.0F;  // たり listing particle needs a parallel predicate
constexpr float kEosBindingParticleBonus = scale::kExtraStrongBonus;  // Keep standalone こそ/さえ intact
constexpr float kEosPrefixPenalty = scale::kAlmostNever;              // Prefix requires a following host
// A conjunction joins what precedes it to what follows, so it cannot be the last
// morpheme of a sentence that already has material in front of it. A conjunction
// that is the whole utterance, or that opens a fragment after a punctuation mark,
// is exempt: there it introduces the following context instead.
constexpr float kEosConjunctionPenalty = scale::kSevere;
// A 連体詞 modifies the nominal after it, so it cannot close a sentence either.
// Without this a determiner homographic with a finished predicate wins over that
// predicate when nothing follows (とんだ read as the determiner, not 飛ん+だ).
constexpr float kEosDeterminerPenalty = kEosPrefixPenalty;
// A one-mora continuative cannot close a sentence: it always needs an auxiliary
// or a subsidiary verb after it (確認したらしい, not 確認したら+し+い).
constexpr float kEosShortRenyokeiPenalty = scale::kStrong;
// A 連用形 followed by a 形式名詞 is a manner nominal (読みよう, 言いよう): a
// bound nominal that heads a phrase only together with the case particle after
// it, so it never closes a clause. Cancelling its connection bonus at the end
// of a sentence leaves the competing volitional reading of the same run — the
// only reading available there — to win (やめよう as やめよ+う, not やめ+よう).
constexpr float kEosRenyokeiFormalNounPenalty = scale::kAlmostNever;
// An irrealis form is the bare stem of an auxiliary chain, so it can never be
// the last morpheme: something has to fill the slot it opened. Without this a
// fabricated irrealis covering the conditional なら+ば wins over the copula
// plus its particle when nothing follows (雨ならば as 雨+ならば).
constexpr float kEosMizenkeiPenalty = scale::kAlmostNever;

enum class EosBoundaryGate {
  Always,
  SingleCodepoint,
  ListingParticle,
  NonDictionary,
  AfterContent,
};

struct BoundaryCost {
  float bos;
  float eos;
  EosBoundaryGate eos_gate{EosBoundaryGate::Always};
};

/**
 * Sentence-boundary costs indexed by ExtendedPOS.
 *
 * Keeping BOS and EOS adjustments in the same row makes intentionally
 * asymmetric categories explicit and prevents boundary-only rules from
 * drifting into wordCost().
 */
constexpr std::array<BoundaryCost, static_cast<size_t>(core::ExtendedPOS::Count_)> kBoundaryCostTable = [] {
  std::array<BoundaryCost, static_cast<size_t>(core::ExtendedPOS::Count_)> table{};

  table[static_cast<size_t>(core::ExtendedPOS::Conjunction)].bos = kBosConjunctionBonus;
  table[static_cast<size_t>(core::ExtendedPOS::Suffix)].bos = kBosSuffixPenalty;

  table[static_cast<size_t>(core::ExtendedPOS::AuxAppearanceSou)].bos = kBosAppearanceSouPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxAspectIru)].bos = kBosAspectIruPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxAspectShimau)].bos = kBosAspectShimauPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxAspectOku)].bos = kBosAspectOkuPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxAspectIku)].bos = kBosAspectIkuPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxAspectKuru)].bos = kBosAspectKuruPenalty;
  // AuxAspectMiru and AuxAspectHajimeru intentionally have no BOS row: their
  // full surfaces are also ordinary finite lexical verbs. Penalizing them at
  // BOS would damage that reading before a grammatical host can disambiguate.
  table[static_cast<size_t>(core::ExtendedPOS::AuxTenseTa)].bos = kBosTensePenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxHonorific)].bos = kBosHonorificAuxPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxKuruwaPolite)].bos = kBosPoliteAuxPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxClassicalPerfect)].bos = kBosClassicalPerfectPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxNegativeNu)].bos = kBosClassicalNegativePenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxCopulaDa)].bos = kBosCopulaPenalty;

  table[static_cast<size_t>(core::ExtendedPOS::ParticleFinal)].bos = kBosFinalParticlePenalty;
  table[static_cast<size_t>(core::ExtendedPOS::ParticleTopic)].bos = kBosTopicParticlePenalty;
  table[static_cast<size_t>(core::ExtendedPOS::ParticleConj)].bos = kBosConjunctiveParticlePenalty;
  table[static_cast<size_t>(core::ExtendedPOS::ParticleBinding)].bos = kBosBindingParticlePenalty;
  table[static_cast<size_t>(core::ExtendedPOS::ParticleCase)].bos = kBosCaseParticlePenalty;
  table[static_cast<size_t>(core::ExtendedPOS::ParticleNo)].bos = kBosNominalizerParticlePenalty;

  table[static_cast<size_t>(core::ExtendedPOS::AuxAspectKuru)].eos = kEosAspectKuruPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::AuxAspectKuru)].eos_gate = EosBoundaryGate::SingleCodepoint;
  // No other aspect auxiliary has an EOS row: its full finite form may close
  // a sentence. Kuru is exceptional only for the one-mora continuative き.
  table[static_cast<size_t>(core::ExtendedPOS::ParticleConj)].eos = kEosListingParticlePenalty;
  table[static_cast<size_t>(core::ExtendedPOS::ParticleConj)].eos_gate = EosBoundaryGate::ListingParticle;
  table[static_cast<size_t>(core::ExtendedPOS::ParticleBinding)].eos = kEosBindingParticleBonus;
  table[static_cast<size_t>(core::ExtendedPOS::Conjunction)].eos = kEosConjunctionPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::Conjunction)].eos_gate = EosBoundaryGate::AfterContent;
  table[static_cast<size_t>(core::ExtendedPOS::Prefix)].eos = kEosPrefixPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::Determiner)].eos = kEosDeterminerPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::Determiner)].eos_gate = EosBoundaryGate::NonDictionary;
  table[static_cast<size_t>(core::ExtendedPOS::VerbRenyokei)].eos = kEosShortRenyokeiPenalty;
  table[static_cast<size_t>(core::ExtendedPOS::VerbRenyokei)].eos_gate = EosBoundaryGate::SingleCodepoint;
  table[static_cast<size_t>(core::ExtendedPOS::VerbMizenkei)].eos = kEosMizenkeiPenalty;

  return table;
}();

[[nodiscard]] constexpr BoundaryCost getBoundaryCost(core::ExtendedPOS extended_pos) noexcept {
  if (!core::isValidExtendedPos(extended_pos)) {
    return {};
  }
  return kBoundaryCostTable[static_cast<size_t>(extended_pos)];
}

}  // namespace suzume::analysis::scorer

#endif  // SUZUME_ANALYSIS_SCORER_CONSTANTS_H_
