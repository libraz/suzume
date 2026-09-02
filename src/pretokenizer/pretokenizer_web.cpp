/**
 * @file pretokenizer_web.cpp
 * @brief Web identifier matchers for the pre-tokenizer
 */

#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "pretokenizer/pretokenizer_internal.h"

namespace suzume::pretokenizer {

using namespace pretokenizer_detail;

namespace {

// Characters an ASCII word keeps inside itself. Brand and technical names are
// written with these as part of the spelling (Coca-Cola, McDonald's, H&M,
// CI/CD, example.com), so splitting on them would break the search unit into
// fragments that no query matches.
constexpr std::string_view kAsciiWordJoiners = ".-'&/";

size_t trailingUnmatchedClosingParentheses(std::string_view text, size_t start, size_t end) {
  size_t open_count = 0;
  size_t trailing_unmatched = 0;
  for (size_t idx = start; idx < end; ++idx) {
    if (text[idx] == '(') {
      ++open_count;
      trailing_unmatched = 0;
    } else if (text[idx] == ')') {
      if (open_count > 0) {
        --open_count;
        trailing_unmatched = 0;
      } else {
        ++trailing_unmatched;
      }
    } else if (text[idx] != '.' && text[idx] != ',') {
      trailing_unmatched = 0;
    }
  }
  return trailing_unmatched;
}

void trimTrailingStopsAndCommas(std::string_view text, size_t start, size_t& end) {
  while (end > start && (text[end - 1] == '.' || text[end - 1] == ',')) {
    --end;
  }
}

}  // namespace

bool PreTokenizer::tryMatchUrl(std::string_view text, size_t pos, PreToken& token) const {
  // Check for http:// or https://
  bool is_https = startsWithCI(text, pos, "https://");
  bool is_http = !is_https && startsWithCI(text, pos, "http://");

  if (!is_https && !is_http) {
    return false;
  }

  size_t start = pos;
  size_t idx = pos + (is_https ? 8 : 7);  // Skip protocol
  size_t apostrophe_count = 0;

  // Match URL characters until whitespace or end
  while (idx < text.size()) {
    char chr = text[idx];
    // URL-safe characters
    if (isAsciiAlnum(chr) || chr == '-' || chr == '.' || chr == '_' || chr == '~' || chr == ':' || chr == '/' ||
        chr == '?' || chr == '#' || chr == '[' || chr == ']' || chr == '@' || chr == '!' || chr == '$' || chr == '&' ||
        chr == '\'' || chr == '(' || chr == ')' || chr == '*' || chr == '+' || chr == ',' || chr == ';' || chr == '=' ||
        chr == '%') {
      if (chr == '\'') {
        ++apostrophe_count;
      }
      ++idx;
    } else {
      break;
    }
  }

  // A trailing apostrophe is surrounding punctuation only when it has no mate
  // inside the URL. Paired apostrophes and balanced parentheses are URL data.
  if (idx > start && text[idx - 1] == '\'' && apostrophe_count % 2 == 1) {
    --idx;
  }
  trimTrailingStopsAndCommas(text, start, idx);
  idx -= trailingUnmatchedClosingParentheses(text, start, idx);
  trimTrailingStopsAndCommas(text, start, idx);

  if (idx > start + (is_https ? 8 : 7)) {
    setTokenFromRange(token, text, start, idx, PreTokenType::Url,
                      core::PartOfSpeech::Noun);  // Treat URLs as nouns (not symbols)
    return true;
  }

  return false;
}

bool PreTokenizer::tryMatchEmail(std::string_view text, size_t pos, PreToken& token) const {
  // Match email: local-part@domain
  // Check that we're not starting in the middle of an email-like string
  if (hasAsciiRunLeftNeighbor(text, pos, ".-_+@")) {
    return false;
  }

  size_t start = pos;
  size_t idx = pos;

  // Parse local-part: alphanumeric, dot, hyphen, underscore, plus
  while (idx < text.size()) {
    char chr = text[idx];
    if (isAsciiAlnum(chr) || chr == '.' || chr == '-' || chr == '_' || chr == '+') {
      ++idx;
    } else {
      break;
    }
  }

  // Local-part must not be empty and must not start/end with dot
  if (idx == start || text[start] == '.' || text[idx - 1] == '.') {
    return false;
  }

  // Must have @
  if (idx >= text.size() || text[idx] != '@') {
    return false;
  }
  ++idx;

  // Parse domain: alphanumeric, dot, hyphen
  size_t domain_start = idx;
  while (idx < text.size()) {
    char chr = text[idx];
    if (isAsciiAlnum(chr) || chr == '.' || chr == '-') {
      ++idx;
    } else {
      break;
    }
  }

  // Domain must not be empty and must contain at least one dot
  if (idx == domain_start) {
    return false;
  }

  std::string_view domain = text.substr(domain_start, idx - domain_start);
  if (domain.find('.') == std::string_view::npos) {
    return false;
  }

  // Domain must not start/end with dot or hyphen
  if (domain[0] == '.' || domain[0] == '-' || domain[domain.size() - 1] == '.' || domain[domain.size() - 1] == '-') {
    return false;
  }

  setTokenFromRange(token, text, start, idx, PreTokenType::Email, core::PartOfSpeech::Noun);
  return true;
}

namespace {

// Check if codepoint is valid for hashtag content
// Allows: Hiragana, Katakana, Kanji, alphanumeric, underscore
// A tag is a single search unit and ends at whitespace, punctuation or any other
// symbol, so the body class is word text in any script. Particles inside a tag
// (#今日は…) belong to the tag; the marker only opens one at a text boundary,
// which is what keeps は#… from starting a tag mid-sentence.
bool isHashtagChar(char32_t codepoint) {
  // ASCII alphanumeric and underscore
  if ((codepoint >= 'a' && codepoint <= 'z') || (codepoint >= 'A' && codepoint <= 'Z') ||
      (codepoint >= '0' && codepoint <= '9') || codepoint == '_') {
    return true;
  }
  // Katakana (U+30A0-U+30FF) - allowed for hashtags
  if (codepoint >= 0x30A0 && codepoint <= 0x30FF) {
    return true;
  }
  if (normalize::isKanjiCodepoint(codepoint)) {
    return true;
  }
  // Full-width ASCII letters, digits, and underscore. Do not accept the
  // surrounding punctuation block: in particular, a second ＃ starts a new
  // hashtag rather than becoming content of the first.
  if ((codepoint >= U'０' && codepoint <= U'９') || (codepoint >= U'Ａ' && codepoint <= U'Ｚ') ||
      (codepoint >= U'ａ' && codepoint <= U'ｚ') || codepoint == U'＿') {
    return true;
  }
  // Hiragana (U+3041-U+3096) and the iteration mark 々.
  if (codepoint >= 0x3041 && codepoint <= 0x3096) {
    return true;
  }
  return codepoint == U'々';
}

// A marker opens a tag at a text boundary — start of input, whitespace, punctuation,
// or the marker of a preceding tag (#東京#テスト). Scanning back over body characters
// and running out of text means the marker sits inside an ordinary word (C#).
bool opensHashtag(std::string_view text, size_t pos) {
  size_t cursor = pos;
  while (cursor > 0) {
    size_t start = cursor - 1;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) {
      --start;
    }
    size_t decode_pos = start;
    if (!isHashtagChar(normalize::decodeUtf8(text, decode_pos))) {
      return true;
    }
    cursor = start;
  }
  return pos == 0;
}

}  // namespace

bool PreTokenizer::tryMatchHashtag(std::string_view text, size_t pos, PreToken& token) const {
  // Match pattern: # + (Japanese chars | alphanumeric | underscore)+
  if (pos >= text.size()) {
    return false;
  }

  // The normalizer has already folded full-width punctuation.
  size_t idx = pos;
  size_t byte_pos = pos;
  char32_t codepoint = normalize::decodeUtf8(text, byte_pos);

  if (codepoint != '#') {
    return false;
  }
  if (!opensHashtag(text, pos)) {
    return false;
  }
  idx = byte_pos;

  // Must have at least one valid hashtag character
  if (idx >= text.size()) {
    return false;
  }

  size_t content_start = idx;
  while (idx < text.size()) {
    byte_pos = idx;
    codepoint = normalize::decodeUtf8(text, byte_pos);
    if (isHashtagChar(codepoint)) {
      idx = byte_pos;
    } else {
      break;
    }
  }

  // Must have content after #
  if (idx == content_start) {
    return false;
  }

  setTokenFromRange(token, text, pos, idx, PreTokenType::Hashtag, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchMention(std::string_view text, size_t pos, PreToken& token) const {
  // Match pattern: @ + (alphanumeric | underscore)+
  // Must NOT be followed by domain (that would be email)
  if (pos >= text.size()) {
    return false;
  }

  // Check for @ (ASCII only for mentions)
  if (text[pos] != '@') {
    return false;
  }
  size_t idx = pos + 1;

  // Must have at least one valid character
  if (idx >= text.size()) {
    return false;
  }

  // Parse username: alphanumeric and underscore only
  size_t content_start = idx;
  while (idx < text.size()) {
    char chr = text[idx];
    if (isAsciiAlnum(chr) || chr == '_') {
      ++idx;
    } else {
      break;
    }
  }

  // Must have content after @
  if (idx == content_start) {
    return false;
  }

  // Check this is NOT an email (no @ followed by domain with dot)
  // If followed by @, it's invalid
  // If the content contains a dot followed by more chars, check if it's email-like
  // Simple check: mentions don't have dots in username typically
  // Also check if there's more content that looks like a domain
  if (idx < text.size() && text[idx] == '.') {
    // Might be email-like, check for domain pattern
    size_t check_pos = idx + 1;
    while (check_pos < text.size()) {
      char chr = text[check_pos];
      if (isAsciiAlnum(chr) || chr == '.' || chr == '-') {
        ++check_pos;
      } else {
        break;
      }
    }
    // If we found something that looks like a domain, skip this as mention
    if (check_pos > idx + 1) {
      return false;
    }
  }

  setTokenFromRange(token, text, pos, idx, PreTokenType::Mention, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchAsciiWithJoiners(std::string_view text, size_t pos, PreToken& token) const {
  // Match ASCII alphanumeric sequences held together by a word-internal joiner
  // Pattern: alnum+ (joiner alnum+)+
  // e.g., example.com, Coca-Cola, McDonald's, H&M, CI/CD
  // Must have at least one joiner to distinguish from regular ASCII sequences

  if (pos >= text.size() || !isAsciiAlnum(text[pos]) || hasAsciiRunLeftNeighbor(text, pos, kAsciiWordJoiners)) {
    return false;
  }

  size_t start = pos;
  size_t idx = pos;
  bool has_joiner = false;

  while (idx < text.size()) {
    char chr = text[idx];
    if (isAsciiAlnum(chr)) {
      ++idx;
    } else if (kAsciiWordJoiners.find(chr) != std::string_view::npos && idx + 1 < text.size() &&
               isAsciiAlnum(text[idx + 1])) {
      // Joiner surrounded by alphanumerics
      has_joiner = true;
      ++idx;
    } else {
      break;
    }
  }

  // Must have at least one joiner and not end with one
  if (!has_joiner || idx <= start + 2) {
    return false;
  }

  setTokenFromRange(token, text, start, idx, PreTokenType::AsciiSeq, core::PartOfSpeech::Noun);
  return true;
}

}  // namespace suzume::pretokenizer
