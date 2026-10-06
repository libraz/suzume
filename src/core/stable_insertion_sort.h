#ifndef SUZUME_CORE_STABLE_INSERTION_SORT_H_
#define SUZUME_CORE_STABLE_INSERTION_SORT_H_

#include <cstddef>
#include <utility>
#include <vector>

namespace suzume::core {

/**
 * @brief Stable insertion sort of items[first_index..] by `less`.
 *
 * Candidate lists are small and already close to generation order. This avoids
 * pulling the generic introsort implementation into WASM while keeping equal
 * elements in generation order, which keeps tie-breaking deterministic.
 */
template <typename T, typename Less>
void stableInsertionSort(std::vector<T>& items, size_t first_index, Less less) {
  for (size_t idx = first_index + 1; idx < items.size(); ++idx) {
    T item = std::move(items[idx]);
    size_t insert_at = idx;
    while (insert_at > first_index && less(item, items[insert_at - 1])) {
      items[insert_at] = std::move(items[insert_at - 1]);
      --insert_at;
    }
    items[insert_at] = std::move(item);
  }
}

}  // namespace suzume::core

#endif  // SUZUME_CORE_STABLE_INSERTION_SORT_H_
