#ifndef SUZUME_DICTIONARY_DOUBLE_ARRAY_H_
#define SUZUME_DICTIONARY_DOUBLE_ARRAY_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace suzume::dictionary {

/**
 * @brief Double-Array Trie implementation
 *
 * Efficient trie structure using XOR-based addressing.
 * Based on the algorithm used by Darts-clone.
 *
 * Properties:
 * - O(m) lookup where m is key length
 * - Compact memory representation
 * - WASM compatible (contiguous memory arrays)
 */
class DoubleArray {
 public:
  /**
   * @brief Result of common prefix search
   */
  struct Result {
    int32_t value;  // Associated value (entry index)
    size_t length;  // Match length in bytes
  };

  DoubleArray();
  ~DoubleArray() = default;

  // Non-copyable, movable
  DoubleArray(const DoubleArray&) = delete;
  DoubleArray& operator=(const DoubleArray&) = delete;
  DoubleArray(DoubleArray&&) noexcept = default;
  DoubleArray& operator=(DoubleArray&&) noexcept = default;

  /**
   * @brief Build double-array from sorted key-value pairs
   * @param keys Sorted keys (must be sorted lexicographically)
   * @param values Values corresponding to each key, each within Unit::kPayloadMask
   * @return true on success, false on failure
   *
   * @note Keys MUST be sorted. Unsorted keys will cause incorrect results.
   * @note Values are unsigned entry indices; exactMatch reserves -1 for "absent",
   *       so the stored payload never carries a sign of its own.
   */
  bool build(const std::vector<std::string>& keys, const std::vector<uint32_t>& values);

  /**
   * @brief Build with each key's sorted index as its value
   */
  bool build(const std::vector<std::string>& keys);

  /**
   * @brief Search for exact match
   * @param key Key to search
   * @return Value if found, -1 otherwise
   */
  int32_t exactMatch(std::string_view key) const;

  /**
   * @brief Common prefix search from position
   * @param text Text to search
   * @param start Start position in bytes
   * @return Vector of matching results (value, length)
   */
  std::vector<Result> commonPrefixSearch(std::string_view text, size_t start = 0) const;

 private:
  /**
   * @brief Double-array unit (packed 32-bit)
   *
   * Bits 0-7 store the incoming byte label and bits 8-30 store the absolute
   * child base. Bit 31 records whether an internal node has a terminal child. The
   * terminal cell stores value+1 in bits 8-31. In the XOR layout, validating a
   * transition's label replaces a separate 32-bit parent/check field.
   */
  struct Unit {
    static constexpr uint32_t kPayloadMask = 0x007FFFFF;

    uint32_t data = 0;

    bool hasLeaf() const { return (data >> 31U) != 0; }
    uint8_t label() const { return static_cast<uint8_t>(data); }
    uint32_t base() const { return (data >> 8U) & kPayloadMask; }
    int32_t value() const { return static_cast<int32_t>((data >> 8U) - 1U); }
    void setLabel(uint8_t label_val) { data = (data & 0xFFFFFF00U) | label_val; }
    void setBase(uint32_t base_val) { data = (data & 0x800000FFU) | (base_val << 8U); }
    void setHasLeaf() { data |= 0x80000000U; }
    void setValue(uint32_t val) { data = (val + 1U) << 8U; }
  };

  std::vector<Unit> units_;

  /** @brief Release every unit, leaving an empty array. */
  void clear();

  /**
   * @brief Read the leaf value reachable from a node via the null terminator
   * @param node_pos Current node position
   * @param out_value Set to the leaf value when a leaf is present
   * @return true if the node has a leaf child, false otherwise
   *
   * @note A null-label child is always a leaf in the serialized trie.
   */
  bool tryLeaf(size_t node_pos, int32_t& out_value) const;

  /**
   * @brief Transition from a node on a single byte
   * @param node_pos Current node position
   * @param chr Transition byte
   * @param next_pos Set to the child position when the transition exists
   * @return true if a valid child exists, false otherwise
   *
   * @note The XOR child position is validated by its stored byte label.
   */
  bool transition(size_t node_pos, uint8_t chr, size_t& next_pos) const;

  // Build helpers
  struct BuildState {
    std::vector<Unit> units;
    std::vector<bool> used;
    size_t next_check_pos = 0;
    // Set when a capacity overflow aborts the build; propagated in place of an
    // exception so the core compiles under -fno-exceptions.
    bool failed = false;

    void resize(size_t new_size);
    size_t findBase(const std::vector<uint8_t>& children);
  };

  void buildRecursive(BuildState& state, const std::vector<std::string>& keys, const std::vector<uint32_t>* values,
                      size_t begin, size_t end, size_t depth, size_t parent_pos);
  bool buildInternal(const std::vector<std::string>& keys, const std::vector<uint32_t>* values);
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_DOUBLE_ARRAY_H_
