#include "lattice.h"

#include <queue>

namespace suzume::core {

const std::vector<LatticeEdge> Lattice::empty_edges_;

Lattice::Lattice(size_t text_length) : text_length_(text_length), edges_by_start_(text_length + 1) {}

void Lattice::addEdge(const LatticeEdge& edge) {
  if (edge.start <= text_length_) {
    LatticeEdge new_edge = edge;
    new_edge.id = static_cast<uint32_t>(all_edges_.size());
    all_edges_.push_back(new_edge);
    edges_by_start_[edge.start].push_back(new_edge);
    ++edge_count_;
  }
}

size_t Lattice::addEdge(std::string_view surface, uint32_t start, uint32_t end,
                        PartOfSpeech pos, float cost, uint8_t flags,
                        std::string_view lemma) {
  if (start > text_length_) {
    return static_cast<size_t>(-1);
  }

  // Store surface string
  surface_storage_.emplace_back(surface);
  std::string_view stored_surface = surface_storage_.back();

  // Store lemma if provided
  std::string_view stored_lemma;
  if (!lemma.empty()) {
    lemma_storage_.emplace_back(lemma);
    stored_lemma = lemma_storage_.back();
  }

  LatticeEdge edge;
  edge.id = static_cast<uint32_t>(all_edges_.size());
  edge.start = start;
  edge.end = end;
  edge.surface = stored_surface;
  edge.pos = pos;
  edge.cost = cost;
  edge.flags = static_cast<EdgeFlags>(flags);
  edge.lemma = stored_lemma;

  all_edges_.push_back(edge);
  edges_by_start_[start].push_back(edge);
  ++edge_count_;

  return edge.id;
}

const std::vector<LatticeEdge>& Lattice::edgesAt(size_t pos) const {
  if (pos < edges_by_start_.size()) {
    return edges_by_start_[pos];
  }
  return empty_edges_;
}

const LatticeEdge& Lattice::getEdge(size_t edge_id) const {
  static const LatticeEdge empty_edge{};
  if (edge_id < all_edges_.size()) {
    return all_edges_[edge_id];
  }
  return empty_edge;
}

bool Lattice::isValid() const {
  if (text_length_ == 0) {
    return true;
  }

  // BFS to check if we can reach end from start
  std::vector<bool> reachable(text_length_ + 1, false);
  std::queue<size_t> que;
  que.push(0);
  reachable[0] = true;

  while (!que.empty()) {
    size_t pos = que.front();
    que.pop();

    const auto& edges = edgesAt(pos);
    for (const auto& edge : edges) {
      if (edge.end <= text_length_ && !reachable[edge.end]) {
        reachable[edge.end] = true;
        que.push(edge.end);
      }
    }
  }

  return reachable[text_length_];
}

void Lattice::clear() {
  for (auto& edges : edges_by_start_) {
    edges.clear();
  }
  all_edges_.clear();
  surface_storage_.clear();
  lemma_storage_.clear();
  edge_count_ = 0;
}

}  // namespace suzume::core
