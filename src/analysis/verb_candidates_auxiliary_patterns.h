/**
 * @file verb_candidates_auxiliary_patterns.h
 * @brief Auxiliary-chain surface patterns and boundary probes after a verb cell
 *
 * Patterns that let a verb + auxiliary chain split instead of merging into
 * one fabricated verb (ます, られる, させる, ない, いただく), and the positional
 * probes that tell where a lexical word or a clause boundary begins.
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_AUXILIARY_PATTERNS_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_AUXILIARY_PATTERNS_H_

#include <initializer_list>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"

namespace suzume::analysis::verb_helpers {

// =============================================================================
// Pattern Skip Helpers
// =============================================================================

/**
 * @brief Check if surface ends with ます auxiliary patterns
 *
 * Returns true if pattern should be skipped (to allow auxiliary split)
 */
bool shouldSkipMasuAuxPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check if surface ends with そう auxiliary patterns
 */
bool shouldSkipSouPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check if surface contains compound adjective patterns (にくい/やすい/がたい)
 */
bool isCompoundAdjectivePattern(std::string_view surface);

/**
 * @brief Check if surface contains adj renyokei + なる conjugation pattern
 *
 * Matches: くなっ, くなり, くなる, くなれ anywhere in the string.
 * Used to skip/penalize false candidates that absorb adj く-form + なる.
 */
bool containsKuNaruPattern(std::string_view surface);

/**
 * @brief Detect a fully spelled-out reduplicated 〜しい adjective head at @p start_pos
 *
 * 畳語 i-adjectives whose doubled stem is written out instead of using the
 * iteration mark: a repeated two-character unit (XYXY) followed by し and an
 * i-adjective inflection onset (い/く/か/け), e.g. 馬鹿馬鹿しい, バカバカしく,
 * ばかばかしかった. The halves are compared by codepoint, so one rule covers
 * kanji and both kana scripts. The iteration-mark spelling (若々しい) needs no
 * special handling because 々 keeps the stem within the regular 2-kanji path.
 *
 * @param codepoints Full input codepoints
 * @param start_pos Index of the first character of the doubled unit
 * @return true if positions [start_pos, start_pos+5] form the reduplicated head
 */
bool isReduplicatedShiiAdjectiveHead(const std::vector<char32_t>& codepoints, size_t start_pos);

/**
 * @brief Check if surface contains passive/potential auxiliary patterns
 */
bool shouldSkipPassiveAuxPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check whether the codepoint after passive れ continues an auxiliary chain
 *
 * Matches the passive/potential continuation set after れ (or られ):
 * る/た/て immediately, the closed ない-family paradigm via
 * naiNegativeFollowsAt(), ま (れます, れました), and the conditional
 * れ+ば. With @p strict_masu the ま branch additionally requires a following
 * す or せ (れます/れません), excluding bare ま.
 *
 * @param codepoints Full input codepoints
 * @param pos_after_re Index of the codepoint immediately after れ
 * @param strict_masu Require す/せ after ま
 */
bool isPassiveAuxContinuation(const std::vector<char32_t>& codepoints, size_t pos_after_re, bool strict_masu);

/** @brief Whether the passive auxiliary starting at @p passive_re_pos consumes the surface remainder. */
bool isCompletePassiveAuxiliaryAt(const std::vector<char32_t>& codepoints, size_t passive_re_pos);

/** @brief Whether the causative auxiliary starting at @p causative_se_pos consumes the surface remainder. */
bool isCompleteCausativeAuxiliaryAt(const std::vector<char32_t>& codepoints, size_t causative_se_pos);

/**
 * @brief Check if surface contains causative auxiliary patterns
 */
bool shouldSkipCausativeAuxPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check if surface matches suru-verb auxiliary patterns
 *
 * Detects サ変名詞 + する-auxiliary chains (勉強して, 対応される, 実行させた)
 * via connection-based inflection analysis: the hiragana tail after the kanji
 * run must analyze as a conjugation of する with an auxiliary chain attached.
 * Returns true if the pattern should be skipped (to allow the noun + する-aux
 * split to win).
 */
bool shouldSkipSuruVerbAuxPattern(std::string_view surface, size_t kanji_count, const grammar::Inflection& inflection);

// =============================================================================
// Auxiliary Pattern Penalty Checks (for verb candidate cost adjustment)
// =============================================================================

/**
 * @brief Check if surface contains te-form + auxiliary verb patterns
 * Uses kTeFormAuxPenaltyPatterns from scorer_constants.h
 */
bool containsTeFormAuxPattern(std::string_view surface);

/**
 * @brief Check if surface contains causative auxiliary patterns (contains-based)
 * Uses kCausativeAuxPenaltyPatterns from scorer_constants.h
 * Unlike shouldSkipCausativeAuxPattern, this uses contains() not endsWith()
 */
bool containsCausativeAuxPattern(std::string_view surface);

// A passive followed by a causative is always an auxiliary chain (書かれさせる),
// unlike an ordinary lexical compound that merely ends in せる.
bool containsPassiveCausativeAuxPattern(std::string_view surface);

// =============================================================================
// Inflection Analysis Helpers
// =============================================================================

/**
 * @brief Check whether a polite-auxiliary (ます family) follows at @p pos.
 *
 * ます / まし / ませ attach only to a verb renyokei, never to a bare noun, so
 * this licenses the verb reading of a noun/renyokei homograph (感じます).
 * Matches ま followed by す (ます), し (ました), or せ (ません).
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading ま
 */
bool masuAuxFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

// Returns the length of a complete finite ます inflection beginning at pos,
// or zero when the following characters do not form one.  The caller decides
// whether the form is at a clause boundary.
size_t finiteMasuFormLengthAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether an ichidan causative auxiliary (させ family) follows at @p pos.
 *
 * The causative させる attaches only to a verb mizenkei, never to a bare noun,
 * so — like the ます family — it licenses the verb reading of a noun/renyokei
 * homograph (感じさせる → 感じ(VERB) + させる, not 感じ(NOUN) + さ + せる).
 * Matches さ followed by せ (させる/させた/させ...).
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading さ
 */
bool causativeSaseFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a character can start a する-auxiliary after renyokei し.
 *
 * Covers the continuations of する in renyokei position:
 * ちゃ (contracted しちゃう), て/た (して/した), な (しない), ま (します),
 * よ (しよう), ろ (imperative しろ), そ (しそう), と/か/つ (しとく/しかける/しつつ).
 * Used to tell a renyokei し + する-auxiliary chain apart from a nominalized
 * noun or a false godan-sa stem that would absorb the し.
 */
bool isSuruAuxiliaryStarter(char32_t next_char);

/**
 * @brief Check whether a ない-family negative begins at @p pos.
 *
 * Matches the negative auxiliary ない and its conjugated/contracted onsets:
 * ない, なかっ(た), なく(て), なけれ(ば), なけりゃ, なきゃ. A bare な followed by
 * anything else (なる, なさい, ...) does not match, so callers can use this as
 * an unambiguous "negation follows" gate after a verb mizenkei.
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading な
 */
size_t naiNegativeFormLengthAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a ない-family negative begins at @p pos.
 *
 * Boolean facade over naiNegativeFormLengthAt() for callers that only need
 * boundary evidence rather than the exact closed-paradigm span.
 */
bool naiNegativeFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether the colloquial negative ねえ/ねぇ/ねー (知ら+ねえ) starts at @p pos.
 */
bool colloquialNegativeFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether a contracted te-continuation opens at @p pos, right after て.
 *
 * てる, てた, ちゃう and てない have no quotative reading: the quotative って is
 * never followed by る, た, ち or な, so a sokuonbin before them is a verb's
 * te-form (沼っ+てる, ディスっ+てる), not a nominal plus って.
 */
bool contractedTeContinuationFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether the sokuon at @p sokuon_pos follows a particle that closes a clause.
 *
 * A final or nominalizing particle mora (よ, の, か, ね, わ, ぞ) right after a
 * predicate ending (a terminal, imperative, い, or the copula な/だ) closes the
 * clause, so a following っ+て is the quotative って (行くよ+って, そうなの+って,
 * いいか+って) rather than a verb's sokuonbin.
 */
bool particleClosesClauseBeforeSokuon(const std::vector<char32_t>& codepoints, size_t sokuon_pos);

/**
 * @brief Check whether the volitional ending begins at @p pos.
 *
 * That is う, or the geminate it contracts to before the question particle
 * (食べよ+う, 食べよ+っ+か).
 */
bool volitionalEndingFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a candidate at @p pos would start inside a kanji run.
 *
 * A kanji run with no boundary in front of it is one word. Carving its last
 * character out as a verb splits that word (提出 into 提 + 出), so a generator
 * that proposes a single-kanji predicate has to know it is mid-run.
 *
 * @param codepoints Full input codepoints
 * @param pos Index the candidate would start at
 */
bool startsInsideKanjiRun(const std::vector<char32_t>& codepoints, size_t pos);

// True when start_pos is strictly inside a contiguous kanji run immediately
// followed by し. Such an internal position cannot begin a separate lexical
// candidate: the complete run is a productive verbal noun (提出+し) or the
// kanji portion of a lexical verb stem (見直し).
bool startsInsideKanjiRunBeforeShi(const std::vector<char32_t>& codepoints, size_t start_pos);

/**
 * @brief Check whether a span crosses a case particle that a predicate follows.
 *
 * A one-mora case particle inside a hiragana candidate is only incidental while
 * nothing on its far side is a word of its own. Once a dictionary predicate
 * stands there the particle reading is real, and the span is a phrase rather
 * than one open-class verb (み+が+ある, not みがある).
 *
 * @param dict_manager Dictionary, may be null
 * @param codepoints Full input codepoints
 * @param start_pos Span start
 * @param end_pos Span end, exclusive
 */
bool crossesCaseParticleBeforePredicate(const dictionary::DictionaryManager* dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Check whether a candidate starting at @p pos would split a known word.
 *
 * Stricter than startsInsideKanjiRun(): the run this candidate starts inside
 * has to be a dictionary word for the split to be a real loss (事故った, not
 * 複数+残った). Use it where the run is otherwise free to be several words.
 *
 * @param dict_manager Dictionary, may be null
 * @param codepoints Full input codepoints
 * @param pos Index the candidate would start at
 * @param end_pos Index the containing kanji run ends at
 */
bool splitsDictionaryKanjiWord(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t pos, size_t end_pos);

/**
 * @brief Check whether a lexical word begins at @p pos rather than an auxiliary.
 *
 * Every cell that a voice auxiliary hosts is written in kana (た, て, ない,
 * ます, 続ける's own ける is preceded by its kanji). A kanji at the boundary
 * therefore starts a new lexical word, so the auxiliary before it has to keep
 * its own token instead of being absorbed into a fabricated stem.
 *
 * @param codepoints Full input codepoints
 * @param pos Index just past the auxiliary
 */
bool lexicalWordFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a span runs across the nominalizer っこ
 *
 * A ない-family predicate behind っこ proves the suffix, and with it a morpheme
 * boundary in front of its sokuon (食べ|られ|っこ|ない). Any span reaching across
 * that boundary — or stopping between the suffix's two morae — is built on
 * material the construction has already spoken for.
 */
bool crossesKkoNominalizer(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Whether a candidate starting here opens inside the suffix がまし〜.
 *
 * The suffix derives an i-adjective from a nominal (未練がましい, 恩着せがましさ)
 * and is bound: no word begins at its が, ま or し. An adjective cell must
 * follow the stem, which separates it from the nominal まし after the subject
 * marker (こちらの方がましだ).
 *
 * @param codepoints Full input codepoints
 * @param pos Candidate start position
 */
bool startsInsideGaMashiiSuffix(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether the conditional negative auxiliary なけれ begins at @p pos.
 *
 * This is the irrealis-stem continuation used by 〜なければ.  Keeping it
 * separate from the broader ない-family gate lets candidate generators apply
 * the conditional's stronger boundary evidence without changing ordinary or
 * contracted negative forms.
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading な
 */
bool naiConditionalFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether a clause ends at @p pos.
 *
 * True at the end of the input and in front of the punctuation that closes a
 * clause. Forms whose paradigm cell is finite need this on their right-hand
 * side, so it is shared rather than restated per generator.
 *
 * @param codepoints Full input codepoints
 * @param pos Index just past the form being judged
 */
bool clauseEndsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether a dictionary entry of the given class and extended POS starts at @p pos.
 *
 * Every entry a spelling carries counts (ぞ is a final particle and a 係助詞),
 * and the probe reaches the longest classical function word (ども, ばや).
 */
bool dictionaryTailFollowsAt(const std::vector<char32_t>& codepoints, size_t pos,
                             const dictionary::DictionaryManager* dict_manager, core::PartOfSpeech pos_class,
                             std::initializer_list<core::ExtendedPOS> accepted);

/**
 * @brief Whether the run [start, end) plus a following し is the continuative of a listed Godan-sa verb.
 *
 * 見逃し is the stem of the registered 見逃す, so the run 見逃 is no verbal
 * noun for する to follow. A run that is itself a listed noun is excluded.
 */
bool isListedGodanSaContinuativeRun(const dictionary::DictionaryManager* dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start, size_t end);

/**
 * @brief Whether a case particle begins at @p pos.
 *
 * A 連体形 nominalizes as well as modifies, and the nominal it forms fills an
 * argument slot, so a case particle marks it (告げぬべかりし+に, 読みし+を). The
 * case particle is what separates that reading from the neighbours the same
 * kana spells: a conjunctive particle takes an inflected form instead
 * (注意すべく+し+て), and a final particle closes the clause behind the finite
 * cell the paradigm already covers (できな+さ+そう+だ). The probe stays within the
 * width of the longest function word so it cannot reach into the next clause.
 *
 * @param dict_manager Dictionary used to probe the follower
 * @param codepoints Full input codepoints
 * @param pos Index just past the form being judged
 */
bool caseParticleFollowsAt(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                           size_t pos);

/**
 * @brief Check whether a conjunctive particle selecting a hypothetical cell starts at @p pos.
 *
 * A paradigm cell that exists only inside a conditional is identified by that
 * particle rather than by what the clause does (高かり+せ+ば). Restricting the
 * match to the particles which select a hypothetical keeps the ordinary
 * conjunctives out, since those take a continuative and would readmit the cell
 * everywhere its spelling occurs. The probe stays within the width of the
 * longest function word so it cannot reach into the next clause.
 *
 * @param dict_manager Dictionary used to probe the follower
 * @param codepoints Full input codepoints
 * @param pos Index just past the form being judged
 */
bool hypotheticalParticleFollowsAt(const dictionary::DictionaryManager& dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether the いただく paradigm begins at @p pos.
 *
 * The receptive humble auxiliary いただく conjugates as いただ + ka-row kana
 * or the onbin い: いただか(ない), いただき, いただく, いただけ(ば/ます),
 * いただこ(う), いただい(た/て). A candidate that ends by absorbing this
 * leading い steals the auxiliary's onset (ご覧いただき → 覧い+ただき,
 * お使いいただく → 使+いい+ただく), so generators use this gate to keep the
 * い with いただく.
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading い
 */
bool itadakuParadigmStartsAt(const std::vector<char32_t>& codepoints, size_t pos);

}  // namespace suzume::analysis::verb_helpers

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_AUXILIARY_PATTERNS_H_
