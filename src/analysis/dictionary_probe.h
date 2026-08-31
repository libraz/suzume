/**
 * @file dictionary_probe.h
 * @brief Bounded dictionary lookups over a codepoint window
 */

#ifndef SUZUME_ANALYSIS_DICTIONARY_PROBE_H_
#define SUZUME_ANALYSIS_DICTIONARY_PROBE_H_

#include <cstddef>
#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume::analysis {

/**
 * @brief Selects which dictionary entry a probe takes; nullptr takes any entry
 *
 * A plain function pointer rather than a template parameter: the scan body
 * allocates a span string per step, so one instantiation per predicate put a
 * copy of that loop in every caller.
 */
using EntryAccept = bool (*)(const dictionary::DictionaryEntry&);

/**
 * @brief Length of the accepted dictionary entry spanning @p start
 *
 * Scans the spans [start, start + len] for len in [min_len, max_len], and
 * returns the first length whose entry @p accept takes. @p max_len is clamped
 * to the end of @p codepoints, so callers state the closed class's longest
 * member rather than repeating the bounds arithmetic.
 *
 * @param pos Restricts the lookup to one part of speech; Unknown accepts any
 * @param accept Called with each candidate entry; the first it takes wins
 * @param longest_first Scans longest span first instead of shortest first
 * @return the accepted length, or 0 when no span in range has one
 */
size_t acceptedDictionaryEntryLength(const dictionary::DictionaryManager* dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t start, size_t min_len,
                                     size_t max_len, core::PartOfSpeech pos, EntryAccept accept, bool longest_first);

/** @brief Whether any dictionary entry spanning @p start is accepted */
inline bool hasDictionaryEntryFrom(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t start, size_t min_len,
                                   size_t max_len, core::PartOfSpeech pos, EntryAccept accept) {
  return acceptedDictionaryEntryLength(dict_manager, codepoints, start, min_len, max_len, pos, accept, false) != 0;
}

/**
 * @brief Length of the longest accepted dictionary entry spanning @p start
 *
 * The mirror of hasDictionaryEntryFrom for the callers that need the longest
 * match rather than the shortest, and the span length rather than the verdict.
 *
 * @return the accepted length, or 0 when no span in range has one
 */
inline size_t longestDictionaryEntryLengthFrom(const dictionary::DictionaryManager* dict_manager,
                                               const std::vector<char32_t>& codepoints, size_t start, size_t min_len,
                                               size_t max_len, core::PartOfSpeech pos, EntryAccept accept) {
  return acceptedDictionaryEntryLength(dict_manager, codepoints, start, min_len, max_len, pos, accept, true);
}

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_DICTIONARY_PROBE_H_
