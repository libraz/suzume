#ifndef SUZUME_DICTIONARY_TRIE_H_
#define SUZUME_DICTIONARY_TRIE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace suzume::dictionary {

/**
 * @brief Trie node for prefix matching
 */
class TrieNode {
 public:
  TrieNode() = default;
  ~TrieNode() = default;

  // Non-copyable, movable
  TrieNode(const TrieNode&) = delete;
  TrieNode& operator=(const TrieNode&) = delete;
  TrieNode(TrieNode&&) = default;
  TrieNode& operator=(TrieNode&&) = default;

  // Entry IDs at this node (for exact matches)
  std::vector<uint32_t> entry_ids;

  // Children nodes
  std::unordered_map<char32_t, std::unique_ptr<TrieNode>> children;
};

/**
 * @brief Trie for dictionary lookup
 */
class Trie {
 public:
  Trie();
  ~Trie() = default;

  // Non-copyable, movable
  Trie(const Trie&) = delete;
  Trie& operator=(const Trie&) = delete;
  Trie(Trie&&) = default;
  Trie& operator=(Trie&&) = default;

  /**
   * @brief Insert a string with entry ID
   * @param key String key
   * @param entry_id Entry ID
   */
  void insert(std::string_view key, uint32_t entry_id);

  /**
   * @brief Exact match lookup
   * @param key String key
   * @return Entry IDs, or empty vector if not found
   */
  std::vector<uint32_t> lookup(std::string_view key) const;

  /**
   * @brief Prefix match lookup (all prefixes of key)
   * @param text Text to search
   * @param start_pos Start position in text
   * @return Vector of (length, entry_ids) pairs
   */
  std::vector<std::pair<size_t, std::vector<uint32_t>>> prefixMatch(std::string_view text, size_t start_pos = 0) const;

  /**
   * @brief Get number of entries
   */
  size_t size() const { return entry_count_; }

  /**
   * @brief Clear the trie
   */
  void clear();

 private:
  std::unique_ptr<TrieNode> root_;
  size_t entry_count_{0};
};

/**
 * @brief Compact read-only trie for memory efficiency
 *
 * Serialized format for reduced memory usage.
 */
class CompactTrie {
 public:
  CompactTrie() = default;
  ~CompactTrie() = default;

  // Non-copyable, movable
  CompactTrie(const CompactTrie&) = delete;
  CompactTrie& operator=(const CompactTrie&) = delete;
  CompactTrie(CompactTrie&&) = default;
  CompactTrie& operator=(CompactTrie&&) = default;

  /**
   * @brief Build from regular Trie
   */
  void build(const Trie& trie);

  /**
   * @brief Load from memory
   * @param data Pointer to data
   * @param size Data size
   * @return true on success
   */
  bool loadFromMemory(const char* data, size_t size);

  /**
   * @brief Serialize to bytes
   * @return Serialized data
   */
  std::vector<char> serialize() const;

  /**
   * @brief Exact match lookup
   */
  static std::vector<uint32_t> lookup(std::string_view key);

  /**
   * @brief Prefix match lookup
   */
  static std::vector<std::pair<size_t, std::vector<uint32_t>>> prefixMatch(std::string_view text, size_t start_pos = 0);

 private:
  std::vector<char> data_;
  bool loaded_ = false;
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_TRIE_H_
