// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Regression tests for copula (だった, でした, であった) recognition

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// =============================================================================
// Regression: Copula だった (断定の助動詞)
// =============================================================================
// Bug: だった was recognized as VERB with lemma だる
// Fix: だった should be AUX with lemma だった (copula doesn't conjugate to だる)

TEST(AnalyzerTest, Regression_DattaCopulaPos) {
  // だった should be recognized as Auxiliary, not Verb
  Suzume analyzer;
  auto result = analyzer.analyze("神だった");
  ASSERT_FALSE(result.empty());

  bool found_datta = false;
  for (const auto& mor : result) {
    if (mor.surface == "だった") {
      found_datta = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "だった should be Auxiliary, not Verb";
      break;
    }
  }
  EXPECT_TRUE(found_datta) << "だった should be found in 神だった";
}

TEST(AnalyzerTest, Regression_DattaCopulaLemma) {
  // だった lemma should be だった, not だる
  Suzume analyzer;
  auto result = analyzer.analyze("本だった");
  ASSERT_FALSE(result.empty());

  bool found_datta = false;
  for (const auto& mor : result) {
    if (mor.surface == "だった") {
      found_datta = true;
      EXPECT_EQ(mor.lemma, "だった")
          << "だった lemma should be だった, not だる";
      break;
    }
  }
  EXPECT_TRUE(found_datta) << "だった should be found in 本だった";
}

TEST(AnalyzerTest, Regression_DattaInSentence) {
  // Full sentence with だった
  Suzume analyzer;
  auto result = analyzer.analyze("ワンマンライブのセットリストが神だった");
  ASSERT_FALSE(result.empty());

  bool found_datta = false;
  for (const auto& mor : result) {
    if (mor.surface == "だった") {
      found_datta = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "だった should be Auxiliary in sentence";
      EXPECT_EQ(mor.lemma, "だった")
          << "だった lemma should be だった in sentence";
      break;
    }
  }
  EXPECT_TRUE(found_datta) << "だった should be found in sentence";
}

TEST(AnalyzerTest, Regression_DeshitaCopula) {
  // でした (polite past copula) should also be Auxiliary
  Suzume analyzer;
  auto result = analyzer.analyze("本でした");
  ASSERT_FALSE(result.empty());

  bool found_deshita = false;
  for (const auto& mor : result) {
    if (mor.surface == "でした") {
      found_deshita = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "でした should be Auxiliary";
      EXPECT_EQ(mor.lemma, "でした")
          << "でした lemma should be でした";
      break;
    }
  }
  EXPECT_TRUE(found_deshita) << "でした should be found in 本でした";
}

TEST(AnalyzerTest, Regression_DeattaCopula) {
  // であった (formal past copula) should be Auxiliary
  // Copula forms are hardcoded because they cannot be reliably split
  Suzume analyzer;
  auto result = analyzer.analyze("重要であった");
  ASSERT_FALSE(result.empty());

  bool found_deatta = false;
  for (const auto& mor : result) {
    if (mor.surface == "であった") {
      found_deatta = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "であった should be Auxiliary";
      break;
    }
  }
  EXPECT_TRUE(found_deatta) << "であった should be found in 重要であった";
}

// Ensure copula after noun is not affected
TEST(AnalyzerTest, Regression_CopulaAfterNounNotAffected) {
  Suzume analyzer;
  auto result = analyzer.analyze("学生だ");
  ASSERT_EQ(result.size(), 2);

  EXPECT_EQ(result[0].surface, "学生");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[1].surface, "だ");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
}

}  // namespace
}  // namespace suzume::analysis
