#ifndef SUZUME_NORMALIZE_EXCEPTIONS_H_
#define SUZUME_NORMALIZE_EXCEPTIONS_H_

// =============================================================================
// Exception Sets
// =============================================================================
// Centralized exception sets for tokenization.
// These sets contain words that should not receive normal scoring penalties.
//
// Note: This module is for closed-class exceptions only.
// Open-class vocabulary belongs in dictionaries (L2/L3).
// =============================================================================

#include <string_view>
#include <unordered_set>

namespace suzume::normalize {

// =============================================================================
// Single Character Exceptions
// =============================================================================

// Single kanji that are valid standalone tokens (counters, temporal nouns, etc.)
// These should not receive single-character penalties during scoring.
extern const std::unordered_set<std::string_view> kSingleKanjiExceptions;

// Single hiragana functional words (particles, auxiliaries)
// These should not receive single-character penalties during scoring.
extern const std::unordered_set<std::string_view> kSingleHiraganaExceptions;

// =============================================================================
// Verb Stem Exceptions
// =============================================================================

// Single-character verb stems that are valid
// Used when validating たい patterns (e.g., したい, 見たい)
// These are Ichidan verbs or irregular verbs with single-character stems
extern const std::unordered_set<char32_t> kValidSingleCharVerbStems;

// =============================================================================
// Compound Verb Auxiliaries
// =============================================================================

// First kanji of compound verb auxiliaries
// Used to detect patterns like 読み+終わる, 食べ+始める
// Format: UTF-8 string of the first character (3 bytes for kanji)
extern const std::unordered_set<std::string_view> kCompoundVerbAuxFirstChars;

// =============================================================================
// Lookup Functions
// =============================================================================

// Check if a surface is a valid single-kanji exception
inline bool isSingleKanjiException(std::string_view surface) {
  return kSingleKanjiExceptions.find(surface) != kSingleKanjiExceptions.end();
}

// Check if a surface is a valid single-hiragana exception
inline bool isSingleHiraganaException(std::string_view surface) {
  return kSingleHiraganaExceptions.find(surface) !=
         kSingleHiraganaExceptions.end();
}

// Check if a codepoint is a valid single-character verb stem
inline bool isValidSingleCharVerbStem(char32_t ch) {
  return kValidSingleCharVerbStems.find(ch) != kValidSingleCharVerbStems.end();
}

// Check if first character indicates a compound verb auxiliary
inline bool isCompoundVerbAuxStart(std::string_view first_char) {
  return kCompoundVerbAuxFirstChars.find(first_char) !=
         kCompoundVerbAuxFirstChars.end();
}

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_EXCEPTIONS_H_
