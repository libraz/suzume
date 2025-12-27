// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Strict analyzer tests: Mixed script, Contractions, Symbols, Complex
// expressions, Real world sentences

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::getSurfaces;

// Base class for tests that need core dictionary
class AnalyzerTestBase : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};

  void SetUp() override {
    analyzer.tryAutoLoadCoreDictionary();
  }
};

// ===== Mixed Script Tests =====

class MixedScriptStrictTest : public AnalyzerTestBase {};

TEST_F(MixedScriptStrictTest, EnglishWithParticle_Wo) {
  auto result = analyzer.analyze("APIを呼ぶ");
  auto surfaces = getSurfaces(result);

  EXPECT_GE(surfaces.size(), 2) << "Should have at least 2 tokens";

  bool found_wo = false;
  for (const auto& sur : surfaces) {
    if (sur == "を") found_wo = true;
  }
  EXPECT_TRUE(found_wo) << "Should contain を particle";
}

TEST_F(MixedScriptStrictTest, CamelCasePreserved) {
  auto result = analyzer.analyze("getUserDataを呼ぶ");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  ASSERT_GE(surfaces.size(), 3) << "Should have at least 3 tokens. " << debug_msg;

  bool found_wo = false;
  for (const auto& sur : surfaces) {
    if (sur == "を") found_wo = true;
  }
  EXPECT_TRUE(found_wo) << "Should contain を particle. " << debug_msg;

  bool found_function = false;
  for (const auto& sur : surfaces) {
    std::string lower_sur = sur;
    for (auto& chr : lower_sur) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    if (lower_sur.find("userdata") != std::string::npos) {
      found_function = true;
      break;
    }
  }
  EXPECT_TRUE(found_function)
      << "Should contain userdata in some token. " << debug_msg;
}

TEST_F(MixedScriptStrictTest, DigitWithUnit) {
  auto result = analyzer.analyze("3人で行く");
  auto surfaces = getSurfaces(result);

  EXPECT_GE(surfaces.size(), 2);
  EXPECT_LE(surfaces.size(), 4);
}

// ===== Contraction Tests =====

class ContractionStrictTest : public AnalyzerTestBase {};

TEST_F(ContractionStrictTest, Shiteru) {
  auto result = analyzer.analyze("してる");
  auto surfaces = getSurfaces(result);

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb ||
        mor.pos == core::PartOfSpeech::Auxiliary) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "してる should contain verb component";
}

TEST_F(ContractionStrictTest, Miteta) {
  auto result = analyzer.analyze("見てた");
  auto surfaces = getSurfaces(result);

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface.find("見") != std::string::npos ||
        mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "見てた should contain verb";
}

// ===== Special Symbol Tests =====

class SymbolStrictTest : public AnalyzerTestBase {};

TEST_F(SymbolStrictTest, Brackets) {
  auto result = analyzer.analyze("AI（人工知能）");
  auto surfaces = getSurfaces(result);

  bool found_ai = false;
  bool found_jinkou = false;
  for (const auto& sur : surfaces) {
    std::string lower_sur = sur;
    for (auto& chr : lower_sur) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    if (lower_sur == "ai") found_ai = true;
    if (sur.find("人工") != std::string::npos ||
        sur.find("知能") != std::string::npos) {
      found_jinkou = true;
    }
  }
  EXPECT_TRUE(found_ai) << "Should recognize AI";
  EXPECT_TRUE(found_jinkou) << "Should recognize 人工知能";
}

TEST_F(SymbolStrictTest, QuotationMarks) {
  auto result = analyzer.analyze("「こんにちは」");
  auto surfaces = getSurfaces(result);

  bool found_greeting = false;
  for (const auto& sur : surfaces) {
    if (sur == "こんにちは") {
      found_greeting = true;
      break;
    }
  }
  EXPECT_TRUE(found_greeting) << "Should recognize こんにちは inside quotes";
}

// ===== Complex Mixed Expression Tests =====

class ComplexExpressionStrictTest : public AnalyzerTestBase {};

TEST_F(ComplexExpressionStrictTest, TechnicalWithEnglish) {
  auto result = analyzer.analyze("Pythonで機械学習を実装する");
  auto surfaces = getSurfaces(result);

  bool found_python = false;
  bool found_de = false;
  bool found_wo = false;
  for (const auto& mor : result) {
    std::string lower_sur = mor.surface;
    for (auto& chr : lower_sur) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    if (lower_sur == "python") found_python = true;
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle)
      found_de = true;
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle)
      found_wo = true;
  }
  EXPECT_TRUE(found_python) << "Should recognize Python";
  EXPECT_TRUE(found_de) << "Should recognize で particle";
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
}

TEST_F(ComplexExpressionStrictTest, BusinessRequest) {
  auto result = analyzer.analyze("ご確認をお願いいたします");
  auto surfaces = getSurfaces(result);

  EXPECT_LE(surfaces.size(), 6) << "Should not over-fragment business request";

  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
}

// ===== Real World Sentence Tests =====

class RealWorldSentenceTest : public AnalyzerTestBase {};

TEST_F(RealWorldSentenceTest, BusinessEmail) {
  auto result = analyzer.analyze("お世話になっております");
  auto surfaces = getSurfaces(result);

  ASSERT_FALSE(result.empty());
  EXPECT_LE(surfaces.size(), 5) << "Should not over-fragment";
}

TEST_F(RealWorldSentenceTest, ShoppingConversation) {
  auto result = analyzer.analyze("これはいくらですか");
  auto surfaces = getSurfaces(result);

  bool found_ha = false;
  bool found_ka = false;
  for (const auto& mor : result) {
    if (mor.surface == "は" && mor.pos == core::PartOfSpeech::Particle) {
      found_ha = true;
    }
    if (mor.surface == "か" && mor.pos == core::PartOfSpeech::Particle) {
      found_ka = true;
    }
  }
  EXPECT_TRUE(found_ha) << "Should contain は particle";
  EXPECT_TRUE(found_ka) << "Should contain か particle";
}

TEST_F(RealWorldSentenceTest, WeatherTalk) {
  auto result = analyzer.analyze("今日は暑いですね");
  auto surfaces = getSurfaces(result);

  bool found_today = false;
  for (const auto& sur : surfaces) {
    if (sur == "今日") found_today = true;
  }
  EXPECT_TRUE(found_today) << "Should recognize 今日";
}

TEST_F(RealWorldSentenceTest, TechnicalDoc) {
  auto result = analyzer.analyze("ファイルが見つかりませんでした");
  auto surfaces = getSurfaces(result);

  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が particle";
}

}  // namespace
}  // namespace suzume::analysis
