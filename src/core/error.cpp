#include "error.h"

#include <algorithm>

namespace suzume::core {

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
