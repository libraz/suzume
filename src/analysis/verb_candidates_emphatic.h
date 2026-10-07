/**
 * @file verb_candidates_emphatic.h
 * @brief Emphatic suffix variants and ordering of generated candidates
 *
 * Colloquial emphatic marks (っ, ー, small and repeated vowels) after a
 * candidate, and the cost ordering applied to a generator's candidate list.
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_EMPHATIC_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_EMPHATIC_H_

#include <string>
#include <vector>

#include "analysis/unknown.h"
#include "core/types.h"

namespace suzume::analysis::verb_helpers {

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
 * @brief Get the vowel character (あいうえお) for a hiragana's ending vowel
 *
 * Maps any hiragana to its vowel row character.
 * Returns 0 for characters without vowels (ん, っ) or non-hiragana.
 */
char32_t getHiraganaVowel(char32_t c);

/**
 * @brief A matched emphatic suffix and the input position after it.
 */
struct EmphaticSuffixMatch {
  std::string suffix;
  size_t end = 0;
  size_t standard_char_count = 0;
  size_t repeated_vowel_count = 0;

  [[nodiscard]] bool empty() const { return suffix.empty(); }

  // Whether the emphasis adds a segment rather than holding the vowel already
  // there. The glottal stop is a clipped colloquial form of the word it closes
  // (ですっ, ますっっ); a prolonged sound mark, a small vowel or a repeated
  // vowel only draws the final mora out (ですー, たああ) and leaves the word
  // itself unchanged.
  [[nodiscard]] bool addsSegment() const {
    return suffix.find("っ") != std::string::npos || suffix.find("ッ") != std::string::npos;
  }
};

/**
 * @brief Context-specific treatment of a sokuon before て/た.
 */
enum class SokuonOnsetPolicy {
  Candidate,        // Generated full-form candidate: release っ from って/った.
  DictionaryEntry,  // Dictionary stem: preserve productive onbin (あらっ+て/た).
};

/**
 * @brief Match standard emphatic marks and repeated final vowels after a candidate.
 */
EmphaticSuffixMatch matchEmphaticSuffix(const std::vector<char32_t>& codepoints, size_t base_end,
                                        core::PartOfSpeech base_pos,
                                        SokuonOnsetPolicy policy = SokuonOnsetPolicy::Candidate);

/**
 * @brief Return the cost adjustment for a matched emphatic suffix.
 */
float emphaticCostAdjustment(const EmphaticSuffixMatch& match);

/**
 * @brief Extend candidates with emphatic suffix variants
 *
 * For each verb/adjective candidate, checks if input continues with emphatic
 * characters and creates an extended variant.
 */
void addEmphaticVariants(std::vector<UnknownCandidate>& candidates, const std::vector<char32_t>& codepoints,
                         size_t first_index = 0);

// =============================================================================
// Candidate Sorting
// =============================================================================

/**
 * @brief Sort candidates by cost (lowest cost first)
 */
void sortCandidatesByCost(std::vector<UnknownCandidate>& candidates, size_t first_index = 0);

}  // namespace suzume::analysis::verb_helpers

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_EMPHATIC_H_
