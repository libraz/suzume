#ifndef SUZUME_CLI_INTERACTIVE_H_
#define SUZUME_CLI_INTERACTIVE_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "tsv_parser.h"

namespace suzume::cli {

/**
 * @brief Dictionary layer enumeration
 *
 * Layer 1: Hardcoded entries (particles, auxiliaries, etc.)
 * Layer 2: Core dictionary (basic vocabulary)
 * Layer 3: User dictionary (domain-specific)
 */
enum class DictLayer {
  Layer1 = 1,  // Hardcoded (entries/*.h)
  Layer2 = 2,  // Core dictionary (core.dic)
  Layer3 = 3   // User dictionary (user.dic)
};

/**
 * @brief Entry with layer information for cross-layer operations
 */
struct LayeredEntry {
  std::string surface;
  core::PartOfSpeech pos;
  float cost;
  std::string reading;
  DictLayer layer;
};

/**
 * @brief Interactive dictionary editing session
 *
 * Provides a REPL interface for dictionary management:
 * - add/remove/update entries
 * - search and list entries
 * - validate and compile dictionaries
 * - analyze text with current dictionary
 */
class InteractiveSession {
 public:
  /**
   * @brief Construct interactive session
   * @param tsv_path Path to TSV dictionary file (created if not exists)
   */
  explicit InteractiveSession(std::string tsv_path);

  /**
   * @brief Run interactive REPL loop
   * @return Exit code (0 = normal exit, non-zero = error)
   */
  int run();

  /**
   * @brief Process single command
   * @param line Command line input
   * @return true to continue, false to exit
   */
  bool processCommand(std::string_view line);

  /**
   * @brief Get prompt string
   */
  std::string getPrompt() const;

  /**
   * @brief Check if session has unsaved changes
   */
  bool hasUnsavedChanges() const { return modified_; }

 private:
  std::string tsv_path_;
  std::vector<TsvEntry> entries_;
  bool modified_ = false;
  std::string last_error_;
  DictLayer current_layer_ = DictLayer::Layer2;

  // Layer caches for cross-layer operations
  std::vector<LayeredEntry> layer1_cache_;  // Hardcoded entries
  std::vector<LayeredEntry> layer2_cache_;  // Core.dic entries (optional)
  std::vector<LayeredEntry> layer3_cache_;  // User.dic entries (optional)

  // Command handlers
  bool cmdAdd(const std::vector<std::string>& args);
  bool cmdRemove(const std::vector<std::string>& args);
  bool cmdUpdate(const std::vector<std::string>& args);
  bool cmdList(const std::vector<std::string>& args);
  bool cmdSearch(const std::vector<std::string>& args);
  bool cmdValidate(const std::vector<std::string>& args);
  bool cmdCompile(const std::vector<std::string>& args);
  static bool cmdAnalyze(const std::vector<std::string>& args);
  bool cmdSave(const std::vector<std::string>& args);
  static bool cmdHelp(const std::vector<std::string>& args);
  bool cmdQuit(const std::vector<std::string>& args);

  // Layer management command handlers
  bool cmdFind(const std::vector<std::string>& args);
  bool cmdStats(const std::vector<std::string>& args);
  bool cmdLayer(const std::vector<std::string>& args);
  bool cmdImport(const std::vector<std::string>& args);

  // Helper methods
  bool loadEntries();
  bool saveEntries();
  void loadLayer1Cache();
  static std::vector<std::string> parseCommandLine(std::string_view line);
  static void printEntry(const TsvEntry& entry);
  static void printLayeredEntry(const LayeredEntry& entry);
  bool confirmDiscard() const;

  // Cross-layer search
  std::vector<LayeredEntry> findInAllLayers(const std::string& surface) const;
  bool existsInOtherLayers(const std::string& surface,
                           core::PartOfSpeech pos) const;

  // Statistics helpers
  std::map<core::PartOfSpeech, size_t> countByPos(
      const std::vector<LayeredEntry>& entries) const;
};

/**
 * @brief Run interactive dictionary editing mode
 * @param tsv_path Path to TSV dictionary file
 * @param verbose Enable verbose output
 * @return Exit code
 */
int runInteractive(const std::string& tsv_path, bool verbose);

}  // namespace suzume::cli
#endif  // SUZUME_CLI_INTERACTIVE_H_
