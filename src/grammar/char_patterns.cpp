/**
 * @file char_patterns.cpp
 * @brief Character pattern utilities for Japanese verb/adjective analysis
 */

#include "char_patterns.h"

#include <unordered_set>

#include "core/kana_constants.h"
#include "core/utf8_constants.h"

namespace suzume::grammar {

// Onbin endings: い, っ, ん
const char* kOnbinEndings[] = {"い", "っ", "ん"};
const size_t kOnbinCount = 3;

// Mizenkei (a-row) endings: か, が, さ, た, な, ば, ま, ら, わ
const char* kMizenkeiEndings[] = {"か", "が", "さ", "た",  "な",
                                   "ば", "ま", "ら", "わ"};
const size_t kMizenkeiCount = 9;

// Renyokei (i-row) endings: き, ぎ, し, ち, に, び, み, り
const char* kRenyokeiEndings[] = {"き", "ぎ", "し", "ち",
                                   "に", "び", "み", "り"};
const size_t kRenyokeiCount = 8;

// Full i-row hiragana including い for u-verb stems
// Includes voiced variants じ (from し) and ぢ (from ち) for ichidan verbs
const char* kIRowEndings[] = {"み", "き", "ぎ", "し", "じ", "ち", "ぢ",
                               "に", "び", "り", "い"};
const size_t kIRowCount = 11;

// E-row hiragana for Ichidan renyokei
const char* kERowEndings[] = {"べ", "め", "せ", "け", "げ", "て", "ね",
                               "れ", "え", "で", "ぜ", "へ", "ぺ"};
const size_t kERowCount = 13;

bool endsWithIRow(std::string_view stem) {
  return endsWithChar(stem, kIRowEndings, kIRowCount);
}

bool endsWithERow(std::string_view stem) {
  return endsWithChar(stem, kERowEndings, kERowCount);
}

bool endsWithOnbin(std::string_view stem) {
  return endsWithChar(stem, kOnbinEndings, kOnbinCount);
}

bool endsWithRenyokeiMarker(std::string_view stem) {
  return endsWithIRow(stem) || endsWithERow(stem);
}

bool isERowCodepoint(char32_t cp) {
  return kana::isERowCodepoint(cp);
}

bool isIRowCodepoint(char32_t cp) {
  return kana::isIRowCodepoint(cp);
}

bool isARowCodepoint(char32_t cp) {
  return kana::isARowCodepoint(cp);
}

bool endsWithChar(std::string_view stem, const char* chars[], size_t count) {
  if (stem.size() < core::kJapaneseCharBytes) {
    return false;
  }
  std::string_view last = stem.substr(stem.size() - core::kJapaneseCharBytes);
  for (size_t idx = 0; idx < count; ++idx) {
    if (last == chars[idx]) {
      return true;
    }
  }
  return false;
}

bool isAllKanji(std::string_view stem) {
  if (stem.empty()) {
    return false;
  }
  size_t pos = 0;
  while (pos < stem.size()) {
    if (!utf8::is3ByteUtf8At(stem, pos)) {
      return false;
    }
    char32_t codepoint = utf8::decode3ByteUtf8At(stem, pos);
    if (!kana::isKanjiCodepoint(codepoint)) {
      return false;
    }
    pos += core::kJapaneseCharBytes;
  }
  return true;
}

bool endsWithKanji(std::string_view stem) {
  char32_t cp = utf8::decodeLastChar(stem);
  return cp != 0 && kana::isKanjiCodepoint(cp);
}

bool containsKanji(std::string_view stem) {
  if (stem.empty()) {
    return false;
  }
  size_t pos = 0;
  while (pos + core::kJapaneseCharBytes <= stem.size()) {
    if (utf8::is3ByteUtf8At(stem, pos)) {
      char32_t codepoint = utf8::decode3ByteUtf8At(stem, pos);
      if (kana::isKanjiCodepoint(codepoint)) {
        return true;
      }
      pos += core::kJapaneseCharBytes;
    } else {
      pos += 1;
    }
  }
  return false;
}

bool containsKatakana(std::string_view stem) {
  if (stem.empty()) {
    return false;
  }
  size_t pos = 0;
  while (pos + core::kJapaneseCharBytes <= stem.size()) {
    if (utf8::is3ByteUtf8At(stem, pos)) {
      char32_t codepoint = utf8::decode3ByteUtf8At(stem, pos);
      if (kana::isKatakanaCodepoint(codepoint)) {
        return true;
      }
      pos += core::kJapaneseCharBytes;
    } else {
      pos += 1;
    }
  }
  return false;
}

bool isPureHiragana(std::string_view stem) {
  if (stem.empty()) {
    return false;
  }
  size_t pos = 0;
  while (pos < stem.size()) {
    if (!utf8::is3ByteUtf8At(stem, pos)) {
      return false;
    }
    char32_t codepoint = utf8::decode3ByteUtf8At(stem, pos);
    if (!kana::isHiraganaCodepoint(codepoint)) {
      return false;
    }
    pos += core::kJapaneseCharBytes;
  }
  return true;
}

bool isSmallKana(std::string_view ch) {
  // Static set for O(1) lookup - initialized once
  static const std::unordered_set<std::string_view> kSmallKana = {
      // Hiragana small kana (拗音・促音)
      "ょ", "ゃ", "ゅ", "ぁ", "ぃ", "ぅ", "ぇ", "ぉ", "っ",
      // Katakana small kana
      "ョ", "ャ", "ュ", "ァ", "ィ", "ゥ", "ェ", "ォ", "ッ"
  };
  return kSmallKana.count(ch) > 0;
}

bool startsWithHiragana(std::string_view s) {
  char32_t cp = utf8::decodeFirstChar(s);
  return cp != 0 && kana::isHiraganaCodepoint(cp);
}

// A-row (あ段) endings for verb mizenkei detection
// Includes all mizenkei endings plus あ for completeness
// Note: Slightly broader than kMizenkeiEndings to catch edge cases
const char* kARowEndings[] = {"あ", "か", "が", "さ", "た",
                               "な", "ば", "ま", "ら", "わ"};
const size_t kARowCount = 10;

bool endsWithARow(std::string_view stem) {
  return endsWithChar(stem, kARowEndings, kARowCount);
}

char32_t getVowelForChar(char32_t ch) {
  // Hiragana vowel rows:
  // あ row (a): あ か が さ ざ た だ な は ば ぱ ま や ら わ
  // い row (i): い き ぎ し じ ち ぢ に ひ び ぴ み り
  // う row (u): う く ぐ す ず つ づ ぬ ふ ぶ ぷ む ゆ る
  // え row (e): え け げ せ ぜ て で ね へ べ ぺ め れ
  // お row (o): お こ ご そ ぞ と ど の ほ ぼ ぽ も よ ろ を

  // A-row characters
  if (ch == U'あ' || ch == U'か' || ch == U'が' || ch == U'さ' || ch == U'ざ' ||
      ch == U'た' || ch == U'だ' || ch == U'な' || ch == U'は' || ch == U'ば' ||
      ch == U'ぱ' || ch == U'ま' || ch == U'や' || ch == U'ら' || ch == U'わ') {
    return U'あ';
  }
  // I-row characters
  if (ch == U'い' || ch == U'き' || ch == U'ぎ' || ch == U'し' || ch == U'じ' ||
      ch == U'ち' || ch == U'ぢ' || ch == U'に' || ch == U'ひ' || ch == U'び' ||
      ch == U'ぴ' || ch == U'み' || ch == U'り') {
    return U'い';
  }
  // U-row characters
  if (ch == U'う' || ch == U'く' || ch == U'ぐ' || ch == U'す' || ch == U'ず' ||
      ch == U'つ' || ch == U'づ' || ch == U'ぬ' || ch == U'ふ' || ch == U'ぶ' ||
      ch == U'ぷ' || ch == U'む' || ch == U'ゆ' || ch == U'る') {
    return U'う';
  }
  // E-row characters
  if (ch == U'え' || ch == U'け' || ch == U'げ' || ch == U'せ' || ch == U'ぜ' ||
      ch == U'て' || ch == U'で' || ch == U'ね' || ch == U'へ' || ch == U'べ' ||
      ch == U'ぺ' || ch == U'め' || ch == U'れ') {
    return U'え';
  }
  // O-row characters
  if (ch == U'お' || ch == U'こ' || ch == U'ご' || ch == U'そ' || ch == U'ぞ' ||
      ch == U'と' || ch == U'ど' || ch == U'の' || ch == U'ほ' || ch == U'ぼ' ||
      ch == U'ぽ' || ch == U'も' || ch == U'よ' || ch == U'ろ' || ch == U'を') {
    return U'お';
  }

  // Small kana (ゃゅょ) - treat as their base vowel
  if (ch == U'ゃ') return U'あ';
  if (ch == U'ゅ') return U'う';
  if (ch == U'ょ') return U'お';

  // Default to the character itself if not recognized
  return ch;
}

std::string_view godanBaseSuffixFromARow(char32_t a_row_cp) {
  switch (a_row_cp) {
    case U'か': return "く";  // GodanKa: 書く
    case U'が': return "ぐ";  // GodanGa: 泳ぐ
    case U'さ': return "す";  // GodanSa: 話す
    case U'た': return "つ";  // GodanTa: 持つ
    case U'な': return "ぬ";  // GodanNa: 死ぬ
    case U'ば': return "ぶ";  // GodanBa: 遊ぶ
    case U'ま': return "む";  // GodanMa: 読む
    case U'ら': return "る";  // GodanRa: 取る
    case U'わ': return "う";  // GodanWa: 買う
    default: return "";
  }
}

VerbType verbTypeFromARowCodepoint(char32_t a_row_cp) {
  switch (a_row_cp) {
    case U'か': return VerbType::GodanKa;
    case U'が': return VerbType::GodanGa;
    case U'さ': return VerbType::GodanSa;
    case U'た': return VerbType::GodanTa;
    case U'な': return VerbType::GodanNa;
    case U'ば': return VerbType::GodanBa;
    case U'ま': return VerbType::GodanMa;
    case U'ら': return VerbType::GodanRa;
    case U'わ': return VerbType::GodanWa;
    default: return VerbType::Unknown;
  }
}

}  // namespace suzume::grammar
