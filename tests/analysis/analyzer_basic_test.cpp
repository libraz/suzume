// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Basic analyzer functionality tests

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

TEST(AnalyzerTest, AnalyzeEmptyStringReturnsEmpty) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("");
  EXPECT_TRUE(result.empty());
}

TEST(AnalyzerTest, AnalyzeSimpleKanji) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("世界");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "世界");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
}

TEST(AnalyzerTest, AnalyzeWithParticle) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("私は");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "私");
  EXPECT_EQ(result[1].surface, "は");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
}

TEST(AnalyzerTest, AnalyzeHiragana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("こんにちは");
  ASSERT_FALSE(result.empty());
  // Entire hiragana string should be parsed as one or more morphemes
}

TEST(AnalyzerTest, AnalyzeMixedText) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("私は猫が好き");
  // Should have multiple morphemes
  ASSERT_GE(result.size(), 3);

  // Check for particles
  bool has_wa = false;
  bool has_ga = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "は") has_wa = true;
    if (morpheme.surface == "が") has_ga = true;
  }
  EXPECT_TRUE(has_wa);
  EXPECT_TRUE(has_ga);
}

TEST(AnalyzerTest, AnalyzeMultipleSentences) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今日は天気です");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, MorphemeHasCorrectLemma) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("は");
  ASSERT_EQ(result.size(), 1);
  EXPECT_FALSE(result[0].lemma.empty());
}

// ===== Edge Cases =====

TEST(AnalyzerTest, EdgeCase_OnlyPunctuation) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("。。。");
  // Should handle gracefully
}

TEST(AnalyzerTest, EdgeCase_MixedPunctuation) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("えっ！？本当に？");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, EdgeCase_RepeatedCharacter) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("あああああ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, EdgeCase_VeryLongWord) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("独立行政法人情報処理推進機構");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, EdgeCase_SingleKanji) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("空");
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(result[0].surface, "空");
}

TEST(AnalyzerTest, EdgeCase_SingleHiragana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("あ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, EdgeCase_SingleKatakana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ア");
  ASSERT_FALSE(result.empty());
}

// ===== Special Character Tests =====

TEST(AnalyzerTest, SpecialChar_LongVowelMark) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("コーヒー");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SpecialChar_SmallTsu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ちょっと待って");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SpecialChar_Kurikaeshi) {
  // 々 iteration mark
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("人々が集まる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SpecialChar_OldKana) {
  // Old kana like ゑ, ゐ
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ゐる");
  ASSERT_FALSE(result.empty());
}

}  // namespace
}  // namespace suzume::analysis
