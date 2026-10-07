#ifndef SUZUME_ANALYSIS_SCORER_BIGRAM_OVERRIDES_H_
#define SUZUME_ANALYSIS_SCORER_BIGRAM_OVERRIDES_H_

#include <array>
#include <string_view>

#include "scorer.h"

namespace suzume::analysis {

// The public option name, POS pair, and storage member are one definition.
// JSON loading, environment loading, and Scorer::bigramCost all consume it.
struct BigramOverrideSpec {
  const char* name;
  core::PartOfSpeech prev;
  core::PartOfSpeech next;
  float ScorerOptions::BigramOverrides::*value;
};

using POS = core::PartOfSpeech;
using BigramOverrides = ScorerOptions::BigramOverrides;

inline constexpr std::array<BigramOverrideSpec, 14> kBigramOverrideSpecs = {{
    {"noun_to_suffix", POS::Noun, POS::Suffix, &BigramOverrides::noun_to_suffix},
    {"prefix_to_noun", POS::Prefix, POS::Noun, &BigramOverrides::prefix_to_noun},
    {"prefix_to_verb", POS::Prefix, POS::Verb, &BigramOverrides::prefix_to_verb},
    {"pron_to_aux", POS::Pronoun, POS::Auxiliary, &BigramOverrides::pron_to_aux},
    {"verb_to_verb", POS::Verb, POS::Verb, &BigramOverrides::verb_to_verb},
    {"verb_to_noun", POS::Verb, POS::Noun, &BigramOverrides::verb_to_noun},
    {"verb_to_aux", POS::Verb, POS::Auxiliary, &BigramOverrides::verb_to_aux},
    {"adj_to_aux", POS::Adjective, POS::Auxiliary, &BigramOverrides::adj_to_aux},
    {"adj_to_verb", POS::Adjective, POS::Verb, &BigramOverrides::adj_to_verb},
    {"adj_to_adj", POS::Adjective, POS::Adjective, &BigramOverrides::adj_to_adj},
    {"part_to_verb", POS::Particle, POS::Verb, &BigramOverrides::part_to_verb},
    {"part_to_noun", POS::Particle, POS::Noun, &BigramOverrides::part_to_noun},
    {"aux_to_part", POS::Auxiliary, POS::Particle, &BigramOverrides::aux_to_part},
    {"aux_to_aux", POS::Auxiliary, POS::Auxiliary, &BigramOverrides::aux_to_aux},
}};

/** The override spec whose public option name is @p name, or nullptr. */
inline const BigramOverrideSpec* findBigramSpec(std::string_view name) {
  for (const BigramOverrideSpec& spec : kBigramOverrideSpecs) {
    if (name == spec.name) {
      return &spec;
    }
  }
  return nullptr;
}

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SCORER_BIGRAM_OVERRIDES_H_
