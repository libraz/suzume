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
   * @brief Add compound verb join candidates
   *
   * Detects V1連用形 + V2 patterns and generates compound verb candidates.
   * V1 = base verb in continuative form (連用形)
   * V2 = subsidiary verb (出す, 込む, 始める, etc.)
   *
   * Examples:
   *   "飛び込む" → compound verb (飛ぶ + 込む)
   *   "読み込む" → compound verb (読む + 込む)
   *   "書き出す" → compound verb (書く + 出す)
   */
  void addCompoundVerbJoinCandidates(
      core::Lattice& lattice, std::string_view text,
      const std::vector<char32_t>& codepoints, size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Add hiragana compound verb join candidates
   *
   * Detects all-hiragana V1連用形 + V2 patterns where V1 is a known dictionary verb.
   *
   * Examples:
   *   "やりなおす" → compound verb (やる + なおす)
   *   "やりなおしたい" → やりなおし + たい
   */
  void addHiraganaCompoundVerbJoinCandidates(
      core::Lattice& lattice, std::string_view text,
      const std::vector<char32_t>& codepoints, size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Add prefix + noun join candidates
   *
   * Detects productive prefix + noun patterns and generates merged candidates.
   * Prefixes include: お/ご (honorific), 不/未/非/無 (negation), 超/再/準, etc.
   *
   * Examples:
   *   "お水" → merged as single noun (お + 水)
   *   "ご確認" → merged as single noun (ご + 確認)
   *   "不安" → merged as single noun (不 + 安)
   *   "未経験" → merged as single noun (未 + 経験)
   */
  void addPrefixNounJoinCandidates(
      core::Lattice& lattice, std::string_view text,
      const std::vector<char32_t>& codepoints, size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Add te-form + auxiliary verb split candidates
   *
   * Detects patterns where a verb in te-form is followed by auxiliary verbs
   * like いく, くる, みる, おく, しまう and generates split candidates.
   *
   * Examples:
   *   "学んでいきたい" → ["学んで" + "いきたい"]
   *   "食べてみる" → ["食べて" + "みる"]
   *   "書いておく" → ["書いて" + "おく"]
   */
  void addTeFormAuxiliaryCandidates(
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
