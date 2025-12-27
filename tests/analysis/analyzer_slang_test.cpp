// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Slang and internet term analyzer tests (スラング・ネット用語)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// Helper to check if morpheme exists with expected POS
bool hasMorpheme(const std::vector<core::Morpheme>& result,
                 const std::string& surface, core::PartOfSpeech pos) {
  for (const auto& mor : result) {
    if (mor.surface == surface && mor.pos == pos) {
      return true;
    }
  }
  return false;
}


// ===== Katakana Verb Slang Tests (カタカナ動詞スラング) =====

class KatakanaVerbSlangTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(KatakanaVerbSlangTest, Bazuru_BaseForm) {
  // バズる (to go viral)
  auto result = analyzer.analyze("バズる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasMorpheme(result, "バズる", core::PartOfSpeech::Verb))
      << "バズる should be recognized as a verb";
}

TEST_F(KatakanaVerbSlangTest, Bazuru_PastForm) {
  // バズった (went viral)
  auto result = analyzer.analyze("バズった");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "バズった");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "バズる") << "Lemma should be バズる";
}

TEST_F(KatakanaVerbSlangTest, Bazuru_PotentialForm) {
  // バズれる (can go viral)
  auto result = analyzer.analyze("バズれる");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "バズれる");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "バズる") << "Lemma should be バズる";
}

TEST_F(KatakanaVerbSlangTest, Saboru_BaseForm) {
  // サボる (to slack off)
  auto result = analyzer.analyze("サボる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasMorpheme(result, "サボる", core::PartOfSpeech::Verb))
      << "サボる should be recognized as a verb";
}

TEST_F(KatakanaVerbSlangTest, Saboru_TeForm) {
  // サボって (slacking off)
  auto result = analyzer.analyze("サボって");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "サボって");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "サボる") << "Lemma should be サボる";
}

TEST_F(KatakanaVerbSlangTest, Guguru_BaseForm) {
  // ググる (to google)
  auto result = analyzer.analyze("ググる");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "ググる");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
}

TEST_F(KatakanaVerbSlangTest, Guguru_NegativeForm) {
  // ググらない (not googling)
  auto result = analyzer.analyze("ググらない");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "ググらない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "ググる") << "Lemma should be ググる";
}

TEST_F(KatakanaVerbSlangTest, Pakuru_BaseForm) {
  // パクる (to steal/copy)
  auto result = analyzer.analyze("パクる");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "パクる");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
}

TEST_F(KatakanaVerbSlangTest, Neguru_BaseForm) {
  // ネグる (to neglect)
  auto result = analyzer.analyze("ネグる");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "ネグる");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
}

// ===== Katakana Adjective Slang Tests (カタカナ形容詞スラング) =====

class KatakanaAdjectiveSlangTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(KatakanaAdjectiveSlangTest, Emoi_BaseForm) {
  // エモい (emotional/evocative)
  auto result = analyzer.analyze("エモい");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "エモい");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
}

TEST_F(KatakanaAdjectiveSlangTest, Emoi_PastForm) {
  // エモかった (was emotional)
  auto result = analyzer.analyze("エモかった");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "エモかった");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "エモい") << "Lemma should be エモい";
}

TEST_F(KatakanaAdjectiveSlangTest, Emoi_NegativeForm) {
  // エモくない (not emotional)
  auto result = analyzer.analyze("エモくない");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "エモくない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "エモい") << "Lemma should be エモい";
}

TEST_F(KatakanaAdjectiveSlangTest, Kimoi_BaseForm) {
  // キモい (gross/creepy)
  auto result = analyzer.analyze("キモい");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "キモい");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
}

TEST_F(KatakanaAdjectiveSlangTest, Kimoi_PastForm) {
  // キモかった (was gross)
  auto result = analyzer.analyze("キモかった");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "キモかった");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "キモい") << "Lemma should be キモい";
}

TEST_F(KatakanaAdjectiveSlangTest, Uzai_BaseForm) {
  // ウザい (annoying)
  auto result = analyzer.analyze("ウザい");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "ウザい");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
}

TEST_F(KatakanaAdjectiveSlangTest, Dasai_BaseForm) {
  // ダサい (uncool/lame)
  auto result = analyzer.analyze("ダサい");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "ダサい");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
}

TEST_F(KatakanaAdjectiveSlangTest, Dasai_AdverbialForm) {
  // ダサく (uncoolly)
  auto result = analyzer.analyze("ダサく");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "ダサく");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "ダサい") << "Lemma should be ダサい";
}

// ===== Combined Slang Phrase Tests (スラングフレーズ) =====

class SlangPhraseTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(SlangPhraseTest, MecchaEmoi) {
  // めっちゃエモい (super emotional)
  auto result = analyzer.analyze("めっちゃエモい");
  ASSERT_EQ(result.size(), 2) << "Should be two tokens";
  EXPECT_EQ(result[0].surface, "めっちゃ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb);
  EXPECT_EQ(result[1].surface, "エモい");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Adjective);
}

TEST_F(SlangPhraseTest, ChoBazutta) {
  // 超バズった (super went viral)
  auto result = analyzer.analyze("超バズった");
  ASSERT_EQ(result.size(), 2) << "Should be two tokens";
  EXPECT_EQ(result[0].surface, "超");
  EXPECT_EQ(result[1].surface, "バズった");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[1].lemma, "バズる");
}

TEST_F(SlangPhraseTest, MajiDeKimoi) {
  // マジでキモい (seriously gross)
  auto result = analyzer.analyze("マジでキモい");
  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";
  // Find キモい in the result
  bool found_kimoi = false;
  for (const auto& mor : result) {
    if (mor.surface == "キモい" && mor.pos == core::PartOfSpeech::Adjective) {
      found_kimoi = true;
      break;
    }
  }
  EXPECT_TRUE(found_kimoi) << "キモい should be recognized as adjective";
}

TEST_F(SlangPhraseTest, GachiDeSaboru) {
  // ガチでサボる (seriously slacking off)
  auto result = analyzer.analyze("ガチでサボる");
  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";
  // Find サボる in the result
  bool found_saboru = false;
  for (const auto& mor : result) {
    if (mor.surface == "サボる" && mor.pos == core::PartOfSpeech::Verb) {
      found_saboru = true;
      break;
    }
  }
  EXPECT_TRUE(found_saboru) << "サボる should be recognized as verb";
}

// ===== Slang Verb Conjugation Tests (スラング動詞活用) =====

class SlangVerbConjugationTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(SlangVerbConjugationTest, Bazuru_Causative) {
  // バズらせる (make go viral)
  auto result = analyzer.analyze("バズらせる");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "バズらせる");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "バズる") << "Lemma should be バズる";
}

TEST_F(SlangVerbConjugationTest, Saboru_Passive) {
  // サボられる (be slacked off on)
  auto result = analyzer.analyze("サボられる");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "サボられる");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "サボる") << "Lemma should be サボる";
}

TEST_F(SlangVerbConjugationTest, Guguru_Volitional) {
  // ググろう (let's google)
  auto result = analyzer.analyze("ググろう");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "ググろう");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "ググる") << "Lemma should be ググる";
}

// ===== Slang Adjective Conjugation Tests (スラング形容詞活用) =====

class SlangAdjectiveConjugationTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(SlangAdjectiveConjugationTest, Emoi_Conditional) {
  // エモければ (if emotional)
  auto result = analyzer.analyze("エモければ");
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(result.size(), 1) << "Should be single token";
  EXPECT_EQ(result[0].surface, "エモければ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "エモい") << "Lemma should be エモい";
}

TEST_F(SlangAdjectiveConjugationTest, Kimoi_Sou) {
  // キモさそう (looks gross)
  auto result = analyzer.analyze("キモさそう");
  ASSERT_FALSE(result.empty());
  // Note: This pattern may be parsed as キモ + さそう or キモさ + そう
  // Just ensure it doesn't crash and produces some output
}

TEST_F(SlangAdjectiveConjugationTest, Uzai_Te) {
  // ウザくて (being annoying and...)
  // Note: Te-form of adjectives is split into 連用形 + て particle
  auto result = analyzer.analyze("ウザくて");
  ASSERT_EQ(result.size(), 2) << "Te-form split into ADJ + て";
  EXPECT_EQ(result[0].surface, "ウザく");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "ウザい") << "Lemma should be ウザい";
  EXPECT_EQ(result[1].surface, "て");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
}

}  // namespace
}  // namespace suzume::analysis
