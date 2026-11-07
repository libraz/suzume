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

namespace suzume::normalize {

// =============================================================================
// Particle/Copula Sets (for verb candidate filtering)
// =============================================================================

// Particle strings that should not be treated as verb endings
// Includes case particles (格助詞), binding particles (係助詞), and compound particles
bool isParticle(std::string_view surface);

// Copula/auxiliary verb patterns that should not be treated as verb endings
bool isCopula(std::string_view surface);

// =============================================================================
// Particle/Copula Lookup Functions
// =============================================================================

// Check if surface is either particle or copula
bool isParticleOrCopula(std::string_view surface);

// =============================================================================
// Formal Noun Strings (形式名詞)
// =============================================================================

// Formal nouns (形式名詞) - single kanji nouns with abstract grammatical functions
// These should be recognized even when not flagged from dictionary lookup
// Check if surface is a formal noun
bool isFormalNounSurface(std::string_view surface);

// Check if a codepoint is a single-kanji formal noun
bool isFormalNounCodepoint(char32_t ch);

// =============================================================================
// Particle Codepoints (for character-level filtering)
// =============================================================================

// Case particles (格助詞) and binding particles (係助詞) as codepoints
// Used to filter out strings that start with particles from verb/adjective analysis
// を, が, は, も, へ, の, に, で, と, や, か
// Check if a codepoint is a case/binding particle
bool isParticleCodepoint(char32_t ch);

// The comma that chains clauses (読点). Unlike sentence-final punctuation it
// marks a juncture inside one sentence, so the predicate in front of it is
// continuative rather than terminal.
bool isClauseChainingComma(char32_t code);

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_EXCEPTIONS_H_
