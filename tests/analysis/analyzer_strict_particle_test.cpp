// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Strict analyzer tests: Compound particles, Pronouns, Number+counter,
// Sentence ending particles

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// Helper: Get surface forms as vector
std::vector<std::string> getSurfaces(const std::vector<core::Morpheme>& result) {
  std::vector<std::string> surfaces;
  surfaces.reserve(result.size());
  for (const auto& mor : result) {
    surfaces.push_back(mor.surface);
  }
  return surfaces;
}

// Base class for tests that need core dictionary
class AnalyzerTestBase : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};

  void SetUp() override {
    analyzer.tryAutoLoadCoreDictionary();
  }
};

// ===== Compound Particle Tests =====

class CompoundParticleStrictTest : public AnalyzerTestBase {};

TEST_F(CompoundParticleStrictTest, Nitsuite) {
  auto result = analyzer.analyze("日本について");
  auto surfaces = getSurfaces(result);

  bool found_nitsuite = false;
  for (const auto& sur : surfaces) {
    if (sur == "について") {
      found_nitsuite = true;
      break;
    }
  }
  EXPECT_TRUE(found_nitsuite) << "Should recognize について as compound particle";
}

TEST_F(CompoundParticleStrictTest, Niyotte) {
  auto result = analyzer.analyze("風によって");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "によって") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize によって as compound particle";
}

TEST_F(CompoundParticleStrictTest, Toshite) {
  auto result = analyzer.analyze("代表として");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "として") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize として as compound particle";
}

TEST_F(CompoundParticleStrictTest, Nitaishite) {
  auto result = analyzer.analyze("彼に対して");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "に対して") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize に対して as compound particle";
}

// ===== Pronoun Tests =====

class PronounStrictTest : public AnalyzerTestBase {};

TEST_F(PronounStrictTest, Demonstrative_Kore) {
  auto result = analyzer.analyze("これを見て");
  auto surfaces = getSurfaces(result);

  bool found_kore = false;
  bool found_wo = false;
  for (const auto& sur : surfaces) {
    if (sur == "これ") found_kore = true;
    if (sur == "を") found_wo = true;
  }
  EXPECT_TRUE(found_kore) << "Should recognize これ as pronoun";
  EXPECT_TRUE(found_wo) << "Should recognize を as particle";
}

TEST_F(PronounStrictTest, Demonstrative_Sore) {
  auto result = analyzer.analyze("それは何ですか");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "それ") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize それ as pronoun";
}

TEST_F(PronounStrictTest, Demonstrative_Are) {
  auto result = analyzer.analyze("あれが欲しい");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "あれ") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize あれ as pronoun";
}

TEST_F(PronounStrictTest, Interrogative_Doko) {
  auto result = analyzer.analyze("どこに行く");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "どこ") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize どこ as interrogative pronoun";
}

// ===== Number + Counter Tests =====

class NumberCounterStrictTest : public AnalyzerTestBase {};

TEST_F(NumberCounterStrictTest, ThreePeople) {
  auto result = analyzer.analyze("3人で行く");
  auto surfaces = getSurfaces(result);

  EXPECT_GE(surfaces.size(), 2);
  EXPECT_LE(surfaces.size(), 5);

  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle) {
      found_de = true;
    }
  }
  EXPECT_TRUE(found_de) << "Should recognize で particle";
}

TEST_F(NumberCounterStrictTest, HundredYen) {
  auto result = analyzer.analyze("100円の商品");
  auto surfaces = getSurfaces(result);

  bool found_no = false;
  for (const auto& mor : result) {
    if (mor.surface == "の" && mor.pos == core::PartOfSpeech::Particle) {
      found_no = true;
    }
  }
  EXPECT_TRUE(found_no) << "Should recognize の particle";
}

// ===== Sentence Ending Particle Tests =====

class SentenceEndingStrictTest : public AnalyzerTestBase {};

TEST_F(SentenceEndingStrictTest, Kana) {
  auto result = analyzer.analyze("行くかな");
  auto surfaces = getSurfaces(result);

  bool found_particle = false;
  for (const auto& mor : result) {
    if ((mor.surface == "か" || mor.surface == "な" || mor.surface == "かな") &&
        mor.pos == core::PartOfSpeech::Particle) {
      found_particle = true;
      break;
    }
  }
  EXPECT_TRUE(found_particle) << "Should recognize sentence-ending particle(s)";
}

TEST_F(SentenceEndingStrictTest, Yone) {
  auto result = analyzer.analyze("いいよね");
  auto surfaces = getSurfaces(result);

  bool found_particle = false;
  for (const auto& mor : result) {
    if ((mor.surface == "よ" || mor.surface == "ね" || mor.surface == "よね") &&
        mor.pos == core::PartOfSpeech::Particle) {
      found_particle = true;
      break;
    }
  }
  EXPECT_TRUE(found_particle) << "Should recognize sentence-ending particle(s)";
}

}  // namespace
}  // namespace suzume::analysis
