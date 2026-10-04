#include "trie.h"

#include "normalize/utf8.h"

namespace suzume::dictionary {

TrieNode::~TrieNode() {
  // A user dictionary can contain an arbitrary-depth single-key chain.  Detach
  // every child before destroying each node so unique_ptr never recurses down
  // that chain on the host or the smaller WebAssembly stack.
  std::vector<TrieNode*> pending;
  pending.reserve(children.size());
  for (auto& [codepoint, child] : children) {
    (void)codepoint;
    pending.push_back(child.release());
  }
  children.clear();

  while (!pending.empty()) {
    std::unique_ptr<TrieNode> node(pending.back());
    pending.pop_back();
    for (auto& [codepoint, child] : node->children) {
      (void)codepoint;
      pending.push_back(child.release());
    }
    node->children.clear();
  }
}

Trie::Trie() : root_(std::make_unique<TrieNode>()) {}

void Trie::insert(std::string_view key, uint32_t entry_id) {
  TrieNode* node = root_.get();
  size_t pos = 0;

  while (pos < key.size()) {
    auto& child = node->children[normalize::decodeUtf8(key, pos)];
    if (!child) {
      child = std::make_unique<TrieNode>();
    }
    node = child.get();
  }

  node->entry_ids.push_back(entry_id);
  ++entry_count_;
}

const std::vector<uint32_t>* Trie::lookupView(std::string_view key) const {
  const TrieNode* node = root_.get();
  size_t pos = 0;

  while (pos < key.size()) {
    const auto child = node->children.find(normalize::decodeUtf8(key, pos));
    if (child == node->children.end()) {
      return nullptr;
    }
    node = child->second.get();
  }

  return node->entry_ids.empty() ? nullptr : &node->entry_ids;
}

std::vector<std::pair<size_t, const std::vector<uint32_t>*>> Trie::prefixMatch(std::string_view text,
                                                                               size_t start_pos) const {
  std::vector<std::pair<size_t, const std::vector<uint32_t>*>> results;
  const TrieNode* node = root_.get();
  size_t pos = start_pos;
  size_t char_count = 0;

  while (pos < text.size()) {
    const auto child = node->children.find(normalize::decodeUtf8(text, pos));
    if (child == node->children.end()) {
      break;
    }
    node = child->second.get();
    ++char_count;

    if (!node->entry_ids.empty()) {
      results.emplace_back(char_count, &node->entry_ids);
    }
  }

  return results;
}

void Trie::clear() {
  root_ = std::make_unique<TrieNode>();
  entry_count_ = 0;
}

}  // namespace suzume::dictionary
