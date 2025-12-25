/**
 * @file verb_candidates.h
 * @brief Verb-based unknown word candidate generation
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_H_

#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

struct UnknownCandidate;

/**
 * @brief Generate compound verb candidates (e.g., 恐れ入ります, 差し上げます)
 *
 * Detects patterns like Kanji+Hiragana+Kanji+Hiragana and checks
 * if the base form exists in dictionary.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary manager for base form verification
 * @return Vector of candidates
 */
std::vector<UnknownCandidate> generateCompoundVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Generate verb candidates (kanji + conjugation endings)
 *
 * Detects patterns like 食べる, 書いた, 飲んでいる where kanji stem
 * is followed by hiragana conjugation endings.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary manager for suffix checking
 * @return Vector of candidates
 */
std::vector<UnknownCandidate> generateVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Generate hiragana verb candidates (pure hiragana verbs like いく, くる)
 *
 * Detects pure hiragana verbs and their conjugated forms
 * like いって, きた, できなくて.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary manager for base form verification
 * @return Vector of candidates
 */
std::vector<UnknownCandidate> generateHiraganaVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_H_
