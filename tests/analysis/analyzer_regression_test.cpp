// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Regression tests to ensure previously fixed bugs don't reoccur

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
// Regression: しそう auxiliary lemma
// =============================================================================
// Bug: 遅刻しそう lemma was 遅刻しい (incorrect)
// Fix: しそう pattern should produce correct lemma 遅刻する

TEST(AnalyzerTest, Regression_ShisouLemma) {
  // 遅刻しそう should have lemma 遅刻する
  // Use full Suzume pipeline which includes lemmatization
  Suzume analyzer;
  auto result = analyzer.analyze("遅刻しそう");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "遅刻しそう" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "遅刻する")
          << "遅刻しそう lemma should be 遅刻する";
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "遅刻しそう should be recognized as verb";
}

TEST(AnalyzerTest, Regression_SouAuxiliaryPattern) {
  // 食べそう should have lemma 食べる
  Suzume analyzer;
  auto result = analyzer.analyze("食べそう");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "食べそう" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "食べる") << "食べそう lemma should be 食べる";
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "食べそう should be recognized as verb";
}

TEST(AnalyzerTest, Regression_SouWithDesu) {
  // 遅刻しそうです should have lemma 遅刻する for the verb part
  Suzume analyzer;
  auto result = analyzer.analyze("遅刻しそうです");
  ASSERT_FALSE(result.empty());

  bool found_chikoku = false;
  for (const auto& mor : result) {
    if (mor.surface.find("遅刻") != std::string::npos &&
        mor.pos == core::PartOfSpeech::Verb) {
      found_chikoku = true;
      EXPECT_EQ(mor.lemma, "遅刻する")
          << "遅刻しそうです verb part lemma should be 遅刻する";
      break;
    }
  }
  EXPECT_TRUE(found_chikoku) << "遅刻しそうです should contain verb with 遅刻";
}

// =============================================================================
// Script Boundary Tests (文字種境界テスト)
// =============================================================================
// Tests for proper segmentation at script boundaries (ASCII/Japanese)

TEST(AnalyzerTest, ScriptBoundary_AsciiToJapaneseVerb) {
  // "iphone買った" should split at ASCII→Japanese boundary
  // Expected: "iphone" (NOUN) + "買った" (VERB)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("iphone買った");
  ASSERT_FALSE(result.empty());

  // Should have at least 2 tokens (iphone + verb)
  EXPECT_GE(result.size(), 2)
      << "iphone買った should split into at least 2 tokens";

  // Check that we have a verb token that looks conjugated
  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      // The verb should contain 買 (not merged with iphone)
      EXPECT_TRUE(mor.surface.find("買") != std::string::npos ||
                  mor.surface.find("った") != std::string::npos)
          << "Verb token should contain 買 or った, got: " << mor.surface;
    }
  }
  EXPECT_TRUE(found_verb) << "Should find a verb token in iphone買った";
}

TEST(AnalyzerTest, ScriptBoundary_ParticlelessNounVerb) {
  // "本買った" without particle should still split noun and verb
  // This is a colloquial pattern (本を買った with を omitted)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本買った");
  ASSERT_FALSE(result.empty());

  // Should have at least 2 tokens (noun + verb)
  EXPECT_GE(result.size(), 2)
      << "本買った should split into at least 2 tokens";

  // Check that 買った is recognized as a separate verb
  bool found_verb_with_katta = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb &&
        mor.surface.find("買") != std::string::npos) {
      found_verb_with_katta = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb_with_katta)
      << "買った should be recognized as a verb in 本買った";
}

TEST(AnalyzerTest, ScriptBoundary_MixedWithTeForm) {
  // "買ってきた" - compound verb with て-form
  // Should be recognized as a single verb unit with correct lemma
  // Note: Using Suzume (not Analyzer) because lemmatization requires
  // the full pipeline including Postprocessor with dictionary verification
  Suzume suzume;
  auto result = suzume.analyze("買ってきた");
  ASSERT_FALSE(result.empty());

  // Check that it's recognized as verb with correct lemma
  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      // Lemma should be 買う (base form of the main verb)
      // This requires dictionary-aware lemmatization to disambiguate
      // between GodanWa (買う), GodanRa (買る), and GodanTa (買つ)
      EXPECT_EQ(mor.lemma, "買う")
          << "買ってきた should have lemma 買う, got: " << mor.lemma;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "買ってきた should be recognized as verb";
}

}  // namespace
}  // namespace suzume::analysis
