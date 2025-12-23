#include "string_pool.h"

#include <cstring>

namespace suzume::dictionary {

DictStringPool::DictStringPool() { data_.reserve(4096); }

uint32_t DictStringPool::add(std::string_view str) {
  auto id = static_cast<uint32_t>(offsets_.size());
  auto offset = static_cast<uint32_t>(data_.size());
  auto length = static_cast<uint16_t>(str.size());

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
  return data_.capacity() + offsets_.capacity() * sizeof(uint32_t) + lengths_.capacity() * sizeof(uint16_t);
}

void DictStringPool::clear() {
  data_.clear();
  offsets_.clear();
  lengths_.clear();
}

std::vector<char> DictStringPool::serialize() const {
  std::vector<char> result;
  size_t total_size = sizeof(uint32_t) * 2 +              // count + data_size
                      offsets_.size() * sizeof(uint32_t) +  // offsets
                      lengths_.size() * sizeof(uint16_t) +  // lengths
                      data_.size();                         // data

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
  std::memcpy(ptr, lengths_.data(), lengths_.size() * sizeof(uint16_t));
  ptr += lengths_.size() * sizeof(uint16_t);
  std::memcpy(ptr, data_.data(), data_.size());

  return result;
}

bool DictStringPool::loadFromMemory(const char* data, size_t size) {
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

  size_t expected_size = sizeof(uint32_t) * 2 + count * sizeof(uint32_t) + count * sizeof(uint16_t) + data_size;
  if (size < expected_size) {
    return false;
  }

  offsets_.resize(count);
  lengths_.resize(count);
  data_.resize(data_size);

  std::memcpy(offsets_.data(), ptr, count * sizeof(uint32_t));
  ptr += count * sizeof(uint32_t);
  std::memcpy(lengths_.data(), ptr, count * sizeof(uint16_t));
  ptr += count * sizeof(uint16_t);
  std::memcpy(data_.data(), ptr, data_size);

  return true;
}

}  // namespace suzume::dictionary
