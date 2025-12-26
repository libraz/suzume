// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Regression tests for compound nouns and script boundaries

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

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

// =============================================================================
// Regression: Prefix + compound noun (お + 買い物)
// =============================================================================
// Bug: お買い物 was split as お + 買い物 instead of joined
// Fix: Should be recognized as single NOUN token

TEST(AnalyzerTest, Regression_PrefixCompoundNoun) {
  // お買い物 should be a single NOUN token
  Suzume analyzer;
  auto result = analyzer.analyze("お買い物");
  ASSERT_FALSE(result.empty());

  // Should be single token
  EXPECT_EQ(result.size(), 1)
      << "お買い物 should be single token, got " << result.size();

  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "お買い物");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "お買い物 should be Noun";
  }
}

// =============================================================================
// Regression: Compound noun 飲み会
// =============================================================================
// Bug: 飲み会 was split as 飲 + み + 会
// Fix: Should be single NOUN token

TEST(AnalyzerTest, Regression_CompoundNounNomikai) {
  Suzume analyzer;
  auto result = analyzer.analyze("飲み会");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "飲み会 should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "飲み会");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "飲み会 should be Noun";
  }
}

// =============================================================================
// Regression: Compound noun splitting (毎日電車)
// =============================================================================
// Bug: 毎日電車 was analyzed as single unknown token
// Fix: Fixed cost and is_formal_noun flag propagation in split candidates

TEST(AnalyzerTest, Regression_CompoundSplit_MainichiDensha) {
  Suzume analyzer;
  auto result = analyzer.analyze("毎日電車");
  ASSERT_GE(result.size(), 2) << "毎日電車 should split into at least 2 tokens";

  bool found_mainichi = false;
  bool found_densha = false;
  for (const auto& mor : result) {
    if (mor.surface == "毎日") {
      found_mainichi = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "毎日 should be Noun";
    }
    if (mor.surface == "電車") {
      found_densha = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "電車 should be Noun";
    }
  }
  EXPECT_TRUE(found_mainichi) << "毎日 should be found as separate token";
  EXPECT_TRUE(found_densha) << "電車 should be found as separate token";
}

TEST(AnalyzerTest, Regression_CompoundSplit_MainichiDenshaDeKommute) {
  Suzume analyzer;
  auto result = analyzer.analyze("毎日電車で通勤");
  ASSERT_GE(result.size(), 4) << "Should have at least 4 tokens";

  bool found_mainichi = false;
  bool found_densha = false;
  bool found_de = false;
  bool found_tsuukin = false;
  for (const auto& mor : result) {
    if (mor.surface == "毎日") found_mainichi = true;
    if (mor.surface == "電車") found_densha = true;
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle)
      found_de = true;
    if (mor.surface == "通勤") found_tsuukin = true;
  }
  EXPECT_TRUE(found_mainichi) << "毎日 should be found";
  EXPECT_TRUE(found_densha) << "電車 should be found";
  EXPECT_TRUE(found_de) << "で particle should be found";
  EXPECT_TRUE(found_tsuukin) << "通勤 should be found";
}

// =============================================================================
// Regression: Compound nouns (食べ物, 飲み物, 買い物)
// =============================================================================
// Bug: 食べ物 was split as 食 + べ + 物
// Fix: Added these compound nouns to common_vocabulary.h

TEST(AnalyzerTest, Regression_CompoundNoun_Tabemono) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べ物");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "食べ物 should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "食べ物");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "食べ物 should be Noun";
  }
}

TEST(AnalyzerTest, Regression_CompoundNoun_Nomimono) {
  Suzume analyzer;
  auto result = analyzer.analyze("飲み物");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "飲み物 should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "飲み物");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "飲み物 should be Noun";
  }
}

TEST(AnalyzerTest, Regression_CompoundNoun_Kaimono) {
  Suzume analyzer;
  auto result = analyzer.analyze("買い物");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "買い物 should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "買い物");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "買い物 should be Noun";
  }
}

TEST(AnalyzerTest, Regression_CompoundNoun_KaimonoNiIku) {
  Suzume analyzer;
  auto result = analyzer.analyze("買い物に行く");
  ASSERT_GE(result.size(), 3) << "Should have at least 3 tokens";

  bool found_kaimono = false;
  bool found_ni = false;
  bool found_iku = false;
  for (const auto& mor : result) {
    if (mor.surface == "買い物") {
      found_kaimono = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "買い物 should be Noun";
    }
    if (mor.surface == "に" && mor.pos == core::PartOfSpeech::Particle) {
      found_ni = true;
    }
    if (mor.surface == "行く") {
      found_iku = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "行く should be Verb";
    }
  }
  EXPECT_TRUE(found_kaimono) << "買い物 should be found";
  EXPECT_TRUE(found_ni) << "に particle should be found";
  EXPECT_TRUE(found_iku) << "行く should be found";
}

// =============================================================================
// Regression: Noun 楽しみ
// =============================================================================
// Bug: 楽しみ was incorrectly tokenized (楽 + しみ)
// Fix: Should be single NOUN token

TEST(AnalyzerTest, Regression_NounTanoshimi) {
  Suzume analyzer;
  auto result = analyzer.analyze("楽しみ");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "楽しみ should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "楽しみ");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "楽しみ should be Noun";
  }
}

// =============================================================================
// Regression: Nominalized noun recognition (連用形転成名詞)
// =============================================================================
// Bug: 手助け was split into 手助 + け
// Fix: Added nominalized noun candidate generation

TEST(AnalyzerTest, Regression_NominalizedNoun_Tedasuke) {
  Suzume analyzer;
  auto result = analyzer.analyze("手助けをする");
  ASSERT_GE(result.size(), 3);

  bool found_tedasuke = false;
  for (const auto& mor : result) {
    if (mor.surface == "手助け") {
      found_tedasuke = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "手助け should be Noun";
    }
  }
  EXPECT_TRUE(found_tedasuke)
      << "手助け should be found as single token, not split";
}

TEST(AnalyzerTest, Regression_NominalizedNoun_Kiri) {
  // 切り should be nominalized noun, に should be particle
  Suzume analyzer;
  auto result = analyzer.analyze("みじん切りにする");
  ASSERT_GE(result.size(), 3);

  bool found_kiri = false;
  bool found_ni = false;
  for (const auto& mor : result) {
    if (mor.surface == "切り") {
      found_kiri = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "切り should be Noun";
    }
    if (mor.surface == "に") {
      found_ni = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Particle)
          << "に should be Particle";
    }
  }
  EXPECT_TRUE(found_kiri) << "切り should be found as nominalized noun";
  EXPECT_TRUE(found_ni) << "に should be found as particle";
}

// =============================================================================
// Regression: Noun + suffix should not be verb
// =============================================================================
// Bug: 学生たち was parsed as single VERB (立つ conjugation)
// Bug: 子供たち was parsed as single VERB
// Fix: Skip VERB candidate when hiragana suffix is in dictionary as suffix

TEST(AnalyzerTest, Regression_NounPlusTachi) {
  Suzume analyzer;
  auto result = analyzer.analyze("学生たち");
  ASSERT_EQ(result.size(), 2);

  EXPECT_EQ(result[0].surface, "学生");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
      << "学生 should be Noun";
  EXPECT_EQ(result[1].surface, "たち");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Other)
      << "たち should be Other (suffix)";
}

TEST(AnalyzerTest, Regression_NounPlusSan) {
  Suzume analyzer;
  auto result = analyzer.analyze("田中さん");
  ASSERT_EQ(result.size(), 2);

  EXPECT_EQ(result[0].surface, "田中");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
      << "田中 should be Noun";
  EXPECT_EQ(result[1].surface, "さん");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Other)
      << "さん should be Other (suffix)";
}

TEST(AnalyzerTest, Regression_KodomoTachi) {
  Suzume analyzer;
  auto result = analyzer.analyze("子供たちが遊ぶ");
  ASSERT_GE(result.size(), 3);

  EXPECT_EQ(result[0].surface, "子供");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
      << "子供 should be Noun";
  EXPECT_EQ(result[1].surface, "たち");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Other)
      << "たち should be Other (suffix)";
}

// =============================================================================
// Regression: Compound verb patterns (複合動詞パターン)
// =============================================================================
// Bug: 読み終わったら was parsed as 読み(NOUN) + 終わったら(VERB)
// Fix: Viterbi (position, POS) pair tracking allows VERB path to survive
//      until connection costs determine the winner

TEST(AnalyzerTest, Regression_CompoundVerb_YomiOwattara) {
  // 読み終わったら should be 読み(VERB renyokei) + 終わったら(compound aux)
  Suzume analyzer;
  auto result = analyzer.analyze("読み終わったら");
  ASSERT_EQ(result.size(), 2) << "読み終わったら should have 2 tokens";

  EXPECT_EQ(result[0].surface, "読み");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "読み should be Verb (renyokei), not Noun";
  EXPECT_EQ(result[0].lemma, "読む") << "読み lemma should be 読む";

  EXPECT_EQ(result[1].surface, "終わったら");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Verb)
      << "終わったら should be Verb";
  EXPECT_EQ(result[1].lemma, "終わる") << "終わったら lemma should be 終わる";
}

TEST(AnalyzerTest, Regression_CompoundVerb_YomiTsuzukeru) {
  // 読み続ける can be single token or split - either is acceptable
  // The key is that if split, 読み should be VERB not NOUN
  Suzume analyzer;
  auto result = analyzer.analyze("読み続ける");
  ASSERT_GE(result.size(), 1) << "読み続ける should have at least 1 token";

  // Check first token
  if (result.size() == 1) {
    // Single token case
    EXPECT_EQ(result[0].surface, "読み続ける");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
        << "読み続ける should be Verb";
  } else {
    // Split case - 読み should be VERB not NOUN
    EXPECT_EQ(result[0].surface, "読み");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
        << "読み should be Verb (renyokei), not Noun";
  }
}

TEST(AnalyzerTest, Regression_CompoundVerb_ArukinagaraHanasu) {
  // 歩きながら話す should keep ながら form intact
  Suzume analyzer;
  auto result = analyzer.analyze("歩きながら話す");
  ASSERT_GE(result.size(), 2) << "歩きながら話す should have at least 2 tokens";

  bool found_arukinagara = false;
  for (const auto& mor : result) {
    if (mor.surface == "歩きながら") {
      found_arukinagara = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "歩きながら should be Verb";
      EXPECT_EQ(mor.lemma, "歩く") << "歩きながら lemma should be 歩く";
    }
  }
  EXPECT_TRUE(found_arukinagara) << "歩きながら should be found as single token";
}

TEST(AnalyzerTest, Regression_CompoundVerb_TabenagaraAruku) {
  // 食べながら歩く should keep ながら form intact (Ichidan verb)
  Suzume analyzer;
  auto result = analyzer.analyze("食べながら歩く");
  ASSERT_GE(result.size(), 2) << "食べながら歩く should have at least 2 tokens";

  bool found_tabenagara = false;
  for (const auto& mor : result) {
    if (mor.surface == "食べながら") {
      found_tabenagara = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "食べながら should be Verb";
      EXPECT_EQ(mor.lemma, "食べる") << "食べながら lemma should be 食べる";
    }
  }
  EXPECT_TRUE(found_tabenagara) << "食べながら should be found as single token";
}

// =============================================================================
// Regression: Viterbi (position, POS) pair tracking
// =============================================================================
// These tests verify that the Viterbi algorithm correctly handles cases where
// multiple POS candidates exist at the same position, allowing connection
// costs to determine the optimal path.

TEST(AnalyzerTest, Regression_Viterbi_RenyokeiVsNoun_Sou) {
  // Verify VERB renyokei + そう is preferred over NOUN + そう
  // 降りそう: 降り should be VERB (renyokei of 降りる), not NOUN
  Suzume analyzer;
  auto result = analyzer.analyze("降りそう");
  ASSERT_GE(result.size(), 2) << "降りそう should have at least 2 tokens";

  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "降り should be Verb (renyokei), not Noun";
}

TEST(AnalyzerTest, Regression_Viterbi_TeFormNotSplit) {
  // Te-form should not be split: 走って should be single token
  Suzume analyzer;
  auto result = analyzer.analyze("走っている");
  ASSERT_GE(result.size(), 1) << "走っている should have tokens";

  // Check that 走っ is not split from て
  bool found_hashitte = false;
  for (const auto& mor : result) {
    if (mor.surface == "走って" || mor.surface == "走っている") {
      found_hashitte = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "走って/走っている should be Verb";
    }
  }
  EXPECT_TRUE(found_hashitte)
      << "走って or 走っている should be found (not split as 走っ + て)";
}

TEST(AnalyzerTest, Regression_Viterbi_MultiMorphemePreference) {
  // When costs are equal, prefer fewer morphemes (longer tokens)
  // いつも should be single ADV, not いつ + も
  Suzume analyzer;
  auto result = analyzer.analyze("いつも来る");
  ASSERT_GE(result.size(), 2) << "いつも来る should have at least 2 tokens";

  EXPECT_EQ(result[0].surface, "いつも")
      << "いつも should be single token, not split as いつ+も";
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb)
      << "いつも should be Adverb";
}

// =============================================================================
// Compound verbs with hiragana V2 (ひらがな補助動詞)
// =============================================================================
// Moved from analyzer_regression_verb_test.cpp
// Bug: Compound verbs written with hiragana V2 (走りだす, 飛びこむ) were not
// recognized
// Fix: Added reading field to SubsidiaryVerb struct to match both kanji and
// hiragana

TEST(AnalyzerTest, Regression_CompoundVerb_HiraganaV2_Dasu) {
  Suzume analyzer;
  auto result = analyzer.analyze("走りだしたくなかった");

  // Should recognize compound verb with hiragana だし
  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "走りだし");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "走り出す");  // Lemma uses kanji form
}

TEST(AnalyzerTest, Regression_CompoundVerb_HiraganaV2_Komu) {
  Suzume analyzer;
  auto result = analyzer.analyze("飛びこみたい");

  // Should recognize compound verb with hiragana こみ
  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "飛びこみ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "飛び込む");  // Lemma uses kanji form
}

TEST(AnalyzerTest, Regression_CompoundVerb_HiraganaV2_Sugiru) {
  Suzume analyzer;
  auto result = analyzer.analyze("読みすぎた");

  // Should recognize compound verb with hiragana すぎ
  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "読みすぎ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "読み過ぎる");  // Lemma uses kanji form
}

// Verify kanji V2 still works
TEST(AnalyzerTest, Regression_CompoundVerb_KanjiV2_Dasu) {
  Suzume analyzer;
  auto result = analyzer.analyze("走り出したくなかった");

  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "走り出し");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "走り出す");
}

// =============================================================================
// All-hiragana compound verbs (全ひらがな複合動詞)
// =============================================================================
// Moved from analyzer_regression_verb_test.cpp
// Bug: All-hiragana compound verbs (やりなおす, わかりあう) were not recognized
// Fix: Added addHiraganaCompoundVerbJoinCandidates function

TEST(AnalyzerTest, Regression_HiraganaCompoundVerb_YariNaosu) {
  Suzume analyzer;
  auto result = analyzer.analyze("やりなおしたい");

  // Should recognize やりなおし as compound verb
  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "やりなおし");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "やり直す");  // Lemma uses kanji V2
}

TEST(AnalyzerTest, Regression_HiraganaCompoundVerb_WakariAu) {
  Suzume analyzer;
  auto result = analyzer.analyze("わかりあう");

  // Should recognize as compound verb
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "わかりあう");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "わかり合う");  // Lemma uses kanji V2
}

}  // namespace
}  // namespace suzume::analysis
