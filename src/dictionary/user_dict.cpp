#include "dictionary/user_dict.h"

#include <fstream>
#include <sstream>

#include "core/types.h"

namespace suzume::dictionary {

namespace {

struct ParsedLine {
  std::vector<std::string> fields;
  std::string error;
};

std::string trimAsciiWhitespace(std::string field) {
  size_t field_start = field.find_first_not_of(" \t");
  size_t field_end = field.find_last_not_of(" \t");
  if (field_start == std::string::npos) {
    return "";
  }
  return field.substr(field_start, field_end - field_start + 1);
}

bool isAsciiWhitespace(char chr) {
  return chr == ' ' || chr == '\t';
}

ParsedLine parseDelimitedLine(std::string_view line, char delimiter) {
  ParsedLine result;
  std::string field;
  bool in_quotes = false;
  bool after_closing_quote = false;
  bool field_has_non_space = false;

  if (delimiter != ',') {
    std::stringstream stream{std::string(line)};
    std::string item;
    while (std::getline(stream, item, delimiter)) {
      result.fields.push_back(trimAsciiWhitespace(item));
    }
    return result;
  }

  for (size_t idx = 0; idx < line.size(); ++idx) {
    char chr = line[idx];

    if (chr == '"') {
      if (in_quotes && idx + 1 < line.size() && line[idx + 1] == '"') {
        field.push_back('"');
        ++idx;
        field_has_non_space = true;
        continue;
      }

      if (in_quotes) {
        in_quotes = false;
        after_closing_quote = true;
        continue;
      }

      if (field_has_non_space) {
        result.error = "quote found inside an unquoted field";
        return result;
      }

      field.clear();
      in_quotes = true;
      field_has_non_space = true;
      continue;
    }

    if (chr == delimiter && !in_quotes) {
      result.fields.push_back(trimAsciiWhitespace(field));
      field.clear();
      after_closing_quote = false;
      field_has_non_space = false;
      continue;
    }

    if (after_closing_quote) {
      if (isAsciiWhitespace(chr)) {
        continue;
      }
      result.error = "unexpected character after closing quote";
      return result;
    }

    if (!isAsciiWhitespace(chr)) {
      field_has_non_space = true;
    }
    field.push_back(chr);
  }

  if (in_quotes) {
    result.error = "unterminated quoted field";
    return result;
  }

  result.fields.push_back(trimAsciiWhitespace(field));
  return result;
}

core::Expected<core::PartOfSpeech, core::Error> parseUserDictPos(std::string_view field, size_t line_number) {
  if (field == "NOUN" || field == "名詞" || field == "PROPN" || field == "PROPER_NOUN") {
    return core::PartOfSpeech::Noun;
  }
  if (field == "VERB" || field == "動詞") {
    return core::PartOfSpeech::Verb;
  }
  if (field == "ADJ" || field == "ADJECTIVE" || field == "形容詞") {
    return core::PartOfSpeech::Adjective;
  }
  if (field == "ADV" || field == "ADVERB" || field == "副詞") {
    return core::PartOfSpeech::Adverb;
  }
  if (field == "PARTICLE" || field == "助詞") {
    return core::PartOfSpeech::Particle;
  }
  if (field == "AUX" || field == "AUXILIARY" || field == "助動詞") {
    return core::PartOfSpeech::Auxiliary;
  }
  if (field == "CONJ" || field == "CONJUNCTION" || field == "接続詞") {
    return core::PartOfSpeech::Conjunction;
  }
  if (field == "DET" || field == "DETERMINER" || field == "ADNOMINAL" || field == "連体詞") {
    return core::PartOfSpeech::Determiner;
  }
  if (field == "PRON" || field == "PRONOUN" || field == "代名詞") {
    return core::PartOfSpeech::Pronoun;
  }
  if (field == "PREFIX" || field == "接頭辞") {
    return core::PartOfSpeech::Prefix;
  }
  if (field == "SUFFIX" || field == "接尾辞") {
    return core::PartOfSpeech::Suffix;
  }
  if (field == "INTJ" || field == "INTERJECTION" || field == "感動詞") {
    return core::PartOfSpeech::Interjection;
  }
  if (field == "SYMBOL" || field == "SYM" || field == "記号") {
    return core::PartOfSpeech::Symbol;
  }
  if (field == "OTHER" || field == "PHRASE" || field == "その他") {
    return core::PartOfSpeech::Other;
  }

  return core::Error(core::ErrorCode::InvalidInput,
                     "Invalid POS at line " + std::to_string(line_number) + ": " + std::string(field));
}

}  // namespace

UserDictionary::UserDictionary() = default;

core::Expected<size_t, core::Error> UserDictionary::loadFromFile(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return core::Error(core::ErrorCode::FileNotFound, "Failed to open dictionary file: " + path);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();
  return loadFromMemory(content.c_str(), content.size());
}

core::Expected<size_t, core::Error> UserDictionary::loadFromMemory(const char* data, size_t size) {
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

std::vector<LookupResult> UserDictionary::lookup(std::string_view text, size_t start_pos) const {
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

core::Expected<size_t, core::Error> UserDictionary::parseCSV(std::string_view csv_data) {
  std::vector<DictionaryEntry> parsed_entries;

  std::string line;
  std::string csv_str(csv_data);
  std::istringstream stream(csv_str);
  size_t line_number = 0;

  while (std::getline(stream, line)) {
    ++line_number;

    // Trim whitespace
    size_t start = line.find_first_not_of(" \t\r\n");
    size_t end = line.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) {
      continue;
    }
    line = line.substr(start, end - start + 1);

    if (line[0] == '#') {
      continue;
    }

    // Detect delimiter: tab for TSV, comma for CSV
    char delimiter = (line.find('\t') != std::string::npos) ? '\t' : ',';

    auto parsed = parseDelimitedLine(line, delimiter);
    if (!parsed.error.empty()) {
      return core::Error(core::ErrorCode::InvalidInput,
                         "Invalid CSV quoting at line " + std::to_string(line_number) + ": " + parsed.error);
    }
    auto& fields = parsed.fields;

    // Minimum: surface, pos
    if (fields.size() < 2) {
      continue;  // Skip invalid lines
    }

    if (fields[0].empty()) {
      return core::Error(core::ErrorCode::InvalidInput, "Empty surface at line " + std::to_string(line_number));
    }
    if (fields[1].empty()) {
      return core::Error(core::ErrorCode::InvalidInput, "Empty POS at line " + std::to_string(line_number));
    }

    DictionaryEntry entry;
    entry.surface = fields[0];
    auto pos_result = parseUserDictPos(fields[1], line_number);
    if (!pos_result.hasValue()) {
      return pos_result.error();
    }
    entry.pos = pos_result.value();

    // TSV format: surface, pos, reading, cost, conj_type
    // CSV format: surface, pos, cost, lemma
    bool is_tsv = (delimiter == '\t');

    // v0.8: Simplified - cost and conj_type removed from DictionaryEntry
    // Cost is now derived from ExtendedPOS via getCategoryCost()
    if (is_tsv) {
      // TSV format: surface, pos, reading, cost, conj_type
      // Fields 2-4 (reading, cost, conj_type) are ignored
      (void)fields;  // suppress unused warning
    } else {
      // CSV format: surface, pos, cost, lemma
      // Field 2 (cost) is ignored
      if (fields.size() > 3) {
        entry.lemma = fields[3];
      }
    }

    entry.extended_pos = core::posToDefaultExtendedPOS(entry.pos);
    parsed_entries.push_back(std::move(entry));
  }

  for (const auto& entry : parsed_entries) {
    addEntry(entry);
  }

  return parsed_entries.size();
}

void UserDictionary::rebuildTrie() {
  trie_.clear();
  for (size_t idx = 0; idx < entries_.size(); ++idx) {
    trie_.insert(entries_[idx].surface, static_cast<uint32_t>(idx));
  }
}

}  // namespace suzume::dictionary
