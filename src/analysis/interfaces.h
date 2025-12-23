#ifndef SUZUME_ANALYSIS_INTERFACES_H_
#define SUZUME_ANALYSIS_INTERFACES_H_

#include <string>
#include <string_view>
#include <vector>

#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

/**
 * @brief Normalizer interface for dependency injection
 */
class INormalizer {
 public:
  virtual ~INormalizer() = default;

  INormalizer(const INormalizer&) = delete;
  INormalizer& operator=(const INormalizer&) = delete;
  INormalizer(INormalizer&&) = delete;
  INormalizer& operator=(INormalizer&&) = delete;

  /**
   * @brief Normalize input text
   * @param input Input text
   * @return Normalized text
   */
  virtual std::string normalize(std::string_view input) const = 0;

  /**
   * @brief Get character types for text
   * @param text Text to analyze
   * @return Vector of character types
   */
  virtual std::vector<normalize::CharType> getCharTypes(
      std::string_view text) const = 0;

 protected:
  INormalizer() = default;
};

/**
 * @brief Dictionary interface for dependency injection
 */
class IDictionaryLookup {
 public:
  virtual ~IDictionaryLookup() = default;

  IDictionaryLookup(const IDictionaryLookup&) = delete;
  IDictionaryLookup& operator=(const IDictionaryLookup&) = delete;
  IDictionaryLookup(IDictionaryLookup&&) = delete;
  IDictionaryLookup& operator=(IDictionaryLookup&&) = delete;

  /**
   * @brief Lookup entries at position
   * @param text Text to search
   * @param start_pos Start position (character index)
   * @return Vector of lookup results
   */
  virtual std::vector<dictionary::LookupResult> lookup(
      std::string_view text, size_t start_pos) const = 0;

 protected:
  IDictionaryLookup() = default;
};

/**
 * @brief Scorer interface for dependency injection
 */
class IScorer {
 public:
  virtual ~IScorer() = default;

  IScorer(const IScorer&) = delete;
  IScorer& operator=(const IScorer&) = delete;
  IScorer(IScorer&&) = delete;
  IScorer& operator=(IScorer&&) = delete;

  /**
   * @brief Calculate word cost
   * @param edge Lattice edge
   * @return Word cost
   */
  virtual float wordCost(const core::LatticeEdge& edge) const = 0;

  /**
   * @brief Calculate connection cost between edges
   * @param prev Previous edge
   * @param next Next edge
   * @return Connection cost
   */
  virtual float connectionCost(const core::LatticeEdge& prev,
                               const core::LatticeEdge& next) const = 0;

 protected:
  IScorer() = default;
};

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_INTERFACES_H_
