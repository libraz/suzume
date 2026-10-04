#include "error.h"

#include <algorithm>

namespace suzume::core {

std::string_view errorCodeToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::Success:
      return "Success";
    case ErrorCode::InvalidUtf8:
      return "InvalidUtf8";
    case ErrorCode::DictionaryLoadFailed:
      return "DictionaryLoadFailed";
    case ErrorCode::FileNotFound:
      return "FileNotFound";
    case ErrorCode::ParseError:
      return "ParseError";
    case ErrorCode::OutOfMemory:
      return "OutOfMemory";
    case ErrorCode::InvalidInput:
      return "InvalidInput";
    case ErrorCode::InternalError:
    default:
      return "InternalError";
  }
}

std::string decimalDigits(size_t value) {
  std::string digits;
  do {
    digits.push_back(static_cast<char>('0' + (value % 10)));
    value /= 10;
  } while (value != 0);
  std::reverse(digits.begin(), digits.end());
  return digits;
}

}  // namespace suzume::core
