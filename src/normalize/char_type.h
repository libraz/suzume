#ifndef SUZUME_NORMALIZE_CHAR_TYPE_H_
#define SUZUME_NORMALIZE_CHAR_TYPE_H_

#include <cstdint>
#include <string_view>

namespace suzume::normalize {

/**
 * @brief Character type classification
 */
enum class CharType : uint8_t {
  Kanji,     // 漢字
  Hiragana,  // ひらがな
  Katakana,  // カタカナ
  Alphabet,  // 英字
  Digit,     // 数字
  Symbol,    // 記号
  Emoji,     // 絵文字
  Unknown    // 不明
};

/**
 * @brief Classify a Unicode codepoint
 * @param codepoint Unicode codepoint
 * @return Character type
 */
CharType classifyChar(char32_t codepoint);

/**
 * @brief Convert character type to string
 */
std::string_view charTypeToString(CharType type);

/**
 * @brief Check if character type can be combined
 * @param first_type First character type
 * @param second_type Second character type
 * @return true if can be combined as unknown word
 */
bool canCombine(CharType first_type, CharType second_type);

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_CHAR_TYPE_H_
