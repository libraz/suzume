/**
 * @file split_candidates.h
 * @brief Split-based candidate generation for tokenizer
 *
 * Functions for generating split candidates during tokenization:
 * - Mixed script joining (e.g., Web開発, APIリクエスト)
 * - Compound noun splitting (e.g., 人工知能 → 人工 + 知能)
 * - Noun+verb splitting (e.g., 本買った → 本 + 買った)
 */

#ifndef SUZUME_ANALYSIS_SPLIT_CANDIDATES_H_
#define SUZUME_ANALYSIS_SPLIT_CANDIDATES_H_

#include <string_view>
#include <vector>

#include "analysis/scorer.h"
#include "analysis/tokenizer_utils.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

/**
 * @brief Extra split-cost adjustment for a digit+counter run ending in period 間
 *
 * A counter run whose last kanji is the period suffix 間 (分間/時間/年間) prefers
 * the duration reading over a rightward 間-compound. `period_bonus` tips the
 * common tie (30分|間営業 → 30分間|営業); it is applied twice when 間 is followed
 * by a lone temporal-relation suffix 後/前, which belongs to the duration
 * (2週|間前 → 2週間|前) rather than a 間後/間前 compound. A stranded non-suffix
 * kanji (3年|間隔) keeps the single bonus so the interval reading still wins.
 *
 * @param codepoints Unicode codepoints
 * @param candidate_end End position (exclusive) of the counter-run candidate
 * @param period_bonus Base duration bonus (opts.duration_period_bonus)
 * @return Cost adjustment to add (0 when the run does not end in 間)
 */
inline float periodSuffixSplitBonus(const std::vector<char32_t>& codepoints, size_t candidate_end, float period_bonus) {
  if (candidate_end == 0 || codepoints[candidate_end - 1] != U'間') {
    return 0.0F;
  }
  float bonus = period_bonus;
  if (candidate_end < codepoints.size() && normalize::isTemporalRelationSuffixKanji(codepoints[candidate_end])) {
    bonus += period_bonus;  // 後/前 belongs to the duration, not a 間後/間前 compound
  }
  return bonus;
}

/**
 * @brief Add mixed script joining candidates
 *
 * Detects transitions between scripts (Alphabet+Kanji, Alphabet+Katakana,
 * Digit+Kanji) and generates merged candidates with a cost bonus.
 *
 * Examples:
 *   "Web開発" → merged as single noun with bonus
 *   "APIリクエスト" → merged as single noun with bonus
 *   "3月" → merged as single noun with bonus
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param scorer Scorer for POS priors
 */
void addMixedScriptCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                              const ByteOffsets& byte_offsets, size_t start_pos,
                              const std::vector<normalize::CharType>& char_types, const Scorer& scorer,
                              const dictionary::DictionaryManager& dict_manager);

/**
 * @brief Add compound noun split candidates
 *
 * For kanji sequences of 4+ characters, generates split candidates
 * using dictionary boundary hints.
 *
 * Examples:
 *   "人工知能" → ["人工知能", "人工" + "知能"]
 *   "人工知能研究所" → ["人工知能" + "研究所", ...]
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 */
void addCompoundSplitCandidates(core::Lattice& lattice, std::string_view text, const ByteOffsets& byte_offsets,
                                size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                const dictionary::DictionaryManager& dict_manager, const Scorer& scorer);

/**
 * @brief Add noun+verb split candidates at kanji boundaries
 *
 * Detects patterns where a kanji (potential noun) is followed by
 * kanji + hiragana (potential verb) and generates split candidates.
 *
 * Examples:
 *   "本買った" → ["本" + "買った"] (noun + verb)
 *   "日本語話す" → ["日本語" + "話す"] (noun + verb)
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param dict_results Dictionary matches already looked up at start_pos
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 */
void addNounVerbSplitCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                                const ByteOffsets& byte_offsets, size_t start_pos,
                                const std::vector<dictionary::LookupResult>& dict_results,
                                const std::vector<normalize::CharType>& char_types,
                                const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                const grammar::Inflection& inflection);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SPLIT_CANDIDATES_H_
