/**
 * @file inflection.h
 * @brief Reverse inflection analysis using connection rules
 *
 * Design: Connection-based analysis replaces pattern enumeration.
 * - Use conjugator to generate candidate stems
 * - Use auxiliary dictionary to match suffixes
 * - Validate with connection rules
 */

#ifndef SUZUME_GRAMMAR_INFLECTION_H_
#define SUZUME_GRAMMAR_INFLECTION_H_

#include <string>
#include <string_view>
#include <vector>

#include "conjugation.h"
#include "conjugator.h"
#include "connection.h"

namespace suzume::grammar {

/**
 * @brief Auxiliary entry for inflection analysis
 */
struct AuxiliaryEntry {
  std::string surface;      // Surface form: ています
  std::string lemma;        // Base form: いる
  uint16_t left_id;         // What this requires (e.g., kAuxTeiru)
  uint16_t right_id;        // What this provides (e.g., kAuxOutMasu)
  uint16_t required_conn;   // Required connection from preceding (e.g., kAuxOutTe)
};

/**
 * @brief Analysis candidate from inflection analysis
 */
struct InflectionCandidate {
  std::string base_form;    // Inferred base form: 住む
  std::string stem;         // Stem: 住
  std::string suffix;       // Suffix chain: んでいます
  VerbType verb_type;       // Verb type: GodanMa
  float confidence;         // Confidence: 0.0-1.0
  std::vector<std::string> morphemes;  // Decomposed: [ん, で, い, ます]
};

/**
 * @brief Connection-based inflection analyzer
 *
 * Algorithm:
 * 1. Try to match auxiliary suffixes from the end of the string
 * 2. For each match, find what it connects to
 * 3. Build a chain of auxiliaries back to the verb stem
 * 4. Use conjugator to find possible base forms
 */
class Inflection {
 public:
  Inflection();

  /**
   * @brief Analyze surface form and infer base form
   * @param surface Surface form: 住んでいます
   * @return Candidates with possible base forms
   */
  std::vector<InflectionCandidate> analyze(std::string_view surface) const;

  /**
   * @brief Check if surface looks like a conjugated form
   */
  bool looksConjugated(std::string_view surface) const;

  /**
   * @brief Get the best candidate (highest confidence)
   */
  InflectionCandidate getBest(std::string_view surface) const;

 private:
  Conjugator conjugator_;
  std::vector<AuxiliaryEntry> auxiliaries_;
  ConnectionMatrix conn_matrix_;

  void initAuxiliaries();

  // Try matching auxiliary at end of surface
  std::vector<std::pair<const AuxiliaryEntry*, size_t>>
  matchAuxiliaries(std::string_view surface) const;

  // Analyze by peeling off auxiliaries from right
  std::vector<InflectionCandidate> analyzeWithAuxiliaries(
      std::string_view surface,
      const std::vector<std::string>& aux_chain,
      uint16_t required_conn) const;

  // Try to match verb stem after removing auxiliaries
  std::vector<InflectionCandidate> matchVerbStem(
      std::string_view remaining,
      const std::vector<std::string>& aux_chain,
      uint16_t required_conn) const;
};

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_INFLECTION_H_
