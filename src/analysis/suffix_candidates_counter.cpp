/**
 * @file suffix_candidates_counter.cpp
 * @brief Suffix-based unknown word candidate generation
 */

#include <algorithm>
#include <utility>

#include "candidate_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "suffix_candidates_counter_internal.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis {

namespace counter_detail {

void appendCounterCandidate(const std::vector<char32_t>& codepoints, size_t start, size_t end, core::PartOfSpeech pos,
                            float cost, core::ExtendedPOS extended_pos, [[maybe_unused]] const char* pattern,
                            std::vector<UnknownCandidate>& candidates) {
  std::string surface = extractSubstring(codepoints, start, end);
  if (surface.empty()) {
    return;
  }
  auto cand = makeCandidate(surface, start, end, pos, cost, false, CandidateOrigin::Counter, extended_pos);
  cand.lemma = std::move(surface);
  SUZUME_DEBUG_CANDIDATE_PATTERN(cand, pattern);
  candidates.push_back(std::move(cand));
}

size_t scanQuantityHead(const std::vector<char32_t>& codepoints, size_t start, bool allow_prefix) {
  size_t scan = start;
  if (allow_prefix && scan < codepoints.size() && normalize::isQuantityPrefixKanji(codepoints[scan])) {
    ++scan;
  }
  const size_t head_start = scan;
  // 億/兆 are no numerals alone (兆し), but after one they continue it (1兆2千億).
  while (scan < codepoints.size() && (normalize::isNumeralCodepoint(codepoints[scan]) ||
                                      (scan > head_start && normalize::isMagnitudeKanji(codepoints[scan])))) {
    ++scan;
  }
  return scan;
}

bool isDigitMagnitudeHead(const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  if (start >= end || end > codepoints.size() || !normalize::isArabicDigitCodepoint(codepoints[start])) {
    return false;
  }
  bool has_magnitude = false;
  for (size_t idx = start; idx < end; ++idx) {
    const bool magnitude = normalize::isMagnitudeKanji(codepoints[idx]);
    if (!magnitude && !normalize::isArabicDigitCodepoint(codepoints[idx])) {
      return false;
    }
    has_magnitude = has_magnitude || magnitude;
  }
  return has_magnitude;
}

size_t linkedCounterEnd(const std::vector<char32_t>& codepoints, size_t pos, bool (*counter)(char32_t)) {
  if (pos + 1 < codepoints.size() && normalize::isCounterLinker(codepoints[pos]) && counter(codepoints[pos + 1])) {
    return pos + 2;
  }
  return pos;
}

// A linked counter followed by an odd kanji run lands inside a two-kanji word
// unless the run is taken whole (5カ国+語), as for any other counter; an even
// run is a sequence of words after it (7か国|首脳会議).
void appendLinkedCounterCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t numeral_end,
                                  const dictionary::DictionaryManager* dict_manager,
                                  std::vector<UnknownCandidate>& candidates) {
  const size_t counter_end = linkedCounterEnd(codepoints, numeral_end, normalize::isLinkedCounterKanji);
  if (counter_end == numeral_end) {
    return;
  }
  const size_t run_end = grammar::nominalKanjiRunEnd(codepoints, counter_end);
  if (run_end > counter_end && !verb_helpers::opensWordAfterQuantity(dict_manager, codepoints, counter_end) &&
      !verb_helpers::kanjiRunMayContinueAt(dict_manager, codepoints, run_end) &&
      grammar::countKanjiRunWords(codepoints, counter_end, run_end) % 2 == 1) {
    appendCounterCandidate(codepoints, start_pos, run_end, core::PartOfSpeech::Noun,
                           candidate::kCounterRelationSplitBonus, core::ExtendedPOS::NounNumber,
                           "numeral_linked_counter_uneven_run", candidates);
    return;
  }
  appendCounterCandidate(codepoints, start_pos, counter_end, core::PartOfSpeech::Noun,
                         candidate::kCounterRelationSplitBonus, core::ExtendedPOS::NounNumber, "numeral_linked_counter",
                         candidates);
}

bool leavesUnevenNominalRun(const std::vector<char32_t>& codepoints, size_t counter_end) {
  const size_t run_end = grammar::nominalKanjiRunEnd(codepoints, counter_end);
  return !grammar::mayBeOkuriganaAt(codepoints, run_end) && !grammar::kanjiRunCutByNumeralAt(codepoints, run_end) &&
         grammar::countKanjiRunWords(codepoints, counter_end, run_end) % 2 == 1;
}

// A kanji-numeral quantity whose counter cut leaves an odd kanji run lands
// inside a two-kanji word (二時間+弱, 三割+強, 一日+付), so the whole run is
// the search unit, as for a digit quantity in split_candidates. Relational
// 前/後 and a listed pronoun open a word of their own (三日|後), and a run
// ending before possible okurigana is left to the predicate (三枚|重ねる).
void appendUnevenKanjiRunCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t numeral_end,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates) {
  size_t counter_end = verb_helpers::counterKanjiRunEnd(dict_manager, codepoints, numeral_end, codepoints.size());
  // 間 heading an interval word belongs to the run (三年|間隔).
  if (counter_end > numeral_end + 1 && counter_end < codepoints.size() && codepoints[counter_end - 1] == U'間' &&
      normalize::isIntervalCompoundSecondKanji(codepoints[counter_end])) {
    --counter_end;
  }
  const size_t run_end = grammar::nominalKanjiRunEnd(codepoints, counter_end);
  if (counter_end == numeral_end || run_end == counter_end ||
      verb_helpers::opensWordAfterQuantity(dict_manager, codepoints, counter_end)) {
    return;
  }
  if (verb_helpers::isQuantityClosingSuffixAt(dict_manager, codepoints, counter_end)) {
    appendCounterCandidate(codepoints, start_pos, counter_end + 1, core::PartOfSpeech::Noun,
                           candidate::kCounterRelationSplitBonus, core::ExtendedPOS::NounNumber,
                           "numeral_closing_suffix", candidates);
    return;
  }
  // A run cut by the next numeral has no parity to read (一日|約+３万人); the
  // counter keeps its own boundary.
  if (grammar::kanjiRunCutByNumeralAt(codepoints, run_end)) {
    appendCounterCandidate(codepoints, start_pos, counter_end, core::PartOfSpeech::Noun,
                           candidate::kCounterNounSplitBonus, core::ExtendedPOS::NounNumber,
                           "numeral_counter_before_numeral_run", candidates);
    return;
  }
  if (verb_helpers::kanjiRunMayContinueAt(dict_manager, codepoints, run_end)) {
    return;
  }
  if (grammar::countKanjiRunWords(codepoints, counter_end, run_end) % 2 == 1) {
    appendCounterCandidate(codepoints, start_pos, run_end, core::PartOfSpeech::Noun,
                           candidate::kCounterRelationSplitBonus, core::ExtendedPOS::NounNumber,
                           "numeral_uneven_kanji_run", candidates);
  } else {
    // An even remainder is a sequence of words after the counter (三段階|評価).
    appendCounterCandidate(codepoints, start_pos, counter_end, core::PartOfSpeech::Noun,
                           candidate::kCounterNounSplitBonus, core::ExtendedPOS::NounNumber,
                           "numeral_even_kanji_run_split", candidates);
  }
}

}  // namespace counter_detail

void generateCounterCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                               const std::vector<normalize::CharType>& char_types,
                               const dictionary::DictionaryManager* dict_manager,
                               std::vector<UnknownCandidate>& candidates) {
  // Need at least 2 characters (numeral + counter suffix)
  if (start_pos + 1 >= codepoints.size()) {
    return;
  }

  counter_detail::appendStructuralCounterCandidates(codepoints, start_pos, char_types, dict_manager, candidates);

  // First character(s) must be numeral(s)
  if (!normalize::isNumeralCodepoint(codepoints[start_pos])) {
    return;
  }

  // Find the end of the numeral sequence
  const size_t numeral_end = counter_detail::scanQuantityHead(codepoints, start_pos, false);

  // A digit number scaled by magnitudes is a search unit before a non-kanji
  // (1万2千+を); a kanji or katakana after it is a counter, handled below.
  if (counter_detail::isDigitMagnitudeHead(codepoints, start_pos, numeral_end) &&
      (numeral_end >= codepoints.size() || (char_types[numeral_end] != normalize::CharType::Kanji &&
                                            char_types[numeral_end] != normalize::CharType::Katakana))) {
    counter_detail::appendCounterCandidate(codepoints, start_pos, numeral_end, core::PartOfSpeech::Noun,
                                           candidate::kNumeralCounterMergeBonus, core::ExtendedPOS::NounNumber,
                                           "digit_magnitude_number", candidates);
  }

  // Must have at least one character after numerals
  if (numeral_end >= codepoints.size()) {
    return;
  }

  counter_detail::appendBasicNumeralCounterCandidates(codepoints, start_pos, numeral_end, char_types, dict_manager,
                                                      candidates);
  counter_detail::appendLinkedCounterCandidate(codepoints, start_pos, numeral_end, dict_manager, candidates);

  // A number + katakana unit merges only when the numeral is written in (half- or
  // full-width) digits: 3キロ, 100ドル, ５センチ are one quantity token. A kanji
  // numeral before katakana (五センチ, 十キロメートル) is split at the natural
  // kanji→katakana boundary (五|センチ), matching MeCab, so it must not merge here.
  const bool numeral_is_digits =
      std::all_of(char_types.begin() + static_cast<std::ptrdiff_t>(start_pos),
                  char_types.begin() + static_cast<std::ptrdiff_t>(numeral_end),
                  [](normalize::CharType type) { return type == normalize::CharType::Digit; });
  if (!numeral_is_digits) {
    counter_detail::appendUnevenKanjiRunCandidate(codepoints, start_pos, numeral_end, dict_manager, candidates);
  }

  // A digit run glued to a preceding letter is part of an alphanumeric
  // identifier, not a quantity: the A of A4 owns the 4, so the following
  // katakana noun starts its own token (A4|サイズ, not A|4サイズ).
  const bool follows_letter = start_pos > 0 && char_types[start_pos - 1] == normalize::CharType::Alphabet;

  // Check for katakana unit suffix (e.g., キロ, ドル, メートル, パーセント)
  // Generate digit + katakana unit candidates like 3キロ, 100ドル, 80パーセント.
  // Digits scaled by magnitude kanji are still digit-written (50億ドル, 3千ドル).
  const bool digit_written =
      numeral_is_digits || counter_detail::isDigitMagnitudeHead(codepoints, start_pos, numeral_end);
  if (follows_letter || char_types[numeral_end] != normalize::CharType::Katakana) {
    return;
  }
  // Find end of katakana sequence (max 8 chars for reasonable unit length)
  const size_t unit_end = findCharRegionEnd(char_types, numeral_end, 8, normalize::CharType::Katakana);
  // A linker before a kanji counter (ヶ月, カ国) is not a unit of its own; the
  // linked-counter candidate above owns it.
  if (unit_end == numeral_end + 1 && unit_end < codepoints.size() &&
      normalize::isCounterLinker(codepoints[numeral_end]) && char_types[unit_end] == normalize::CharType::Kanji) {
    return;
  }
  // Any all-katakana run merges with the preceding numeral (3キロ, 100メダル);
  // MeCab treats number + katakana as one quantity token, so there is no
  // curated unit list.
  if (!normalize::isAllKatakana(extractSubstring(codepoints, numeral_end, unit_end))) {
    return;
  }

  // A kanji numeral before a katakana unit stays split, but an ordinal 目
  // closing that quantity is a suffix rather than a lexical noun. Require 目
  // to end before a non-kanji boundary so compounds such as 目標 and 目次 keep
  // their ordinary noun candidate. Digit-written quantities already have a
  // complete merged candidate below, so they do not need this competing edge.
  if (!digit_written) {
    if (unit_end < codepoints.size() && codepoints[unit_end] == U'目' &&
        (unit_end + 1 == codepoints.size() || char_types[unit_end + 1] != normalize::CharType::Kanji)) {
      counter_detail::appendCounterCandidate(codepoints, unit_end, unit_end + 1, core::PartOfSpeech::Suffix,
                                             candidate::kOrdinalDigitCounterSplitBonus, core::ExtendedPOS::Unknown,
                                             "kanji_numeral_katakana_ordinal_suffix", candidates);
    }
    return;
  }

  // Penalize numbers starting with 0 (e.g., "00ポイント" is unnatural)
  // "0ドル" is fine, but "00ドル", "000キロ" are not typical Japanese patterns
  const bool starts_with_zero_prefix = numeral_end - start_pos >= 2 && codepoints[start_pos] == U'0';
  // Give bonus to prefer combined token over split
  // Longer units get slightly more bonus (キロ, ドル vs キログラム, パーセント)
  // Strong bonus (-0.5) to beat optimal_length bonuses on split candidates
  const size_t unit_len = unit_end - numeral_end;
  float cost = starts_with_zero_prefix ? 2.0F  // Penalize unnatural zero-prefix numbers
                                       : -0.5F - (static_cast<float>(unit_len) * 0.05F);
  auto cand =
      makeCandidate(codepoints, start_pos, unit_end, core::PartOfSpeech::Noun, cost, false, CandidateOrigin::Counter);
  SUZUME_DEBUG_CANDIDATE(cand, starts_with_zero_prefix ? 0.3F : 0.9F, "numeric_unit_katakana");
  candidates.push_back(cand);
}

}  // namespace suzume::analysis
