// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Regression tests for particle separation and recognition

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// =============================================================================
// Regression: Particle を separation
// =============================================================================
// Bug: をなくしてしまった was being merged as one token
// Fix: を should always be recognized as separate particle

TEST(AnalyzerTest, Regression_WoParticleSeparation) {
  // 本をなくした - を should be separate particle
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本をなくした");
  ASSERT_FALSE(result.empty());

  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "を should be recognized as separate particle";
}

TEST(AnalyzerTest, Regression_WoNotMergedWithVerb) {
  // をなくして should not be merged as single unknown word
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("をなくして");
  ASSERT_FALSE(result.empty());

  // First token should be を as particle
  EXPECT_EQ(result[0].surface, "を");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Particle);
}

TEST(AnalyzerTest, Regression_WoInComplex) {
  // Full sentence: 昨日買ったばかりの本をなくしてしまった
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("昨日買ったばかりの本をなくしてしまった");
  ASSERT_FALSE(result.empty());

  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "を should be separate particle in complex sentence";
}

// =============================================================================
// Regression: ので lemma
// =============================================================================
// Bug: ので lemma was のる (incorrectly treated as verb)
// Fix: ので lemma should be ので (particle/conjunction doesn't conjugate)

TEST(AnalyzerTest, Regression_NodeLemma) {
  // ので should have lemma ので (not のる)
  // Use full Suzume pipeline which includes lemmatization
  Suzume analyzer;
  auto result = analyzer.analyze("ので");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "ので");
  EXPECT_EQ(result[0].lemma, "ので") << "ので lemma should be ので, not のる";
}

TEST(AnalyzerTest, Regression_NodeInSentence) {
  // 電車が遅れているので遅刻しそうです
  Suzume analyzer;
  auto result = analyzer.analyze("電車が遅れているので遅刻しそうです");
  ASSERT_FALSE(result.empty());

  bool found_node = false;
  for (const auto& mor : result) {
    if (mor.surface == "ので") {
      found_node = true;
      EXPECT_EQ(mor.lemma, "ので") << "ので lemma should be ので";
      break;
    }
  }
  EXPECT_TRUE(found_node) << "ので should be recognized";
}

// =============================================================================
// Regression: Particle filter in verb/adjective candidates
// =============================================================================
// Bug: 家にいます was parsed as verb 家にう, 金がない as verb 金ぐ
// Fix: Added に/が to particle filter in generateVerbCandidates/generateAdjectiveCandidates

TEST(AnalyzerTest, Regression_ParticleFilter_IeNiImasu) {
  // 家にいます should be: 家 + に + います
  Suzume analyzer;
  auto result = analyzer.analyze("家にいます");
  ASSERT_GE(result.size(), 3) << "家にいます should have at least 3 tokens";

  bool found_ie = false;
  bool found_ni = false;
  bool found_imasu = false;
  for (const auto& mor : result) {
    if (mor.surface == "家") {
      found_ie = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun) << "家 should be Noun";
    }
    if (mor.surface == "に" && mor.pos == core::PartOfSpeech::Particle) {
      found_ni = true;
    }
    if (mor.surface == "います") {
      found_imasu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb) << "います should be Verb";
    }
  }
  EXPECT_TRUE(found_ie) << "家 should be found as separate token";
  EXPECT_TRUE(found_ni) << "に should be found as particle";
  EXPECT_TRUE(found_imasu) << "います should be found as verb";
}

TEST(AnalyzerTest, Regression_ParticleFilter_KaneGaNai) {
  // 金がない should be: 金 + が + ない
  Suzume analyzer;
  auto result = analyzer.analyze("金がない");
  ASSERT_GE(result.size(), 3) << "金がない should have at least 3 tokens";

  bool found_kane = false;
  bool found_ga = false;
  bool found_nai = false;
  for (const auto& mor : result) {
    if (mor.surface == "金") {
      found_kane = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun) << "金 should be Noun";
    }
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
    }
    if (mor.surface == "ない") {
      found_nai = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary) << "ない should be Aux";
    }
  }
  EXPECT_TRUE(found_kane) << "金 should be found as separate token";
  EXPECT_TRUE(found_ga) << "が should be found as particle";
  EXPECT_TRUE(found_nai) << "ない should be found as auxiliary";
}

}  // namespace
}  // namespace suzume::analysis
