/**
 * @file pretokenizer.cpp
 * @brief Pre-tokenizer matcher orchestration
 */

#include <utility>

#include "core/text_boundaries.h"
#include "normalize/utf8.h"
#include "pretokenizer/pretokenizer_internal.h"

namespace suzume::pretokenizer {

using namespace pretokenizer_detail;

PreTokenResult PreTokenizer::process(std::string_view text) const {
  PreTokenResult result;

  if (text.empty()) {
    return result;
  }

  size_t pos = 0;
  size_t span_start = 0;
  bool previous_was_digit = false;

  while (pos < text.size()) {
    PreToken token;
    size_t next_pos = pos;
    const char32_t codepoint = normalize::decodeUtf8(text, next_pos);
    const bool current_is_digit = codepoint >= U'0' && codepoint <= U'9';
    // Numeric matchers scan the complete digit run. Once the first position
    // has failed, retrying every suffix of the same run is quadratic and can
    // only manufacture a token that starts in the middle of a number.
    const bool inside_numeric_run = previous_was_digit && current_is_digit;

    // Try to match patterns in priority order
    // Note: URL must come before Email (URLs contain @ in some cases)
    // Note: Email must come before Mention (emails have @ followed by domain)
    // Note: Percentage must come before Version to avoid "3.14%" being parsed as version
    // Note: Date must come before Time (日付 includes 日 which looks like time suffix)
    if (!inside_numeric_run &&
        (tryMatchUrl(text, pos, token) || tryMatchEmail(text, pos, token) || tryMatchHashtag(text, pos, token) ||
         tryMatchMention(text, pos, token) || tryMatchDate(text, pos, token) || tryMatchCounter(text, pos, token) ||
         tryMatchTime(text, pos, token) || tryMatchCurrency(text, pos, token) ||
         tryMatchGroupedNumber(text, pos, token) || tryMatchStorage(text, pos, token) ||
         tryMatchPercentage(text, pos, token) || tryMatchAddressNumber(text, pos, token) ||
         tryMatchVersion(text, pos, token) || tryMatchAsciiWithJoiners(text, pos, token))) {
      // Add span before this token if any
      if (pos > span_start) {
        result.spans.push_back({span_start, pos});
      }

      pos = token.end;
      result.tokens.push_back(std::move(token));
      span_start = pos;
      previous_was_digit = false;
      continue;
    }

    // Check for sentence boundary
    if (core::isSentenceBoundaryCodepoint(codepoint)) {
      // Add span before boundary if any
      if (pos > span_start) {
        result.spans.push_back({span_start, pos});
      }

      // Add boundary token
      setTokenFromRange(token, text, pos, next_pos, PreTokenType::Boundary, core::PartOfSpeech::Symbol);
      result.tokens.push_back(std::move(token));

      pos = next_pos;
      span_start = pos;
      previous_was_digit = false;
      continue;
    }

    // Move to next character
    pos = next_pos;
    previous_was_digit = current_is_digit;
  }

  // Add final span if any
  if (pos > span_start) {
    result.spans.push_back({span_start, pos});
  }

  return result;
}
}  // namespace suzume::pretokenizer
