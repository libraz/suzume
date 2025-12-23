#ifndef SUZUME_CORE_LATTICE_H_
#define SUZUME_CORE_LATTICE_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string_view>
#include <vector>

#include "types.h"

namespace suzume::core {

/**
 * @brief Lattice edge flags
 */
enum class EdgeFlags : uint8_t {
  None = 0,
  FromDictionary = 1 << 0,
  FromUserDict = 1 << 1,
  IsFormalNoun = 1 << 2,
  IsLowInfo = 1 << 3
};

inline EdgeFlags operator|(EdgeFlags lhs, EdgeFlags rhs) {
  return static_cast<EdgeFlags>(static_cast<uint8_t>(lhs) |
                                static_cast<uint8_t>(rhs));
}

inline bool hasFlag(EdgeFlags flags, EdgeFlags flag) {
  return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

/**
 * @brief Lattice edge (morpheme candidate)
 */
struct LatticeEdge {
  uint32_t id{0};                          // Edge ID
  uint32_t start{0};                       // Start position (character index)
  uint32_t end{0};                         // End position (character index)
  std::string_view surface;                // Surface string (StringPool reference)
  PartOfSpeech pos{PartOfSpeech::Unknown}; // Part of speech
  float cost{0.0F};                        // Cost
  EdgeFlags flags{EdgeFlags::None};        // Flags
  std::string_view lemma;                  // Lemma (optional)

  // Flag constants for compatibility
  static constexpr uint8_t kFromDictionary = static_cast<uint8_t>(EdgeFlags::FromDictionary);
  static constexpr uint8_t kFromUserDict = static_cast<uint8_t>(EdgeFlags::FromUserDict);
  static constexpr uint8_t kIsFormalNoun = static_cast<uint8_t>(EdgeFlags::IsFormalNoun);
  static constexpr uint8_t kIsLowInfo = static_cast<uint8_t>(EdgeFlags::IsLowInfo);
  static constexpr uint8_t kIsUnknown = 1 << 4;

  // Flag accessors
  bool fromDictionary() const { return hasFlag(flags, EdgeFlags::FromDictionary); }
  bool fromUserDict() const { return hasFlag(flags, EdgeFlags::FromUserDict); }
  bool isFormalNoun() const { return hasFlag(flags, EdgeFlags::IsFormalNoun); }
  bool isLowInfo() const { return hasFlag(flags, EdgeFlags::IsLowInfo); }
  bool isUnknown() const { return (static_cast<uint8_t>(flags) & kIsUnknown) != 0; }
};

/**
 * @brief Lattice graph for morpheme candidates
 */
class Lattice {
 public:
  explicit Lattice(size_t text_length);
  ~Lattice() = default;

  // Non-copyable, but movable
  Lattice(const Lattice&) = delete;
  Lattice& operator=(const Lattice&) = delete;
  Lattice(Lattice&&) = default;
  Lattice& operator=(Lattice&&) = default;

  /**
   * @brief Add an edge to the lattice
   */
  void addEdge(const LatticeEdge& edge);

  /**
   * @brief Add an edge with parameters
   * @param surface Surface string
   * @param start Start position
   * @param end End position
   * @param pos Part of speech
   * @param cost Cost
   * @param flags Flags
   * @param lemma Lemma (optional)
   * @return Edge ID
   */
  size_t addEdge(std::string_view surface, uint32_t start, uint32_t end,
                 PartOfSpeech pos, float cost, uint8_t flags,
                 std::string_view lemma = {});

  /**
   * @brief Get all edges starting at a position
   */
  const std::vector<LatticeEdge>& edgesAt(size_t pos) const;

  /**
   * @brief Get edge by ID
   * @param edge_id Edge ID
   * @return Edge reference
   */
  const LatticeEdge& getEdge(size_t edge_id) const;

  /**
   * @brief Check if lattice is valid (path exists from start to end)
   */
  bool isValid() const;

  /**
   * @brief Get text length
   */
  size_t textLength() const { return text_length_; }

  /**
   * @brief Get total number of edges
   */
  size_t edgeCount() const { return edge_count_; }

  /**
   * @brief Clear the lattice
   */
  void clear();

 private:
  size_t text_length_{0};
  size_t edge_count_{0};
  std::vector<std::vector<LatticeEdge>> edges_by_start_;
  std::vector<LatticeEdge> all_edges_;  // All edges for getEdge()
  std::deque<std::string> surface_storage_;  // Storage for surface strings (deque for stable pointers)
  std::deque<std::string> lemma_storage_;    // Storage for lemma strings (deque for stable pointers)
  static const std::vector<LatticeEdge> empty_edges_;
};

}  // namespace suzume::core

#endif  // SUZUME_CORE_LATTICE_H_
