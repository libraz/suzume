#include "interactive_utils.h"

#include <cctype>

#include "cli_common.h"

namespace suzume::cli {

std::string trim(std::string_view str) {
  size_t start = 0;
  while (start < str.size() && (std::isspace(static_cast<unsigned char>(str[start])) != 0)) {
    ++start;
  }
  size_t end = str.size();
  while (end > start && (std::isspace(static_cast<unsigned char>(str[end - 1])) != 0)) {
    --end;
  }
  return std::string(str.substr(start, end - start));
}

std::string toUpper(std::string_view str) {
  std::string result(str);
  for (char& chr : result) {
    chr = static_cast<char>(std::toupper(static_cast<unsigned char>(chr)));
  }
  return result;
}

std::string conjTypeToString(dictionary::ConjugationType conj_type) {
  return std::string(dictionary::conjTypeToCanonicalString(conj_type));
}

std::optional<dictionary::ConjugationType> parseConjType(const std::string& str) {
  auto conj_type = dictionary::conjTypeFromCanonical(str);
  if (!conj_type) {
    return std::nullopt;
  }
  // Interjection is represented canonically by the POS column. FAMILY/GIVEN
  // remain valid markers for proper-name bigram behavior.
  if (*conj_type == dictionary::ConjugationType::Interjection) {
    return std::nullopt;
  }
  return conj_type;
}

std::optional<core::PartOfSpeech> parsePos(const std::string& str) {
  return core::stringToPosStrict(str);
}

std::string parseListFilter(const std::vector<std::string>& args, size_t first, ListFilter* filter) {
  for (size_t idx = first; idx < args.size(); ++idx) {
    std::string value;
    ValueOptionMatch match = matchValueOption(args, &idx, "--pos", &value);
    if (match == ValueOptionMatch::Matched) {
      filter->pos = core::stringToPosStrict(toUpper(value));
      if (!filter->pos.has_value()) {
        return "invalid --pos value: " + value;
      }
      continue;
    }
    if (match == ValueOptionMatch::Missing) {
      return "missing value for --pos";
    }
    match = matchValueOption(args, &idx, "--pattern", &filter->pattern);
    if (match == ValueOptionMatch::Matched) {
      continue;
    }
    if (match == ValueOptionMatch::Missing) {
      return "missing value for --pattern";
    }
    match = matchValueOption(args, &idx, "--limit", &value);
    if (match == ValueOptionMatch::Matched) {
      if (!parseSizeOption(value, &filter->limit)) {
        return "invalid --limit value: " + value;
      }
      continue;
    }
    if (match == ValueOptionMatch::Missing) {
      return "missing value for --limit";
    }
    return "unknown list option: " + args[idx];
  }
  if (!filter->pattern.empty()) {
    auto validation = validateWildcardPattern(filter->pattern);
    if (!validation.hasValue()) {
      return validation.error().message;
    }
  }
  return {};
}

}  // namespace suzume::cli
