#include "cmd_dict.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

#include "dict_compiler.h"
#include "dictionary/binary_dict.h"
#include "dictionary/core_dict.h"
#include "grammar/conjugation.h"
#include "grammar/conjugator.h"
#include "grammar/connection.h"
#include "interactive.h"
#include "interactive_utils.h"
#include "tsv_parser.h"

namespace suzume::cli {

namespace {

// Helper to convert connection ID to form name
std::string formNameFromConnId(uint16_t conn_id) {
  using namespace grammar::conn;
  switch (conn_id) {
    case kVerbBase:
      return "終止形";
    case kVerbMizenkei:
      return "未然形";
    case kVerbRenyokei:
      return "連用形";
    case kVerbOnbinkei:
      return "音便形";
    case kVerbPotential:
      return "可能形";
    case kVerbVolitional:
      return "意志形";
    case kVerbKatei:
      return "仮定形";
    case kVerbMeireikei:
      return "命令形";
    default:
      return "";
  }
}

// Helper to print conjugation stems for a verb/adjective
void printConjugationStems(const std::string& base_form,
                           dictionary::ConjugationType conj_type) {
  auto verb_type = grammar::conjTypeToVerbType(conj_type);
  if (verb_type == grammar::VerbType::Unknown) {
    return;  // Not a conjugatable type
  }

  grammar::Conjugator conjugator;
  auto stems = conjugator.generateStems(base_form, verb_type);

  if (stems.empty()) {
    return;
  }

  std::cout << "  Conjugation stems:\n";
  for (const auto& stem : stems) {
    auto form_name = formNameFromConnId(stem.right_id);
    if (!form_name.empty()) {
      std::cout << "    " << stem.surface << "- (" << form_name << ")\n";
    } else {
      std::cout << "    " << stem.surface << "-\n";
    }
  }
}

int cmdDictNew(const std::vector<std::string>& args, bool /* verbose */) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict new <file.tsv>");
    return 1;
  }

  const std::string& path = args[0];

  // Check if file already exists
  std::ifstream check(path);
  if (check.good()) {
    printError("File already exists: " + path);
    return 1;
  }
  check.close();

  // Create empty TSV file with header
  std::ofstream file(path);
  if (!file) {
    printError("Failed to create file: " + path);
    return 1;
  }

  file << "# suzume dictionary source file\n";
  file << "# Format: surface<TAB>pos<TAB>reading<TAB>cost<TAB>conj_type\n";
  file << "#\n";
  file << "# POS values: NOUN, PROPN, VERB, ADJECTIVE, ADVERB, PARTICLE, "
          "AUXILIARY, SYMBOL, OTHER\n";
  file << "# Conjugation types (VERB/ADJECTIVE): ICHIDAN, GODAN_KA, GODAN_GA, "
          "GODAN_SA, GODAN_TA,\n";
  file << "#   GODAN_NA, GODAN_BA, GODAN_MA, GODAN_RA, GODAN_WA, SURU, KURU, "
          "I_ADJ, NA_ADJ\n";
  file << "\n";

  std::cout << "Created: " << path << "\n";
  return 0;
}

int cmdDictInfo(const std::vector<std::string>& args, bool /* verbose */) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict info <file>");
    return 1;
  }

  const std::string& path = args[0];

  // Check file extension
  if (path.size() >= 4 && path.substr(path.size() - 4) == ".dic") {
    // Binary dictionary
    dictionary::BinaryDictionary dict;
    auto result = dict.loadFromFile(path);
    if (!result.hasValue()) {
      printError("Failed to load dictionary: " + result.error().message);
      return 1;
    }

    std::cout << "Dictionary: " << path << "\n";
    std::cout << "Format: Binary v1.0\n";
    std::cout << "Entries: " << dict.size() << "\n";

    // File size
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (file) {
      std::cout << "Size: " << file.tellg() << " bytes\n";
    }
  } else {
    // TSV file
    TsvParser parser;
    auto result = parser.parseFile(path);
    if (!result.hasValue()) {
      printError("Failed to parse TSV: " + result.error().message);
      return 1;
    }

    std::cout << "Dictionary: " << path << "\n";
    std::cout << "Format: TSV (source)\n";
    std::cout << "Entries: " << result.value().size() << "\n";
    std::cout << "Comments: " << parser.commentLines() << " lines\n";
    std::cout << "Empty lines: " << parser.emptyLines() << "\n";
  }

  return 0;
}

int cmdDictValidate(const std::vector<std::string>& args, bool /* verbose */) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict validate <file.tsv>");
    return 1;
  }

  const std::string& path = args[0];

  TsvParser parser;
  auto result = parser.parseFile(path);
  if (!result.hasValue()) {
    printError("Parse error: " + result.error().message);
    return 1;
  }

  std::vector<std::string> issues;
  size_t issue_count = suzume::cli::TsvParser::validate(result.value(), &issues);

  if (issue_count == 0) {
    std::cout << "OK: " << result.value().size() << " entries, no errors\n";
    return 0;
  }

  for (const auto& issue : issues) {
    printWarning(issue);
  }

  std::cout << "Found " << issue_count << " issues in " << result.value().size()
            << " entries\n";
  return 1;
}

// Expand glob patterns in a path (e.g., "data/core/*.tsv")
// Returns the path as-is if it contains no glob characters or if expansion
// yields no matches.
std::vector<std::string> expandGlob(const std::string& pattern) {
  // Check if the pattern contains glob characters
  if (pattern.find('*') == std::string::npos &&
      pattern.find('?') == std::string::npos) {
    return {pattern};
  }

  namespace fs = std::filesystem;

  // Split into directory and filename pattern
  fs::path p(pattern);
  fs::path dir = p.parent_path();
  std::string file_pattern = p.filename().string();

  if (dir.empty()) {
    dir = ".";
  }

  if (!fs::exists(dir) || !fs::is_directory(dir)) {
    return {pattern};
  }

  // Convert glob pattern to regex
  std::string regex_str;
  for (char ch : file_pattern) {
    if (ch == '*') {
      regex_str += ".*";
    } else if (ch == '?') {
      regex_str += ".";
    } else if (ch == '.' || ch == '^' || ch == '$' || ch == '+' ||
               ch == '(' || ch == ')' || ch == '[' || ch == ']' ||
               ch == '|' || ch == '\\') {
      regex_str += '\\';
      regex_str += ch;
    } else {
      regex_str += ch;
    }
  }

  std::regex re(regex_str);
  std::vector<std::string> matches;

  for (const auto& entry : fs::directory_iterator(dir)) {
    if (entry.is_regular_file() &&
        std::regex_match(entry.path().filename().string(), re)) {
      matches.push_back(entry.path().string());
    }
  }

  std::sort(matches.begin(), matches.end());

  return matches.empty() ? std::vector<std::string>{pattern} : matches;
}

// Expand all glob patterns in a list of paths
std::vector<std::string> expandGlobs(const std::vector<std::string>& paths) {
  std::vector<std::string> result;
  for (const auto& path : paths) {
    auto expanded = expandGlob(path);
    result.insert(result.end(), expanded.begin(), expanded.end());
  }
  return result;
}

int cmdDictCompile(const std::vector<std::string>& args, bool verbose) {
  if (args.empty()) {
    printError(
        "Usage: suzume-cli dict compile [--filter-trivial] <input.tsv>... "
        "<output.dic>\n"
        "       suzume-cli dict compile [--filter-trivial] <input.tsv>  "
        "(output: input.dic)");
    return 1;
  }

  // Extract --filter-trivial flag from args
  bool filter_trivial = false;
  std::vector<std::string> file_args;
  for (const auto& arg : args) {
    if (arg == "--filter-trivial") {
      filter_trivial = true;
    } else {
      file_args.push_back(arg);
    }
  }

  if (file_args.empty()) {
    printError("No input files specified");
    return 1;
  }

  DictCompiler compiler;
  compiler.setVerbose(verbose);
  compiler.setFilterTrivial(filter_trivial);

  // Single file mode: dict compile foo.tsv -> foo.dic
  if (file_args.size() == 1) {
    const std::string& tsv_path = file_args[0];
    std::string dic_path;
    if (tsv_path.size() >= 4 &&
        tsv_path.substr(tsv_path.size() - 4) == ".tsv") {
      dic_path = tsv_path.substr(0, tsv_path.size() - 4) + ".dic";
    } else {
      dic_path = tsv_path + ".dic";
    }

    // Single arg might be a glob pattern
    auto expanded = expandGlob(tsv_path);
    if (expanded.size() == 1 && expanded[0] == tsv_path) {
      // No glob expansion, single file compile
      auto result = compiler.compile(tsv_path, dic_path);
      if (!result.hasValue()) {
        printError("Compile error: " + result.error().message);
        return 1;
      }

      std::cout << "Compiled " << result.value() << " entries to " << dic_path
                << "\n";
      return 0;
    }

    // Glob expanded to multiple files - need an output path
    // Use the directory name as base: data/core/*.tsv -> data/core.dic
    namespace fs = std::filesystem;
    fs::path p(tsv_path);
    fs::path dir = p.parent_path();
    dic_path = dir.string() + ".dic";

    auto result = compiler.compileMultiple(expanded, dic_path);
    if (!result.hasValue()) {
      printError("Compile error: " + result.error().message);
      return 1;
    }

    std::cout << "Compiled " << result.value() << " entries to " << dic_path
              << "\n";
    return 0;
  }

  // Multi-file mode: last arg is output .dic, rest are input .tsv files
  const std::string& dic_path = file_args.back();
  if (dic_path.size() < 4 || dic_path.substr(dic_path.size() - 4) != ".dic") {
    printError("Output file must have .dic extension: " + dic_path);
    return 1;
  }

  std::vector<std::string> raw_paths(file_args.begin(), file_args.end() - 1);
  auto tsv_paths = expandGlobs(raw_paths);

  auto result = compiler.compileMultiple(tsv_paths, dic_path);
  if (!result.hasValue()) {
    printError("Compile error: " + result.error().message);
    return 1;
  }

  std::cout << "Compiled " << result.value() << " entries to " << dic_path
            << "\n";
  return 0;
}

int cmdDictDecompile(const std::vector<std::string>& args, bool verbose) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict decompile <input.dic> [output.tsv]");
    return 1;
  }

  const std::string& dic_path = args[0];
  std::string tsv_path;
  if (args.size() >= 2) {
    tsv_path = args[1];
  } else {
    // Auto-generate output path: replace .dic with .tsv
    if (dic_path.size() >= 4 && dic_path.substr(dic_path.size() - 4) == ".dic") {
      tsv_path = dic_path.substr(0, dic_path.size() - 4) + ".tsv";
    } else {
      tsv_path = dic_path + ".tsv";
    }
  }

  DictCompiler compiler;
  compiler.setVerbose(verbose);

  auto result = compiler.decompile(dic_path, tsv_path);
  if (!result.hasValue()) {
    printError("Decompile error: " + result.error().message);
    return 1;
  }

  std::cout << "Decompiled " << result.value() << " entries to " << tsv_path
            << "\n";
  return 0;
}

int cmdDictList(const std::vector<std::string>& args, bool /* verbose */) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict list <file> [--pos=POS] [--limit=N]");
    return 1;
  }

  const std::string& path = args[0];
  std::string pos_filter;
  size_t limit = 0;

  // Parse options
  for (size_t idx = 1; idx < args.size(); ++idx) {
    if (args[idx].substr(0, 6) == "--pos=") {
      pos_filter = args[idx].substr(6);
    } else if (args[idx].substr(0, 8) == "--limit=") {
      limit = std::stoul(args[idx].substr(8));
    }
  }

  // Load dictionary
  if (path.size() >= 4 && path.substr(path.size() - 4) == ".dic") {
    // Binary dictionary
    dictionary::BinaryDictionary dict;
    auto result = dict.loadFromFile(path);
    if (!result.hasValue()) {
      printError("Failed to load dictionary: " + result.error().message);
      return 1;
    }

    size_t count = 0;
    for (size_t idx = 0; idx < dict.size(); ++idx) {
      const auto* entry = dict.getEntry(static_cast<uint32_t>(idx));
      if (entry == nullptr) {
        continue;
      }

      if (!pos_filter.empty() &&
          core::posToString(entry->pos) != pos_filter) {
        continue;
      }

      std::cout << entry->surface << "\t" << core::posToString(entry->pos)
                << "\t" << entry->lemma << "\t" << 0.0F /* v0.8: cost removed */ << "\n";

      ++count;
      if (limit > 0 && count >= limit) {
        break;
      }
    }

    std::cout << "(" << count << " entries)\n";
  } else {
    // TSV file
    TsvParser parser;
    auto result = parser.parseFile(path);
    if (!result.hasValue()) {
      printError("Failed to parse TSV: " + result.error().message);
      return 1;
    }

    size_t count = 0;
    for (const auto& entry : result.value()) {
      if (!pos_filter.empty() &&
          core::posToString(entry.pos) != pos_filter) {
        continue;
      }

      std::cout << entry.surface << "\t" << core::posToString(entry.pos) << "\n";

      ++count;
      if (limit > 0 && count >= limit) {
        break;
      }
    }

    std::cout << "(" << count << " entries)\n";
  }

  return 0;
}

int cmdDictSearch(const std::vector<std::string>& args, bool /* verbose */) {
  if (args.size() < 2) {
    printError("Usage: suzume-cli dict search <file> <pattern>");
    return 1;
  }

  const std::string& path = args[0];
  std::string pattern = args[1];

  // Convert simple wildcard to regex
  std::string regex_str;
  for (char chr : pattern) {
    if (chr == '*') {
      regex_str += ".*";
    } else if (chr == '?') {
      regex_str += ".";
    } else if (chr == '.' || chr == '^' || chr == '$' || chr == '+' ||
               chr == '(' || chr == ')' || chr == '[' || chr == ']' ||
               chr == '|' || chr == '\\') {
      regex_str += '\\';
      regex_str += chr;
    } else {
      regex_str += chr;
    }
  }

  std::regex regex_pattern(regex_str);

  // Load and search
  TsvParser parser;
  auto result = parser.parseFile(path);
  if (!result.hasValue()) {
    printError("Failed to parse TSV: " + result.error().message);
    return 1;
  }

  size_t count = 0;
  for (const auto& entry : result.value()) {
    if (std::regex_match(entry.surface, regex_pattern)) {
      std::cout << entry.surface << "\t" << core::posToString(entry.pos) << "\n";
      ++count;
    }
  }

  std::cout << "(" << count << " matches)\n";
  return 0;
}

int cmdDictLookup(const std::vector<std::string>& args, bool /* verbose */) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict lookup <word>");
    return 1;
  }

  const std::string& word = args[0];
  bool found = false;

  // Search L1 (hardcoded entries)
  std::cout << "=== L1 (Core Dictionary) ===\n";
  dictionary::CoreDictionary core_dict;
  size_t l1_count = 0;

  // The lookup method requires text and start position - search from start
  auto results = core_dict.lookup(word, 0);
  for (const auto& result : results) {
    // Only show exact matches (same length as input)
    if (result.entry != nullptr && result.entry->surface == word) {
      std::cout << result.entry->surface << "\t"
                << core::posToString(result.entry->pos);
      if (!result.entry->lemma.empty() &&
          result.entry->lemma != result.entry->surface) {
        std::cout << "\t(lemma: " << result.entry->lemma << ")";
      }
      std::cout << "\n";
      ++l1_count;
      found = true;
    }
  }

  if (l1_count == 0) {
    std::cout << "(not found)\n";
  } else {
    std::cout << "(" << l1_count << " entries)\n";
  }

  // Search L2 (TSV dictionary)
  std::cout << "\n=== L2 (data/core/*.tsv) ===\n";

  // Find L2 dictionary directory
  std::string tsv_dir;
  const char* data_dir = std::getenv("SUZUME_DATA_DIR");
  if (data_dir != nullptr) {
    tsv_dir = std::string(data_dir);
  } else {
    tsv_dir = "data/core";
  }

  TsvParser parser;
  size_t l2_count = 0;
  bool tsv_found = false;

  namespace fs = std::filesystem;
  if (fs::exists(tsv_dir) && fs::is_directory(tsv_dir)) {
    for (const auto& entry : fs::directory_iterator(tsv_dir)) {
      if (entry.path().extension() == ".tsv") {
        auto result = parser.parseFile(entry.path().string());
        if (!result.hasValue()) {
          continue;
        }
        tsv_found = true;
        for (const auto& e : result.value()) {
          if (e.surface == word) {
            std::cout << e.surface << "\t" << core::posToString(e.pos);
            if (e.conj_type != dictionary::ConjugationType::None) {
              std::cout << "\t(" << conjTypeToString(e.conj_type) << ")";
            }
            std::cout << " [" << entry.path().filename().string() << "]\n";
            if (e.conj_type != dictionary::ConjugationType::None) {
              printConjugationStems(e.surface, e.conj_type);
            }
            ++l2_count;
            found = true;
          }
        }
      }
    }
  }

  if (!tsv_found) {
    printWarning("No TSV files found in: " + tsv_dir);
  }

  if (l2_count == 0 && tsv_found) {
    std::cout << "(not found)\n";
  } else if (l2_count > 0) {
    std::cout << "(" << l2_count << " entries)\n";
  }

  return found ? 0 : 1;
}

}  // namespace

int cmdDictInteractive(const std::vector<std::string>& args, bool verbose) {
  std::string tsv_path = "user.tsv";
  if (!args.empty()) {
    tsv_path = args[0];
  }
  return runInteractive(tsv_path, verbose);
}

int cmdDict(const CommandArgs& args) {
  if (args.help) {
    printDictHelp();
    return 0;
  }

  if (args.args.empty()) {
    printDictHelp();
    return 1;
  }

  const std::string& subcommand = args.args[0];
  std::vector<std::string> subargs(args.args.begin() + 1, args.args.end());

  // Check for -i flag (interactive mode shorthand)
  if (subcommand == "-i") {
    return cmdDictInteractive(subargs, args.verbose);
  }
  if (subcommand == "interactive" || subcommand == "edit") {
    return cmdDictInteractive(subargs, args.verbose);
  }
  if (subcommand == "new") {
    return cmdDictNew(subargs, args.verbose);
  }
  if (subcommand == "info") {
    return cmdDictInfo(subargs, args.verbose);
  }
  if (subcommand == "validate") {
    return cmdDictValidate(subargs, args.verbose);
  }
  if (subcommand == "compile") {
    return cmdDictCompile(subargs, args.verbose);
  }
  if (subcommand == "decompile") {
    return cmdDictDecompile(subargs, args.verbose);
  }
  if (subcommand == "list") {
    return cmdDictList(subargs, args.verbose);
  }
  if (subcommand == "search") {
    return cmdDictSearch(subargs, args.verbose);
  }
  if (subcommand == "lookup") {
    return cmdDictLookup(subargs, args.verbose);
  }

  printError("Unknown dict subcommand: " + subcommand);
  printDictHelp();
  return 1;
}

}  // namespace cli
