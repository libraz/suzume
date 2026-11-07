#include "cmd_dict.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>

#include "cli_common.h"
#include "conjugator.h"
#include "dict_compiler.h"
#include "dictionary/binary_dict.h"
#include "dictionary/core_dict.h"
#include "grammar/conjugation.h"
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
    case kIAdjStem:
      return "語幹";
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
void printConjugationStems(const std::string& base_form, dictionary::ConjugationType conj_type) {
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
  file << "# Format: surface<TAB>pos<TAB>conj_type<TAB>lemma\n";
  file << "#\n";
  file << "# POS values: NOUN, PROPN, VERB, ADJECTIVE, ADVERB, PARTICLE, AUXILIARY,\n";
  file << "#   CONJUNCTION, DETERMINER, PRONOUN, PREFIX, SUFFIX, INTERJECTION, SYMBOL, OTHER\n";
  file << "# Conjugation types / name roles: ICHIDAN, GODAN_KA, GODAN_GA, "
          "GODAN_SA, GODAN_TA,\n";
  file << "#   GODAN_NA, GODAN_BA, GODAN_MA, GODAN_RA, GODAN_WA, SURU, KURU, "
          "I_ADJ, NA_ADJ,\n";
  file << "#   FAMILY, GIVEN\n";
  file << "\n";

  file.flush();
  if (!file) {
    printError("Failed to write file: " + path);
    return 1;
  }
  file.close();
  if (!file) {
    printError("Failed to finalize file: " + path);
    return 1;
  }

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
  if (hasExtension(path, ".dic")) {
    // Binary dictionary
    dictionary::BinaryDictionary dict;
    auto result = dict.loadFromFile(path);
    if (!result.hasValue()) {
      printError("Failed to load dictionary: " + result.error().message);
      return 1;
    }

    std::cout << "Dictionary: " << path << "\n";
    std::cout << "Format: Binary v" << static_cast<unsigned>(dictionary::BinaryDictHeader::kVersion) << "\n";
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

  std::cout << "Found " << issue_count << " issues in " << result.value().size() << " entries\n";
  return 1;
}

// Expand glob patterns in a path (e.g., "data/core/*.tsv")
// Returns the path as-is if it contains no glob characters or if expansion
// yields no matches.
core::Expected<std::vector<std::string>, core::Error> expandGlob(const std::string& pattern) {
  // Check if the pattern contains glob characters
  if (pattern.find('*') == std::string::npos && pattern.find('?') == std::string::npos) {
    return std::vector<std::string>{pattern};
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
    return std::vector<std::string>{pattern};
  }

  auto validation = validateWildcardPattern(file_pattern);
  if (!validation.hasValue()) {
    return core::makeUnexpected(validation.error());
  }
  std::vector<std::string> matches;

  for (const auto& entry : fs::directory_iterator(dir)) {
    if (entry.is_regular_file() && wildcardMatches(file_pattern, entry.path().filename().string())) {
      matches.push_back(entry.path().string());
    }
  }

  std::sort(matches.begin(), matches.end());

  return matches.empty() ? std::vector<std::string>{pattern} : matches;
}

// Expand all glob patterns in a list of paths
core::Expected<std::vector<std::string>, core::Error> expandGlobs(const std::vector<std::string>& paths) {
  std::vector<std::string> result;
  for (const auto& path : paths) {
    auto expanded_result = expandGlob(path);
    if (!expanded_result.hasValue()) {
      return core::makeUnexpected(expanded_result.error());
    }
    auto& expanded = expanded_result.value();
    result.reserve(result.size() + expanded.size());
    for (auto& match : expanded) {
      result.push_back(std::move(match));
    }
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
    } else if (arg.rfind("--", 0) == 0) {
      printError("Unknown compile option: " + arg);
      return 1;
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
    std::string dic_path = swapOrAppendExtension(tsv_path, ".tsv", ".dic");

    // Single arg might be a glob pattern
    auto expanded_result = expandGlob(tsv_path);
    if (!expanded_result.hasValue()) {
      printError(expanded_result.error().message);
      return 1;
    }
    auto& expanded = expanded_result.value();
    if (expanded.size() == 1 && expanded[0] == tsv_path) {
      // No glob expansion, single file compile
      if (std::filesystem::exists(dic_path)) {
        printError("Refusing to overwrite inferred dictionary output: " + dic_path +
                   " (specify an explicit output .dic path)");
        return 1;
      }
      auto result = compiler.compile(tsv_path, dic_path);
      if (!result.hasValue()) {
        printError("Compile error: " + result.error().message);
        return 1;
      }

      std::cout << "Compiled " << result.value() << " entries to " << dic_path << "\n";
      return 0;
    }

    // Only a directory-wide *.tsv glob has an unambiguous aggregate output.
    // A subset glob such as adj*.tsv must never be allowed to silently replace
    // the directory's full .dic file.
    namespace fs = std::filesystem;
    fs::path p(tsv_path);
    fs::path dir = p.parent_path();
    if (dir.empty() || p.filename() != "*.tsv") {
      printError("A subset glob requires an explicit output .dic path");
      return 1;
    }
    dic_path = dir.string() + ".dic";

    // This output was inferred rather than explicitly named.  Refuse to
    // replace an existing dictionary, which may contain shards outside the
    // glob match set.
    if (fs::exists(dic_path)) {
      printError("Refusing to overwrite inferred dictionary output: " + dic_path +
                 " (specify an explicit output .dic path)");
      return 1;
    }

    auto result = compiler.compileMultiple(expanded, dic_path);
    if (!result.hasValue()) {
      printError("Compile error: " + result.error().message);
      return 1;
    }

    std::cout << "Compiled " << result.value() << " entries to " << dic_path << "\n";
    return 0;
  }

  // Multi-file mode: last arg is output .dic, rest are input .tsv files
  const std::string& dic_path = file_args.back();
  if (!hasExtension(dic_path, ".dic")) {
    printError("Output file must have .dic extension: " + dic_path);
    return 1;
  }

  std::vector<std::string> raw_paths(file_args.begin(), file_args.end() - 1);
  auto tsv_paths_result = expandGlobs(raw_paths);
  if (!tsv_paths_result.hasValue()) {
    printError(tsv_paths_result.error().message);
    return 1;
  }
  auto& tsv_paths = tsv_paths_result.value();

  auto result = compiler.compileMultiple(tsv_paths, dic_path);
  if (!result.hasValue()) {
    printError("Compile error: " + result.error().message);
    return 1;
  }

  std::cout << "Compiled " << result.value() << " entries to " << dic_path << "\n";
  return 0;
}

int cmdDictDecompile(const std::vector<std::string>& args, bool verbose) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict decompile <input.dic> [output.tsv] [--force]");
    return 1;
  }

  bool force = false;
  std::vector<std::string> file_args;
  for (const auto& arg : args) {
    if (arg == "--force") {
      force = true;
    } else if (arg.rfind("--", 0) == 0) {
      printError("Unknown decompile option: " + arg);
      return 1;
    } else {
      file_args.push_back(arg);
    }
  }
  if (file_args.empty() || file_args.size() > 2) {
    printError("Usage: suzume-cli dict decompile <input.dic> [output.tsv] [--force]");
    return 1;
  }

  const std::string& dic_path = file_args[0];
  std::string tsv_path;
  if (file_args.size() == 2) {
    tsv_path = file_args[1];
  } else {
    std::filesystem::path output_path(dic_path);
    if (output_path.extension() == ".dic") {
      output_path.replace_extension(".dump.tsv");
    } else {
      output_path += ".dump.tsv";
    }
    tsv_path = output_path.string();
  }

  if (!force && std::filesystem::exists(tsv_path)) {
    printError("Refusing to overwrite existing decompile output: " + tsv_path + " (use --force)");
    return 1;
  }

  DictCompiler compiler;
  compiler.setVerbose(verbose);

  auto result = compiler.decompile(dic_path, tsv_path);
  if (!result.hasValue()) {
    printError("Decompile error: " + result.error().message);
    return 1;
  }

  std::cout << "Decompiled " << result.value() << " entries to " << tsv_path << "\n";
  return 0;
}

int cmdDictList(const std::vector<std::string>& args, bool /* verbose */) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict list <file> [--pos=POS] [--pattern=PATTERN] [--limit=N]");
    return 1;
  }

  const std::string& path = args[0];
  std::optional<core::PartOfSpeech> pos_filter;
  std::string pattern;
  size_t limit = 0;

  // Parse options
  for (size_t idx = 1; idx < args.size(); ++idx) {
    if (args[idx].substr(0, 6) == "--pos=") {
      const std::string value = toUpper(args[idx].substr(6));
      pos_filter = core::stringToPosStrict(value);
      if (!pos_filter.has_value()) {
        printError("Invalid POS: " + args[idx].substr(6));
        return 1;
      }
    } else if (args[idx].substr(0, 10) == "--pattern=") {
      pattern = args[idx].substr(10);
    } else if (args[idx].substr(0, 8) == "--limit=") {
      if (!parseSizeOption(args[idx].substr(8), &limit)) {
        printError("Invalid limit: " + args[idx].substr(8));
        return 1;
      }
    } else {
      printError("Unknown list option: " + args[idx]);
      return 1;
    }
  }

  bool has_pattern = !pattern.empty();
  if (has_pattern) {
    auto validation = validateWildcardPattern(pattern);
    if (!validation.hasValue()) {
      printError(validation.error().message);
      return 1;
    }
  }

  // Load dictionary
  if (hasExtension(path, ".dic")) {
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

      if (pos_filter.has_value() && entry->pos != *pos_filter) {
        continue;
      }
      if (has_pattern && !wildcardMatches(pattern, entry->surface)) {
        continue;
      }

      std::cout << tabEscape(entry->surface) << "\t" << core::posToString(entry->pos) << "\t" << tabEscape(entry->lemma)
                << "\n";

      ++count;
      if (limitReached(count, limit)) {
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
      if (pos_filter.has_value() && entry.pos != *pos_filter) {
        continue;
      }
      if (has_pattern && !wildcardMatches(pattern, entry.surface)) {
        continue;
      }

      std::cout << tabEscape(entry.surface) << "\t" << core::posToString(entry.pos) << "\t"
                << tabEscape(entry.lemma.empty() ? entry.surface : entry.lemma) << "\n";

      ++count;
      if (limitReached(count, limit)) {
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

  auto validation = validateWildcardPattern(pattern);
  if (!validation.hasValue()) {
    printError(validation.error().message);
    return 1;
  }

  size_t count = 0;
  if (hasExtension(path, ".dic")) {
    dictionary::BinaryDictionary dictionary;
    auto result = dictionary.loadFromFile(path);
    if (!result.hasValue()) {
      printError("Failed to load dictionary: " + result.error().message);
      return 1;
    }
    for (size_t idx = 0; idx < dictionary.size(); ++idx) {
      const auto* entry = dictionary.getEntry(static_cast<uint32_t>(idx));
      if (entry != nullptr && wildcardMatches(pattern, entry->surface)) {
        std::cout << entry->surface << "\t" << core::posToString(entry->pos) << "\n";
        ++count;
      }
    }
  } else {
    TsvParser parser;
    auto result = parser.parseFile(path);
    if (!result.hasValue()) {
      printError("Failed to parse TSV: " + result.error().message);
      return 1;
    }
    for (const auto& entry : result.value()) {
      if (wildcardMatches(pattern, entry.surface)) {
        std::cout << entry.surface << "\t" << core::posToString(entry.pos) << "\n";
        ++count;
      }
    }
  }

  std::cout << "(" << count << " matches)\n";
  return 0;
}

int cmdDictLookup(const std::vector<std::string>& args, bool /* verbose */) {
  if (args.empty()) {
    printError("Usage: suzume-cli dict lookup <word> [dictionary ...]");
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
      std::cout << result.entry->surface << "\t" << core::posToString(result.entry->pos);
      if (!result.entry->lemma.empty() && result.entry->lemma != result.entry->surface) {
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

  // Search editable L2 and user dictionary sources.  SUZUME_DATA_DIR names
  // the data root, so both directories are relative to it.
  std::cout << "\n=== L2 (core/*.tsv source) ===\n";
  std::filesystem::path data_root = "data";
  const char* data_dir = std::getenv("SUZUME_DATA_DIR");
  if (data_dir != nullptr) {
    data_root = data_dir;
  }

  namespace fs = std::filesystem;
  size_t source_count = 0;
  auto search_tsv_directory = [&](const fs::path& tsv_dir) {
    bool tsv_found = false;
    if (fs::exists(tsv_dir) && fs::is_directory(tsv_dir)) {
      for (const auto& entry : fs::directory_iterator(tsv_dir)) {
        if (entry.path().extension() == ".tsv") {
          TsvParser parser;
          auto result = parser.parseFile(entry.path().string());
          if (!result.hasValue()) {
            printWarning("Failed to parse source TSV " + entry.path().string() + ": " + result.error().message);
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
              ++source_count;
              found = true;
            }
          }
        }
      }
    }
    return tsv_found;
  };

  const fs::path core_dir = data_root / "core";
  const bool core_sources_found = search_tsv_directory(core_dir);
  if (!core_sources_found) {
    printWarning("No TSV files found in: " + core_dir.string());
  }

  std::cout << "\n=== User (user/*.tsv source) ===\n";
  const fs::path user_dir = data_root / "user";
  const bool user_sources_found = search_tsv_directory(user_dir);
  if (!user_sources_found) {
    printWarning("No TSV files found in: " + user_dir.string());
  }

  if (args.size() > 1) {
    std::cout << "\n=== Additional dictionaries ===\n";
    for (size_t index = 1; index < args.size(); ++index) {
      const fs::path path(args[index]);
      if (path.extension() == ".dic") {
        dictionary::BinaryDictionary dictionary;
        auto result = dictionary.loadFromFile(path.string());
        if (!result.hasValue()) {
          printWarning("Failed to load dictionary " + path.string() + ": " + result.error().message);
          continue;
        }
        for (size_t entry_index = 0; entry_index < dictionary.size(); ++entry_index) {
          const auto* entry = dictionary.getEntry(static_cast<uint32_t>(entry_index));
          if (entry != nullptr && entry->surface == word) {
            std::cout << entry->surface << "\t" << core::posToString(entry->pos) << " [" << path.filename().string()
                      << "]\n";
            ++source_count;
            found = true;
          }
        }
      } else {
        TsvParser parser;
        auto result = parser.parseFile(path.string());
        if (!result.hasValue()) {
          printWarning("Failed to parse dictionary " + path.string() + ": " + result.error().message);
          continue;
        }
        for (const auto& entry : result.value()) {
          if (entry.surface == word) {
            std::cout << entry.surface << "\t" << core::posToString(entry.pos) << " [" << path.filename().string()
                      << "]\n";
            ++source_count;
            found = true;
          }
        }
      }
    }
  }

  if (source_count == 0) {
    std::cout << "(not found)\n";
  } else {
    std::cout << "(" << source_count << " source entries)\n";
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
    printDictHelp(std::cerr);
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
  if (subcommand == "select" || subcommand == "add" || subcommand == "remove") {
    printError("dict " + subcommand + " is available only in interactive mode (use 'suzume-cli dict -i [file.tsv]')");
    return 1;
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
  printDictHelp(std::cerr);
  return 1;
}

}  // namespace suzume::cli
