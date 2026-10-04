#include "analysis/scorer.h"

#include <cmath>
#include <limits>

#include "analysis/scorer_bigram_overrides.h"

namespace suzume::analysis {

namespace {

// Convert POS to array index
constexpr size_t posToIndex(core::PartOfSpeech pos) {
  switch (pos) {
    case core::PartOfSpeech::Noun:
      return 0;
    case core::PartOfSpeech::Verb:
      return 1;
    case core::PartOfSpeech::Adjective:
      return 2;
    case core::PartOfSpeech::Adverb:
      return 3;
    case core::PartOfSpeech::Particle:
      return 4;
    case core::PartOfSpeech::Auxiliary:
      return 5;
    case core::PartOfSpeech::Conjunction:
      return 6;
    case core::PartOfSpeech::Determiner:
      return 7;
    case core::PartOfSpeech::Pronoun:
      return 8;
    case core::PartOfSpeech::Prefix:
      return 9;
    case core::PartOfSpeech::Suffix:
      return 10;
    case core::PartOfSpeech::Symbol:
      return 11;
    case core::PartOfSpeech::Interjection:
    case core::PartOfSpeech::Other:
    case core::PartOfSpeech::Unknown:
    case core::PartOfSpeech::Count_:
      return 12;
  }
  return 12;
}

BigramOverrideCostTable makeBigramOverrideTable(const ScorerOptions& options) {
  BigramOverrideCostTable table{};
  for (auto& row : table) {
    row.fill(std::numeric_limits<float>::quiet_NaN());
  }
  for (const BigramOverrideSpec& spec : kBigramOverrideSpecs) {
    const size_t prev_index = posToIndex(spec.prev);
    const size_t next_index = posToIndex(spec.next);
    table[prev_index][next_index] = options.bigram.*(spec.value);
  }
  return table;
}

// Bigram cost table [prev][next]
// Scale reference: kNegligible=0.2, kMinor=0.5, kRare=1.0, kStrong=1.5
// Negative values = bonus (encourages connection)
// clang-format off
constexpr float kBigramCostTable[13][13] = {
    //        Noun  Verb  Adj   Adv   Part  Aux   Conj  Det   Pron  Pref  Suff  Sym   Other
    /* Noun */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.0F, 0.5F, 0.5F, 0.5F, 1.0F,-0.8F, 0.5F, 0.5F},
    /* Verb */ {0.2F, 0.8F, 0.8F, 0.5F, 0.0F, 0.0F, 0.5F, 0.5F, 0.2F, 1.0F, 1.5F, 0.5F, 0.5F},  // Suff: 人 in 知ってる+人 is a noun
    /* Adj  */ {0.2F, 0.5F, 0.8F, 0.3F, 0.0F, 0.0F, 0.5F, 0.5F, 0.2F, 1.0F, 0.8F, 0.5F, 0.5F},  // Aux: おいし+そう (ADJ_STEM+AUX)
    /* Adv  */ {0.0F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Part */ {0.0F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.5F, 0.3F, 0.0F, 0.3F, 1.0F, 0.5F, 0.5F},  // Pref: は+何 PREFIX (何番線)
    /* Aux  */ {0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 0.3F, 0.5F, 0.5F, 0.5F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Conj */ {0.0F, 0.2F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.2F, 0.0F, 0.3F, 1.0F, 0.3F, 0.3F},
    /* Det  */ {0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.8F, 0.0F, 1.0F, 1.5F, 0.5F, 0.5F},  // Suff: 人 in あんな+人 is a noun
    /* Pron */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.2F, 0.5F, 0.5F, 0.5F, 1.0F,-0.8F, 0.5F, 0.5F},  // Aux: 私+だ; Suff: 彼女+ら
    /* Pref */{-0.5F,-0.5F, 0.0F, 0.5F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    /* Suff */ {0.5F, 0.8F, 0.8F, 0.5F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 1.0F, 0.3F, 0.5F, 0.5F},
    /* Sym  */ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.5F, 0.0F, 0.2F},
    // Sym: raised to the column's common value. An unanalyzed chunk is not a
    // better clause-ender than an analyzed word, and the discount let a whole
    // hiragana clause collapse into one Other edge whenever a symbol closed it
    // (そうだね vs そうだね〜) — the segmentation flipped on the punctuation alone.
    /* Other*/ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.5F, 0.5F, 0.5F, 0.2F},
};
// clang-format on

}  // namespace

Scorer::Scorer(const ScorerOptions& options)
    : options_(options), bigram_override_costs_(makeBigramOverrideTable(options)) {}

float Scorer::posPrior(core::PartOfSpeech pos) const {
  switch (pos) {
    case core::PartOfSpeech::Noun:
      return options_.noun_prior;
    case core::PartOfSpeech::Verb:
      return options_.verb_prior;
    case core::PartOfSpeech::Adjective:
      return options_.adj_prior;
    case core::PartOfSpeech::Adverb:
      return options_.adv_prior;
    case core::PartOfSpeech::Particle:
      return options_.particle_prior;
    case core::PartOfSpeech::Auxiliary:
      return options_.aux_prior;
    case core::PartOfSpeech::Pronoun:
      return options_.pronoun_prior;
    default:
      return 0.5F;
  }
}

float Scorer::bigramCost(core::PartOfSpeech prev, core::PartOfSpeech next) const {
  const float override_value = bigram_override_costs_[posToIndex(prev)][posToIndex(next)];
  if (!std::isnan(override_value)) {
    return override_value;
  }
  // Fall back to default table
  return kBigramCostTable[posToIndex(prev)][posToIndex(next)];
}

}  // namespace suzume::analysis
