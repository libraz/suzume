/**
 * @file auxiliary_generator.h
 * @brief Auto-generation of auxiliary verb conjugation patterns
 *
 * Design: Define base forms with conjugation types, then auto-generate
 * all conjugated forms with readings. Replaces 200+ hardcoded patterns
 * with ~25 base definitions + generation logic.
 */

#ifndef SUZUME_GRAMMAR_AUXILIARY_GENERATOR_H_
#define SUZUME_GRAMMAR_AUXILIARY_GENERATOR_H_

#include <string>
#include <vector>

#include "auxiliaries.h"
#include "conjugation.h"
#include "connection.h"

namespace suzume::grammar {

/**
 * @brief Base definition for an auxiliary verb
 *
 * Contains the base form and metadata needed to generate all conjugated forms.
 */
struct AuxiliaryBase {
  std::string surface;        ///< Base form surface (e.g., "いる")
  std::string reading;        ///< Base form reading (e.g., "いる")
  VerbType conj_type;         ///< Conjugation type for expansion
  uint16_t left_id;           ///< Connection input ID
  uint16_t required_conn;     ///< Required connection from preceding stem
};

/**
 * @brief Get all auxiliary base definitions
 * @return Vector of base definitions for auxiliary verbs
 *
 * Categories:
 * - Te-form attachments (て形接続): いる, しまう, おく, くる, みる, etc.
 * - Mizenkei attachments (未然形接続): ない, れる, られる, せる, させる
 * - Renyokei attachments (連用形接続): たい, やすい, にくい, すぎる
 * - Onbinkei attachments (音便形接続): た, て, たら, たり
 * - Special forms: ます
 */
const std::vector<AuxiliaryBase>& getAuxiliaryBases();

/**
 * @brief Expand a base definition into all conjugated forms
 * @param base The base definition to expand
 * @return Vector of all conjugated forms with readings
 *
 * Expansion rules by conjugation type:
 * - Ichidan: る → た, て, ます, ない, なかった, etc.
 * - GodanWa: う → った, って, います, わない, etc.
 * - GodanKa: く → いた, いて, きます, かない, etc.
 * - IAdjective: い → かった, くて, くない, くなかった, etc.
 * - Kuru: special irregular conjugation
 * - Unknown: no expansion (single form)
 */
std::vector<AuxiliaryEntry> expandAuxiliaryBase(const AuxiliaryBase& base);

/**
 * @brief Generate all auxiliary entries from base definitions
 * @return Vector of all auxiliary entries (expanded)
 *
 * This is the main entry point. It:
 * 1. Gets all base definitions
 * 2. Expands each into conjugated forms
 * 3. Adds special patterns that can't be auto-generated
 * 4. Sorts by surface length (longest first)
 */
std::vector<AuxiliaryEntry> generateAllAuxiliaries();

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_AUXILIARY_GENERATOR_H_
