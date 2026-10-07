#ifndef SUZUME_ANALYSIS_BIGRAM_TABLE_INTERNAL_H_
#define SUZUME_ANALYSIS_BIGRAM_TABLE_INTERNAL_H_

#include <limits>

#include "bigram_table.h"

namespace suzume::analysis::bigram_rules {

using BigramMatrix = std::array<std::array<uint8_t, BigramTable::kSize>, BigramTable::kSize>;

inline constexpr uint8_t kUnsetCost = std::numeric_limits<uint8_t>::max();

// Every value used by the grammatical rule tables. Keep this list compact:
// the dense matrix stores only an index into it.
inline constexpr std::array<float, 21> kCostPalette = {
    -2.5F,
    bigram_cost::kCompletiveVolitionalBonus,
    bigram_cost::kDoubleVeryStrongBonus,
    bigram_cost::kTripleVeryStrongBonus,
    bigram_cost::kExtremeBonus,
    bigram_cost::kVeryStrongBonus,
    bigram_cost::kExtraStrongBonus,
    bigram_cost::kStrongBonus,
    bigram_cost::kModerateBonus,
    bigram_cost::kMinorBonus,
    bigram_cost::kNeutral,
    bigram_cost::kNegligible,
    bigram_cost::kUncommon,
    bigram_cost::kMinor,
    bigram_cost::kRare,
    bigram_cost::kStrong,
    bigram_cost::kVeryRare,
    bigram_cost::kSevere,
    bigram_cost::kAlmostNever,
    bigram_cost::kNever,
    bigram_cost::kProhibitive,
};
static_assert(kCostPalette.size() < kUnsetCost);

constexpr uint8_t encodeCost(float value) {
  for (size_t index = 0; index < kCostPalette.size(); ++index) {
    if (kCostPalette[index] == value) {
      return static_cast<uint8_t>(index);
    }
  }
  return kUnsetCost;
}

inline float decodeCost(uint8_t index) {
  return index < kCostPalette.size() ? kCostPalette[index] : bigram_cost::kNeutral;
}

// Three encoded bytes replace hundreds of individually emitted table stores.
// Rules are expanded into the same dense matrix once at startup, preserving
// the hot getCost() lookup path while saving ~1.7 KB raw / ~0.3 KB gzip.
struct BigramRule {
  constexpr BigramRule(core::ExtendedPOS prev_value, core::ExtendedPOS next_value, float cost_value)
      : prev(static_cast<uint8_t>(prev_value)), next(static_cast<uint8_t>(next_value)), cost(encodeCost(cost_value)) {}

  uint8_t prev;
  uint8_t next;
  uint8_t cost;
};
static_assert(sizeof(BigramRule) == 3);

// Returns false for an invalid rule set rather than terminating the embedding
// process during static table construction. Rule tables are verified by unit
// tests and their constexpr encoded costs are constrained by kCostPalette.
bool applyRules(BigramMatrix& table, const BigramRule* rules, size_t rule_count);

template <size_t Size>
bool applyRules(BigramMatrix& table, const BigramRule (&rules)[Size]) {
  return applyRules(table, rules, Size);
}

// Overwrites one cell, whatever it held.
inline void setCost(BigramMatrix& table, core::ExtendedPOS prev, core::ExtendedPOS next, float cost) {
  table[static_cast<size_t>(prev)][static_cast<size_t>(next)] = encodeCost(cost);
}
void inheritRuleProfile(BigramMatrix& table, core::ExtendedPOS source, core::ExtendedPOS target);

void setVerbAndAdjectiveCosts(BigramMatrix& table);
void setAuxiliaryAndNounCosts(BigramMatrix& table);
void setNominalParticleCosts(BigramMatrix& table);
void setParticleAndLexicalCosts(BigramMatrix& table);
void setParticleAndLexicalPenaltyCosts(BigramMatrix& table);

}  // namespace suzume::analysis::bigram_rules

#endif  // SUZUME_ANALYSIS_BIGRAM_TABLE_INTERNAL_H_
