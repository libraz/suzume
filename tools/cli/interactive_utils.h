#ifndef SUZUME_CLI_INTERACTIVE_UTILS_H_
#define SUZUME_CLI_INTERACTIVE_UTILS_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume::cli {

// Trim whitespace from both ends of string
std::string trim(std::string_view str);

// Convert string to uppercase
std::string toUpper(std::string_view str);

// Convert conjugation type to string
std::string conjTypeToString(dictionary::ConjugationType conj_type);

// Parse conjugation type from string
std::optional<dictionary::ConjugationType> parseConjType(const std::string& str);

// Check if POS string is valid and return the POS
std::optional<core::PartOfSpeech> parsePos(const std::string& str);

/** @brief Entry filters shared by `dict list` and the interactive `list` command. */
struct ListFilter {
  std::optional<core::PartOfSpeech> pos;
  std::string pattern;
  size_t limit = 0;  // 0 = unlimited
};

/**
 * @brief Parse --pos, --pattern and --limit (space or = form) from args[first..] into @p filter.
 * @return The error message, or an empty string on success.
 */
std::string parseListFilter(const std::vector<std::string>& args, size_t first, ListFilter* filter);

}  // namespace suzume::cli

#endif  // SUZUME_CLI_INTERACTIVE_UTILS_H_
