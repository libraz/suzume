#ifndef SUZUME_CORE_VITERBI_H_
#define SUZUME_CORE_VITERBI_H_

#include <algorithm>
#include <iostream>
#include <limits>
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
    // Use the existing implementation but return edge IDs
    ViterbiResult result;
    result.total_cost = 0.0F;

    // Simple implementation using existing logic
    const size_t num_pos = lattice.textLength() + 1;
    std::vector<float> costs(num_pos, std::numeric_limits<float>::max());
    std::vector<int> prev_edge(num_pos, -1);
    std::vector<size_t> prev_pos(num_pos, 0);

    costs[0] = 0.0F;

    for (size_t pos = 0; pos < lattice.textLength(); ++pos) {
      if (costs[pos] == std::numeric_limits<float>::max()) {
        continue;
      }

      const auto& edges = lattice.edgesAt(pos);
      for (size_t idx = 0; idx < edges.size(); ++idx) {
        const auto& edge = edges[idx];
        float word_cost = scorer.wordCost(edge);

        // Connection cost from previous edge
        float conn_cost = 0.0F;
        if (prev_edge[pos] >= 0) {
          const auto& prev = lattice.edgesAt(prev_pos[pos])[static_cast<size_t>(prev_edge[pos])];
          conn_cost = scorer.connectionCost(prev, edge);
        }

        float total = costs[pos] + word_cost + conn_cost;

        SUZUME_DEBUG_VITERBI("[VITERBI] pos=" << pos << " \"" << edge.surface
                    << "\" word=" << word_cost << " conn=" << conn_cost
                    << " total=" << total << " best[" << edge.end << "]="
                    << (costs[edge.end] < 1e10 ? costs[edge.end] : -999)
                    << (total < costs[edge.end] ? " UPDATE" : "") << "\n");

        if (total < costs[edge.end]) {
          costs[edge.end] = total;
          prev_edge[edge.end] = static_cast<int>(idx);
          prev_pos[edge.end] = pos;
        }
      }
    }

    // Backtrack
    if (costs[lattice.textLength()] < std::numeric_limits<float>::max()) {
      result.total_cost = costs[lattice.textLength()];
      size_t pos = lattice.textLength();
      while (pos > 0 && prev_edge[pos] >= 0) {
        const auto& edges = lattice.edgesAt(prev_pos[pos]);
        if (prev_edge[pos] < static_cast<int>(edges.size())) {
          result.path.push_back(edges[static_cast<size_t>(prev_edge[pos])].id);
        }
        pos = prev_pos[pos];
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
