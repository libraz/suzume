#ifndef SUZUME_CORE_VITERBI_H_
#define SUZUME_CORE_VITERBI_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

// <iostream> is pulled in by debug.h only under SUZUME_DEBUG; the debug streaming
// below goes through SUZUME_DEBUG_STREAM, so no unconditional include here (keeps
// iostream out of the release/WASM build).
#include "debug.h"
#include "lattice.h"
#include "morpheme.h"

namespace suzume::core {

inline constexpr size_t kNumExtendedPosTypes = static_cast<size_t>(ExtendedPOS::Count_);

// Sentence-boundary costs are linguistic, so they belong to the scorer: a
// Scorer supplies bosCost(edge) for an edge that opens the sentence and
// eosCost(edge, prev) for one that closes it. Keeping them out of core also
// keeps them inside the guardrail ratchet's named-constant rule.
//
// eosCost takes the preceding category because whether a morpheme can close a
// sentence is not always a property of that morpheme alone: a bound nominal
// closes one when a modifier heads it and does not when a continuative does.
// The BOS state has no preceding morpheme and passes ExtendedPOS::Unknown.

// Per-transition tie-break: slightly prefer fewer, longer morphemes.
inline constexpr float kTransitionCost = 0.001F;

// Number of lowest-cost paths retained per (position, ExtendedPOS) state key.
// Connection rules branch on the predecessor's surface, so a single-best key
// would prune a surface that a downstream surface-conditional rule prefers.
inline constexpr size_t kStatesPerKey = 2;

// Sentinel edge ID marking the BOS state (no arriving edge).
inline constexpr uint32_t kBosEdgeId = std::numeric_limits<uint32_t>::max();

/**
 * @brief Viterbi result with path and cost
 */
struct ViterbiResult {
  std::vector<size_t> path;  // Edge IDs in order
  float total_cost{0.0F};    // Total path cost
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

    // States are keyed on (position, ExtendedPOS). Each key retains up to
    // kStatesPerKey lowest-cost paths instead of a single one, so predecessors
    // with the same ExtendedPOS but different scoring identities survive into
    // the next position's connectionCost.
    // The cost of extending a path depends only on the edge it arrived with,
    // so per edge we insert only its cheapest predecessor. Entries within a
    // key are kept identity-distinct: among arrivals indistinguishable to the
    // scorer only the cheapest path is retained.
    struct StateEntry {
      float cost{std::numeric_limits<float>::max()};
      uint32_t edge_id{kBosEdgeId};  // Edge the path arrived with (kBosEdgeId = BOS)
      uint16_t prev_epos_idx{0};     // ExtendedPOS key of the predecessor state
      uint8_t prev_slot{0};          // K-best slot within the predecessor key
    };

    struct StateSlots {
      std::array<StateEntry, kStatesPerKey> entries{};
      uint8_t count{0};

      static bool hasSameScoringIdentity(const LatticeEdge& lhs, const LatticeEdge& rhs) {
        return lhs.start == rhs.start && lhs.surface == rhs.surface && lhs.lemma == rhs.lemma && lhs.pos == rhs.pos &&
               lhs.origin == rhs.origin && lhs.conj_type == rhs.conj_type &&
               lhs.fromDictionary() == rhs.fromDictionary() && lhs.isFormalNoun() == rhs.isFormalNoun() &&
               lhs.lemmaVerified() == rhs.lemmaVerified();
      }

      // Insert a candidate, keeping entries sorted by ascending cost and
      // dropping the most expensive one when the key is full. A candidate
      // whose scoring identity matches an existing entry replaces it only if
      // cheaper (identity-distinct invariant). Slot indices of existing entries may
      // shift here; this is safe because every write to a position's keys
      // happens before any backpointer into them is recorded (the forward
      // pass is position-ordered and start < end).
      void insert(const Lattice& lattice, float cost, uint32_t edge_id, uint16_t prev_epos_idx, uint8_t prev_slot) {
        const auto& edge = lattice.getEdge(edge_id);
        // The slot vacated for the candidate: its identity twin, else a new or the most expensive slot.
        size_t slot = count;
        for (size_t existing = 0; existing < count; ++existing) {
          if (hasSameScoringIdentity(lattice.getEdge(entries[existing].edge_id), edge)) {
            if (cost >= entries[existing].cost) {
              return;
            }
            slot = existing;
            break;
          }
        }
        if (slot == kStatesPerKey) {
          if (cost >= entries[kStatesPerKey - 1].cost) {
            return;
          }
          slot = kStatesPerKey - 1;
        } else if (slot == count) {
          ++count;
        }
        while (slot > 0 && cost < entries[slot - 1].cost) {
          entries[slot] = entries[slot - 1];
          --slot;
        }
        entries[slot] = StateEntry{cost, edge_id, prev_epos_idx, prev_slot};
      }
    };

    struct KeyState {
      uint16_t epos_idx{0};
      StateSlots slots;
    };

    struct PositionStates {
      std::vector<KeyState> keys;

      const StateSlots* find(uint16_t epos_idx) const {
        for (const auto& key : keys) {
          if (key.epos_idx == epos_idx) {
            return &key.slots;
          }
        }
        return nullptr;
      }

      StateSlots* find(uint16_t epos_idx) { return const_cast<StateSlots*>(std::as_const(*this).find(epos_idx)); }

      StateSlots& getOrCreate(uint16_t epos_idx) {
        if (StateSlots* existing = find(epos_idx); existing != nullptr) {
          return *existing;
        }
        keys.push_back(KeyState{epos_idx, {}});
        return keys.back().slots;
      }
    };

    // Keep only ExtendedPOS keys that actually occur at each position. The
    // former dense table retained dozens of empty StateSlots per position.
    std::vector<PositionStates> states_by_pos(text_len + 1);

    // Initialize BOS state at position 0, ExtendedPOS=Unknown
    states_by_pos[0].getOrCreate(static_cast<uint16_t>(ExtendedPOS::Unknown)).insert(lattice, 0.0F, kBosEdgeId, 0, 0);

    // Forward pass - process positions in order
    for (size_t pos = 0; pos < text_len; ++pos) {
      const auto& states_at_pos = states_by_pos[pos];
      if (states_at_pos.keys.empty()) {
        continue;
      }

      for (uint32_t edge_id : lattice.edgeIdsAt(pos)) {
        const auto& edge = lattice.getEdge(edge_id);
        // Bounds check - skip edges that go beyond text length
        if (edge.end > text_len) {
          continue;
        }
        const float word_cost = scorer.wordCost(edge);

        const bool closes_sentence = edge.end == text_len;

        // Find the cheapest predecessor for this edge across all retained
        // states at this position.
        float best_total = std::numeric_limits<float>::max();
        uint16_t best_prev_epos = 0;
        uint8_t best_prev_slot = 0;

        for (const auto& key : states_at_pos.keys) {
          const auto& slots = key.slots;
          for (uint8_t slot = 0; slot < slots.count; ++slot) {
            const auto& entry = slots.entries[slot];

            float conn_cost = 0.0F;
            ExtendedPOS prev_extended_pos = ExtendedPOS::Unknown;
            if (entry.edge_id != kBosEdgeId) {
              const auto& prev_edge = lattice.getEdge(entry.edge_id);
              conn_cost = scorer.connectionCost(prev_edge, edge);
              prev_extended_pos = prev_edge.extended_pos;
            } else {
              // BOS (beginning of sentence) connection cost
              conn_cost = scorer.bosCost(edge);
            }

            // EOS penalty: an edge that terminates the sentence but cannot
            // naturally end one (mirror of the BOS cost above).
            const float eos_cost = closes_sentence ? scorer.eosCost(edge, prev_extended_pos) : 0.0F;

            const float total = entry.cost + word_cost + conn_cost + eos_cost + kTransitionCost;

            SUZUME_DEBUG_VERBOSE_BLOCK {
              SUZUME_DEBUG_STREAM << "[VITERBI] pos=" << pos << " \"" << edge.surface << "\" (" << posToString(edge.pos)
                                  << "/" << extendedPosToString(edge.extended_pos) << ")";
#ifdef SUZUME_DEBUG_INFO
              if (edge.origin != CandidateOrigin::Unknown) {
                SUZUME_DEBUG_STREAM << " [src:" << originToString(edge.origin);
                if (!edge.origin_detail.empty()) {
                  SUZUME_DEBUG_STREAM << "/" << edge.origin_detail;
                }
                SUZUME_DEBUG_STREAM << "]";
              }
#endif
              SUZUME_DEBUG_STREAM << " from " << extendedPosToString(static_cast<ExtendedPOS>(key.epos_idx)) << "#"
                                  << static_cast<int>(slot) << " word=" << word_cost << " conn=" << conn_cost
                                  << " total=" << total << "\n";
            }

            if (total < best_total) {
              best_total = total;
              best_prev_epos = key.epos_idx;
              best_prev_slot = slot;
            }
          }
        }

        if (best_total == std::numeric_limits<float>::max()) {
          continue;
        }

        size_t next_epos_idx = static_cast<size_t>(edge.extended_pos);
        if (next_epos_idx >= kNumExtendedPosTypes) {
          next_epos_idx = static_cast<size_t>(ExtendedPOS::Unknown);
        }
        states_by_pos[edge.end]
            .getOrCreate(static_cast<uint16_t>(next_epos_idx))
            .insert(lattice, best_total, edge.id, best_prev_epos, best_prev_slot);
      }
    }

    // Find best and second-best states at final position
    size_t best_final_epos_idx = 0;
    uint8_t best_final_slot = 0;
    size_t second_final_epos_idx = 0;
    uint8_t second_final_slot = 0;
    float best_cost = std::numeric_limits<float>::max();
    float second_cost = std::numeric_limits<float>::max();

    const auto& final_states = states_by_pos[text_len];
    for (const auto& key : final_states.keys) {
      for (uint8_t slot = 0; slot < key.slots.count; ++slot) {
        const float cost = key.slots.entries[slot].cost;
        if (cost < best_cost) {
          second_cost = best_cost;
          second_final_epos_idx = best_final_epos_idx;
          second_final_slot = best_final_slot;
          best_cost = cost;
          best_final_epos_idx = key.epos_idx;
          best_final_slot = slot;
        } else if (cost < second_cost) {
          second_cost = cost;
          second_final_epos_idx = key.epos_idx;
          second_final_slot = slot;
        }
      }
    }
    (void)second_final_epos_idx;
    (void)second_final_slot;

    // Backtrack: follow the specific (ExtendedPOS, slot) entry that produced
    // each chosen edge.
    const auto backtrack = [&](size_t epos_idx, uint8_t slot) {
      std::vector<size_t> path;
      size_t current_pos = text_len;
      while (current_pos > 0) {
        const StateSlots* slots = states_by_pos[current_pos].find(static_cast<uint16_t>(epos_idx));
        if (slots == nullptr || slot >= slots->count) {
          break;
        }
        const auto& entry = slots->entries[slot];
        if (entry.edge_id == kBosEdgeId) {
          break;
        }
        path.push_back(entry.edge_id);
        current_pos = lattice.getEdge(entry.edge_id).start;
        epos_idx = entry.prev_epos_idx;
        slot = entry.prev_slot;
      }
      std::reverse(path.begin(), path.end());
      return path;
    };

    if (best_cost < std::numeric_limits<float>::max()) {
      result.total_cost = best_cost;
      result.path = backtrack(best_final_epos_idx, best_final_slot);
    }

    // Debug: print final path and runner-up comparison
    SUZUME_DEBUG_IF(!result.path.empty()) {
      SUZUME_DEBUG_STREAM << "[VITERBI] Best path (cost=" << result.total_cost << "): ";
      for (size_t i = 0; i < result.path.size(); ++i) {
        const auto& edge = lattice.getEdge(result.path[i]);
        if (i > 0)
          SUZUME_DEBUG_STREAM << " → ";
        SUZUME_DEBUG_STREAM << "\"" << edge.surface << "\"(" << posToString(edge.pos) << "/"
                            << extendedPosToString(edge.extended_pos) << ")";
      }
      // Show margin over second-best if available
      if (second_cost < std::numeric_limits<float>::max() && second_cost != best_cost) {
        SUZUME_DEBUG_STREAM << " [margin=" << (second_cost - best_cost) << "]";
      }
      SUZUME_DEBUG_STREAM << "\n";

      // Show runner-up path at verbose level
      SUZUME_DEBUG_VERBOSE_BLOCK {
        if (second_cost < std::numeric_limits<float>::max() && second_cost != best_cost) {
          const std::vector<size_t> runner_up_path = backtrack(second_final_epos_idx, second_final_slot);

          if (!runner_up_path.empty()) {
            SUZUME_DEBUG_STREAM << "[VITERBI] Runner-up (cost=" << second_cost << "): ";
            for (size_t i = 0; i < runner_up_path.size(); ++i) {
              const auto& edge = lattice.getEdge(runner_up_path[i]);
              if (i > 0)
                SUZUME_DEBUG_STREAM << " → ";
              SUZUME_DEBUG_STREAM << "\"" << edge.surface << "\"(" << posToString(edge.pos) << ")";
            }
            SUZUME_DEBUG_STREAM << "\n";
          }
        }
      }
    }

    return result;
  }
};

}  // namespace suzume::core

#endif  // SUZUME_CORE_VITERBI_H_
