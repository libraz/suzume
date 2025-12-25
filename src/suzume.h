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
#include "postprocess/tag_generator.h"

namespace suzume {

/**
 * @brief Suzume configuration options
 */
struct SuzumeOptions {
  core::AnalysisMode mode = core::AnalysisMode::Normal;
  bool lemmatize = true;
  bool merge_compounds = true;
  postprocess::TagGeneratorOptions tag_options;
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
   */
  bool loadUserDictionary(const std::string& path);

  /**
   * @brief Load user dictionary from memory
   * @param data Dictionary data
   * @param size Data size
   * @return true on success
   */
  bool loadUserDictionaryFromMemory(const char* data, size_t size);

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
  std::vector<core::Morpheme> analyzeDebug(std::string_view text,
                                           core::Lattice* out_lattice) const;

  /**
   * @brief Generate tags from text
   * @param text UTF-8 encoded Japanese text
   * @return Vector of tag strings
   */
  std::vector<std::string> generateTags(std::string_view text) const;

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
