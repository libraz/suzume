// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Strict analyzer tests: Greetings, Honorific prefixes, Business phrases, Keigo

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
    // Load core binary dictionary for L2 entries
    analyzer.tryAutoLoadCoreDictionary();
  }
};

// ===== Greeting Tests =====

class GreetingStrictTest : public AnalyzerTestBase {};

TEST_F(GreetingStrictTest, Konnichiwa) {
  auto result = analyzer.analyze("こんにちは");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1)
      << "こんにちは should be single token, got: " << surfaces.size()
      << " tokens";
  if (surfaces.size() == 1) {
    EXPECT_EQ(surfaces[0], "こんにちは");
  }
}

TEST_F(GreetingStrictTest, Ohayougozaimasu) {
  auto result = analyzer.analyze("おはようございます");
  auto surfaces = getSurfaces(result);

  EXPECT_LE(surfaces.size(), 2)
      << "おはようございます should be at most 2 tokens, got: "
      << surfaces.size();
}

TEST_F(GreetingStrictTest, Arigatougozaimasu) {
  auto result = analyzer.analyze("ありがとうございます");
  auto surfaces = getSurfaces(result);

  EXPECT_LE(surfaces.size(), 2)
      << "ありがとうございます should be at most 2 tokens, got: "
      << surfaces.size();
}

TEST_F(GreetingStrictTest, Sumimasen) {
  auto result = analyzer.analyze("すみません");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "すみません should be single token";
  if (!result.empty()) {
    EXPECT_TRUE(result[0].lemma == "すみません" || result[0].lemma == "すむ" ||
                result[0].lemma == "済む")
        << "すみません lemma incorrect: " << result[0].lemma;
  }
}

// ===== Honorific Prefix Tests =====

class HonorificPrefixTest : public AnalyzerTestBase {};

TEST_F(HonorificPrefixTest, Ocha) {
  auto result = analyzer.analyze("お茶");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "お茶 should be single token";
  if (surfaces.size() == 1) {
    EXPECT_EQ(surfaces[0], "お茶");
  }
}

TEST_F(HonorificPrefixTest, Gohan) {
  auto result = analyzer.analyze("ご飯");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "ご飯 should be single token";
  if (surfaces.size() == 1) {
    EXPECT_EQ(surfaces[0], "ご飯");
  }
}

TEST_F(HonorificPrefixTest, Onegai) {
  auto result = analyzer.analyze("お願い");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "お願い should be single token";
}

TEST_F(HonorificPrefixTest, Otsukareama) {
  auto result = analyzer.analyze("お疲れ様");
  auto surfaces = getSurfaces(result);

  EXPECT_LE(surfaces.size(), 2)
      << "お疲れ様 should be at most 2 tokens, got: " << surfaces.size();
}

TEST_F(HonorificPrefixTest, OsewaNiNatteOrimasu) {
  auto result = analyzer.analyze("お世話になっております");
  auto surfaces = getSurfaces(result);

  bool found_osewa = false;
  for (const auto& sur : surfaces) {
    if (sur == "お世話" || sur == "世話") {
      found_osewa = true;
      break;
    }
  }
  EXPECT_TRUE(found_osewa) << "Should contain お世話 or 世話";
}

// ===== Business Phrase Tests =====

class BusinessPhraseStrictTest : public AnalyzerTestBase {};

TEST_F(BusinessPhraseStrictTest, YoroshikuOnegaiitashimasu) {
  auto result = analyzer.analyze("よろしくお願いいたします");
  auto surfaces = getSurfaces(result);

  bool found_yoroshiku = false;
  bool found_onegai = false;
  for (const auto& sur : surfaces) {
    if (sur.find("よろしく") != std::string::npos) found_yoroshiku = true;
    if (sur.find("願") != std::string::npos) found_onegai = true;
  }
  EXPECT_TRUE(found_yoroshiku) << "Should contain よろしく";
  EXPECT_TRUE(found_onegai) << "Should contain お願い/願い";
}

TEST_F(BusinessPhraseStrictTest, Ikagadeshouka) {
  auto result = analyzer.analyze("いかがでしょうか");
  auto surfaces = getSurfaces(result);

  bool found_ikaga = false;
  for (const auto& sur : surfaces) {
    if (sur == "いかが") {
      found_ikaga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ikaga) << "Should contain いかが as single token";
}

// ===== Keigo Expression Tests =====

class KeigoStrictTest : public AnalyzerTestBase {};

TEST_F(KeigoStrictTest, Gozaimasu) {
  auto result = analyzer.analyze("ございます");
  auto surfaces = getSurfaces(result);

  EXPECT_LE(surfaces.size(), 2) << "ございます should not be over-fragmented";
}

TEST_F(KeigoStrictTest, OtsukareSama) {
  auto result = analyzer.analyze("お疲れ様です");
  auto surfaces = getSurfaces(result);

  EXPECT_LE(surfaces.size(), 4) << "Should not over-fragment business greeting";
}

}  // namespace
}  // namespace suzume::analysis
