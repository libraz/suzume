#ifndef SUZUME_DICTIONARY_STRING_POOL_H_
#define SUZUME_DICTIONARY_STRING_POOL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace suzume::dictionary {

/**
 * @brief Dictionary-specific string pool
 *
 * Optimized for dictionary storage where strings are
 * added once and never removed.
 */
class DictStringPool {
 public:
  DictStringPool();
  ~DictStringPool() = default;

  // Non-copyable
  DictStringPool(const DictStringPool&) = delete;
  DictStringPool& operator=(const DictStringPool&) = delete;

  // Movable
  DictStringPool(DictStringPool&&) = default;
  DictStringPool& operator=(DictStringPool&&) = default;

  /**
   * @brief Add a string and return its ID
   * @param str String to add
   * @return String ID
   */
  uint32_t add(std::string_view str);

  /**
   * @brief Get string by ID
   * @param id String ID
   * @return String view, or empty if invalid ID
   */
  std::string_view get(uint32_t id) const;

  /**
   * @brief Get number of strings
   */
  size_t size() const { return offsets_.size(); }

  /**
   * @brief Get total memory usage
   */
  size_t memoryUsage() const;

  /**
   * @brief Clear all strings
   */
  void clear();

  /**
   * @brief Serialize to bytes
   */
  std::vector<char> serialize() const;

  /**
   * @brief Load from memory
   */
  bool loadFromMemory(const char* data, size_t size);

 private:
  std::string data_;                // Concatenated strings
  std::vector<uint32_t> offsets_;   // Start offset for each string
  std::vector<uint16_t> lengths_;   // Length of each string
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_STRING_POOL_H_
