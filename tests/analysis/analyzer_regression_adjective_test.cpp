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

// Regression: Benefactive te-form should split correctly
TEST(AnalyzerTest, Regression_TeMorau_Separate) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べてもらわない");
  ASSERT_GE(result.size(), 2);

  bool found_tabete = false;
  bool found_morawanai = false;
  for (const auto& mor : result) {
    if (mor.surface == "食べて" && mor.pos == core::PartOfSpeech::Verb) {
      found_tabete = true;
    }
    if (mor.surface == "もらわない" && mor.pos == core::PartOfSpeech::Verb) {
      found_morawanai = true;
    }
  }
  EXPECT_TRUE(found_tabete) << "食べて should be recognized as verb";
  EXPECT_TRUE(found_morawanai) << "もらわない should be recognized as verb";
}

// Regression: Progressive negative should stay unified
TEST(AnalyzerTest, Regression_TeInai_Unified) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べていない");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "食べていない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "食べる");
}

// Regression: Aspectual te-form negatives should stay unified
TEST(AnalyzerTest, Regression_TeShimawanai_Unified) {
  Suzume analyzer;
  auto result = analyzer.analyze("忘れてしまわない");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "忘れてしまわない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "忘れる");
}

TEST(AnalyzerTest, Regression_TeIkanai_Unified) {
  Suzume analyzer;
  auto result = analyzer.analyze("走っていかない");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "走っていかない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "走る");
}

// Regression: Benefactive positive forms should stay unified
TEST(AnalyzerTest, Regression_TeAgeru_Unified) {
  Suzume analyzer;
  auto result = analyzer.analyze("見てあげる");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "見てあげる");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "見る");
}

// Regression: Godan verb + benefactive negative should split correctly
TEST(AnalyzerTest, Regression_GodanTeAgenai_Split) {
  Suzume analyzer;
  auto result = analyzer.analyze("書いてあげない");
  ASSERT_GE(result.size(), 2);

  bool found_kaite = false;
  bool found_agenai = false;
  for (const auto& mor : result) {
    if (mor.surface == "書いて" && mor.pos == core::PartOfSpeech::Verb) {
      found_kaite = true;
      EXPECT_EQ(mor.lemma, "書く");
    }
    if (mor.surface == "あげない" && mor.pos == core::PartOfSpeech::Verb) {
      found_agenai = true;
      EXPECT_EQ(mor.lemma, "あげる");
    }
  }
  EXPECT_TRUE(found_kaite) << "書いて should be recognized as verb";
  EXPECT_TRUE(found_agenai) << "あげない should be recognized as verb";
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

// =============================================================================
// Regression: I-adjective + そう vs Suru verb + そう disambiguation
// =============================================================================
// Bug: 美味しそう was incorrectly analyzed as verb (美味する + そう)
// Fix: Check all inflection candidates, not just the best one;
//      Added 美味しい to L2 dictionary as I_ADJ

TEST(AnalyzerTest, Regression_IAdjectiveSou_Oishisou) {
  // 美味しそう should be recognized as adjective with lemma 美味しい
  Suzume analyzer;
  auto result = analyzer.analyze("美味しそう");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "美味しそう should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "美味しそう");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "美味しそう should be Adjective, not Verb";
    EXPECT_EQ(result[0].lemma, "美味しい")
        << "美味しそう lemma should be 美味しい";
  }
}

TEST(AnalyzerTest, Regression_IAdjectiveSou_Kanashisou) {
  // 悲しそう should be recognized as adjective with lemma 悲しい
  Suzume analyzer;
  auto result = analyzer.analyze("悲しそう");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "悲しそう should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "悲しそう");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "悲しそう should be Adjective";
    EXPECT_EQ(result[0].lemma, "悲しい")
        << "悲しそう lemma should be 悲しい";
  }
}

TEST(AnalyzerTest, Regression_IAdjectiveSou_InSentence) {
  // 美味しそうに食べている - 美味しそう should be adjective
  Suzume analyzer;
  auto result = analyzer.analyze("美味しそうに食べている");
  ASSERT_GE(result.size(), 3);

  bool found_oishisou = false;
  for (const auto& mor : result) {
    if (mor.surface == "美味しそう") {
      found_oishisou = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "美味しそう should be Adjective";
      EXPECT_EQ(mor.lemma, "美味しい")
          << "美味しそう lemma should be 美味しい";
    }
  }
  EXPECT_TRUE(found_oishisou) << "美味しそう should be found";
}

TEST(AnalyzerTest, Regression_SuruVerbSou_ChikokuShisou) {
  // 遅刻しそう should be segmented as 遅刻 (noun) + しそう (verb)
  // This is the correct analysis for SURU nouns
  Suzume analyzer;
  auto result = analyzer.analyze("遅刻しそう");
  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";

  bool found_chikoku = false;
  bool found_shisou = false;
  for (const auto& mor : result) {
    if (mor.surface == "遅刻" && mor.pos == core::PartOfSpeech::Noun) {
      found_chikoku = true;
    }
    if (mor.surface == "しそう" && mor.pos == core::PartOfSpeech::Verb) {
      found_shisou = true;
      EXPECT_EQ(mor.lemma, "する") << "しそう lemma should be する";
    }
  }
  EXPECT_TRUE(found_chikoku) << "遅刻 should be recognized as noun";
  EXPECT_TRUE(found_shisou) << "しそう should be recognized as verb";
}

// =============================================================================
// Regression: し+そう disambiguation (verb renyokei vs adjective stem)
// =============================================================================
// Bug: 話しそう was incorrectly analyzed as adjective (話しい + そう)
// Fix: Added dictionary validation for し+そう patterns - only generate
//      adjective candidate if base form (kanji + しい) exists in dictionary

TEST(AnalyzerTest, Regression_ShiSou_HanashiSou_Verb) {
  // 話しそう should be 話す (verb) + そう, NOT 話しい (adjective)
  // 話しい is not a valid adjective in Japanese
  Suzume analyzer;
  auto result = analyzer.analyze("話しそう");
  ASSERT_GE(result.size(), 2) << "話しそう should have at least 2 tokens";

  bool found_hanashi = false;
  bool found_sou = false;
  for (const auto& mor : result) {
    if (mor.surface == "話し" && mor.pos == core::PartOfSpeech::Verb) {
      found_hanashi = true;
      EXPECT_EQ(mor.lemma, "話す") << "話し lemma should be 話す";
    }
    if (mor.surface == "そう" && mor.pos == core::PartOfSpeech::Adverb) {
      found_sou = true;
    }
  }
  EXPECT_TRUE(found_hanashi) << "話し should be recognized as verb (renyokei)";
  EXPECT_TRUE(found_sou) << "そう should be recognized as adverb";
}

TEST(AnalyzerTest, Regression_ShiSou_MuzukashiSou_Adjective) {
  // 難しそう should be adjective with lemma 難しい
  // 難しい IS a valid adjective in dictionary
  Suzume analyzer;
  auto result = analyzer.analyze("難しそう");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "難しそう should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "難しそう");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "難しそう should be Adjective";
    EXPECT_EQ(result[0].lemma, "難しい")
        << "難しそう lemma should be 難しい";
  }
}

TEST(AnalyzerTest, Regression_ShiSou_TanoshiSou_Adjective) {
  // 楽しそう should be adjective with lemma 楽しい
  Suzume analyzer;
  auto result = analyzer.analyze("楽しそう");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "楽しそう should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "楽しそう");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "楽しそう should be Adjective";
    EXPECT_EQ(result[0].lemma, "楽しい")
        << "楽しそう lemma should be 楽しい";
  }
}

TEST(AnalyzerTest, Regression_ShiSou_TameshiSou_Verb) {
  // 試しそう should be 試す (verb) + そう, NOT 試しい (adjective)
  // 試しい is not a valid adjective in Japanese
  Suzume analyzer;
  auto result = analyzer.analyze("試しそう");
  ASSERT_GE(result.size(), 2) << "試しそう should have at least 2 tokens";

  bool found_tameshi = false;
  bool found_sou = false;
  for (const auto& mor : result) {
    if (mor.surface == "試し" && mor.pos == core::PartOfSpeech::Verb) {
      found_tameshi = true;
      EXPECT_EQ(mor.lemma, "試す") << "試し lemma should be 試す";
    }
    if (mor.surface == "そう" && mor.pos == core::PartOfSpeech::Adverb) {
      found_sou = true;
    }
  }
  EXPECT_TRUE(found_tameshi) << "試し should be recognized as verb (renyokei)";
  EXPECT_TRUE(found_sou) << "そう should be recognized as adverb";
}

TEST(AnalyzerTest, Regression_ShiSou_UreshiSou_Adjective) {
  // 嬉しそう should be adjective with lemma 嬉しい
  Suzume analyzer;
  auto result = analyzer.analyze("嬉しそう");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "嬉しそう should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "嬉しそう");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "嬉しそう should be Adjective";
    EXPECT_EQ(result[0].lemma, "嬉しい")
        << "嬉しそう lemma should be 嬉しい";
  }
}

// =============================================================================
// Regression: 〜やすい auxiliary vs 安い adjective
// =============================================================================
// Bug: 読みやすい in context was split as 読み (noun) + やすい (安い)
// Fix: Added connection cost penalty for やすい (安い) after verb renyokei-like nouns

TEST(AnalyzerTest, Regression_Yasui_YomiYasui_Context) {
  // この本は読みやすい - should be 読みやすい (easy to read), not 読み + 安い
  Suzume analyzer;
  auto result = analyzer.analyze("この本は読みやすい");
  ASSERT_GE(result.size(), 4);

  bool found_yomiyasui = false;
  for (const auto& mor : result) {
    if (mor.surface == "読みやすい" && mor.pos == core::PartOfSpeech::Adjective) {
      found_yomiyasui = true;
    }
  }
  EXPECT_TRUE(found_yomiyasui)
      << "読みやすい should be single adjective (easy to read)";
}

TEST(AnalyzerTest, Regression_Yasui_Yasui_Standalone) {
  // この服は安い - should be 安い (cheap) as standalone adjective
  Suzume analyzer;
  auto result = analyzer.analyze("この服は安い");
  ASSERT_GE(result.size(), 4);

  bool found_yasui = false;
  for (const auto& mor : result) {
    if (mor.surface == "安い" && mor.lemma == "安い" &&
        mor.pos == core::PartOfSpeech::Adjective) {
      found_yasui = true;
    }
  }
  EXPECT_TRUE(found_yasui) << "安い should be recognized as cheap adjective";
}

// =============================================================================
// Regression: 〜なければ conditional not adjective
// =============================================================================
// Bug: 行かなければ was incorrectly analyzed as adjective (行かない + ければ)
// Fix: Added a-row hiragana to penalty check in inflection_scorer.cpp
//      and added penalty for short な-ending stems (しな, 来な)

TEST(AnalyzerTest, Regression_Nakereba_IkaNakereba_Verb) {
  // 行かなければ should be 行く (verb), not 行かない (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("行かなければ");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "行かなければ" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "行く") << "行かなければ lemma should be 行く";
    }
  }
  EXPECT_TRUE(found_verb) << "行かなければ should be recognized as verb";
}

TEST(AnalyzerTest, Regression_Nakereba_ShiNakereba_Verb) {
  // しなければ should be する (verb), not しない (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("しなければならない");
  ASSERT_FALSE(result.empty());

  bool found_suru = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb && mor.lemma == "する") {
      found_suru = true;
    }
  }
  EXPECT_TRUE(found_suru) << "しなければならない should contain する verb";
}

TEST(AnalyzerTest, Regression_Nakereba_KoNakereba_Verb) {
  // 来なければ should be 来る (verb), not 来ない (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("来なければ");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "来なければ" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "来る") << "来なければ lemma should be 来る";
    }
  }
  EXPECT_TRUE(found_verb) << "来なければ should be recognized as verb";
}

TEST(AnalyzerTest, Regression_Nakereba_KakaNakereba_Verb) {
  // 書かなければ should be 書く (verb), not 書かない (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("書かなければ");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "書かなければ" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "書く") << "書かなければ lemma should be 書く";
    }
  }
  EXPECT_TRUE(found_verb) << "書かなければ should be recognized as verb";
}

// =============================================================================
// Regression: 手伝って lemma should be 手伝う (GodanWa), not 手伝る (GodanRa)
// =============================================================================
// Bug: 手伝って was getting lemma 手伝る due to equal confidence for
//      GodanWa/GodanRa/GodanTa in っ-onbin context with all-kanji stems
// Fix: Added GodanWa boost for multi-kanji stems in onbinkei context

TEST(AnalyzerTest, Regression_TetsudatteAgenai_Split) {
  // 手伝ってあげない should be split: 手伝って (verb) + あげない (verb)
  // Not: 手伝ってあげない as single verb
  // Benefactive verbs (あげる) in negative form should split at te-form boundary
  Suzume analyzer;
  auto result = analyzer.analyze("手伝ってあげない");
  ASSERT_GE(result.size(), 2) << "手伝ってあげない should have at least 2 tokens";

  bool found_tetsudatte = false;
  bool found_agenai = false;
  for (const auto& mor : result) {
    if (mor.surface == "手伝って" && mor.pos == core::PartOfSpeech::Verb) {
      found_tetsudatte = true;
      EXPECT_EQ(mor.lemma, "手伝う") << "手伝って lemma should be 手伝う (GodanWa)";
    }
    if (mor.surface == "あげない" && mor.pos == core::PartOfSpeech::Verb) {
      found_agenai = true;
      EXPECT_EQ(mor.lemma, "あげる") << "あげない lemma should be あげる";
    }
  }
  EXPECT_TRUE(found_tetsudatte) << "手伝って should be recognized";
  EXPECT_TRUE(found_agenai) << "あげない should be recognized";
}

TEST(AnalyzerTest, Regression_Tetsudatte_LemmaGodanWa) {
  // 手伝って should have lemma 手伝う (GodanWa), not 手伝る (GodanRa)
  Suzume analyzer;
  auto result = analyzer.analyze("手伝って");
  ASSERT_EQ(result.size(), 1) << "手伝って should be single token";
  EXPECT_EQ(result[0].surface, "手伝って");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "手伝う") << "手伝って lemma should be 手伝う (GodanWa)";
}

TEST(AnalyzerTest, Regression_HashiridashiTakunattekita_TaiPattern) {
  // 走り出したくなってきた should be split: 走り出し (verb) + たくなってきた (adj)
  // Not: 走り出し + た + くなってきた (where くなってきた is wrongly parsed as verb)
  // たくなってきた is a たい-pattern adjective (lemma=たい) that follows verb renyokei
  Suzume analyzer;
  auto result = analyzer.analyze("走り出したくなってきた");
  ASSERT_EQ(result.size(), 2) << "走り出したくなってきた should have 2 tokens";

  EXPECT_EQ(result[0].surface, "走り出し");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "走り出す");

  EXPECT_EQ(result[1].surface, "たくなってきた");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[1].lemma, "たい");
}

TEST(AnalyzerTest, Regression_TokoroDatta_FormalNoun) {
  // ところだった should not be split as と + ころだった
  // ところ is a formal noun used in aspectual patterns (Vたところだ = "just V'd")
  Suzume analyzer;
  auto result = analyzer.analyze("勉強させられていたところだった");

  // Find ところ and verify it's not split into と + ころ
  bool found_tokoro = false;
  bool found_to = false;
  for (const auto& mor : result) {
    if (mor.surface == "ところ") {
      found_tokoro = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "ところ should be formal noun";
    }
    if (mor.surface == "と" && mor.pos == core::PartOfSpeech::Particle) {
      found_to = true;  // This would indicate wrong split
    }
  }
  EXPECT_TRUE(found_tokoro) << "ところ should be recognized as formal noun";
  EXPECT_FALSE(found_to) << "と particle should not appear (wrong split)";
}

}  // namespace
}  // namespace suzume::analysis
