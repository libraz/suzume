#ifndef SUZUME_SUZUME_H_
#define SUZUME_SUZUME_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "analysis/scorer.h"
#include "core/lattice.h"
#include "core/morpheme.h"
#include "core/types.h"
#include "normalize/normalizer.h"
#include "postprocess/tag_generator.h"

namespace suzume {

/**
 * @brief Suzume configuration options
 */
struct SuzumeOptions {
  core::AnalysisMode mode = core::AnalysisMode::Normal;
  bool lemmatize = true;  // Retain corrected lemmas; conjugation/POS annotations are always computed
  bool merge_compounds = false;
  bool remove_symbols = true;         // Remove symbol-only morphemes (default: true)
  bool skip_user_dictionary = false;  // Skip auto-loading user.dic (for testing)
  bool skip_core_dictionary = false;  // Skip auto-loading core.dic (for testing)
  bool skip_env_config = false;       // Ignore SUZUME_SCORER_CONFIG and SUZUME_SCORER_* variables
  bool report_scorer_config = false;  // Add scorer config status/warnings to dictionaryWarnings()
  std::string scorer_options_json;    // Final-priority JSON overrides (available on every target)
  std::string data_directory;         // Exclusive directory for core.dic and user.dic
  postprocess::TagGeneratorOptions tag_options;
  normalize::NormalizeOptions normalize_options;
  analysis::ScorerOptions scorer_options;  // Scoring parameters (tunable at runtime)
};

/**
 * @brief Main Suzume API class
 *
 * Provides a simple interface for Japanese tokenization
 * and tag generation.
 *
 * Instances are not safe for concurrent calls, including const analysis
 * methods, because analysis maintains mutable caches. Use one instance per
 * thread or externally serialize access to a shared instance.
 */
class Suzume {
 public:
  /**
   * @brief Create Suzume instance with default options
   */
  Suzume();

  /**
   * @brief Create Suzume instance with custom options
   */
  explicit Suzume(const SuzumeOptions& options);

  ~Suzume();

  // Non-copyable
  Suzume(const Suzume&) = delete;
  Suzume& operator=(const Suzume&) = delete;

  // Movable
  Suzume(Suzume&&) noexcept;
  Suzume& operator=(Suzume&&) noexcept;

  /**
   * @brief Add a user dictionary from a source file
   * @param path Path to dictionary source file (current TSV or legacy CSV)
   * @note Loads are additive until clearUserDictionaries() is called.
   * @return true on success
   * @see loadUserDictionaryResult for error details
   */
  bool loadUserDictionary(const std::string& path);

  /**
   * @brief Load user dictionary from file with error details
   * @param path Path to dictionary source file (current TSV or legacy CSV)
   * @return Number of loaded entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadUserDictionaryResult(const std::string& path);

  /**
   * @brief Add a user dictionary from source text in memory
   * @param data Dictionary data
   * @param size Data size
   * @note Loads are additive until clearUserDictionaries() is called.
   * @return true on success
   * @see loadUserDictionaryFromMemoryResult for error details
   */
  bool loadUserDictionaryFromMemory(const char* data, size_t size);

  /**
   * @brief Load user dictionary from memory with error details
   * @param data Dictionary data
   * @param size Data size
   * @return Number of loaded entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadUserDictionaryFromMemoryResult(const char* data, size_t size);

  /**
   * @brief Add a binary dictionary from memory (as a user dictionary)
   * @param data Dictionary data (.dic binary format)
   * @param size Data size in bytes
   * @note Loads are additive until clearUserDictionaries() is called.
   * @return true on success
   */
  bool loadBinaryDictionary(const uint8_t* data, size_t size);

  /**
   * @brief Load binary dictionary from memory with error details
   * @param data Dictionary data (.dic binary format)
   * @param size Data size in bytes
   * @return Number of loaded entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadBinaryDictionaryResult(const uint8_t* data, size_t size);

  /**
   * @brief Remove every source and binary user dictionary loaded at runtime
   *
   * Auto-loaded/bundled user dictionaries and built-in core dictionaries are
   * retained.
   */
  void clearUserDictionaries();

  /**
   * @brief Dictionary-loading, parsing, and scorer-configuration diagnostics.
   */
  const std::vector<std::string>& dictionaryWarnings() const;

  /**
   * @brief Whether the analyzer has a loaded L2 core binary dictionary.
   */
  bool hasCoreDictionary() const;

  /**
   * @brief Analyze text into morphemes, lenient form
   *
   * Collapses failure into an empty vector, which is indistinguishable from a
   * legitimately empty result (empty input, or input that normalizes away).
   * Use analyzeResult() when the caller has to tell the two apart.
   * The const qualifier does not imply that concurrent calls on one instance
   * are safe; see the class-level thread-safety note.
   *
   * @param text UTF-8 encoded Japanese text
   * @return Vector of morphemes, or empty vector if text is empty or invalid UTF-8
   */
  std::vector<core::Morpheme> analyze(std::string_view text) const;

  /**
   * @brief Analyze text into morphemes, reporting malformed input
   *
   * An empty vector here always means "nothing to segment" — empty input, or
   * input consisting only of characters the normalizer drops. Malformed UTF-8
   * is reported as ErrorCode::InvalidUtf8 instead.
   *
   * @param text UTF-8 encoded Japanese text
   * @return Vector of morphemes on success, or the normalizer error
   */
  core::Expected<std::vector<core::Morpheme>, core::Error> analyzeResult(std::string_view text) const;

  /**
   * @brief Analyze text and return the normalized text addressed by offsets.
   *
   * @param text UTF-8 encoded input text
   * @return Normalized text plus morphemes on success, or normalization error
   */
  core::Expected<core::AnalysisOutput, core::Error> analyzeWithNormalizedTextResult(std::string_view text) const;

  /**
   * @brief Debug analyze - returns lattice for debugging
   * @param text UTF-8 encoded Japanese text
   * @param out_lattice Output lattice (if not null)
   * @return Vector of morphemes
   */
  std::vector<core::Morpheme> analyzeDebug(std::string_view text, core::Lattice* out_lattice) const;

  /**
   * @brief Generate tags from text
   * @param text UTF-8 encoded Japanese text
   * @return Vector of tag entries with POS information
   */
  std::vector<postprocess::TagEntry> generateTags(std::string_view text) const;

  /**
   * @brief Generate tags from text while preserving analysis failures
   * @param text UTF-8 encoded Japanese text
   * @return Tag entries on success, or the underlying analysis error
   */
  core::Expected<std::vector<postprocess::TagEntry>, core::Error> generateTagsResult(std::string_view text) const;

  /**
   * @brief Generate tags from text with custom options
   * @param text UTF-8 encoded Japanese text
   * @param options Tag generation options (POS filter, exclude_basic, etc.)
   * @return Vector of tag entries with POS information
   */
  std::vector<postprocess::TagEntry> generateTags(std::string_view text,
                                                  const postprocess::TagGeneratorOptions& options) const;

  /**
   * @brief Generate tags with custom options while preserving analysis failures
   * @param text UTF-8 encoded Japanese text
   * @param options Tag generation options (POS filter, exclude_basic, etc.)
   * @return Tag entries on success, or the underlying analysis error
   */
  core::Expected<std::vector<postprocess::TagEntry>, core::Error> generateTagsResult(
      std::string_view text, const postprocess::TagGeneratorOptions& options) const;

  /**
   * @brief Get analysis mode
   */
  core::AnalysisMode mode() const;

  /**
   * @brief Set analysis mode
   */
  void setMode(core::AnalysisMode mode);

  /**
   * @brief Get version string
   */
  static std::string version();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace suzume

#endif  // SUZUME_SUZUME_H_
