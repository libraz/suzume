/**
 * @file tokenizer_utils.h
 * @brief Utility functions for tokenizer
 */

#ifndef SUZUME_ANALYSIS_TOKENIZER_UTILS_H_
#define SUZUME_ANALYSIS_TOKENIZER_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace suzume::analysis {

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
