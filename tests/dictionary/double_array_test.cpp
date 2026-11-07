#include "dictionary/double_array.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
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
  EXPECT_EQ(trie_.exactMatch("a"), -1);
  EXPECT_TRUE(trie_.commonPrefixSearch("a").empty());
}

TEST_F(DoubleArrayTest, BuildSingleKey) {
  std::vector<std::string> keys = {"hello"};
  std::vector<uint32_t> values = {42};

  EXPECT_TRUE(trie_.build(keys, values));

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

TEST_F(DoubleArrayTest, BuildRejectsValueOutsideSignedResultRange) {
  std::vector<std::string> keys = {"a"};
  std::vector<uint32_t> values = {static_cast<uint32_t>(INT32_MAX) + 1u};

  EXPECT_FALSE(trie_.build(keys, values));
  EXPECT_EQ(trie_.exactMatch("a"), -1);
}

TEST_F(DoubleArrayTest, PackedValueBoundary) {
  std::vector<std::string> keys = {"a"};
  EXPECT_TRUE(trie_.build(keys, std::vector<uint32_t>{0x007FFFFFU}));
  EXPECT_EQ(trie_.exactMatch("a"), 0x007FFFFF);

  EXPECT_FALSE(trie_.build(keys, std::vector<uint32_t>{0x00800000U}));
  EXPECT_EQ(trie_.exactMatch("a"), 0x007FFFFF);
}

TEST_F(DoubleArrayTest, BuildRejectsEmptyAndNulKeysWithoutReplacingTrie) {
  ASSERT_TRUE(trie_.build(std::vector<std::string>{"valid"}));

  EXPECT_FALSE(trie_.build(std::vector<std::string>{""}));
  EXPECT_FALSE(trie_.build(std::vector<std::string>{std::string("a\0b", 3)}));
  EXPECT_EQ(trie_.exactMatch("valid"), 0);
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

TEST_F(DoubleArrayTest, CommonPrefixSearchNoMatch) {
  std::vector<std::string> keys = {"a", "ab"};
  std::vector<uint32_t> values = {1, 2};

  EXPECT_TRUE(trie_.build(keys, values));

  auto results = trie_.commonPrefixSearch("xyz");
  EXPECT_TRUE(results.empty());
}

TEST_F(DoubleArrayTest, JapaneseText) {
  std::vector<std::string> keys = {
      "あ",      // Hiragana A
      "あい",    // Hiragana AI
      "東",      // Kanji East
      "東京",    // Tokyo
      "東京都",  // Tokyo Metropolis
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
    EXPECT_EQ(trie_.exactMatch(sorted_keys[idx]), static_cast<int32_t>(sorted_values[idx]));
  }

  // Test common prefix search
  auto results = trie_.commonPrefixSearch("東京都庁");
  EXPECT_GE(results.size(), 3u);  // Should match "東", "東京", "東京都"
}

// Regression: a byte that is not a child of the root must never be accepted as a
// transition. A zero-initialized cell has label zero, so all nonzero bytes must
// be backed by a cell carrying that exact incoming label.
TEST_F(DoubleArrayTest, RootTransitionRejectsEmptyCells) {
  // Sparse first bytes force wide bases and interior empty cells.
  std::vector<std::string> keys = {std::string("\x01z", 2),
                                   std::string("\x7f"
                                               "z",
                                               2),
                                   std::string("\xf0"
                                               "z",
                                               2)};
  std::vector<uint32_t> values = {1, 2, 3};
  ASSERT_TRUE(trie_.build(keys, values));

  const auto is_root_child = [](int byte) { return byte == 0x01 || byte == 0x7f || byte == 0xf0; };
  for (int byte = 0; byte < 256; ++byte) {
    std::string one(1, static_cast<char>(byte));
    EXPECT_EQ(trie_.exactMatch(one), -1) << "single byte=" << byte;
    if (!is_root_child(byte)) {
      // A two-byte key whose first byte is not a root child must miss outright.
      EXPECT_TRUE(trie_.commonPrefixSearch(one + "z").empty()) << "prefix byte=" << byte;
    }
  }

  // Genuine members still resolve.
  EXPECT_EQ(trie_.exactMatch(std::string("\x01z", 2)), 1);
  EXPECT_EQ(trie_.exactMatch(std::string("\x7f"
                                         "z",
                                         2)),
            2);
  EXPECT_EQ(trie_.exactMatch(std::string("\xf0"
                                         "z",
                                         2)),
            3);
}

// Regression: label-only transition validation requires every parent base to
// be unique. Reusing a base let a missing edge borrow the same-label child of a
// different parent and falsely return a registered value.
TEST_F(DoubleArrayTest, MissingDeepTransitionDoesNotMatchAnotherParent) {
  const std::vector<std::string> keys = {"aaaa", "aabc", "aac", "accb", "babc", "bacb", "bacc", "bbb",
                                         "bcaa", "bcbc", "ca",  "cac",  "cbca", "cbcb", "cccc"};
  ASSERT_TRUE(trie_.build(keys));

  EXPECT_EQ(trie_.exactMatch("acb"), -1);
  EXPECT_TRUE(trie_.commonPrefixSearch("acb").empty());
  for (size_t idx = 0; idx < keys.size(); ++idx) {
    EXPECT_EQ(trie_.exactMatch(keys[idx]), static_cast<int32_t>(idx));
  }
}

}  // namespace
}  // namespace suzume::dictionary
