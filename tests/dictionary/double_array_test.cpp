#include "dictionary/double_array.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace suzume::dictionary {
namespace {

class DoubleArrayTest : public ::testing::Test {
 protected:
  DoubleArray trie_;
};

TEST_F(DoubleArrayTest, BuildEmpty) {
  std::vector<std::string> keys;
  std::vector<uint32_t> values;

  EXPECT_TRUE(trie_.build(keys, values));
  EXPECT_TRUE(trie_.empty());
}

TEST_F(DoubleArrayTest, BuildSingleKey) {
  std::vector<std::string> keys = {"hello"};
  std::vector<uint32_t> values = {42};

  EXPECT_TRUE(trie_.build(keys, values));
  EXPECT_FALSE(trie_.empty());

  EXPECT_EQ(trie_.exactMatch("hello"), 42);
  EXPECT_EQ(trie_.exactMatch("world"), -1);
  EXPECT_EQ(trie_.exactMatch("hell"), -1);
  EXPECT_EQ(trie_.exactMatch("hello!"), -1);
}

TEST_F(DoubleArrayTest, BuildMultipleKeys) {
  std::vector<std::string> keys = {"a", "ab", "abc", "b", "bc"};
  std::vector<uint32_t> values = {1, 2, 3, 4, 5};

  EXPECT_TRUE(trie_.build(keys, values));

  EXPECT_EQ(trie_.exactMatch("a"), 1);
  EXPECT_EQ(trie_.exactMatch("ab"), 2);
  EXPECT_EQ(trie_.exactMatch("abc"), 3);
  EXPECT_EQ(trie_.exactMatch("b"), 4);
  EXPECT_EQ(trie_.exactMatch("bc"), 5);
  EXPECT_EQ(trie_.exactMatch("c"), -1);
  EXPECT_EQ(trie_.exactMatch("abcd"), -1);
}

TEST_F(DoubleArrayTest, BuildUnsortedFails) {
  std::vector<std::string> keys = {"b", "a"};  // Not sorted
  std::vector<uint32_t> values = {1, 2};

  EXPECT_FALSE(trie_.build(keys, values));
}

TEST_F(DoubleArrayTest, BuildDuplicateFails) {
  std::vector<std::string> keys = {"a", "a"};  // Duplicate
  std::vector<uint32_t> values = {1, 2};

  EXPECT_FALSE(trie_.build(keys, values));
}

TEST_F(DoubleArrayTest, BuildMismatchedSizeFails) {
  std::vector<std::string> keys = {"a", "b"};
  std::vector<uint32_t> values = {1};  // Size mismatch

  EXPECT_FALSE(trie_.build(keys, values));
}

TEST_F(DoubleArrayTest, CommonPrefixSearchBasic) {
  std::vector<std::string> keys = {"a", "ab", "abc", "abcd"};
  std::vector<uint32_t> values = {1, 2, 3, 4};

  EXPECT_TRUE(trie_.build(keys, values));

  auto results = trie_.commonPrefixSearch("abcde");

  EXPECT_EQ(results.size(), 4u);
  EXPECT_EQ(results[0].value, 1);
  EXPECT_EQ(results[0].length, 1u);
  EXPECT_EQ(results[1].value, 2);
  EXPECT_EQ(results[1].length, 2u);
  EXPECT_EQ(results[2].value, 3);
  EXPECT_EQ(results[2].length, 3u);
  EXPECT_EQ(results[3].value, 4);
  EXPECT_EQ(results[3].length, 4u);
}

TEST_F(DoubleArrayTest, CommonPrefixSearchWithStart) {
  std::vector<std::string> keys = {"a", "ab", "b", "bc"};
  std::vector<uint32_t> values = {1, 2, 3, 4};

  EXPECT_TRUE(trie_.build(keys, values));

  auto results = trie_.commonPrefixSearch("xbc", 1);

  EXPECT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].value, 3);  // "b"
  EXPECT_EQ(results[0].length, 1u);
  EXPECT_EQ(results[1].value, 4);  // "bc"
  EXPECT_EQ(results[1].length, 2u);
}

TEST_F(DoubleArrayTest, CommonPrefixSearchMaxResults) {
  std::vector<std::string> keys = {"a", "ab", "abc", "abcd"};
  std::vector<uint32_t> values = {1, 2, 3, 4};

  EXPECT_TRUE(trie_.build(keys, values));

  auto results = trie_.commonPrefixSearch("abcde", 0, 2);

  EXPECT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].value, 1);
  EXPECT_EQ(results[1].value, 2);
}

TEST_F(DoubleArrayTest, CommonPrefixSearchNoMatch) {
  std::vector<std::string> keys = {"a", "ab"};
  std::vector<uint32_t> values = {1, 2};

  EXPECT_TRUE(trie_.build(keys, values));

  auto results = trie_.commonPrefixSearch("xyz");
  EXPECT_TRUE(results.empty());
}

TEST_F(DoubleArrayTest, JapaneseText) {
  std::vector<std::string> keys = {
      "あ",        // Hiragana A
      "あい",      // Hiragana AI
      "東",        // Kanji East
      "東京",      // Tokyo
      "東京都",    // Tokyo Metropolis
  };
  std::vector<uint32_t> values = {1, 2, 3, 4, 5};

  // Sort keys (UTF-8 byte order)
  std::vector<std::pair<std::string, uint32_t>> pairs;
  for (size_t idx = 0; idx < keys.size(); ++idx) {
    pairs.emplace_back(keys[idx], values[idx]);
  }
  std::sort(pairs.begin(), pairs.end());

  std::vector<std::string> sorted_keys;
  std::vector<uint32_t> sorted_values;
  for (const auto& kv_pair : pairs) {
    sorted_keys.push_back(kv_pair.first);
    sorted_values.push_back(kv_pair.second);
  }

  EXPECT_TRUE(trie_.build(sorted_keys, sorted_values));

  // Test exact match
  for (size_t idx = 0; idx < sorted_keys.size(); ++idx) {
    EXPECT_EQ(trie_.exactMatch(sorted_keys[idx]),
              static_cast<int32_t>(sorted_values[idx]));
  }

  // Test common prefix search
  auto results = trie_.commonPrefixSearch("東京都庁");
  EXPECT_GE(results.size(), 3u);  // Should match "東", "東京", "東京都"
}

TEST_F(DoubleArrayTest, SerializeDeserialize) {
  std::vector<std::string> keys = {"a", "ab", "abc", "b", "bc"};
  std::vector<uint32_t> values = {10, 20, 30, 40, 50};

  EXPECT_TRUE(trie_.build(keys, values));

  // Serialize
  auto data = trie_.serialize();
  EXPECT_GT(data.size(), 8u);

  // Create new trie and deserialize
  DoubleArray trie2;
  EXPECT_TRUE(trie2.deserialize(data.data(), data.size()));

  // Verify same behavior
  EXPECT_EQ(trie2.exactMatch("a"), 10);
  EXPECT_EQ(trie2.exactMatch("ab"), 20);
  EXPECT_EQ(trie2.exactMatch("abc"), 30);
  EXPECT_EQ(trie2.exactMatch("b"), 40);
  EXPECT_EQ(trie2.exactMatch("bc"), 50);
  EXPECT_EQ(trie2.exactMatch("c"), -1);
}

TEST_F(DoubleArrayTest, DeserializeInvalidData) {
  DoubleArray trie2;

  // Too short
  std::vector<uint8_t> short_data = {'D', 'A', '0', '1'};
  EXPECT_FALSE(trie2.deserialize(short_data.data(), short_data.size()));

  // Wrong magic number
  std::vector<uint8_t> bad_magic = {'X', 'X', 'X', 'X', 0, 0, 0, 0};
  EXPECT_FALSE(trie2.deserialize(bad_magic.data(), bad_magic.size()));
}

TEST_F(DoubleArrayTest, Clear) {
  std::vector<std::string> keys = {"a", "b"};
  std::vector<uint32_t> values = {1, 2};

  EXPECT_TRUE(trie_.build(keys, values));
  EXPECT_FALSE(trie_.empty());

  trie_.clear();
  EXPECT_TRUE(trie_.empty());
  EXPECT_EQ(trie_.exactMatch("a"), -1);
}

TEST_F(DoubleArrayTest, MemoryUsage) {
  std::vector<std::string> keys = {"a", "b", "c"};
  std::vector<uint32_t> values = {1, 2, 3};

  EXPECT_TRUE(trie_.build(keys, values));

  size_t usage = trie_.memoryUsage();
  EXPECT_GT(usage, 0u);
  // Memory usage should be related to the number of nodes
  // Each node uses 2 * sizeof(int32_t) = 8 bytes
  EXPECT_EQ(usage % 8, 0u);
}

}  // namespace
}  // namespace suzume::dictionary
