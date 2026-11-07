/**
 * @file inflection.h
 * @brief Reverse inflection analysis using connection rules
 *
 * Design: Connection-based analysis replaces pattern enumeration.
 * - Use the conjugation tables to generate candidate stems
 * - Use auxiliary dictionary to match suffixes
 * - Validate with connection rules
 */

#ifndef SUZUME_GRAMMAR_INFLECTION_H_
#define SUZUME_GRAMMAR_INFLECTION_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "auxiliaries.h"
#include "conjugation.h"
#include "connection.h"
#include "inflection_scorer.h"

namespace suzume::grammar {

/**
 * @brief Analysis candidate from inflection analysis
 */
struct InflectionCandidate {
  std::string base_form;                  ///< Inferred base form: 住む
  std::string stem;                       ///< Stem: 住
  std::string suffix;                     ///< Suffix chain: んでいます
  VerbType verb_type{VerbType::Unknown};  ///< Verb type: GodanMa
  float confidence{0.0F};                 ///< Confidence: 0.0-1.0
  std::vector<std::string> morphemes;     ///< Decomposed: [ん, で, い, ます]
  bool has_explanatory_suffix = false;    ///< True if matched via のだ/んだ stripping
};

/**
 * @brief Connection-based inflection analyzer
 *
 * Algorithm:
 * 1. Try to match auxiliary suffixes from the end of the string
 * 2. For each match, find what it connects to
 * 3. Build a chain of auxiliaries back to the verb stem
 * 4. Use the conjugation tables to find possible base forms
 */
class Inflection {
 public:
  explicit Inflection(const InflectionScorerOptions& scorer_options = {}) : scorer_options_(scorer_options) {}

  /**
   * @brief Analyze surface form and infer base form
   * @param surface Surface form: 住んでいます
   * @return Candidates with possible base forms
   * @note The reference survives later inserts and one cache-generation
   *       rollover. Candidate generators routinely hold a result while
   *       analysing a related surface; its generation is not discarded until a
   *       second subsequent rollover.
   */
  const std::vector<InflectionCandidate>& analyze(std::string_view surface) const;

  /**
   * @brief Advance a full analysis-cache generation
   *
   * The active generation is retained as the previous generation so references
   * returned before this call remain valid. The generation retained before that
   * is discarded. analyze() also advances the cache before adding an entry past
   * the active-generation bound.
   */
  void rollCache() const;

  /// Number of entries in the active cache generation.
  size_t activeCacheSize() const { return cache_.size(); }

  /// Number of entries retained from the immediately preceding generation.
  size_t previousCacheSize() const { return previous_.size(); }

  /**
   * @brief Get the best candidate (highest confidence)
   */
  InflectionCandidate getBest(std::string_view surface) const;

 private:
  static constexpr size_t kMaxCacheEntries = 4096;

  void rollCacheIfFull() const;

  // Try matching auxiliary at end of surface
  std::vector<std::pair<const AuxiliaryEntry*, size_t>> matchAuxiliaries(std::string_view surface) const;

  // Analyze by peeling off auxiliaries from right (aux_chain mutated via push/pop)
  std::vector<InflectionCandidate> analyzeWithAuxiliaries(std::string_view surface, std::vector<std::string>& aux_chain,
                                                          uint16_t required_conn) const;

  // Try to match verb stem after removing auxiliaries
  std::vector<InflectionCandidate> matchVerbStem(std::string_view remaining, const std::vector<std::string>& aux_chain,
                                                 uint16_t required_conn) const;

  // Cache for analyze() results (mutable for const methods)
  // Two bounded cache generations. node-based storage keeps references stable
  // across inserts, and moving an active generation to previous_ preserves
  // references through its first rollover. Together they cap retained entries
  // at twice kMaxCacheEntries rather than growing with a chunk's probes.
  // Note: single-threaded only. Add synchronization if multi-threading is needed.
  mutable std::unordered_map<std::string, std::vector<InflectionCandidate>> cache_;
  mutable std::unordered_map<std::string, std::vector<InflectionCandidate>> previous_;
  InflectionScorerOptions scorer_options_;
};

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_INFLECTION_H_
