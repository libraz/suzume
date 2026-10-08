#ifndef SUZUME_CLI_TSV_PARSER_H_
#define SUZUME_CLI_TSV_PARSER_H_

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.h"
#include "core/types.h"
#include "dictionary/binary_dict.h"
#include "dictionary/dictionary.h"
#include "dictionary/source_parser.h"

namespace suzume::cli {

/**
 * @brief TSV dictionary entry (parsed from TSV file)
 *
 * Current format: surface<TAB>pos<TAB>conj_type<TAB>lemma
 * Legacy reading/cost fields are accepted but not retained.
 */
using TsvEntry = dictionary::SourceEntry;

/**
 * @brief TSV dictionary parser
 *
 * Wraps the Dictionary-layer source parser with CLI statistics and validation.
 * Both current TSV and legacy runtime CSV input are accepted.
 *
 * Comments start with #, empty lines are ignored.
 */
class TsvParser {
 public:
  TsvParser();

  /**
   * @brief Parse TSV file
   * @param path File path
   * @return Entries on success, error on failure
   */
  core::Expected<std::vector<TsvEntry>, core::Error> parseFile(const std::string& path);

  /**
   * @brief Parse TSV string
   * @param content TSV content
   * @return Entries on success, error on failure
   */
  core::Expected<std::vector<TsvEntry>, core::Error> parseString(std::string_view content);

  /**
   * @brief Parse single TSV line
   * @param line Line content (without newline)
   * @param line_number Line number for error reporting
   * @return Entry on success, error on failure
   */
  static core::Expected<TsvEntry, core::Error> parseLine(std::string_view line, size_t line_number);

  /**
   * @brief Validate entries (check for duplicates, invalid values)
   * @param entries Entries to validate
   * @return Number of issues found (0 = valid)
   */
  static size_t validate(const std::vector<TsvEntry>& entries, std::vector<std::string>* issues = nullptr);

  /**
   * @brief Get parse statistics
   */
  size_t entriesParsed() const { return entries_parsed_; }
  size_t commentLines() const { return comment_lines_; }
  size_t emptyLines() const { return empty_lines_; }
  size_t errorLines() const { return error_lines_; }

 private:
  size_t entries_parsed_ = 0;
  size_t comment_lines_ = 0;
  size_t empty_lines_ = 0;
  size_t error_lines_ = 0;
};

/**
 * @brief Write entries to TSV file
 * @param path Output file path
 * @param entries Entries to write
 * @return Number of entries written, or error
 */
core::Expected<size_t, core::Error> writeTsvFile(const std::string& path, const std::vector<TsvEntry>& entries);

/**
 * @brief Return why a surface or lemma cannot be represented by the plain TSV writer.
 */
std::string tsvWriteFieldIssue(std::string_view value, std::string_view field_name);

/**
 * @brief Write TSV entry rows to an output stream.
 * @param output Output stream
 * @param entries Entries to write
 */
void writeTsvEntries(std::ostream& output, const std::vector<TsvEntry>& entries);

}  // namespace suzume::cli

#endif  // SUZUME_CLI_TSV_PARSER_H_
