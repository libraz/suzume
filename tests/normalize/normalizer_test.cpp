#include "normalize/normalizer.h"

#include <gtest/gtest.h>

#include "core/error.h"

namespace suzume::normalize {
namespace {

class NormalizerTest : public ::testing::Test {
 protected:
  Normalizer normalizer_;
};

// Full-width to half-width conversion tests
TEST_F(NormalizerTest, FullwidthDigitsToHalfwidth) {
  auto result = normalizer_.normalize("０１２３４５６７８９");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "0123456789");
}

TEST_F(NormalizerTest, FullwidthUppercaseToHalfwidthLowercase) {
  auto result = normalizer_.normalize("ＡＢＣＤＥＦ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "abcdef");
}

TEST_F(NormalizerTest, FullwidthLowercaseToHalfwidth) {
  auto result = normalizer_.normalize("ａｂｃｄｅｆ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "abcdef");
}

TEST_F(NormalizerTest, HalfwidthUppercaseToLowercase) {
  auto result = normalizer_.normalize("HELLO");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "hello");
}

// Vu-series normalization tests
TEST_F(NormalizerTest, VuSeries_VaToBA) {
  auto result = normalizer_.normalize("ヴァイオリン");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "バイオリン");
}

TEST_F(NormalizerTest, VuSeries_ViToBi) {
  auto result = normalizer_.normalize("ヴィオラ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ビオラ");
}

TEST_F(NormalizerTest, VuSeries_VuToBu) {
  auto result = normalizer_.normalize("ヴ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ブ");
}

TEST_F(NormalizerTest, VuSeries_VeToBe) {
  auto result = normalizer_.normalize("ヴェルディ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ベルディ");
}

TEST_F(NormalizerTest, VuSeries_VoToBo) {
  auto result = normalizer_.normalize("ヴォルテール");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ボルテール");
}

TEST_F(NormalizerTest, VuSeries_AloneVuToBu) {
  // When ヴ is not followed by small vowel, it becomes ブ
  auto result = normalizer_.normalize("ヴルスト");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ブルスト");
}

TEST_F(NormalizerTest, VuSeries_MixedText) {
  auto result = normalizer_.normalize("ヴァイオリンとピアノ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "バイオリンとピアノ");
}

// needsNormalization tests
TEST_F(NormalizerTest, NeedsNormalization_FullwidthDigit) {
  EXPECT_TRUE(normalizer_.needsNormalization("１２３"));
}

TEST_F(NormalizerTest, NeedsNormalization_FullwidthAlpha) {
  EXPECT_TRUE(normalizer_.needsNormalization("ＡＢＣ"));
}

TEST_F(NormalizerTest, NeedsNormalization_Vu) {
  EXPECT_TRUE(normalizer_.needsNormalization("ヴァイオリン"));
}

TEST_F(NormalizerTest, NeedsNormalization_NoChange) {
  EXPECT_FALSE(normalizer_.needsNormalization("こんにちは"));
}

TEST_F(NormalizerTest, NeedsNormalization_Mixed) {
  EXPECT_TRUE(normalizer_.needsNormalization("hello world ０"));
}

// Empty and special cases
TEST_F(NormalizerTest, EmptyString) {
  auto result = normalizer_.normalize("");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "");
}

TEST_F(NormalizerTest, JapaneseTextUnchanged) {
  auto result = normalizer_.normalize("日本語のテスト");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "日本語のテスト");
}

// ===== Half-width Katakana Tests (半角カタカナ) =====

TEST_F(NormalizerTest, HalfwidthKatakana_BasicConversion) {
  // ｱｲｳｴｵ → アイウエオ
  auto result = normalizer_.normalize("ｱｲｳｴｵ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "アイウエオ");
}

TEST_F(NormalizerTest, HalfwidthKatakana_WithDakuten) {
  // ｶﾞｷﾞｸﾞｹﾞｺﾞ → ガギグゲゴ
  auto result = normalizer_.normalize("ｶﾞｷﾞｸﾞｹﾞｺﾞ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ガギグゲゴ");
}

TEST_F(NormalizerTest, HalfwidthKatakana_WithHandakuten) {
  // ﾊﾟﾋﾟﾌﾟﾍﾟﾎﾟ → パピプペポ
  auto result = normalizer_.normalize("ﾊﾟﾋﾟﾌﾟﾍﾟﾎﾟ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "パピプペポ");
}

TEST_F(NormalizerTest, HalfwidthKatakana_MixedText) {
  // ｺﾝﾋﾟｭｰﾀｰを使う → コンピューターを使う
  auto result = normalizer_.normalize("ｺﾝﾋﾟｭｰﾀｰを使う");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "コンピューターを使う");
}

TEST_F(NormalizerTest, HalfwidthKatakana_SmallCharacters) {
  // ｧｨｩｪｫ → ァィゥェォ
  auto result = normalizer_.normalize("ｧｨｩｪｫ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ァィゥェォ");
}

TEST_F(NormalizerTest, HalfwidthKatakana_LongVowel) {
  // ｰ → ー
  auto result = normalizer_.normalize("ｺｰﾋｰ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "コーヒー");
}

// ===== Long Vowel Mark Tests (長音記号) =====

TEST_F(NormalizerTest, LongVowel_FullwidthTilde) {
  // ～ handling
  auto result = normalizer_.normalize("ラーメン～");
  ASSERT_TRUE(core::isSuccess(result));
  // Should preserve or convert consistently
}

TEST_F(NormalizerTest, LongVowel_WaveDash) {
  // 〜 (wave dash U+301C)
  auto result = normalizer_.normalize("東京〜大阪");
  ASSERT_TRUE(core::isSuccess(result));
}

// ===== Iteration Mark Tests (繰り返し記号) =====

TEST_F(NormalizerTest, IterationMark_Kanji) {
  // 々 iteration mark for kanji
  auto result = normalizer_.normalize("人々");
  ASSERT_TRUE(core::isSuccess(result));
  // Should preserve 々 as-is (it's a valid character)
  EXPECT_EQ(std::get<std::string>(result), "人々");
}

TEST_F(NormalizerTest, IterationMark_Hiragana) {
  // ゝ iteration mark for hiragana
  auto result = normalizer_.normalize("あゝ");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, IterationMark_HiraganaVoiced) {
  // ゞ voiced iteration mark for hiragana
  auto result = normalizer_.normalize("みすゞ");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, IterationMark_Katakana) {
  // ヽ iteration mark for katakana
  auto result = normalizer_.normalize("アヽ");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, IterationMark_KatakanaVoiced) {
  // ヾ voiced iteration mark for katakana
  auto result = normalizer_.normalize("カヾ");
  ASSERT_TRUE(core::isSuccess(result));
}

// ===== Punctuation Normalization Tests =====

TEST_F(NormalizerTest, Punctuation_FullwidthComma) {
  auto result = normalizer_.normalize("東京、大阪");
  ASSERT_TRUE(core::isSuccess(result));
  // Should preserve Japanese comma
  EXPECT_EQ(std::get<std::string>(result), "東京、大阪");
}

TEST_F(NormalizerTest, Punctuation_FullwidthPeriod) {
  auto result = normalizer_.normalize("終わり。");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "終わり。");
}

TEST_F(NormalizerTest, Punctuation_FullwidthQuestionMark) {
  auto result = normalizer_.normalize("本当？");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, Punctuation_FullwidthExclamation) {
  auto result = normalizer_.normalize("すごい！");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, Punctuation_MixedMarks) {
  auto result = normalizer_.normalize("えっ！？");
  ASSERT_TRUE(core::isSuccess(result));
}

// ===== Bracket Normalization Tests =====

TEST_F(NormalizerTest, Bracket_JapaneseQuotes) {
  // 「」 Japanese quotation marks
  auto result = normalizer_.normalize("「こんにちは」");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "「こんにちは」");
}

TEST_F(NormalizerTest, Bracket_DoubleQuotes) {
  // 『』 Double Japanese quotation marks
  auto result = normalizer_.normalize("『本のタイトル』");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, Bracket_Parentheses) {
  // （） Full-width parentheses
  auto result = normalizer_.normalize("（注）");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, Bracket_CornerBrackets) {
  // 【】 Lenticular brackets
  auto result = normalizer_.normalize("【重要】");
  ASSERT_TRUE(core::isSuccess(result));
}

// ===== Symbol Normalization Tests =====

TEST_F(NormalizerTest, Symbol_JapaneseYen) {
  // ￥ Full-width yen sign
  auto result = normalizer_.normalize("￥1000");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, Symbol_FullwidthColon) {
  auto result = normalizer_.normalize("時間：10分");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, Symbol_MiddleDot) {
  // ・ Middle dot (katakana middle dot)
  auto result = normalizer_.normalize("東京・大阪");
  ASSERT_TRUE(core::isSuccess(result));
}

// ===== Kana Variation Tests =====

TEST_F(NormalizerTest, KanaVariation_SmallKana) {
  // Small kana ぁぃぅぇぉ should be preserved
  auto result = normalizer_.normalize("ふぁいる");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ふぁいる");
}

TEST_F(NormalizerTest, KanaVariation_SmallTsu) {
  // っ small tsu
  auto result = normalizer_.normalize("がっこう");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "がっこう");
}

TEST_F(NormalizerTest, KanaVariation_SmallYaYuYo) {
  // ゃゅょ small ya, yu, yo
  auto result = normalizer_.normalize("きょうと");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "きょうと");
}

TEST_F(NormalizerTest, KanaVariation_KatakanaSmallKana) {
  // Small katakana
  auto result = normalizer_.normalize("ファイル");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ファイル");
}

// ===== Combining Character Tests =====

TEST_F(NormalizerTest, CombiningChar_DakutenSeparate) {
  // が as か + combining dakuten (U+3099)
  // This tests NFC normalization
  auto result = normalizer_.normalize("か\u3099");
  ASSERT_TRUE(core::isSuccess(result));
  // Should normalize to single character が
}

TEST_F(NormalizerTest, CombiningChar_HandakutenSeparate) {
  // ぱ as は + combining handakuten (U+309A)
  auto result = normalizer_.normalize("は\u309A");
  ASSERT_TRUE(core::isSuccess(result));
  // Should normalize to single character ぱ
}

// ===== Mixed Script Normalization Tests =====

TEST_F(NormalizerTest, MixedScript_AlphaNumericJapanese) {
  auto result = normalizer_.normalize("ABC123日本語");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "abc123日本語");
}

TEST_F(NormalizerTest, MixedScript_FullwidthAlphaJapanese) {
  auto result = normalizer_.normalize("ＡＢＣ日本語");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "abc日本語");
}

// ===== Whitespace Normalization Tests =====

TEST_F(NormalizerTest, Whitespace_FullwidthSpace) {
  // Full-width space U+3000
  auto result = normalizer_.normalize("東京　大阪");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, Whitespace_MultipleSpaces) {
  auto result = normalizer_.normalize("東京  大阪");
  ASSERT_TRUE(core::isSuccess(result));
}

// ===== needsNormalization Additional Tests =====

TEST_F(NormalizerTest, NeedsNormalization_HalfwidthKatakana) {
  EXPECT_TRUE(normalizer_.needsNormalization("ｱｲｳ"));
}

TEST_F(NormalizerTest, NeedsNormalization_UppercaseAlpha) {
  EXPECT_TRUE(normalizer_.needsNormalization("ABC"));
}

TEST_F(NormalizerTest, NeedsNormalization_PureHiragana) {
  EXPECT_FALSE(normalizer_.needsNormalization("ひらがな"));
}

TEST_F(NormalizerTest, NeedsNormalization_PureKatakana) {
  EXPECT_FALSE(normalizer_.needsNormalization("カタカナ"));
}

TEST_F(NormalizerTest, NeedsNormalization_PureKanji) {
  EXPECT_FALSE(normalizer_.needsNormalization("漢字"));
}

// ===== Extended Character Tests =====

TEST_F(NormalizerTest, ExtendedChar_RareKanji) {
  // Rare kanji that might be in extension areas
  auto result = normalizer_.normalize("龍");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "龍");
}

TEST_F(NormalizerTest, ExtendedChar_CircledNumbers) {
  // ① ② ③ Circled numbers
  auto result = normalizer_.normalize("①②③");
  ASSERT_TRUE(core::isSuccess(result));
}

TEST_F(NormalizerTest, ExtendedChar_RomanNumerals) {
  // Ⅰ Ⅱ Ⅲ Roman numerals
  auto result = normalizer_.normalize("ⅠⅡⅢ");
  ASSERT_TRUE(core::isSuccess(result));
}

// ===== Normalization Options Tests =====

TEST(NormalizerOptionsTest, PreserveVu_Default) {
  // Default: ヴ→ブ conversion enabled
  Normalizer normalizer;
  auto result = normalizer.normalize("ヴィトン");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ビトン");
}

TEST(NormalizerOptionsTest, PreserveVu_Enabled) {
  // With preserve_vu: ヴ is kept as-is
  NormalizeOptions opts;
  opts.preserve_vu = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("ヴィトン");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ヴィトン");
}

TEST(NormalizerOptionsTest, PreserveVu_LouisVuitton) {
  NormalizeOptions opts;
  opts.preserve_vu = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("ルイ・ヴィトン");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ルイ・ヴィトン");
}

TEST(NormalizerOptionsTest, PreserveVu_MixedText) {
  NormalizeOptions opts;
  opts.preserve_vu = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("ヴァイオリンとピアノ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ヴァイオリンとピアノ");
}

TEST(NormalizerOptionsTest, PreserveCase_Default) {
  // Default: uppercase→lowercase conversion enabled
  Normalizer normalizer;
  auto result = normalizer.normalize("Hello World");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "hello world");
}

TEST(NormalizerOptionsTest, PreserveCase_Enabled) {
  // With preserve_case: case is kept as-is
  NormalizeOptions opts;
  opts.preserve_case = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("Hello World");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "Hello World");
}

TEST(NormalizerOptionsTest, PreserveCase_FullwidthAlpha) {
  NormalizeOptions opts;
  opts.preserve_case = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("ＡＢＣＤＥＦ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "ABCDEF");
}

TEST(NormalizerOptionsTest, PreserveCase_MixedJapaneseEnglish) {
  NormalizeOptions opts;
  opts.preserve_case = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("Tokyo Tower");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "Tokyo Tower");
}

TEST(NormalizerOptionsTest, BothOptionsEnabled) {
  NormalizeOptions opts;
  opts.preserve_vu = true;
  opts.preserve_case = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("LOUIS VUITTONのヴァッグ");
  ASSERT_TRUE(core::isSuccess(result));
  EXPECT_EQ(std::get<std::string>(result), "LOUIS VUITTONのヴァッグ");
}

TEST(NormalizerOptionsTest, SetOptionsAfterConstruction) {
  Normalizer normalizer;

  // Default behavior
  auto result1 = normalizer.normalize("Hello");
  ASSERT_TRUE(core::isSuccess(result1));
  EXPECT_EQ(std::get<std::string>(result1), "hello");

  // Change options
  NormalizeOptions opts;
  opts.preserve_case = true;
  normalizer.setOptions(opts);

  // New behavior
  auto result2 = normalizer.normalize("Hello");
  ASSERT_TRUE(core::isSuccess(result2));
  EXPECT_EQ(std::get<std::string>(result2), "Hello");
}

// ===== Error Handling Tests =====

TEST_F(NormalizerTest, ErrorHandling_InvalidUtf8) {
  // Invalid UTF-8 sequence - should handle gracefully
  std::string invalid_utf8 = "\xFF\xFE";
  auto result = normalizer_.normalize(invalid_utf8);
  // Should either return error or handle gracefully
}

TEST_F(NormalizerTest, ErrorHandling_IncompleteUtf8) {
  // Incomplete UTF-8 sequence
  std::string incomplete = "\xE3\x81";  // Incomplete hiragana
  auto result = normalizer_.normalize(incomplete);
  // Should handle gracefully
}

}  // namespace
}  // namespace suzume::normalize
