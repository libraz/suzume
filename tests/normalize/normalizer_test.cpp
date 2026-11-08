#include "normalize/normalizer.h"

#include <gtest/gtest.h>

#include "core/error.h"

namespace suzume::normalize {
namespace {

class NormalizerTest : public ::testing::Test {
 protected:
  // Use explicit normalization options to test conversion functionality
  NormalizerTest() {
    NormalizeOptions opts;
    opts.preserve_vu = false;    // Enable ヴ→ビ conversion for testing
    opts.preserve_case = false;  // Enable lowercase conversion for testing
    normalizer_ = Normalizer(opts);
  }
  Normalizer normalizer_;
};

// Full-width to half-width conversion tests
TEST_F(NormalizerTest, FullwidthDigitsToHalfwidth) {
  auto result = normalizer_.normalize("０１２３４５６７８９");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "0123456789");
}

TEST_F(NormalizerTest, FullwidthUppercaseToHalfwidthLowercase) {
  auto result = normalizer_.normalize("ＡＢＣＤＥＦ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "abcdef");
}

TEST_F(NormalizerTest, FullwidthLowercaseToHalfwidth) {
  auto result = normalizer_.normalize("ａｂｃｄｅｆ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "abcdef");
}

TEST_F(NormalizerTest, FullwidthAsciiPunctuationToHalfwidth) {
  auto result = normalizer_.normalize("１，２３４円 ｔｅｓｔ＠ｅｘａｍｐｌｅ．ｊｐ ｖ１．２．３");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "1,234円 test@example.jp v1.2.3");
}

TEST_F(NormalizerTest, TransparentFormatControlsAreRemoved) {
  auto result = normalizer_.normalize("テ\u200Bスト これ\u200Bは \u200Bテスト");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "テスト これは テスト");
}

TEST_F(NormalizerTest, HalfwidthUppercaseToLowercase) {
  auto result = normalizer_.normalize("HELLO");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "hello");
}

// Vu-series normalization tests
TEST_F(NormalizerTest, VuSeries_VaToBA) {
  auto result = normalizer_.normalize("ヴァイオリン");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "バイオリン");
}

TEST_F(NormalizerTest, VuSeries_ViToBi) {
  auto result = normalizer_.normalize("ヴィオラ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ビオラ");
}

TEST_F(NormalizerTest, VuSeries_VuToBu) {
  auto result = normalizer_.normalize("ヴ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ブ");
}

TEST_F(NormalizerTest, VuSeries_VeToBe) {
  auto result = normalizer_.normalize("ヴェルディ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ベルディ");
}

TEST_F(NormalizerTest, VuSeries_VoToBo) {
  auto result = normalizer_.normalize("ヴォルテール");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ボルテール");
}

TEST_F(NormalizerTest, VuSeries_AloneVuToBu) {
  // When ヴ is not followed by small vowel, it becomes ブ
  auto result = normalizer_.normalize("ヴルスト");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ブルスト");
}

TEST_F(NormalizerTest, VuSeries_MixedText) {
  auto result = normalizer_.normalize("ヴァイオリンとピアノ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "バイオリンとピアノ");
}

// Empty and special cases
TEST_F(NormalizerTest, EmptyString) {
  auto result = normalizer_.normalize("");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "");
}

TEST_F(NormalizerTest, JapaneseTextUnchanged) {
  auto result = normalizer_.normalize("日本語のテスト");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "日本語のテスト");
}

// ===== Half-width Katakana Tests (半角カタカナ) =====

TEST_F(NormalizerTest, HalfwidthKatakana_BasicConversion) {
  // ｱｲｳｴｵ → アイウエオ
  auto result = normalizer_.normalize("ｱｲｳｴｵ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "アイウエオ");
}

TEST_F(NormalizerTest, HalfwidthKatakana_WithDakuten) {
  // ｶﾞｷﾞｸﾞｹﾞｺﾞ → ガギグゲゴ
  auto result = normalizer_.normalize("ｶﾞｷﾞｸﾞｹﾞｺﾞ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ガギグゲゴ");
}

TEST_F(NormalizerTest, HalfwidthKatakana_WithHandakuten) {
  // ﾊﾟﾋﾟﾌﾟﾍﾟﾎﾟ → パピプペポ
  auto result = normalizer_.normalize("ﾊﾟﾋﾟﾌﾟﾍﾟﾎﾟ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "パピプペポ");
}

TEST_F(NormalizerTest, HalfwidthKatakana_MixedText) {
  // ｺﾝﾋﾟｭｰﾀｰを使う → コンピューターを使う
  auto result = normalizer_.normalize("ｺﾝﾋﾟｭｰﾀｰを使う");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "コンピューターを使う");
}

TEST_F(NormalizerTest, HalfwidthKatakana_SmallCharacters) {
  // ｧｨｩｪｫ → ァィゥェォ
  auto result = normalizer_.normalize("ｧｨｩｪｫ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ァィゥェォ");
}

TEST_F(NormalizerTest, HalfwidthKatakana_LongVowel) {
  // ｰ → ー
  auto result = normalizer_.normalize("ｺｰﾋｰ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "コーヒー");
}

TEST_F(NormalizerTest, CombiningSoundMarksNormalizeLikeSpacingMarks) {
  auto voiced = normalizer_.normalize("ア\xE3\x82\x99");
  auto semi_voiced = normalizer_.normalize("ハ\xE3\x82\x9A");
  ASSERT_TRUE(voiced.hasValue());
  ASSERT_TRUE(semi_voiced.hasValue());
  EXPECT_EQ(voiced.value(), "ア゛");
  EXPECT_EQ(semi_voiced.value(), "パ");
}

TEST_F(NormalizerTest, RepeatedProlongedMarksCollapseIndependentOfWidth) {
  auto fullwidth = normalizer_.normalize("長いーー音");
  auto halfwidth = normalizer_.normalize("長いｰｰ音");
  ASSERT_TRUE(fullwidth.hasValue());
  ASSERT_TRUE(halfwidth.hasValue());
  EXPECT_EQ(fullwidth.value(), "長いー音");
  EXPECT_EQ(halfwidth.value(), "長いー音");
}

TEST(NormalizerOptionsTest, PreserveRepeatedProlongedMarks) {
  NormalizeOptions options;
  options.collapse_repeated_prolonged_marks = false;
  Normalizer normalizer(options);

  const auto result = normalizer.normalize("長いーー音");

  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "長いーー音");
}

// ===== Long Vowel Mark Tests (長音記号) =====

TEST_F(NormalizerTest, LongVowel_FullwidthTilde) {
  auto result = normalizer_.normalize("ラーメン～");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ラーメン~");
}

TEST_F(NormalizerTest, LongVowel_WaveDash) {
  // 〜 (wave dash U+301C)
  auto result = normalizer_.normalize("東京〜大阪");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "東京〜大阪");
}

// ===== Iteration Mark Tests (繰り返し記号) =====

TEST_F(NormalizerTest, IterationMark_Kanji) {
  // 々 iteration mark for kanji
  auto result = normalizer_.normalize("人々");
  ASSERT_TRUE(result.hasValue());
  // Should preserve 々 as-is (it's a valid character)
  EXPECT_EQ(result.value(), "人々");
}

TEST_F(NormalizerTest, IterationMark_Hiragana) {
  // ゝ iteration mark for hiragana
  auto result = normalizer_.normalize("あゝ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "あゝ");
}

TEST_F(NormalizerTest, IterationMark_HiraganaVoiced) {
  // ゞ voiced iteration mark for hiragana
  auto result = normalizer_.normalize("みすゞ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "みすゞ");
}

TEST_F(NormalizerTest, IterationMark_Katakana) {
  // ヽ iteration mark for katakana
  auto result = normalizer_.normalize("アヽ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "アヽ");
}

TEST_F(NormalizerTest, IterationMark_KatakanaVoiced) {
  // ヾ voiced iteration mark for katakana
  auto result = normalizer_.normalize("カヾ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "カヾ");
}

// ===== Punctuation Normalization Tests =====

TEST_F(NormalizerTest, Punctuation_FullwidthComma) {
  auto result = normalizer_.normalize("東京、大阪");
  ASSERT_TRUE(result.hasValue());
  // Should preserve Japanese comma
  EXPECT_EQ(result.value(), "東京、大阪");
}

TEST_F(NormalizerTest, Punctuation_FullwidthPeriod) {
  auto result = normalizer_.normalize("終わり。");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "終わり。");
}

TEST_F(NormalizerTest, Punctuation_FullwidthQuestionMark) {
  auto result = normalizer_.normalize("本当？");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "本当?");
}

TEST_F(NormalizerTest, Punctuation_FullwidthExclamation) {
  auto result = normalizer_.normalize("すごい！");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "すごい!");
}

TEST_F(NormalizerTest, Punctuation_MixedMarks) {
  auto result = normalizer_.normalize("えっ！？");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "えっ!?");
}

// ===== Bracket Normalization Tests =====

TEST_F(NormalizerTest, Bracket_JapaneseQuotes) {
  // 「」 Japanese quotation marks
  auto result = normalizer_.normalize("「こんにちは」");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "「こんにちは」");
}

TEST_F(NormalizerTest, Bracket_DoubleQuotes) {
  // 『』 Double Japanese quotation marks
  auto result = normalizer_.normalize("『本のタイトル』");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "『本のタイトル』");
}

TEST_F(NormalizerTest, Bracket_Parentheses) {
  // （） Full-width parentheses
  auto result = normalizer_.normalize("（注）");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "(注)");
}

TEST_F(NormalizerTest, Bracket_CornerBrackets) {
  // 【】 Lenticular brackets
  auto result = normalizer_.normalize("【重要】");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "【重要】");
}

// ===== Symbol Normalization Tests =====

TEST_F(NormalizerTest, Symbol_JapaneseYen) {
  // ￥ Full-width yen sign
  auto result = normalizer_.normalize("￥1000");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "￥1000");
}

TEST_F(NormalizerTest, Symbol_FullwidthColon) {
  auto result = normalizer_.normalize("時間：10分");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "時間:10分");
}

TEST_F(NormalizerTest, Symbol_MiddleDot) {
  // ・ Middle dot (katakana middle dot)
  auto result = normalizer_.normalize("東京・大阪");
  ASSERT_TRUE(result.hasValue());
}

// ===== Kana Variation Tests =====

TEST_F(NormalizerTest, KanaVariation_SmallKana) {
  // Small kana ぁぃぅぇぉ should be preserved
  auto result = normalizer_.normalize("ふぁいる");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ふぁいる");
}

TEST_F(NormalizerTest, KanaVariation_SmallTsu) {
  // っ small tsu
  auto result = normalizer_.normalize("がっこう");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "がっこう");
}

TEST_F(NormalizerTest, KanaVariation_SmallYaYuYo) {
  // ゃゅょ small ya, yu, yo
  auto result = normalizer_.normalize("きょうと");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "きょうと");
}

TEST_F(NormalizerTest, KanaVariation_KatakanaSmallKana) {
  // Small katakana
  auto result = normalizer_.normalize("ファイル");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ファイル");
}

// ===== Combining Character Tests =====

TEST_F(NormalizerTest, CombiningChar_DakutenSeparate) {
  // が as か + combining dakuten (U+3099)
  // This tests NFC normalization
  auto result = normalizer_.normalize("か\u3099");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "が");
}

TEST_F(NormalizerTest, CombiningChar_HandakutenSeparate) {
  // ぱ as は + combining handakuten (U+309A)
  auto result = normalizer_.normalize("は\u309A");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ぱ");
}

TEST_F(NormalizerTest, CombiningChar_KatakanaCombiningDakuten) {
  // カ + combining dakuten (U+3099) -> ガ
  auto result = normalizer_.normalize("カ\u3099行");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ガ行");
}

TEST_F(NormalizerTest, CombiningChar_KatakanaHandakuten) {
  // ハ + combining handakuten (U+309A) -> パ
  auto result = normalizer_.normalize("ハ\u309Aン");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "パン");
}

TEST_F(NormalizerTest, SpacingMark_KatakanaDakuten) {
  // カ + spacing dakuten (U+309B) -> ガ
  auto result = normalizer_.normalize("カ\u309B行");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ガ行");
}

TEST_F(NormalizerTest, SpacingMark_HiraganaDakuten) {
  // か + spacing dakuten (U+309B) -> が
  auto result = normalizer_.normalize("か\u309B");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "が");
}

TEST_F(NormalizerTest, SpacingMark_HiraganaHandakuten) {
  // は + spacing handakuten (U+309C) -> ぱ
  auto result = normalizer_.normalize("は\u309C");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ぱ");
}

TEST_F(NormalizerTest, SpacingMark_TaRowDakuten) {
  // タ + spacing dakuten (U+309B) -> ダ
  auto result = normalizer_.normalize("タ\u309Bンス");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ダンス");
}

TEST_F(NormalizerTest, KatakanaWaRowComposesEveryVoicedForm) {
  struct VoicedCase {
    std::string_view base;
    std::string_view expected;
  };
  constexpr VoicedCase kCases[] = {
      {"ワ", "ヷ"},
      {"ヰ", "ヸ"},
      {"ヱ", "ヹ"},
      {"ヲ", "ヺ"},
  };
  constexpr std::string_view kMarks[] = {"\u3099", "\u309B", "ﾞ"};

  for (const auto& test_case : kCases) {
    for (std::string_view mark : kMarks) {
      auto result = normalizer_.normalize(std::string(test_case.base) + std::string(mark));
      ASSERT_TRUE(result.hasValue()) << test_case.base << mark;
      EXPECT_EQ(result.value(), test_case.expected) << test_case.base << mark;
    }
    auto precomposed = normalizer_.normalize(test_case.expected);
    ASSERT_TRUE(precomposed.hasValue());
    EXPECT_EQ(precomposed.value(), test_case.expected);
  }

  // Hiragana わ has no precomposed voiced counterpart.
  auto hiragana_result = normalizer_.normalize("わ\u309B");
  ASSERT_TRUE(hiragana_result.hasValue());
  EXPECT_EQ(hiragana_result.value(), "わ\u309B");
}

TEST_F(NormalizerTest, SpacingMark_InvalidBaseUnchanged) {
  // あ cannot take dakuten: sequence passes through unchanged
  auto result = normalizer_.normalize("あ\u309B");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "あ\u309B");
}

TEST_F(NormalizerTest, CombiningChar_InvalidBaseUsesSpacingMark) {
  // A stray combining dakuten uses the same standalone spelling as U+309B.
  auto result = normalizer_.normalize("あ\u3099");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "あ\u309B");
}

TEST_F(NormalizerTest, CombiningChar_PrecomposedUnchanged) {
  // Already-precomposed voiced kana stay as-is
  auto result = normalizer_.normalize("がぎだばぱ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "がぎだばぱ");
}

// ===== Mixed Script Normalization Tests =====

TEST_F(NormalizerTest, MixedScript_AlphaNumericJapanese) {
  auto result = normalizer_.normalize("ABC123日本語");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "abc123日本語");
}

TEST_F(NormalizerTest, MixedScript_FullwidthAlphaJapanese) {
  auto result = normalizer_.normalize("ＡＢＣ日本語");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "abc日本語");
}

// ===== Whitespace Normalization Tests =====

TEST_F(NormalizerTest, Whitespace_FullwidthSpace) {
  // Full-width space U+3000
  auto result = normalizer_.normalize("東京　大阪");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "東京　大阪");
}

TEST_F(NormalizerTest, Whitespace_MultipleSpaces) {
  auto result = normalizer_.normalize("東京  大阪");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "東京  大阪");
}

// ===== Extended Character Tests =====

TEST_F(NormalizerTest, ExtendedChar_RareKanji) {
  // Rare kanji that might be in extension areas
  auto result = normalizer_.normalize("龍");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "龍");
}

TEST_F(NormalizerTest, ExtendedChar_CircledNumbers) {
  // ① ② ③ Circled numbers
  auto result = normalizer_.normalize("①②③");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "①②③");
}

TEST_F(NormalizerTest, ExtendedChar_RomanNumerals) {
  // Ⅰ Ⅱ Ⅲ Roman numerals
  auto result = normalizer_.normalize("ⅠⅡⅢ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ⅠⅡⅢ");
}

// ===== Normalization Options Tests =====

TEST(NormalizerOptionsTest, PreserveVu_Default) {
  // Default: ヴ is preserved (preserve_vu=true)
  Normalizer normalizer;
  auto result = normalizer.normalize("ヴィトン");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ヴィトン");
}

TEST(NormalizerOptionsTest, PreserveVu_Disabled) {
  // With preserve_vu=false: ヴ→ビ conversion enabled
  NormalizeOptions opts;
  opts.preserve_vu = false;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("ヴィトン");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ビトン");
}

TEST(NormalizerOptionsTest, PreserveVu_LouisVuitton) {
  NormalizeOptions opts;
  opts.preserve_vu = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("ルイ・ヴィトン");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ルイ・ヴィトン");
}

TEST(NormalizerOptionsTest, PreserveVu_MixedText) {
  NormalizeOptions opts;
  opts.preserve_vu = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("ヴァイオリンとピアノ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ヴァイオリンとピアノ");
}

TEST(NormalizerOptionsTest, PreserveCase_Default) {
  // Default: case is preserved (preserve_case=true)
  Normalizer normalizer;
  auto result = normalizer.normalize("Hello World");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "Hello World");
}

TEST(NormalizerOptionsTest, PreserveCase_Disabled) {
  // With preserve_case=false: uppercase→lowercase conversion enabled
  NormalizeOptions opts;
  opts.preserve_case = false;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("Hello World");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "hello world");
}

TEST(NormalizerOptionsTest, PreserveCase_FullwidthAlpha) {
  NormalizeOptions opts;
  opts.preserve_case = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("ＡＢＣＤＥＦ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "ABCDEF");
}

TEST(NormalizerOptionsTest, PreserveCase_MixedJapaneseEnglish) {
  NormalizeOptions opts;
  opts.preserve_case = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("Tokyo Tower");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "Tokyo Tower");
}

TEST(NormalizerOptionsTest, BothOptionsEnabled) {
  NormalizeOptions opts;
  opts.preserve_vu = true;
  opts.preserve_case = true;
  Normalizer normalizer(opts);
  auto result = normalizer.normalize("LOUIS VUITTONのヴァッグ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "LOUIS VUITTONのヴァッグ");
}

TEST(NormalizerOptionsTest, SetOptionsAfterConstruction) {
  Normalizer normalizer;

  // Default behavior: preserve case (preserve_case=true by default)
  auto result1 = normalizer.normalize("Hello");
  ASSERT_TRUE(result1.hasValue());
  EXPECT_EQ(result1.value(), "Hello");

  // Change options to enable lowercase conversion
  NormalizeOptions opts;
  opts.preserve_case = false;
  normalizer.setOptions(opts);

  // New behavior: lowercase
  auto result2 = normalizer.normalize("Hello");
  ASSERT_TRUE(result2.hasValue());
  EXPECT_EQ(result2.value(), "hello");
}

// ===== Error Handling Tests =====

TEST_F(NormalizerTest, ErrorHandling_InvalidUtf8) {
  std::string invalid_utf8 = "\xFF\xFE";
  auto result = normalizer_.normalize(invalid_utf8);
  ASSERT_FALSE(result.hasValue());
  ASSERT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, core::ErrorCode::InvalidUtf8);
}

TEST_F(NormalizerTest, ErrorHandling_IncompleteUtf8) {
  std::string incomplete = "\xE3\x81";  // Incomplete hiragana
  auto result = normalizer_.normalize(incomplete);
  ASSERT_FALSE(result.hasValue());
  ASSERT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, core::ErrorCode::InvalidUtf8);
}

}  // namespace
}  // namespace suzume::normalize
