#ifndef SUZUME_CORE_STRING_POOL_H_
#define SUZUME_CORE_STRING_POOL_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace suzume::core {

/**
 * @brief String interning pool for memory efficiency
 *
 * Stores unique strings and returns string_view references.
 * Useful for reducing memory usage when many strings are duplicated.
 */
class StringPool {
 public:
  StringPool() = default;
  ~StringPool() = default;

  // Non-copyable, movable
  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;
  StringPool(StringPool&&) = default;
  StringPool& operator=(StringPool&&) = default;

  /**
   * @brief Intern a string and return a view to the stored copy
   * @param str String to intern
   * @return View to the interned string
   */
  std::string_view intern(std::string_view str);

  /**
   * @brief Get the number of unique strings stored
   */
  size_t size() const { return index_.size(); }

  /**
   * @brief Get total memory usage (approximate)
   */
  size_t memoryUsage() const;

  /**
   * @brief Clear all interned strings
   */
  void clear();

 private:
  std::vector<std::unique_ptr<std::string>> storage_;
  std::unordered_map<std::string_view, size_t> index_;
};

}  // namespace suzume::core

#endif  // SUZUME_CORE_STRING_POOL_H_
