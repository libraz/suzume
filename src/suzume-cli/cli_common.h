#ifndef SUZUME_CLI_CLI_COMMON_H_
#define SUZUME_CLI_CLI_COMMON_H_

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace suzume::cli {

/**
 * @brief Output format for analysis results
 */
enum class OutputFormat {
  Morpheme,  // Default: surface TAB pos TAB lemma
  Tags,      // Tags only, one per line
  Json,      // JSON format
  Tsv,       // TSV with all fields
  Chasen     // ChaSen-like format (Japanese POS, conjugation info)
};

/**
 * @brief Parse output format from string
 * @param str Format string (morpheme, tags, json, tsv)
 * @return OutputFormat enum value
 */
OutputFormat parseOutputFormat(std::string_view str);

/**
 * @brief Convert OutputFormat to string
 */
std::string_view outputFormatToString(OutputFormat fmt);

/**
 * @brief Print error message to stderr
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
 * @brief Read all lines from stdin
 * @return Vector of lines
 */
std::vector<std::string> readStdin();

/**
 * @brief Read single line from stdin
 * @return Line (empty if EOF)
 */
std::string readLine();

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
  OutputFormat format = OutputFormat::Morpheme;
  bool verbose = false;
  bool very_verbose = false;
  bool debug = false;
  bool help = false;
  bool no_user_dict = false;
  bool no_core_dict = false;
  bool compare = false;

  // Normalization options (defaults preserve original)
  bool normalize_vu = false;    // --normalize-vu: convert ヴ→ビ
  bool lowercase = false;       // --lowercase: convert to lowercase

  // Postprocess options
  bool preserve_symbols = false; // --preserve-symbols: keep symbols in output
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
void printHelp();

/**
 * @brief Print help for analyze command
 */
void printAnalyzeHelp();

/**
 * @brief Print help for dict command
 */
void printDictHelp();

/**
 * @brief Print help for test command
 */
void printTestHelp();

}  // namespace suzume::cli

#endif  // SUZUME_CLI_CLI_COMMON_H_
