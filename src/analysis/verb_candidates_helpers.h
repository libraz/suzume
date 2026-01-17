/**
 * @file verb_candidates_helpers.h
 * @brief Internal helpers for verb candidate generation
 *
 * This file contains shared helper functions used by verb candidate generators.
 * These helpers are internal to the analysis module and should not be exposed
 * in the public API.
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_HELPERS_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_HELPERS_H_

#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/conjugation.h"
#include "unknown.h"

namespace suzume::analysis::verb_helpers {

// =============================================================================
// Single-kanji Ichidan verbs (単漢字一段動詞)
// =============================================================================

/**
 * @brief Check if character is a known single-kanji ichidan verb
 *
 * Common single-kanji Ichidan verbs:
 * 見(みる), 居(いる), 着(きる), 寝(ねる), 煮(にる), 似(にる)
 * 経(へる), 干(ひる), 射(いる), 得(える/うる), 出(でる), 鋳(いる)
 */
bool isSingleKanjiIchidan(char32_t c);

// =============================================================================
// Dictionary Lookup Helpers
// =============================================================================

/**
 * @brief Check if a base form exists in dictionary as a verb
 */
bool isVerbInDictionary(const dictionary::DictionaryManager* dict_manager,
                        std::string_view base_form);

/**
 * @brief Check if a verb is in dictionary with matching conjugation type
 */
bool isVerbInDictionaryWithType(const dictionary::DictionaryManager* dict_manager,
                                std::string_view base_form,
                                grammar::VerbType verb_type);

/**
 * @brief Check if a surface has a non-verb entry in dictionary
 */
bool hasNonVerbDictionaryEntry(const dictionary::DictionaryManager* dict_manager,
                               std::string_view surface);

// =============================================================================
// Candidate Sorting
// =============================================================================

/**
 * @brief Sort candidates by cost (lowest cost first)
 */
void sortCandidatesByCost(std::vector<UnknownCandidate>& candidates);

// =============================================================================
// Emphatic Pattern Helpers (口語強調パターン)
// =============================================================================

/**
 * @brief Check if character is an emphatic suffix character
 *
 * Emphatic characters: っ, ッ, ー, ぁぃぅぇぉ, ァィゥェォ
 */
bool isEmphaticChar(char32_t c);

/**
 * @brief Convert codepoint to UTF-8 string
 */
std::string codepointToUtf8(char32_t c);

/**
 * @brief Get the vowel character (あいうえお) for a hiragana's ending vowel
 *
 * Maps any hiragana to its vowel row character.
 * Returns 0 for characters without vowels (ん, っ) or non-hiragana.
 */
char32_t getHiraganaVowel(char32_t c);

/**
 * @brief Check if position is likely part of a verb te/ta-form, not emphatic
 */
bool isTeTaFormSokuon(const std::vector<char32_t>& codepoints, size_t sokuon_pos);

/**
 * @brief Extend candidates with emphatic suffix variants
 *
 * For each verb/adjective candidate, checks if input continues with emphatic
 * characters and creates an extended variant.
 */
void addEmphaticVariants(std::vector<UnknownCandidate>& candidates,
                         const std::vector<char32_t>& codepoints);

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
 * @brief Check if verb type is a Godan verb
 */
bool isGodanVerbType(grammar::VerbType verb_type);

/**
 * @brief Check if surface contains passive/potential auxiliary patterns
 */
bool shouldSkipPassiveAuxPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check if surface contains causative auxiliary patterns
 */
bool shouldSkipCausativeAuxPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check if surface matches suru-verb auxiliary patterns
 */
bool shouldSkipSuruVerbAuxPattern(std::string_view surface, size_t kanji_count);

}  // namespace suzume::analysis::verb_helpers

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_HELPERS_H_
