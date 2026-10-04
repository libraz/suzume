#include "cli_common.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>

#include "normalize/utf8.h"
#include "postprocess/tag_generator.h"
#include "suzume.h"

namespace suzume::cli {

namespace {

bool isKnownCommand(std::string_view value) {
  return value == "analyze" || value == "dict" || value == "test" || value == "version" || value == "help";
}

bool isKnownOption(std::string_view value) {
  return value == "--" || value == "-h" || value == "--help" || value == "-v" || value == "--version" ||
         value == "-V" || value == "--verbose" || value == "-VV" || value == "--very-verbose" || value == "--debug" ||
         value == "-d" || value == "--dict" || value == "-m" || value == "--mode" || value == "-f" ||
         value == "--format" || value == "--no-user-dict" || value == "--no-core-dict" ||
         value == "--skip-env-config" || value == "--compare" || value == "--normalize-vu" || value == "--lowercase" ||
         value == "--preserve-symbols" || value == "--no-lemmatize" || value == "--merge-compounds" ||
         value == "--include-particles" || value == "--include-auxiliaries" || value == "--include-formal-nouns" ||
         value == "--include-low-info" || value == "--tag-keep-duplicates" || value == "--tag-use-surface" ||
         value == "--tag-pos" || value == "--tag-exclude-basic" || value == "--tag-min-length" ||
         value == "--tag-max-tags" || value == "--expect" || value == "--file";
}

bool isValidMode(std::string_view value) {
  return value == "normal" || value == "search" || value == "split";
}

bool tryParseOutputFormat(std::string_view value, OutputFormat* output) {
  if (output == nullptr) {
    return false;
  }
  if (value == "morpheme") {
    *output = OutputFormat::Morpheme;
  } else if (value == "tags") {
    *output = OutputFormat::Tags;
  } else if (value == "json") {
    *output = OutputFormat::Json;
  } else if (value == "tsv") {
    *output = OutputFormat::Morpheme;
  } else if (value == "chasen") {
    *output = OutputFormat::Chasen;
  } else {
    return false;
  }
  return true;
}

bool takeOptionValue(int argc, char* argv[], int* index, std::string_view option, std::string* value,
                     std::string* error) {
  if (*index + 1 >= argc || isKnownOption(argv[*index + 1])) {
    *error = "Missing value for " + std::string(option);
    return false;
  }
  *value = argv[++(*index)];
  return true;
}

bool parseTagPos(std::string_view value, uint8_t* filter) {
  uint8_t bit = 0;
  if (value == "noun") {
    bit = postprocess::kTagPosNoun;
  } else if (value == "verb") {
    bit = postprocess::kTagPosVerb;
  } else if (value == "adjective") {
    bit = postprocess::kTagPosAdjective;
  } else if (value == "adverb") {
    bit = postprocess::kTagPosAdverb;
  } else if (value == "particle") {
    bit = postprocess::kTagPosParticle;
  } else if (value == "auxiliary") {
    bit = postprocess::kTagPosAuxiliary;
  } else {
    return false;
  }
  *filter = static_cast<uint8_t>(*filter | bit);  // NOLINT(hicpp-signed-bitwise): bit flag operation
  return true;
}

}  // namespace

void printError(std::string_view message) {
  std::cerr << "error: " << message << "\n";
}

void printWarning(std::string_view message) {
  std::cerr << "warning: " << message << "\n";
}

void printInfo(std::string_view message) {
  std::cerr << "info: " << message << "\n";
}

bool parseSizeOption(std::string_view value, size_t* out) {
  if (out == nullptr || value.empty()) {
    return false;
  }
  for (char chr : value) {
    if (chr < '0' || chr > '9') {
      return false;
    }
  }

  try {
    size_t parsed_len = 0;
    unsigned long long parsed = std::stoull(std::string(value), &parsed_len, 10);
    if (parsed_len != value.size()) {
      return false;
    }
    // std::stoull already rejects values exceeding unsigned long long. Only when
    // size_t is narrower than unsigned long long (e.g. ILP32) can a parsed value
    // still overflow size_t; on LP64 the two share a width, so this guard is
    // compiled out rather than left as an always-false comparison.
    if constexpr (std::numeric_limits<size_t>::max() < std::numeric_limits<unsigned long long>::max()) {
      if (parsed > std::numeric_limits<size_t>::max()) {
        return false;
      }
    }
    *out = static_cast<size_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

bool limitReached(size_t count, size_t limit) {
  return limit > 0 && count >= limit;
}

std::string jsonEscape(std::string_view value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');

  size_t idx = 0;
  while (idx < value.size()) {
    unsigned char chr = static_cast<unsigned char>(value[idx]);

    // ASCII range: apply JSON escaping for control/special characters.
    if (chr < 0x80) {
      switch (chr) {
        case '"':
          out << "\\\"";
          break;
        case '\\':
          out << "\\\\";
          break;
        case '\b':
          out << "\\b";
          break;
        case '\f':
          out << "\\f";
          break;
        case '\n':
          out << "\\n";
          break;
        case '\r':
          out << "\\r";
          break;
        case '\t':
          out << "\\t";
          break;
        default:
          if (chr < 0x20) {
            out << "\\u" << std::setw(4) << static_cast<int>(chr);
          } else {
            out << static_cast<char>(chr);
          }
          break;
      }
      ++idx;
      continue;
    }

    // Multi-byte UTF-8: determine the expected sequence length from the lead
    // byte, then verify every continuation byte before passing it through.
    size_t seq_len = 0;
    bool scalar_prefix_valid = false;
    if (chr >= 0xC2 && chr <= 0xDF) {
      seq_len = 2;
      scalar_prefix_valid = true;
    } else if (chr >= 0xE0 && chr <= 0xEF) {
      seq_len = 3;
      scalar_prefix_valid = true;
    } else if (chr >= 0xF0 && chr <= 0xF4) {
      seq_len = 4;
      scalar_prefix_valid = true;
    }

    bool valid = scalar_prefix_valid && idx + seq_len <= value.size();
    if (valid) {
      for (size_t off = 1; off < seq_len; ++off) {
        if ((static_cast<unsigned char>(value[idx + off]) & 0xC0) != 0x80) {
          valid = false;
          break;
        }
      }
    }
    if (valid && seq_len == 3) {
      const auto second = static_cast<unsigned char>(value[idx + 1]);
      valid = !((chr == 0xE0 && second < 0xA0) || (chr == 0xED && second >= 0xA0));
    }
    if (valid && seq_len == 4) {
      const auto second = static_cast<unsigned char>(value[idx + 1]);
      valid = !((chr == 0xF0 && second < 0x90) || (chr == 0xF4 && second > 0x8F));
    }

    if (valid) {
      // Emit the validated multi-byte sequence unchanged.
      for (size_t off = 0; off < seq_len; ++off) {
        out << value[idx + off];
      }
      idx += seq_len;
    } else {
      // Replace a stray/invalid byte with the Unicode replacement character so
      // the output stays well-formed JSON.
      out << "\\ufffd";
      ++idx;
    }
  }

  return out.str();
}

std::string tabEscape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char chr : value) {
    switch (chr) {
      case '\\':
        escaped += "\\\\";
        break;
      case '\t':
        escaped += "\\t";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped += chr;
        break;
    }
  }
  return escaped;
}

void stripUtf8Bom(std::string* value) {
  if (value != nullptr && normalize::startsWithUtf8Bom(*value)) {
    value->erase(0, normalize::kUtf8Bom.size());
  }
}

core::Expected<size_t, core::Error> loadUserDictionaryPath(Suzume* analyzer, const std::string& path) {
  if (analyzer == nullptr) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Analyzer must not be null"));
  }

  std::string extension;
  if (path.size() >= 4) {
    extension = path.substr(path.size() - 4);
    for (char& chr : extension) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
  }
  if (extension != ".dic") {
    return analyzer->loadUserDictionaryResult(path);
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::FileNotFound, "Failed to open binary dictionary file: " + path));
  }
  const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (file.bad()) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::DictionaryLoadFailed, "Failed to read binary dictionary file: " + path));
  }
  return analyzer->loadBinaryDictionaryResult(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool hasExtension(std::string_view path, std::string_view ext) {
  if (path.size() < ext.size()) {
    return false;
  }
  const std::string_view suffix = path.substr(path.size() - ext.size());
  for (size_t idx = 0; idx < ext.size(); ++idx) {
    const auto suffix_chr = static_cast<unsigned char>(suffix[idx]);
    const auto expected_chr = static_cast<unsigned char>(ext[idx]);
    if (std::tolower(suffix_chr) != std::tolower(expected_chr)) {
      return false;
    }
  }
  return true;
}

std::string swapOrAppendExtension(std::string_view path, std::string_view from_ext, std::string_view to_ext) {
  if (hasExtension(path, from_ext)) {
    return std::string(path.substr(0, path.size() - from_ext.size())) + std::string(to_ext);
  }
  return std::string(path) + std::string(to_ext);
}

core::Expected<bool, core::Error> validateWildcardPattern(std::string_view pattern) {
  constexpr size_t kMaxWildcardStars = 64;
  size_t star_count = 0;
  for (char chr : pattern) {
    if (chr == '*' && ++star_count > kMaxWildcardStars) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Wildcard pattern has too many '*' characters (maximum 64)"));
    }
  }
  return true;
}

bool wildcardMatches(std::string_view pattern, std::string_view value) {
  size_t pattern_pos = 0;
  size_t value_pos = 0;
  size_t star_pos = std::string_view::npos;
  size_t retry_value_pos = 0;

  while (value_pos < value.size()) {
    if (pattern_pos < pattern.size() && (pattern[pattern_pos] == '?' || pattern[pattern_pos] == value[value_pos])) {
      ++pattern_pos;
      ++value_pos;
    } else if (pattern_pos < pattern.size() && pattern[pattern_pos] == '*') {
      star_pos = pattern_pos++;
      retry_value_pos = value_pos;
    } else if (star_pos != std::string_view::npos) {
      pattern_pos = star_pos + 1;
      value_pos = ++retry_value_pos;
    } else {
      return false;
    }
  }

  while (pattern_pos < pattern.size() && pattern[pattern_pos] == '*') {
    ++pattern_pos;
  }
  return pattern_pos == pattern.size();
}

bool isTerminal() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
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
  bool positional_only = false;
  while (idx < argc) {
    std::string arg = argv[idx];

    // After a literal "--", every remaining argument is positional.
    if (positional_only) {
      if (args.command.empty()) {
        args.command = "analyze";
      }
      args.args.push_back(arg);
      ++idx;
      continue;
    }

    if (arg == "--") {
      if (args.command.empty()) {
        args.command = "analyze";
      }
      positional_only = true;
      ++idx;
      continue;
    }

    if (arg == "-h" || arg == "--help") {
      args.help = true;
      ++idx;
      continue;
    }

    if (arg == "-v" || arg == "--version") {
      args.version = true;
      ++idx;
      continue;
    }

    if (arg == "-V" || arg == "--verbose") {
      args.verbose = true;
      ++idx;
      continue;
    }

    if (arg == "-VV" || arg == "--very-verbose") {
      args.verbose = true;
      args.very_verbose = true;
      args.debug = true;
      ++idx;
      continue;
    }

    // Dict owns its complete option grammar. Test owns only its expectation,
    // input-file, and benchmark options; shared analysis options continue
    // through the regular parser below.
    if (args.command == "dict") {
      args.args.push_back(arg);
      ++idx;
      continue;
    }
    if (args.command == "test" &&
        (arg == "--expect" || arg.rfind("--expect=", 0) == 0 || arg == "-f" || arg == "--file" ||
         arg.rfind("--file=", 0) == 0 || arg.rfind("--iterations=", 0) == 0 || arg.rfind("--samples=", 0) == 0 ||
         arg.rfind("--warmup=", 0) == 0)) {
      args.args.push_back(arg);
      ++idx;
      continue;
    }

    if (arg[0] != '-') {
      if (args.command.empty() && isKnownCommand(arg)) {
        args.command = arg;
      } else {
        if (args.command.empty()) {
          args.command = "analyze";
        }
        args.args.push_back(arg);
      }
      ++idx;
      continue;
    }

    if (arg == "--debug") {
      args.debug = true;
      ++idx;
      continue;
    }

    if (arg == "-d" || arg == "--dict") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      args.dict_paths.push_back(std::move(value));
      ++idx;
      continue;
    }
    if (arg.rfind("--dict=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--dict="));
      if (value.empty()) {
        args.parse_error = "Missing value for --dict";
        return args;
      }
      args.dict_paths.push_back(std::move(value));
      ++idx;
      continue;
    }

    if (arg == "-m" || arg == "--mode") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!isValidMode(value)) {
        args.parse_error = "Invalid mode: " + value + " (expected normal, search, or split)";
        return args;
      }
      args.mode = std::move(value);
      ++idx;
      continue;
    }
    if (arg.rfind("--mode=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--mode="));
      if (!isValidMode(value)) {
        args.parse_error = "Invalid mode: " + value + " (expected normal, search, or split)";
        return args;
      }
      args.mode = std::move(value);
      ++idx;
      continue;
    }

    if (arg == "-f" || arg == "--format") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!tryParseOutputFormat(value, &args.format)) {
        args.parse_error = "Invalid format: " + value + " (expected morpheme, tags, json, tsv, or chasen)";
        return args;
      }
      ++idx;
      continue;
    }
    if (arg.rfind("--format=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--format="));
      if (!tryParseOutputFormat(value, &args.format)) {
        args.parse_error = "Invalid format: " + value + " (expected morpheme, tags, json, tsv, or chasen)";
        return args;
      }
      ++idx;
      continue;
    }

    if (arg == "--no-user-dict") {
      args.no_user_dict = true;
      ++idx;
      continue;
    }

    if (arg == "--no-core-dict") {
      args.no_core_dict = true;
      ++idx;
      continue;
    }

    if (arg == "--skip-env-config") {
      args.skip_env_config = true;
      ++idx;
      continue;
    }

    if (arg == "--compare") {
      args.compare = true;
      ++idx;
      continue;
    }

    if (arg == "--normalize-vu") {
      args.normalize_vu = true;
      ++idx;
      continue;
    }

    if (arg == "--lowercase") {
      args.lowercase = true;
      ++idx;
      continue;
    }

    if (arg == "--preserve-symbols") {
      args.preserve_symbols = true;
      ++idx;
      continue;
    }

    if (arg == "--no-lemmatize") {
      args.no_lemmatize = true;
      ++idx;
      continue;
    }

    if (arg == "--merge-compounds") {
      args.merge_compounds = true;
      ++idx;
      continue;
    }

    if (arg == "--include-particles") {
      args.tag_include_particles = true;
      ++idx;
      continue;
    }

    if (arg == "--include-auxiliaries") {
      args.tag_include_auxiliaries = true;
      ++idx;
      continue;
    }

    if (arg == "--include-formal-nouns") {
      args.tag_include_formal_nouns = true;
      ++idx;
      continue;
    }

    if (arg == "--include-low-info") {
      args.tag_include_low_info = true;
      ++idx;
      continue;
    }

    if (arg == "--tag-keep-duplicates") {
      args.tag_keep_duplicates = true;
      ++idx;
      continue;
    }

    if (arg == "--tag-use-surface") {
      args.tag_use_surface = true;
      ++idx;
      continue;
    }

    if (arg == "--tag-pos") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!parseTagPos(value, &args.tag_pos_filter)) {
        args.parse_error =
            "Invalid --tag-pos value: " + value + " (expected noun, verb, adjective, adverb, particle, or auxiliary)";
        return args;
      }
      ++idx;
      continue;
    }
    if (arg.rfind("--tag-pos=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--tag-pos="));
      if (!parseTagPos(value, &args.tag_pos_filter)) {
        args.parse_error =
            "Invalid --tag-pos value: " + value + " (expected noun, verb, adjective, adverb, particle, or auxiliary)";
        return args;
      }
      ++idx;
      continue;
    }

    if (arg == "--tag-exclude-basic") {
      args.tag_exclude_basic = true;
      ++idx;
      continue;
    }

    if (arg == "--tag-min-length") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!parseSizeOption(value, &args.tag_min_length)) {
        args.parse_error = "Invalid --tag-min-length value: " + value;
        return args;
      }
      ++idx;
      continue;
    }
    if (arg.rfind("--tag-min-length=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--tag-min-length="));
      if (!parseSizeOption(value, &args.tag_min_length)) {
        args.parse_error = "Invalid --tag-min-length value: " + value;
        return args;
      }
      ++idx;
      continue;
    }

    if (arg == "--tag-max-tags") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!parseSizeOption(value, &args.tag_max_tags)) {
        args.parse_error = "Invalid --tag-max-tags value: " + value;
        return args;
      }
      ++idx;
      continue;
    }
    if (arg.rfind("--tag-max-tags=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--tag-max-tags="));
      if (!parseSizeOption(value, &args.tag_max_tags)) {
        args.parse_error = "Invalid --tag-max-tags value: " + value;
        return args;
      }
      ++idx;
      continue;
    }

    if (args.command == "test") {
      args.args.push_back(arg);
      ++idx;
      continue;
    }
    if (args.command.empty()) {
      args.command = "analyze";
    }
    args.parse_error = "Unknown analysis option: " + arg;
    return args;
  }

  // Default command is analyze
  if (args.command.empty() && !args.help) {
    args.command = "analyze";
  }

  return args;
}

void printHelp(std::ostream& output) {
  output << R"(suzume-cli - Japanese morphological analyzer

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
  -f, --format FMT       Output format: morpheme, tags, json, tsv, chasen
  -V, --verbose          Verbose output
  -VV, --very-verbose    Very verbose output (includes lattice dump)
  --debug                Write lattice diagnostics to stderr; preserves the selected output format
  --no-user-dict         Disable user dictionary
  --no-core-dict         Disable auto-loaded core.dic
  --skip-env-config      Ignore scorer configuration environment variables
  --compare              Write user-dictionary comparison diagnostics to stderr; preserves output format
  --normalize-vu         Normalize ヴ to ビ etc. (default: preserve)
  --lowercase            Convert ASCII to lowercase (default: preserve)
  --preserve-symbols     Keep symbols/emoji in output (default: remove)
  --no-lemmatize         Keep source lemmas without post-analysis correction
  --merge-compounds      Merge consecutive noun compounds
  --include-particles    Include particles in tag output
  --include-auxiliaries  Include auxiliaries in tag output
  --include-formal-nouns Include formal nouns in tag output
  --include-low-info     Include low-information words in tag output
  --tag-keep-duplicates  Keep duplicate tags
  --tag-use-surface      Use surface instead of lemma for tags
  --tag-pos POS          Include noun, verb, adjective, adverb, particle, or auxiliary (repeatable)
  --tag-exclude-basic    Exclude hiragana-only basic words
  --tag-min-length N     Minimum tag length (default: 2)
  --tag-max-tags N       Maximum tags (default: 0, unlimited)
  -h, --help             Show help
  -v, --version          Show version

Long options that take a value also accept --option=value.

Environment:
  SUZUME_DEBUG           Debug level: 1=basic, 2=detailed, 3=trace
  SUZUME_SCORER_CONFIG   Path to a scorer-options JSON file
  SUZUME_SCORER_{SECTION}_{KEY}
                         Override one scorer option; sections are JOIN, SPLIT,
                         UNARY, OPTLEN, BIGRAM, VERB, and INFL

Examples:
  suzume-cli "text"                  Analyze text
  suzume-cli analyze -f json "text"  Analyze with JSON output
  suzume-cli dict compile user.tsv   Compile dictionary
  suzume-cli dict -i user.tsv        Interactive dictionary editor

Use 'suzume-cli [command] --help' for command-specific help.
)";
}

void printAnalyzeHelp(std::ostream& output) {
  output << R"(suzume-cli analyze - Morphological analysis

Usage:
  suzume-cli analyze [options] [text]
  suzume-cli [options] [text]         (analyze is default)

Options:
  -d, --dict PATH        Load user dictionary (can specify multiple)
  -m, --mode MODE        Analysis mode: normal, search, split
  -f, --format FMT       Output format: morpheme, tags, json, tsv, chasen
  -V, --verbose          Verbose output
  -VV, --very-verbose    Very verbose output (includes lattice dump)
  --debug                Write lattice diagnostics to stderr; preserves the selected output format
  --no-user-dict         Disable user dictionary
  --no-core-dict         Disable auto-loaded core.dic
  --skip-env-config      Ignore scorer configuration environment variables
  --compare              Write user-dictionary comparison diagnostics to stderr; preserves output format
  --normalize-vu         Normalize ヴ to ビ etc. (default: preserve)
  --lowercase            Convert ASCII to lowercase (default: preserve)
  --preserve-symbols     Keep symbols/emoji in output (default: remove)
  --no-lemmatize         Keep source lemmas without post-analysis correction
  --merge-compounds      Merge consecutive noun compounds
  --include-particles    Include particles in tag output
  --include-auxiliaries  Include auxiliaries in tag output
  --include-formal-nouns Include formal nouns in tag output
  --include-low-info     Include low-information words in tag output
  --tag-keep-duplicates  Keep duplicate tags
  --tag-use-surface      Use surface instead of lemma for tags
  --tag-pos POS          Include noun, verb, adjective, adverb, particle, or auxiliary (repeatable)
  --tag-exclude-basic    Exclude hiragana-only basic words
  --tag-min-length N     Minimum tag length (default: 2)
  --tag-max-tags N       Maximum tags (default: 0, unlimited)
  -h, --help             Show this help

Long options that take a value also accept --option=value.

Output Formats:
  morpheme               surface TAB pos TAB lemma TAB start TAB end (default)
  tags                   tag TAB pos, one per line
  json                   JSON format
  tsv                    Alias of morpheme
  chasen                 ChaSen-like format (Japanese POS, conjugation info)
  TAB formats escape backslash, TAB, CR, and LF as \\, \t, \r, and \n
  Morpheme start/end offsets refer to normalized text; JSON includes it as normalized_text

Examples:
  suzume-cli "text"
  suzume-cli analyze "text"
  suzume-cli analyze -d user.dic "text"
  suzume-cli analyze -f json "text"
  suzume-cli analyze -f chasen "text"
  suzume-cli analyze --compare -d user.dic "text"
  suzume-cli analyze --normalize-vu "ヴァイオリン"
  echo "text" | suzume-cli analyze

Environment:
  SUZUME_DATA_DIR        Data root containing core.dic/user.dic and core/*.tsv
  SUZUME_DEBUG           Debug level: 1=basic, 2=detailed, 3=trace
  SUZUME_SCORER_CONFIG   Path to a scorer-options JSON file
  SUZUME_SCORER_{SECTION}_{KEY}
                         Override one scorer option; sections are JOIN, SPLIT,
                         UNARY, OPTLEN, BIGRAM, VERB, and INFL
)";
}

void printDictHelp(std::ostream& output) {
  output << R"(suzume-cli dict - Dictionary management

Usage:
  suzume-cli dict [subcommand] [options] [arguments]

Subcommands:
  list <file> [--pos=POS] [--pattern=PATTERN] [--limit=N]
                         List entries in TSV or binary dictionary
  search <file> <pattern>
                         Search entries by pattern in file
  lookup <word> [file...] Look up word in L1, core/user sources, and optional dictionaries
  new <file.tsv>         Create new dictionary file
  info <file>            Show dictionary information
  validate <file>        Validate dictionary
  compile <in.tsv> [out.dic]
                         Compile to binary format (default: in.dic)
  compile <in1.tsv> <in2.tsv>... <out.dic>
                         Compile multiple inputs or glob matches into one file
      --filter-trivial   Omit 3+ character pure-kanji/katakana entries and report the count
  decompile <in.dic> [out.tsv]
                         Dump expanded entries to TSV (default: in.tsv).
                         A dump is for inspection, not compiler input.
  -i, --interactive [file.tsv]
                         Interactive mode

Environment:
  SUZUME_DATA_DIR        Data root; source lookup reads core/*.tsv and user/*.tsv below it

POS Values:
  NOUN, PROPN, VERB, ADJECTIVE, ADVERB, PARTICLE, AUXILIARY,
  CONJUNCTION, DETERMINER, PRONOUN, PREFIX, SUFFIX, INTERJECTION,
  SYMBOL, OTHER

Conjugation Types / Name Roles:
  ICHIDAN, GODAN_KA, GODAN_GA, GODAN_SA, GODAN_TA,
  GODAN_NA, GODAN_BA, GODAN_MA, GODAN_RA, GODAN_WA,
  SURU, KURU, I_ADJ, NA_ADJ, FAMILY, GIVEN

Examples:
  suzume-cli dict lookup すぎる
  suzume-cli dict new user.tsv
  suzume-cli dict list user.tsv --pos=NOUN --limit=10
  suzume-cli dict compile user.tsv
  suzume-cli dict -i user.tsv
)";
}

void printTestHelp(std::ostream& output) {
  output << R"(suzume-cli test - Verification and testing

Usage:
  suzume-cli test [subcommand] [options] [arguments]

Subcommands:
  <text> --expect <tags>
                         Test single input with expected output
  -f, --file <tests.tsv>
                         Run tests from file
  benchmark [--iterations=N] [--samples=N] [--warmup=N] [-f <corpus.txt>]
                         Report median initialize, first-analysis, and steady-analysis timing
Options:
  -d, --dict PATH        Load user dictionary
  Analysis, normalization, dictionary, and --tag-* options match `analyze`
  -h, --help             Show this help

Test File Format (TSV):
  input<TAB>expected_tags (comma-separated)

Examples:
  suzume-cli test "text" --expect "tag1,tag2"
  suzume-cli test -f tests.tsv
  suzume-cli test -f tests.tsv -d user.dic
  suzume-cli test benchmark --iterations=1000 --samples=5 --warmup=1
)";
}

}  // namespace suzume::cli
