#include "interactive.h"

#include <fstream>
#include <iostream>
#include <utility>

#include "cli_common.h"
#include "dictionary/core_dict.h"
#include "interactive_utils.h"

namespace suzume::cli {

// =============================================================================
// Constructor and Core Methods
// =============================================================================

InteractiveSession::InteractiveSession(std::string tsv_path)
    : tsv_path_(std::move(tsv_path)) {}

int InteractiveSession::run() {
  // Load Layer 1 cache (hardcoded entries)
  loadLayer1Cache();

  // Load existing entries if file exists
  std::ifstream check(tsv_path_);
  if (check.good()) {
    check.close();
    if (!loadEntries()) {
      printError("Failed to load dictionary: " + last_error_);
      return 1;
    }
    std::cout << "Loaded " << entries_.size() << " entries from " << tsv_path_
              << "\n";
  } else {
    std::cout << "Creating new dictionary: " << tsv_path_ << "\n";
  }

  std::cout << "Layer 1 (hardcoded): " << layer1_cache_.size() << " entries\n";
  std::cout << "Type 'help' for available commands.\n\n";

  // REPL loop
  while (true) {
    std::cout << getPrompt() << std::flush;

    std::string line;
    if (!std::getline(std::cin, line)) {
      // EOF
      std::cout << "\n";
      break;
    }

    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (!processCommand(line)) {
      break;
    }
  }

  return 0;
}

bool InteractiveSession::processCommand(std::string_view line) {
  auto args = parseCommandLine(line);
  if (args.empty()) {
    return true;
  }

  std::string cmd = args[0];
  for (char& chr : cmd) {
    chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
  }
  std::vector<std::string> cmd_args(args.begin() + 1, args.end());

  if (cmd == "add") {
    return cmdAdd(cmd_args);
  }
  if (cmd == "remove" || cmd == "rm" || cmd == "delete") {
    return cmdRemove(cmd_args);
  }
  if (cmd == "update" || cmd == "set") {
    return cmdUpdate(cmd_args);
  }
  if (cmd == "list" || cmd == "ls") {
    return cmdList(cmd_args);
  }
  if (cmd == "search") {
    return cmdSearch(cmd_args);
  }
  if (cmd == "find") {
    return cmdFind(cmd_args);
  }
  if (cmd == "stats") {
    return cmdStats(cmd_args);
  }
  if (cmd == "layer") {
    return cmdLayer(cmd_args);
  }
  if (cmd == "import") {
    return cmdImport(cmd_args);
  }
  if (cmd == "validate" || cmd == "check") {
    return cmdValidate(cmd_args);
  }
  if (cmd == "compile") {
    return cmdCompile(cmd_args);
  }
  if (cmd == "analyze" || cmd == "parse") {
    return cmdAnalyze(cmd_args);
  }
  if (cmd == "save" || cmd == "write") {
    return cmdSave(cmd_args);
  }
  if (cmd == "help" || cmd == "?") {
    return cmdHelp(cmd_args);
  }
  if (cmd == "quit" || cmd == "exit" || cmd == "q") {
    return cmdQuit(cmd_args);
  }

  printError("Unknown command: " + cmd);
  std::cout << "Type 'help' for available commands.\n";
  return true;
}

std::string InteractiveSession::getPrompt() const {
  std::string prompt = "suzume";
  if (modified_) {
    prompt += "*";
  }
  prompt += "> ";
  return prompt;
}

std::vector<std::string> InteractiveSession::parseCommandLine(
    std::string_view line) {
  std::vector<std::string> args;
  std::string current;
  bool in_quotes = false;
  char quote_char = 0;

  for (char chr : line) {
    if (in_quotes) {
      if (chr == quote_char) {
        in_quotes = false;
        if (!current.empty()) {
          args.push_back(current);
          current.clear();
        }
      } else {
        current += chr;
      }
    } else {
      if (chr == '"' || chr == '\'') {
        in_quotes = true;
        quote_char = chr;
      } else if (std::isspace(chr) != 0) {
        if (!current.empty()) {
          args.push_back(current);
          current.clear();
        }
      } else {
        current += chr;
      }
    }
  }

  if (!current.empty()) {
    args.push_back(current);
  }

  return args;
}

// =============================================================================
// File Operations
// =============================================================================

bool InteractiveSession::loadEntries() {
  TsvParser parser;
  auto result = parser.parseFile(tsv_path_);
  if (!result.hasValue()) {
    last_error_ = result.error().message;
    return false;
  }
  entries_ = std::move(result.value());
  modified_ = false;
  return true;
}

bool InteractiveSession::saveEntries() {
  auto result = writeTsvFile(tsv_path_, entries_);
  if (!result.hasValue()) {
    last_error_ = result.error().message;
    return false;
  }
  modified_ = false;
  return true;
}

// =============================================================================
// Helper Methods
// =============================================================================

void InteractiveSession::printEntry(const TsvEntry& entry) {
  std::cout << entry.surface << "\t" << core::posToString(entry.pos) << "\t"
            << entry.reading << "\t" << entry.cost;
  if (entry.conj_type != dictionary::ConjugationType::None) {
    std::cout << "\t" << conjTypeToString(entry.conj_type);
  }
  std::cout << "\n";
}

bool InteractiveSession::confirmDiscard() const {
  if (!modified_) {
    return true;
  }

  std::cout << "You have unsaved changes. Discard? (y/n) " << std::flush;
  std::string response;
  if (!std::getline(std::cin, response)) {
    return false;
  }
  response = trim(response);
  return !response.empty() && (response[0] == 'y' || response[0] == 'Y');
}

// =============================================================================
// Layer Management
// =============================================================================

void InteractiveSession::loadLayer1Cache() {
  dictionary::CoreDictionary core_dict;
  layer1_cache_.clear();
  layer1_cache_.reserve(core_dict.size());

  for (size_t idx = 0; idx < core_dict.size(); ++idx) {
    const auto* entry = core_dict.getEntry(static_cast<uint32_t>(idx));
    if (entry != nullptr) {
      LayeredEntry layered;
      layered.surface = entry->surface;
      layered.pos = entry->pos;
      layered.cost = entry->cost;
      layered.layer = DictLayer::Layer1;
      layer1_cache_.push_back(layered);
    }
  }
}

void InteractiveSession::printLayeredEntry(const LayeredEntry& entry) {
  std::cout << "  Layer " << static_cast<int>(entry.layer) << ": "
            << entry.surface << " [" << core::posToString(entry.pos) << "] "
            << "cost=" << entry.cost << "\n";
}

std::vector<LayeredEntry> InteractiveSession::findInAllLayers(
    const std::string& surface) const {
  std::vector<LayeredEntry> results;

  // Search Layer 1 (hardcoded)
  for (const auto& entry : layer1_cache_) {
    if (entry.surface == surface) {
      results.push_back(entry);
    }
  }

  // Search current working entries (Layer 2 or 3)
  for (const auto& entry : entries_) {
    if (entry.surface == surface) {
      LayeredEntry layered;
      layered.surface = entry.surface;
      layered.pos = entry.pos;
      layered.cost = entry.cost;
      layered.reading = entry.reading;
      layered.layer = current_layer_;
      results.push_back(layered);
    }
  }

  return results;
}

bool InteractiveSession::existsInOtherLayers(const std::string& surface,
                                              core::PartOfSpeech pos) const {
  // Check Layer 1
  for (const auto& entry : layer1_cache_) {
    if (entry.surface == surface && entry.pos == pos) {
      return true;
    }
  }
  return false;
}

std::map<core::PartOfSpeech, size_t> InteractiveSession::countByPos(
    const std::vector<LayeredEntry>& entries) const {
  std::map<core::PartOfSpeech, size_t> counts;
  for (const auto& entry : entries) {
    counts[entry.pos]++;
  }
  return counts;
}

// =============================================================================
// Entry Point
// =============================================================================

int runInteractive(const std::string& tsv_path, bool /* verbose */) {
  InteractiveSession session(tsv_path);
  return session.run();
}

}  // namespace suzume::cli
