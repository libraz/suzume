#include "dictionary/trie.h"

#include <gtest/gtest.h>

namespace suzume {
namespace dictionary {
namespace {

TEST(TrieTest, InsertAndLookup) {
  Trie trie;
  trie.insert("hello", 1);
  trie.insert("world", 2);

  auto result = trie.lookup("hello");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], 1);

  result = trie.lookup("world");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], 2);
}

TEST(TrieTest, LookupNotFound) {
  Trie trie;
  trie.insert("hello", 1);

  auto result = trie.lookup("world");
  EXPECT_TRUE(result.empty());
}

TEST(TrieTest, InsertJapanese) {
  Trie trie;
  trie.insert("日本", 1);
  trie.insert("日本語", 2);

  auto result = trie.lookup("日本");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], 1);

  result = trie.lookup("日本語");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], 2);
}

TEST(TrieTest, PrefixMatch) {
  Trie trie;
  trie.insert("日", 1);
  trie.insert("日本", 2);
  trie.insert("日本語", 3);

  auto results = trie.prefixMatch("日本語話者");
  ASSERT_EQ(results.size(), 3);
  // Results should be (length, entry_ids) pairs
  EXPECT_EQ(results[0].first, 1);  // length 1: 日
  EXPECT_EQ(results[1].first, 2);  // length 2: 日本
  EXPECT_EQ(results[2].first, 3);  // length 3: 日本語
}

TEST(TrieTest, PrefixMatchFromPosition) {
  Trie trie;
  trie.insert("本", 1);
  trie.insert("本語", 2);

  // "日本語" starts at byte 0, "本" is at byte 3
  std::string text = "日本語";
  auto results = trie.prefixMatch(text, 3);  // Start from byte 3
  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].first, 1);  // length 1: 本
  EXPECT_EQ(results[1].first, 2);  // length 2: 本語
}

TEST(TrieTest, MultipleEntriesSameKey) {
  Trie trie;
  trie.insert("は", 1);
  trie.insert("は", 2);
  trie.insert("は", 3);

  auto result = trie.lookup("は");
  ASSERT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], 1);
  EXPECT_EQ(result[1], 2);
  EXPECT_EQ(result[2], 3);
}

TEST(TrieTest, Clear) {
  Trie trie;
  trie.insert("test", 1);
  EXPECT_EQ(trie.size(), 1);

  trie.clear();
  EXPECT_EQ(trie.size(), 0);
  EXPECT_TRUE(trie.lookup("test").empty());
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
