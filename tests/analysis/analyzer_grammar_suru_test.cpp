// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Grammar tests for suru verbs (サ変動詞)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// ===== Suru Verb Tests (サ変動詞) =====
// These tests verify that Noun+する patterns are handled by the inflection
// analyzer without needing dictionary entries.

class SuruVerbTest : public ::testing::Test {
 protected:
  SuruVerbTest() : analyzer(options) {}

  bool hasVerb(const std::vector<core::Morpheme>& morphemes,
               std::string_view surface) {
    for (const auto& mor : morphemes) {
      if (mor.surface == surface && mor.pos == core::PartOfSpeech::Verb) {
        return true;
      }
    }
    return false;
  }

  AnalyzerOptions options;
  Analyzer analyzer;
};

TEST_F(SuruVerbTest, Basic_BenkyouSuru) {
  // 勉強する (to study)
  auto result = analyzer.analyze("勉強する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "勉強する")) << "勉強する should be VERB";
}

TEST_F(SuruVerbTest, Basic_BunsekiSuru) {
  // 分析する (to analyze)
  auto result = analyzer.analyze("分析する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "分析する")) << "分析する should be VERB";
}

TEST_F(SuruVerbTest, Basic_KakuninSuru) {
  // 確認する (to confirm)
  auto result = analyzer.analyze("確認する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "確認する")) << "確認する should be VERB";
}

TEST_F(SuruVerbTest, Basic_SetsumeiSuru) {
  // 説明する (to explain)
  auto result = analyzer.analyze("説明する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "説明する")) << "説明する should be VERB";
}

TEST_F(SuruVerbTest, Basic_ShoriSuru) {
  // 処理する (to process)
  auto result = analyzer.analyze("処理する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "処理する")) << "処理する should be VERB";
}

TEST_F(SuruVerbTest, Past_BenkyouShita) {
  // 勉強した (studied)
  auto result = analyzer.analyze("勉強した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "勉強した")) << "勉強した should be VERB";
}

TEST_F(SuruVerbTest, Teiru_BenkyouShiteiru) {
  // 勉強している (is studying)
  auto result = analyzer.analyze("勉強している");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "勉強している")) << "勉強している should be VERB";
}

TEST_F(SuruVerbTest, Masu_KakuninShimasu) {
  // 確認します (will confirm - polite)
  auto result = analyzer.analyze("確認します");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "確認します")) << "確認します should be VERB";
}

TEST_F(SuruVerbTest, Nai_SetsumeiShinai) {
  // 説明しない (does not explain)
  auto result = analyzer.analyze("説明しない");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "説明しない")) << "説明しない should be VERB";
}

TEST_F(SuruVerbTest, Tai_RyokouShitai) {
  // 旅行したい (want to travel)
  auto result = analyzer.analyze("旅行したい");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "旅行したい")) << "旅行したい should be VERB";
}

TEST_F(SuruVerbTest, Passive_ShoriSareru) {
  // 処理される (is processed)
  auto result = analyzer.analyze("処理される");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "処理される")) << "処理される should be VERB";
}

TEST_F(SuruVerbTest, Causative_BenkyouSaseru) {
  // 勉強させる (make someone study)
  auto result = analyzer.analyze("勉強させる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "勉強させる")) << "勉強させる should be VERB";
}

}  // namespace
}  // namespace suzume::analysis
