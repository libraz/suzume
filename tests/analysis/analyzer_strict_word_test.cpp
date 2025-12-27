// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Strict analyzer tests: Verb conjugation, Suffix, Interrogative, Common nouns,
// Lemma correctness

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

// ===== Verb Conjugation Tests =====

class VerbConjugationStrictTest : public AnalyzerTestBase {};

TEST_F(VerbConjugationStrictTest, TaberuLemma) {
  auto result = analyzer.analyze("食べる");
  ASSERT_FALSE(result.empty());

  for (const auto& mor : result) {
    if (mor.surface.find("食べ") != std::string::npos ||
        mor.surface == "食べる") {
      EXPECT_EQ(mor.lemma, "食べる")
          << "食べる lemma should be 食べる, got: " << mor.lemma;
      break;
    }
  }
}

TEST_F(VerbConjugationStrictTest, GohanWoTaberu) {
  auto result = analyzer.analyze("ご飯を食べる");
  auto surfaces = getSurfaces(result);

  EXPECT_GE(surfaces.size(), 2);
  EXPECT_LE(surfaces.size(), 4);

  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb &&
        mor.surface.find("食べ") != std::string::npos) {
      EXPECT_NE(mor.lemma, "食ぶ") << "食べる lemma should not be 食ぶ";
    }
  }
}

// ===== Suffix Attachment Tests =====

class SuffixStrictTest : public AnalyzerTestBase {};

TEST_F(SuffixStrictTest, Tsuke_NotSplit) {
  auto result = analyzer.analyze("付けで");
  auto surfaces = getSurfaces(result);

  bool has_standalone_ke = false;
  for (const auto& sur : surfaces) {
    if (sur == "け") {
      has_standalone_ke = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_ke) << "付け should not split into 付 + け";
}

TEST_F(SuffixStrictTest, HizukeDe) {
  auto result = analyzer.analyze("日付けで");
  auto surfaces = getSurfaces(result);

  bool has_standalone_ke = false;
  for (const auto& sur : surfaces) {
    if (sur == "け") {
      has_standalone_ke = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_ke) << "日付けで: 付け should not split";
}

// ===== Interrogative Tests =====

class InterrogativeStrictTest : public AnalyzerTestBase {};

TEST_F(InterrogativeStrictTest, Ikaga) {
  auto result = analyzer.analyze("いかが");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "いかが should be single token";
  if (surfaces.size() == 1) {
    EXPECT_EQ(surfaces[0], "いかが");
  }
}

TEST_F(InterrogativeStrictTest, Doushite) {
  auto result = analyzer.analyze("どうして");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "どうして should be single token";
}

// ===== Common Noun Tests =====

class CommonNounStrictTest : public AnalyzerTestBase {};

TEST_F(CommonNounStrictTest, Yoroshiku) {
  auto result = analyzer.analyze("よろしく");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "よろしく should be single token";
}

// ===== Lemma Correctness Tests =====

class LemmaCorrectnessTest : public AnalyzerTestBase {};

TEST_F(LemmaCorrectnessTest, GohanLemma) {
  auto result = analyzer.analyze("ご飯");
  ASSERT_FALSE(result.empty());

  for (const auto& mor : result) {
    if (mor.surface == "ご飯") {
      EXPECT_EQ(mor.lemma, "ご飯") << "ご飯 lemma should be ご飯";
      break;
    }
  }
}

TEST_F(LemmaCorrectnessTest, OchaLemma) {
  auto result = analyzer.analyze("お茶");
  ASSERT_FALSE(result.empty());

  for (const auto& mor : result) {
    if (mor.surface == "お茶") {
      EXPECT_EQ(mor.lemma, "お茶") << "お茶 lemma should be お茶";
      break;
    }
  }
}

TEST_F(LemmaCorrectnessTest, KonnichiwaLemma) {
  auto result = analyzer.analyze("こんにちは");
  ASSERT_FALSE(result.empty());

  for (const auto& mor : result) {
    if (mor.surface == "こんにちは") {
      EXPECT_EQ(mor.lemma, "こんにちは") << "こんにちは lemma incorrect";
      break;
    }
  }
}

}  // namespace
}  // namespace suzume::analysis
