#ifndef SUZUME_SUZUME_H_
#define SUZUME_SUZUME_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "analysis/analyzer.h"
#include "core/lattice.h"
#include "core/morpheme.h"
#include "core/types.h"
#include "dictionary/user_dict.h"
#include "normalize/normalizer.h"
#include "postprocess/tag_generator.h"

namespace suzume {

/**
 * @brief Suzume configuration options
 */
struct SuzumeOptions {
  core::AnalysisMode mode = core::AnalysisMode::Normal;
  bool lemmatize = true;
  bool merge_compounds = false;
  bool remove_symbols = true;         // Remove symbol-only morphemes (default: true)
  bool skip_user_dictionary = false;  // Skip auto-loading user.dic (for testing)
  bool report_scorer_config = false;  // Print scorer config status/warnings
  postprocess::TagGeneratorOptions tag_options;
  normalize::NormalizeOptions normalize_options;
  analysis::ScorerOptions scorer_options;  // Scoring parameters (tunable at runtime)
};

/**
 * @brief Main Suzume API class
 *
 * Provides a simple interface for Japanese morphological analysis
 * and tag generation.
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
   * @brief Load user dictionary from file
   * @param path Path to dictionary file (CSV format)
   * @return true on success
   * @see loadUserDictionaryResult for error details
   */
  bool loadUserDictionary(const std::string& path);

  /**
   * @brief Load user dictionary from file with error details
   * @param path Path to dictionary file (CSV format)
   * @return Number of loaded entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadUserDictionaryResult(const std::string& path);

  /**
   * @brief Load user dictionary from memory
   * @param data Dictionary data
   * @param size Data size
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
   * @brief Load binary dictionary from memory (as user dictionary)
   * @param data Dictionary data (.dic binary format)
   * @param size Data size in bytes
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
   * @brief Warnings produced while auto-loading dictionaries at construction.
   */
  std::vector<std::string> dictionaryWarnings() const;

  /**
   * @brief Analyze text into morphemes
   * @param text UTF-8 encoded Japanese text
   * @return Vector of morphemes
   */
  std::vector<core::Morpheme> analyze(std::string_view text) const;

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
   * @brief Generate tags from text with custom options
   * @param text UTF-8 encoded Japanese text
   * @param options Tag generation options (POS filter, exclude_basic, etc.)
   * @return Vector of tag entries with POS information
   */
  std::vector<postprocess::TagEntry> generateTags(std::string_view text,
                                                  const postprocess::TagGeneratorOptions& options) const;

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
