#include "dictionary/user_dict.h"

#include "core/types.h"

#include <fstream>
#include <sstream>

namespace suzume::dictionary {

UserDictionary::UserDictionary() = default;

core::Expected<size_t, core::Error> UserDictionary::loadFromFile(
    const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return core::Error(core::ErrorCode::FileNotFound,
                       "Failed to open dictionary file: " + path);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();
  return loadFromMemory(content.c_str(), content.size());
}

core::Expected<size_t, core::Error> UserDictionary::loadFromMemory(
    const char* data, size_t size) {
  if (data == nullptr || size == 0) {
    return core::Error(core::ErrorCode::InvalidInput, "Empty dictionary data");
  }

  std::string_view csv_data(data, size);
  return parseCSV(csv_data);
}

void UserDictionary::addEntry(const DictionaryEntry& entry) {
  auto idx = static_cast<uint32_t>(entries_.size());
  entries_.push_back(entry);
  trie_.insert(entry.surface, idx);
}

std::vector<LookupResult> UserDictionary::lookup(std::string_view text,
                                                 size_t start_pos) const {
  std::vector<LookupResult> results;

  auto matches = trie_.prefixMatch(text, start_pos);
  for (const auto& [length, entry_ids] : matches) {
    for (uint32_t idx : entry_ids) {
      if (idx < entries_.size()) {
        LookupResult result{};
        result.entry_id = idx;
        result.length = length;
        result.entry = &entries_[idx];
        results.push_back(result);
      }
    }
  }

  return results;
}

const DictionaryEntry* UserDictionary::getEntry(uint32_t idx) const {
  if (idx < entries_.size()) {
    return &entries_[idx];
  }
  return nullptr;
}

void UserDictionary::clear() {
  entries_.clear();
  trie_.clear();
}

namespace {

// Parse conjugation type string to enum
ConjugationType parseConjType(const std::string& type_str) {
  if (type_str.empty() || type_str == "NONE") {
    return ConjugationType::None;
  }
  if (type_str == "ICHIDAN") {
    return ConjugationType::Ichidan;
  }
  if (type_str == "GODAN_KA") {
    return ConjugationType::GodanKa;
  }
  if (type_str == "GODAN_GA") {
    return ConjugationType::GodanGa;
  }
  if (type_str == "GODAN_SA") {
    return ConjugationType::GodanSa;
  }
  if (type_str == "GODAN_TA") {
    return ConjugationType::GodanTa;
  }
  if (type_str == "GODAN_NA") {
    return ConjugationType::GodanNa;
  }
  if (type_str == "GODAN_BA") {
    return ConjugationType::GodanBa;
  }
  if (type_str == "GODAN_MA") {
    return ConjugationType::GodanMa;
  }
  if (type_str == "GODAN_RA") {
    return ConjugationType::GodanRa;
  }
  if (type_str == "GODAN_WA") {
    return ConjugationType::GodanWa;
  }
  if (type_str == "SURU") {
    return ConjugationType::Suru;
  }
  if (type_str == "KURU") {
    return ConjugationType::Kuru;
  }
  if (type_str == "I_ADJ") {
    return ConjugationType::IAdjective;
  }
  if (type_str == "NA_ADJ") {
    return ConjugationType::NaAdjective;
  }
  return ConjugationType::None;
}

}  // namespace

core::Expected<size_t, core::Error> UserDictionary::parseCSV(
    std::string_view csv_data) {
  size_t count = 0;
  size_t line_num = 0;

  std::string line;
  std::string csv_str(csv_data);
  std::istringstream stream(csv_str);

  while (std::getline(stream, line)) {
    ++line_num;

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Trim whitespace
    size_t start = line.find_first_not_of(" \t\r\n");
    size_t end = line.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) {
      continue;
    }
    line = line.substr(start, end - start + 1);

    // Detect delimiter: tab for TSV, comma for CSV
    char delimiter = (line.find('\t') != std::string::npos) ? '\t' : ',';

    // Parse fields
    std::vector<std::string> fields;
    std::string field;
    std::istringstream line_stream(line);

    while (std::getline(line_stream, field, delimiter)) {
      // Trim field
      size_t field_start = field.find_first_not_of(" \t");
      size_t field_end = field.find_last_not_of(" \t");
      if (field_start != std::string::npos) {
        field = field.substr(field_start, field_end - field_start + 1);
      } else {
        field.clear();
      }
      fields.push_back(field);
    }

    // Minimum: surface, pos
    if (fields.size() < 2) {
      continue;  // Skip invalid lines
    }

    DictionaryEntry entry;
    entry.surface = fields[0];
    entry.pos = core::stringToPos(fields[1]);

    // TSV format: surface, pos, reading, cost, conj_type
    // CSV format: surface, pos, cost, lemma
    bool is_tsv = (delimiter == '\t');

    if (is_tsv) {
      // TSV format: surface, pos, reading, cost, conj_type
      // Field 2 is reading (ignored for now)
      if (fields.size() > 3 && !fields[3].empty()) {
        try {
          entry.cost = std::stof(fields[3]);
        } catch (...) {
          entry.cost = 1.0F;
        }
      } else {
        entry.cost = 0.5F;
      }
      // Field 4 is conj_type
      if (fields.size() > 4 && !fields[4].empty()) {
        entry.conj_type = parseConjType(fields[4]);
      }
    } else {
      // CSV format: surface, pos, cost, lemma
      if (fields.size() > 2 && !fields[2].empty()) {
        try {
          entry.cost = std::stof(fields[2]);
        } catch (...) {
          entry.cost = 1.0F;
        }
      } else {
        entry.cost = 0.5F;
      }
      if (fields.size() > 3) {
        entry.lemma = fields[3];
      }
    }

    addEntry(entry);
    ++count;
  }

  return count;
}

void UserDictionary::rebuildTrie() {
  trie_.clear();
  for (size_t idx = 0; idx < entries_.size(); ++idx) {
    trie_.insert(entries_[idx].surface, static_cast<uint32_t>(idx));
  }
}

}  // namespace dictionary
