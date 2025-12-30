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
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

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
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param scorer Scorer for POS priors
 */
void addMixedScriptCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const Scorer& scorer);

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
void addCompoundSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer);

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
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 */
void addNounVerbSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SPLIT_CANDIDATES_H_
