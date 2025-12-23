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
    int32_t value;    // Associated value (entry index)
    size_t length;    // Match length in bytes
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
   * @param values Values corresponding to each key
   * @return true on success, false on failure
   *
   * @note Keys MUST be sorted. Unsorted keys will cause incorrect results.
   */
  bool build(const std::vector<std::string>& keys,
             const std::vector<int32_t>& values);

  /**
   * @brief Build with uint32_t values (convenience overload)
   */
  bool build(const std::vector<std::string>& keys,
             const std::vector<uint32_t>& values);

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
   * @param max_results Maximum number of results (0 = unlimited)
   * @return Vector of matching results (value, length)
   */
  std::vector<Result> commonPrefixSearch(std::string_view text,
                                         size_t start = 0,
                                         size_t max_results = 0) const;

  /**
   * @brief Get size of the double-array (number of units)
   */
  size_t size() const { return units_.size(); }

  /**
   * @brief Check if the double-array is empty
   */
  bool empty() const { return units_.empty(); }

  /**
   * @brief Clear the double-array
   */
  void clear();

  /**
   * @brief Get memory usage in bytes
   */
  size_t memoryUsage() const;

  /**
   * @brief Serialize to binary data
   * @return Binary representation
   */
  std::vector<uint8_t> serialize() const;

  /**
   * @brief Deserialize from binary data
   * @param data Binary data
   * @param size Data size
   * @return true on success
   */
  bool deserialize(const uint8_t* data, size_t size);

 private:
  /**
   * @brief Double-array unit (packed 32-bit)
   *
   * For internal nodes:
   *   - Bits 0-30: base (offset to children)
   *   - Bit 31: 0 (not a leaf)
   *
   * For leaf nodes:
   *   - Bits 0-30: value
   *   - Bit 31: 1 (leaf)
   */
  struct Unit {
    uint32_t base_or_value;  // base for internal, value for leaf
    uint32_t check;          // parent position ^ label

    bool hasLeaf() const { return (base_or_value >> 31) != 0; }
    uint32_t base() const { return base_or_value & 0x7FFFFFFF; }
    int32_t value() const { return static_cast<int32_t>(base_or_value & 0x7FFFFFFF); }
    void setBase(uint32_t base_val) { base_or_value = base_val & 0x7FFFFFFF; }
    void setLeaf(int32_t val) { base_or_value = (static_cast<uint32_t>(val) & 0x7FFFFFFF) | 0x80000000; }
  };

  std::vector<Unit> units_;

  // Build helpers
  struct BuildState {
    std::vector<Unit> units;
    std::vector<bool> used;
    size_t next_check_pos = 0;

    void resize(size_t new_size);
    size_t findBase(const std::vector<uint8_t>& children);
  };

  void buildRecursive(BuildState& state,
                      const std::vector<std::string>& keys,
                      const std::vector<int32_t>& values,
                      size_t begin, size_t end,
                      size_t depth, size_t parent_pos);
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_DOUBLE_ARRAY_H_
