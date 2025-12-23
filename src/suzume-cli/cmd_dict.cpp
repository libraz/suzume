#include "cmd_dict.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>

#include "dict_compiler.h"
#include "dictionary/binary_dict.h"
#include "interactive.h"
#include "tsv_parser.h"

namespace suzume::cli {

namespace {

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

int cmdDictCompile(const std::vector<std::string>& args, bool verbose) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict compile <input.tsv> [output.dic]");
    return 1;
  }

  const std::string& tsv_path = args[0];
  std::string dic_path;
  if (args.size() >= 2) {
    dic_path = args[1];
  } else {
    // Auto-generate output path: replace .tsv with .dic
    if (tsv_path.size() >= 4 && tsv_path.substr(tsv_path.size() - 4) == ".tsv") {
      dic_path = tsv_path.substr(0, tsv_path.size() - 4) + ".dic";
    } else {
      dic_path = tsv_path + ".dic";
    }
  }

  DictCompiler compiler;
  compiler.setVerbose(verbose);

  auto result = compiler.compile(tsv_path, dic_path);
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
                << "\t" << entry->lemma << "\t" << entry->cost << "\n";

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

      std::cout << entry.surface << "\t" << core::posToString(entry.pos)
                << "\t" << entry.reading << "\t" << entry.cost << "\n";

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
      std::cout << entry.surface << "\t" << core::posToString(entry.pos)
                << "\t" << entry.reading << "\t" << entry.cost << "\n";
      ++count;
    }
  }

  std::cout << "(" << count << " matches)\n";
  return 0;
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

  printError("Unknown dict subcommand: " + subcommand);
  printDictHelp();
  return 1;
}

}  // namespace cli
