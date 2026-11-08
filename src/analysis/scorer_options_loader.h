#ifndef SUZUME_ANALYSIS_SCORER_OPTIONS_LOADER_H_
#define SUZUME_ANALYSIS_SCORER_OPTIONS_LOADER_H_

// =============================================================================
// Scorer Options Loader
// =============================================================================
// Loads scorer options from JSON configuration files and environment variables.
// Provides partial override capability - only specified fields are updated.
//
// Environment variable format: SUZUME_SCORER_{SECTION}_{KEY}=value
//   SECTION: JOIN, SPLIT, UNARY, BIGRAM, VERB, INFL
//   KEY: Field name (e.g., compound_verb_bonus)
//
// Within the environment layer:
//   Default < SUZUME_SCORER_CONFIG file < SUZUME_SCORER_* variables
// Suzume applies this environment layer before scorer_options_json, so an
// explicit program JSON always has final priority.
// =============================================================================

#include <cstddef>
#include <string>
#include <vector>

#include "candidate_options.h"
#include "scorer.h"
#include "scorer_bigram_overrides.h"

namespace suzume::analysis {

/// Result of loading scorer options from environment
struct ScorerLoadResult {
  std::string config_path;    // Path to JSON config file (if loaded)
  int env_override_count{0};  // Number of individual env overrides applied
  std::vector<std::string> warnings;

  bool hasConfig() const { return !config_path.empty() || env_override_count > 0; }
};

/// Parses JSON and loads scorer options
class ScorerOptionsLoader {
 public:
  /// Load scorer options from file (simple interface)
  /// @param path Path to JSON config file
  /// @param options Output options (modified on success)
  /// @param error_msg Optional error message output (nullptr to ignore)
  /// @return true on success, false on failure
  static bool loadFromFile(const std::string& path, ScorerOptions& options, std::string* error_msg = nullptr);

  /// Load scorer options directly from a JSON string.
  ///
  /// Unknown sections and option names are rejected so misspelled tuning
  /// parameters cannot silently leave the defaults active.
  static bool loadFromJsonString(const std::string& json, ScorerOptions& options, std::string* error_msg = nullptr);

#ifndef __EMSCRIPTEN__
  /// Apply environment variable overrides to scorer options
  /// Environment variables: SUZUME_SCORER_{SECTION}_{KEY}=value
  /// @param options Options to modify
  /// @return Number of overrides applied
  static int applyEnvOverrides(ScorerOptions& options, bool report_warnings = true);

  /// Load scorer options from environment variables
  /// Checks SUZUME_SCORER_CONFIG for JSON file path, then individual overrides
  /// @param options Options to modify
  /// @return Result containing what was loaded
  static ScorerLoadResult loadFromEnv(ScorerOptions& options, bool report_warnings = true);
#endif
};

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SCORER_OPTIONS_LOADER_H_
