#include "cli_common.h"

#include <unistd.h>

#include <cstring>

#include "suzume.h"

namespace suzume::cli {

OutputFormat parseOutputFormat(std::string_view str) {
  if (str == "tags") {
    return OutputFormat::Tags;
  }
  if (str == "json") {
    return OutputFormat::Json;
  }
  if (str == "tsv") {
    return OutputFormat::Tsv;
  }
  return OutputFormat::Morpheme;
}

std::string_view outputFormatToString(OutputFormat fmt) {
  switch (fmt) {
    case OutputFormat::Morpheme:
      return "morpheme";
    case OutputFormat::Tags:
      return "tags";
    case OutputFormat::Json:
      return "json";
    case OutputFormat::Tsv:
      return "tsv";
  }
  return "morpheme";
}

void printError(std::string_view message) {
  std::cerr << "error: " << message << "\n";
}

void printWarning(std::string_view message) {
  std::cerr << "warning: " << message << "\n";
}

void printInfo(std::string_view message) {
  std::cerr << "info: " << message << "\n";
}

std::vector<std::string> readStdin() {
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(std::cin, line)) {
    lines.push_back(std::move(line));
  }
  return lines;
}

std::string readLine() {
  std::string line;
  if (std::getline(std::cin, line)) {
    return line;
  }
  return "";
}

bool isTerminal() {
  return isatty(STDIN_FILENO) != 0;
}

std::string getVersionString() {
  return Suzume::version();
}

void printVersion() {
  std::cout << "suzume-cli " << getVersionString() << "\n";
  std::cout << "Japanese morphological analyzer\n";
}

CommandArgs parseArgs(int argc, char* argv[]) {
  CommandArgs args;

  int idx = 1;
  while (idx < argc) {
    std::string arg = argv[idx];

    // Help flags
    if (arg == "-h" || arg == "--help") {
      args.help = true;
      ++idx;
      continue;
    }

    // Version
    if (arg == "-v" || arg == "--version") {
      printVersion();
      exit(0);
    }

    // Verbose
    if (arg == "-V" || arg == "--verbose") {
      args.verbose = true;
      ++idx;
      continue;
    }

    if (arg == "-VV" || arg == "--very-verbose") {
      args.verbose = true;
      args.very_verbose = true;
      ++idx;
      continue;
    }

    // Debug mode (show lattice candidates and scores)
    if (arg == "--debug") {
      args.debug = true;
      ++idx;
      continue;
    }

    // Dictionary path
    if ((arg == "-d" || arg == "--dict") && idx + 1 < argc) {
      args.dict_paths.emplace_back(argv[++idx]);
      ++idx;
      continue;
    }

    // Mode
    if ((arg == "-m" || arg == "--mode") && idx + 1 < argc) {
      args.mode = argv[++idx];
      ++idx;
      continue;
    }

    // Format
    if ((arg == "-f" || arg == "--format") && idx + 1 < argc) {
      args.format = parseOutputFormat(argv[++idx]);
      ++idx;
      continue;
    }

    // No user dict
    if (arg == "--no-user-dict") {
      args.no_user_dict = true;
      ++idx;
      continue;
    }

    // No core dict
    if (arg == "--no-core-dict") {
      args.no_core_dict = true;
      ++idx;
      continue;
    }

    // Compare mode
    if (arg == "--compare") {
      args.compare = true;
      ++idx;
      continue;
    }

    // Command or positional argument
    if (arg[0] != '-') {
      if (args.command.empty()) {
        // Check if it's a known command
        if (arg == "analyze" || arg == "dict" || arg == "test" ||
            arg == "version" || arg == "help") {
          args.command = arg;
        } else {
          // Not a command, treat as text input (implicit analyze)
          args.command = "analyze";
          args.args.push_back(arg);
        }
      } else {
        args.args.push_back(arg);
      }
    } else if (!args.command.empty()) {
      // Pass through unknown options to subcommands (e.g., dict -i)
      args.args.push_back(arg);
    }

    ++idx;
  }

  // Default command is analyze
  if (args.command.empty()) {
    args.command = "analyze";
  }

  return args;
}

void printHelp() {
  std::cout << R"(suzume-cli - Japanese morphological analyzer

Usage:
  suzume-cli [command] [options] [arguments]

Commands:
  analyze     Morphological analysis (default)
  dict        Dictionary management
  test        Verification and testing
  version     Show version information
  help        Show this help

Global Options:
  -d, --dict PATH        Load user dictionary (can specify multiple)
  -m, --mode MODE        Analysis mode: normal, search, split
  -f, --format FMT       Output format: morpheme, tags, json, tsv
  -V, --verbose          Verbose output
  -VV, --very-verbose    Very verbose output (includes lattice dump)
  --no-user-dict         Disable user dictionary
  --no-core-dict         Disable core dictionary
  --compare              Compare with/without user dictionary
  -h, --help             Show help
  -v, --version          Show version

Examples:
  suzume-cli "text"                  Analyze text
  suzume-cli analyze -f json "text"  Analyze with JSON output
  suzume-cli dict compile user.tsv   Compile dictionary
  suzume-cli dict -i user.tsv        Interactive dictionary editor

Use 'suzume-cli [command] --help' for command-specific help.
)";
}

void printAnalyzeHelp() {
  std::cout << R"(suzume-cli analyze - Morphological analysis

Usage:
  suzume-cli analyze [options] [text]
  suzume-cli [options] [text]         (analyze is default)

Options:
  -d, --dict PATH        Load user dictionary (can specify multiple)
  -m, --mode MODE        Analysis mode: normal, search, split
  -f, --format FMT       Output format: morpheme, tags, json, tsv
  -V, --verbose          Verbose output
  --no-user-dict         Disable user dictionary
  --compare              Compare with/without user dictionary
  -h, --help             Show this help

Examples:
  suzume-cli "text"
  suzume-cli analyze "text"
  suzume-cli analyze -d user.dic "text"
  suzume-cli analyze -f json "text"
  suzume-cli analyze --compare -d user.dic "text"
  echo "text" | suzume-cli analyze
)";
}

void printDictHelp() {
  std::cout << R"(suzume-cli dict - Dictionary management

Usage:
  suzume-cli dict [subcommand] [options] [arguments]

Subcommands:
  select <file.tsv>      Select dictionary file for editing
  add <surface> <pos> [reading] [cost] [conj_type]
                         Add entry to selected dictionary
  remove <surface> [pos] Remove entry from selected dictionary
  list [--pos=POS] [--pattern=PATTERN] [--limit=N]
                         List entries in selected dictionary
  search <pattern>       Search entries by pattern
  new <file.tsv>         Create new dictionary file
  info [file]            Show dictionary information
  validate [file]        Validate dictionary
  compile <in.tsv> [out.dic]
                         Compile to binary format (default: in.dic)
  decompile <in.dic> [out.tsv]
                         Decompile binary to TSV (default: in.tsv)
  -i, --interactive [file.tsv]
                         Interactive mode

POS Values:
  NOUN, PROPN, VERB, ADJECTIVE, ADVERB, PARTICLE,
  AUXILIARY, SYMBOL, OTHER

Conjugation Types (for VERB/ADJECTIVE):
  ICHIDAN, GODAN_KA, GODAN_GA, GODAN_SA, GODAN_TA,
  GODAN_NA, GODAN_BA, GODAN_MA, GODAN_RA, GODAN_WA,
  SURU, KURU, I_ADJ, NA_ADJ

Examples:
  suzume-cli dict new user.tsv
  suzume-cli dict select user.tsv
  suzume-cli dict add "Tokyo" PROPN "Tokyo" 0.3
  suzume-cli dict list --pos=NOUN --limit=10
  suzume-cli dict compile user.tsv
  suzume-cli dict -i user.tsv
)";
}

void printTestHelp() {
  std::cout << R"(suzume-cli test - Verification and testing

Usage:
  suzume-cli test [subcommand] [options] [arguments]

Subcommands:
  <text> --expect <tags>
                         Test single input with expected output
  -f, --file <tests.tsv>
                         Run tests from file
  benchmark [--iterations=N] [-f <corpus.txt>]
                         Run performance benchmark
  regression -f <baseline.tsv>
                         Run regression tests
  coverage -d <dict.dic> -f <corpus.txt>
                         Analyze dictionary coverage

Options:
  -d, --dict PATH        Load user dictionary
  -h, --help             Show this help

Test File Format (TSV):
  input<TAB>expected_tags (comma-separated)

Examples:
  suzume-cli test "text" --expect "tag1,tag2"
  suzume-cli test -f tests.tsv
  suzume-cli test -f tests.tsv -d user.dic
  suzume-cli test benchmark --iterations=1000
)";
}

}  // namespace suzume::cli
