#include "dictionary/double_array.h"

#include <algorithm>
#include <stdexcept>

namespace suzume::dictionary {

namespace {

constexpr size_t kInitialSize = 8192;
constexpr size_t kBlockSize = 256;

inline uint8_t toByte(char chr) {
  return static_cast<uint8_t>(chr);
}

}  // namespace

// BuildState implementation
void DoubleArray::BuildState::resize(size_t new_size) {
  units.resize(new_size);
  used.resize(new_size, false);
}

size_t DoubleArray::BuildState::findBase(const std::vector<uint8_t>& children) {
  if (children.empty()) {
    return 0;
  }

  size_t first_child = children[0];

  // Start searching from next_check_pos
  for (size_t base_cand = std::max(next_check_pos, first_child); base_cand < units.size() + kBlockSize; ++base_cand) {
    // Check if all children positions are available
    bool all_empty = true;
    for (uint8_t child : children) {
      size_t pos = base_cand ^ child;
      if (pos < units.size() && used[pos]) {
        all_empty = false;
        break;
      }
    }

    if (all_empty) {
      return base_cand;
    }
  }

  return units.size();
}

// DoubleArray implementation
DoubleArray::DoubleArray() = default;

bool DoubleArray::build(const std::vector<std::string>& keys, const std::vector<uint32_t>& values) {
  if (keys.size() != values.size()) {
    return false;
  }

  for (uint32_t value : values) {
    if (value > Unit::kPayloadMask) {
      return false;
    }
  }
  return buildInternal(keys, &values);
}

bool DoubleArray::build(const std::vector<std::string>& keys) {
  if (keys.size() > static_cast<size_t>(Unit::kPayloadMask) + 1U) {
    return false;
  }
  return buildInternal(keys, nullptr);
}

bool DoubleArray::buildInternal(const std::vector<std::string>& keys, const std::vector<uint32_t>* values) {
  if (keys.empty()) {
    clear();
    return true;
  }

  // Empty keys and embedded NUL bytes conflict with the terminal label.
  for (size_t idx = 0; idx < keys.size(); ++idx) {
    if (keys[idx].empty() || keys[idx].find('\0') != std::string::npos || (idx > 0 && keys[idx] <= keys[idx - 1])) {
      return false;
    }
  }

  // The try/catch guards against std::bad_alloc from the growing build buffers;
  // it is compiled only when exceptions are enabled so the core also builds with
  // -fno-exceptions (where an allocation failure terminates, as is standard).
#if defined(__cpp_exceptions) && __cpp_exceptions
  try {
#endif
    BuildState state;
    state.resize(kInitialSize);
    state.used[0] = true;

    // Build recursively starting from root.
    buildRecursive(state, keys, values, 0, keys.size(), 0, 0);
    if (state.failed) {
      return false;
    }

    size_t last_used = 0;
    for (size_t idx = state.units.size(); idx > 0; --idx) {
      if (state.units[idx - 1].data != 0) {
        last_used = idx;
        break;
      }
    }

    // Construct an exact-size result before replacing the current trie. This
    // keeps build transactional and does not depend on shrink_to_fit's
    // non-binding capacity reduction.
    std::vector<Unit> compact(state.units.begin(), state.units.begin() + last_used);
    units_.swap(compact);
#if defined(__cpp_exceptions) && __cpp_exceptions
  } catch (const std::exception&) {
    return false;
  }
#endif

  return true;
}

void DoubleArray::buildRecursive(BuildState& state, const std::vector<std::string>& keys,
                                 const std::vector<uint32_t>* values, size_t begin, size_t end, size_t depth,
                                 size_t parent_pos) {
  if (begin >= end || state.failed) {
    return;
  }

  // Collect unique children at current depth
  std::vector<uint8_t> children;
  size_t leaf_begin = begin;
  size_t leaf_end = begin;

  // Find keys that terminate at this depth
  while (leaf_end < end && keys[leaf_end].size() == depth) {
    ++leaf_end;
  }

  // Collect children (including null terminator for leaves)
  if (leaf_end > leaf_begin) {
    children.push_back(0);  // Null terminator for leaf
  }

  // Collect other children
  uint8_t prev_char = 0;
  bool first = true;
  for (size_t idx = leaf_end; idx < end; ++idx) {
    uint8_t chr = toByte(keys[idx][depth]);
    if (first || chr != prev_char) {
      children.push_back(chr);
      prev_char = chr;
      first = false;
    }
  }

  if (children.empty()) {
    return;
  }

  // Find base value for all children
  size_t base_val = state.findBase(children);

  // Labels replace the old parent/check word, so each parent must have a
  // globally unique base. Reserve this base before descending into children.
  if (base_val > Unit::kPayloadMask) {
    state.failed = true;
    return;
  }
  state.next_check_pos = base_val + 1;

  // Ensure array is large enough
  size_t max_pos = base_val;
  for (uint8_t child : children) {
    max_pos = std::max(max_pos, base_val ^ child);
  }
  if (max_pos >= state.units.size()) {
    state.resize(std::max(max_pos + kBlockSize, state.units.size() * 2));
  }

  // Set base value for parent node
  state.units[parent_pos].setBase(static_cast<uint32_t>(base_val));

  // First pass: mark all children as used (before recursion!)
  for (uint8_t chr : children) {
    size_t child_pos = base_val ^ chr;
    state.units[child_pos].setLabel(chr);
    state.used[child_pos] = true;
  }

  // Second pass: set values and recurse
  size_t child_idx = 0;

  // Handle leaf children (null terminator)
  if (leaf_end > leaf_begin) {
    size_t leaf_pos = base_val ^ 0;  // XOR with null
    const uint32_t value = values == nullptr ? static_cast<uint32_t>(leaf_begin) : (*values)[leaf_begin];
    state.units[parent_pos].setHasLeaf();
    state.units[leaf_pos].setValue(value);
    ++child_idx;
  }

  // Handle other children
  size_t range_begin = leaf_end;
  for (size_t cidx = child_idx; cidx < children.size(); ++cidx) {
    uint8_t chr = children[cidx];
    size_t child_pos = base_val ^ chr;

    // Find range for this child
    size_t range_end = range_begin;
    while (range_end < end && toByte(keys[range_end][depth]) == chr) {
      ++range_end;
    }

    // Recurse
    buildRecursive(state, keys, values, range_begin, range_end, depth + 1, child_pos);

    range_begin = range_end;
  }
}

bool DoubleArray::tryLeaf(size_t node_pos, int32_t& out_value) const {
  if (!units_[node_pos].hasLeaf()) {
    return false;
  }
  size_t base_val = units_[node_pos].base();
  size_t leaf_pos = base_val ^ 0;  // XOR with null terminator

  if (leaf_pos >= units_.size()) {
    return false;
  }

  out_value = units_[leaf_pos].value();
  return true;
}

bool DoubleArray::transition(size_t node_pos, uint8_t chr, size_t& next_pos) const {
  if (chr == 0) {
    return false;
  }
  size_t base_val = units_[node_pos].base();
  size_t child_pos = base_val ^ chr;

  if (child_pos >= units_.size()) {
    return false;
  }

  if (units_[child_pos].label() != chr) {
    return false;
  }

  next_pos = child_pos;
  return true;
}

int32_t DoubleArray::exactMatch(std::string_view key) const {
  if (units_.empty()) {
    return -1;
  }

  size_t node_pos = 0;

  for (char idx : key) {
    size_t next_pos = 0;
    if (!transition(node_pos, toByte(idx), next_pos)) {
      return -1;
    }
    node_pos = next_pos;
  }

  // Check for null terminator (leaf)
  int32_t value = 0;
  if (!tryLeaf(node_pos, value)) {
    return -1;
  }

  return value;
}

std::vector<DoubleArray::Result> DoubleArray::commonPrefixSearch(std::string_view text, size_t start) const {
  std::vector<Result> results;

  if (units_.empty() || start >= text.size()) {
    return results;
  }

  size_t node_pos = 0;

  for (size_t idx = start; idx <= text.size(); ++idx) {
    // Check for null terminator (leaf) at current position
    int32_t value = 0;
    if (tryLeaf(node_pos, value)) {
      Result res{};
      res.value = value;
      res.length = idx - start;
      results.push_back(res);
    }

    // End of text
    if (idx >= text.size()) {
      break;
    }

    // Transition to next node
    size_t next_pos = 0;
    if (!transition(node_pos, toByte(text[idx]), next_pos)) {
      break;
    }

    node_pos = next_pos;
  }

  return results;
}

void DoubleArray::clear() {
  std::vector<Unit>().swap(units_);
}

}  // namespace suzume::dictionary
