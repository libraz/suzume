/**
 * @file tokenizer_utils.cpp
 * @brief Utility functions for tokenizer
 */

#include "tokenizer_utils.h"

namespace suzume::analysis {

size_t charPosToBytePos(const std::vector<char32_t>& codepoints, size_t char_pos) {
  size_t byte_pos = 0;
  for (size_t idx = 0; idx < char_pos && idx < codepoints.size(); ++idx) {
    // Calculate UTF-8 byte length for this codepoint
    char32_t code = codepoints[idx];
    if (code < 0x80) {
      byte_pos += 1;
    } else if (code < 0x800) {
      byte_pos += 2;
    } else if (code < 0x10000) {
      byte_pos += 3;
    } else {
      byte_pos += 4;
    }
  }
  return byte_pos;
}

}  // namespace suzume::analysis
