#include "string_pool.h"

#include <cstring>
#include <limits>

namespace suzume::dictionary {

DictStringPool::DictStringPool() {
  data_.reserve(4096);
}

uint32_t DictStringPool::add(std::string_view str) {
  if (offsets_.size() >= static_cast<size_t>(std::numeric_limits<uint32_t>::max()) ||
      data_.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) ||
      str.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) ||
      data_.size() + str.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    return std::numeric_limits<uint32_t>::max();
  }

  auto id = static_cast<uint32_t>(offsets_.size());
  auto offset = static_cast<uint32_t>(data_.size());
  auto length = static_cast<uint32_t>(str.size());

  data_.append(str);
  offsets_.push_back(offset);
  lengths_.push_back(length);

  return id;
}

std::string_view DictStringPool::get(uint32_t id) const {
  if (id >= offsets_.size()) {
    return {};
  }
  return {data_.data() + offsets_[id], lengths_[id]};
}

size_t DictStringPool::memoryUsage() const {
  return data_.capacity() + offsets_.capacity() * sizeof(uint32_t) + lengths_.capacity() * sizeof(uint32_t);
}

void DictStringPool::clear() {
  data_.clear();
  offsets_.clear();
  lengths_.clear();
}

std::vector<char> DictStringPool::serialize() const {
  std::vector<char> result;
  size_t total_size = sizeof(uint32_t) * 2 +                // count + data_size
                      offsets_.size() * sizeof(uint32_t) +  // offsets
                      lengths_.size() * sizeof(uint32_t) +  // lengths
                      data_.size();                         // data

  if (offsets_.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) ||
      data_.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    return {};
  }

  result.resize(total_size);
  char* ptr = result.data();

  auto count = static_cast<uint32_t>(offsets_.size());
  auto data_size = static_cast<uint32_t>(data_.size());

  std::memcpy(ptr, &count, sizeof(count));
  ptr += sizeof(count);
  std::memcpy(ptr, &data_size, sizeof(data_size));
  ptr += sizeof(data_size);
  std::memcpy(ptr, offsets_.data(), offsets_.size() * sizeof(uint32_t));
  ptr += offsets_.size() * sizeof(uint32_t);
  std::memcpy(ptr, lengths_.data(), lengths_.size() * sizeof(uint32_t));
  ptr += lengths_.size() * sizeof(uint32_t);
  std::memcpy(ptr, data_.data(), data_.size());

  return result;
}

bool DictStringPool::loadFromMemory(const char* data, size_t size) {
  if (data == nullptr) {
    return false;
  }

  if (size < sizeof(uint32_t) * 2) {
    return false;
  }

  const char* ptr = data;
  uint32_t count = 0;
  uint32_t data_size = 0;

  std::memcpy(&count, ptr, sizeof(count));
  ptr += sizeof(count);
  std::memcpy(&data_size, ptr, sizeof(data_size));
  ptr += sizeof(data_size);

  size_t remaining = size - sizeof(uint32_t) * 2;
  if (static_cast<size_t>(count) > remaining / sizeof(uint32_t)) {
    return false;
  }
  size_t offsets_size = static_cast<size_t>(count) * sizeof(uint32_t);
  remaining -= offsets_size;

  if (static_cast<size_t>(count) > remaining / sizeof(uint32_t)) {
    return false;
  }
  size_t lengths_size = static_cast<size_t>(count) * sizeof(uint32_t);
  remaining -= lengths_size;

  if (static_cast<size_t>(data_size) > remaining) {
    return false;
  }

  std::vector<uint32_t> offsets(count);
  std::vector<uint32_t> lengths(count);
  std::string loaded_data(data_size, '\0');

  std::memcpy(offsets.data(), ptr, offsets_size);
  ptr += offsets_size;
  std::memcpy(lengths.data(), ptr, lengths_size);
  ptr += lengths_size;
  std::memcpy(loaded_data.data(), ptr, data_size);

  for (size_t idx = 0; idx < offsets.size(); ++idx) {
    if (offsets[idx] > data_size || lengths[idx] > data_size - offsets[idx]) {
      return false;
    }
  }

  offsets_ = std::move(offsets);
  lengths_ = std::move(lengths);
  data_ = std::move(loaded_data);

  return true;
}

}  // namespace suzume::dictionary
