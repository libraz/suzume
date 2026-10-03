/**
 * @file auxiliary_generator.h
 * @brief Auto-generation of auxiliary verb conjugation patterns
 *
 * Design: Define base forms with conjugation types, then auto-generate
 * all conjugated forms. Replaces 200+ hardcoded patterns
 * with ~25 base definitions + generation logic.
 */

#ifndef SUZUME_GRAMMAR_AUXILIARY_GENERATOR_H_
#define SUZUME_GRAMMAR_AUXILIARY_GENERATOR_H_

#include <string_view>
#include <vector>

#include "auxiliaries.h"

namespace suzume::grammar {

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

/**
 * @brief Whether @p base_form is an auxiliary base that attaches to a verb's
 *        continuative (ます, たい, やすい, にくい, すぎる, ...).
 */
bool attachesToVerbRenyokei(std::string_view base_form);

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_AUXILIARY_GENERATOR_H_
