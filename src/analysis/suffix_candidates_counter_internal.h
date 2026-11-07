#ifndef SUZUME_ANALYSIS_SUFFIX_CANDIDATES_COUNTER_INTERNAL_H_
#define SUZUME_ANALYSIS_SUFFIX_CANDIDATES_COUNTER_INTERNAL_H_

#include <cstddef>
#include <vector>

#include "dictionary/dictionary.h"
#include "normalize/char_type.h"
#include "unknown.h"

namespace suzume::analysis::counter_detail {

/**
 * @brief Append a Counter-origin candidate over [start, end) lemmatized to its own surface
 *
 * An empty span appends nothing.
 */
void appendCounterCandidate(const std::vector<char32_t>& codepoints, size_t start, size_t end, core::PartOfSpeech pos,
                            float cost, core::ExtendedPOS extended_pos, const char* pattern,
                            std::vector<UnknownCandidate>& candidates);

/**
 * @brief End of the quantity head at start: an optional quantity prefix kanji, then numerals
 *
 * Returns start when no quantity char is present.
 */
size_t scanQuantityHead(const std::vector<char32_t>& codepoints, size_t start, bool allow_prefix);

/**
 * @brief Whether [start, end) is a digit-written number scaled by magnitude kanji (3千, 2万3千, 50億)
 *
 * Digit-led, made only of Arabic digits and magnitude kanji, with at least one
 * magnitude. Such a head behaves like a digit numeral, not a kanji one.
 */
bool isDigitMagnitudeHead(const std::vector<char32_t>& codepoints, size_t start, size_t end);

/**
 * @brief End of a linker + counter kanji pair at pos (3+か月, 7+カ国), or pos when there is none
 *
 * @param counter Which counter kanji the linker may take
 */
size_t linkedCounterEnd(const std::vector<char32_t>& codepoints, size_t pos, bool (*counter)(char32_t));

/**
 * @brief Numeral + linker + counter kanji (7か国, 三ヶ所), with the parity rule for a kanji run after it
 */
void appendLinkedCounterCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t numeral_end,
                                  const dictionary::DictionaryManager* dict_manager,
                                  std::vector<UnknownCandidate>& candidates);

void appendTemporalCounterCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const std::vector<normalize::CharType>& char_types,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates);
void appendStructuralCounterCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                       const std::vector<normalize::CharType>& char_types,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates);
void appendBasicNumeralCounterCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t numeral_end,
                                         const std::vector<normalize::CharType>& char_types,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates);
// Whether the nominal kanji run after a counter has odd length, so a cut at the
// counter would land inside a two-kanji word; a run before possible okurigana
// is not decided here.
bool leavesUnevenNominalRun(const std::vector<char32_t>& codepoints, size_t counter_end);
void appendUnevenKanjiRunCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t numeral_end,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates);

}  // namespace suzume::analysis::counter_detail

#endif  // SUZUME_ANALYSIS_SUFFIX_CANDIDATES_COUNTER_INTERNAL_H_
