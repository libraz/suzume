#ifndef SUZUME_ANALYSIS_UNKNOWN_H_
#define SUZUME_ANALYSIS_UNKNOWN_H_

#include <string_view>
#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

/**
 * @brief Unknown word generation options
 */
struct UnknownOptions {
  size_t max_kanji_length = 8;
  size_t max_katakana_length = 16;
  size_t max_alphabet_length = 20;
  size_t max_alphanumeric_length = 24;
  size_t max_hiragana_length = 6;
  size_t max_unknown_length = 12;

  // Suffix separation
  bool separate_suffix = true;
  float suffix_separation_bonus = -0.3F;
};

/**
 * @brief Unknown word candidate
 */
struct UnknownCandidate {
  std::string surface;  // Store as string, not string_view
  size_t start{0};
  size_t end{0};
  core::PartOfSpeech pos{core::PartOfSpeech::Noun};
  float cost{0.0F};
  bool has_suffix{false};
};

/**
 * @brief Unknown word generator
 *
 * Generates candidates for words not in dictionary
 * based on character type sequences.
 */
class UnknownWordGenerator {
 public:
  explicit UnknownWordGenerator(
      const UnknownOptions& options = {},
      const dictionary::DictionaryManager* dict_manager = nullptr);
  ~UnknownWordGenerator() = default;

  // Non-copyable, non-movable
  UnknownWordGenerator(const UnknownWordGenerator&) = delete;
  UnknownWordGenerator& operator=(const UnknownWordGenerator&) = delete;
  UnknownWordGenerator(UnknownWordGenerator&&) = delete;
  UnknownWordGenerator& operator=(UnknownWordGenerator&&) = delete;

  /**
   * @brief Generate unknown word candidates
   * @param text Text
   * @param codepoints Codepoints of text
   * @param start_pos Start position (character index)
   * @param char_types Character types
   * @return Vector of candidates
   */
  std::vector<UnknownCandidate> generate(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

 private:
  UnknownOptions options_;
  const dictionary::DictionaryManager* dict_manager_;
  grammar::Inflection inflection_;

  /**
   * @brief Generate compound verb candidates (e.g., 恐れ入ります, 差し上げます)
   *
   * Detects patterns like Kanji+Hiragana+Kanji+Hiragana and checks
   * if the base form exists in dictionary.
   */
  std::vector<UnknownCandidate> generateCompoundVerbCandidates(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate verb candidates (kanji + conjugation endings)
   */
  std::vector<UnknownCandidate> generateVerbCandidates(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate hiragana verb candidates (pure hiragana verbs like いく, くる)
   */
  std::vector<UnknownCandidate> generateHiraganaVerbCandidates(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate i-adjective candidates (kanji + conjugation endings)
   */
  std::vector<UnknownCandidate> generateAdjectiveCandidates(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate hiragana i-adjective candidates (pure hiragana like まずい)
   */
  std::vector<UnknownCandidate> generateHiraganaAdjectiveCandidates(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate na-adjective candidates (〜的 patterns)
   */
  std::vector<UnknownCandidate> generateNaAdjectiveCandidates(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate nominalized noun candidates (kanji + short hiragana)
   *
   * Detects nominalized verb stems (連用形転成名詞) like:
   *   - 手助け (from 手助ける)
   *   - 片付け (from 片付ける)
   *   - 引き上げ (from 引き上げる)
   */
  std::vector<UnknownCandidate> generateNominalizedNounCandidates(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate candidates for same-type sequences
   */
  std::vector<UnknownCandidate> generateBySameType(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate alphanumeric sequence candidates
   */
  std::vector<UnknownCandidate> generateAlphanumeric(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Generate candidates with suffix separation
   */
  std::vector<UnknownCandidate> generateWithSuffix(
      std::string_view text, const std::vector<char32_t>& codepoints,
      size_t start_pos,
      const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Get max length for character type
   */
  size_t getMaxLength(normalize::CharType ctype) const;

  /**
   * @brief Get POS for character type
   */
  static core::PartOfSpeech getPosForType(normalize::CharType ctype);

  /**
   * @brief Get cost for character type
   */
  static float getCostForType(normalize::CharType ctype, size_t length);
};

}  // namespace suzume::analysis
#endif  // SUZUME_ANALYSIS_UNKNOWN_H_
