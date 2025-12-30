#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::test {
namespace {

class UnicodeEmojiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create analyzer with preserve_symbols = true
    SuzumeOptions opts;
    opts.remove_symbols = false;  // preserve symbols
    analyzer_with_symbols_ = std::make_unique<Suzume>(opts);

    // Default analyzer (symbols removed)
    analyzer_default_ = std::make_unique<Suzume>();
  }

  std::unique_ptr<Suzume> analyzer_with_symbols_;
  std::unique_ptr<Suzume> analyzer_default_;
};

// Test: Basic emoticon emoji (U+1F600-1F64F)
TEST_F(UnicodeEmojiTest, BasicEmoticon) {
  auto result = analyzer_with_symbols_->analyze("こんにちは😊");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "こんにちは");
  EXPECT_EQ(result[1].surface, "😊");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Supplemental Symbols (U+1F900-1F9FF) - 🥳🤔🤗
TEST_F(UnicodeEmojiTest, SupplementalSymbols) {
  auto result = analyzer_with_symbols_->analyze("テスト🥳");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "テスト");
  EXPECT_EQ(result[1].surface, "🥳");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Extended-A (U+1FA70-1FAFF) - 🪐
TEST_F(UnicodeEmojiTest, ExtendedA) {
  auto result = analyzer_with_symbols_->analyze("宇宙🪐");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "宇宙");
  EXPECT_EQ(result[1].surface, "🪐");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Variation selector - ❤️
TEST_F(UnicodeEmojiTest, VariationSelector) {
  auto result = analyzer_with_symbols_->analyze("愛❤️");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "愛");
  EXPECT_EQ(result[1].surface, "❤️");  // Heart with variation selector
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: ZWJ family emoji - 👨‍👩‍👧‍👦
TEST_F(UnicodeEmojiTest, ZwjFamily) {
  auto result = analyzer_with_symbols_->analyze("家族👨‍👩‍👧‍👦");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "家族");
  EXPECT_EQ(result[1].surface, "👨‍👩‍👧‍👦");  // ZWJ family emoji as single token
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Skin tone modifier - 👍🏻
TEST_F(UnicodeEmojiTest, SkinToneModifier) {
  auto result = analyzer_with_symbols_->analyze("良い👍🏻");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "良い");
  EXPECT_EQ(result[1].surface, "👍🏻");  // Thumbs up with skin tone
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Regional indicator flag - 🇯🇵
TEST_F(UnicodeEmojiTest, RegionalIndicatorFlag) {
  auto result = analyzer_with_symbols_->analyze("日本🇯🇵");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "日本");
  EXPECT_EQ(result[1].surface, "🇯🇵");  // Japan flag
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Consecutive emojis grouped together
TEST_F(UnicodeEmojiTest, ConsecutiveEmojis) {
  auto result = analyzer_with_symbols_->analyze("楽しい😊🎉");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "楽しい");
  EXPECT_EQ(result[1].surface, "😊🎉");  // Grouped together
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Default behavior removes symbols
TEST_F(UnicodeEmojiTest, DefaultRemovesSymbols) {
  auto result = analyzer_default_->analyze("こんにちは😊");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "こんにちは");
}

// Test: Misc symbols - ☀️
TEST_F(UnicodeEmojiTest, MiscSymbols) {
  auto result = analyzer_with_symbols_->analyze("天気☀️");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "天気");
  EXPECT_EQ(result[1].surface, "☀️");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Misc technical symbols - ⌚⌛
TEST_F(UnicodeEmojiTest, MiscTechnical) {
  auto result = analyzer_with_symbols_->analyze("時計⌚");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "時計");
  EXPECT_EQ(result[1].surface, "⌚");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Stars and circles - ⭐
TEST_F(UnicodeEmojiTest, StarsAndCircles) {
  auto result = analyzer_with_symbols_->analyze("星⭐");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "星");
  EXPECT_EQ(result[1].surface, "⭐");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Symbol);
}

// Test: Multiple skin tones grouped
TEST_F(UnicodeEmojiTest, MultipleSkinTones) {
  auto result = analyzer_with_symbols_->analyze("👍🏻👍🏿");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "👍🏻👍🏿");  // All grouped as consecutive emojis
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Symbol);
}

}  // namespace
}  // namespace suzume::test
