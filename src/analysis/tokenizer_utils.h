/**
 * @file tokenizer_utils.h
 * @brief Utility functions for tokenizer
 */

#ifndef SUZUME_ANALYSIS_TOKENIZER_UTILS_H_
#define SUZUME_ANALYSIS_TOKENIZER_UTILS_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/lattice.h"
#include "normalize/char_type.h"

namespace suzume {
namespace dictionary {
class DictionaryManager;
struct LookupResult;
}  // namespace dictionary
namespace grammar {
class Inflection;
struct InflectionCandidate;
}  // namespace grammar
}  // namespace suzume

namespace suzume::analysis {

using ByteOffsets = std::vector<size_t>;
using PartOfSpeechMask = uint32_t;

inline constexpr size_t kDictionaryLookbehindChars = 8;
inline constexpr size_t kClosedClassProbeChars = 5;

constexpr PartOfSpeechMask partOfSpeechMask(core::PartOfSpeech pos) {
  return 1U << static_cast<uint8_t>(pos);
}

/** Whether an exact dictionary surface has any of the requested parts of speech. */
bool hasExactPartOfSpeech(const dictionary::DictionaryManager& dict_manager, std::string_view surface,
                          PartOfSpeechMask pos_mask);

/**
 * @brief The span form of hasExactPartOfSpeech
 *
 * Owns the conversion from the codepoint range to the surface, which otherwise
 * inlines the UTF-8 encode loop and the temporary's teardown into every caller.
 */
bool hasExactPartOfSpeech(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                          size_t start, size_t end, PartOfSpeechMask pos_mask);

/** Whether dictionary lookup results contain a requested POS, optionally at an exact character length. */
bool lookupResultsHavePartOfSpeech(const std::vector<dictionary::LookupResult>& results, PartOfSpeechMask pos_mask,
                                   size_t length = 0);

/** Whether dictionary lookup results contain a requested ExtendedPOS, optionally at an exact character length. */
bool lookupResultsHaveExtendedPOS(const std::vector<dictionary::LookupResult>& results, core::ExtendedPOS extended_pos,
                                  size_t length = 0);

/**
 * Largest number of registered words of one part of speech a span can be
 * segmented into, or -1 when no segmentation covers it entirely. Multi-mora
 * entries stay whole, so a span that is one such word counts as one part
 * rather than as its moras.
 *
 * @p excluded drops one category from the segmentation. Callers use it for the
 * cells the tokenizer itself admits only inside a named chain: a registered
 * surface the analyzer would never place here is not evidence about what the
 * span spells.
 */
int maximalSegmentCount(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start_pos, size_t end_pos, core::PartOfSpeech pos,
                        core::ExtendedPOS excluded = core::ExtendedPOS::Count_);

/**
 * Whether some span ending at @p end_pos, and starting at or after
 * @p scan_start, is a dictionary entry with one of the requested parts of
 * speech. Used to prove that a closed or attested word closes a boundary.
 */
bool hasDictionaryEntryEndingAt(const dictionary::DictionaryManager& dict_manager,
                                const std::vector<char32_t>& codepoints, size_t scan_start, size_t end_pos,
                                PartOfSpeechMask pos_mask);

/**
 * Whether the span splits at some interior boundary into two dictionary
 * entries, the left one matching @p left_mask and the right one @p right_mask.
 * The first admissible split is enough; no split is preferred over another.
 */
bool hasDictionarySplit(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start_pos, size_t end_pos, PartOfSpeechMask left_mask, PartOfSpeechMask right_mask);

/** Whether a complete dictionary match is a verb with the requested lemma. */
bool hasCompleteVerbLemma(const dictionary::DictionaryManager& dict_manager, std::string_view surface,
                          size_t char_length, std::string_view lemma);

/** Whether a character boundary starts inside a registered noun. */
bool startsInsideRegisteredNoun(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                const ByteOffsets& byte_offsets, size_t start_pos);

/**
 * @brief Find end position of consecutive characters of a given type
 *
 * Scans from start_pos until one of: bounds exceeded, max_len reached,
 * or character type changes.
 *
 * @param char_types Character type array
 * @param start_pos Starting position
 * @param max_len Maximum characters to scan
 * @param target_type Character type to match
 * @return End position (exclusive)
 *
 * Example:
 *   // Find up to 3 kanji starting at start_pos
 *   size_t kanji_end = findCharRegionEnd(char_types, start_pos, 3, CharType::Kanji);
 */
size_t findCharRegionEnd(const std::vector<normalize::CharType>& char_types, size_t start_pos, size_t max_len,
                         normalize::CharType target_type);

/**
 * @brief Check whether a kanji run at @p start_pos is followed by する.
 *
 * @param minimum_kanji_count Minimum length required for the kanji run.
 */
bool hasKanjiSuruPredicateAt(const std::vector<char32_t>& codepoints,
                             const std::vector<normalize::CharType>& char_types, size_t start_pos,
                             size_t minimum_kanji_count = 1);

/**
 * @brief Check whether a kanji run at @p start_pos heads a サ変 predicate
 *
 * Same construction hasKanjiSuruPredicateAt recognizes, plus the continuative
 * cell the predicate uses whenever anything follows it (確認+し+やすい,
 * 実施+し+た, 報告+し+て).  The continuative needs its own continuation to count:
 * a bare clause-final し is the conjunctive particle, and し opening a longer
 * particle (しか, しも) does not head a predicate either.
 */
bool headsKanjiSuruPredicateAt(const dictionary::DictionaryManager& dict_manager,
                               const std::vector<char32_t>& codepoints,
                               const std::vector<normalize::CharType>& char_types, size_t start_pos,
                               size_t minimum_kanji_count = 1);

/**
 * @brief Build UTF-8 byte offsets for every character boundary
 *
 * @param codepoints Vector of Unicode codepoints
 * @return Prefix offsets with codepoints.size() + 1 entries
 */
ByteOffsets buildByteOffsets(const std::vector<char32_t>& codepoints);

/**
 * @brief Look up a character boundary's UTF-8 byte offset
 *
 * Positions beyond the available boundaries safely resolve to the final byte
 * offset, matching the former scanning helper's clamping behavior.
 */
inline size_t byteOffsetAt(const ByteOffsets& byte_offsets, size_t char_pos) {
  return byte_offsets.empty() ? 0 : byte_offsets[std::min(char_pos, byte_offsets.size() - 1)];
}

/**
 * @brief Return the UTF-8 text covered by a character range.
 *
 * Invalid, empty, or out-of-bounds ranges resolve to an empty view.
 */
std::string_view textRange(std::string_view text, const ByteOffsets& byte_offsets, size_t start, size_t end);

/**
 * @brief Advance a character position until its byte offset reaches a target
 *
 * Starting from @p start_char (whose UTF-8 byte offset is @p start_byte), walk
 * forward one codepoint at a time, accumulating each codepoint's UTF-8 byte
 * length, and stop as soon as the accumulated byte offset is no longer below
 * @p target_byte or the codepoints are exhausted.
 *
 * @param codepoints Vector of Unicode codepoints
 * @param start_char Character position to start from (0-indexed)
 * @param start_byte Byte offset corresponding to @p start_char
 * @param target_byte Byte offset to advance up to
 * @return Character position whose byte offset reaches @p target_byte
 */
size_t advanceCharsToBytePos(const std::vector<char32_t>& codepoints, size_t start_char, size_t start_byte,
                             size_t target_byte);

/**
 * @brief Encode a codepoint range as UTF-8.
 */
std::string extractSubstring(const std::vector<char32_t>& codepoints, size_t start, size_t end);

/** Encode only the bounded closed-class lookahead starting at @p start. */
std::string extractClosedClassProbe(const std::vector<char32_t>& codepoints, size_t start);

/**
 * @brief Inflection analyses of the surface spelled by codepoints[start, end)
 *
 * The single owner of the span-to-surface conversion the inflection probes
 * share. Encoding the span at the call site inlines the UTF-8 encode loop and
 * the temporary's teardown into every caller, and the surface itself is only
 * ever viewed: the analysis cache keeps its own copy of whatever it retains,
 * so the returned reference outlives the temporary.
 */
const std::vector<grammar::InflectionCandidate>& analysesInRange(const grammar::Inflection& inflection,
                                                                 const std::vector<char32_t>& codepoints, size_t start,
                                                                 size_t end);

/** Whether a position begins a case/topic/nominalizer particle sequence. */
bool startsNominalForcingParticle(const std::vector<char32_t>& codepoints, size_t pos);

/** Whether a particle category can turn a preceding continuative into a nominal head. */
bool isNominalForcingParticle(core::ExtendedPOS extended_pos);

/** Whether dictionary evidence at a position starts a nominal-forcing particle. */
bool hasNominalForcingParticleContinuation(const std::vector<char32_t>& codepoints, size_t pos,
                                           const dictionary::DictionaryManager* dict_manager);

/** Whether a position begins a multi-character non-particle dictionary entry. */
bool startsLongerNonParticleEntry(const std::vector<char32_t>& codepoints, size_t start_pos,
                                  const dictionary::DictionaryManager* dict_manager);

/**
 * Find the earliest start of the longest productive verb continuative ending
 * one or two hiragana morae after a kanji run.
 */
size_t longestNominalVerbContinuativeStart(const std::vector<char32_t>& codepoints,
                                           const std::vector<normalize::CharType>& char_types, size_t kanji_start,
                                           size_t kanji_end, const grammar::Inflection& inflection,
                                           const dictionary::DictionaryManager* dict_manager);

/** Whether an edge with one of the requested parts of speech ends at a boundary. */
bool hasPrecedingPartOfSpeech(const core::Lattice& lattice, size_t end_pos, PartOfSpeechMask pos_mask);

/** Whether an edge with the requested extended part of speech ends at a boundary. */
bool hasPrecedingExtendedPOS(const core::Lattice& lattice, size_t end_pos, core::ExtendedPOS extended_pos);

/**
 * @brief Find the end of a dictionary-evidenced compound verb covering pos.
 *
 * Only the explicit LemmaVerified flag counts. Compound candidates also use
 * FromDictionary for their component evidence, which is not sufficient to
 * establish the complete lexical compound.
 */
size_t verifiedCompoundEndCovering(const core::Lattice& lattice, size_t pos);

/** Find a dictionary-derived verb onbin form spanning an interior boundary. */
size_t dictionarySokuonbinEndCovering(const core::Lattice& lattice, size_t pos);

/**
 * @brief Find the end of any structurally generated compound verb covering pos.
 */
size_t compoundVerbEndCovering(const core::Lattice& lattice, size_t pos);

/**
 * @brief Whether a candidate joins a licensed particle to an adverb prefix.
 *
 * A particle is treated as grammatical only when a content/predicate edge
 * ends at the candidate start. This prevents homographic kana inside open
 * adjectives and nouns from claiming a particle boundary.
 */
bool joinsParticleToDictionaryAdverb(const core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                     std::string_view text, const ByteOffsets& byte_offsets, size_t candidate_start,
                                     size_t candidate_end, core::ExtendedPOS candidate_extended_pos);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_TOKENIZER_UTILS_H_
