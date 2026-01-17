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
#include <vector>

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

// Hiragana compound verb auxiliary surfaces
// For MeCab-compatible splitting: 食べすぎる → 食べ + すぎる
extern const std::unordered_set<std::string_view> kHiraganaCompoundVerbAux;

// Hiragana compound verb auxiliary prefixes (for conjugated forms)
// For MeCab-compatible splitting: 食べすぎた → 食べ + すぎ + た
extern const std::vector<std::string_view> kHiraganaCompoundVerbAuxPrefixes;

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

// Check if surface is a hiragana compound verb auxiliary
inline bool isHiraganaCompoundVerbAux(std::string_view surface) {
  return kHiraganaCompoundVerbAux.find(surface) !=
         kHiraganaCompoundVerbAux.end();
}

// Check if surface starts with a hiragana compound verb auxiliary prefix
inline bool startsWithHiraganaCompoundVerbAux(std::string_view surface) {
  for (const auto& prefix : kHiraganaCompoundVerbAuxPrefixes) {
    if (surface.size() >= prefix.size() &&
        surface.substr(0, prefix.size()) == prefix) {
      return true;
    }
  }
  return false;
}

// =============================================================================
// Particle/Copula Sets (for verb candidate filtering)
// =============================================================================

// Particle strings that should not be treated as verb endings
// Includes case particles (格助詞), binding particles (係助詞), and compound particles
extern const std::unordered_set<std::string_view> kParticleStrings;

// Copula/auxiliary verb patterns that should not be treated as verb endings
extern const std::unordered_set<std::string_view> kCopulaStrings;

// =============================================================================
// Particle/Copula Lookup Functions
// =============================================================================

// Check if surface is a particle (should not be verb ending)
inline bool isParticle(std::string_view surface) {
  return kParticleStrings.find(surface) != kParticleStrings.end();
}

// Check if surface is a copula pattern (should not be verb ending)
inline bool isCopula(std::string_view surface) {
  return kCopulaStrings.find(surface) != kCopulaStrings.end();
}

// Check if surface is either particle or copula
inline bool isParticleOrCopula(std::string_view surface) {
  return isParticle(surface) || isCopula(surface);
}

// =============================================================================
// Formal Noun Strings (形式名詞)
// =============================================================================

// Formal nouns (形式名詞) - single kanji nouns with abstract grammatical functions
// These should be recognized even when not flagged from dictionary lookup
extern const std::unordered_set<std::string_view> kFormalNounStrings;

// Check if surface is a formal noun
inline bool isFormalNounSurface(std::string_view surface) {
  return kFormalNounStrings.find(surface) != kFormalNounStrings.end();
}

// =============================================================================
// Particle Codepoints (for character-level filtering)
// =============================================================================

// Case particles (格助詞) and binding particles (係助詞) as codepoints
// Used to filter out strings that start with particles from verb/adjective analysis
// を, が, は, も, へ, の, に, で, と, や, か
extern const std::unordered_set<char32_t> kParticleCodepoints;

// Check if a codepoint is a case/binding particle
inline bool isParticleCodepoint(char32_t ch) {
  return kParticleCodepoints.find(ch) != kParticleCodepoints.end();
}

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_EXCEPTIONS_H_
