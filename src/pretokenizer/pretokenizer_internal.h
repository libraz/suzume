#ifndef SUZUME_PRETOKENIZER_PRETOKENIZER_INTERNAL_H_
#define SUZUME_PRETOKENIZER_PRETOKENIZER_INTERNAL_H_

#include <cstddef>
#include <string>
#include <string_view>

#include "pretokenizer/pretokenizer.h"

namespace suzume::pretokenizer::pretokenizer_detail {

struct IntegerScan {
  size_t end;
  size_t digit_count;
  int value;

  bool empty() const { return digit_count == 0; }
};

bool isAsciiDigit(char chr);
bool isAsciiAlpha(char chr);
bool isAsciiAlnum(char chr);
bool isMonthPlaceCounterPrefix(char32_t codepoint);
IntegerScan scanInteger(std::string_view text, size_t pos);
size_t scanDigits(std::string_view text, size_t pos);
bool hasAsciiRunLeftNeighbor(std::string_view text, size_t pos, std::string_view punctuation);
bool startsWithCI(std::string_view text, size_t pos, std::string_view prefix);
bool absorbsPeriodKan(std::string_view text, size_t pos_after_kan);
bool hasIntervalSuffix(std::string_view text, size_t pos);
bool leavesOddKanjiRun(std::string_view text, size_t pos);
void setTokenFromRange(PreToken& token, std::string_view text, size_t start, size_t end, PreTokenType type,
                       core::PartOfSpeech pos);

}  // namespace suzume::pretokenizer::pretokenizer_detail

#endif  // SUZUME_PRETOKENIZER_PRETOKENIZER_INTERNAL_H_
