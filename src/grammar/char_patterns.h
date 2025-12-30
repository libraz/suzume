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

/**
 * @brief Check if stem contains any kanji character
 * @param stem The stem to check
 * @return True if at least one character is kanji
 *
 * Used to identify pure hiragana/katakana stems (no kanji).
 */
bool containsKanji(std::string_view stem);

/**
 * @brief Check if stem contains any katakana character
 * @param stem The stem to check
 * @return True if at least one character is katakana
 *
 * Used to identify loanword verb/adjective stems (バズる, エモい, etc.)
 */
bool containsKatakana(std::string_view stem);

/**
 * @brief Check if stem consists only of hiragana characters
 * @param stem The stem to check
 * @return True if all characters are hiragana (no kanji, katakana, etc.)
 *
 * Used to identify pure hiragana verb stems which are rare for Godan verbs.
 * Real Godan verbs typically have kanji stems (読む, 書く, 泳ぐ).
 */
bool isPureHiragana(std::string_view stem);

// Onbin endings: い, っ, ん
extern const char* kOnbinEndings[];
extern const size_t kOnbinCount;

// Mizenkei (a-row) endings: か, が, さ, た, な, ば, ま, ら, わ
extern const char* kMizenkeiEndings[];
extern const size_t kMizenkeiCount;

// Renyokei (i-row) endings: き, ぎ, し, ち, に, び, み, り
extern const char* kRenyokeiEndings[];
extern const size_t kRenyokeiCount;

// Full i-row hiragana: み, き, ぎ, し, じ, ち, ぢ, に, び, り, い
// Includes い for u-verbs (会う→会い), じ/ぢ for ichidan verbs (感じる, etc.)
extern const char* kIRowEndings[];
extern const size_t kIRowCount;

// E-row hiragana for Ichidan renyokei: べ, め, せ, け, げ, て, ね, れ, え, で, ぜ, へ, ぺ
extern const char* kERowEndings[];
extern const size_t kERowCount;

/**
 * @brief Check if stem ends with i-row hiragana (godan renyokei markers)
 * @param stem The stem to check
 * @return True if the stem ends with i-row hiragana
 *
 * I-row hiragana includes: み, き, ぎ, し, ち, に, び, り, い
 */
bool endsWithIRow(std::string_view stem);

/**
 * @brief Check if a codepoint is e-row hiragana
 * @param cp Unicode codepoint to check
 * @return True if the codepoint is e-row hiragana
 *
 * E-row includes: え, け, せ, て, ね, へ, め, れ, げ, ぜ, で, べ, ぺ
 */
bool isERowCodepoint(char32_t cp);

/**
 * @brief Check if a codepoint is i-row hiragana
 * @param cp Unicode codepoint to check
 * @return True if the codepoint is i-row hiragana
 *
 * I-row includes: い, き, ぎ, し, ち, に, ひ, び, み, り
 */
bool isIRowCodepoint(char32_t cp);

/**
 * @brief Check if stem ends with onbin marker (音便)
 * @param stem The stem to check
 * @return True if the stem ends with い, っ, or ん
 *
 * Used for te-form and ta-form detection.
 */
bool endsWithOnbin(std::string_view stem);

/**
 * @brief Check if stem ends with renyokei marker (連用形)
 * @param stem The stem to check
 * @return True if the stem ends with i-row or e-row hiragana
 *
 * Combines godan (i-row) and ichidan (e-row) renyokei patterns.
 */
bool endsWithRenyokeiMarker(std::string_view stem);

/**
 * @brief Check if character is small kana (拗音・促音)
 * @param ch UTF-8 encoded character (3 bytes for Japanese)
 * @return True if the character is small kana
 *
 * Small kana includes: ょ, ゃ, ゅ, ぁ, ぃ, ぅ, ぇ, ぉ, っ (hiragana)
 *                      ョ, ャ, ュ, ァ, ィ, ゥ, ェ, ォ, ッ (katakana)
 * These characters cannot start a word independently.
 */
bool isSmallKana(std::string_view ch);

/**
 * @brief Check if string starts with a hiragana character
 * @param s The string to check
 * @return True if the first character is hiragana (U+3040-U+309F)
 *
 * Used to quickly check if a word starts with hiragana.
 * Checks the first 3 bytes of UTF-8 encoded string.
 */
bool startsWithHiragana(std::string_view s);

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_CHAR_PATTERNS_H_
