#include "analysis/scorer.h"

#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "core/debug.h"

namespace {

// Convert POS to array index
constexpr size_t posToIndex(suzume::core::PartOfSpeech pos) {
  switch (pos) {
    case suzume::core::PartOfSpeech::Noun:
      return 0;
    case suzume::core::PartOfSpeech::Verb:
      return 1;
    case suzume::core::PartOfSpeech::Adjective:
      return 2;
    case suzume::core::PartOfSpeech::Adverb:
      return 3;
    case suzume::core::PartOfSpeech::Particle:
      return 4;
    case suzume::core::PartOfSpeech::Auxiliary:
      return 5;
    case suzume::core::PartOfSpeech::Conjunction:
      return 6;
    case suzume::core::PartOfSpeech::Determiner:
      return 7;
    case suzume::core::PartOfSpeech::Pronoun:
      return 8;
    case suzume::core::PartOfSpeech::Prefix:
      return 9;
    case suzume::core::PartOfSpeech::Suffix:
      return 10;
    case suzume::core::PartOfSpeech::Symbol:
      return 11;
    case suzume::core::PartOfSpeech::Other:
    case suzume::core::PartOfSpeech::Unknown:
      return 12;
  }
  return 12;
}

// Bigram cost table [prev][next]
// Scale reference: kTrivial=0.2, kMinor=0.5, kModerate=1.0, kStrong=1.5
// Negative values = bonus (encourages connection)
// clang-format off
constexpr float kBigramCostTable[13][13] = {
    //        Noun  Verb  Adj   Adv   Part  Aux   Conj  Det   Pron  Pref  Suff  Sym   Other
    /* Noun */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.0F, 0.5F, 0.5F, 0.5F, 1.0F,-0.8F, 0.5F, 0.5F},
    /* Verb */ {0.2F, 0.8F, 0.8F, 0.5F, 0.0F, 0.0F, 0.5F, 0.5F, 0.2F, 1.0F, 1.5F, 0.5F, 0.5F},  // Suff: 0.8→1.5 (知ってる人: NOUN優先)
    /* Adj  */ {0.2F, 0.5F, 0.8F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.2F, 1.0F, 0.8F, 0.5F, 0.5F},  // Keep 0.5 (P3-2 causes side effects)
    /* Adv  */ {0.0F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Part */ {0.0F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.5F, 0.3F, 0.0F, 0.3F, 1.0F, 0.5F, 0.5F},  // Pref: 1.0→0.3 (何番線: は→何PREFIX)
    /* Aux  */ {0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 0.3F, 0.5F, 0.5F, 0.5F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Conj */ {0.0F, 0.2F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.2F, 0.0F, 0.3F, 1.0F, 0.3F, 0.3F},
    /* Det  */ {0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.8F, 0.0F, 1.0F, 1.5F, 0.5F, 0.5F},  // Suff: 0.8→1.5 (あんな人: NOUN優先)
    /* Pron */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.2F, 0.5F, 0.5F, 0.5F, 1.0F, 0.0F, 0.5F, 0.5F},  // P3-1: Aux 1.0→0.2 (私だ is basic)
    /* Pref */{-0.5F,-0.5F, 0.0F, 0.5F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    /* Suff */ {0.5F, 0.8F, 0.8F, 0.5F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 1.0F, 0.3F, 0.5F, 0.5F},
    /* Sym  */ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.5F, 0.0F, 0.2F},
    /* Other*/ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.5F, 0.5F, 0.2F, 0.2F},
};
// clang-format on

}  // namespace

namespace suzume::analysis {

// static
void Scorer::logAdjustment(float amount, const char* reason) {
  if (amount != 0.0F) {
    SUZUME_DEBUG_LOG_VERBOSE("  " << reason << ": "
                            << (amount > 0 ? "+" : "") << amount << "\n");
  }
}

Scorer::Scorer(const ScorerOptions& options) : options_(options) {}

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
  // Check for BigramOverrides first (NaN = use default)
  const auto& bg = options_.bigram;

  // Helper lambda to check if override is set
  auto isSet = [](float v) { return !std::isnan(v); };

  // Check specific pair overrides
  using POS = core::PartOfSpeech;
  if (prev == POS::Noun && next == POS::Suffix && isSet(bg.noun_to_suffix))
    return bg.noun_to_suffix;
  if (prev == POS::Prefix && next == POS::Noun && isSet(bg.prefix_to_noun))
    return bg.prefix_to_noun;
  if (prev == POS::Prefix && next == POS::Verb && isSet(bg.prefix_to_verb))
    return bg.prefix_to_verb;
  if (prev == POS::Pronoun && next == POS::Auxiliary && isSet(bg.pron_to_aux))
    return bg.pron_to_aux;
  if (prev == POS::Verb && next == POS::Verb && isSet(bg.verb_to_verb))
    return bg.verb_to_verb;
  if (prev == POS::Verb && next == POS::Noun && isSet(bg.verb_to_noun))
    return bg.verb_to_noun;
  if (prev == POS::Verb && next == POS::Auxiliary && isSet(bg.verb_to_aux))
    return bg.verb_to_aux;
  if (prev == POS::Adjective && next == POS::Auxiliary && isSet(bg.adj_to_aux))
    return bg.adj_to_aux;
  if (prev == POS::Adjective && next == POS::Verb && isSet(bg.adj_to_verb))
    return bg.adj_to_verb;
  if (prev == POS::Adjective && next == POS::Adjective && isSet(bg.adj_to_adj))
    return bg.adj_to_adj;
  if (prev == POS::Particle && next == POS::Verb && isSet(bg.part_to_verb))
    return bg.part_to_verb;
  if (prev == POS::Particle && next == POS::Noun && isSet(bg.part_to_noun))
    return bg.part_to_noun;
  if (prev == POS::Auxiliary && next == POS::Particle && isSet(bg.aux_to_part))
    return bg.aux_to_part;
  if (prev == POS::Auxiliary && next == POS::Auxiliary && isSet(bg.aux_to_aux))
    return bg.aux_to_aux;

  // Fall back to default table
  return kBigramCostTable[posToIndex(prev)][posToIndex(next)];
}

float Scorer::wordCost(const core::LatticeEdge& edge) const {
  // v0.8: Base cost from ExtendedPOS category
  float category_cost = getCategoryCost(edge.extended_pos);

  // Use edge.cost if explicitly set (non-zero), otherwise use category cost
  // This allows candidates (e.g., adjective stem + すぎる) to have custom costs
  float cost = (edge.cost != 0.0F) ? edge.cost : category_cost;

  // User dictionary bonus (still needed for user customization)
  if (edge.fromUserDict()) {
    cost += options_.user_dict_bonus;
  }

  // Dictionary adverb bonus
  // Adverbs like どうして, いつも are often oversplit due to aggressive verb analysis
  // Give dictionary adverbs a bonus to compete with split paths
  // Longer adverbs (3+ chars) get stronger bonus to beat VerbRenyokei→ParticleConj
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adverb) {
    float adv_bonus = -0.5F;  // Base bonus for dictionary adverbs
    // Extra bonus for 3+ character adverbs (せめて, どうして, etc.)
    // These need to beat verb renyokei + て split paths
    // Japanese chars are 3 bytes each, so 3 chars = 9 bytes
    if (edge.surface.size() >= 9) {
      adv_bonus = -1.5F;  // Strong bonus for longer adverbs to beat split paths
    }
    cost += adv_bonus;
  }

  // Debug output - show which cost was used (verbose level)
  SUZUME_DEBUG_VERBOSE_BLOCK {
    const char* source = edge.fromDictionary() ? "dict" :
                         edge.isUnknown() ? "unk" : "infl";
    const char* cost_from = (edge.cost != 0.0F) ? "edge" : "category";
    SUZUME_DEBUG_STREAM << "[WORD] \"" << edge.surface << "\" (" << source << ") cost="
                        << cost << " (from " << cost_from << ")";
    SUZUME_DEBUG_STREAM << " [cat=" << category_cost;
    if (edge.cost != 0.0F) {
      SUZUME_DEBUG_STREAM << " edge=" << edge.cost;
    }
    SUZUME_DEBUG_STREAM << "]\n";
  }
  return cost;
}

float Scorer::connectionCost(const core::LatticeEdge& prev,
                             const core::LatticeEdge& next) const {
  float base_cost = bigramCost(prev.pos, next.pos);

  // ExtendedPOS bigram cost (replaces all check functions)
  float extended_cost = BigramTable::getCost(prev.extended_pos, next.extended_pos);

  float total = base_cost + extended_cost;

  // Surface-specific bonus: NOUN → する/し/さ (VERB) for suru-verb pattern
  // MeCab compatibility: suru-verbs should split as Noun + する/し/さ(Verb)
  // The POS-based NOUN→VERB cost is 0.5, while NOUN→AUX/PARTICLE is 0.0
  // Apply strong bonus to compensate and prefer the Verb interpretation
  // Suru-verb forms: する (shuushikei), し (renyokei), さ (mizenkei)
  if (prev.pos == core::PartOfSpeech::Noun &&
      next.pos == core::PartOfSpeech::Verb &&
      (next.surface == "する" || next.surface == "し" || next.surface == "さ")) {
    total += bigram_cost::kStrongBonus;  // Negative value = bonus
  }

  // Surface-specific bonus: しれ (VERB) → ない (AUX) for かもしれない pattern
  // MeCab compatibility: かもしれない → かも|しれ|ない
  // The path かも|し|れ|ない incorrectly wins due to VerbRenyokei→AuxPassive bonus
  // Give strong bonus to しれ→ない to ensure correct parse
  if (prev.surface == "しれ" && prev.pos == core::PartOfSpeech::Verb &&
      next.surface.compare(0, 6, "ない") == 0) {
    total += bigram_cost::kStrongBonus;  // Negative value = bonus
  }

  SUZUME_DEBUG_LOG_VERBOSE("[CONN] \"" << prev.surface << "\" ("
                    << core::posToString(prev.pos) << ") → \""
                    << next.surface << "\" ("
                    << core::posToString(next.pos) << "): "
                    << "base=" << base_cost << " epos=" << extended_cost
                    << " = " << total << "\n");

  return total;
}

}  // namespace suzume::analysis
