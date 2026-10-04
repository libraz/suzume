/**
 * @file pretokenizer_utils.cpp
 * @brief Shared byte and numeric helpers for pre-tokenizer matchers
 */

#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "pretokenizer/pretokenizer_internal.h"

namespace suzume::pretokenizer::pretokenizer_detail {

// Check if byte is ASCII digit
bool isAsciiDigit(char chr) {
  return chr >= '0' && chr <= '9';
}

// Decide whether a period suffix 間 immediately following a time/duration
// counter (N時/N年) should be absorbed into the atomic counter token.
//
// Absorb by default — N時間/N年間 are lexicalized durations, so keeping them
// atomic prevents 間 from being stranded at a segment boundary. The single
// exception is the interval signal: 間 followed by a lone kanji that is itself
// followed by a non-kanji boundary (間隔で, 間近に). There the trailing kanji
// would be stranded, so 間 heads the following interval word (間隔/間近) rather
// than the counter. When 間 is followed by two or more kanji (時間営業, 年間活動)
// or by a non-kanji (年間の, 年間で), the duration reading is correct. This
// mirrors the cost-model discrimination that split_candidates applies to the
// non-pretokenized N分間 path.
//
// `pos_after_kan` is the byte offset of the character following 間.
bool absorbsPeriodKan(std::string_view text, size_t pos_after_kan) {
  if (pos_after_kan >= text.size()) {
    return true;  // 間 at end of input → duration
  }
  size_t idx = pos_after_kan;
  char32_t next_cp = normalize::decodeUtf8(text, idx);
  if (!normalize::isKanjiCodepoint(next_cp)) {
    return true;  // 間 + hiragana/particle → duration (年間の, 年間で)
  }
  if (normalize::isTemporalRelationSuffixKanji(next_cp)) {
    return true;  // 間 + 後/前 → duration + relational suffix (2時間|後, 5年間|前)
  }
  if (idx < text.size() && normalize::isKanjiCodepoint(normalize::decodeUtf8(text, idx))) {
    return true;  // 2+ kanji → duration (時間営業, 年間活動)
  }
  // 間 + single kanji at the end or before a non-kanji: interval only when the kanji
  // forms an 間X compound (間隔で), else the duration reading (年間|続けた, 時間|半).
  return !normalize::isIntervalCompoundSecondKanji(next_cp);
}

// Whether the kanji run starting at `pos` has odd length, a closing
// quantity-phrase suffix aside (3時限+目). Read in two-kanji words, such a run
// cannot follow an atomic counter token without one kanji being cut off its
// word (3年|生, 3年|計画書), so the span is left to the analyzer, which takes
// the run whole. Relational 前/後 stand on their own (3年|後).
bool leavesOddKanjiRun(std::string_view text, size_t pos) {
  if (pos >= text.size()) {
    return false;
  }
  size_t idx = pos;
  const char32_t first = normalize::decodeUtf8(text, idx);
  if (!normalize::isKanjiCodepoint(first) || normalize::isNumeralCodepoint(first) ||
      normalize::isTemporalRelationSuffixKanji(first)) {
    return false;
  }
  size_t run_length = 1;
  char32_t last = first;
  while (idx < text.size()) {
    size_t next_idx = idx;
    const char32_t next = normalize::decodeUtf8(text, next_idx);
    if (!normalize::isKanjiCodepoint(next) || normalize::isNumeralCodepoint(next)) {
      break;
    }
    idx = next_idx;
    last = next;
    ++run_length;
  }
  if (run_length > 1 && normalize::isQuantityPhraseSuffixKanji(last)) {
    --run_length;
  }
  return run_length % 2 == 1;
}

// Keep a duration counter in the analyzer when the closed-class interval
// suffix follows it. This preserves the noun→suffix boundary in 1時間おき,
// which an atomic time pretoken would otherwise hide.
bool hasIntervalSuffix(std::string_view text, size_t pos) {
  constexpr std::string_view kIntervalSuffix{"おき"};
  return text.substr(pos).compare(0, kIntervalSuffix.size(), kIntervalSuffix) == 0;
}

// Check if byte is ASCII alpha
bool isAsciiAlpha(char chr) {
  return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z');
}

// Check if byte is ASCII alphanumeric
bool isAsciiAlnum(char chr) {
  return isAsciiDigit(chr) || isAsciiAlpha(chr);
}

// Scan integer digits without allocating. Value is retained for the first two
// digits, which is sufficient for the bounded time fields below.
IntegerScan scanInteger(std::string_view text, size_t pos) {
  size_t idx = pos;
  size_t digit_count = 0;
  int value = 0;
  while (idx < text.size() && isAsciiDigit(text[idx])) {
    if (digit_count < 2) {
      value = value * 10 + (text[idx] - '0');
    }
    ++digit_count;
    ++idx;
  }
  return {idx, digit_count, value};
}

// Month and place counters have five conventional prefix spellings. Treating
// the spelling variation as one orthographic class keeps the numeric counter
// atomic without adding entries for individual surface forms.
bool isMonthPlaceCounterPrefix(char32_t codepoint) {
  return codepoint == U'か' || codepoint == U'ヶ' || codepoint == U'ヵ' || codepoint == U'ケ' || codepoint == U'箇';
}

bool hasAsciiRunLeftNeighbor(std::string_view text, size_t pos, std::string_view punctuation) {
  if (pos == 0) {
    return false;
  }
  const char previous = text[pos - 1];
  return isAsciiAlnum(previous) || punctuation.find(previous) != std::string_view::npos;
}

// Parse digits at position (including decimals), return end position
size_t scanDigits(std::string_view text, size_t pos) {
  size_t idx = pos;
  bool seen_comma = false;
  size_t digits_since_comma = 0;
  while (idx < text.size()) {
    char chr = text[idx];
    if (isAsciiDigit(chr)) {
      ++idx;
      ++digits_since_comma;
    } else if (chr == '.') {
      // Check if followed by digit (decimal point)
      if (idx + 1 < text.size() && isAsciiDigit(text[idx + 1])) {
        ++idx;
        digits_since_comma = 0;
      } else {
        break;
      }
    } else if (chr == ',') {
      // Thousand separator: require one to three digits before the first comma,
      // and exactly three digits between subsequent commas.
      if ((!seen_comma || digits_since_comma == 3) && digits_since_comma > 0 && digits_since_comma <= 3 &&
          idx + 3 < text.size() && isAsciiDigit(text[idx + 1]) && isAsciiDigit(text[idx + 2]) &&
          isAsciiDigit(text[idx + 3]) && (idx + 4 >= text.size() || !isAsciiDigit(text[idx + 4]))) {
        seen_comma = true;
        digits_since_comma = 0;
        ++idx;
      } else {
        break;
      }
    } else {
      break;
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
    const auto fold_ascii = [](char chr) {
      return chr >= 'A' && chr <= 'Z' ? static_cast<char>(chr - 'A' + 'a') : chr;
    };
    if (fold_ascii(text[pos + idx]) != fold_ascii(prefix[idx])) {
      return false;
    }
  }
  return true;
}

void setTokenFromRange(PreToken& token, std::string_view text, size_t start, size_t end, PreTokenType type,
                       core::PartOfSpeech pos) {
  token.surface = std::string(text.substr(start, end - start));
  token.start = start;
  token.end = end;
  token.type = type;
  token.pos = pos;
}

}  // namespace suzume::pretokenizer::pretokenizer_detail
