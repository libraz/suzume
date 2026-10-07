#include "error.h"

#include <algorithm>
#include <iterator>

namespace suzume::core {

std::string_view errorCodeToString(ErrorCode code) {
  static constexpr std::string_view kNames[] = {
      "Success",    "InvalidUtf8", "DictionaryLoadFailed", "FileNotFound",
      "ParseError", "OutOfMemory", "InvalidInput",         "InternalError",
  };
  static_assert(std::size(kNames) == static_cast<size_t>(ErrorCode::InternalError) + 1,
                "kNames must cover every ErrorCode value");
  const auto idx = static_cast<size_t>(code);
  return idx < std::size(kNames) ? kNames[idx] : "InternalError";
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
