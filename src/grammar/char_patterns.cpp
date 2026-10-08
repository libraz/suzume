/**
 * @file char_patterns.cpp
 * @brief Character pattern utilities for Japanese verb/adjective analysis
 */

#include "char_patterns.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"

namespace suzume::grammar {

using normalize::encodeUtf8;

namespace {

// =============================================================================
// Character Iteration Templates
// =============================================================================

/**
 * @brief Check if all characters in string match a predicate
 * Iterates through 3-byte UTF-8 sequences (Japanese characters)
 */
template <typename Predicate>
bool allCharsMatch(std::string_view str, Predicate pred) {
  if (str.empty())
    return false;
  size_t pos = 0;
  while (pos < str.size()) {
    if (!utf8::is3ByteUtf8At(str, pos))
      return false;
    char32_t cp = utf8::decode3ByteUtf8At(str, pos);
    if (!pred(cp))
      return false;
    pos += core::kJapaneseCharBytes;
  }
  return true;
}

/**
 * @brief Check if any character in string matches a predicate
 * Handles mixed-byte strings (skips non-3-byte sequences)
 */
template <typename Predicate>
bool anyCharMatches(std::string_view str, Predicate pred) {
  if (str.empty())
    return false;
  size_t pos = 0;
  while (pos + core::kJapaneseCharBytes <= str.size()) {
    if (utf8::is3ByteUtf8At(str, pos)) {
      char32_t cp = utf8::decode3ByteUtf8At(str, pos);
      if (pred(cp))
        return true;
      pos += core::kJapaneseCharBytes;
    } else {
      pos += 1;
    }
  }
  return false;
}

/// Whether the last character exists and satisfies a predicate.
template <typename Predicate>
bool lastCharMatches(std::string_view str, Predicate pred) {
  const char32_t cp = utf8::decodeLastChar(str);
  return cp != 0 && pred(cp);
}

/// Whether the first character exists and satisfies a predicate.
template <typename Predicate>
bool firstCharMatches(std::string_view str, Predicate pred) {
  const char32_t cp = utf8::decodeFirstChar(str);
  return cp != 0 && pred(cp);
}

}  // namespace

bool endsWithIRow(std::string_view stem) {
  return lastCharMatches(stem, kana::isIRowCodepoint);
}

bool endsWithERow(std::string_view stem) {
  return lastCharMatches(stem, kana::isERowCodepoint);
}

bool endsWithOnbin(std::string_view stem) {
  return lastCharMatches(stem, kana::isOnbinCodepoint);
}

bool endsWithRenyokeiMarker(std::string_view stem) {
  return endsWithIRow(stem) || endsWithERow(stem);
}

bool isDeverbalSuffixKanji(char32_t codepoint) {
  return codepoint == U'方' || codepoint == U'手' || codepoint == U'物' || codepoint == U'所' || codepoint == U'場';
}

bool isBoundDeverbalSuffixAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size() || !isDeverbalSuffixKanji(codepoints[pos])) {
    return false;
  }
  return codepoints[pos] == U'物' || pos + 1 >= codepoints.size() || !normalize::isKanjiCodepoint(codepoints[pos + 1]);
}

size_t nominalKanjiRunEnd(const std::vector<char32_t>& codepoints, size_t pos) {
  size_t run_end = pos;
  while (run_end < codepoints.size() && normalize::isKanjiCodepoint(codepoints[run_end]) &&
         !normalize::isNumeralCodepoint(codepoints[run_end])) {
    ++run_end;
  }
  return run_end;
}

size_t countKanjiRunWords(const std::vector<char32_t>& codepoints, size_t pos, size_t run_end) {
  size_t counted = run_end - pos;
  if (counted > 1 && normalize::isQuantityPhraseSuffixKanji(codepoints[run_end - 1])) {
    --counted;
  }
  return counted;
}

bool mayBeOkuriganaAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size() || !kana::isHiraganaCodepoint(codepoints[pos]) ||
      normalize::isParticleCodepoint(codepoints[pos])) {
    return false;
  }
  constexpr size_t kCopulaProbeLength = 3;
  return !startsPredicativeCopula(
      normalize::encodeRange(codepoints, pos, std::min(pos + kCopulaProbeLength, codepoints.size())));
}

bool isTransitiveAsuStem(std::u32string_view stem) {
  if (stem.empty() || !kana::isARowCodepoint(stem.back())) {
    return false;
  }
  const size_t half = stem.size() / 2;
  return stem.size() % 2 != 0 || stem.substr(0, half) != stem.substr(half);
}

bool endsWithChar(std::string_view stem, const char* const chars[], size_t count) {
  if (stem.size() < core::kJapaneseCharBytes) {
    return false;
  }
  std::string_view last = utf8::lastChar(stem);
  for (size_t idx = 0; idx < count; ++idx) {
    if (last == chars[idx]) {
      return true;
    }
  }
  return false;
}

bool isAllKanji(std::string_view stem) {
  return allCharsMatch(stem, kana::isKanjiCodepoint);
}

bool endsWithKanji(std::string_view stem) {
  return lastCharMatches(stem, kana::isKanjiCodepoint);
}

bool endsWithHiragana(std::string_view stem) {
  return lastCharMatches(stem, kana::isHiraganaCodepoint);
}

bool startsWithKanji(std::string_view stem) {
  return firstCharMatches(stem, kana::isKanjiCodepoint);
}

bool containsKanji(std::string_view stem) {
  return anyCharMatches(stem, kana::isKanjiCodepoint);
}

bool isPureHiragana(std::string_view stem) {
  return allCharsMatch(stem, kana::isHiraganaCodepoint);
}

bool isBigradeTerminalKana(char32_t code) {
  // す is left out: サ行 bigrade is marginal, while する is the light verb every
  // sahen nominal takes, so the row would claim that construction instead.
  constexpr std::array<char32_t, 11> kBigradeTerminals = {U'う', U'く', U'ぐ', U'つ', U'づ', U'ぬ',
                                                          U'ふ', U'ぶ', U'む', U'ゆ', U'る'};
  return kana::isCodepointIn(kBigradeTerminals, code);
}

char32_t bigradeIrrealisKana(char32_t terminal) {
  constexpr std::array<std::pair<char32_t, char32_t>, 11> kIrrealis = {{{U'う', U'え'},
                                                                        {U'く', U'け'},
                                                                        {U'ぐ', U'げ'},
                                                                        {U'つ', U'て'},
                                                                        {U'づ', U'で'},
                                                                        {U'ぬ', U'ね'},
                                                                        {U'ふ', U'へ'},
                                                                        {U'ぶ', U'べ'},
                                                                        {U'む', U'め'},
                                                                        {U'ゆ', U'え'},
                                                                        {U'る', U'れ'}}};
  for (const auto& [u_row, e_row] : kIrrealis) {
    if (u_row == terminal) {
      return e_row;
    }
  }
  return 0;
}

char32_t bigradeTerminalFromIrrealis(char32_t irrealis) {
  // え is shared by ア行 and ヤ行; the ヤ行 verbs (見ゆ, 消ゆ, 越ゆ) are the ones
  // that survive as modern え-ending lower-monograde stems, so it maps there.
  constexpr std::array<std::pair<char32_t, char32_t>, 8> kTerminals = {{{U'け', U'く'},
                                                                        {U'げ', U'ぐ'},
                                                                        {U'て', U'つ'},
                                                                        {U'で', U'づ'},
                                                                        {U'ね', U'ぬ'},
                                                                        {U'べ', U'ぶ'},
                                                                        {U'め', U'む'},
                                                                        {U'え', U'ゆ'}}};
  for (const auto& [e_row, u_row] : kTerminals) {
    if (e_row == irrealis) {
      return u_row;
    }
  }
  return irrealis == U'れ' ? U'る' : 0;
}

std::string yaRowBigradeTerminalLemma(std::string_view base_form) {
  if (!utf8::endsWith(base_form, "ゆる") || base_form.size() <= core::kTwoJapaneseCharBytes) {
    return {};
  }
  return std::string(base_form.substr(0, base_form.size() - core::kJapaneseCharBytes));
}

bool isModernGodanTerminalKana(char32_t code) {
  return kana::isGodanTerminalCodepoint(code);
}

bool isUnproductiveGodanTerminalKana(char32_t code) {
  // ナ行 Godan is closed (死ぬ, 往ぬ), so a ぬ-ending verb outside it is a
  // classical bigrade verb the modern conjugation table cannot derive.
  return code == U'ぬ';
}

bool isMonogradeStemFinalKana(char32_t code) {
  constexpr std::array<char32_t, 2> kShiftedRow = {U'ひ', U'へ'};
  return (kana::isIRowCodepoint(code) || kana::isERowCodepoint(code)) && !kana::isCodepointIn(kShiftedRow, code);
}

bool isClassicalAuxiliaryHomographKana(char32_t code) {
  constexpr std::array<char32_t, 6> kAuxiliaryHomographs = {U'す', U'つ', U'ぬ', U'ふ', U'む', U'る'};
  return kana::isCodepointIn(kAuxiliaryHomographs, code);
}

bool isPureKatakana(std::string_view stem) {
  return allCharsMatch(stem, kana::isKatakanaCodepoint);
}

bool isSmallKana(std::string_view ch) {
  return firstCharMatches(ch, kana::isSmallKanaCodepoint);
}

// A-row (あ段) endings for Godan mizenkei detection.
// This is a DELIBERATE subset of the full phonological a-row recognized by
// kana::isARowCodepoint, which also carries だ/ざ/は/ぱ/や — none of which are
// Godan mizenkei endings.
// In particular だ (copula) and は must NOT match here, so this cannot be
// replaced by the kana::isARowCodepoint predicate the way endsWithORow uses
// kana::isORowCodepoint. kana::kMizenkeiEndings plus あ is the source of truth.
bool endsWithARow(std::string_view stem) {
  return utf8::lastChar(stem) == "あ" || endsWithChar(stem, kana::kMizenkeiEndings, kana::kMizenkeiCount);
}

// O-row (お段) ending: the mizenkei a Godan verb takes before volitional う.
// Shares the kana::isORowCodepoint source of truth.
bool endsWithORow(std::string_view stem) {
  return lastCharMatches(stem, kana::isORowCodepoint);
}

bool endsWithURow(std::string_view stem) {
  return lastCharMatches(stem, kana::isURowCodepoint);
}

bool isSingleHiragana(std::string_view text, char32_t codepoint) {
  return text.size() == core::kJapaneseCharBytes && utf8::decode3ByteUtf8At(text, 0) == codepoint;
}

bool isSingleKanjiSurface(std::string_view text) {
  return normalize::utf8Length(text) == 1 && isAllKanji(text);
}

char32_t getVowelForChar(char32_t ch) {
  if (kana::isARowCodepoint(ch)) {
    return U'あ';
  }
  if (kana::isIRowCodepoint(ch)) {
    return U'い';
  }
  if (kana::isURowCodepoint(ch)) {
    return U'う';
  }
  if (kana::isERowCodepoint(ch)) {
    return U'え';
  }
  if (kana::isORowCodepoint(ch)) {
    return U'お';
  }

  // Small kana (ゃゅょ) - treat as their base vowel
  if (ch == U'ゃ')
    return U'あ';
  if (ch == U'ゅ')
    return U'う';
  if (ch == U'ょ')
    return U'お';

  // Default to the character itself if not recognized
  return ch;
}

namespace {

enum class GodanColumn : uint8_t { Base, A, I, E };

using EncodedGodanRow = std::array<std::string, 4>;

const std::array<EncodedGodanRow, Conjugation::kGodanRowCount>& encodedGodanRows() {
  static const std::array<EncodedGodanRow, Conjugation::kGodanRowCount> kEncodedRows = []() {
    std::array<EncodedGodanRow, Conjugation::kGodanRowCount> rows;
    size_t index = 0;
    for (const auto& [type, row] : Conjugation::getGodanRows()) {
      (void)type;
      rows[index++] = {encodeUtf8(row.base_vowel), encodeUtf8(row.a_row), encodeUtf8(row.i_row), encodeUtf8(row.e_row)};
    }
    return rows;
  }();
  return kEncodedRows;
}

char32_t codepointAt(const Conjugation::GodanRow& row, GodanColumn column) {
  switch (column) {
    case GodanColumn::Base:
      return row.base_vowel;
    case GodanColumn::A:
      return row.a_row;
    case GodanColumn::I:
      return row.i_row;
    case GodanColumn::E:
      return row.e_row;
  }
  return 0;
}

size_t columnIndex(GodanColumn column) {
  return static_cast<size_t>(column);
}

std::string_view lookupGodanSuffix(char32_t key, GodanColumn key_column, GodanColumn result_column) {
  const auto& rows = Conjugation::getGodanRows();
  const auto& encoded_rows = encodedGodanRows();
  for (size_t index = 0; index < rows.size(); ++index) {
    if (codepointAt(rows[index].second, key_column) == key) {
      return encoded_rows[index][columnIndex(result_column)];
    }
  }
  return {};
}

VerbType lookupGodanType(char32_t key, GodanColumn key_column) {
  for (const auto& [type, row] : Conjugation::getGodanRows()) {
    if (codepointAt(row, key_column) == key) {
      return type;
    }
  }
  return VerbType::Unknown;
}

}  // namespace

std::string_view godanARowSuffixFromURow(char32_t u_row_cp) {
  return lookupGodanSuffix(u_row_cp, GodanColumn::Base, GodanColumn::A);
}

std::string_view godanIRowSuffixFromURow(char32_t u_row_cp) {
  return lookupGodanSuffix(u_row_cp, GodanColumn::Base, GodanColumn::I);
}

std::string_view godanBaseSuffixFromARow(char32_t a_row_cp) {
  return lookupGodanSuffix(a_row_cp, GodanColumn::A, GodanColumn::Base);
}

VerbType verbTypeFromARowCodepoint(char32_t a_row_cp) {
  return lookupGodanType(a_row_cp, GodanColumn::A);
}

std::string_view godanBaseSuffixFromIRow(char32_t i_row_cp) {
  return lookupGodanSuffix(i_row_cp, GodanColumn::I, GodanColumn::Base);
}

std::string_view godanBaseSuffixFromERow(char32_t e_row_cp) {
  return lookupGodanSuffix(e_row_cp, GodanColumn::E, GodanColumn::Base);
}

VerbType verbTypeFromIRowCodepoint(char32_t i_row_cp) {
  return lookupGodanType(i_row_cp, GodanColumn::I);
}

VerbType verbTypeFromBaseCodepoint(char32_t base_cp) {
  const VerbType verb_type = lookupGodanType(base_cp, GodanColumn::Base);
  // Dictionary-form る is ambiguous between GodanRa and Ichidan, and the
  // dictionary entry intentionally carries no conjugation type. Do not guess.
  return verb_type == VerbType::GodanRa ? VerbType::Unknown : verb_type;
}

bool isMixedHiraganaKanji(std::string_view stem) {
  return anyCharMatches(stem, kana::isHiraganaCodepoint) && anyCharMatches(stem, kana::isKanjiCodepoint);
}

bool isRenyokeiNominalizingSuffix(std::string_view suffix) {
  return utf8::equalsAny(suffix, {"気味", "っけ"});
}

}  // namespace suzume::grammar
