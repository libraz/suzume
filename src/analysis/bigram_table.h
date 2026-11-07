#ifndef SUZUME_ANALYSIS_BIGRAM_TABLE_H_
#define SUZUME_ANALYSIS_BIGRAM_TABLE_H_

// =============================================================================
// ExtendedPOS Bigram Connection Table
// =============================================================================
// Replaces 90+ check functions with a single kSize x kSize bigram table
// (kSize = ExtendedPOS::Count_).
//
// Design Principles:
//   1. All connection costs are determined by ExtendedPOS bigram (no exceptions)
//   2. If behavior is wrong, either the category or assignment is wrong
//   3. Table values: positive = penalty, negative = bonus, 0.0 = neutral
//
// Usage:
//   float cost = BigramTable::getCost(prev.extended_pos, next.extended_pos);
// =============================================================================

#include <array>
#include <cstdint>

#include "core/types.h"

namespace suzume::analysis {

/**
 * @brief ExtendedPOS bigram connection cost table
 *
 * This table provides connection costs between ExtendedPOS pairs.
 * The table is initialized with grammatically-motivated values.
 */
class BigramTable {
 public:
  // Table dimensions
  static constexpr size_t kSize = static_cast<size_t>(core::ExtendedPOS::Count_);

  /**
   * @brief Get connection cost for an ExtendedPOS pair
   * @param prev Previous edge's ExtendedPOS
   * @param next Next edge's ExtendedPOS
   * @return Connection cost modifier (positive = penalty, negative = bonus)
   */
  static float getCost(core::ExtendedPOS prev, core::ExtendedPOS next);

 private:
  // Costs come from a small fixed palette. Storing an 8-bit palette index
  // instead of the same float values in every cell cuts the dense table to a
  // quarter of its former size while preserving O(1) lookup.
  using EncodedTable = std::array<std::array<uint8_t, kSize>, kSize>;
  static const EncodedTable table_;

  // Initialize table with grammatical connection costs
  static EncodedTable initTable();
};

// =============================================================================
// Cost Constants for Table Initialization
// =============================================================================
// These constants define the connection cost scale.
// Use consistent values across all table entries.

namespace bigram_cost {

// Bonuses (negative values - encourage connection)
// Design principle: Bonuses should help valid grammatical patterns win over
// single-token alternatives, but not be so strong that they overwhelm
// dictionary entries or create false positives.
constexpr float kVeryStrongBonus = -1.6F;   // Very strong grammatical connection
constexpr float kExtraStrongBonus = -1.0F;  // Extra strong (scale compat)
constexpr float kStrongBonus = -0.8F;       // Strong grammatical connection
constexpr float kModerateBonus = -0.5F;     // Normal grammatical connection
constexpr float kMinorBonus = -0.25F;       // Slight preference

// Neutral
constexpr float kNeutral = 0.0F;  // No preference

// Penalties (positive values - discourage connection)
// Named by likelihood: use when connection has this probability level
constexpr float kNegligible = 0.2F;     // Negligible impact
constexpr float kUncommon = 0.4F;       // Uncommon but possible
constexpr float kMinor = 0.5F;          // Minor penalty
constexpr float kRare = 1.0F;           // Rare
constexpr float kStrong = 1.5F;         // Strong grammatical violation
constexpr float kVeryRare = 1.8F;       // Very rare
constexpr float kSevere = 2.5F;         // Severe violation
constexpr float kAlmostNever = 3.0F;    // Almost never happens
constexpr float kNever = 3.5F;          // Near prohibition
constexpr float kExtremeBonus = -2.0F;  // Extreme bonus for grammatically necessary connections
constexpr float kAppearanceAuxiliaryBonus = kExtremeBonus + kMinorBonus;
constexpr float kCompletiveVolitionalBonus = kExtremeBonus + kMinorBonus;
constexpr float kDoubleVeryStrongBonus = kVeryStrongBonus * 2;
constexpr float kTripleVeryStrongBonus = kVeryStrongBonus * 3;
constexpr float kDeterminerNounBonus = -kSevere;
constexpr float kProhibitive = 5.0F;  // Absolute prohibition — exceeds kNever

}  // namespace bigram_cost

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_BIGRAM_TABLE_H_
