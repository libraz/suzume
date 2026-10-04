#ifndef SUZUME_NORMALIZE_UTF8_H_
#define SUZUME_NORMALIZE_UTF8_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace suzume::normalize {

/**
 * @brief UTF-8 utility functions (ICU-independent)
 */

/**
 * @brief Decode one UTF-8 character
 * @param str UTF-8 string
 * @param pos Current position (updated to next character)
 * @return Unicode codepoint, or 0xFFFD on error
 */
char32_t decodeUtf8(std::string_view str, size_t& pos);

/**
 * @brief Encode one codepoint to UTF-8
 * @param codepoint Unicode codepoint
 * @param out Output string
 */
void encodeUtf8(char32_t codepoint, std::string& out);

/**
 * @brief Encode one codepoint to UTF-8
 * @param codepoint Unicode codepoint
 * @return UTF-8 encoded string
 */
inline std::string encodeUtf8(char32_t codepoint) {
  std::string result;
  encodeUtf8(codepoint, result);
  return result;
}

/**
 * @brief Get UTF-8 string length in characters
 * @param str UTF-8 string
 * @return Number of Unicode codepoints
 */
size_t utf8Length(std::string_view str);

/**
 * @brief Get byte offset for character index
 * @param str UTF-8 string
 * @param char_index Character index
 * @return Byte offset
 */
size_t charToByteOffset(std::string_view str, size_t char_index);

/**
 * @brief Get character index for byte offset
 * @param str UTF-8 string
 * @param byte_offset Byte offset
 * @return Character index
 */
size_t byteToCharOffset(std::string_view str, size_t byte_offset);

/**
 * @brief Extract substring by character indices
 * @param str UTF-8 string
 * @param start Start character index
 * @param length Number of characters
 * @return Substring
 */
std::string_view utf8Substr(std::string_view str, size_t start, size_t length);

/**
 * @brief Find the start of the UTF-8 character containing a byte position
 * @param str UTF-8 string
 * @param pos Byte position (must be < str.size())
 * @return Byte offset of the nearest non-continuation byte at or before pos
 */
size_t findUtf8Boundary(std::string_view str, size_t pos);

/**
 * @brief Rebuild a string with its last character swapped for another one
 * @param str UTF-8 string
 * @param replacement UTF-8 text to put in place of the last character
 * @return The string with its final character replaced, or just the
 *         replacement when the input is empty
 */
std::string replaceFinalChar(std::string_view str, std::string_view replacement);

/**
 * @brief Join two text fragments into one string
 * @param head Leading fragment
 * @param tail Trailing fragment
 * @return head followed by tail, in a single exact-size allocation
 *
 * @note This is the single owner of "rebuild a surface from a stem and an
 *       ending". Callers holding views must not write
 *       `std::string(head) + std::string(tail)`: that materializes two
 *       temporaries and pulls the string concatenation machinery into every
 *       call site.
 */
std::string concat(std::string_view head, std::string_view tail);

/**
 * @brief Join three text fragments into one string
 * @see concat(std::string_view, std::string_view)
 */
std::string concat(std::string_view head, std::string_view middle, std::string_view tail);

/**
 * @brief Check if string is valid UTF-8
 * @param str String to check
 * @return true if valid UTF-8
 */
bool isValidUtf8(std::string_view str);

/**
 * @brief Convert string to vector of codepoints
 * @param str UTF-8 string
 * @return Vector of codepoints
 */
std::vector<char32_t> toCodepoints(std::string_view str);

/**
 * @brief Convert vector of codepoints to UTF-8 string
 * @param codepoints Vector of codepoints
 * @return UTF-8 string
 */
std::string fromCodepoints(const std::vector<char32_t>& codepoints);

/**
 * @brief Encode a range of codepoints to UTF-8 string (no intermediate vector)
 * @param codepoints Vector of codepoints
 * @param start Start index (inclusive)
 * @param end End index (exclusive)
 * @return UTF-8 string, or empty if range is invalid
 */
std::string encodeRange(const std::vector<char32_t>& codepoints, size_t start, size_t end);

/**
 * @brief Namespace alias for convenience
 */
namespace utf8 {

/**
 * @brief Decode UTF-8 string to codepoints
 */
inline std::vector<char32_t> decode(std::string_view str) {
  return toCodepoints(str);
}

/**
 * @brief Encode codepoints to UTF-8 string
 */
inline std::string encode(const std::vector<char32_t>& codepoints) {
  return fromCodepoints(codepoints);
}

}  // namespace utf8

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_UTF8_H_
