#ifndef SUZUME_ANALYSIS_BIGRAM_TABLE_H_
#define SUZUME_ANALYSIS_BIGRAM_TABLE_H_

// =============================================================================
// ExtendedPOS Bigram Connection Table
// =============================================================================
// Replaces 90+ check functions with a single 55x55 bigram table.
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

  /**
   * @brief Get the raw bigram table for debugging/inspection
   */
  static const std::array<std::array<float, kSize>, kSize>& getTable();

  /**
   * @brief Get cost by indices (for testing)
   */
  static float getCostByIndex(size_t prev_idx, size_t next_idx);

 private:
  // The actual 55x55 bigram table
  static const std::array<std::array<float, kSize>, kSize> table_;

  // Initialize table with grammatical connection costs
  static std::array<std::array<float, kSize>, kSize> initTable();
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
constexpr float kStrongBonus = -0.8F;    // Strong grammatical connection
constexpr float kModerateBonus = -0.5F;  // Normal grammatical connection
constexpr float kMinorBonus = -0.25F;    // Slight preference

// Neutral
constexpr float kNeutral = 0.0F;         // No preference

// Penalties (positive values - discourage connection)
constexpr float kMinorPenalty = 0.4F;    // Slight discouragement
constexpr float kModeratePenalty = 1.0F; // Moderate discouragement
constexpr float kStrongPenalty = 1.8F;   // Strong discouragement
constexpr float kProhibitive = 3.0F;     // Effectively blocked

}  // namespace bigram_cost

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_BIGRAM_TABLE_H_
