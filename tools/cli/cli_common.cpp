#include "cli_common.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>

#include "normalize/utf8.h"
#include "suzume.h"

namespace suzume::cli {

void printError(std::string_view message) {
  std::cerr << "error: " << message << "\n";
}

void printWarning(std::string_view message) {
  std::cerr << "warning: " << message << "\n";
}

void printInfo(std::string_view message) {
  std::cerr << "info: " << message << "\n";
}

bool parseSizeOption(std::string_view value, size_t* out) {
  if (out == nullptr || value.empty()) {
    return false;
  }
  for (char chr : value) {
    if (chr < '0' || chr > '9') {
      return false;
    }
  }

  try {
    size_t parsed_len = 0;
    unsigned long long parsed = std::stoull(std::string(value), &parsed_len, 10);
    if (parsed_len != value.size()) {
      return false;
    }
    // std::stoull already rejects values exceeding unsigned long long. Only when
    // size_t is narrower than unsigned long long (e.g. ILP32) can a parsed value
    // still overflow size_t; on LP64 the two share a width, so this guard is
    // compiled out rather than left as an always-false comparison.
    if constexpr (std::numeric_limits<size_t>::max() < std::numeric_limits<unsigned long long>::max()) {
      if (parsed > std::numeric_limits<size_t>::max()) {
        return false;
      }
    }
    *out = static_cast<size_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

bool limitReached(size_t count, size_t limit) {
  return limit > 0 && count >= limit;
}

std::string jsonEscape(std::string_view value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');

  size_t idx = 0;
  while (idx < value.size()) {
    unsigned char chr = static_cast<unsigned char>(value[idx]);

    // ASCII range: apply JSON escaping for control/special characters.
    if (chr < 0x80) {
      switch (chr) {
        case '"':
          out << "\\\"";
          break;
        case '\\':
          out << "\\\\";
          break;
        case '\b':
          out << "\\b";
          break;
        case '\f':
          out << "\\f";
          break;
        case '\n':
          out << "\\n";
          break;
        case '\r':
          out << "\\r";
          break;
        case '\t':
          out << "\\t";
          break;
        default:
          if (chr < 0x20) {
            out << "\\u" << std::setw(4) << static_cast<int>(chr);
          } else {
            out << static_cast<char>(chr);
          }
          break;
      }
      ++idx;
      continue;
    }

    // Multi-byte UTF-8: determine the expected sequence length from the lead
    // byte, then verify every continuation byte before passing it through.
    size_t seq_len = 0;
    bool scalar_prefix_valid = false;
    if (chr >= 0xC2 && chr <= 0xDF) {
      seq_len = 2;
      scalar_prefix_valid = true;
    } else if (chr >= 0xE0 && chr <= 0xEF) {
      seq_len = 3;
      scalar_prefix_valid = true;
    } else if (chr >= 0xF0 && chr <= 0xF4) {
      seq_len = 4;
      scalar_prefix_valid = true;
    }

    bool valid = scalar_prefix_valid && idx + seq_len <= value.size();
    if (valid) {
      for (size_t off = 1; off < seq_len; ++off) {
        if ((static_cast<unsigned char>(value[idx + off]) & 0xC0) != 0x80) {
          valid = false;
          break;
        }
      }
    }
    if (valid && seq_len == 3) {
      const auto second = static_cast<unsigned char>(value[idx + 1]);
      valid = !((chr == 0xE0 && second < 0xA0) || (chr == 0xED && second >= 0xA0));
    }
    if (valid && seq_len == 4) {
      const auto second = static_cast<unsigned char>(value[idx + 1]);
      valid = !((chr == 0xF0 && second < 0x90) || (chr == 0xF4 && second > 0x8F));
    }

    if (valid) {
      // Emit the validated multi-byte sequence unchanged.
      for (size_t off = 0; off < seq_len; ++off) {
        out << value[idx + off];
      }
      idx += seq_len;
    } else {
      // Replace a stray/invalid byte with the Unicode replacement character so
      // the output stays well-formed JSON.
      out << "\\ufffd";
      ++idx;
    }
  }

  return out.str();
}

std::string tabEscape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char chr : value) {
    switch (chr) {
      case '\\':
        escaped += "\\\\";
        break;
      case '\t':
        escaped += "\\t";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped += chr;
        break;
    }
  }
  return escaped;
}

void stripUtf8Bom(std::string* value) {
  if (value != nullptr && normalize::startsWithUtf8Bom(*value)) {
    value->erase(0, normalize::kUtf8Bom.size());
  }
}

core::Expected<size_t, core::Error> loadUserDictionaryPath(Suzume* analyzer, const std::string& path) {
  if (analyzer == nullptr) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Analyzer must not be null"));
  }

  std::string extension;
  if (path.size() >= 4) {
    extension = path.substr(path.size() - 4);
    for (char& chr : extension) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
  }
  if (extension != ".dic") {
    return analyzer->loadUserDictionaryResult(path);
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::FileNotFound, "Failed to open binary dictionary file: " + path));
  }
  const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (file.bad()) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::DictionaryLoadFailed, "Failed to read binary dictionary file: " + path));
  }
  return analyzer->loadBinaryDictionaryResult(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool hasExtension(std::string_view path, std::string_view ext) {
  if (path.size() < ext.size()) {
    return false;
  }
  const std::string_view suffix = path.substr(path.size() - ext.size());
  for (size_t idx = 0; idx < ext.size(); ++idx) {
    const auto suffix_chr = static_cast<unsigned char>(suffix[idx]);
    const auto expected_chr = static_cast<unsigned char>(ext[idx]);
    if (std::tolower(suffix_chr) != std::tolower(expected_chr)) {
      return false;
    }
  }
  return true;
}

std::string swapOrAppendExtension(std::string_view path, std::string_view from_ext, std::string_view to_ext) {
  if (hasExtension(path, from_ext)) {
    return std::string(path.substr(0, path.size() - from_ext.size())) + std::string(to_ext);
  }
  return std::string(path) + std::string(to_ext);
}

core::Expected<bool, core::Error> validateWildcardPattern(std::string_view pattern) {
  constexpr size_t kMaxWildcardStars = 64;
  size_t star_count = 0;
  for (char chr : pattern) {
    if (chr == '*' && ++star_count > kMaxWildcardStars) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Wildcard pattern has too many '*' characters (maximum 64)"));
    }
  }
  return true;
}

bool wildcardMatches(std::string_view pattern, std::string_view value) {
  size_t pattern_pos = 0;
  size_t value_pos = 0;
  size_t star_pos = std::string_view::npos;
  size_t retry_value_pos = 0;

  while (value_pos < value.size()) {
    if (pattern_pos < pattern.size() && (pattern[pattern_pos] == '?' || pattern[pattern_pos] == value[value_pos])) {
      ++pattern_pos;
      ++value_pos;
    } else if (pattern_pos < pattern.size() && pattern[pattern_pos] == '*') {
      star_pos = pattern_pos++;
      retry_value_pos = value_pos;
    } else if (star_pos != std::string_view::npos) {
      pattern_pos = star_pos + 1;
      value_pos = ++retry_value_pos;
    } else {
      return false;
    }
  }

  while (pattern_pos < pattern.size() && pattern[pattern_pos] == '*') {
    ++pattern_pos;
  }
  return pattern_pos == pattern.size();
}

bool isTerminal() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

std::string getVersionString() {
  return Suzume::version();
}

void printVersion() {
  std::cout << "suzume-cli " << getVersionString() << "\n";
  std::cout << "Japanese tokenizer\n";
}

}  // namespace suzume::cli
