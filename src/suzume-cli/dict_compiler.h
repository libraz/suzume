#ifndef SUZUME_CLI_DICT_COMPILER_H_
#define SUZUME_CLI_DICT_COMPILER_H_

#include <string>
#include <vector>

#include "core/error.h"
#include "tsv_parser.h"

namespace suzume::cli {

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

 private:
  size_t entries_compiled_ = 0;
  size_t conj_expanded_ = 0;
  bool verbose_ = false;
};

}  // namespace suzume::cli

#endif  // SUZUME_CLI_DICT_COMPILER_H_
