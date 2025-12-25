/**
 * @file verb_endings.h
 * @brief Verb ending patterns for reverse inflection analysis
 */

#ifndef SUZUME_GRAMMAR_VERB_ENDINGS_H_
#define SUZUME_GRAMMAR_VERB_ENDINGS_H_

#include <string>
#include <vector>

#include "conjugation.h"
#include "connection.h"

namespace suzume::grammar {

/**
 * @brief Verb ending pattern for reverse lookup
 *
 * Used to identify verb stems by matching ending patterns and
 * determining what connection ID the stem provides.
 */
struct VerbEnding {
  std::string suffix;       ///< Ending suffix to match (e.g., "い", "き", "")
  std::string base_suffix;  ///< Base form suffix to restore (e.g., "く", "る")
  VerbType verb_type;       ///< Verb conjugation type
  uint16_t provides_conn;   ///< Connection ID this stem provides
  bool is_onbin;            ///< True if this is euphonic (音便) form
};

/**
 * @brief Get all verb ending patterns for reverse lookup
 * @return Reference to static vector containing all verb endings
 *
 * Patterns are organized by verb type:
 * - Godan verbs: 9 rows (Ka, Ga, Sa, Ta, Ma, Ba, Na, Wa, Ra)
 * - Ichidan verbs
 * - Suru verbs (サ変)
 * - Kuru verbs (カ変)
 * - I-adjectives (い形容詞)
 *
 * Total: 100+ patterns covering all major conjugation forms
 */
const std::vector<VerbEnding>& getVerbEndings();

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_VERB_ENDINGS_H_
