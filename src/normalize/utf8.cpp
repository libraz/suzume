#include "utf8.h"

namespace suzume::normalize {

char32_t decodeUtf8(std::string_view str, size_t& pos) {
  if (pos >= str.size()) {
    return 0xFFFD;
  }

  auto byte1 = static_cast<unsigned char>(str[pos]);

  // 1-byte sequence (ASCII)
  if ((byte1 & 0x80) == 0) {
    pos += 1;
    return byte1;
  }

  // 2-byte sequence
  if ((byte1 & 0xE0) == 0xC0) {
    if (pos + 1 >= str.size()) {
      pos += 1;
      return 0xFFFD;
    }
    auto byte2 = static_cast<unsigned char>(str[pos + 1]);
    if ((byte2 & 0xC0) != 0x80) {
      pos += 1;
      return 0xFFFD;
    }
    pos += 2;
    return ((byte1 & 0x1F) << 6) | (byte2 & 0x3F);
  }

  // 3-byte sequence
  if ((byte1 & 0xF0) == 0xE0) {
    if (pos + 2 >= str.size()) {
      pos += 1;
      return 0xFFFD;
    }
    auto byte2 = static_cast<unsigned char>(str[pos + 1]);
    auto byte3 = static_cast<unsigned char>(str[pos + 2]);
    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80) {
      pos += 1;
      return 0xFFFD;
    }
    pos += 3;
    return ((byte1 & 0x0F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F);
  }

  // 4-byte sequence
  if ((byte1 & 0xF8) == 0xF0) {
    if (pos + 3 >= str.size()) {
      pos += 1;
      return 0xFFFD;
    }
    auto byte2 = static_cast<unsigned char>(str[pos + 1]);
    auto byte3 = static_cast<unsigned char>(str[pos + 2]);
    auto byte4 = static_cast<unsigned char>(str[pos + 3]);
    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || (byte4 & 0xC0) != 0x80) {
      pos += 1;
      return 0xFFFD;
    }
    pos += 4;
    return ((byte1 & 0x07) << 18) | ((byte2 & 0x3F) << 12) | ((byte3 & 0x3F) << 6) |
           (byte4 & 0x3F);
  }

  pos += 1;
  return 0xFFFD;
}

void encodeUtf8(char32_t codepoint, std::string& out) {
  if (codepoint < 0x80) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
}

size_t utf8Length(std::string_view str) {
  size_t count = 0;
  size_t pos = 0;
  while (pos < str.size()) {
    decodeUtf8(str, pos);
    ++count;
  }
  return count;
}

size_t charToByteOffset(std::string_view str, size_t char_index) {
  size_t pos = 0;
  for (size_t i = 0; i < char_index && pos < str.size(); ++i) {
    decodeUtf8(str, pos);
  }
  return pos;
}

size_t byteToCharOffset(std::string_view str, size_t byte_offset) {
  size_t count = 0;
  size_t pos = 0;
  while (pos < byte_offset && pos < str.size()) {
    decodeUtf8(str, pos);
    ++count;
  }
  return count;
}

std::string_view utf8Substr(std::string_view str, size_t start, size_t length) {
  size_t start_byte = charToByteOffset(str, start);
  size_t end_byte = charToByteOffset(str, start + length);
  return str.substr(start_byte, end_byte - start_byte);
}

bool isValidUtf8(std::string_view str) {
  size_t pos = 0;
  while (pos < str.size()) {
    size_t old_pos = pos;
    char32_t codepoint = decodeUtf8(str, pos);
    if (codepoint == 0xFFFD && pos == old_pos + 1) {
      // Invalid sequence detected
      return false;
    }
  }
  return true;
}

std::vector<char32_t> toCodepoints(std::string_view str) {
  std::vector<char32_t> result;
  size_t pos = 0;
  while (pos < str.size()) {
    result.push_back(decodeUtf8(str, pos));
  }
  return result;
}

std::string fromCodepoints(const std::vector<char32_t>& codepoints) {
  std::string result;
  result.reserve(codepoints.size() * 3);  // Estimate for Japanese
  for (char32_t codepoint : codepoints) {
    encodeUtf8(codepoint, result);
  }
  return result;
}

std::string encodeRange(const std::vector<char32_t>& codepoints,
                        size_t start, size_t end) {
  if (start >= codepoints.size() || end > codepoints.size() || start >= end) {
    return "";
  }
  std::string result;
  result.reserve((end - start) * 3);  // Estimate for Japanese
  for (size_t idx = start; idx < end; ++idx) {
    encodeUtf8(codepoints[idx], result);
  }
  return result;
}

}  // namespace suzume::normalize
