#ifndef SUZUME_CLI_CLI_COMMON_H_
#define SUZUME_CLI_CLI_COMMON_H_

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.h"

namespace suzume {
class Suzume;
}

namespace suzume::cli {

/**
 * @brief Output format for analysis results
 */
enum class OutputFormat {
  Tsv,     // Default: surface TAB pos TAB base_form TAB start TAB end
  Json,    // JSON format
  Chasen,  // ChaSen-like format (Japanese POS, conjugation info)
  Tags     // Tags only, one per line
};

/**
 * @brief Print "suzume-cli: error: <message>" to stderr
 */
void printError(std::string_view message);

/**
 * @brief Print warning message to stderr
 */
void printWarning(std::string_view message);

/**
 * @brief Print info message to stderr
 */
void printInfo(std::string_view message);

/**
 * @brief Parse a non-negative integer option.
 */
bool parseSizeOption(std::string_view value, size_t* out);

/** @brief Whether a 0-as-unlimited result cap has been reached. */
bool limitReached(size_t count, size_t limit);

/**
 * @brief Escape a string for JSON output.
 */
std::string jsonEscape(std::string_view value);

/**
 * @brief Escape a string for one-line TAB-separated output.
 *
 * Backslash, TAB, CR, and LF become \\ , \t, \r, and \n respectively.
 */
std::string tabEscape(std::string_view value);

/**
 * @brief Remove a UTF-8 byte order mark at the start of a string, if present.
 */
void stripUtf8Bom(std::string* value);

/**
 * @brief Add one source or binary user dictionary based on its file extension.
 *
 * Source TSV/legacy CSV and binary .dic files share additive load semantics.
 */
core::Expected<size_t, core::Error> loadUserDictionaryPath(Suzume* analyzer, const std::string& path);

/**
 * @brief Validate a shell-style wildcard pattern before matching it.
 *
 * The limit keeps dictionary commands responsive even when a pattern comes
 * from an untrusted or accidentally repeated shell expansion.
 */
core::Expected<bool, core::Error> validateWildcardPattern(std::string_view pattern);

/**
 * @brief Match a shell-style wildcard pattern (* and ?) without regex.
 *
 * All characters other than '*' and '?' are literal. Matching is byte-based,
 * preserving the narrow-string behavior previously used by the CLI regexes.
 */
bool wildcardMatches(std::string_view pattern, std::string_view value);

/**
 * @brief True if @p path ends with @p ext (e.g. ".dic"). ASCII, case-sensitive.
 */
bool hasExtension(std::string_view path, std::string_view ext);

/**
 * @brief Swap a trailing @p from_ext for @p to_ext (e.g. ".tsv" -> ".dic"); if
 *        @p path does not end with @p from_ext, @p to_ext is appended instead.
 */
std::string swapOrAppendExtension(std::string_view path, std::string_view from_ext, std::string_view to_ext);

/**
 * @brief Check if input is from terminal (not piped)
 */
bool isTerminal();

/**
 * @brief Get version string
 */
std::string getVersionString();

/**
 * @brief Print version information
 */
void printVersion();

/**
 * @brief Command argument structure
 */
struct CommandArgs {
  std::string command;
  std::vector<std::string> args;

  // Common options
  std::vector<std::string> dict_paths;
  std::string mode = "normal";
  OutputFormat format = OutputFormat::Tsv;
  bool verbose = false;
  bool very_verbose = false;
  bool debug = false;
  bool help = false;
  bool version = false;
  bool skip_bundled_user_dictionary = false;
  bool skip_core_dictionary = false;
  bool skip_env_config = false;
  bool compare = false;
  bool tag_option_given = false;  // any --tag-* option was parsed
  std::string parse_error;

  // Normalization options
  bool preserve_vu = true;
  bool preserve_case = true;

  // Postprocess options
  bool preserve_symbols = false;
  bool lemmatize = true;
  bool merge_compounds = false;

  // Tag generation options
  bool tag_exclude_particles = true;
  bool tag_exclude_auxiliaries = true;
  bool tag_exclude_formal_nouns = true;
  bool tag_exclude_low_info = true;
  bool tag_remove_duplicates = true;
  bool tag_use_base_form = true;
  uint8_t tag_pos_filter = 0;
  bool tag_exclude_basic = false;
  size_t tag_min_length = 2;
  size_t tag_max_tags = 0;
};

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @return Parsed CommandArgs structure
 */
CommandArgs parseArgs(int argc, char* argv[]);

/**
 * @brief Print main help message
 */
void printHelp(std::ostream& output = std::cout);

/**
 * @brief Print help for analyze command
 */
void printAnalyzeHelp(std::ostream& output = std::cout);

/**
 * @brief Print help for dict command
 */
void printDictHelp(std::ostream& output = std::cout);

/**
 * @brief Print help for test command
 */
void printTestHelp(std::ostream& output = std::cout);

}  // namespace suzume::cli

#endif  // SUZUME_CLI_CLI_COMMON_H_
