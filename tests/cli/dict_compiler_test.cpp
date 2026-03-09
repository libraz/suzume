#include <gtest/gtest.h>

#include <string_view>

// Include the header directly since we add the CLI source dir to includes
#include "dict_compiler.h"

namespace suzume::cli {
namespace {

// Pure kanji 3+ chars: trivial (can be segmented by rules)
TEST(IsTrivialEntryTest, PureKanjiThreeChars) {
  EXPECT_TRUE(isTrivialEntry("経済成長"));
  EXPECT_TRUE(isTrivialEntry("東京都"));
  EXPECT_TRUE(isTrivialEntry("形態素解析"));
}

// Pure katakana 3+ chars: trivial
TEST(IsTrivialEntryTest, PureKatakanaThreeChars) {
  EXPECT_TRUE(isTrivialEntry("テスト"));
  EXPECT_TRUE(isTrivialEntry("コンピュータ"));
  EXPECT_TRUE(isTrivialEntry("プログラム"));
}

// Pure hiragana 3+ chars: trivial
TEST(IsTrivialEntryTest, PureHiraganaThreeChars) {
  EXPECT_TRUE(isTrivialEntry("ありがとう"));
  EXPECT_TRUE(isTrivialEntry("こんにちは"));
}

// 2-char entries: always non-trivial (kept)
TEST(IsTrivialEntryTest, TwoCharEntries) {
  EXPECT_FALSE(isTrivialEntry("東京"));
  EXPECT_FALSE(isTrivialEntry("りん"));
  EXPECT_FALSE(isTrivialEntry("AB"));
}

// 1-char entries: non-trivial (kept)
TEST(IsTrivialEntryTest, SingleCharEntries) {
  EXPECT_FALSE(isTrivialEntry("東"));
  EXPECT_FALSE(isTrivialEntry("A"));
}

// Mixed kanji+katakana: non-trivial (kept)
TEST(IsTrivialEntryTest, MixedKanjiKatakana) {
  EXPECT_FALSE(isTrivialEntry("二次エロ"));
  EXPECT_FALSE(isTrivialEntry("東京タワー"));
}

// Mixed kanji+hiragana: non-trivial (kept)
TEST(IsTrivialEntryTest, MixedKanjiHiragana) {
  EXPECT_FALSE(isTrivialEntry("掘り出し物"));
  EXPECT_FALSE(isTrivialEntry("食べ物"));
}

// Entries with spaces: always non-trivial (kept)
TEST(IsTrivialEntryTest, EntriesWithSpaces) {
  EXPECT_FALSE(isTrivialEntry("東京 都"));
  EXPECT_FALSE(isTrivialEntry("テスト ケース"));
}

// Empty string: non-trivial (0 codepoints <= 2)
TEST(IsTrivialEntryTest, EmptyString) {
  EXPECT_FALSE(isTrivialEntry(""));
}

// Pure alphabet 3+ chars: trivial
TEST(IsTrivialEntryTest, PureAlphabetThreeChars) {
  EXPECT_TRUE(isTrivialEntry("ABC"));
  EXPECT_TRUE(isTrivialEntry("test"));
}

// Mixed alphabet+digit: non-trivial (kept)
TEST(IsTrivialEntryTest, MixedAlphabetDigit) {
  EXPECT_FALSE(isTrivialEntry("part3"));
  EXPECT_FALSE(isTrivialEntry("ABC123"));
}

}  // namespace
}  // namespace suzume::cli
