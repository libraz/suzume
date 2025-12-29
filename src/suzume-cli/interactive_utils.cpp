#include "interactive_utils.h"

#include <cctype>

namespace suzume::cli {

std::string trim(std::string_view str) {
  size_t start = 0;
  while (start < str.size() && (std::isspace(str[start]) != 0)) {
    ++start;
  }
  size_t end = str.size();
  while (end > start && (std::isspace(str[end - 1]) != 0)) {
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
  switch (conj_type) {
    case dictionary::ConjugationType::None:
      return "";
    case dictionary::ConjugationType::Ichidan:
      return "ICHIDAN";
    case dictionary::ConjugationType::GodanKa:
      return "GODAN_KA";
    case dictionary::ConjugationType::GodanGa:
      return "GODAN_GA";
    case dictionary::ConjugationType::GodanSa:
      return "GODAN_SA";
    case dictionary::ConjugationType::GodanTa:
      return "GODAN_TA";
    case dictionary::ConjugationType::GodanNa:
      return "GODAN_NA";
    case dictionary::ConjugationType::GodanBa:
      return "GODAN_BA";
    case dictionary::ConjugationType::GodanMa:
      return "GODAN_MA";
    case dictionary::ConjugationType::GodanRa:
      return "GODAN_RA";
    case dictionary::ConjugationType::GodanWa:
      return "GODAN_WA";
    case dictionary::ConjugationType::Suru:
      return "SURU";
    case dictionary::ConjugationType::Kuru:
      return "KURU";
    case dictionary::ConjugationType::IAdjective:
      return "I_ADJ";
    case dictionary::ConjugationType::NaAdjective:
      return "NA_ADJ";
  }
  return "";
}

std::optional<dictionary::ConjugationType> parseConjType(
    const std::string& str) {
  if (str.empty() || str == "NONE") {
    return dictionary::ConjugationType::None;
  }
  if (str == "ICHIDAN") {
    return dictionary::ConjugationType::Ichidan;
  }
  if (str == "GODAN_KA") {
    return dictionary::ConjugationType::GodanKa;
  }
  if (str == "GODAN_GA") {
    return dictionary::ConjugationType::GodanGa;
  }
  if (str == "GODAN_SA") {
    return dictionary::ConjugationType::GodanSa;
  }
  if (str == "GODAN_TA") {
    return dictionary::ConjugationType::GodanTa;
  }
  if (str == "GODAN_NA") {
    return dictionary::ConjugationType::GodanNa;
  }
  if (str == "GODAN_BA") {
    return dictionary::ConjugationType::GodanBa;
  }
  if (str == "GODAN_MA") {
    return dictionary::ConjugationType::GodanMa;
  }
  if (str == "GODAN_RA") {
    return dictionary::ConjugationType::GodanRa;
  }
  if (str == "GODAN_WA") {
    return dictionary::ConjugationType::GodanWa;
  }
  if (str == "SURU") {
    return dictionary::ConjugationType::Suru;
  }
  if (str == "KURU") {
    return dictionary::ConjugationType::Kuru;
  }
  if (str == "I_ADJ") {
    return dictionary::ConjugationType::IAdjective;
  }
  if (str == "NA_ADJ") {
    return dictionary::ConjugationType::NaAdjective;
  }
  return std::nullopt;
}

std::optional<core::PartOfSpeech> parsePos(const std::string& str) {
  auto pos = core::stringToPos(str);
  // stringToPos returns Other for unknown strings, so we need to check
  // if the input was actually "OTHER" or just invalid
  if (pos == core::PartOfSpeech::Other && str != "OTHER") {
    // Check if it's a known POS
    if (str == "NOUN" || str == "PROPN" || str == "VERB" ||
        str == "ADJECTIVE" || str == "ADJ" || str == "ADVERB" ||
        str == "ADV" || str == "PARTICLE" || str == "AUXILIARY" ||
        str == "AUX" || str == "CONJUNCTION" || str == "CONJ" ||
        str == "SYMBOL" || str == "SYM") {
      return pos;
    }
    return std::nullopt;
  }
  return pos;
}

}  // namespace suzume::cli
