/**
 * @file tokenizer_utils.h
 * @brief Utility functions for tokenizer
 */

#ifndef SUZUME_ANALYSIS_TOKENIZER_UTILS_H_
#define SUZUME_ANALYSIS_TOKENIZER_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "normalize/char_type.h"

namespace suzume::analysis {

/**
 * @brief Find end position of consecutive characters of a given type
 *
 * Scans from start_pos until one of: bounds exceeded, max_len reached,
 * or character type changes.
 *
 * @param char_types Character type array
 * @param start_pos Starting position
 * @param max_len Maximum characters to scan
 * @param target_type Character type to match
 * @return End position (exclusive)
 *
 * Example:
 *   // Find up to 3 kanji starting at start_pos
 *   size_t kanji_end = findCharRegionEnd(char_types, start_pos, 3, CharType::Kanji);
 */
inline size_t findCharRegionEnd(
    const std::vector<normalize::CharType>& char_types,
    size_t start_pos,
    size_t max_len,
    normalize::CharType target_type) {
  size_t end = start_pos;
  while (end < char_types.size() &&
         end - start_pos < max_len &&
         char_types[end] == target_type) {
    ++end;
  }
  return end;
}

/**
 * @brief Convert character position to byte position in UTF-8 text
 *
 * Given a sequence of Unicode codepoints and a character position,
 * calculate the corresponding byte position in the UTF-8 encoded string.
 *
 * @param codepoints Vector of Unicode codepoints
 * @param char_pos Character position (0-indexed)
 * @return Byte position in UTF-8 encoded string
 */
size_t charPosToBytePos(const std::vector<char32_t>& codepoints, size_t char_pos);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_TOKENIZER_UTILS_H_
