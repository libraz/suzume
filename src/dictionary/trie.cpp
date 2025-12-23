#include "trie.h"

#include "normalize/utf8.h"

namespace suzume::dictionary {

Trie::Trie() : root_(std::make_unique<TrieNode>()) {}

void Trie::insert(std::string_view key, uint32_t entry_id) {
  TrieNode* node = root_.get();
  size_t pos = 0;

  while (pos < key.size()) {
    char32_t cp = normalize::decodeUtf8(key, pos);
    auto it = node->children.find(cp);
    if (it == node->children.end()) {
      auto [inserted, _] = node->children.emplace(cp, std::make_unique<TrieNode>());
      node = inserted->second.get();
    } else {
      node = it->second.get();
    }
  }

  node->entry_ids.push_back(entry_id);
  ++entry_count_;
}

std::vector<uint32_t> Trie::lookup(std::string_view key) const {
  const TrieNode* node = root_.get();
  size_t pos = 0;

  while (pos < key.size()) {
    char32_t cp = normalize::decodeUtf8(key, pos);
    auto it = node->children.find(cp);
    if (it == node->children.end()) {
      return {};
    }
    node = it->second.get();
  }

  return node->entry_ids;
}

std::vector<std::pair<size_t, std::vector<uint32_t>>> Trie::prefixMatch(std::string_view text, size_t start_pos) const {
  std::vector<std::pair<size_t, std::vector<uint32_t>>> results;
  const TrieNode* node = root_.get();
  size_t pos = start_pos;
  size_t char_count = 0;

  while (pos < text.size()) {
    char32_t cp = normalize::decodeUtf8(text, pos);
    auto it = node->children.find(cp);
    if (it == node->children.end()) {
      break;
    }
    node = it->second.get();
    ++char_count;

    if (!node->entry_ids.empty()) {
      results.emplace_back(char_count, node->entry_ids);
    }
  }

  return results;
}

void Trie::clear() {
  root_ = std::make_unique<TrieNode>();
  entry_count_ = 0;
}

// CompactTrie implementation (simplified - uses regular Trie internally for now)
void CompactTrie::build(const Trie& trie) {
  // TODO: Implement actual compact serialization
  (void)trie;
  loaded_ = true;
}

bool CompactTrie::loadFromMemory(const char* data, size_t size) {
  data_.assign(data, data + size);
  loaded_ = true;
  return true;
}

std::vector<char> CompactTrie::serialize() const { return data_; }

std::vector<uint32_t> CompactTrie::lookup(std::string_view /*key*/) {
  // TODO: Implement
  return {};
}

std::vector<std::pair<size_t, std::vector<uint32_t>>> CompactTrie::prefixMatch(std::string_view /*text*/,
                                                                               size_t /*start_pos*/) {
  // TODO: Implement
  return {};
}

}  // namespace suzume::dictionary
