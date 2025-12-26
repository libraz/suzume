/**
 * @file auxiliaries.h
 * @brief Auxiliary verb entries for inflection analysis
 */

#ifndef SUZUME_GRAMMAR_AUXILIARIES_H_
#define SUZUME_GRAMMAR_AUXILIARIES_H_

#include <cstdint>
#include <string>
#include <vector>

namespace suzume::grammar {

/**
 * @brief Auxiliary verb entry for inflection analysis
 *
 * Represents a single auxiliary verb pattern used for reverse
 * inflection analysis. Auxiliaries are matched from the end of
 * a conjugated form to identify the base verb.
 */
struct AuxiliaryEntry {
  std::string surface;    ///< Surface form (e.g., "ています", "された")
  std::string reading;    ///< Reading in hiragana (e.g., "ています", "された")
  std::string lemma;      ///< Base/lemma form (e.g., "いる", "される")
  uint16_t left_id;       ///< What this auxiliary requires (connection input)
  uint16_t right_id;      ///< What this auxiliary provides (connection output)
  uint16_t required_conn; ///< Required connection from preceding stem
};

/**
 * @brief Get all auxiliary verb entries
 * @return Reference to static vector containing all auxiliary entries
 *
 * Entries are sorted by surface length (longest first) for greedy matching.
 *
 * Categories include:
 * - Polite forms (ます系)
 * - Past forms (た系)
 * - Te-forms (て系)
 * - Progressive (ている系)
 * - Completion (てしまう系)
 * - Preparation (ておく系)
 * - Direction (てくる/ていく系)
 * - Attempt (てみる系)
 * - Benefactive (てもらう/てくれる/てあげる系)
 * - Negation (ない系)
 * - Desire (たい系)
 * - Passive/Potential (れる/られる系)
 * - Causative (せる/させる系)
 * - Causative-passive (させられる系)
 * - Humble progressive (ておる系)
 * - Polite receiving (ていただく系)
 * - Honorific giving (てくださる系)
 * - And many more patterns...
 *
 * Total: 170+ auxiliary patterns
 */
const std::vector<AuxiliaryEntry>& getAuxiliaries();

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_AUXILIARIES_H_
