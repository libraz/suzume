#ifndef SUZUME_ANALYSIS_TOKENIZER_H_
#define SUZUME_ANALYSIS_TOKENIZER_H_

#include <string_view>
#include <vector>

#include "analysis/scorer.h"
#include "analysis/tokenizer_utils.h"
#include "analysis/unknown.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

/**
 * @brief Tokenizer that builds lattice from text
 */
class Tokenizer {
 public:
  Tokenizer(const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
            const UnknownWordGenerator& unknown_gen, core::AnalysisMode mode = core::AnalysisMode::Normal);

  /**
   * @brief Build lattice from text
   * @param text Normalized text
   * @param codepoints Codepoints of text
   * @param char_types Character types
   * @return Lattice with all candidates
   */
  core::Lattice buildLattice(std::string_view text, const std::vector<char32_t>& codepoints,
                             const std::vector<normalize::CharType>& char_types) const;

 private:
  const dictionary::DictionaryManager& dict_manager_;
  const Scorer& scorer_;
  const UnknownWordGenerator& unknown_gen_;
  const grammar::Inflection& inflection_;  // Shared from unknown_gen_
  core::AnalysisMode mode_;

  /**
   * @brief Add dictionary candidates at position
   */
  void addDictionaryCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                               const ByteOffsets& byte_offsets, size_t start_pos,
                               std::vector<dictionary::LookupResult>& lookup_results) const;

  /**
   * @brief Add unknown word candidates at position
   */
  void addUnknownCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                            const ByteOffsets& byte_offsets, size_t start_pos,
                            const std::vector<normalize::CharType>& char_types,
                            const std::vector<dictionary::LookupResult>& dict_results) const;

  /**
   * @brief Drop shape-derived lexical bonuses inside user dictionary spans
   *
   * A user dictionary entry states that a span is one token, and it is the
   * most specific evidence the analyser has. Shape-derived unknown-word
   * candidates carry bonuses stronger than any dictionary edge — an exact
   * reduplication scores as a mimetic adverb on shape alone — so a guess
   * confined to the registered span can outscore the registration, which has
   * no cost of its own to tune. Inside a registered span those candidates fall
   * back to their category cost: they stay selectable on grammatical grounds
   * but no longer override the user. Lexically grounded candidates (counters,
   * conjugation paradigms, dictionary-backed joins and splits) are untouched,
   * so a competing reading assembled from real entries still wins on merit.
   */
  static void clampHeuristicBonusesInUserDictSpans(core::Lattice& lattice);
};

}  // namespace suzume::analysis
#endif  // SUZUME_ANALYSIS_TOKENIZER_H_
