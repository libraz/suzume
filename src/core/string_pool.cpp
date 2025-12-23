#include "string_pool.h"

namespace suzume::core {

std::string_view StringPool::intern(std::string_view str) {
  auto iter = index_.find(str);
  if (iter != index_.end()) {
    return *storage_[iter->second];
  }

  auto owned = std::make_unique<std::string>(str);
  std::string_view view = *owned;
  size_t idx = storage_.size();
  storage_.push_back(std::move(owned));
  index_.emplace(view, idx);
  return view;
}

size_t StringPool::memoryUsage() const {
  size_t total = 0;
  for (const auto& str_ptr : storage_) {
    total += str_ptr->size() + sizeof(std::string);
  }
  total += index_.size() * (sizeof(std::string_view) + sizeof(size_t));
  return total;
}

void StringPool::clear() {
  index_.clear();
  storage_.clear();
}

}  // namespace suzume::core
