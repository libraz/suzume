// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Regression tests for adjective recognition (i-adjectives, na-adjectives)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// =============================================================================
// Regression: I-adjective recognition
// =============================================================================
// Bug: 悲しい was incorrectly recognized as Verb
// Fix: Should be recognized as Adjective

TEST(AnalyzerTest, Regression_IAdjectiveKanashii) {
  // 悲しい should be recognized as Adjective
  Suzume analyzer;
  auto result = analyzer.analyze("悲しい");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "悲しい should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "悲しい");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "悲しい should be Adjective, not Verb";
    EXPECT_EQ(result[0].lemma, "悲しい")
        << "悲しい lemma should be 悲しい";
  }
}

TEST(AnalyzerTest, Regression_IAdjectiveUtsukushikatta) {
  // 美しかった should be recognized as Adjective with lemma 美しい
  Suzume analyzer;
  auto result = analyzer.analyze("美しかった");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "美しかった should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "美しかった");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "美しかった should be Adjective";
    EXPECT_EQ(result[0].lemma, "美しい")
        << "美しかった lemma should be 美しい";
  }
}

// =============================================================================
// Regression: Adjective + particle pattern
// =============================================================================
// Bug: 面白いな was not properly splitting adjective and particle
// Fix: Should be 面白い (ADJ) + な (PARTICLE)

TEST(AnalyzerTest, Regression_AdjectiveParticleOmoshiroina) {
  // 面白いな should split as: 面白い + な
  Suzume analyzer;
  auto result = analyzer.analyze("面白いな");
  ASSERT_GE(result.size(), 2) << "面白いな should have at least 2 tokens";

  bool found_omoshiroi = false;
  bool found_na = false;
  for (const auto& mor : result) {
    if (mor.surface == "面白い") {
      found_omoshiroi = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "面白い should be Adjective";
    }
    if (mor.surface == "な" && mor.pos == core::PartOfSpeech::Particle) {
      found_na = true;
    }
  }
  EXPECT_TRUE(found_omoshiroi) << "面白い should be found";
  EXPECT_TRUE(found_na) << "な particle should be found";
}

// =============================================================================
// Regression: Irregular adjective いい
// =============================================================================
// Bug: いいよね was not properly tokenized (いい not recognized)
// Fix: いい should be recognized as Adjective

TEST(AnalyzerTest, Regression_IrregularAdjectiveIi) {
  // いいよね should split as: いい + よ + ね (or いい + よね)
  Suzume analyzer;
  auto result = analyzer.analyze("いいよね");
  ASSERT_FALSE(result.empty());

  // Should find いい as adjective
  bool found_ii = false;
  for (const auto& mor : result) {
    if (mor.surface == "いい") {
      found_ii = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "いい should be Adjective";
    }
  }
  EXPECT_TRUE(found_ii) << "いい should be found in いいよね";

  // Should have sentence-ending particles
  bool found_particle = false;
  for (const auto& mor : result) {
    if ((mor.surface == "よ" || mor.surface == "ね" ||
         mor.surface == "よね") &&
        mor.pos == core::PartOfSpeech::Particle) {
      found_particle = true;
    }
  }
  EXPECT_TRUE(found_particle)
      << "Sentence-ending particle should be found in いいよね";
}

// =============================================================================
// Regression: Single-kanji i-adjective 寒い
// =============================================================================
// Bug: 寒い was split as 寒 + い due to ADJ candidate skip heuristic
// Fix: Should be single ADJ token via dictionary

TEST(AnalyzerTest, Regression_IAdjective_Samui) {
  Suzume analyzer;
  auto result = analyzer.analyze("今日は寒いですね");
  ASSERT_GE(result.size(), 4) << "Should have at least 4 tokens";

  bool found_samui = false;
  for (const auto& mor : result) {
    if (mor.surface == "寒い") {
      found_samui = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "寒い should be Adjective";
      EXPECT_EQ(mor.lemma, "寒い")
          << "寒い lemma should be 寒い";
    }
  }
  EXPECT_TRUE(found_samui) << "寒い should be found as single token";
}

// =============================================================================
// Regression: Na-adjective 好き
// =============================================================================
// Bug: 好き was split as 好 + き
// Fix: Added 好き to na_adjectives.h

TEST(AnalyzerTest, Regression_NaAdjective_Suki) {
  Suzume analyzer;
  auto result = analyzer.analyze("好き");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "好き should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "好き");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "好き should be Adjective";
  }
}

TEST(AnalyzerTest, Regression_NaAdjective_SukiNa) {
  Suzume analyzer;
  auto result = analyzer.analyze("好きな食べ物");
  ASSERT_GE(result.size(), 3) << "Should have at least 3 tokens";

  bool found_suki = false;
  bool found_na = false;
  bool found_tabemono = false;
  for (const auto& mor : result) {
    if (mor.surface == "好き") {
      found_suki = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "好き should be Adjective";
    }
    if (mor.surface == "な" && mor.pos == core::PartOfSpeech::Particle) {
      found_na = true;
    }
    if (mor.surface == "食べ物") {
      found_tabemono = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "食べ物 should be Noun";
    }
  }
  EXPECT_TRUE(found_suki) << "好き should be found";
  EXPECT_TRUE(found_na) << "な particle should be found";
  EXPECT_TRUE(found_tabemono) << "食べ物 should be found";
}

TEST(AnalyzerTest, Regression_NaAdjective_Kirai) {
  Suzume analyzer;
  auto result = analyzer.analyze("嫌い");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "嫌い should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "嫌い");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "嫌い should be Adjective";
  }
}

// =============================================================================
// Regression: Te-form contraction not adjective
// =============================================================================
// Bug: 待ってく was analyzed as adjective, not 待って + く
// Fix: Skip っ + hiragana patterns in generateAdjectiveCandidates

TEST(AnalyzerTest, Regression_TeKuNotAdjective) {
  Suzume analyzer;
  auto result = analyzer.analyze("待ってくれない");
  ASSERT_GE(result.size(), 2);

  // Should be 待って + くれない, not 待ってく + れない
  bool found_matte = false;
  bool found_kurenai = false;
  for (const auto& mor : result) {
    if (mor.surface == "待って" && mor.pos == core::PartOfSpeech::Verb) {
      found_matte = true;
    }
    if (mor.surface == "くれない" && mor.pos == core::PartOfSpeech::Verb) {
      found_kurenai = true;
    }
  }
  EXPECT_TRUE(found_matte) << "待って should be recognized as verb";
  EXPECT_TRUE(found_kurenai) << "くれない should be recognized as verb";
}

// =============================================================================
// Regression: Hiragana adjective conjugation
// =============================================================================
// Bug: まずかった was split as まず + か + った
// Fix: Added generateHiraganaAdjectiveCandidates

TEST(AnalyzerTest, Regression_HiraganaAdjective) {
  Suzume analyzer;
  auto result = analyzer.analyze("まずかった");
  ASSERT_EQ(result.size(), 1) << "まずかった should be single token";
  EXPECT_EQ(result[0].surface, "まずかった");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  // Hiragana-only adjective keeps hiragana lemma
  EXPECT_EQ(result[0].lemma, "まずい");
}

TEST(AnalyzerTest, Regression_HiraganaAdjective_Oishii) {
  Suzume analyzer;
  auto result = analyzer.analyze("おいしくない");
  ASSERT_EQ(result.size(), 1) << "おいしくない should be single token";
  EXPECT_EQ(result[0].surface, "おいしくない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "おいしい");
}

// =============================================================================
// Regression: Na-adjective + copula (幸いです)
// =============================================================================
// Bug: 幸いです was being parsed as 幸いで (VERB) + す (OTHER)
// Fix: Added 幸い to na_adjectives.h, added penalty for い-ending stems

TEST(AnalyzerTest, Regression_NaAdjective_SaiwaiDesu) {
  Suzume analyzer;
  auto result = analyzer.analyze("幸いです");
  ASSERT_GE(result.size(), 2) << "幸いです should split into 幸い + です";

  bool found_saiwai = false;
  bool found_desu = false;
  for (const auto& mor : result) {
    if (mor.surface == "幸い") {
      found_saiwai = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "幸い should be Adjective";
    }
    if (mor.surface == "です") {
      found_desu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "です should be Auxiliary";
    }
  }
  EXPECT_TRUE(found_saiwai) << "幸い should be found as separate token";
  EXPECT_TRUE(found_desu) << "です should be found as separate token";
}

TEST(AnalyzerTest, Regression_NaAdjective_BusinessEmail) {
  // Full business email pattern: ご返信いただけますと幸いです
  Suzume analyzer;
  auto result = analyzer.analyze("ご返信いただけますと幸いです");
  ASSERT_GE(result.size(), 4);

  bool found_saiwai = false;
  bool found_desu = false;
  for (const auto& mor : result) {
    if (mor.surface == "幸い") {
      found_saiwai = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective);
    }
    if (mor.surface == "です") {
      found_desu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary);
    }
  }
  EXPECT_TRUE(found_saiwai) << "幸い should be found";
  EXPECT_TRUE(found_desu) << "です should be found";
}

}  // namespace
}  // namespace suzume::analysis
