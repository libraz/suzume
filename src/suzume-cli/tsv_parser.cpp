#include "tsv_parser.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>

namespace suzume::cli {

TsvParser::TsvParser() = default;

core::Expected<std::vector<TsvEntry>, core::Error> TsvParser::parseFile(
    const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::FileNotFound,
                    "Failed to open TSV file: " + path));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return parseString(buffer.str());
}

core::Expected<std::vector<TsvEntry>, core::Error> TsvParser::parseString(
    std::string_view content) {
  std::vector<TsvEntry> entries;
  entries_parsed_ = 0;
  comment_lines_ = 0;
  empty_lines_ = 0;
  error_lines_ = 0;

  size_t line_number = 0;
  size_t pos = 0;

  while (pos < content.size()) {
    ++line_number;

    // Find end of line
    size_t eol = content.find('\n', pos);
    if (eol == std::string_view::npos) {
      eol = content.size();
    }

    std::string_view line = content.substr(pos, eol - pos);

    // Remove carriage return if present
    if (!line.empty() && line.back() == '\r') {
      line = line.substr(0, line.size() - 1);
    }

    // Skip empty lines
    if (line.empty() || line.find_first_not_of(" \t") == std::string_view::npos) {
      ++empty_lines_;
      pos = eol + 1;
      continue;
    }

    // Skip comments
    size_t first_char = line.find_first_not_of(" \t");
    if (first_char != std::string_view::npos && line[first_char] == '#') {
      ++comment_lines_;
      pos = eol + 1;
      continue;
    }

    // Parse line
    auto result = parseLine(line, line_number);
    if (result.hasValue()) {
      entries.push_back(std::move(result.value()));
      ++entries_parsed_;
    } else {
      ++error_lines_;
      // Return first error
      return core::makeUnexpected(result.error());
    }

    pos = eol + 1;
  }

  return entries;
}

core::Expected<TsvEntry, core::Error> TsvParser::parseLine(
    std::string_view line, size_t line_number) {
  TsvEntry entry;
  entry.line_number = line_number;

  // Split by tab
  std::vector<std::string_view> fields;
  size_t start = 0;
  while (start < line.size()) {
    size_t tab = line.find('\t', start);
    if (tab == std::string_view::npos) {
      fields.push_back(line.substr(start));
      break;
    }
    fields.push_back(line.substr(start, tab - start));
    start = tab + 1;
  }

  if (fields.empty()) {
    return core::makeUnexpected(core::Error(
        core::ErrorCode::ParseError,
        "Line " + std::to_string(line_number) + ": Empty line"));
  }

  // Field 0: surface (required)
  entry.surface = std::string(fields[0]);
  if (entry.surface.empty()) {
    return core::makeUnexpected(core::Error(
        core::ErrorCode::ParseError,
        "Line " + std::to_string(line_number) + ": Empty surface"));
  }

  // Field 1: POS (required)
  if (fields.size() < 2) {
    return core::makeUnexpected(core::Error(
        core::ErrorCode::ParseError,
        "Line " + std::to_string(line_number) + ": Missing POS field"));
  }
  auto pos_result = parsePos(fields[1], line_number);
  if (!pos_result.hasValue()) {
    return core::makeUnexpected(pos_result.error());
  }
  entry.pos = pos_result.value();

  // Field 2: reading (optional)
  if (fields.size() > 2 && !fields[2].empty()) {
    entry.reading = std::string(fields[2]);
  }

  // Field 3: cost (optional, default 0.5)
  if (fields.size() > 3 && !fields[3].empty()) {
    auto cost_result = parseCost(fields[3], line_number);
    if (!cost_result.hasValue()) {
      return core::makeUnexpected(cost_result.error());
    }
    entry.cost = cost_result.value();
  } else {
    entry.cost = 0.5F;
  }

  // Field 4: conj_type (optional, default None)
  if (fields.size() > 4 && !fields[4].empty()) {
    auto conj_result = parseConjType(fields[4], line_number);
    if (!conj_result.hasValue()) {
      return core::makeUnexpected(conj_result.error());
    }
    entry.conj_type = conj_result.value();
  } else {
    entry.conj_type = dictionary::ConjugationType::None;
  }

  return entry;
}

core::Expected<core::PartOfSpeech, core::Error> TsvParser::parsePos(
    std::string_view str, size_t line) {
  // Trim whitespace
  size_t start = str.find_first_not_of(" \t");
  size_t end = str.find_last_not_of(" \t");
  if (start == std::string_view::npos) {
    return core::makeUnexpected(core::Error(
        core::ErrorCode::ParseError,
        "Line " + std::to_string(line) + ": Empty POS"));
  }
  str = str.substr(start, end - start + 1);

  // Map string to POS
  if (str == "NOUN") {
    return core::PartOfSpeech::Noun;
  }
  if (str == "PROPN") {
    return core::PartOfSpeech::Noun;  // Map PROPN to Noun for now
  }
  if (str == "VERB") {
    return core::PartOfSpeech::Verb;
  }
  if (str == "ADJECTIVE" || str == "ADJ") {
    return core::PartOfSpeech::Adjective;
  }
  if (str == "ADVERB" || str == "ADV") {
    return core::PartOfSpeech::Adverb;
  }
  if (str == "PARTICLE") {
    return core::PartOfSpeech::Particle;
  }
  if (str == "AUXILIARY" || str == "AUX") {
    return core::PartOfSpeech::Auxiliary;
  }
  if (str == "CONJUNCTION" || str == "CONJ") {
    return core::PartOfSpeech::Conjunction;
  }
  if (str == "SYMBOL" || str == "SYM") {
    return core::PartOfSpeech::Symbol;
  }
  if (str == "OTHER") {
    return core::PartOfSpeech::Other;
  }
  if (str == "PHRASE") {
    return core::PartOfSpeech::Other;  // Map PHRASE to Other
  }
  if (str == "INTJ") {
    return core::PartOfSpeech::Other;  // Map INTJ (interjection) to Other
  }
  if (str == "PRONOUN" || str == "PRON") {
    return core::PartOfSpeech::Pronoun;
  }
  if (str == "DETERMINER" || str == "DET") {
    return core::PartOfSpeech::Determiner;
  }

  return core::makeUnexpected(core::Error(
      core::ErrorCode::ParseError,
      "Line " + std::to_string(line) + ": Invalid POS: " + std::string(str)));
}

core::Expected<dictionary::ConjugationType, core::Error>
TsvParser::parseConjType(std::string_view str, size_t line) {
  // Trim whitespace
  size_t start = str.find_first_not_of(" \t");
  size_t end = str.find_last_not_of(" \t");
  if (start == std::string_view::npos) {
    return dictionary::ConjugationType::None;
  }
  str = str.substr(start, end - start + 1);

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

  return core::makeUnexpected(
      core::Error(core::ErrorCode::ParseError,
                  "Line " + std::to_string(line) +
                      ": Invalid conjugation type: " + std::string(str)));
}

core::Expected<float, core::Error> TsvParser::parseCost(std::string_view str,
                                                         size_t line) {
  // Trim whitespace
  size_t start = str.find_first_not_of(" \t");
  size_t end = str.find_last_not_of(" \t");
  if (start == std::string_view::npos) {
    return 0.5F;
  }
  std::string cost_str(str.substr(start, end - start + 1));

  char* endptr = nullptr;
  float cost = std::strtof(cost_str.c_str(), &endptr);

  if (endptr == cost_str.c_str() || *endptr != '\0') {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError,
                    "Line " + std::to_string(line) +
                        ": Invalid cost: " + cost_str));
  }

  if (cost < -10.0F || cost > 10.0F) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError,
                    "Line " + std::to_string(line) +
                        ": Cost out of range (-10 to 10): " + cost_str));
  }

  return cost;
}

size_t TsvParser::validate(const std::vector<TsvEntry>& entries,
                           std::vector<std::string>* issues) {
  std::set<std::pair<std::string, core::PartOfSpeech>> seen;
  size_t issue_count = 0;

  for (const auto& entry : entries) {
    auto key = std::make_pair(entry.surface, entry.pos);
    if (seen.count(key) > 0) {
      ++issue_count;
      if (issues != nullptr) {
        std::string pos_str(core::posToString(entry.pos));
        issues->push_back("Duplicate entry at line " +
                          std::to_string(entry.line_number) + ": " +
                          entry.surface + " (" + pos_str + ")");
      }
    }
    seen.insert(key);

    // Check conjugation type for verbs/adjectives
    if (entry.pos == core::PartOfSpeech::Verb ||
        entry.pos == core::PartOfSpeech::Adjective) {
      if (entry.conj_type == dictionary::ConjugationType::None) {
        ++issue_count;
        if (issues != nullptr) {
          issues->push_back("Missing conjugation type at line " +
                            std::to_string(entry.line_number) + ": " +
                            entry.surface);
        }
      }
    }
  }

  return issue_count;
}

core::Expected<size_t, core::Error> writeTsvFile(
    const std::string& path, const std::vector<TsvEntry>& entries) {
  std::ofstream file(path);
  if (!file) {
    return core::makeUnexpected(core::Error(
        core::ErrorCode::InternalError, "Failed to create file: " + path));
  }

  // Write header comment
  file << "# suzume dictionary source file\n";
  file << "# Format: surface<TAB>pos<TAB>reading<TAB>cost<TAB>conj_type\n";
  file << "\n";

  for (const auto& entry : entries) {
    file << entry.surface << "\t" << core::posToString(entry.pos) << "\t"
         << entry.reading << "\t" << entry.cost;

    if (entry.conj_type != dictionary::ConjugationType::None) {
      file << "\t";
      switch (entry.conj_type) {
        case dictionary::ConjugationType::Ichidan:
          file << "ICHIDAN";
          break;
        case dictionary::ConjugationType::GodanKa:
          file << "GODAN_KA";
          break;
        case dictionary::ConjugationType::GodanGa:
          file << "GODAN_GA";
          break;
        case dictionary::ConjugationType::GodanSa:
          file << "GODAN_SA";
          break;
        case dictionary::ConjugationType::GodanTa:
          file << "GODAN_TA";
          break;
        case dictionary::ConjugationType::GodanNa:
          file << "GODAN_NA";
          break;
        case dictionary::ConjugationType::GodanBa:
          file << "GODAN_BA";
          break;
        case dictionary::ConjugationType::GodanMa:
          file << "GODAN_MA";
          break;
        case dictionary::ConjugationType::GodanRa:
          file << "GODAN_RA";
          break;
        case dictionary::ConjugationType::GodanWa:
          file << "GODAN_WA";
          break;
        case dictionary::ConjugationType::Suru:
          file << "SURU";
          break;
        case dictionary::ConjugationType::Kuru:
          file << "KURU";
          break;
        case dictionary::ConjugationType::IAdjective:
          file << "I_ADJ";
          break;
        case dictionary::ConjugationType::NaAdjective:
          file << "NA_ADJ";
          break;
        default:
          break;
      }
    }

    file << "\n";
  }

  return entries.size();
}

dictionary::DictionaryEntry tsvToDictEntry(const TsvEntry& tsv_entry) {
  dictionary::DictionaryEntry entry;
  entry.surface = tsv_entry.surface;
  entry.pos = tsv_entry.pos;
  entry.cost = tsv_entry.cost;
  entry.lemma = tsv_entry.surface;  // Default lemma to surface
  entry.conj_type = tsv_entry.conj_type;
  return entry;
}

}  // namespace cli
