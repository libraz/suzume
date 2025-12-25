// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Grammar tests for auxiliary verbs and keigo (honorific expressions)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// ===== Auxiliary Verb Tests (助動詞) =====

TEST(AnalyzerTest, AuxiliaryVerb_Desu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("これは本です");
  bool found_desu = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "です") {
      found_desu = true;
      break;
    }
  }
  EXPECT_TRUE(found_desu);
}

TEST(AnalyzerTest, AuxiliaryVerb_Masu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, AuxiliaryVerb_Tai) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行きたい");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, AuxiliaryVerb_Nai) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行かない");
  ASSERT_FALSE(result.empty());
}

// ===== Keigo (敬語) Tests =====

TEST(AnalyzerTest, Keigo_Irassharu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("先生がいらっしゃる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Gozaimasu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ございます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Itadaku) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("いただきます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Kudasaru) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("教えてくださる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_OPrefix) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("お忙しいところ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_GoPrefix) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ご確認ください");
  ASSERT_FALSE(result.empty());
}

}  // namespace
}  // namespace suzume::analysis
