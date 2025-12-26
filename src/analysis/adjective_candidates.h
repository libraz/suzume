/**
 * @file adjective_candidates.h
 * @brief Adjective-based unknown word candidate generation
 */

#ifndef SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_H_
#define SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_H_

#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

struct UnknownCandidate;
struct UnknownOptions;

/**
 * @brief Generate i-adjective candidates (kanji + conjugation endings)
 *
 * Detects patterns like 寒い, 美しい, 面白かった where kanji stem
 * is followed by hiragana conjugation endings.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary manager for base form validation (optional)
 * @return Vector of candidates
 */
std::vector<UnknownCandidate> generateAdjectiveCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager = nullptr);

/**
 * @brief Generate na-adjective candidates (〜的 patterns)
 *
 * Detects kanji compounds ending with 的 (teki) which form
 * na-adjectives like 理性的, 論理的, 感情的.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param options Unknown word generation options
 * @return Vector of candidates
 */
std::vector<UnknownCandidate> generateNaAdjectiveCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const UnknownOptions& options);

/**
 * @brief Generate hiragana i-adjective candidates (pure hiragana like まずい)
 *
 * Detects pure hiragana i-adjectives and their conjugated forms
 * like まずい, おいしい, まずかった.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @return Vector of candidates
 */
std::vector<UnknownCandidate> generateHiraganaAdjectiveCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_H_
