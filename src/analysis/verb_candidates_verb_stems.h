/**
 * @file verb_candidates_verb_stems.h
 * @brief Verb class and stem shape helpers for verb candidate generation
 *
 * Single-kanji Ichidan stems, terminal-form suffixes per verb type, godan
 * onbin rows matched against the dictionary, and the best inflection reading
 * per verb class.
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_VERB_STEMS_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_VERB_STEMS_H_

#include <string>
#include <string_view>
#include <vector>

#include "dictionary/dictionary.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"

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

/** Return true for a one-kanji stem that takes the polite auxiliary directly. */
bool isSingleKanjiPoliteStem(char32_t c);

/**
 * @brief Check if a surface form is exactly one single-kanji Ichidan verb
 *
 * True when the surface consists of exactly one codepoint and that codepoint
 * is a known single-kanji Ichidan verb (see isSingleKanjiIchidan).
 */
bool isSingleKanjiIchidanSurface(std::string_view surface);

/**
 * @brief Whether codepoints[start, end) is a verb continuative
 *
 * A dictionary VerbRenyokei cell (読み, 語り) or a single-kanji Ichidan stem
 * (見, 着, 寝), whose continuative is the bare kanji.
 */
bool isVerbContinuativeSpan(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                            size_t start, size_t end);

/** Whether some proper prefix of codepoints[start, end) is a verb continuative. */
bool startsWithVerbContinuative(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start, size_t end);

// =============================================================================
// Verb Type / Stem Analysis Helpers
// =============================================================================

/**
 * @brief Get the terminal-form (終止形) okurigana suffix for a verb type
 *
 * Returns the dictionary-form ending: Ichidan yields "る", Godan types yield
 * their base vowel (GodanKa -> "く", GodanSa -> "す", ...). Returns an empty
 * string for verb types without a Godan terminal ending (Suru, Kuru,
 * IAdjective, Unknown).
 */
std::string baseFormSuffix(grammar::VerbType verb_type);

/**
 * @brief Check whether a stem is a valid i-row Ichidan verb stem
 *
 * A valid i-row Ichidan stem ends in an i-row hiragana, has at least two
 * characters, and is not the single-kanji + い pattern (人い -> 人 + いる),
 * which is almost always NOUN + いる rather than an Ichidan verb.
 */
bool isValidIRowIchidanStem(std::string_view stem);

/**
 * @brief Get Godan VerbTypes that use a specific onbin pattern
 *
 * Onbin patterns:
 * - "い" (ikuon) → GodanKa, GodanGa
 * - "っ" (sokuon) → GodanKa (行く irregular), GodanRa, GodanTa, GodanWa
 * - "ん" (hatsuonbin) → GodanNa, GodanBa, GodanMa
 * - "" (none) → GodanSa
 *
 * @param onbin Onbin pattern to match ("い", "っ", "ん", or "")
 * @return Reference to a shared immutable table of (VerbType, base_suffix) pairs
 */
grammar::GodanOnbinRange getGodanTypesByOnbin(std::string_view onbin);

/**
 * @brief Result of matching an onbin stem against the dictionary's godan verbs.
 *
 * @c base_suffix points into the immutable getGodanTypesByOnbin() table and is
 * valid for the program's lifetime. When @c matched is false, @c verb_type is
 * Unknown, @c base_form is empty, and @c base_suffix is empty.
 */
struct GodanOnbinDictMatch {
  grammar::VerbType verb_type = grammar::VerbType::Unknown;
  std::string base_form;         // stem + base_suffix
  std::string_view base_suffix;  // the matched suffix from the table
  bool matched = false;
};

/**
 * @brief First (verb_type, stem+base_suffix) pair for @p onbin whose base form
 *        is a dictionary verb, in getGodanTypesByOnbin() table order.
 *
 * Reproduces the phase-1 "check every godan candidate, keep the first dictionary
 * hit" scan shared by the onbin candidate generators.
 *
 * @param dict_manager Dictionary manager (may be null → no match)
 * @param stem         Verb stem to which each table suffix is appended
 * @param onbin        Onbin pattern ("い", "っ", "ん", or "")
 * @return The first dictionary-verified match, or an unmatched result
 */
GodanOnbinDictMatch firstGodanOnbinDictBase(const dictionary::DictionaryManager* dict_manager, std::string_view stem,
                                            std::string_view onbin);

/**
 * @brief Best inflection candidate per verb class (Ichidan / Suru / Godan)
 *
 * Members left unmatched keep confidence 0.0 and are otherwise
 * default-constructed.
 */
struct VerbClassBests {
  grammar::InflectionCandidate ichidan;
  grammar::InflectionCandidate suru;
  grammar::InflectionCandidate godan;
};

/**
 * @brief Scan inflection candidates for the best Ichidan, Suru, and Godan entries
 *
 * Candidates matched via のだ/んだ stripping (has_explanatory_suffix) are
 * ignored. Ties keep the earlier candidate (strict > comparison).
 */
VerbClassBests bestByVerbClass(const std::vector<grammar::InflectionCandidate>& candidates);

}  // namespace suzume::analysis::verb_helpers

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_VERB_STEMS_H_
