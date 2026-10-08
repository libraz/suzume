#include "tsv_parser.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "grammar/dictionary_expansion.h"

namespace suzume::cli {

namespace {

constexpr uintmax_t kMaxDictionarySourceBytes = 64U * 1024U * 1024U;

}  // namespace

std::string tsvWriteFieldIssue(std::string_view value, std::string_view field_name) {
  if (!value.empty() && value.front() == '#') {
    return std::string(field_name) + " cannot begin with # because the TSV writer would serialize it as a comment";
  }
  if (value.find_first_of("\t\r\n") != std::string_view::npos) {
    return std::string(field_name) + " cannot contain a tab, carriage return, or newline";
  }
  return {};
}

TsvParser::TsvParser() = default;

core::Expected<std::vector<TsvEntry>, core::Error> TsvParser::parseFile(const std::string& path) {
  std::error_code filesystem_error;
  const std::filesystem::path source_path(path);
  if (!std::filesystem::is_regular_file(source_path, filesystem_error)) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput, "Dictionary source is not a regular file: " + path));
  }
  const uintmax_t source_size = std::filesystem::file_size(source_path, filesystem_error);
  if (filesystem_error) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput, "Failed to inspect dictionary source: " + path));
  }
  if (source_size > kMaxDictionarySourceBytes) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput, "Dictionary source exceeds 64 MiB limit: " + path));
  }

  std::ifstream file(path);
  if (!file) {
    return core::makeUnexpected(core::Error(core::ErrorCode::FileNotFound, "Failed to open TSV file: " + path));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  if (file.bad() || buffer.bad()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InternalError, "Failed to read TSV file: " + path));
  }
  return parseString(buffer.str());
}

core::Expected<std::vector<TsvEntry>, core::Error> TsvParser::parseString(std::string_view content) {
  entries_parsed_ = 0;
  comment_lines_ = 0;
  empty_lines_ = 0;
  error_lines_ = 0;

  auto result = dictionary::parseDictionarySource(content);
  if (!result.hasValue()) {
    ++error_lines_;
    return core::makeUnexpected(result.error());
  }

  entries_parsed_ = result.value().stats.entries;
  comment_lines_ = result.value().stats.comment_lines;
  empty_lines_ = result.value().stats.empty_lines;
  return std::move(result.value().entries);
}

core::Expected<TsvEntry, core::Error> TsvParser::parseLine(std::string_view line, size_t line_number) {
  return dictionary::parseDictionarySourceLine(line, line_number);
}

size_t TsvParser::validate(const std::vector<TsvEntry>& entries, std::vector<std::string>* issues) {
  std::set<std::pair<std::string, core::PartOfSpeech>> seen;
  size_t issue_count = 0;

  for (const auto& entry : entries) {
    const std::string line_suffix =
        entry.line_number == 0 ? std::string() : " at line " + std::to_string(entry.line_number);
    auto key = std::make_pair(entry.surface, entry.pos);
    if (seen.count(key) > 0) {
      ++issue_count;
      if (issues != nullptr) {
        std::string pos_str(core::posToString(entry.pos));
        issues->push_back("Duplicate entry" + line_suffix + ": " + entry.surface + " (" + pos_str + ")");
      }
    }
    seen.insert(key);

    if (entry.ignored_empty_padding_columns) {
      ++issue_count;
      if (issues != nullptr) {
        issues->push_back("Unexpected empty padding columns" + line_suffix + ": " + entry.surface);
      }
    }

    // Check conjugation type for verbs/adjectives
    if (entry.pos == core::PartOfSpeech::Verb || entry.pos == core::PartOfSpeech::Adjective) {
      if (entry.conj_type == dictionary::ConjugationType::None) {
        ++issue_count;
        if (issues != nullptr) {
          issues->push_back("Missing conjugation type" + line_suffix + ": " + entry.surface);
        }
      }
    }

    const std::string conjugation_issue = grammar::dictionaryConjugationTypeIssue(entry);
    if (!conjugation_issue.empty()) {
      ++issue_count;
      if (issues != nullptr) {
        issues->push_back("Invalid conjugation surface" + line_suffix + ": " + entry.surface + " (" +
                          conjugation_issue + ")");
      }
    }
  }

  return issue_count;
}

void writeTsvEntries(std::ostream& output, const std::vector<TsvEntry>& entries) {
  for (const auto& entry : entries) {
    output << entry.surface << "\t" << core::posToString(entry.pos);

    if (entry.conj_type != dictionary::ConjugationType::None || !entry.lemma.empty()) {
      output << "\t";
      // Interjection is represented canonically by its POS field. Every other
      // marker, including FAMILY/GIVEN, belongs in the conjugation column.
      if (entry.conj_type != dictionary::ConjugationType::Interjection) {
        output << dictionary::conjTypeToCanonicalString(entry.conj_type);
      }
    }
    if (!entry.lemma.empty()) {
      output << "\t" << entry.lemma;
    }

    output << "\n";
  }
}

core::Expected<size_t, core::Error> writeTsvFile(const std::string& path, const std::vector<TsvEntry>& entries) {
  for (const auto& entry : entries) {
    if (const std::string issue = tsvWriteFieldIssue(entry.surface, "Surface"); !issue.empty()) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, issue));
    }
    if (const std::string issue = tsvWriteFieldIssue(entry.lemma, "Lemma"); !issue.empty()) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, issue));
    }
  }

  const std::filesystem::path output_path(path);
  const std::filesystem::path temporary_path = output_path.string() + ".tmp";
  std::ofstream file(temporary_path);
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError, "Failed to create temporary file: " + temporary_path.string()));
  }

  // Write header comment
  file << "# suzume dictionary source file\n";
  file << "# Format: surface<TAB>pos<TAB>conj_type<TAB>lemma\n";
  file << "\n";

  writeTsvEntries(file, entries);

  file.close();
  if (!file) {
    std::error_code remove_error;
    std::filesystem::remove(temporary_path, remove_error);
    return core::makeUnexpected(core::Error(core::ErrorCode::InternalError, "Failed to write file: " + path));
  }

  std::error_code rename_error;
  std::filesystem::rename(temporary_path, output_path, rename_error);
  if (rename_error) {
    std::error_code remove_error;
    std::filesystem::remove(temporary_path, remove_error);
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError, "Failed to replace file: " + path + ": " + rename_error.message()));
  }

  return entries.size();
}

}  // namespace suzume::cli
