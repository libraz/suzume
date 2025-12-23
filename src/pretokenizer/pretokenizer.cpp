#include "pretokenizer/pretokenizer.h"

#include <algorithm>
#include <cctype>

#include "normalize/utf8.h"

namespace suzume::pretokenizer {

namespace {

// Check if byte is ASCII digit
bool isAsciiDigit(char chr) { return chr >= '0' && chr <= '9'; }

// Check if byte is ASCII alpha
bool isAsciiAlpha(char chr) {
  return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z');
}

// Check if byte is ASCII alphanumeric
bool isAsciiAlnum(char chr) { return isAsciiDigit(chr) || isAsciiAlpha(chr); }

// Check if this is a full-width digit (０-９)
bool isFullwidthDigit(char32_t codepoint) {
  return codepoint >= 0xFF10 && codepoint <= 0xFF19;
}

// Parse integer digits only (no decimal points), return end position
size_t parseInteger(std::string_view text, size_t pos, std::string& digits) {
  digits.clear();
  size_t idx = pos;
  while (idx < text.size()) {
    char chr = text[idx];
    if (isAsciiDigit(chr)) {
      digits += chr;
      ++idx;
    } else {
      // Check for full-width digits
      size_t byte_pos = idx;
      char32_t codepoint = normalize::decodeUtf8(text, byte_pos);
      if (isFullwidthDigit(codepoint)) {
        digits += static_cast<char>('0' + (codepoint - 0xFF10));
        idx = byte_pos;
      } else {
        break;
      }
    }
  }
  return idx;
}

// Parse digits at position (including decimals), return end position
size_t parseDigits(std::string_view text, size_t pos, std::string& digits) {
  digits.clear();
  size_t idx = pos;
  while (idx < text.size()) {
    char chr = text[idx];
    if (isAsciiDigit(chr)) {
      digits += chr;
      ++idx;
    } else if (chr == '.') {
      // Check if followed by digit (decimal point)
      if (idx + 1 < text.size() && isAsciiDigit(text[idx + 1])) {
        digits += chr;
        ++idx;
      } else {
        break;
      }
    } else if (chr == ',') {
      // Thousand separator - skip but continue
      if (idx + 1 < text.size() && isAsciiDigit(text[idx + 1])) {
        ++idx;
      } else {
        break;
      }
    } else {
      // Check for full-width digits
      size_t byte_pos = idx;
      char32_t codepoint = normalize::decodeUtf8(text, byte_pos);
      if (isFullwidthDigit(codepoint)) {
        digits += static_cast<char>('0' + (codepoint - 0xFF10));
        idx = byte_pos;
      } else {
        break;
      }
    }
  }
  return idx;
}

// Check if text at pos starts with given string (case-insensitive for ASCII)
bool startsWithCI(std::string_view text, size_t pos, std::string_view prefix) {
  if (pos + prefix.size() > text.size()) {
    return false;
  }
  for (size_t idx = 0; idx < prefix.size(); ++idx) {
    char txt_c = text[pos + idx];
    char pre_c = prefix[idx];
    if (std::tolower(static_cast<unsigned char>(txt_c)) !=
        std::tolower(static_cast<unsigned char>(pre_c))) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool PreTokenizer::tryMatchUrl(std::string_view text, size_t pos,
                               PreToken& token) const {
  // Check for http:// or https://
  bool is_https = startsWithCI(text, pos, "https://");
  bool is_http = !is_https && startsWithCI(text, pos, "http://");

  if (!is_https && !is_http) {
    return false;
  }

  size_t start = pos;
  size_t idx = pos + (is_https ? 8 : 7);  // Skip protocol

  // Match URL characters until whitespace or end
  while (idx < text.size()) {
    char chr = text[idx];
    // URL-safe characters
    if (isAsciiAlnum(chr) || chr == '-' || chr == '.' || chr == '_' ||
        chr == '~' || chr == ':' || chr == '/' || chr == '?' || chr == '#' ||
        chr == '[' || chr == ']' || chr == '@' || chr == '!' || chr == '$' ||
        chr == '&' || chr == '\'' || chr == '(' || chr == ')' || chr == '*' ||
        chr == '+' || chr == ',' || chr == ';' || chr == '=' || chr == '%') {
      ++idx;
    } else {
      break;
    }
  }

  // Remove trailing punctuation that's likely not part of URL
  while (idx > start && (text[idx - 1] == '.' || text[idx - 1] == ',' ||
                         text[idx - 1] == ')' || text[idx - 1] == '\'')) {
    --idx;
  }

  if (idx > start + (is_https ? 8 : 7)) {
    token.surface = std::string(text.substr(start, idx - start));
    token.start = start;
    token.end = idx;
    token.type = PreTokenType::Url;
    token.pos = core::PartOfSpeech::Symbol;
    return true;
  }

  return false;
}

bool PreTokenizer::tryMatchDate(std::string_view text, size_t pos,
                                PreToken& token) const {
  // Match patterns: YYYY年MM月DD日, YYYY年MM月, YYYY年
  std::string year_str;
  size_t idx = parseDigits(text, pos, year_str);

  if (year_str.empty() || year_str.size() < 1 || year_str.size() > 4) {
    return false;
  }

  // Check for 年
  size_t byte_pos = idx;
  if (byte_pos >= text.size()) {
    return false;
  }

  char32_t codepoint = normalize::decodeUtf8(text, byte_pos);
  if (codepoint != U'年') {
    return false;
  }
  idx = byte_pos;

  // Try to match month
  std::string month_str;
  size_t month_end = parseDigits(text, idx, month_str);

  if (!month_str.empty() && month_str.size() <= 2) {
    byte_pos = month_end;
    if (byte_pos < text.size()) {
      codepoint = normalize::decodeUtf8(text, byte_pos);
      if (codepoint == U'月') {
        idx = byte_pos;

        // Try to match day
        std::string day_str;
        size_t day_end = parseDigits(text, idx, day_str);

        if (!day_str.empty() && day_str.size() <= 2) {
          byte_pos = day_end;
          if (byte_pos < text.size()) {
            codepoint = normalize::decodeUtf8(text, byte_pos);
            if (codepoint == U'日') {
              idx = byte_pos;
            }
          }
        }
      }
    }
  }

  if (idx > pos) {
    token.surface = std::string(text.substr(pos, idx - pos));
    token.start = pos;
    token.end = idx;
    token.type = PreTokenType::Date;
    token.pos = core::PartOfSpeech::Noun;
    return true;
  }

  return false;
}

bool PreTokenizer::tryMatchCurrency(std::string_view text, size_t pos,
                                    PreToken& token) const {
  // Match patterns: 数字+[万億兆]?円
  std::string num_str;
  size_t idx = parseDigits(text, pos, num_str);

  if (num_str.empty()) {
    return false;
  }

  size_t byte_pos = idx;
  if (byte_pos >= text.size()) {
    return false;
  }

  char32_t codepoint = normalize::decodeUtf8(text, byte_pos);

  // Optional: 万, 億, 兆
  if (codepoint == U'万' || codepoint == U'億' || codepoint == U'兆') {
    idx = byte_pos;
    if (byte_pos < text.size()) {
      codepoint = normalize::decodeUtf8(text, byte_pos);
    } else {
      return false;
    }
  }

  // Required: 円
  if (codepoint != U'円') {
    return false;
  }
  idx = byte_pos;

  token.surface = std::string(text.substr(pos, idx - pos));
  token.start = pos;
  token.end = idx;
  token.type = PreTokenType::Currency;
  token.pos = core::PartOfSpeech::Noun;
  return true;
}

bool PreTokenizer::tryMatchStorage(std::string_view text, size_t pos,
                                   PreToken& token) const {
  // Match patterns: 数字[KMGT]?B
  std::string num_str;
  size_t idx = parseDigits(text, pos, num_str);

  if (num_str.empty()) {
    return false;
  }

  if (idx >= text.size()) {
    return false;
  }

  // Optional: K, M, G, T prefix
  char prefix = text[idx];
  if (prefix == 'K' || prefix == 'k' || prefix == 'M' || prefix == 'm' ||
      prefix == 'G' || prefix == 'g' || prefix == 'T' || prefix == 't') {
    ++idx;
  }

  // Required: B
  if (idx >= text.size() || (text[idx] != 'B' && text[idx] != 'b')) {
    return false;
  }
  ++idx;

  token.surface = std::string(text.substr(pos, idx - pos));
  token.start = pos;
  token.end = idx;
  token.type = PreTokenType::Storage;
  token.pos = core::PartOfSpeech::Noun;
  return true;
}

bool PreTokenizer::tryMatchVersion(std::string_view text, size_t pos,
                                   PreToken& token) const {
  // Match patterns: v?数字.数字(.数字)*
  size_t idx = pos;

  // Optional 'v' or 'V' prefix
  if (idx < text.size() && (text[idx] == 'v' || text[idx] == 'V')) {
    ++idx;
  }

  // First number (use parseInteger to avoid consuming decimal points)
  std::string num_str;
  size_t num_end = parseInteger(text, idx, num_str);
  if (num_str.empty()) {
    return false;
  }
  idx = num_end;

  // Must have at least one .number
  if (idx >= text.size() || text[idx] != '.') {
    return false;
  }
  ++idx;

  num_end = parseInteger(text, idx, num_str);
  if (num_str.empty()) {
    return false;
  }
  idx = num_end;

  // Additional .number segments
  while (idx < text.size() && text[idx] == '.') {
    size_t next = idx + 1;
    num_end = parseInteger(text, next, num_str);
    if (num_str.empty()) {
      break;
    }
    idx = num_end;
  }

  token.surface = std::string(text.substr(pos, idx - pos));
  token.start = pos;
  token.end = idx;
  token.type = PreTokenType::Version;
  token.pos = core::PartOfSpeech::Noun;
  return true;
}

bool PreTokenizer::tryMatchPercentage(std::string_view text, size_t pos,
                                      PreToken& token) const {
  // Match patterns: 数字%
  std::string num_str;
  size_t idx = parseDigits(text, pos, num_str);

  if (num_str.empty()) {
    return false;
  }

  if (idx >= text.size()) {
    return false;
  }

  // Check for % (ASCII) or ％ (full-width)
  char chr = text[idx];
  if (chr == '%') {
    ++idx;
  } else {
    size_t byte_pos = idx;
    char32_t codepoint = normalize::decodeUtf8(text, byte_pos);
    if (codepoint == U'％') {
      idx = byte_pos;
    } else {
      return false;
    }
  }

  token.surface = std::string(text.substr(pos, idx - pos));
  token.start = pos;
  token.end = idx;
  token.type = PreTokenType::Percentage;
  token.pos = core::PartOfSpeech::Noun;
  return true;
}

bool PreTokenizer::isSentenceBoundary(char32_t codepoint) const {
  return codepoint == U'。' || codepoint == U'！' || codepoint == U'？' ||
         codepoint == U'!' || codepoint == U'?' || codepoint == U'\n';
}

PreTokenResult PreTokenizer::process(std::string_view text) const {
  PreTokenResult result;

  if (text.empty()) {
    return result;
  }

  size_t pos = 0;
  size_t span_start = 0;

  while (pos < text.size()) {
    PreToken token;

    // Try to match patterns in priority order
    // Note: Percentage must come before Version to avoid "3.14%" being parsed as version
    if (tryMatchUrl(text, pos, token) || tryMatchDate(text, pos, token) ||
        tryMatchCurrency(text, pos, token) ||
        tryMatchStorage(text, pos, token) ||
        tryMatchPercentage(text, pos, token) ||
        tryMatchVersion(text, pos, token)) {
      // Add span before this token if any
      if (pos > span_start) {
        result.spans.push_back({span_start, pos});
      }

      result.tokens.push_back(token);
      pos = token.end;
      span_start = pos;
      continue;
    }

    // Check for sentence boundary
    size_t byte_pos = pos;
    char32_t codepoint = normalize::decodeUtf8(text, byte_pos);

    if (isSentenceBoundary(codepoint)) {
      // Add span before boundary if any
      if (pos > span_start) {
        result.spans.push_back({span_start, pos});
      }

      // Add boundary token
      token.surface = std::string(text.substr(pos, byte_pos - pos));
      token.start = pos;
      token.end = byte_pos;
      token.type = PreTokenType::Boundary;
      token.pos = core::PartOfSpeech::Symbol;
      result.tokens.push_back(token);

      pos = byte_pos;
      span_start = pos;
      continue;
    }

    // Move to next character
    pos = byte_pos;
  }

  // Add final span if any
  if (pos > span_start) {
    result.spans.push_back({span_start, pos});
  }

  return result;
}

}  // namespace suzume::pretokenizer
