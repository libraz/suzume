#ifndef SUZUME_CORE_LATTICE_H_
#define SUZUME_CORE_LATTICE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "dictionary/dictionary.h"
#include "edge_flags.h"
#include "types.h"

namespace suzume::core {

/**
 * @brief Lattice edge (morpheme candidate)
 */
struct LatticeEdge {
  uint32_t id{0};                                                            // Edge ID
  uint32_t start{0};                                                         // Start position (character index)
  uint32_t end{0};                                                           // End position (character index)
  std::string_view surface;                                                  // Surface string (StringPool reference)
  PartOfSpeech pos{PartOfSpeech::Unknown};                                   // Part of speech
  ExtendedPOS extended_pos{ExtendedPOS::Unknown};                            // Extended POS for fine-grained bigram
  float cost{0.0F};                                                          // Cost
  EdgeFlags flags{EdgeFlags::None};                                          // Flags
  std::string_view lemma;                                                    // Lemma (optional)
  dictionary::ConjugationType conj_type{dictionary::ConjugationType::None};  // Conjugation type
  // Candidate provenance used by scoring rules. This compact enum is retained
  // in release and WASM builds so their tokenization matches debug builds.
  CandidateOrigin origin{CandidateOrigin::Unknown};

#ifdef SUZUME_DEBUG_INFO
  float origin_confidence{0.0F};   // Inflection confidence (for debug)
  std::string_view origin_detail;  // Pattern detail (e.g., "ichidan_te_form")
  std::string_view epos_source;    // Where ExtendedPOS was set (e.g., "binary_dict", "l1_dict")
#endif

  // Flag constants for compatibility
  static constexpr uint8_t kFromDictionary = static_cast<uint8_t>(EdgeFlags::FromDictionary);
  static constexpr uint8_t kFromUserDict = static_cast<uint8_t>(EdgeFlags::FromUserDict);
  static constexpr uint8_t kIsFormalNoun = static_cast<uint8_t>(EdgeFlags::IsFormalNoun);
  static constexpr uint8_t kIsUnknown = static_cast<uint8_t>(EdgeFlags::IsUnknown);
  static constexpr uint8_t kHasCustomCost = static_cast<uint8_t>(EdgeFlags::HasCustomCost);
  static constexpr uint8_t kLemmaVerified = static_cast<uint8_t>(EdgeFlags::LemmaVerified);

  // Flag accessors
  bool fromDictionary() const { return hasFlag(flags, EdgeFlags::FromDictionary); }
  bool fromUserDict() const { return hasFlag(flags, EdgeFlags::FromUserDict); }
  bool isFormalNoun() const { return hasFlag(flags, EdgeFlags::IsFormalNoun); }
  bool hasCustomCost() const { return hasFlag(flags, EdgeFlags::HasCustomCost); }
  // A dictionary edge's lemma is dictionary-attested by definition, so OR in
  // fromDictionary(): consumers see a strict superset of the dictionary gate.
  bool lemmaVerified() const { return fromDictionary() || hasFlag(flags, EdgeFlags::LemmaVerified); }
  bool isUnknown() const { return hasFlag(flags, EdgeFlags::IsUnknown); }
};

/**
 * @brief Lattice graph for morpheme candidates
 *
 * @note Maximum number of edges is limited to UINT32_MAX to prevent ID overflow.
 *       In practice, this limit is never reached with normal text.
 */
class Lattice {
 public:
  /// Maximum number of edges (limited by uint32_t ID)
  static constexpr size_t kMaxEdges = static_cast<size_t>(UINT32_MAX);

  explicit Lattice(size_t text_length);
  ~Lattice() = default;

  // Non-copyable, but movable
  Lattice(const Lattice&) = delete;
  Lattice& operator=(const Lattice&) = delete;
  Lattice(Lattice&&) = default;
  Lattice& operator=(Lattice&&) = default;

  /**
   * @brief Add an edge with parameters
   * @param surface Surface string
   * @param start Start position
   * @param end End position
   * @param pos Part of speech
   * @param cost Cost
   * @param flags Flags
   * @param lemma Lemma (optional)
   * @param conj_type Conjugation type (optional)
   * @param origin Candidate origin for scoring and debug output (optional)
   * @param origin_confidence Origin confidence for debug (optional)
   * @param origin_detail Origin detail pattern for debug (optional)
   * @param extended_pos Extended POS for bigram (optional, defaults from pos)
   * @param epos_source Where ExtendedPOS was determined (optional, for debug)
   * @return Edge ID
   */
  size_t addEdge(std::string_view surface, uint32_t start, uint32_t end, PartOfSpeech pos, float cost, uint8_t flags,
                 std::string_view lemma = {}, dictionary::ConjugationType conj_type = dictionary::ConjugationType::None,
                 CandidateOrigin origin = CandidateOrigin::Unknown, float origin_confidence = 0.0F,
                 std::string_view origin_detail = {}, ExtendedPOS extended_pos = ExtendedPOS::Unknown,
                 std::string_view epos_source = {});

  /**
   * @brief Get all edges starting at a position
   * @note Returns by value (indices internally, edges constructed on-demand)
   *       This enables memory optimization while keeping API compatibility.
   *       Caller can use: const auto& edges = edgesAt(pos); // lifetime extended
   */
  std::vector<LatticeEdge> edgesAt(size_t pos) const;

  /**
   * @brief Get IDs of all edges starting at a position (no copy)
   * @note Returns a reference to internal storage. Resolve each ID with
   *       getEdge(); prefer this over edgesAt() on hot paths to avoid
   *       building a temporary edge vector.
   */
  const std::vector<uint32_t>& edgeIdsAt(size_t pos) const;

  /**
   * @brief Get IDs of all edges ending at a position (no copy)
   * @note Returns a reference to internal storage. This reverse index lets
   *       predecessor queries inspect only edges adjacent to the requested
   *       boundary instead of rescanning every earlier start position.
   */
  const std::vector<uint32_t>& edgeIdsEndingAt(size_t pos) const;

  /**
   * @brief Get edge by ID
   * @param edge_id Edge ID
   * @return Edge reference
   */
  const LatticeEdge& getEdge(size_t edge_id) const;

  /**
   * @brief Overwrite an edge's cost after generation
   * @param edge_id Edge ID (ignored when out of range)
   * @param cost New cost
   * @note Marks the edge as carrying a deliberately tuned cost so the scorer
   *       honours the value instead of falling back to the category cost.
   */
  void setEdgeCost(size_t edge_id, float cost);

  /**
   * @brief Check if lattice is valid (path exists from start to end)
   */
  bool isValid() const;

  /**
   * @brief Get text length
   */
  size_t textLength() const { return text_length_; }

  /**
   * @brief Clear the lattice
   */
  void clear();

 private:
  /**
   * @brief Append-only text storage whose handed-out views stay valid
   *
   * Every edge field that is not a view into the input text points here, so a
   * stored run of bytes may never move afterwards. Filling fixed chunks in
   * place satisfies that without the per-string allocation and per-element
   * bookkeeping a node-based container needs.
   */
  class TextStorage {
   public:
    /** Copy text into the storage and return a view that stays valid until clear(). */
    std::string_view store(std::string_view text);

    /** Drop every stored run, keeping the first chunk's memory for reuse. */
    void clear();

   private:
    static constexpr size_t kChunkBytes = 4096;
    std::vector<std::unique_ptr<char[]>> chunks_;
    size_t used_{0};      // Bytes already handed out from the newest chunk
    size_t capacity_{0};  // Size of the newest chunk
  };

  size_t text_length_{0};
  std::vector<std::vector<uint32_t>> edge_indices_by_start_;  // Edge indices per position
  std::vector<std::vector<uint32_t>> edge_indices_by_end_;    // Edge indices per end position
  std::vector<LatticeEdge> all_edges_;                        // All edges (primary storage)
  TextStorage text_storage_;                                  // Backing store for every string an edge holds
};

/** @brief Whether any edge among edge_ids satisfies pred */
template <typename Pred>
bool anyEdgeAmong(const Lattice& lattice, const std::vector<uint32_t>& edge_ids, Pred pred) {
  for (const uint32_t edge_id : edge_ids) {
    if (pred(lattice.getEdge(edge_id))) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Whether any edge ending at end_pos satisfies pred
 * @note Takes the predicate as a template parameter so the test inlines; this
 *       runs inside candidate generation on every boundary.
 */
template <typename Pred>
bool anyEdgeEndingAt(const Lattice& lattice, size_t end_pos, Pred pred) {
  return anyEdgeAmong(lattice, lattice.edgeIdsEndingAt(end_pos), pred);
}

/** @brief Whether any edge starting at start_pos satisfies pred */
template <typename Pred>
bool anyEdgeStartingAt(const Lattice& lattice, size_t start_pos, Pred pred) {
  return anyEdgeAmong(lattice, lattice.edgeIdsAt(start_pos), pred);
}

}  // namespace suzume::core

#endif  // SUZUME_CORE_LATTICE_H_
