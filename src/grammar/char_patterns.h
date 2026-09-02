/**
 * @file char_patterns.h
 * @brief Character pattern utilities for Japanese verb/adjective analysis
 */

#ifndef SUZUME_GRAMMAR_CHAR_PATTERNS_H_
#define SUZUME_GRAMMAR_CHAR_PATTERNS_H_

#include <string_view>

#include "conjugation.h"  // VerbType
#include "core/kana_constants.h"

namespace suzume::grammar {

// Import the curated conjugation-ending tables used by the inflection scorer.
using kana::kMizenkeiCount;
using kana::kMizenkeiEndings;
using kana::kRenyokeiCount;
using kana::kRenyokeiEndings;

/**
 * @brief Check if stem ends with e-row hiragana (common Ichidan endings)
 * @param stem The stem to check
 * @return True if the stem ends with e-row hiragana
 *
 * E-row hiragana includes: え, け, せ, て, ね, へ, め, れ, べ, ぺ, げ, ぜ, で
 */
bool endsWithERow(std::string_view stem);

/**
 * @brief Check if stem ends with any of the specified characters
 * @param stem The stem to check
 * @param chars Array of character strings to match
 * @param count Number of characters in the array
 * @return True if the stem ends with any of the characters
 */
bool endsWithChar(std::string_view stem, const char* const chars[], size_t count);

/**
 * @brief Check if entire stem consists only of kanji (no hiragana/katakana)
 * @param stem The stem to check
 * @return True if the stem contains only kanji characters
 *
 * Used to identify サ変名詞 stems that shouldn't be i-adjective stems.
 * CJK Unified Ideographs: U+4E00-U+9FFF and Extension A: U+3400-U+4DBF
 */
bool isAllKanji(std::string_view stem);

/**
 * @brief Check if stem ends with a kanji character
 * @param stem The stem to check
 * @return True if the last character is kanji
 *
 * Used to identify potential サ変 verb stems (勉強, 準備, etc.)
 */
bool endsWithKanji(std::string_view stem);

/**
 * @brief Check if stem starts with a kanji character
 * @param stem The stem to check
 * @return True if the first character is kanji
 *
 * Used to tell a kana-written compound-verb element (つけ, 出し) from a
 * kanji-initial content word (散り, 食べ) in the same position.
 */
bool startsWithKanji(std::string_view stem);

/**
 * @brief Check if stem contains any kanji character
 * @param stem The stem to check
 * @return True if at least one character is kanji
 *
 * Used to identify pure hiragana/katakana stems (no kanji).
 */
bool containsKanji(std::string_view stem);

/**
 * @brief Check if stem consists only of hiragana characters
 * @param stem The stem to check
 * @return True if all characters are hiragana (no kanji, katakana, etc.)
 *
 * Used to identify pure hiragana verb stems which are rare for Godan verbs.
 * Real Godan verbs typically have kanji stems (読む, 書く, 泳ぐ).
 */
bool isPureHiragana(std::string_view stem);

/**
 * @brief Whether a compound particle is the quotative + suru te-form sequence
 * @param surface Dictionary particle surface
 * @return True for the closed grammatical particle として
 */
bool isQuotativeSuruTeCompoundParticle(std::string_view surface);

/**
 * @brief Whether a surface is the renyokei form of the irregular verb する
 * @param surface Candidate surface
 * @return True for し
 */
bool isSuruRenyokeiSurface(std::string_view surface);
/** @brief Whether a surface is the base form of the irregular verb する */
bool isSuruBaseForm(std::string_view surface);
/** @brief Whether a surface is the modern volitional stem しよ of する */
bool isSuruVolitionalStemSurface(std::string_view surface);
bool isSuruImperativeSurface(std::string_view surface);

/** @brief Whether a surface is the conjunctive particle し */
bool isConjunctiveParticleShi(std::string_view surface);

/** @brief Whether a surface is a demonstrative adverb ending in う */
bool isDemonstrativeUAdverb(std::string_view surface);

/**
 * @brief Whether a prefix is the productive honorific prefix お or ご
 * @param surface Prefix surface
 * @return True for an honorific prefix
 */
bool isHonorificPrefix(std::string_view surface);

/**
 * @brief Whether a prefix is the Sino-Japanese member of the honorific pair
 *
 * ご takes a Sino-Japanese nominal and お a native one, so the two select
 * different hosts even though they fill the same slot.
 */
bool isSinoHonorificPrefix(std::string_view surface);

/**
 * @brief Whether a surface is a bound prefix that forms a lexical verb with V2
 *
 * Unlike a verb continuative, this closed-class prefix has no independent
 * predicate lemma.  A verified following verb supplies the lexical head.
 */
bool isBoundVerbPrefix(std::string_view surface);

/**
 * @brief Whether a kana ends the 終止形 of a classical bigrade verb
 *
 * The bigrade paradigm closes on the U-row kana of its own row (受く, 過ぐ,
 * 捨つ, 述ぶ, 求む, 越ゆ, 流る), which is also what its 連体形 adds る to.
 */
bool isBigradeTerminalKana(char32_t code);

/**
 * @brief Whether a U-row kana still ends a modern Godan 終止形
 *
 * The rows the modern paradigm kept (書く, 過ぐ, 立つ, 死ぬ, 求む… as 五段) are
 * reached by the conjugation table on their own, so a bigrade terminal only
 * needs its own candidate on the rows the paradigm dropped (越ゆ, 出づ).
 */
bool isModernGodanTerminalKana(char32_t code);

/**
 * @brief Whether a kana can end the stem of a modern Ichidan verb
 *
 * ハ行 is the one row the monograde paradigm lost: its intervocalic morae
 * shifted to ワ行, leaving no modern verb whose stem ends in okurigana ひ or へ.
 * A candidate built on that shape is historical kana for a ワ行五段 continuative
 * (思ひ) or simply the head of the following word (肩+ひじ). Every other i-row
 * and e-row kana still ends a monograde stem (起き, 過ぎ, 感じ, 落ち, 浴び, 求め).
 */
bool isMonogradeStemFinalKana(char32_t code);

/**
 * @brief Whether a U-row kana also spells a classical auxiliary
 *
 * つ, ぬ, む, る, ふ and す open auxiliaries that attach to a 未然形 or a
 * 連用形, so behind an inflected stem the kana is the auxiliary rather than a
 * verb's own ending.
 */
bool isClassicalAuxiliaryHomographKana(char32_t code);

/** @brief Whether a surface is a one-character kanji honorific title. */
bool isKanjiHonorificTitle(std::string_view surface);

/** @brief Whether a surface is the attributive form な of the copula だ */
bool isAttributiveCopulaNa(std::string_view surface);

/** @brief Whether a surface starts a predicative copula form (だ, です, である) */
bool startsPredicativeCopula(std::string_view surface);

/** @brief Whether a surface is the fused particle/conjunction でも */
bool isFusedDemo(std::string_view surface);

/**
 * @brief Whether a kana conjunction spells a predicate plus the conditional と
 *
 * すると, そうすると and さもないと are listed conjunctions whose surface is
 * also a productive chain. Both readings compete for the same span in ordinary
 * text, so the surface is not a fixed expression and cannot open a clause
 * directly after a nominal host.
 */
bool isConditionalToConjunction(std::string_view surface);

/** @brief Whether a surface is the benefactive formal noun おかげ */
bool isBenefactiveFormalNoun(std::string_view surface);

/**
 * @brief Whether a formal noun lexicalizes with a preceding 連用形
 *
 * もの and こと name a thing and a matter, so they combine with a continuative
 * into a compound noun of their own (飲みもの, ねがい事, 隠し事). The remaining
 * formal nouns stay bound there: they head a manner nominal that needs a case
 * particle after it (読みようがない) rather than a search unit.
 */
bool isSubstantiveFormalNoun(std::string_view surface);

/** @brief Whether a surface is the independent negative adjective ない */
bool isIndependentNegativeAdjective(std::string_view surface);

/** @brief Whether a surface is the hypothetical stem of the irregular verb ある */
bool isAruHypotheticalStem(std::string_view surface);

/** @brief Whether a surface is the complete hypothetical form of the irregular verb ある */
bool isAruHypotheticalSurface(std::string_view surface);

/** @brief Whether a surface is the continuative form of the irregular verb ある */
bool isAruContinuativeSurface(std::string_view surface);

/** @brief Whether a surface ends in the complete negative form ない */
bool endsWithNegativeNai(std::string_view surface);

/** @brief Whether an auxiliary lemma is the classical causative す */
bool isClassicalCausativeAuxiliaryLemma(std::string_view lemma);

/** @brief Whether an auxiliary lemma is the contracted negative ん */
bool isContractedNegativeAuxiliaryLemma(std::string_view lemma);

/** @brief Whether a surface is either connective-form marker て or で */
bool isTeDeSurface(std::string_view surface);

// Returns true when two lexical-verb surfaces would split the closed polite
// copula です at its internal boundary (で + す).
bool formsPoliteCopulaDesu(std::string_view left, std::string_view right);

/** @brief Whether a temporal suffix is written directly after its nominal base. */
bool isDirectAttachmentTemporalSuffix(std::string_view surface);

/** @brief Whether an aspect auxiliary has a contracted progressive surface */
bool isContractedProgressiveSurface(std::string_view surface);

/** @brief Whether a lemma is a regional contraction of the aspect verb おる */
bool isDialectalOruContractionLemma(std::string_view lemma);

// Closed subsidiary-verb homographs whose public POS depends on the selected
// grammatical predecessor rather than on their lexical lemma alone.
bool isRenyokeiPotentialAuxiliaryLemma(std::string_view lemma);
bool isTeFormCompletiveAuxiliaryLemma(std::string_view lemma);

/** @brief Whether a lemma is the passive/potential auxiliary られる */
bool isPassiveAuxiliaryLemma(std::string_view lemma);

/** @brief Whether a case-particle surface is the accusative を */
bool isAccusativeParticleWoSurface(std::string_view surface);

/** @brief Whether a conjunctive-particle surface is the concessive とも */
bool isConcessiveParticleTomoSurface(std::string_view surface);

/** @brief Whether a conjunctive-particle surface is the listing たり */
bool isListingParticleTariSurface(std::string_view surface);

/**
 * @brief Whether a conjunctive particle can only complete a 已然形/hypothetical slot
 *
 * ば/ど/ども select the inflection a predicate spells in that one paradigm cell
 * (読め+ば, 飲め+ど, 読め+ども).  Rules that reward the inflection and rules that
 * must refuse a host offering no inflected stem both read this set.
 */
bool isHypotheticalSelectingConjunctiveParticle(std::string_view surface);

/**
 * @brief Whether an auxiliary surface spells the cell ば/ど/ども select
 *
 * Each paradigm names that cell by its tail: the passive spells it れれ/られれ
 * (書か+れれ+ば) and the classical perfect spells it たれ (記録し+たれ+ども).
 * Their other cells do not — the passive's bare 未然/連用 れ, the perfect's 終止
 * たり — which is what a caller needs on either side of the boundary: whether the
 * たれ dictionary edge is licensed at all, and whether a passive form may carry
 * one of those particles.  Callers pair this with the auxiliary's class, so a tail
 * match cannot reach a cell of some other paradigm.
 */
bool spellsHypotheticalAuxiliaryCell(std::string_view surface);

/** @brief Whether a negative auxiliary surface is a colloquial conditional form */
bool isColloquialConditionalNegativeSurface(std::string_view surface);

/** @brief Whether a one-character surface is the past marker た or だ */
bool isPastMarkerTaDaSurface(std::string_view surface);

/**
 * @brief Whether an adverb is the standalone component of とともに
 * @param surface Candidate adverb surface
 * @return True for the parallel adverb ともに
 */
bool isParallelTogetherAdverb(std::string_view surface);

/**
 * @brief Whether a suffix surface is the productive state/duration marker 中
 * @param surface Candidate suffix surface
 * @return True when the suffix is 中
 */
bool isStateDurationSuffix(std::string_view surface);

/**
 * @brief Whether a suffix forms a bound event noun after a verb continuative
 * @param surface Candidate suffix surface
 * @return True for the closed deverbal nominal-suffix class
 */
bool isDeverbalNominalSuffix(std::string_view surface);

/**
 * @brief Whether a conjunctive particle contains a formal-noun host
 *
 * This closed concessive class requires a predicate on its left, unlike a
 * lexical conjunction that may open a sentence.
 */
bool isFormalNounConjunctiveParticle(std::string_view surface);

/**
 * @brief Whether a surface is the duration predicate かかる
 * @param surface Candidate predicate surface
 * @return True when the surface is かかる
 */
bool isDurationPredicateKakaru(std::string_view surface);

/**
 * @brief Whether two final particles form a licensed sentence-final stack
 * @param first Surface of the preceding final particle
 * @param second Surface of the following final particle
 * @return True for productive final-particle sequences
 */
bool isFinalParticleStackTail(std::string_view surface);

/**
 * @brief Whether a final particle also has a non-final reading of its own
 *
 * か, よ and わ are the members whose other class (focus particle, binding
 * particle) the lattice may have selected, so in a stack they need retagging;
 * the remaining members have no such homograph.
 */
bool isAmbiguousFinalParticleStackHead(std::string_view surface);

/**
 * @brief Whether a noun surface ends with an administrative suffix
 * @param surface Candidate noun surface
 * @return True for prefectural, municipal, and regional suffixes
 */
bool endsWithAdministrativeSuffix(std::string_view surface);

/**
 * @brief Whether text begins the classical desiderative まほしき sequence
 * @param surface Text at a prospective auxiliary boundary
 * @return True when the sequence begins with まほしき
 */
bool startsClassicalDesiderativeSequence(std::string_view surface);

/**
 * @brief Whether an auxiliary surface is the classical desiderative marker
 * @param surface Dictionary auxiliary surface
 * @return True for the marker ま in まほしき
 */
bool isClassicalDesiderativeMarker(std::string_view surface);

/**
 * @brief Whether text begins the classical honorific まふ sequence
 * @param surface Text at a prospective auxiliary boundary
 * @return True when the sequence begins with まふ
 */
bool startsClassicalHonorificSequence(std::string_view surface);

/**
 * @brief Whether text begins the split classical honorific auxiliary chain
 * @param surface Text after a prospective verb stem
 * @return True when the text begins with たまふ
 */
bool startsClassicalHonorificAuxiliaryChain(std::string_view surface);

/**
 * @brief Whether an auxiliary surface is a component of classical まふ
 * @param surface Dictionary auxiliary surface
 * @return True for either ま or ふ
 */
bool isClassicalHonorificComponent(std::string_view surface);

/**
 * @brief Whether a surface is the terminal component of a classical ふる form
 * @param surface Candidate verb surface
 * @return True for the one-mora terminal component ふ
 */
bool isClassicalFuruTerminal(std::string_view surface);

/** @brief Whether text begins the classical existential-conjectural construction あらん限り */
bool startsClassicalAraNLimit(std::string_view surface);

/**
 * @brief Whether a conjunctive-particle candidate is the causal ので before は
 * @param particle_surface Candidate particle surface
 * @param following_surface Text immediately after the candidate
 * @return True for the contrastive nominal construction のでは
 */
bool isCausalParticleBeforeTopic(std::string_view particle_surface, std::string_view following_surface);

/**
 * @brief Whether text begins a quoted sentence-particle sequence かなと
 * @param surface Text at a prospective sentence-particle boundary
 * @return True when the sequence begins with かなと
 */
bool startsSentenceParticleKanaQuote(std::string_view surface);

/**
 * @brief Whether text begins the interrogative quotative introduction かというと
 * @param surface Text immediately after an adverb candidate
 * @return True for the explanatory quote opener
 */
bool startsInterrogativeQuoteIntroduction(std::string_view surface);

/**
 * @brief Whether text begins the classical conjectural auxiliary けむ
 * @param surface Text immediately after a prospective verb stem
 * @return True when the classical continuative auxiliary follows
 */
bool startsClassicalConjecturalAuxiliary(std::string_view surface);

/**
 * @brief Whether text begins with a closed temporal relation/formal noun
 * @param surface Text immediately after a nominalized predicate stem
 * @return True for 前/後/時/頃 and the hiragana temporal formal-noun forms
 */
bool startsClosedTemporalNominal(std::string_view surface);

/**
 * @brief Find a long final particle immediately followed by quote particle と
 * @param surface Text at a prospective sentence-particle boundary
 * @return Long final-particle surface, or empty when absent
 */
std::string_view longFinalParticleBeforeQuote(std::string_view surface);

/**
 * @brief Whether text begins the contracted explanatory negative んじゃない
 * @param surface Text at a prospective auxiliary boundary
 * @return True when the sequence begins with んじゃない
 */
bool startsContractedNjaNegative(std::string_view surface);

/**
 * @brief Check if stem consists entirely of katakana characters
 * @param stem The stem to check
 * @return True if all characters are katakana (カタカナ)
 *
 * Used to identify pure katakana words which may be slang or loanwords.
 */
bool isPureKatakana(std::string_view stem);

// Note: kMizenkeiEndings and kRenyokeiEndings are imported from
// kana_constants.h for the inflection scorer.

/**
 * @brief Check if stem ends with i-row hiragana (godan renyokei markers)
 * @param stem The stem to check
 * @return True if the stem ends with i-row hiragana
 *
 * I-row hiragana includes: み, き, ぎ, し, ち, に, び, り, い
 */
bool endsWithIRow(std::string_view stem);

/**
 * @brief Check if a codepoint is e-row hiragana
 * @param cp Unicode codepoint to check
 * @return True if the codepoint is e-row hiragana
 *
 * E-row includes: え, け, せ, て, ね, へ, め, れ, げ, ぜ, で, べ, ぺ
 */
bool isERowCodepoint(char32_t cp);

/**
 * @brief Check if a codepoint is i-row hiragana
 * @param cp Unicode codepoint to check
 * @return True if the codepoint is i-row hiragana
 *
 * I-row includes: い, き, ぎ, し, ち, に, ひ, び, み, り
 */
bool isIRowCodepoint(char32_t cp);

/**
 * @brief Check if a codepoint is a-row hiragana
 * @param cp Unicode codepoint to check
 * @return True if the codepoint is a-row hiragana
 *
 * A-row includes: あ, か, が, さ, ざ, た, だ, な, は, ば, ぱ, ま, や, ら, わ
 * Used for verb mizenkei (未然形) detection in passive/causative patterns.
 */
bool isARowCodepoint(char32_t cp);

/**
 * @brief Check if a codepoint is o-row hiragana
 */
bool isORowCodepoint(char32_t cp);

/**
 * @brief Check if stem ends with onbin marker (音便)
 * @param stem The stem to check
 * @return True if the stem ends with い, っ, or ん
 *
 * Used for te-form and ta-form detection.
 */
bool endsWithOnbin(std::string_view stem);

/**
 * @brief Check if stem ends with renyokei marker (連用形)
 * @param stem The stem to check
 * @return True if the stem ends with i-row or e-row hiragana
 *
 * Combines godan (i-row) and ichidan (e-row) renyokei patterns.
 */
bool endsWithRenyokeiMarker(std::string_view stem);

/**
 * @brief Check if character is small kana (拗音・促音)
 * @param ch UTF-8 encoded character (3 bytes for Japanese)
 * @return True if the character is small kana
 *
 * Small kana includes: ょ, ゃ, ゅ, ぁ, ぃ, ぅ, ぇ, ぉ, っ (hiragana)
 *                      ョ, ャ, ュ, ァ, ィ, ゥ, ェ, ォ, ッ (katakana)
 * These characters cannot start a word independently.
 */
bool isSmallKana(std::string_view ch);

/**
 * @brief Check if stem ends with a-row hiragana (verb mizenkei indicators)
 * @param stem The stem to check
 * @return True if stem ends with a-row hiragana
 *
 * A-row (あ段) hiragana indicates Godan verb mizenkei (未然形).
 * Includes: あ, か, が, さ, た, な, ば, ま, ら, わ
 * Used to detect verb+ない patterns misanalyzed as adjectives.
 * E.g., 走らない (走ら = 走る mizenkei) should be verb+aux, not adjective.
 */
bool endsWithARow(std::string_view stem);

/**
 * @brief Check if stem ends with o-row hiragana (verb volitional mizenkei)
 * @param stem The stem to check
 * @return True if the final codepoint is o-row hiragana
 *
 * O-row (お段) hiragana is the mizenkei a Godan verb takes before the
 * volitional auxiliary う (書こ+う, 泳ご+う, 読も+う, しよ+う). Used to reject
 * an う reading after any non-o-row verb ending (つか+う, あら+う, す+う are
 * ungrammatical — a-row/u-row endings never take the volitional う).
 */
bool endsWithORow(std::string_view stem);

/**
 * @brief Check whether text is exactly one specified hiragana codepoint
 */
bool isSingleHiragana(std::string_view text, char32_t codepoint);

/**
 * @brief Get the vowel row character for any hiragana character
 * @param ch Unicode codepoint to check
 * @return The vowel (あ/い/う/え/お) for the character's row, or ch if not hiragana
 *
 * Used for prolonged sound mark (ー) expansion: すごーい → すごおい
 * Handles:
 * - All basic hiragana (あ-ん)
 * - Voiced variants (が, ざ, だ, ば, etc.)
 * - Semi-voiced variants (ぱ, ぴ, ぷ, ぺ, ぽ)
 * - Small kana (ゃ→あ, ゅ→う, ょ→お)
 */
char32_t getVowelForChar(char32_t ch);

/**
 * @brief Convert Godan U-row (終止形) ending to A-row (未然形) ending
 * @param u_row_cp U-row codepoint (く, す, つ, etc.)
 * @return Corresponding a-row ending (か, さ, た, etc.), or empty if invalid
 *
 * Mapping: く→か, ぐ→が, す→さ, つ→た, ぬ→な, ぶ→ば, む→ま, る→ら, う→わ
 */
std::string_view godanARowSuffixFromURow(char32_t u_row_cp);

/**
 * @brief Convert Godan U-row (終止形) ending to I-row (連用形) ending
 * @param u_row_cp U-row codepoint (く, す, つ, etc.)
 * @return Corresponding i-row ending (き, し, ち, etc.), or empty if invalid
 *
 * Mapping: く→き, ぐ→ぎ, す→し, つ→ち, ぬ→に, ぶ→び, む→み, る→り, う→い
 */
std::string_view godanIRowSuffixFromURow(char32_t u_row_cp);

/**
 * @brief Get Godan verb base suffix from A-row mizenkei ending
 * @param a_row_cp A-row codepoint (か, さ, た, etc.)
 * @return Corresponding u-row ending (く, す, つ, etc.), or empty if invalid
 *
 * Mapping:
 * - か → く (GodanKa: 書く)
 * - が → ぐ (GodanGa: 泳ぐ)
 * - さ → す (GodanSa: 話す)
 * - た → つ (GodanTa: 持つ)
 * - な → ぬ (GodanNa: 死ぬ)
 * - ば → ぶ (GodanBa: 遊ぶ)
 * - ま → む (GodanMa: 読む)
 * - ら → る (GodanRa: 取る)
 * - わ → う (GodanWa: 買う)
 */
std::string_view godanBaseSuffixFromARow(char32_t a_row_cp);

/**
 * @brief Get VerbType from A-row mizenkei ending
 * @param a_row_cp A-row codepoint (か, さ, た, etc.)
 * @return Corresponding VerbType, or Unknown if invalid
 */
VerbType verbTypeFromARowCodepoint(char32_t a_row_cp);

/**
 * @brief Get Godan verb base suffix from I-row renyokei ending
 * @param i_row_cp I-row codepoint (き, し, ち, etc.)
 * @return Corresponding u-row ending (く, す, つ, etc.), or empty if invalid
 *
 * Mapping:
 * - き → く (GodanKa: 書く)
 * - ぎ → ぐ (GodanGa: 泳ぐ)
 * - し → す (GodanSa: 話す)
 * - ち → つ (GodanTa: 持つ)
 * - に → ぬ (GodanNa: 死ぬ)
 * - び → ぶ (GodanBa: 遊ぶ)
 * - み → む (GodanMa: 読む)
 * - り → る (GodanRa: 取る)
 * - い → う (GodanWa: 買う)
 */
std::string_view godanBaseSuffixFromIRow(char32_t i_row_cp);

/**
 * @brief Get Godan verb base suffix from E-row (仮定形/potential) ending
 * @param e_row_cp E-row codepoint (け, せ, て, etc.)
 * @return Corresponding u-row ending (く, す, つ, etc.), or empty if invalid
 *
 * Mapping: け→く, げ→ぐ, せ→す, て→つ, ね→ぬ, べ→ぶ, め→む, れ→る, え→う
 * Used to derive a Godan dictionary form from a conditional/potential stem
 * (取れ→取る) or from a mis-analyzed ichidan-potential lemma (書け→書く).
 */
std::string_view godanBaseSuffixFromERow(char32_t e_row_cp);

/**
 * @brief Get VerbType from I-row renyokei ending
 * @param i_row_cp I-row codepoint (き, し, ち, etc.)
 * @return Corresponding VerbType, or Unknown if invalid
 */
VerbType verbTypeFromIRowCodepoint(char32_t i_row_cp);

/**
 * @brief Get the Godan verb type from its dictionary-form ending.
 *
 * The ending uniquely identifies every Godan row except る, which is shared
 * with Ichidan verbs. Callers must leave an ambiguous る ending unresolved.
 *
 * @param base_cp Dictionary-form final codepoint (く, す, む, etc.)
 * @return Corresponding Godan type, or Unknown for non-Godan/ambiguous endings
 */
VerbType verbTypeFromBaseCodepoint(char32_t base_cp);

/**
 * @brief Check if stem contains both hiragana and kanji characters
 * @param stem The stem to check
 * @return True if at least one hiragana and one kanji character are present
 *
 * Used to identify idiomatic mixed-script nouns (なし崩し, みじん切り, お茶).
 */
bool isMixedHiraganaKanji(std::string_view stem);

/**
 * @brief Check whether a suffix nominalizes a preceding continuative stem.
 *
 * Unlike tendency and manner suffixes, this closed suffix forms a nominal
 * predicate from the stem (疲れ気味だ).
 */
bool isRenyokeiNominalizingSuffix(std::string_view suffix);

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_CHAR_PATTERNS_H_
