#ifndef SUZUME_CORE_VITERBI_H_
#define SUZUME_CORE_VITERBI_H_

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

#include "debug.h"
#include "lattice.h"
#include "morpheme.h"

// Forward declaration for scorer interface
namespace suzume::analysis {
class IScorer;
}  // namespace suzume::analysis

namespace suzume::core {

// Number of PartOfSpeech types (Unknown=0 to Other=13)
inline constexpr size_t kNumPosTypes = 14;

/**
 * @brief Viterbi result with path and cost
 */
struct ViterbiResult {
  std::vector<size_t> path;   // Edge IDs in order
  float total_cost{0.0F};     // Total path cost
};

/**
 * @brief Viterbi algorithm for finding optimal path
 */
class Viterbi {
 public:
  Viterbi() = default;
  ~Viterbi() = default;

  // Non-copyable, movable
  Viterbi(const Viterbi&) = delete;
  Viterbi& operator=(const Viterbi&) = delete;
  Viterbi(Viterbi&&) = default;
  Viterbi& operator=(Viterbi&&) = default;

  /**
   * @brief Solve with custom scorer (returns edge IDs)
   * @param lattice Lattice graph
   * @param scorer Custom scorer
   * @return ViterbiResult with path and cost
   */
  template <typename Scorer>
  ViterbiResult solve(const Lattice& lattice, const Scorer& scorer) const {
    ViterbiResult result;
    result.total_cost = 0.0F;

    const size_t text_len = lattice.textLength();
    if (text_len == 0) {
      return result;
    }

    // State info for (position, POS) pair tracking
    // Using 2D array: states_by_pos[position][pos_tag_index]
    // This eliminates hash overhead and O(n) position scanning
    struct StateInfo {
      float cost{std::numeric_limits<float>::max()};
      int prev_edge{-1};
      size_t prev_pos{0};
      PartOfSpeech prev_pos_tag{PartOfSpeech::Unknown};
      bool valid{false};  // Track if this state has been set
    };

    // Pre-allocate for all positions + 1 (for final position)
    std::vector<std::array<StateInfo, kNumPosTypes>> states_by_pos(text_len + 1);

    // Initialize BOS state at position 0, POS=Unknown
    auto& bos_state = states_by_pos[0][static_cast<size_t>(PartOfSpeech::Unknown)];
    bos_state.cost = 0.0F;
    bos_state.prev_edge = -1;
    bos_state.valid = true;

    // Forward pass - process positions in order
    for (size_t pos = 0; pos < text_len; ++pos) {
      const auto& states_at_pos = states_by_pos[pos];

      // Check if any valid state exists at this position
      bool has_valid_state = false;
      for (size_t i = 0; i < kNumPosTypes; ++i) {
        if (states_at_pos[i].valid) {
          has_valid_state = true;
          break;
        }
      }
      if (!has_valid_state) {
        continue;
      }

      const auto& edges = lattice.edgesAt(pos);
      for (size_t idx = 0; idx < edges.size(); ++idx) {
        const auto& edge = edges[idx];
        float word_cost = scorer.wordCost(edge);

        // Try all valid states at this position
        for (size_t pos_idx = 0; pos_idx < kNumPosTypes; ++pos_idx) {
          const auto& state_info = states_at_pos[pos_idx];
          if (!state_info.valid) {
            continue;
          }

          float conn_cost = 0.0F;
          if (state_info.prev_edge >= 0) {
            const auto& prev_edges = lattice.edgesAt(state_info.prev_pos);
            const auto& prev = prev_edges[static_cast<size_t>(state_info.prev_edge)];
            conn_cost = scorer.connectionCost(prev, edge);
          } else {
            // BOS (beginning of sentence) connection cost
            // Suffix should not appear at sentence start
            if (edge.pos == PartOfSpeech::Suffix) {
              conn_cost = 3.0F;  // High penalty for suffix at BOS
            }
            // Conjunction at sentence start is natural (e.g., でも, しかし)
            if (edge.pos == PartOfSpeech::Conjunction) {
              conn_cost = -0.5F;  // Bonus for conjunction at BOS
            }
          }

          // Small per-transition cost to prefer fewer morphemes (longer tokens)
          // This breaks ties when paths have equal cost
          constexpr float kTransitionCost = 0.001F;
          float total = state_info.cost + word_cost + conn_cost + kTransitionCost;
          size_t next_pos_idx = static_cast<size_t>(edge.pos);

          SUZUME_DEBUG_LOG("[VITERBI] pos=" << pos << " \"" << edge.surface
                      << "\" (from " << posToString(static_cast<PartOfSpeech>(pos_idx)) << ")"
                      << " word=" << word_cost << " conn=" << conn_cost
                      << " total=" << total << "\n");

          auto& next_state = states_by_pos[edge.end][next_pos_idx];
          if (!next_state.valid || total < next_state.cost) {
            next_state.cost = total;
            next_state.prev_edge = static_cast<int>(idx);
            next_state.prev_pos = pos;
            next_state.prev_pos_tag = static_cast<PartOfSpeech>(pos_idx);
            next_state.valid = true;
          }
        }
      }
    }

    // Find best state at final position
    size_t best_final_pos_idx = 0;
    float best_cost = std::numeric_limits<float>::max();

    const auto& final_states = states_by_pos[text_len];
    for (size_t i = 0; i < kNumPosTypes; ++i) {
      if (final_states[i].valid && final_states[i].cost < best_cost) {
        best_cost = final_states[i].cost;
        best_final_pos_idx = i;
      }
    }

    // Backtrack
    if (best_cost < std::numeric_limits<float>::max()) {
      result.total_cost = best_cost;
      size_t current_pos = text_len;
      size_t current_pos_idx = best_final_pos_idx;

      while (current_pos > 0) {
        const auto& state = states_by_pos[current_pos][current_pos_idx];
        if (!state.valid || state.prev_edge < 0) {
          break;
        }

        const auto& prev_edges = lattice.edgesAt(state.prev_pos);
        result.path.push_back(prev_edges[static_cast<size_t>(state.prev_edge)].id);
        current_pos = state.prev_pos;
        current_pos_idx = static_cast<size_t>(state.prev_pos_tag);
      }
      std::reverse(result.path.begin(), result.path.end());
    }

    // Debug: print final path
    if (Debug::isEnabled() && !result.path.empty()) {
      Debug::log() << "[VITERBI] Best path (cost=" << result.total_cost << "): ";
      for (size_t i = 0; i < result.path.size(); ++i) {
        const auto& edge = lattice.getEdge(result.path[i]);
        if (i > 0) Debug::log() << " → ";
        Debug::log() << "\"" << edge.surface << "\"(" << posToString(edge.pos) << ")";
      }
      Debug::log() << "\n";
    }

    return result;
  }
};

}  // namespace suzume::core

#endif  // SUZUME_CORE_VITERBI_H_
