#include "dictionary/trie.h"

#include <gtest/gtest.h>

namespace suzume {
namespace dictionary {
namespace {

TEST(TrieTest, LookupViewReturnsOwnedEntryIdsWithoutCopying) {
  Trie trie;
  trie.insert("テスト", 3);
  trie.insert("テスト", 7);

  const auto* result = trie.lookupView("テスト");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->size(), 2);
  EXPECT_EQ((*result)[0], 3);
  EXPECT_EQ((*result)[1], 7);
  EXPECT_EQ(trie.lookupView("テス"), nullptr);
  EXPECT_EQ(trie.lookupView(""), nullptr);
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

TEST(TrieTest, DestroysADepthFirstChainWithoutUsingTheCallStack) {
  {
    Trie trie;
    const std::string key(30000, 'a');
    trie.insert(key, 1);
    EXPECT_NE(trie.lookupView(key), nullptr);
  }
  SUCCEED();
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
