#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <utility>

#include "cli_common.h"
#include "dict_compiler.h"
#include "interactive.h"
#include "interactive_utils.h"
#include "suzume.h"

namespace suzume::cli {

bool InteractiveSession::cmdAdd(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    printError("usage: add <surface> <pos> [conj_type]");
    return true;
  }

  TsvEntry entry;
  entry.surface = args[0];
  if (entry.surface.empty()) {
    printError("surface must not be empty");
    return true;
  }
  if (const std::string issue = tsvWriteFieldIssue(entry.surface, "Surface"); !issue.empty()) {
    printError(issue);
    return true;
  }

  // Parse POS
  std::string pos_str = toUpper(args[1]);
  auto pos_opt = parsePos(pos_str);
  if (!pos_opt.has_value()) {
    printError("invalid POS: " + args[1]);
    std::cout << "Valid: NOUN, PROPN, VERB, ADJECTIVE, ADVERB, PARTICLE, "
                 "AUXILIARY, CONJUNCTION, DETERMINER, PRONOUN, PREFIX, SUFFIX, "
                 "INTERJECTION, SYMBOL, OTHER\n";
    return true;
  }
  entry.pos = pos_opt.value();

  // Parse optional conjugation type
  if (args.size() >= 3) {
    std::string conj_str = toUpper(args[2]);
    auto conj_opt = parseConjType(conj_str);
    if (!conj_opt.has_value()) {
      printError("invalid conjugation type: " + args[2]);
      std::cout << "Valid: ICHIDAN, GODAN_KA, GODAN_GA, GODAN_SA, GODAN_TA, "
                   "GODAN_NA, GODAN_BA, GODAN_MA, GODAN_RA, GODAN_WA, SURU, "
                   "KURU, I_ADJ, NA_ADJ, FAMILY, GIVEN\n";
      return true;
    }
    entry.conj_type = conj_opt.value();
  }
  if ((entry.pos == core::PartOfSpeech::Verb || entry.pos == core::PartOfSpeech::Adjective) &&
      entry.conj_type == dictionary::ConjugationType::None) {
    printError("missing conjugation type: " + entry.surface);
    return true;
  }

  // Check for existing entry with same surface and POS in current file
  for (const auto& existing : entries_) {
    if (existing.surface == entry.surface && existing.pos == entry.pos) {
      printError("entry already exists: " + entry.surface + " (" + std::string(core::posToString(entry.pos)) + ")");
      return true;
    }
    if (existing.surface == entry.surface && existing.pos != entry.pos) {
      std::cout << "Note: Entry " << entry.surface << " (" << core::posToString(existing.pos)
                << ") exists with different POS\n";
    }
  }

  // Check Layer 1 (hardcoded) for same surface+POS
  if (existsInOtherLayers(entry.surface, entry.pos)) {
    std::cout << "Note: \"" << entry.surface << "\" exists in Layer 1 (hardcoded)\n";
  }

  // Check if the word is already analyzed correctly without adding to dictionary
  // This prevents redundant entries for words handled by grammar logic
  ::suzume::Suzume* analyzer = currentAnalyzer();
  if (analyzer == nullptr) {
    printError("failed to rebuild session analyzer: " + last_error_);
    return true;
  }
  auto morphemes = analyzer->analyze(entry.surface);

  if (morphemes.size() == 1 && morphemes[0].surface == entry.surface) {
    // Already recognized as single token with correct surface
    std::cout << "Warning: \"" << entry.surface << "\" is already analyzed correctly as single token.\n";
    std::cout << "  Current analysis: " << morphemes[0].surface << " [" << core::posToString(morphemes[0].pos) << "]\n";

    // Check if POS also matches
    if (morphemes[0].pos == entry.pos) {
      // Check if conj_type also matches (for verbs/adjectives)
      if (entry.conj_type != dictionary::ConjugationType::None) {
        if (morphemes[0].conj_type == entry.conj_type) {
          std::cout << "  POS and conjugation type match. Registration is redundant.\n";
        } else {
          std::cout << "  POS matches but conjugation type differs.\n";
          std::cout << "  Current: " << conjTypeToString(morphemes[0].conj_type)
                    << ", New: " << conjTypeToString(entry.conj_type) << "\n";
        }
      } else {
        std::cout << "  POS matches. Registration may be redundant.\n";
      }
    }

    if (isTerminal()) {
      std::cerr << "Skip registration? (y/n) " << std::flush;
      std::string response;
      if (!std::getline(std::cin, response)) {
        return true;
      }
      response = trim(response);
      if (!response.empty() && (response[0] == 'y' || response[0] == 'Y')) {
        std::cout << "Skipped.\n";
        return true;
      }
    }
  }

  entries_.push_back(entry);
  modified_ = true;
  analyzer_dirty_ = true;
  std::cout << "Added: " << entry.surface << " (" << core::posToString(entry.pos) << ")\n";

  return true;
}

bool InteractiveSession::cmdRemove(const std::vector<std::string>& args) {
  if (args.empty()) {
    printError("usage: remove <surface> [pos]");
    return true;
  }

  const std::string& surface = args[0];
  std::optional<core::PartOfSpeech> pos_filter;

  if (args.size() >= 2) {
    std::string pos_str = toUpper(args[1]);
    pos_filter = parsePos(pos_str);
    if (!pos_filter.has_value()) {
      printError("invalid POS: " + args[1]);
      return true;
    }
  }

  size_t removed = 0;
  auto iter = entries_.begin();
  while (iter != entries_.end()) {
    bool match = iter->surface == surface;
    if (match && pos_filter.has_value()) {
      match = iter->pos == pos_filter.value();
    }

    if (match) {
      std::cout << "Removed: " << iter->surface << " (" << core::posToString(iter->pos) << ")\n";
      iter = entries_.erase(iter);
      ++removed;
      modified_ = true;
      analyzer_dirty_ = true;
    } else {
      ++iter;
    }
  }

  if (removed == 0) {
    std::cout << "No entries found for: " << surface << "\n";
  }

  return true;
}

bool InteractiveSession::cmdUpdate(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    printError("usage: update <surface> <pos> [conj_type]");
    return true;
  }

  const std::string& surface = args[0];
  std::string pos_str = toUpper(args[1]);
  auto pos_opt = parsePos(pos_str);
  if (!pos_opt.has_value()) {
    printError("invalid POS: " + args[1]);
    return true;
  }
  auto pos = pos_opt.value();

  // Find entry
  TsvEntry* found = nullptr;
  for (auto& entry : entries_) {
    if (entry.surface == surface && entry.pos == pos) {
      found = &entry;
      break;
    }
  }

  if (found == nullptr) {
    printError("entry not found: " + surface + " (" + pos_str + ")");
    return true;
  }

  // Update conjugation type if provided
  if (args.size() >= 3) {
    std::string conj_str = toUpper(args[2]);
    auto conj_opt = parseConjType(conj_str);
    if (!conj_opt.has_value()) {
      printError("invalid conjugation type: " + args[2]);
      return true;
    }
    found->conj_type = conj_opt.value();
  }
  if ((found->pos == core::PartOfSpeech::Verb || found->pos == core::PartOfSpeech::Adjective) &&
      found->conj_type == dictionary::ConjugationType::None) {
    printError("missing conjugation type: " + found->surface);
    return true;
  }

  modified_ = true;
  analyzer_dirty_ = true;
  std::cout << "Updated: ";
  printEntry(*found);

  return true;
}

bool InteractiveSession::cmdList(const std::vector<std::string>& args) {
  ListFilter filter;
  filter.limit = 50;
  if (const std::string error = parseListFilter(args, 0, &filter); !error.empty()) {
    printError(error);
    return true;
  }

  size_t count = 0;
  for (const auto& entry : entries_) {
    // Apply POS filter
    if (filter.pos.has_value() && entry.pos != *filter.pos) {
      continue;
    }

    // Apply pattern filter
    if (!filter.pattern.empty() && !wildcardMatches(filter.pattern, entry.surface)) {
      continue;
    }

    printEntry(entry);
    ++count;

    if (limitReached(count, filter.limit)) {
      std::cout << "...(limited to " << filter.limit << " entries)\n";
      break;
    }
  }

  std::cout << "(" << count << " entries";
  if (count < entries_.size()) {
    std::cout << " of " << entries_.size();
  }
  std::cout << ")\n";

  return true;
}

bool InteractiveSession::cmdSearch(const std::vector<std::string>& args) {
  if (args.empty()) {
    printError("usage: search <pattern>");
    return true;
  }

  std::string pattern = args[0];

  auto validation = validateWildcardPattern(pattern);
  if (!validation.hasValue()) {
    printError(validation.error().message);
    return true;
  }

  size_t count = 0;
  for (const auto& entry : entries_) {
    if (wildcardMatches(pattern, entry.surface)) {
      printEntry(entry);
      ++count;
    }
  }

  std::cout << "(" << count << " matches)\n";
  return true;
}

bool InteractiveSession::cmdValidate(const std::vector<std::string>& /* args */) {
  TsvParser parser;
  std::vector<std::string> issues;
  size_t issue_count = suzume::cli::TsvParser::validate(entries_, &issues);

  if (issue_count == 0) {
    std::cout << "OK: " << entries_.size() << " entries, no errors\n";
  } else {
    for (const auto& issue : issues) {
      printWarning(issue);
    }
    std::cout << "Found " << issue_count << " issues in " << entries_.size() << " entries\n";
  }

  return true;
}

bool InteractiveSession::cmdCompile(const std::vector<std::string>& args) {
  if (args.empty()) {
    printError("usage: compile <output.dic>");
    return true;
  }

  // Save current entries first if modified
  if (modified_) {
    if (!saveEntries()) {
      printError("failed to save before compile: " + last_error_);
      return true;
    }
    std::cout << "Saved " << entries_.size() << " entries to " << tsv_path_ << "\n";
  }

  const std::string& output_path = args[0];
  if (!hasExtension(output_path, ".dic")) {
    printError("output file must have .dic extension: " + output_path);
    return true;
  }

  namespace fs = std::filesystem;
  std::error_code error;
  const fs::path input_canonical = fs::weakly_canonical(tsv_path_, error);
  if (error) {
    printError("cannot resolve dictionary source path: " + error.message());
    return true;
  }
  const fs::path output_canonical = fs::weakly_canonical(output_path, error);
  if (error) {
    printError("cannot resolve dictionary output path: " + error.message());
    return true;
  }
  if (input_canonical == output_canonical) {
    printError("refusing to overwrite dictionary source: " + output_path);
    return true;
  }

  DictCompiler compiler;
  auto result = compiler.compile(tsv_path_, output_path);
  if (!result.hasValue()) {
    printError("compile error: " + result.error().message);
    return true;
  }

  std::cout << "Compiled " << result.value() << " entries to " << output_path << "\n";
  return true;
}

bool InteractiveSession::cmdAnalyze(const std::vector<std::string>& args) {
  if (args.empty()) {
    printError("usage: analyze <text>");
    return true;
  }

  // Join all args as text
  std::string text;
  for (size_t idx = 0; idx < args.size(); ++idx) {
    if (idx > 0) {
      text += " ";
    }
    text += args[idx];
  }

  ::suzume::Suzume* analyzer = currentAnalyzer();
  if (analyzer == nullptr) {
    printError("failed to rebuild session analyzer: " + last_error_);
    return true;
  }

  // Analyze text
  auto morphemes = analyzer->analyze(text);

  for (const auto& morpheme : morphemes) {
    std::cout << morpheme.surface << "\t" << core::posToString(morpheme.pos) << "\t" << morpheme.lemma << "\n";
  }

  return true;
}

bool InteractiveSession::cmdSave(const std::vector<std::string>& /* args */) {
  if (!saveEntries()) {
    printError("failed to save: " + last_error_);
    return true;
  }

  std::cout << "Saved " << entries_.size() << " entries to " << tsv_path_ << "\n";
  return true;
}

bool InteractiveSession::cmdHelp(const std::vector<std::string>& /* args */) {
  std::cout << R"(Commands:
  add <surface> <pos> [conj_type]
      Add a new dictionary entry
      POS: NOUN, PROPN, VERB, ADJECTIVE, ADVERB, PARTICLE, AUXILIARY,
           CONJUNCTION, DETERMINER, PRONOUN, PREFIX, SUFFIX, INTERJECTION,
           SYMBOL, OTHER
      Conj: ICHIDAN, GODAN_KA, GODAN_GA, GODAN_SA, GODAN_TA, GODAN_NA,
            GODAN_BA, GODAN_MA, GODAN_RA, GODAN_WA, SURU, KURU, I_ADJ,
            NA_ADJ, FAMILY, GIVEN
      VERB and ADJECTIVE require conj_type

  remove <surface> [pos]
      Remove entry (all with surface, or specific POS)

  update <surface> <pos> [conj_type]
      Update existing entry

  list [--pos=POS] [--pattern=PATTERN] [--limit=N]
      List entries (default limit: 50; 0 = unlimited)

  search <pattern>
      Search entries by pattern (* = wildcard)

  find <surface>
      Search across all layers (hardcoded + current file)

  stats
      Show layer and POS statistics

  layer [N]
      Show or set working layer (2=core.dic, 3=user.dic)

  import <file.tsv> [--skip-duplicates]
      Import entries from TSV file

  validate
      Check entries for errors

  compile <output.dic>
      Compile to binary dictionary

  analyze <text>
      Analyze text with the current session dictionary

  save
      Save changes to TSV file

  help
      Show this help

  quit
      Exit; saves modified entries non-interactively, or confirms before discarding them in a terminal
)";
  return true;
}

bool InteractiveSession::cmdQuit(const std::vector<std::string>& /* args */) {
  if (modified_ && !isTerminal()) {
    if (!saveEntries()) {
      printError("failed to save before non-interactive exit: " + last_error_);
      return true;
    }
    std::cerr << "Saved unsaved changes before non-interactive exit.\n";
    return false;
  }
  return !confirmDiscard();  // Exit REPL
}

bool InteractiveSession::cmdFind(const std::vector<std::string>& args) {
  if (args.empty()) {
    printError("usage: find <surface>");
    return true;
  }

  const std::string& surface = args[0];
  auto results = findInAllLayers(surface);

  if (results.empty()) {
    std::cout << "No entries found for \"" << surface << "\"\n";
  } else {
    std::cout << "Found " << results.size() << " entries for \"" << surface << "\":\n";
    for (const auto& entry : results) {
      printLayeredEntry(entry);
    }
  }

  return true;
}

bool InteractiveSession::cmdStats(const std::vector<std::string>& /* args */) {
  std::cout << "\n=== Dictionary Statistics ===\n\n";

  // Layer 1 stats
  auto layer1_counts = countByPos(layer1_cache_);
  std::cout << "Layer 1 (hardcoded): " << layer1_cache_.size() << " entries\n";
  for (const auto& [pos, count] : layer1_counts) {
    std::cout << "  " << std::setw(12) << std::left << core::posToString(pos) << ": " << count << "\n";
  }

  // Current working file stats
  std::cout << "\nLayer " << static_cast<int>(current_layer_) << " (";
  if (current_layer_ == DictLayer::Layer2) {
    std::cout << "core.dic";
  } else {
    std::cout << "user.dic";
  }
  std::cout << "): " << entries_.size() << " entries";
  if (modified_) {
    std::cout << " *";
  }
  std::cout << "\n";

  std::map<core::PartOfSpeech, size_t> working_counts;
  for (const auto& entry : entries_) {
    working_counts[entry.pos]++;
  }
  for (const auto& [pos, count] : working_counts) {
    std::cout << "  " << std::setw(12) << std::left << core::posToString(pos) << ": " << count << "\n";
  }

  std::cout << "\nTotal: " << (layer1_cache_.size() + entries_.size()) << " entries\n\n";

  return true;
}

bool InteractiveSession::cmdLayer(const std::vector<std::string>& args) {
  if (args.empty()) {
    std::cout << "Current working layer: " << static_cast<int>(current_layer_);
    if (current_layer_ == DictLayer::Layer2) {
      std::cout << " (core.dic)\n";
    } else if (current_layer_ == DictLayer::Layer3) {
      std::cout << " (user.dic)\n";
    } else {
      std::cout << " (hardcoded - read only)\n";
    }
    return true;
  }

  int layer_num = 0;
  try {
    layer_num = std::stoi(args[0]);
  } catch (...) {
    printError("invalid layer number: " + args[0]);
    return true;
  }

  if (layer_num == 1) {
    printError("layer 1 (hardcoded) is read-only");
    return true;
  }

  if (layer_num != 2 && layer_num != 3) {
    printError("invalid layer: " + args[0] + " (valid: 2, 3)");
    return true;
  }

  current_layer_ = static_cast<DictLayer>(layer_num);
  std::cout << "Working layer set to " << layer_num;
  if (current_layer_ == DictLayer::Layer2) {
    std::cout << " (core.dic)\n";
  } else {
    std::cout << " (user.dic)\n";
  }

  return true;
}

bool InteractiveSession::cmdImport(const std::vector<std::string>& args) {
  if (args.empty()) {
    printError("usage: import <file.tsv> [--skip-duplicates]");
    return true;
  }

  std::string import_path = args[0];
  bool skip_duplicates = false;

  for (size_t idx = 1; idx < args.size(); ++idx) {
    if (args[idx] == "--skip-duplicates") {
      skip_duplicates = true;
    } else {
      printError("unknown import option: " + args[idx]);
      return true;
    }
  }

  // Parse import file
  TsvParser parser;
  auto result = parser.parseFile(import_path);
  if (!result.hasValue()) {
    printError("failed to parse import file: " + result.error().message);
    return true;
  }

  const auto& import_entries = result.value();
  size_t added = 0;
  size_t skipped_dup = 0;
  size_t skipped_layer1 = 0;

  std::cout << "Importing " << import_entries.size() << " entries from " << import_path << "...\n";

  for (const auto& import_entry : import_entries) {
    // Check for duplicates in current file
    bool exists_current = false;
    for (const auto& existing : entries_) {
      if (existing.surface == import_entry.surface && existing.pos == import_entry.pos) {
        exists_current = true;
        break;
      }
    }

    if (exists_current) {
      if (skip_duplicates) {
        ++skipped_dup;
        continue;
      }
      std::cout << "  Skipped (duplicate): " << import_entry.surface << " (" << core::posToString(import_entry.pos)
                << ")\n";
      ++skipped_dup;
      continue;
    }

    // Check Layer 1
    if (existsInOtherLayers(import_entry.surface, import_entry.pos)) {
      if (skip_duplicates) {
        ++skipped_layer1;
        continue;
      }
      std::cout << "  Note: " << import_entry.surface << " exists in Layer 1 (hardcoded)\n";
    }

    entries_.push_back(import_entry);
    ++added;
    modified_ = true;
    analyzer_dirty_ = true;
  }

  std::cout << "\nImport complete:\n";
  std::cout << "  Added: " << added << "\n";
  if (skipped_dup > 0) {
    std::cout << "  Skipped (duplicates): " << skipped_dup << "\n";
  }
  if (skipped_layer1 > 0) {
    std::cout << "  Skipped (in Layer 1): " << skipped_layer1 << "\n";
  }

  return true;
}

}  // namespace suzume::cli
