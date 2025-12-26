#ifndef SUZUME_CORE_VITERBI_H_
#define SUZUME_CORE_VITERBI_H_

#include <algorithm>
#include <iostream>
#include <limits>
#include <unordered_map>
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

/**
 * @brief State key for (position, POS) pair tracking in Viterbi
 */
struct ViterbiStateKey {
  size_t pos{0};
  PartOfSpeech pos_tag{PartOfSpeech::Unknown};

  bool operator==(const ViterbiStateKey& other) const {
    return pos == other.pos && pos_tag == other.pos_tag;
  }
};

/**
 * @brief Hash function for ViterbiStateKey
 */
struct ViterbiStateKeyHash {
  size_t operator()(const ViterbiStateKey& key) const {
    return std::hash<size_t>()(key.pos) ^
           (std::hash<uint8_t>()(static_cast<uint8_t>(key.pos_tag)) << 4);
  }
};

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
   * @brief Find optimal path through the lattice
   * @param lattice Lattice graph
   * @return Vector of morphemes on the optimal path
   */
  std::vector<Morpheme> findBestPath(const Lattice& lattice);

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

    // State info for (position, POS) pair tracking
    struct StateInfo {
      float cost{std::numeric_limits<float>::max()};
      int prev_edge{-1};
      ViterbiStateKey prev_state{0, PartOfSpeech::Unknown};
    };

    // Track states by (position, POS) pairs instead of just position
    std::unordered_map<ViterbiStateKey, StateInfo, ViterbiStateKeyHash> states;
    states[{0, PartOfSpeech::Unknown}] = {0.0F, -1, {0, PartOfSpeech::Unknown}};

    // Forward pass - process positions in order
    for (size_t pos = 0; pos < lattice.textLength(); ++pos) {
      // Collect all states at this position (snapshot to avoid iterator invalidation)
      std::vector<std::pair<ViterbiStateKey, StateInfo>> states_at_pos;
      for (const auto& [key, info] : states) {
        if (key.pos == pos) {
          states_at_pos.push_back({key, info});
        }
      }

      if (states_at_pos.empty()) {
        continue;
      }

      const auto& edges = lattice.edgesAt(pos);
      for (size_t idx = 0; idx < edges.size(); ++idx) {
        const auto& edge = edges[idx];
        float word_cost = scorer.wordCost(edge);

        // Try all states at this position (different POS may have different paths)
        for (const auto& [state_key, state_info] : states_at_pos) {
          float conn_cost = 0.0F;
          if (state_info.prev_edge >= 0) {
            const auto& prev_edges = lattice.edgesAt(state_info.prev_state.pos);
            const auto& prev = prev_edges[static_cast<size_t>(state_info.prev_edge)];
            conn_cost = scorer.connectionCost(prev, edge);
          }

          // Small per-transition cost to prefer fewer morphemes (longer tokens)
          // This breaks ties when paths have equal cost
          constexpr float kTransitionCost = 0.001F;
          float total = state_info.cost + word_cost + conn_cost + kTransitionCost;
          ViterbiStateKey next_key{edge.end, edge.pos};

          SUZUME_DEBUG_VITERBI("[VITERBI] pos=" << pos << " \"" << edge.surface
                      << "\" (from " << posToString(state_key.pos_tag) << ")"
                      << " word=" << word_cost << " conn=" << conn_cost
                      << " total=" << total << "\n");

          auto it = states.find(next_key);
          if (it == states.end() || total < it->second.cost) {
            states[next_key] = {total, static_cast<int>(idx), state_key};
          }
        }
      }
    }

    // Find best state at final position
    ViterbiStateKey best_final{lattice.textLength(), PartOfSpeech::Unknown};
    float best_cost = std::numeric_limits<float>::max();

    for (const auto& [key, info] : states) {
      if (key.pos == lattice.textLength() && info.cost < best_cost) {
        best_cost = info.cost;
        best_final = key;
      }
    }

    // Backtrack
    if (best_cost < std::numeric_limits<float>::max()) {
      result.total_cost = best_cost;
      ViterbiStateKey current = best_final;

      while (current.pos > 0) {
        auto it = states.find(current);
        if (it == states.end() || it->second.prev_edge < 0) {
          break;
        }

        const auto& prev_edges = lattice.edgesAt(it->second.prev_state.pos);
        result.path.push_back(prev_edges[static_cast<size_t>(it->second.prev_edge)].id);
        current = it->second.prev_state;
      }
      std::reverse(result.path.begin(), result.path.end());
    }

    // Debug: print final path
    if (Debug::isViterbiEnabled() && !result.path.empty()) {
      Debug::log() << "[VITERBI] Best path (cost=" << result.total_cost << "): ";
      for (size_t i = 0; i < result.path.size(); ++i) {
        const auto& edge = lattice.getEdge(result.path[i]);
        if (i > 0) Debug::log() << " → ";
        Debug::log() << "\"" << edge.surface << "\"";
      }
      Debug::log() << "\n";
    }

    return result;
  }

  /**
   * @brief Get connection cost between two POS
   * @param left Left POS
   * @param right Right POS
   * @return Connection cost
   */
  static float connectionCost(PartOfSpeech left, PartOfSpeech right);

 private:
  struct Node {
    float cost{0.0F};
    int prev_edge{-1};
    int prev_node{-1};
  };

  std::vector<std::vector<Node>> nodes_;
};

}  // namespace suzume::core

#endif  // SUZUME_CORE_VITERBI_H_
