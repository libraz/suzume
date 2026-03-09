#ifndef SUZUME_CLI_DICT_COMPILER_H_
#define SUZUME_CLI_DICT_COMPILER_H_

#include <string>
#include <string_view>
#include <vector>

#include "core/error.h"
#include "tsv_parser.h"

namespace suzume::cli {

/**
 * @brief Check if a dictionary entry surface is "trivial"
 *
 * An entry is trivial if it has 3+ codepoints, contains no spaces, and
 * all meaningful characters (ignoring Symbol/Unknown) share the same
 * CharType. Such entries can be filtered to reduce dictionary bloat.
 *
 * @param surface UTF-8 encoded surface string
 * @return true if the entry is trivial and can be filtered out
 */
bool isTrivialEntry(std::string_view surface);

/**
 * @brief Dictionary compiler (TSV to binary)
 */
class DictCompiler {
 public:
  DictCompiler();

  /**
   * @brief Compile TSV file to binary dictionary
   * @param tsv_path Input TSV file path
   * @param dic_path Output binary dictionary path
   * @return Number of entries compiled, or error
   */
  core::Expected<size_t, core::Error> compile(const std::string& tsv_path,
                                               const std::string& dic_path);

  /**
   * @brief Compile multiple TSV files to a single binary dictionary
   * @param tsv_paths Input TSV file paths
   * @param dic_path Output binary dictionary path
   * @return Number of entries compiled, or error
   */
  core::Expected<size_t, core::Error> compileMultiple(
      const std::vector<std::string>& tsv_paths,
      const std::string& dic_path);

  /**
   * @brief Compile entries to binary dictionary
   * @param entries TSV entries
   * @param dic_path Output binary dictionary path
   * @return Number of entries compiled, or error
   */
  core::Expected<size_t, core::Error> compileEntries(
      const std::vector<TsvEntry>& entries, const std::string& dic_path);

  /**
   * @brief Decompile binary dictionary to TSV
   * @param dic_path Input binary dictionary path
   * @param tsv_path Output TSV file path
   * @return Number of entries decompiled, or error
   */
  core::Expected<size_t, core::Error> decompile(const std::string& dic_path, const std::string& tsv_path) const;

  /**
   * @brief Get compilation statistics
   */
  size_t entriesCompiled() const { return entries_compiled_; }
  size_t conjExpanded() const { return conj_expanded_; }

  /**
   * @brief Enable/disable verbose output
   */
  void setVerbose(bool verbose) { verbose_ = verbose; }

  /**
   * @brief Enable/disable trivial entry filtering
   *
   * When enabled, entries are filtered out if they have 3+ codepoints,
   * contain no spaces, and consist of only one character type (e.g., pure
   * kanji or pure katakana). Mixed-type entries (kanji+katakana, etc.)
   * and 2-char entries are always kept.
   */
  void setFilterTrivial(bool filter) { filter_trivial_ = filter; }

 private:
  size_t entries_compiled_ = 0;
  size_t conj_expanded_ = 0;
  bool verbose_ = false;
  bool filter_trivial_ = false;
};

}  // namespace suzume::cli

#endif  // SUZUME_CLI_DICT_COMPILER_H_
