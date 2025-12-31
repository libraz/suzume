#include "lattice.h"

#include <queue>

namespace suzume::core {

Lattice::Lattice(size_t text_length) : text_length_(text_length), edge_indices_by_start_(text_length + 1) {}

void Lattice::addEdge(const LatticeEdge& edge) {
  if (edge.start <= text_length_ && all_edges_.size() < kMaxEdges) {
    LatticeEdge new_edge = edge;
    new_edge.id = static_cast<uint32_t>(all_edges_.size());
    all_edges_.push_back(new_edge);
    edge_indices_by_start_[edge.start].push_back(new_edge.id);
    ++edge_count_;
  }
}

size_t Lattice::addEdge(std::string_view surface, uint32_t start, uint32_t end,
                        PartOfSpeech pos, float cost, uint8_t flags,
                        std::string_view lemma,
                        dictionary::ConjugationType conj_type,
                        std::string_view reading,
                        [[maybe_unused]] CandidateOrigin origin,
                        [[maybe_unused]] float origin_confidence,
                        [[maybe_unused]] std::string_view origin_detail) {
  if (start > text_length_ || all_edges_.size() >= kMaxEdges) {
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

  // Store reading if provided
  std::string_view stored_reading;
  if (!reading.empty()) {
    reading_storage_.emplace_back(reading);
    stored_reading = reading_storage_.back();
  }

#ifdef SUZUME_DEBUG_INFO
  // Store origin_detail if provided (debug only)
  std::string_view stored_origin_detail;
  if (!origin_detail.empty()) {
    origin_detail_storage_.emplace_back(origin_detail);
    stored_origin_detail = origin_detail_storage_.back();
  }
#endif

  LatticeEdge edge;
  edge.id = static_cast<uint32_t>(all_edges_.size());
  edge.start = start;
  edge.end = end;
  edge.surface = stored_surface;
  edge.pos = pos;
  edge.cost = cost;
  edge.flags = static_cast<EdgeFlags>(flags);
  edge.lemma = stored_lemma;
  edge.reading = stored_reading;
  edge.conj_type = conj_type;
#ifdef SUZUME_DEBUG_INFO
  edge.origin = origin;
  edge.origin_confidence = origin_confidence;
  edge.origin_detail = stored_origin_detail;
#endif

  all_edges_.push_back(edge);
  edge_indices_by_start_[start].push_back(edge.id);
  ++edge_count_;

  return edge.id;
}

std::vector<LatticeEdge> Lattice::edgesAt(size_t pos) const {
  if (pos >= edge_indices_by_start_.size()) {
    return {};
  }
  const auto& indices = edge_indices_by_start_[pos];
  std::vector<LatticeEdge> result;
  result.reserve(indices.size());
  for (uint32_t idx : indices) {
    result.push_back(all_edges_[idx]);
  }
  return result;
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
  for (auto& indices : edge_indices_by_start_) {
    indices.clear();
  }
  all_edges_.clear();
  surface_storage_.clear();
  lemma_storage_.clear();
  reading_storage_.clear();
#ifdef SUZUME_DEBUG_INFO
  origin_detail_storage_.clear();
#endif
  edge_count_ = 0;
}

}  // namespace suzume::core
