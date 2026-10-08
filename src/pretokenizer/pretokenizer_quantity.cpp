/**
 * @file pretokenizer_quantity.cpp
 * @brief Date, time, and quantity matchers for the pre-tokenizer
 */

#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "pretokenizer/pretokenizer_internal.h"

namespace suzume::pretokenizer {

using namespace pretokenizer_detail;

namespace {

// Whether a scanned one- or two-digit field lies within [min_value, max_value].
bool isTwoDigitFieldInRange(const IntegerScan& field, int min_value, int max_value) {
  return !field.empty() && field.digit_count <= 2 && field.value >= min_value && field.value <= max_value;
}

bool isValidCalendarMonth(const IntegerScan& month) {
  return isTwoDigitFieldInRange(month, 1, 12);
}

bool isValidCalendarDay(const IntegerScan& day) {
  return isTwoDigitFieldInRange(day, 1, 31);
}

// Advances pos past the next codepoint when it is expected; leaves pos unchanged otherwise.
bool consumeCodepoint(std::string_view text, size_t& pos, char32_t expected) {
  size_t next = pos;
  if (next >= text.size() || normalize::decodeUtf8(text, next) != expected) {
    return false;
  }
  pos = next;
  return true;
}

// Advances pos past a duration suffix 間 when absorbsPeriodKan keeps it in the counter.
bool consumePeriodKan(std::string_view text, size_t& pos) {
  size_t after_kan = pos;
  if (!consumeCodepoint(text, after_kan, U'間') || !absorbsPeriodKan(text, after_kan)) {
    return false;
  }
  pos = after_kan;
  return true;
}

}  // namespace

bool PreTokenizer::tryMatchDate(std::string_view text, size_t pos, PreToken& token) const {
  // Match patterns: MM月DD日, YYYY年MM月DD日, YYYY年MM月, YYYY年, YYYY年度 (fiscal year)
  if (pos > 0 && isAsciiDigit(text[pos - 1])) {
    return false;
  }

  const IntegerScan year = scanInteger(text, pos);
  size_t idx = year.end;

  if (year.empty()) {
    return false;
  }

  // A month and day without a year is still an atomic calendar date. Check it
  // before requiring 年 so 7月18日 does not become two adjacent date tokens.
  size_t byte_pos = idx;
  if (isValidCalendarMonth(year) && consumeCodepoint(text, byte_pos, U'月')) {
    const IntegerScan day = scanInteger(text, byte_pos);
    byte_pos = day.end;
    if (isValidCalendarDay(day) && consumeCodepoint(text, byte_pos, U'日')) {
      setTokenFromRange(token, text, pos, byte_pos, PreTokenType::Date, core::PartOfSpeech::Noun);
      return true;
    }
  }

  if (year.digit_count > 4) {
    return false;
  }

  // Check for 年
  if (!consumeCodepoint(text, idx, U'年')) {
    return false;
  }
  const size_t year_end = idx;  // Position right after "年", before any month match

  // Try to match month, then day
  const IntegerScan month = scanInteger(text, idx);
  byte_pos = month.end;
  bool matched_month = false;
  if (isValidCalendarMonth(month) && consumeCodepoint(text, byte_pos, U'月')) {
    idx = byte_pos;
    matched_month = true;
    const IntegerScan day = scanInteger(text, idx);
    byte_pos = day.end;
    if (isValidCalendarDay(day) && consumeCodepoint(text, byte_pos, U'日')) {
      idx = byte_pos;
    }
  }

  // Fiscal year suffix: N年度 (令和6年度, 2024年度予算). Only applies when no
  // month was matched — 年度 marks a fiscal/administrative year, distinct
  // from a calendar date with month/day. Matching it here as part of the
  // atomic date token avoids leaving 度 stranded at a pretokenizer segment
  // boundary, where it has no context to attach to the preceding 年.
  // Duration suffix 間 (期間接尾): N年 + 間 = N年間. Absorbed only when it
  // is not the interval signal 間 + stranded-kanji (see absorbsPeriodKan),
  // so 3年間活動 stays 3年間|活動 while 3年間隔 splits as 3年|間隔.
  byte_pos = year_end;
  if (!matched_month && (consumeCodepoint(text, byte_pos, U'度') || consumePeriodKan(text, byte_pos))) {
    idx = byte_pos;
  }

  // Extent marker 中 (2024年中, 6年度中, 2024年12月中): the date is the quantity
  // the marker measures, so the two belong to one token. Left outside, 中 is
  // stranded at a segment boundary where the analyzer no longer sees the
  // quantity that licenses the extent reading. A following kanji is excluded
  // because 中 then heads a lexical compound of its own (2024年|中止); a numeral
  // is exempt since it opens the second term of a ratio.
  byte_pos = idx;
  if (consumeCodepoint(text, byte_pos, U'中')) {
    size_t after_extent = byte_pos;
    bool closes_phrase = true;
    if (after_extent < text.size()) {
      const char32_t following = normalize::decodeUtf8(text, after_extent);
      closes_phrase = !normalize::isKanjiCodepoint(following) || normalize::isNumeralCodepoint(following);
    }
    if (closes_phrase) {
      idx = byte_pos;
    }
  }

  // A bare year leaves an odd kanji run to the analyzer (3年生, 3年計画書).
  if (!(idx == year_end && leavesOddKanjiRun(text, idx))) {
    setTokenFromRange(token, text, pos, idx, PreTokenType::Date, core::PartOfSpeech::Noun);
    return true;
  }

  return false;
}

bool PreTokenizer::tryMatchCounter(std::string_view text, size_t pos, PreToken& token) const {
  const IntegerScan number = scanInteger(text, pos);
  if (number.empty()) {
    return false;
  }

  size_t idx = number.end;
  if (idx >= text.size()) {
    return false;
  }
  const char32_t prefix = normalize::decodeUtf8(text, idx);
  if (!isMonthPlaceCounterPrefix(prefix) || idx >= text.size()) {
    return false;
  }

  const char32_t unit = normalize::decodeUtf8(text, idx);
  if (unit != U'月' && unit != U'所') {
    return false;
  }

  setTokenFromRange(token, text, pos, idx, PreTokenType::Counter, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchCurrency(std::string_view text, size_t pos, PreToken& token) const {
  // Match patterns: 数字+[万億兆]?円
  size_t idx = scanDigits(text, pos);

  if (idx == pos) {
    return false;
  }

  // Optional: 万, 億, 兆
  for (const char32_t multiplier : {U'万', U'億', U'兆'}) {
    if (consumeCodepoint(text, idx, multiplier)) {
      break;
    }
  }

  // Required: 円
  if (!consumeCodepoint(text, idx, U'円')) {
    return false;
  }
  // A kanji after 円 may continue the quantity phrase (10万円超, 1万円札), which
  // the lattice's counter scan decides by its kanji-run rule, as after 人.
  if (idx < text.size()) {
    size_t next = idx;
    if (normalize::isKanjiCodepoint(normalize::decodeUtf8(text, next))) {
      return false;
    }
  }

  setTokenFromRange(token, text, pos, idx, PreTokenType::Currency, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchGroupedNumber(std::string_view text, size_t pos, PreToken& token) const {
  // A thousands separator joins groups of exactly three digits.  Currency is
  // matched first, so a following 円 retains its established quantity token;
  // this branch supplies the same numeric boundary before other counters.
  size_t idx = pos;
  size_t first_group = 0;
  while (idx < text.size() && isAsciiDigit(text[idx]) && first_group < 4) {
    ++idx;
    ++first_group;
  }
  if (first_group == 0 || first_group > 3 || idx >= text.size() || text[idx] != ',') {
    return false;
  }

  bool has_group = false;
  while (idx < text.size() && text[idx] == ',') {
    ++idx;
    size_t group_size = 0;
    while (idx < text.size() && isAsciiDigit(text[idx]) && group_size < 4) {
      ++idx;
      ++group_size;
    }
    if (group_size != 3) {
      return false;
    }
    has_group = true;
  }
  if (!has_group) {
    return false;
  }

  setTokenFromRange(token, text, pos, idx, PreTokenType::Number, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchStorage(std::string_view text, size_t pos, PreToken& token) const {
  // Match patterns: 数字[KMGT]?B
  size_t idx = scanDigits(text, pos);

  if (idx == pos || idx >= text.size()) {
    return false;
  }

  // Optional: K, M, G, T prefix
  char prefix = text[idx];
  if (prefix == 'K' || prefix == 'k' || prefix == 'M' || prefix == 'm' || prefix == 'G' || prefix == 'g' ||
      prefix == 'T' || prefix == 't') {
    ++idx;
  }

  // Required: B
  if (idx >= text.size() || (text[idx] != 'B' && text[idx] != 'b')) {
    return false;
  }
  ++idx;

  // Reject when the byte suffix is immediately followed by another ASCII letter
  // (e.g. the 'p' of Mbps/kbps/bps), which marks a network bit-rate unit rather
  // than a storage size; let it fall through to normal tokenization instead.
  if (idx < text.size() && isAsciiAlpha(text[idx])) {
    return false;
  }

  setTokenFromRange(token, text, pos, idx, PreTokenType::Storage, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchVersion(std::string_view text, size_t pos, PreToken& token) const {
  // Match patterns: v?数字.数字(.数字)*
  size_t idx = pos;

  // Optional 'v' or 'V' prefix
  if (idx < text.size() && (text[idx] == 'v' || text[idx] == 'V')) {
    ++idx;
  }

  // Dot-separated integer segments (integer-only scans avoid consuming decimal
  // points); a trailing dot without digits is left outside the token.
  size_t segment_count = 0;
  size_t segment_start = idx;
  while (true) {
    const IntegerScan number = scanInteger(text, segment_start);
    if (number.empty()) {
      break;
    }
    idx = number.end;
    ++segment_count;
    if (idx >= text.size() || text[idx] != '.') {
      break;
    }
    segment_start = idx + 1;
  }
  if (segment_count < 2) {
    return false;
  }

  setTokenFromRange(token, text, pos, idx, PreTokenType::Version, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchPercentage(std::string_view text, size_t pos, PreToken& token) const {
  // Match patterns: 数字%
  size_t idx = scanDigits(text, pos);

  // The normalizer has already folded full-width punctuation.
  if (idx == pos || idx >= text.size() || text[idx] != '%') {
    return false;
  }
  ++idx;

  setTokenFromRange(token, text, pos, idx, PreTokenType::Percentage, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchAddressNumber(std::string_view text, size_t pos, PreToken& token) const {
  // Match address number patterns: 1-2-3, 1-2-3-4 etc.
  // Pattern: digit(s) + (hyphen + digit(s))+
  const IntegerScan first_number = scanInteger(text, pos);
  size_t idx = first_number.end;
  if (first_number.empty()) {
    return false;
  }

  // Must have at least one hyphen-number sequence
  bool has_hyphen = false;

  while (idx < text.size() && text[idx] == '-') {
    size_t hyphen_pos = idx;
    ++idx;

    // Parse the next number
    const IntegerScan next_number = scanInteger(text, idx);
    if (next_number.empty()) {
      // No number after hyphen, revert
      idx = hyphen_pos;
      break;
    }

    idx = next_number.end;
    has_hyphen = true;
  }

  if (!has_hyphen) {
    return false;
  }

  setTokenFromRange(token, text, pos, idx, PreTokenType::Number, core::PartOfSpeech::Noun);
  return true;
}

bool PreTokenizer::tryMatchTime(std::string_view text, size_t pos, PreToken& token) const {
  // Match patterns: HH時, HH時MM分, HH時MM分SS秒
  // Check that we're not starting in the middle of a number
  if (pos > 0 && isAsciiDigit(text[pos - 1])) {
    return false;
  }

  // Validate hour (0-23 or 1-24)
  const IntegerScan hour = scanInteger(text, pos);
  size_t idx = hour.end;
  if (!isTwoDigitFieldInRange(hour, 0, 24)) {
    return false;
  }

  // Check for 時
  if (!consumeCodepoint(text, idx, U'時')) {
    return false;
  }

  // A duration starts with 時間 rather than 時. Consume 間 before scanning
  // its optional minute/second fields so 1時間15分 remains one quantity.
  // Leave 間 to the following span when it heads an interval word (5時|間隔).
  consumePeriodKan(text, idx);

  // Try to match minutes, then seconds
  const IntegerScan minute = scanInteger(text, idx);
  size_t byte_pos = minute.end;
  if (isTwoDigitFieldInRange(minute, 0, 59) && consumeCodepoint(text, byte_pos, U'分')) {
    idx = byte_pos;
    const IntegerScan second = scanInteger(text, idx);
    byte_pos = second.end;
    if (isTwoDigitFieldInRange(second, 0, 59) && consumeCodepoint(text, byte_pos, U'秒')) {
      idx = byte_pos;
    }
  }

  // Duration suffix 間 (期間接尾): HH時 + 間 = HH時間. Absorbed only when it is
  // not the interval signal 間 + stranded-kanji (see absorbsPeriodKan), so
  // 24時間営業 stays 24時間|営業 while 5時間隔 splits as 5時|間隔.
  consumePeriodKan(text, idx);

  // An hour or duration leaves an odd kanji run to the analyzer (3時限, 2時間弱).
  if (hasIntervalSuffix(text, idx) || leavesOddKanjiRun(text, idx)) {
    return false;
  }
  setTokenFromRange(token, text, pos, idx, PreTokenType::Time, core::PartOfSpeech::Noun);
  return true;
}

}  // namespace suzume::pretokenizer
