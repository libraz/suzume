#ifndef SUZUME_ANALYSIS_TOKENIZER_H_
#define SUZUME_ANALYSIS_TOKENIZER_H_

#include <string_view>
#include <vector>

#include "analysis/scorer.h"
#include "analysis/unknown.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

/**
 * @brief Tokenizer that builds lattice from text
 */
class Tokenizer {
 public:
  Tokenizer(const dictionary::DictionaryManager& dict_manager,
            const Scorer& scorer, const UnknownWordGenerator& unknown_gen);

  /**
   * @brief Build lattice from text
   * @param text Normalized text
   * @param codepoints Codepoints of text
   * @param char_types Character types
   * @return Lattice with all candidates
   */
  core::Lattice buildLattice(
      std::string_view text, const std::vector<char32_t>& codepoints,
      const std::vector<normalize::CharType>& char_types) const;

 private:
  const dictionary::DictionaryManager& dict_manager_;
  const Scorer& scorer_;
  const UnknownWordGenerator& unknown_gen_;

  /**
   * @brief Add dictionary candidates at position
   */
  void addDictionaryCandidates(core::Lattice& lattice, std::string_view text,
                               const std::vector<char32_t>& codepoints,
                               size_t start_pos) const;

  /**
   * @brief Add unknown word candidates at position
   */
  void addUnknownCandidates(
      core::Lattice& lattice, std::string_view text,
      const std::vector<char32_t>& codepoints, size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Add mixed script joining candidates
   *
   * Detects transitions between scripts (Alphabet+Kanji, Alphabet+Katakana,
   * Digit+Kanji) and generates merged candidates with a cost bonus.
   *
   * Examples:
   *   "Web開発" → merged as single noun with bonus
   *   "APIリクエスト" → merged as single noun with bonus
   *   "3月" → merged as single noun with bonus
   */
  void addMixedScriptCandidates(
      core::Lattice& lattice, std::string_view text,
      const std::vector<char32_t>& codepoints, size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Add compound noun split candidates
   *
   * For kanji sequences of 4+ characters, generates split candidates
   * using dictionary boundary hints.
   *
   * Examples:
   *   "人工知能" → ["人工知能", "人工" + "知能"]
   *   "人工知能研究所" → ["人工知能" + "研究所", ...]
   */
  void addCompoundSplitCandidates(
      core::Lattice& lattice, std::string_view text,
      const std::vector<char32_t>& codepoints, size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Add noun+verb split candidates at kanji boundaries
   *
   * Detects patterns where a kanji (potential noun) is followed by
   * kanji + hiragana (potential verb) and generates split candidates.
   *
   * Examples:
   *   "本買った" → ["本" + "買った"] (noun + verb)
   *   "日本語話す" → ["日本語" + "話す"] (noun + verb)
   */
  void addNounVerbSplitCandidates(
      core::Lattice& lattice, std::string_view text,
      const std::vector<char32_t>& codepoints, size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Convert character position to byte position
   */
  static size_t charPosToBytePos(const std::vector<char32_t>& codepoints, size_t char_pos);
};

}  // namespace suzume::analysis
#endif  // SUZUME_ANALYSIS_TOKENIZER_H_
