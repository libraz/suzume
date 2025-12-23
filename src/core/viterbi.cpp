#include "viterbi.h"

#include <algorithm>
#include <array>
#include <limits>

namespace suzume::core {

namespace {

// Number of POS types
constexpr size_t kNumPos = 9;

// POS bigram connection cost table
// Lower cost = more likely connection
// clang-format off
constexpr std::array<std::array<float, kNumPos>, kNumPos> kBigramCostTable = {{
  //          NOUN   VERB   ADJ    ADV    PART   AUX    CONJ   SYM    OTHER
  /* NOUN */ {{1.0F,  0.5F,  1.5F,  1.0F,  0.3F,  0.8F,  1.5F,  1.0F,  2.0F}},
  /* VERB */ {{0.8F,  1.5F,  2.0F,  1.0F,  0.3F,  0.2F,  2.0F,  1.0F,  2.0F}},
  /* ADJ  */ {{0.8F,  1.0F,  2.0F,  0.8F,  0.3F,  0.5F,  2.0F,  1.0F,  2.0F}},
  /* ADV  */ {{0.5F,  0.3F,  0.5F,  1.5F,  1.0F,  1.0F,  2.0F,  1.0F,  2.0F}},
  /* PART */ {{0.3F,  0.3F,  0.5F,  0.5F,  1.5F,  0.8F,  2.0F,  1.0F,  2.0F}},
  /* AUX  */ {{0.5F,  1.0F,  1.5F,  0.8F,  0.3F,  0.8F,  2.0F,  1.0F,  2.0F}},
  /* CONJ */ {{0.5F,  0.5F,  0.5F,  0.5F,  1.5F,  1.5F,  2.5F,  1.0F,  2.0F}},
  /* SYM  */ {{1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  0.5F,  1.5F}},
  /* OTHR */ {{1.5F,  1.5F,  1.5F,  1.5F,  1.5F,  1.5F,  1.5F,  1.5F,  1.5F}}
}};
// clang-format on

}  // namespace

float Viterbi::connectionCost(PartOfSpeech left, PartOfSpeech right) {
  return kBigramCostTable.at(static_cast<size_t>(left))
      .at(static_cast<size_t>(right));
}

std::vector<Morpheme> Viterbi::findBestPath(const Lattice& lattice) {
  const size_t num_positions = lattice.textLength() + 1;
  constexpr float kInf = std::numeric_limits<float>::max();

  // Initialize DP table
  // nodes_[pos] = vector of nodes at each position
  nodes_.clear();
  nodes_.resize(num_positions);

  // BOS node at position 0
  nodes_[0].push_back({0.0F, -1, -1});

  // Forward pass
  for (size_t pos = 0; pos < num_positions; ++pos) {
    if (nodes_[pos].empty()) {
      continue;
    }

    const auto& edges = lattice.edgesAt(pos);
    for (size_t edge_idx = 0; edge_idx < edges.size(); ++edge_idx) {
      const auto& edge = edges[edge_idx];
      size_t end_pos = edge.end;
      if (end_pos > num_positions) {
        continue;
      }

      // Find best path to this edge
      float best_cost = kInf;

      for (auto prev_node : nodes_[pos]) {
        float conn_cost = 0.0F;

        // Connection cost (skip for BOS)
        if (prev_node.prev_edge >= 0) {
          const auto& prev_edge = lattice.edgesAt(prev_node.prev_node >= 0 ? static_cast<size_t>(prev_node.prev_node) : 0)[static_cast<size_t>(prev_node.prev_edge)];
          conn_cost = connectionCost(prev_edge.pos, edge.pos);
        }

        float total = prev_node.cost + conn_cost + edge.cost;
        if (total < best_cost) {
          best_cost = total;
        }
      }

      if (best_cost < kInf) {
        nodes_[end_pos].push_back({best_cost, static_cast<int>(edge_idx), static_cast<int>(pos)});
      }
    }
  }

  // Find best path to EOS
  std::vector<Morpheme> result;
  const size_t last_pos = num_positions - 1;
  if (nodes_[last_pos].empty()) {
    return result;
  }

  int best_node = 0;
  float best_cost = kInf;
  for (size_t idx = 0; idx < nodes_[last_pos].size(); ++idx) {
    if (nodes_[last_pos][idx].cost < best_cost) {
      best_cost = nodes_[last_pos][idx].cost;
      best_node = static_cast<int>(idx);
    }
  }

  // Backtrack
  size_t pos = last_pos;
  while (pos > 0 && best_node >= 0) {
    const auto& node = nodes_[pos][static_cast<size_t>(best_node)];
    if (node.prev_edge >= 0 && node.prev_node >= 0) {
      const auto& edge = lattice.edgesAt(static_cast<size_t>(node.prev_node))[static_cast<size_t>(node.prev_edge)];
      Morpheme morpheme;
      morpheme.surface = std::string(edge.surface);
      morpheme.start = edge.start;
      morpheme.end = edge.end;
      morpheme.pos = edge.pos;
      morpheme.lemma = std::string(edge.lemma);
      morpheme.features.is_dictionary = hasFlag(edge.flags, EdgeFlags::FromDictionary);
      morpheme.features.is_user_dict = hasFlag(edge.flags, EdgeFlags::FromUserDict);
      morpheme.features.is_formal_noun = hasFlag(edge.flags, EdgeFlags::IsFormalNoun);
      morpheme.features.is_low_info = hasFlag(edge.flags, EdgeFlags::IsLowInfo);
      morpheme.features.score = edge.cost;
      result.push_back(morpheme);
      pos = static_cast<size_t>(node.prev_node);
    }

    // Find previous node
    best_node = -1;
    for (size_t idx = 0; idx < nodes_[pos].size(); ++idx) {
      if (nodes_[pos][idx].prev_node < 0 || static_cast<size_t>(nodes_[pos][idx].prev_node) < pos) {
        best_node = static_cast<int>(idx);
        break;
      }
    }
  }

  std::reverse(result.begin(), result.end());
  return result;
}

}  // namespace suzume::core
