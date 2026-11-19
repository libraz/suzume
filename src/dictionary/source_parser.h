#ifndef SUZUME_DICTIONARY_SOURCE_PARSER_H_
#define SUZUME_DICTIONARY_SOURCE_PARSER_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.h"
#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume {
namespace dictionary {

/**
 * @brief One entry parsed from a dictionary source file.
 *
 * Rows are tab-separated: surface, POS[, conj_type][, base_form]. The removed
 * legacy TSV (numeric cost in column 4), cost-column (numeric column 3) and CSV
 * layouts fail with a ParseError naming the line.
 */
struct SourceEntry {
  std::string surface;
  core::PartOfSpeech pos{core::PartOfSpeech::Noun};
  ConjugationType conj_type{ConjugationType::None};
  std::string lemma;
  size_t line_number{0};
  bool is_proper_noun{false};
  bool ignored_empty_padding_columns{false};
};

struct SourceParseStats {
  size_t entries{0};
  size_t comment_lines{0};
  size_t empty_lines{0};
};

struct SourceParseResult {
  std::vector<SourceEntry> entries;
  SourceParseStats stats;
  std::vector<std::string> warnings;
};

struct SourceParseOptions {
  // Runtime loaders skip records without a POS column. The CLI
  // compiler remains strict by using the default.
  bool skip_single_field_records{false};
};

/**
 * @brief Parse tab-separated dictionary source text.
 *
 * A data record without a tab is read as comma-separated only to reject it as
 * the removed CSV layout. UTF-8 BOM, CRLF,
 * comments, surrounding ASCII whitespace, and RFC 4180 quoted fields
 * (including escaped quotes and embedded newlines) are supported.
 */
core::Expected<SourceParseResult, core::Error> parseDictionarySource(std::string_view content,
                                                                     SourceParseOptions options = {});

/**
 * @brief Parse one dictionary source record using an explicit line number.
 */
core::Expected<SourceEntry, core::Error> parseDictionarySourceLine(std::string_view line, size_t line_number);

/**
 * @brief Convert a parsed source entry to the runtime dictionary shape.
 */
DictionaryEntry sourceToDictionaryEntry(const SourceEntry& source_entry);

}  // namespace dictionary
}  // namespace suzume

#endif  // SUZUME_DICTIONARY_SOURCE_PARSER_H_
