/**
 * @file char_patterns.h
 * @brief Character pattern utilities for Japanese verb/adjective analysis
 */

#ifndef SUZUME_GRAMMAR_CHAR_PATTERNS_H_
#define SUZUME_GRAMMAR_CHAR_PATTERNS_H_

#include <string_view>

namespace suzume::grammar {

/**
 * @brief Check if stem ends with e-row hiragana (common Ichidan endings)
 * @param stem The stem to check
 * @return True if the stem ends with e-row hiragana
 *
 * E-row hiragana includes: え, け, せ, て, ね, へ, め, れ, べ, ぺ, げ, ぜ, で
 */
bool endsWithERow(std::string_view stem);

/**
 * @brief Check if stem ends with any of the specified characters
 * @param stem The stem to check
 * @param chars Array of character strings to match
 * @param count Number of characters in the array
 * @return True if the stem ends with any of the characters
 */
bool endsWithChar(std::string_view stem, const char* chars[], size_t count);

/**
 * @brief Check if entire stem consists only of kanji (no hiragana/katakana)
 * @param stem The stem to check
 * @return True if the stem contains only kanji characters
 *
 * Used to identify サ変名詞 stems that shouldn't be i-adjective stems.
 * CJK Unified Ideographs: U+4E00-U+9FFF and Extension A: U+3400-U+4DBF
 */
bool isAllKanji(std::string_view stem);

/**
 * @brief Check if stem ends with a kanji character
 * @param stem The stem to check
 * @return True if the last character is kanji
 *
 * Used to identify potential サ変 verb stems (勉強, 準備, etc.)
 */
bool endsWithKanji(std::string_view stem);

// Onbin endings: い, っ, ん
extern const char* kOnbinEndings[];
extern const size_t kOnbinCount;

// Mizenkei (a-row) endings: か, が, さ, た, な, ば, ま, ら, わ
extern const char* kMizenkeiEndings[];
extern const size_t kMizenkeiCount;

// Renyokei (i-row) endings: き, ぎ, し, ち, に, び, み, り
extern const char* kRenyokeiEndings[];
extern const size_t kRenyokeiCount;

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_CHAR_PATTERNS_H_
