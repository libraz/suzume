/**
 * @file char_patterns.cpp
 * @brief Character pattern utilities for Japanese verb/adjective analysis
 */

#include "char_patterns.h"

#include <unordered_set>

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
const char* kIRowEndings[] = {"み", "き", "ぎ", "し", "ち",
                               "に", "び", "り", "い"};
const size_t kIRowCount = 9;

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
    const unsigned char* ptr =
        reinterpret_cast<const unsigned char*>(stem.data() + pos);
    if ((ptr[0] & 0xF0) != 0xE0) {
      return false;  // Not a 3-byte UTF-8 sequence
    }
    char32_t codepoint =
        ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);
    // CJK Unified Ideographs: U+4E00-U+9FFF
    // CJK Extension A: U+3400-U+4DBF
    bool is_kanji = (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||
                    (codepoint >= 0x3400 && codepoint <= 0x4DBF);
    if (!is_kanji) {
      return false;
    }
    pos += core::kJapaneseCharBytes;
  }
  return true;
}

bool endsWithKanji(std::string_view stem) {
  if (stem.size() < core::kJapaneseCharBytes) {
    return false;
  }
  // Decode last UTF-8 character
  // Kanji in UTF-8 is 3 bytes: E4-E9 xx xx (U+4E00-U+9FFF)
  const unsigned char* ptr =
      reinterpret_cast<const unsigned char*>(stem.data() + stem.size() - core::kJapaneseCharBytes);
  if ((ptr[0] & 0xF0) == 0xE0) {
    // 3-byte UTF-8 sequence
    char32_t codepoint =
        ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);
    // CJK Unified Ideographs: U+4E00-U+9FFF
    // CJK Extension A: U+3400-U+4DBF
    return (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||
           (codepoint >= 0x3400 && codepoint <= 0x4DBF);
  }
  return false;
}

bool containsKanji(std::string_view stem) {
  if (stem.empty()) {
    return false;
  }
  size_t pos = 0;
  while (pos + core::kJapaneseCharBytes <= stem.size()) {
    const unsigned char* ptr =
        reinterpret_cast<const unsigned char*>(stem.data() + pos);
    if ((ptr[0] & 0xF0) == 0xE0) {
      // 3-byte UTF-8 sequence
      char32_t codepoint =
          ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);
      // CJK Unified Ideographs: U+4E00-U+9FFF
      // CJK Extension A: U+3400-U+4DBF
      bool is_kanji = (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||
                      (codepoint >= 0x3400 && codepoint <= 0x4DBF);
      if (is_kanji) {
        return true;
      }
      pos += core::kJapaneseCharBytes;
    } else {
      // Skip non-3-byte sequences
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
    const unsigned char* ptr =
        reinterpret_cast<const unsigned char*>(stem.data() + pos);
    if ((ptr[0] & 0xF0) == 0xE0) {
      // 3-byte UTF-8 sequence
      char32_t codepoint =
          ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);
      // Katakana: U+30A0-U+30FF
      // Katakana Phonetic Extensions: U+31F0-U+31FF
      bool is_katakana = (codepoint >= 0x30A0 && codepoint <= 0x30FF) ||
                         (codepoint >= 0x31F0 && codepoint <= 0x31FF);
      if (is_katakana) {
        return true;
      }
      pos += core::kJapaneseCharBytes;
    } else {
      // Skip non-3-byte sequences
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
    const unsigned char* ptr =
        reinterpret_cast<const unsigned char*>(stem.data() + pos);
    if ((ptr[0] & 0xF0) != 0xE0) {
      return false;  // Not a 3-byte UTF-8 sequence (not hiragana)
    }
    char32_t codepoint =
        ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);
    // Hiragana: U+3040-U+309F
    bool is_hiragana = (codepoint >= 0x3040 && codepoint <= 0x309F);
    if (!is_hiragana) {
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

}  // namespace suzume::grammar
