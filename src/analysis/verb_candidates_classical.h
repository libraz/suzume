/**
 * @file verb_candidates_classical.h
 * @brief Classical (文語) auxiliary and ending helpers for verb candidate generation
 *
 * Environments that license classical past/perfect and 終止形 cells, the
 * literary auxiliaries that follow them, and 係り結び agreement.
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_CLASSICAL_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_CLASSICAL_H_

#include <cstdint>
#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::analysis::verb_helpers {

/**
 * @brief True when a literary (文語) auxiliary starts at @p pos.
 *
 * Narrower than predicateAuxiliaryFollowsAt: only the auxiliaries the inflection
 * analyzer never emits as part of a modern paradigm qualify (see
 * core::isClassicalAuxiliaryType). Candidate generators that normally demand a
 * dictionary base form use this as the missing lexical evidence — a continuative
 * or irrealis stem is the only thing these auxiliaries can stand on, so the
 * ambiguity the dictionary gate was protecting against (連用形 み against the
 * auxiliary みたい) cannot arise in front of one.
 */
bool classicalAuxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether a classical past/perfect form can end at this position.
 *
 * The 連体形 either modifies a nominal (読みし人, 見しこと) or closes the clause,
 * and the 已然形 takes a conjunctive particle. Requiring one of those keeps the
 * far more frequent readings of the same spellings intact — the サ変 continuative
 * for し, and the ordinary stem-internal kana for the one-mora perfect (見つける
 * is not 見 + つ).
 *
 * The 已然形 has one further position, the clause-final predicate slot, which
 * this function cannot judge because the evidence for it is the continuative in
 * front rather than anything that follows; see @ref clauseEndsAt for the caller
 * that pairs the two.
 *
 * @param dict_manager Dictionary used to probe the follower
 * @param codepoints Full input codepoints
 * @param end_pos Index just past the classical form
 * @param is_izenkei Whether the form is the 已然形 cell
 */
bool classicalPastEnvironmentFollows(const dictionary::DictionaryManager& dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t end_pos, bool is_izenkei);

/**
 * @brief Whether the literary past auxiliary begins at @p pos.
 *
 * The continuative cells of the classical perfect are the one environment
 * @ref classicalPastEnvironmentFollows cannot describe: they neither close a
 * clause nor head a nominal, they hand the predicate to another auxiliary
 * (秋来+に+けり, 散り+に+き). What licenses them is that auxiliary itself.
 *
 * @param dict_manager Dictionary used to probe the follower
 * @param codepoints Full input codepoints
 * @param pos Index the follower would start at
 */
bool literaryPastAuxiliaryFollowsAt(const dictionary::DictionaryManager& dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether a classical 終止形 may end at @p pos.
 *
 * A 終止形 closes its clause or carries an auxiliary that attaches to one: the
 * conjectural べし and its negative counterpart まじ, the volitional む, and the
 * hearsay なり. Anything else after the cell belongs to a different form.
 */
bool shuushikeiEndsAt(const std::vector<char32_t>& codepoints, size_t pos,
                      const dictionary::DictionaryManager* dict_manager);

// =============================================================================
// 係り結び (binding particle and the clause-final cell it selects)
// =============================================================================

/**
 * @brief The clause-final cell a binding particle demands of its 結び.
 */
enum class KakariMusubi : uint8_t {
  None,       ///< No governing binding particle, or one that selects no cell.
  Izenkei,    ///< こそ closes its clause on the 已然形.
  Rentaikei,  ///< ぞ and なむ close theirs on the 連体形.
};

/**
 * @brief Which cell the binding particle governing @p clause_pos demands.
 *
 * 係り結び is the one long-distance agreement in the language: the particle sits
 * arbitrarily far from the form it selects, so a rule about a clause-final cell
 * cannot read the requirement off the adjacent token. The search therefore runs
 * leftward from @p clause_pos and stops at a clause boundary, since a particle
 * in an earlier clause governs nothing here.
 *
 * The binding particles do not agree on a cell — こそ takes the 已然形 while ぞ
 * and なむ take the 連体形, and the modern members (さえ, すら, しか, しも) select
 * none — so the class alone is not the answer and the individual particle is
 * resolved here once for every caller.
 */
KakariMusubi governingKakariMusubi(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t clause_pos);

/**
 * @brief Whether a registered classical auxiliary closes [.., @p end_pos).
 *
 * True when a proper suffix of the span is a dictionary auxiliary of one of the
 * classical types, so at least one mora of the span precedes it. Unlike
 * endsWithAuxiliaryAfterOkurigana this admits a one-mora head, because the head
 * being a whole word rather than okurigana is exactly the case it is asked
 * about: に+ける is the perfect's continuative plus けり, not a cell of にける.
 */
bool endsWithClassicalAuxiliary(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Whether [start_pos, end_pos) is a verb cell followed by classical auxiliaries.
 *
 * The head is a dictionary verb cell, a kanji stem with one i/e-row okurigana
 * (咲き), or a bare-kanji verb stem (見, 来, 月+見),
 * and the rest is classical auxiliary cells of 2+ morae in total, optionally
 * closed by the conjunctive ば/ど (来+べし, 月見+ぬれ+ば, 咲き+ける). A coined verb
 * spanning such a run has absorbed the auxiliary chain into its ending.
 * @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
 */
bool spellsVerbCellWithClassicalAuxiliaries(const dictionary::DictionaryManager* dict_manager,
                                            const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

}  // namespace suzume::analysis::verb_helpers

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_CLASSICAL_H_
