#ifndef SUZUME_ANALYSIS_CANDIDATE_CONSTANTS_H_
#define SUZUME_ANALYSIS_CANDIDATE_CONSTANTS_H_

#include <cstddef>

#include "analysis/bigram_table.h"

// =============================================================================
// Candidate Generation Constants
// =============================================================================
// This file centralizes all penalty and bonus values used in candidate
// generation (join_candidates.cpp and split_candidates.cpp).
//
// Naming convention:
//   kPenalty* - increases cost (discourages pattern)
//   kBonus*   - decreases cost (encourages pattern, note: negative values)
//
// These constants determine which candidate splits/joins are preferred
// during lattice construction, before Viterbi path selection.
// =============================================================================

namespace suzume::analysis::candidate {

/**
 * @brief Candidate cost discounted by inflection confidence.
 *
 * Shared by verb and adjective generators so the scoring shape remains
 * consistent: a confidence of 1.0 keeps the base cost, and lower confidence
 * adds up to @p scale.
 */
[[nodiscard]] constexpr float confidenceScaledCost(float base, float confidence, float scale) noexcept {
  return base + (1.0F - confidence) * scale;
}

// =============================================================================
// Join Candidate Constants (join_candidates.cpp)
// =============================================================================

// Neutral origin-confidence for lattice edges that carry no inflection confidence
// (debug-only field). Named so callers needing an explicit ExtendedPOS argument can
// reach it positionally without a raw score literal.
constexpr float kNoOriginConfidence = 0.0F;

// Confidence for a direct dictionary candidate or an equivalently constrained
// closed-class candidate emitted by the tokenizer.
constexpr float kDictionaryOriginConfidence = 1.0F;

// Dictionary-backed lexicalized compounds with an inflected verb-like medial
// segment and a formal-noun tail (見た目, 行く末). This offsets the otherwise
// very strong verb + auxiliary connection path while remaining limited to the
// grammaticalized compound-noun shape.
constexpr float kLexicalizedMixedScriptNounBonus = -1.6F;

// A registered multi-character kanji noun is a verified search unit. Give it
// a small preference over a path that crosses its boundary with an unknown
// kanji run.
constexpr float kVerifiedMultiCharacterNounBonus = -0.1F;

// Closed interrogative pronouns are strong phrase anchors and must remain
// available ahead of homographic inflected-verb readings (どれを選ぶ).
constexpr float kInterrogativePronounBonus = -0.3F;

// An adverb before the explanatory quotative opener かというと keeps its
// independent interrogative boundary (なぜ+かというと).
constexpr float kInterrogativeQuoteIntroductionBonus = -1.4F;

// Ordinal compounds beginning with 第. 第 + numeral before 次 begins a
// compositional ordinal boundary (第二|次計画).
constexpr float kOrdinalSequentialSplitBonus = -1.2F;
constexpr float kOrdinalDigitCounterSplitBonus = -1.2F;
constexpr float kNumeralKanaMonthMergeBonus = -1.2F;

// A standalone duration unit must stay ahead of its clock-reading prefix
// before a degree particle (一時間くらい).
constexpr float kDurationCounterMergeBonus = bigram_cost::kVeryStrongBonus;

// A completed temporal counter with its grammatical closing suffix is a
// single search unit (二時間目, 三ヶ月間).
constexpr float kClosedTemporalCounterMergeBonus = -3.2F;

// A chained time/date/ratio quantity has one search boundary irrespective of
// the particle or word that follows it (十時三十分, 三割五分).
constexpr float kCounterChainMergeBonus = bigram_cost::kDoubleVeryStrongBonus;

// Last-resort single-character edge used to keep the lattice connected.
constexpr float kFallbackCandidateCost = 5.0F;

// A normal whole-word alternative for a suffix candidate.  Relational suffixes
// (年度末) retain a preferred stem + suffix boundary.
constexpr float kSuffixWholeCandidateCost = 1.2F;

// A kanji compound formed by a derivational noun suffix (新制度, 安全性).
// It is a complete search unit and must compete with the stem + suffix path,
// which receives a strong Noun→Suffix connection bonus.
constexpr float kDerivedSuffixCompoundNounCost = -0.2F;
// A deverbal compound noun (笑い声, 立ち姿) is the same kind of complete search
// unit, competing against a continuative + noun split whose two halves are both
// cheap on their own, so it carries the same magnitude.
constexpr float kDeverbalCompoundNounCost = kDerivedSuffixCompoundNounCost;
// The same compound when the continuative reconstructs no listed verb. The
// shape still names the construction, so the search unit stays available; what
// the missing lexical evidence costs is the discount above. Priced so a
// dictionary entry covering the span outranks it (なし崩し|的) while the
// continuative + noun split of an otherwise opaque run does not.
constexpr float kUnverifiedDeverbalCompoundNounCost = bigram_cost::kRare;
// A non-quantity nominal stem plus the closed comparison bound 以上/以下 is a
// single search unit (必要以上, 期待以下). The strong local cost compensates
// for the lexical adverb bonus carried by the homographic standalone 以上;
// generation is gated away from numeral+counter phrases.
constexpr float kComparisonCompoundNounCost = -3.6F;

// Productive nominal-base suffix verbs such as 春めく and 謎めく. The
// derivational ending identifies a verbal predicate while keeping ordinary
// unknown kanji+hira compounds available as alternatives.
constexpr float kProductiveSuffixVerbCost = -0.2F;

// Colloquial emphatic suffixes (やばいっ, きたあああ).
constexpr size_t kEmphaticMinRepeatedVowels = 2;
constexpr float kEmphaticRepeatedVowelBonus = -0.5F;
constexpr float kEmphaticRepeatedVowelLengthPenalty = 0.05F;
constexpr float kEmphaticCharacterPenalty = 0.3F;

// High origin-confidence for a rule-derived candidate whose surface context makes
// the analysis effectively unambiguous (e.g. the gated 来る mizenkei こ before a
// ない-family negative).
constexpr float kHighOriginConfidence = 0.9F;

// Compound verb bonus (連用形 + 補助動詞)
// E.g., 読み+終わる, 書き+始める
constexpr float kCompoundVerbBonus = -0.8F;

// A verified V1 continuative plus a complete closed-class V2 is one
// productive search unit.  This small category-level preference prevents the
// independently valid V1+V2 path from winning on dictionary word priors alone.
constexpr float kCompleteCompoundVerbBonus = -0.2F;

// A compound verb in renyokei followed by a deverbal noun suffix is one
// searchable nominal unit (組み合わせ方, 引き受け手).
constexpr float kCompoundVerbSuffixNounBonus = -1.0F;

// A compound whose final one-kanji Godan stem is dictionary-verified carries
// stronger evidence than a generic compound-verb proposal.
constexpr float kVerifiedTailCompoundVerbBonus = -1.4F;

// Minimum inflection confidence when a particle boundary is checked against a
// following verified verb or auxiliary form.
constexpr float kParticleVerbBoundaryMinConfidence = 0.5F;

// A particle-prefixed verb candidate is rejected when its remainder is a
// sufficiently credible verb form. Four-character surfaces need a relaxed
// threshold because one-character stems receive inflection penalties.
constexpr float kParticlePrefixedVerbRemainderMinConfidenceLong = 0.5F;
constexpr float kParticlePrefixedVerbRemainderMinConfidenceShort = 0.3F;

// Verified Ichidan verb bonus
// Applied when join creates a valid ichidan verb pattern
constexpr float kVerifiedV1Bonus = -0.3F;

// Verified noun in compound bonus
// Applied when noun component is verified in dictionary
constexpr float kVerifiedNounBonus = -0.3F;

// A geminate or moraic nasal infixed between two kanji belongs to one lexical
// compound (真っ赤, 真ん前, 赤ん坊) rather than to either neighbour.
constexpr float kInfixCompoundNounCost = 0.5F;

// An honorific prefix followed by a kanji noun and the closed suffix 様 is a
// single searchable nominal expression (お客様, ご利用者様).
constexpr float kHonorificSamaNounBonus = -0.8F;

// A terminal honorific prefix is used as an interjection rather than as an
// unattached prefix.
constexpr float kStandaloneHonorificPrefixInterjectionBonus = -0.8F;

// Unverified 3-char prefix+noun join penalty (全部食, 全部飲 from 全部食べちゃった).
// A productive prefix (全 etc.) greedily takes the whole following kanji run as
// its noun part; when that noun is not a dictionary entry and the combined
// surface is 3 chars, the previous +0.8 penalty still left the join cheaper
// than the plain 2-char kanji_seq noun split (1.0), so a fake noun like 全部食
// won over 全部|食. Raised so the final cost clears the split-path cost.
constexpr float kUnverifiedPrefixJoin3charPenalty = 1.4F;

// Productive kanji prefixes are generally favored over an unconnected prefix
// plus noun path. The intensifier 超 is intentionally a little weaker than
// closed negation, repetition, and quantity prefixes.
constexpr float kProductivePrefixJoinBonus = -0.4F;
constexpr float kIntensifierPrefixJoinBonus = -0.3F;

// An unverified long productive-prefix compound must lose to its component
// analysis unless the following noun is dictionary-attested.
constexpr float kUnverifiedPrefixJoinLongBasePenalty = 2.0F;
constexpr float kUnverifiedPrefixJoinLongPerCharPenalty = 0.5F;

// A verb continuative plus a nominal suffix (物/方/所/手/場) is one searchable
// deverbal noun unit.
constexpr float kVerbSuffixNounJoinBonus = -1.0F;

constexpr float kSuruVerbSplitPenalty = 3.0F;
constexpr float kStandaloneKanjiVerbSplitPenalty = 3.0F;
constexpr float kSingleKanjiIruVerbSplitPenalty = 2.5F;
constexpr float kGodanRaIchidanAmbiguityPenalty = 1.0F;
constexpr float kUnverifiedGodanWaExceedsVerbPenalty = 2.0F;
constexpr float kUnverifiedVerbExceedsTerminalPenalty = 3.0F;

// A negation-prefix compound with a na-adjective continuation is a productive
// predicative adjective even when its open-class base is not in the compact
// dictionary (不十分だ, 不確かではない).
constexpr float kPredicativeNegationPrefixAdjectiveBonus = -1.2F;

// =============================================================================
// Split Candidate Constants (split_candidates.cpp)
// =============================================================================

// Alpha + Kanji split bonus
// E.g., Web開発, AI研究
constexpr float kAlphaKanjiBonus = -0.3F;

// Alpha + Katakana split bonus
// E.g., APIリクエスト
constexpr float kAlphaKatakanaBonus = -0.3F;

// Dictionary word split bonus
// Applied when split creates a dictionary-verified word
constexpr float kDictSplitBonus = -0.5F;

// Base cost for split candidates
// Added to all split candidates as baseline cost
constexpr float kSplitBaseCost = 1.0F;

// Quantified-time + relational-suffix split bonus (三日|後, 十年|前, 数日|後, 半年|後)
// A temporal counter (日/年/分…) followed by 後/前 is always compositional; the
// whole run is otherwise emitted as one kanji_seq unknown word (三日後), so this
// discounts the left counter token enough for the split path to win over the
// merged token while the standalone 後/前 comes from the single-kanji candidate.
// Strong enough to also beat a productive-prefix join (半 in 半年後 → 半+年後 at 0.4);
// only ever applied when 後/前 follows a temporal counter, so it cannot over-split.
constexpr float kCounterRelationSplitBonus = -1.8F;

// Quantity + comparison-boundary split bonus (百倍|以上, 三名|以上). A
// numeral-counter phrase remains a search unit before the independent
// comparison expression, rather than merging into a long kanji sequence.
constexpr float kCounterComparisonSplitBonus = -1.2F;

// Counter-quantity 半 suffix token (三時間|半, 五分|半). Zero defers to the
// NounNumber category cost; the discount that lets the split beat the merged
// kanji_seq run lives on the left counter token (kCounterRelationSplitBonus).
// The candidate exists to carry the NounNumber EPOS, marking 半 as a quantity
// noun so connection scoring can distinguish it from an ordinary single-kanji
// noun in front of a hiragana verb (三時間|半|かかった).
constexpr float kCounterHalfSuffixCost = 0.0F;

// Closed extent suffix after a temporal quantity (一日|がけ).  The candidate
// is emitted only together with a verified NounNumber duration, so lexicalized
// continuative compounds such as 通りがけ retain their whole-word path.
constexpr float kCounterExtentSuffixCost = 0.5F;

// Native numeral counters ending in つ (一つ, 二つ) are closed-class quantity
// expressions. Keep their nominal reading ahead of accidental short-verb
// analyses before a following content word.
constexpr float kNativeTsuCounterBonus = -0.4F;

// Quantity + object-counter split bonus (三名|参加, 二台|故障, 五冊|注文)
// A numeral + discrete-object counter followed by an independent two-kanji noun
// is a compositional quantity phrase, but the whole run is otherwise emitted as
// one kanji_seq unknown word (三名参加) that beats the two-token split on total
// cost. This discounts the counter-phrase token enough for the split path to
// win; the trailing noun keeps its ordinary kanji_seq cost. Applied only under
// the structural gates in generateCounterCandidates (single object counter,
// exactly two trailing kanji, non-reduplicated), so it cannot shatter lexical
// compounds.
constexpr float kCounterNounSplitBonus = -1.2F;

// A repeated numeral+noun unit immediately before a kanji サ変 predicate is a
// distributive quantity phrase (一語一語|確認する, 一件一件|点検する). The
// regular same-type candidate otherwise absorbs the predicate into one long
// unknown kanji run.
constexpr float kRepeatedNumeralNounPredicateSplitBonus = -1.2F;

// A single-kanji ～く form immediately after a counter and before a kanji
// サ変 predicate is an adverbial modifier of the quantity (百件近く確認する).
// It needs a small preference over the homographic Godan-ka candidate.
constexpr float kCounterConditionedKuAdjectiveCost = 0.45F;

// Method suffix 方 after a surface that is both a deverbal noun and a verb
// continuative (打ち合わせ+方). This narrowly scoped cost lets the
// grammatical suffix boundary beat the fused deverbal-noun candidate.
constexpr float kDeverbalMethodSuffixCost = -1.0F;

// Leading noun/prefix + numeral + counter split bonus (徒歩|五分, 約|二時間,
// 気温|三十度, 定員|五名). A leading kanji noun or numeric-aggregation prefix glued
// to a following numeral+counter phrase is otherwise emitted as one kanji_seq unknown
// word that beats the split on total cost. This discounts the leading token enough for
// the split path to win, keeping the numeral+counter as its own search unit. Applied
// only under the structural gates in generateCounterCandidates (2+ leading kanji or a
// numeric-aggregation prefix, and a counter kanji right after the numerals).
constexpr float kLeadingNounCounterSplitBonus = -1.2F;

// Duration span + following noun split bonus (三年間|勉強, 三ヶ月間|入院, 二時間|睡眠).
// A numeral-led temporal-counter run closed by the span marker 間 is a complete
// duration; a kanji noun right after 間 is a separate word. The whole run is otherwise
// emitted as one kanji_seq unknown word (三年間勉強) that beats the split on total cost.
// This discounts the duration-phrase token enough for the split to win, keeping the
// duration as its own search unit. Applied only under the structural gates in
// generateCounterCandidates (run ends in 間 preceded by a temporal counter, followed by
// an ordinary kanji noun — not a counter/relation/span suffix or interval member 隔).
constexpr float kDurationSpanSplitBonus = -2.6F;

// Numeral + single kanji counter merge bonus (三十度, 九十度, 三十分, 十本). A multi-
// digit kanji numeral before a counter that doubles as a nominal suffix (度: 態度,
// 難易度) is pulled apart by the suffix reading plus the suffix-stem split (三十|度),
// which the plain kanji_seq merge (cost 1.0) cannot beat. This discounts the merged
// number+counter unit below the split. Applied only to a lone counter kanji at a
// kanji→non-kanji boundary, so a following kanji (五度目, 五度見た) keeps its boundary.
constexpr float kNumeralCounterMergeBonus = -0.5F;

// Quantity phrase closed by the extent marker 中 (10件中3件, 一日中, 一週間中).
// The marker also reads as an ordinary state suffix after a plain noun (作業|中),
// and that reading is what the suffix lexicon supplies; after a numeral+counter
// it instead closes the quantity into a single ratio/extent unit. This has to
// beat the counter merge plus the dictionary suffix on the split side, so it is
// set below their combined cost rather than at the plain counter-merge level.
constexpr float kQuantityExtentMergeBonus = -1.4F;

// Closed kana NounNumber + quantitative Suffix composition (いち+まい,
// よん+にん).  Both split components receive strong dictionary/bigram bonuses,
// so the complete quantity search unit needs a correspondingly stronger cost.
// The candidate generator requires exact L1 class evidence on both halves.
constexpr float kKanaNumeralCounterMergeBonus = -2.0F;

// Native counter repeated across scripts (一つひとつ). Both halves are closed
// quantity expressions and the kana half is an L1 entry with a full dictionary
// bonus, so the complete distributive unit needs a cost below their combined
// split. The generator requires a registered kana quantity closing on the same
// counter, so no productive kana run can reach this cost.
constexpr float kMixedScriptRepeatedQuantityBonus = -3.2F;

// Fraction merge cost (三分の一, 十分の三). A numerator, the denominator
// marker 分の, and a numeral denominator form one quantity search unit. The
// structure is unambiguous and this cost keeps it ahead of the individual
// counter and particle candidates.
constexpr float kFractionMergeCost = -0.8F;

// Approximate numeral split bonus (十|数件, 百|数名). A cardinal numeral before
// 数 + counter is distinct from an ordinary multi-digit numeral and keeps the
// approximate-count element as its own search unit.
constexpr float kApproximateNumeralSplitBonus = -1.2F;

// Temporal-noun boundary: an adverbial temporal noun (現在, 昨日) heading a
// 2+-kanji noun run splits off (現在|担当者) rather than merging the whole run.
// Applied in suffix_candidates_prefix.cpp.
constexpr float kTemporalNounBoundarySplitBonus = -1.2F;

// Closed kango prefixes (再/未/不) plus the nominal suffix 用 form a
// searchable compound (再利用, 未使用, 不使用) without absorbing arbitrary
// kanji runs.
constexpr float kPrefixNominalUseBonus = -1.0F;

// Noun + Verb split bonus
// E.g., 勉強+する, 説明+する
constexpr float kNounVerbSplitBonus = -1.0F;

// A kanji verbal noun after a completed predicate retains a small preference
// over a fabricated terminal verb before the independent する token.
constexpr float kSuruVerbalNounContextBonus = -0.2F;

// Post-particle noun promotion penalty (は|たばこ|を: たばこ as a Noun)
// A non-particle-initial hiragana run bracketed by genuine particles (私は…を) is
// far more likely a content noun than the merged particle-blob (はたばこ). Emit a
// parallel Noun candidate exempt from the exceeds_dict_length penalty; this small
// positive cost keeps it just below a real dictionary/verb reading so it wins only
// when nothing better spans the bracket, never shattering (た|ば|こ).
constexpr float kPostParticleNounPenalty = 0.4F;

// A short unknown nominal head selected on the left by a genitive or verified
// attributive modifier, and closed on the right by a nominal particle. These
// reproduce the ordinary hiragana length costs plus the two-sided bracket
// bonus without granting the same preference to unbounded unknown spans.
constexpr float kSelectedNominalShortHeadCost = -1.4F;
constexpr float kSelectedNominalFourMoraHeadCost = -0.8F;

// A pure-hiragana reading enclosed in brackets is metadata for the preceding
// written form. Keep it as one content token without allowing an unbounded
// unknown-word merge.
constexpr size_t kParentheticalReadingMaxLength = 12;
// A bracketed kana reading is explicit orthographic evidence, so it must win
// over the generic unknown-word fallback for the same span.
constexpr float kParentheticalReadingCandidateCost = bigram_cost::kUncommon;

// Formal-noun + na-adjective boundary (時+妙な, 事+不思議な).
// A one-kanji formal noun is a grammatical boundary before an attributive
// na-adjective. Without this penalty, the generic unknown-kanji generator
// can fuse both kanji into a fabricated noun and bypass that boundary.
constexpr float kFormalNounNaAdjectiveBoundaryPenalty = 0.5F;

// Adverb before the explanatory copular nominalizer (な+の).
// The ordinary ADV→AUX_断定 connection is intentionally penalized because
// adverbs do not normally take a copula.  In an explanatory question such as
// なぜ+な+の, however, the attributive copula closes the interrogative phrase
// before the nominalizer.  Discount the complete dictionary-backed adverb so
// a particle-like suffix inside it cannot win by avoiding that connection.
constexpr float kAdverbExplanatoryCopulaBonus = -0.5F;

// Hiragana manner adverb ending in んと (きちんと, ちゃんと). This is a
// productive mimetic shape that otherwise degrades into an unknown noun plus
// the quotative particle.
constexpr float kMimeticNtoAdverbBonus = -0.8F;

// Productive mimetic patterns are prosodic words, not arbitrary same-script
// spans.  Twelve morae covers emphatic and repeated sound-symbolic forms
// while keeping candidate generation bounded for unsegmented input.  A valid
// kana mora can have one small-kana modifier, hence the corresponding
// codepoint ceiling also bounds malformed runs made only of small kana.
constexpr size_t kMaxMimeticMorae = 12;
constexpr size_t kMaxMimeticCodepoints = kMaxMimeticMorae * 2;

// Dictionary-confirmed adverb + adjective phrases are short grammatical
// units.  Unknown candidates longer than this cannot be validated as one
// such phrase, so do not repeatedly materialize their every split.
constexpr size_t kMaxAdverbAdjectiveBoundaryChars = 16;

// Pure-hiragana onbin recognition only needs to inspect a lexical verb host
// and its two-character te/past ending.  Longer runs are compositional and
// are handled by bounded local candidates instead.
constexpr size_t kMaxHiraganaOnbinProbeChars = 12;

// Four-mora mimetic adverbs followed by the quotative particle (ちくたくと).
constexpr float kMimeticHeterogeneousAdverbCost = -0.5F;

// ABんCDん+と has two fixed nasal closures and an explicit quotative boundary,
// making it substantially stronger than an arbitrary heterogeneous kana run.
constexpr float kMimeticAlternatingNasalAdverbCost = -5.8F;

// Exact repeated halves followed by quotative と are a high-confidence
// prosodic unit; the bonus must beat one-mora verb/auxiliary homographs.
constexpr float kMimeticExactReduplicationAdverbCost = -3.5F;

// Four-character sokuon mimetics ending in り/ら (ぐったり, ふっくら).
constexpr float kMimeticSokuonMannerAdverbCost = -0.4F;
constexpr float kMimeticSokuonMannerConfidence = 0.8F;

// Verified verb in split bonus
// Applied when verb component is verified in dictionary
constexpr float kVerifiedVerbBonus = -0.8F;

// A dictionary-backed lexical Ichidan base ending in せる competes with the
// productive godan-mizenkei + causative せる analysis (合わせる vs 合わ+せる).
// Only the finite dictionary form receives this bonus; genuinely productive
// causatives have no matching lexical base entry.
constexpr float kLexicalSeruBaseBonus = -1.2F;

// =============================================================================
// Adjective Candidate Constants (adjective_candidates.cpp)
// =============================================================================

// I-adjective conjugation form split bonuses
// Applied when an inflected adjective and its following auxiliary form
// separate grammatical search units.
// E.g., 美味しくない → 美味しく + ない

// く形 split bonus (kanji i-adjectives: 美味しく, 高く)
constexpr float kAdjKuSplitBonus = -0.5F;
// く形 split bonus (hiragana i-adjectives: しんどく, うまく)
constexpr float kAdjKuSplitBonusWeak = -0.3F;
// かっ形 split bonus (past tense: 美味しかっ + た)
constexpr float kAdjKattSplitBonus = -1.0F;
// け形 split bonus (conditional: 美味しけれ + ば)
constexpr float kAdjKeSplitBonus = -0.1F;
// 語幹 split bonus (stem + そう: 美味し + そう)
constexpr float kAdjStemSplitBonus = -0.5F;

// Base costs for confidence-based adjective cost formulas
// Formula: base + (1.0 - confidence) * scale

// Kanji i-adjective candidates (美味しい, 恐ろしい)
constexpr float kKanjiAdjBaseCost = 0.2F;
constexpr float kKanjiAdjConfScale = 0.3F;
// Hiragana i-adjective candidates (すごい, うまい)
constexpr float kHiraganaAdjBaseCost = 0.25F;
constexpr float kHiraganaAdjConfScale = 0.5F;
// Adjective stem candidates (美味し + そう, 高 + さ)
constexpr float kAdjStemBaseCost = -0.8F;
constexpr float kAdjStemConfScale = 0.2F;

// Single-kanji i-adjective candidate costs
// Moderate costs so these beat competing verb candidates.
constexpr float kSingleKanjiICost = 0.35F;   // 高い, 辛い (in-context 甘いもの)
constexpr float kSingleKanjiKuCost = 0.52F;  // 甘く, 辛く renyokei

// Minimum inflection confidence to accept an i-adjective candidate
// (kanji/katakana paths and しそう stem validation)
constexpr float kIAdjConfMin = 0.5F;

// Initial confidence for a candidate that has not yet been validated.
constexpr float kNoConfidence = 0.0F;

// Confidence recorded for a candidate validated by an exact dictionary base.
constexpr float kVerifiedConfidence = 1.0F;

// Debug confidence recorded on the generated 未然形 (かろ) conjectural candidate
constexpr float kIAdjKaroConfidence = 0.8F;

// Minimum verb hypothesis confidence to treat a ゆく/いく prefix as 連用形
constexpr float kV1PrefixMinConfidence = 0.3F;

// Minimum inflection confidence for a prefix-compound's second kanji to count as
// a verb stem and thus suppress the compound (今食べてる → 今|食べ|てる)
constexpr float kPrefixCompoundVerbStemConf = 0.5F;

// Compound adjective (kanji stem: 薄暗い, 用心深い, 物悲しく)
constexpr float kCompoundAdjConfMin = 0.3F;   // minimum inflection confidence
constexpr float kCompoundAdjBaseCost = 0.5F;  // base cost for generated candidate
// A generated attributive selector must remain on an unpenalized adjective
// path.  Structurally rejected whole-span hypotheses receive additional cost
// before this gate, while the weakest regular compound adjective still passes.
constexpr float kAttributiveSelectorMaxCost =
    confidenceScaledCost(kCompoundAdjBaseCost, kCompoundAdjConfMin, kKanjiAdjConfScale);
// A morphologically complete kanji i-adjective is a lexical compound rather
// than a nominal host followed by its productive adjectival second element
// (物悲しく, 用心深い, 我慢強い).
constexpr float kCompoundIAdjectiveLexicalBonus = -2.2F;
// A kana-prefixed kanji adjective beginning at a verified post-particle word
// boundary must beat the accidental particle-chain reading of its prefix
// (が+も+の+悲しく).
constexpr float kPrefixedIAdjectiveAfterParticleBonus = -1.6F;
// Productive 連用形 + っぽい adjective (忘れっぽい, 飽きっぽい). The
// complete derived adjective competes with a very cheap stem + suffix path.
constexpr float kProductivePpoiAdjCost = -3.0F;
// Productive adjective-forming suffixes -がましい/-めかしい.  Their closed
// derivational shape is strong enough to outrank a fabricated particle plus
// independent adjective sequence while leaving ordinary i-adjectives on the
// confidence-scaled path.
constexpr float kDerivedSuffixAdjectiveCost = -1.2F;
constexpr float kHiraganaNaAdjNariConfidence = 0.9F;
constexpr float kNaAdjPredicateConfidence = 0.8F;
// Context-gated lexicalized adverbial adjective (間もなく). Its boundary must
// compete with the strong noun-to-suffix connection for the preceding 間.
constexpr float kLexicalizedAdverbialAdjCost = -0.8F;

// Na-adjective candidate costs
constexpr float kNaAdjYakaCost = 0.2F;  // やか/らか/か + な (華やかな, 静かな)
constexpr float kNaAdjTekiCost = 0.4F;  // 的 suffix (論理的) as one na-adjective search unit
constexpr float kNaAdjStemCost = 0.5F;  // kanji compound + な (獰猛な)

// Hiragana i-adjective confidence thresholds
constexpr float kHiraAdjConfMin = 0.55F;        // default hiragana-only
constexpr float kHiraAdjConfParticle = 0.50F;   // particle-starting sequences
constexpr float kHiraAdjConfProlonged = 0.40F;  // prolonged sound mark (すごーい)
constexpr float kHiraAdjUnverifiedNaiRenyokeiMin = 0.30F;
constexpr float kDerivedSuffixAdjectiveConfidence = 0.30F;

// Hiragana i-adjective cost adjustments
constexpr float kProlongedSoundBonus = -0.1F;   // colloquial すごーい, やばーい
constexpr float kLongParticleAdjBonus = -0.5F;  // 5+ char particle-starting (はなはだしい)
// A complete long -く form before a kanji/katakana head has both inflectional
// and right-boundary evidence (たやすく+答え, いちじるしく+空).
constexpr float kBoundedHiraganaKuAdjBonus = -0.5F;

// Full-reduplication 〜しい adjective bonus (馬鹿馬鹿しい, バカバカしい, ばかばかしい)
// A spelled-out doubled stem is otherwise pre-empted by onomatopoeia ADV candidates
// (aa_doubled -1.0 / abab_pattern 0.1) plus a split-off しい tail, so the whole-word
// adjective needs a strong bonus to win. Applied only when the surface matches the
// reduplicated head (see verb_helpers::isReduplicatedShiiAdjectiveHead), where the
// adjective reading is decisively correct across all three scripts.
// Must beat the otherwise valid reduplicated-adverb + fragmentary しい path;
// the exact AABB+しい structural gate makes this bonus local to complete
// adjective candidates rather than to arbitrary lexical surfaces.
constexpr float kReduplicatedShiiAdjBonus = -4.5F;

// Garu-connection adjective stem (高すぎる, 怖がる)
constexpr float kGaruAdjConfMin = 0.35F;            // minimum stem+い validity confidence
constexpr float kDictFallbackAdjConfidence = 0.5F;  // assumed confidence for dict fallback (可愛い)

// Minimum adj-over-verb confidence margin for non-dict しい stem split (話し vs 難し)
constexpr float kAdjVerbConfDiffMin = 0.15F;

// =============================================================================
// Verb Candidate Constants (verb_candidates_kanji.cpp, verb_candidates_hiragana.cpp)
// =============================================================================

// Shared cost values for verb candidate generation
namespace verb_cost {
// Standard bonus for verb candidates (mizenkei, passive, etc.)
constexpr float kStandardBonus = -0.5F;
// Strong evidence from a contracted auxiliary follower (やっ+とく).
constexpr float kContractedOnbinBonus = -1.6F;
// A one-kanji irrealis stem before the fully identified なけれ conditional
// auxiliary has an unambiguous closed-class boundary.
constexpr float kSingleKanjiNegativeConditionalBonus = -1.6F;
// Moderate bonus for verb candidates (extended/te-aux sokuonbin)
constexpr float kModerateBonus = -0.3F;
// Strong bonus for verb candidates (ichidan renyokei, te/ta forms)
constexpr float kStrongBonus = -0.8F;
// Context-gated one-mora 来る renyokei.  It must overcome the generic
// case-particle-to-short-hiragana-verb guard once both its left boundary and
// following tense/polite continuation have been proven.
constexpr float kKuruRenyokeiBonus = -1.05F;
// A bare continuative between a licensed left argument/focus phrase and a
// Japanese comma is a clause predicate, not a homographic deverbal noun or
// i-adjective. This structural evidence is intentionally independent of the
// open-class lemma and therefore applies equally to unknown Godan/Ichidan
// candidates.
constexpr float kCommaClauseRenyokeiBonus = -1.2F;
// The context-gated shortened causative candidate (読ま+さ+れる)
// must remain cheaper than the homographic suru mizenkei before both paths
// converge on the same passive lattice state.
constexpr float kShortenedCausativePassiveBonus = -0.3F;
// A quotative predicate followed by the passive される is analyzed as
// する's irrealis plus the passive auxiliary (と + さ + れる).
constexpr float kQuotedPassiveSuruBonus = -0.3F;
// A sahen nominal followed by the complete passive paradigm has both its
// lexical host and closed auxiliary boundary established (反映+さ+れ+ます).
constexpr float kSahenPassiveSuruBonus = kContractedOnbinBonus;
// Weak penalty for uncertain verb patterns (passive, causative, zu-form)
constexpr float kWeakPenalty = 0.1F;
// Minor penalty for a tense candidate with less evidence than a contracted
// auxiliary boundary.
constexpr float kMinorPenalty = 0.2F;
// An unattested onbin span whose tail is a complete auxiliary cell is reading
// that auxiliary's own onbin as its stem ending (くつだった as a form of くつだる
// rather than くつ + だっ + た). Both readings are structurally available — a
// godan stem may genuinely end in だ — so the reconstruction is discouraged
// rather than removed, and a lexical head that leaves nothing for the copula to
// predicate over still wins (坂 + を + くだっ + た).
constexpr float kSwallowedAuxiliaryCellPenalty = bigram_cost::kRare;

// A non-dictionary candidate that covers a complete closed-class particle
// crosses a grammatical boundary; keep it in the lattice but make the
// explicitly segmented path win.
constexpr float kGeneratedSpanParticlePenalty = bigram_cost::kProhibitive;
// An unattested sa-row irrealis on a pure-hiragana stem competes with the two
// other things さ is: the causative marker and the nominalizer. Those readings
// are the common ones, so the irrealis is discouraged — but not removed, since
// removing it leaves the sa row as the only row missing from the paradigm and
// its verbs then split at their own ending (おろさ+ない as おろ + さ + ない).
constexpr float kPureHiraganaSaIrrealisPenalty = bigram_cost::kStrong;
// A one-mora negative auxiliary (ぬ, ず, ね, ん, じ) after an a-row mora is also
// spelled like the last two morae of an ordinary word (かず, さくらね, みかん), so
// an irrealis built on it is weaker evidence than one closed by a multi-mora
// auxiliary. The reading stays available for the forms nothing else explains
// (しらぬ, たらぬ).
constexpr float kMonomoraNegativeIrrealisPenalty = bigram_cost::kStrong;
// Bonus for a kanji dict-verb imperative/kateikei standing sentence-final (書け, 読め, 止まれ).
// A bare え-row form terminating a clause is the imperative (命令形) of the base verb (読め→読む);
// the potential-verb reading (読める) is a distinct word that needs the full surface or a
// continuation (読めます/読めば). Applied only when no auxiliary/ば continuation follows, so
// 読める/走れます/止まれる are untouched. Sized to beat both the spurious potential-verb renyokei
// candidate (~-0.17) and the 未然+受身れ split (止ま+れ) that otherwise win over the single token.
constexpr float kImperativeFinalBonus = -0.8F;
// Minimum inflection confidence to accept a rule-constructed verb reading of a
// hiragana run — either a base form built by a candidate generator
// (isVerifiedVerbBase) or a prefix probed by a tail guard. Below this bar the run
// is treated as a non-word and the candidate/guard rejects it. Shared so the
// acceptance threshold stays uniform across the fabricated closed-class
// absorption guard family (see the guard-family note in verb_candidates_helpers.h).
constexpr float kConstructedVerbMinConfidence = 0.5F;
// Compound V1 context resolves the usual Ichidan/Godan ambiguity.
constexpr float kCompoundVerbIchidanMinConfidence = 0.25F;
// A closed, allowlisted V2 sokuonbin followed immediately by た/て supplies
// stronger compound evidence than a bare V1+V2 adjacency.  Keep this relaxed
// V1 floor local to that construction; ordinary hiragana compounds retain the
// general 0.5 threshold.
constexpr float kClosedOnbinCompoundV1MinConfidence = 0.3F;
// Stricter bar for WA-row passive base forms, which match spuriously more often.
constexpr float kConstructedVerbPassiveMinConfidence = 0.6F;
// Minimum inflection evidence for an unattested short hiragana sokuonbin
// immediately before た/て (あらっ+た, やっ+て).
constexpr float kShortHiraganaSokuonbinMinConfidence = 0.25F;
// Minimum inflection evidence for a kanji sokuonbin whose base is absent
// from the dictionary.
constexpr float kKanjiSokuonbinMinConfidence = 0.3F;
// Confidence assigned to a terminal pure-hiragana Godan base form: either one
// the inflection analyzer misclassifies as an i-adjective fragment, or one it
// reads correctly but scores as an open-class word with no dictionary entry.
constexpr float kTerminalHiraganaGodanConfidence = 0.6F;
// Minimum evidence for a rule-derived Ichidan conditional stem (…れ + ば).
constexpr float kIchidanKateikeiMinConfidence = 0.3F;

// Default floor when selecting an Ichidan analysis from ambiguous inflection
// candidates. More restrictive call sites use their own named thresholds.
constexpr float kIchidanDefaultMinConfidence = 0.4F;

// Cost of a classical ハ行四段 cell (思ふ, 思ひ, 思へ) resting on a clause
// boundary alone. The row kana is also a frequent modern particle, so the
// candidate carries no bonus and wins only where the competing split has to
// fabricate a reading of its own.
constexpr float kClassicalHaRowCost = 0.0F;
// Same cell named outright by the closed-class tail that follows it (思ひ+けり,
// 思へ+ど). The auxiliary or conjunction is positive evidence for the paradigm,
// so the cell outranks a fabricated Ichidan reading of the same span.
constexpr float kClassicalHaRowLicensedCost = kStandardBonus;
// Cost of the classical ク語法 nominalization (言わく, 思わく). The span is
// admitted only where the dictionary confirms the verb its irrealis cell comes
// from, so every competing reading of the same characters has to fabricate a
// stem — an i-adjective 言わい or a godan 言わく — and the candidate carries the
// same bonus as a paradigm cell named by its own closed-class tail.
constexpr float kKuNominalizationCost = kStandardBonus;
}  // namespace verb_cost

// =============================================================================
// Adjective Cost Adjustment Constants (adjective_candidates.cpp)
// =============================================================================

// Extended cost for adjective stem candidates (dict and non-dict)
// Used for stem+そう, stem splits where confidence is high
constexpr float kAdjStemExtCost = -1.2F;
// A dictionary-verified i-adjective stem before a productive suffix or
// auxiliary must outrank an incidental noun plus する split.
constexpr float kAdjStemDictionaryCost = -1.8F;

// Strong penalty that preserves grammatical adjective boundaries.
// Applied to compound adjectives, く+なる, という, and まい patterns.
constexpr float kAdjSplitForcePenalty = 2.0F;

// Moderate penalty for uncertain adjective patterns
// Applied to unconfirmed さ nominalization and らしい conjecture
constexpr float kAdjModeratePenalty = 1.5F;

// Nominalized renyokei before a particle is a productive deverbal noun
// context (答えは, 決まりを), not a finite verbal continuation.
constexpr float kNominalizedNounParticleBonus = -1.5F;

// A noun followed by a continuative whose verb the dictionary knows is a
// productive compound noun (手書き, 恩返し, 山登り) wherever a nominal is
// selected. Its evidence is the compound's own shape rather than an explicit
// marker to its right, which is what the standard candidate bonus is worth;
// the particle-selected bonus above is larger because it has that marker.
constexpr float kDeverbalCompoundNounBonus = verb_cost::kStandardBonus;

// A multi-kanji nominalization that no frame selects and no dictionary verb
// backs is a guess about an open-class word. The same span always reads as a
// noun plus a continuative the grammar derives, so the guess must not undercut
// that split (水|流れ, 石|転び).
constexpr float kUnselectedNominalizationPenalty = -verb_cost::kStandardBonus;

// Base candidate cost for a verified verb continuative used as a deverbal
// noun before a case particle (鳴らしを、書きを).
constexpr float kVerifiedRenyokeiNominalCandidateCost = 0.8F;

// The reciprocal-action nominalizer っこ closes a continuative into one search
// unit (にらめっこ, かけっこ). A pure-hiragana span this long is otherwise priced
// by the generic unknown model, which fragments it, so the candidate carries a
// merge bonus rather than an ordinary noun cost — the same shape as a numeral
// and its counter, whose pieces are likewise analysable on their own.
constexpr float kReciprocalActionNounCost = kNumeralCounterMergeBonus;

// The humble construction 敬語接頭辞 + V連用形 + する nominalizes the
// continuative (お伝えする, おかけする).  The verbal reading of the same span is
// generated with a continuative bonus of its own, so the nominal needs a
// stronger preference to win inside this closed frame.
constexpr float kHumbleNominalCandidateBonus = -1.2F;

// Confidence floor the continuative must clear inside that frame.  The
// inflection analyzer reconstructs a nominal ichidan paradigm for any kana run
// at the same value it uses for a bare kateikei stem, so the comparison is
// strict and the two floors move together.
constexpr float kHumbleNominalStemMinConfidence = verb_cost::kIchidanKateikeiMinConfidence;

// A dictionary-confirmed i-adjective in the classical terminal -し form is a
// self-contained lexical unit at the end of a predicate.
constexpr float kClassicalIAdjectiveTerminalNounBonus = -0.5F;

// A formal noun immediately before the copular topic sequence では/でも is a
// complete nominal predicate (はずでは, わけでも), not a particle + classical
// negative fragment. The context gate keeps locative ところで untouched.
constexpr float kFormalNounCopularTopicBonus = -0.8F;

// A formal copular topic before ある is a fixed syntactic unit (ではある),
// preferred over the separately analyzed copula and topic particle.
constexpr float kCopularTopicAruCandidateCost = -3.0F;

// A sentence-initial evaluative ことに is a closed adverbial unit only when
// the following nominal predicate is explicitly copular.  This cost keeps
// the unit available without changing ordinary こと + に argument phrases.
constexpr float kEvaluativeKotoniCandidateCost = -3.0F;

// A pure-hiragana deverbal nominal ending in -み before the independent
// adjective continuative なく (よどみなく, たゆみなく) is a content noun,
// not a fabricated inflected verb.  A lexicalized full-span adjective is
// otherwise spuriously cheaper, so the productive boundary needs the same
// modest preference as other contextually nominalized continuatives.
constexpr float kHiraganaNominalNakuCandidateBonus = -1.5F;

// The quoted final-particle pair かなと keeps two explicit sentence-particle
// boundaries despite the usual BOS/final-particle connection penalties.
constexpr float kSentenceParticleQuoteCost = -1.2F;

// A two-mora final particle at BOS needs to overcome the generic sentence-
// particle start penalty before its following quote particle supplies context.
constexpr float kLongSentenceParticleQuoteCost = -2.3F;

// The contracted explanatory negative んじゃない must outrank a chain of
// one-mora auxiliary homographs at the beginning of an utterance.
constexpr float kContractedNjaNegativeCost = -3.0F;
constexpr float kClassicalAraNLimitCost = -4.0F;

// Within that contraction, the independent negative auxiliary must remain
// whole rather than decomposing into copular and continuative homographs.
constexpr float kContractedNegativeAuxCost = -0.5F;

// Minimum inflection confidence for treating a lexical adverb ending in
// て/で as a productive verb te-form homograph before progressive いる.
constexpr float kAdverbVerbTeHomographMinConfidence = 0.65F;

}  // namespace suzume::analysis::candidate

#endif  // SUZUME_ANALYSIS_CANDIDATE_CONSTANTS_H_
