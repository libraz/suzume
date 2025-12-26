/**
 * @file join_candidates.h
 * @brief Join-based candidate generation for tokenizer
 *
 * Functions for generating join candidates during tokenization:
 * - Compound verb joining (e.g., 飛び込む = 飛ぶ + 込む)
 * - Prefix+noun joining (e.g., お水 = お + 水)
 * - Te-form+auxiliary joining (e.g., 学んでいく = 学んで + いく)
 */

#ifndef SUZUME_ANALYSIS_JOIN_CANDIDATES_H_
#define SUZUME_ANALYSIS_JOIN_CANDIDATES_H_

#include <string_view>
#include <vector>

#include "analysis/scorer.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

/**
 * @brief Add compound verb join candidates
 *
 * Detects V1連用形 + V2 patterns and generates compound verb candidates.
 * V1 = base verb in continuative form (連用形)
 * V2 = subsidiary verb (出す, 込む, 始める, etc.)
 *
 * Examples:
 *   "飛び込む" → compound verb (飛ぶ + 込む)
 *   "読み込む" → compound verb (読む + 込む)
 *   "書き出す" → compound verb (書く + 出す)
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 * @param scorer Scorer for POS priors
 */
void addCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer);

/**
 * @brief Add hiragana compound verb join candidates
 *
 * Detects all-hiragana V1連用形 + V2 patterns where V1 is a known dictionary verb.
 * This handles compound verbs written entirely in hiragana like やりなおす.
 *
 * Examples:
 *   "やりなおす" → compound verb (やる + なおす)
 *   "やりなおしたい" → やりなおし + たい
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 * @param scorer Scorer for POS priors
 */
void addHiraganaCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer);

/**
 * @brief Add prefix + noun join candidates
 *
 * Detects productive prefix + noun patterns and generates merged candidates.
 * Prefixes include: お/ご (honorific), 不/未/非/無 (negation), 超/再/準, etc.
 *
 * Examples:
 *   "お水" → merged as single noun (お + 水)
 *   "ご確認" → merged as single noun (ご + 確認)
 *   "不安" → merged as single noun (不 + 安)
 *   "未経験" → merged as single noun (未 + 経験)
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 * @param scorer Scorer for POS priors
 */
void addPrefixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer);

/**
 * @brief Add te-form + auxiliary verb split candidates
 *
 * Detects patterns where a verb in te-form is followed by auxiliary verbs
 * like いく, くる, みる, おく, しまう and generates split candidates.
 *
 * Examples:
 *   "学んでいきたい" → ["学んで" + "いきたい"]
 *   "食べてみる" → ["食べて" + "みる"]
 *   "書いておく" → ["書いて" + "おく"]
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param scorer Scorer for POS priors
 */
void addTeFormAuxiliaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const Scorer& scorer);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_JOIN_CANDIDATES_H_
