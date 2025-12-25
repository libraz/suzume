#ifndef SUZUME_PRETOKENIZER_PRETOKENIZER_H_
#define SUZUME_PRETOKENIZER_PRETOKENIZER_H_

#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"

namespace suzume::pretokenizer {

/**
 * @brief Type of pre-tokenized element
 */
enum class PreTokenType : uint8_t {
  Url,        // URL (https://...)
  Email,      // Email address
  Date,       // Date (2024年12月23日)
  Time,       // Time (14時30分)
  Currency,   // Currency (100万円)
  Version,    // Version (v2.0.1)
  Storage,    // Storage size (3.5GB)
  Percentage, // Percentage (50%)
  Hashtag,    // Hashtag (#プログラミング)
  Mention,    // Mention (@user)
  Number,     // Plain number
  Boundary,   // Sentence boundary (。！？)
};

/**
 * @brief Pre-tokenized element (confirmed token)
 */
struct PreToken {
  std::string surface;      // Surface string
  size_t start;             // Start position (byte offset)
  size_t end;               // End position (byte offset)
  PreTokenType type;        // Token type
  core::PartOfSpeech pos;   // Part of speech
};

/**
 * @brief Text span that needs further analysis
 */
struct TextSpan {
  size_t start;  // Start position (byte offset)
  size_t end;    // End position (byte offset)
};

/**
 * @brief Result of pre-tokenization
 */
struct PreTokenResult {
  std::vector<PreToken> tokens;  // Confirmed tokens
  std::vector<TextSpan> spans;   // Spans needing analysis
};

/**
 * @brief Pre-tokenizer that extracts confirmed tokens before main analysis
 *
 * Detects patterns that should not be split by the main analyzer:
 * - URLs, email addresses
 * - Dates, times, currencies
 * - Version numbers, storage sizes
 * - Sentence boundaries
 */
class PreTokenizer {
 public:
  PreTokenizer() = default;
  ~PreTokenizer() = default;

  // Copyable and movable (stateless)
  PreTokenizer(const PreTokenizer&) = default;
  PreTokenizer& operator=(const PreTokenizer&) = default;
  PreTokenizer(PreTokenizer&&) = default;
  PreTokenizer& operator=(PreTokenizer&&) = default;

  /**
   * @brief Process text and extract pre-tokens
   * @param text Input text
   * @return Result containing confirmed tokens and spans needing analysis
   */
  PreTokenResult process(std::string_view text) const;

 private:
  /**
   * @brief Try to match URL at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if URL matched
   */
  bool tryMatchUrl(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match date at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if date matched
   */
  bool tryMatchDate(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match currency at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if currency matched
   */
  bool tryMatchCurrency(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match storage size at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if storage size matched
   */
  bool tryMatchStorage(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match version at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if version matched
   */
  bool tryMatchVersion(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match percentage at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if percentage matched
   */
  bool tryMatchPercentage(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match email at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if email matched
   */
  bool tryMatchEmail(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match time at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if time matched
   */
  bool tryMatchTime(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match hashtag at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if hashtag matched
   */
  bool tryMatchHashtag(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Try to match mention at position
   * @param text Input text
   * @param pos Current position
   * @param token Output token if matched
   * @return true if mention matched
   */
  bool tryMatchMention(std::string_view text, size_t pos, PreToken& token) const;

  /**
   * @brief Check if character is sentence boundary
   * @param codepoint Unicode codepoint
   * @return true if sentence boundary
   */
  bool isSentenceBoundary(char32_t codepoint) const;
};

}  // namespace suzume::pretokenizer

#endif  // SUZUME_PRETOKENIZER_PRETOKENIZER_H_
