#include "dictionary/source_parser.h"

#include <optional>
#include <utility>

#include "normalize/utf8.h"

namespace suzume {
namespace dictionary {

namespace {

struct ParsedRecord {
  std::vector<std::string> fields;
  std::string error;
};

std::string_view trimAsciiWhitespaceView(std::string_view field) {
  const size_t field_start = field.find_first_not_of(" \t\r\n");
  if (field_start == std::string_view::npos) {
    return {};
  }
  const size_t field_end = field.find_last_not_of(" \t\r\n");
  return field.substr(field_start, field_end - field_start + 1);
}

std::string trimAsciiWhitespace(std::string_view field) {
  return std::string(trimAsciiWhitespaceView(field));
}

void stripUtf8Bom(std::string_view& text) {
  if (normalize::startsWithUtf8Bom(text)) {
    text.remove_prefix(normalize::kUtf8Bom.size());
  }
}

core::Error quotingError(size_t line_number, std::string_view detail) {
  return core::Error(core::ErrorCode::ParseError, "Invalid legacy CSV quoting at line " +
                                                      core::decimalDigits(line_number) + ": " + std::string(detail));
}

// Rejects any non-empty field from index `first_unused` on: those columns carry no meaning.
std::optional<core::Error> findUnexpectedColumn(const std::vector<std::string>& fields, size_t first_unused,
                                                size_t line_number) {
  for (size_t field_idx = first_unused; field_idx < fields.size(); ++field_idx) {
    if (!fields[field_idx].empty()) {
      return core::Error(core::ErrorCode::ParseError, "Unexpected non-empty TSV column at line " +
                                                          core::decimalDigits(line_number) + ": " +
                                                          core::decimalDigits(field_idx + 1));
    }
  }
  return std::nullopt;
}

bool isAsciiHorizontalWhitespace(char chr) {
  return chr == ' ' || chr == '\t' || chr == '\r';
}

bool isNumericField(std::string_view field) {
  if (field.empty()) {
    return false;
  }
  bool seen_digit = false;
  for (char chr : field) {
    if (chr >= '0' && chr <= '9') {
      seen_digit = true;
      continue;
    }
    if (chr == '.' || chr == '-' || chr == '+') {
      continue;
    }
    return false;
  }
  return seen_digit;
}

std::optional<ConjugationType> parseConjugationType(std::string_view field) {
  auto canonical = conjTypeFromCanonical(field);
  if (canonical.has_value()) {
    return canonical;
  }
  return conjTypeFromAnyAlias(field);
}

bool usesLegacyTsvLayout(const std::vector<std::string>& fields) {
  // The legacy schema identifies itself with its numeric cost column:
  // surface, POS, reading, cost[, conj_type, lemma]. Empty spreadsheet padding
  // must not silently switch a current-format row into this schema.
  return fields.size() >= 4 && isNumericField(fields[3]);
}

char detectDelimiter(std::string_view record) {
  bool in_quotes = false;
  bool field_has_non_space = false;
  for (size_t idx = 0; idx < record.size(); ++idx) {
    const char chr = record[idx];
    if (chr == '"') {
      if (in_quotes && idx + 1 < record.size() && record[idx + 1] == '"') {
        ++idx;
        continue;
      }
      if (in_quotes) {
        in_quotes = false;
      } else if (!field_has_non_space) {
        in_quotes = true;
      } else {
        field_has_non_space = true;
      }
      continue;
    }
    if (!in_quotes && chr == '\t') {
      return '\t';
    }
    if (!in_quotes && chr == ',') {
      field_has_non_space = false;
      continue;
    }
    if (!in_quotes && chr != ' ' && chr != '\r' && chr != '\n') {
      field_has_non_space = true;
    }
  }
  return ',';
}

ParsedRecord parseDelimitedRecord(std::string_view record, char delimiter) {
  ParsedRecord result;
  std::string field;
  bool in_quotes = false;
  bool after_closing_quote = false;
  bool field_has_non_space = false;

  for (size_t idx = 0; idx < record.size(); ++idx) {
    const char chr = record[idx];

    if (chr == '"') {
      if (in_quotes && idx + 1 < record.size() && record[idx + 1] == '"') {
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
        field.push_back('"');
        continue;
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
      if (isAsciiHorizontalWhitespace(chr)) {
        continue;
      }
      result.error = "unexpected character after closing quote";
      return result;
    }

    if (chr != ' ' && chr != '\t' && chr != '\r' && chr != '\n') {
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

core::Expected<SourceEntry, core::Error> convertFields(const std::vector<std::string>& fields, size_t line_number,
                                                       char delimiter) {
  if (fields.size() < 2) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError, "Missing POS field at line " + core::decimalDigits(line_number)));
  }
  if (fields[0].empty()) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError, "Empty surface at line " + core::decimalDigits(line_number)));
  }
  if (fields[1].empty()) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError, "Empty POS at line " + core::decimalDigits(line_number)));
  }

  auto pos = core::stringToPosStrict(fields[1]);
  if (!pos.has_value()) {
    return core::makeUnexpected(core::Error(
        core::ErrorCode::ParseError, "Invalid POS at line " + core::decimalDigits(line_number) + ": " + fields[1]));
  }

  SourceEntry entry;
  entry.surface = fields[0];
  entry.pos = *pos;
  entry.is_proper_noun = fields[1] == "PROPN" || fields[1] == "PROPER_NOUN";
  entry.line_number = line_number;
  if (entry.pos == core::PartOfSpeech::Interjection) {
    entry.conj_type = ConjugationType::Interjection;
  }

  const bool is_tsv = delimiter == '\t';
  if (is_tsv) {
    entry.used_legacy_tsv_layout = usesLegacyTsvLayout(fields);
    if (entry.used_legacy_tsv_layout) {
      // Legacy extended TSV: surface, POS, reading, cost, conj_type, lemma.
      if (fields.size() > 4 && !fields[4].empty()) {
        auto conj_type = parseConjugationType(fields[4]);
        if (!conj_type.has_value()) {
          return core::makeUnexpected(
              core::Error(core::ErrorCode::ParseError,
                          "Line " + core::decimalDigits(line_number) + ": Invalid conjugation type: " + fields[4]));
        }
        entry.conj_type = *conj_type;
      }
      if (fields.size() > 5) {
        entry.lemma = fields[5];
      }
      if (auto error = findUnexpectedColumn(fields, 6, line_number)) {
        return core::makeUnexpected(std::move(*error));
      }
      return entry;
    }

    if (fields.size() > 4) {
      if (auto error = findUnexpectedColumn(fields, 4, line_number)) {
        return core::makeUnexpected(std::move(*error));
      }
      entry.ignored_empty_padding_columns = true;
    }

    // Current TSV: surface, POS, conj_type[, lemma]. A third field which is
    // neither a conjugation marker nor a numeric legacy cost is a runtime lemma.
    if (fields.size() > 2 && !fields[2].empty()) {
      auto conj_type = parseConjugationType(fields[2]);
      if (conj_type.has_value()) {
        entry.conj_type = *conj_type;
      } else if (!isNumericField(fields[2])) {
        if (fields.size() >= 4 && !fields[3].empty()) {
          return core::makeUnexpected(
              core::Error(core::ErrorCode::ParseError,
                          "Ambiguous non-empty TSV columns 3 and 4 at line " + core::decimalDigits(line_number)));
        }
        entry.lemma = fields[2];
      }
    }
    if (fields.size() >= 4 && !fields[3].empty()) {
      entry.lemma = fields[3];
    }
  } else {
    // Legacy CSV: surface, POS, cost, lemma.
    if (fields.size() > 3) {
      entry.lemma = fields[3];
    }
  }

  if (!normalize::isValidUtf8(entry.surface) || (!entry.lemma.empty() && !normalize::isValidUtf8(entry.lemma))) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError,
                    "Dictionary entry is not valid UTF-8 at line " + core::decimalDigits(line_number)));
  }

  return entry;
}

core::Expected<SourceEntry, core::Error> parseRecord(std::string_view record, size_t line_number, char delimiter) {
  auto parsed = parseDelimitedRecord(record, delimiter);
  if (!parsed.error.empty()) {
    return core::makeUnexpected(quotingError(line_number, parsed.error));
  }
  return convertFields(parsed.fields, line_number, delimiter);
}

}  // namespace

core::Expected<SourceParseResult, core::Error> parseDictionarySource(std::string_view content,
                                                                     SourceParseOptions options) {
  SourceParseResult result;
  stripUtf8Bom(content);

  std::optional<char> delimiter;
  size_t record_start = 0;
  size_t record_line = 1;
  size_t current_line = 1;
  bool in_quotes = false;
  bool field_has_non_space = false;

  for (size_t idx = 0; idx <= content.size(); ++idx) {
    const bool at_end = idx == content.size();
    if (at_end && record_start == content.size()) {
      break;
    }
    const char chr = at_end ? '\0' : content[idx];
    if (!at_end && chr == '"') {
      if (in_quotes && idx + 1 < content.size() && content[idx + 1] == '"') {
        ++idx;
        continue;
      }
      if (in_quotes) {
        in_quotes = false;
      } else if (!field_has_non_space) {
        in_quotes = true;
      } else {
        field_has_non_space = true;
      }
    } else if (!at_end && !in_quotes && (chr == '\t' || chr == ',')) {
      field_has_non_space = false;
    } else if (!at_end && !in_quotes && chr != ' ' && chr != '\r' && chr != '\n') {
      field_has_non_space = true;
    }

    if (!at_end && chr == '\n') {
      ++current_line;
    }
    if (!at_end && (chr != '\n' || in_quotes)) {
      continue;
    }

    std::string_view record = content.substr(record_start, idx - record_start);
    std::string_view trimmed = trimAsciiWhitespaceView(record);
    if (trimmed.empty()) {
      ++result.stats.empty_lines;
    } else if (trimmed.front() == '#') {
      ++result.stats.comment_lines;
    } else {
      const char record_delimiter = delimiter.value_or(detectDelimiter(record));
      auto parsed_record = parseDelimitedRecord(record, record_delimiter);
      if (!parsed_record.error.empty()) {
        return core::makeUnexpected(quotingError(record_line, parsed_record.error));
      }
      if (options.skip_single_field_records) {
        if (parsed_record.fields.size() < 2) {
          result.warnings.push_back("Skipped dictionary record at line " + core::decimalDigits(record_line) +
                                    ": missing POS field");
          record_start = idx + 1;
          record_line = current_line;
          continue;
        }
      }
      if (!delimiter.has_value()) {
        delimiter = record_delimiter;
      }
      auto entry = convertFields(parsed_record.fields, record_line, record_delimiter);
      if (!entry.hasValue()) {
        return core::makeUnexpected(entry.error());
      }
      if (entry.value().used_legacy_tsv_layout) {
        result.warnings.push_back("Used legacy TSV layout at line " + core::decimalDigits(record_line));
      }
      if (entry.value().ignored_empty_padding_columns) {
        result.warnings.push_back("Ignored empty TSV padding columns at line " + core::decimalDigits(record_line));
      }
      if (entry.value().pos == core::PartOfSpeech::Auxiliary || entry.value().pos == core::PartOfSpeech::Particle) {
        result.warnings.push_back("Dictionary entry at line " + core::decimalDigits(record_line) +
                                  " uses closed-class POS " + std::string(core::posToString(entry.value().pos)) +
                                  "; register grammatical auxiliaries and particles in L1 instead");
      }
      result.entries.push_back(std::move(entry.value()));
      ++result.stats.entries;
    }
    record_start = idx + 1;
    record_line = current_line;
    field_has_non_space = false;
  }

  if (in_quotes) {
    return core::makeUnexpected(quotingError(record_line, "unterminated quoted field"));
  }
  return result;
}

core::Expected<SourceEntry, core::Error> parseDictionarySourceLine(std::string_view line, size_t line_number) {
  stripUtf8Bom(line);
  return parseRecord(line, line_number, detectDelimiter(line));
}

DictionaryEntry sourceToDictionaryEntry(const SourceEntry& source_entry) {
  DictionaryEntry entry;
  entry.surface = source_entry.surface;
  entry.pos = source_entry.pos;
  entry.lemma = source_entry.lemma;

  switch (source_entry.conj_type) {
    case ConjugationType::Interjection:
      // Listed interjections are the greetings; the exclamations are L1.
      entry.extended_pos = core::ExtendedPOS::InterjectionGreeting;
      break;
    case ConjugationType::NaAdjective:
      entry.extended_pos = core::ExtendedPOS::AdjNaAdj;
      break;
    case ConjugationType::ProperFamily:
      entry.extended_pos = core::ExtendedPOS::NounProperFamily;
      break;
    case ConjugationType::ProperGiven:
      entry.extended_pos = core::ExtendedPOS::NounProperGiven;
      break;
    default:
      entry.extended_pos =
          source_entry.is_proper_noun ? core::ExtendedPOS::NounProper : core::posToExtendedPos(entry.pos);
      break;
  }
  return entry;
}

}  // namespace dictionary
}  // namespace suzume
