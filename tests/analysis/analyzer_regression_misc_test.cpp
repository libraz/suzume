// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Regression tests for miscellaneous items (auxiliaries, adverbs, time nouns)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

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
// Regression: Colloquial adverb めっちゃ
// =============================================================================
// Bug: めっちゃ was classified as OTHER
// Fix: Should be ADVERB

TEST(AnalyzerTest, Regression_ColloquialAdverbMeccha) {
  Suzume analyzer;
  auto result = analyzer.analyze("めっちゃ面白い");
  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";

  bool found_meccha = false;
  for (const auto& mor : result) {
    if (mor.surface == "めっちゃ") {
      found_meccha = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adverb)
          << "めっちゃ should be Adverb, not Other";
    }
  }
  EXPECT_TRUE(found_meccha) << "めっちゃ should be found";
}

// =============================================================================
// Regression: Dictionary entries
// =============================================================================

TEST(AnalyzerTest, Regression_Conjunction_Nimokakawarazu) {
  Suzume analyzer;
  auto result = analyzer.analyze("にもかかわらず");
  ASSERT_EQ(result.size(), 1) << "にもかかわらず should be single token";
  EXPECT_EQ(result[0].surface, "にもかかわらず");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Conjunction);
}

TEST(AnalyzerTest, Regression_Determiner_Souiu) {
  Suzume analyzer;
  auto result = analyzer.analyze("そういうこと");
  ASSERT_GE(result.size(), 2);

  bool found_souiu = false;
  for (const auto& mor : result) {
    if (mor.surface == "そういう" && mor.pos == core::PartOfSpeech::Determiner) {
      found_souiu = true;
    }
  }
  EXPECT_TRUE(found_souiu) << "そういう should be recognized as determiner";
}

TEST(AnalyzerTest, Regression_Adverb_Imasugu) {
  Suzume analyzer;
  auto result = analyzer.analyze("今すぐ行く");
  ASSERT_GE(result.size(), 2);

  bool found_imasugu = false;
  for (const auto& mor : result) {
    if (mor.surface == "今すぐ" && mor.pos == core::PartOfSpeech::Adverb) {
      found_imasugu = true;
    }
  }
  EXPECT_TRUE(found_imasugu) << "今すぐ should be recognized as adverb";
}

// =============================================================================
// Regression: Negative auxiliary ない + んだ
// =============================================================================
// Bug: ないんだ was analyzed as verb with lemma ないむ
// Fix: Skip ない in generateHiraganaVerbCandidates (should be AUX)

TEST(AnalyzerTest, Regression_NaiNda_Split) {
  Suzume analyzer;
  auto result = analyzer.analyze("ないんだ");
  ASSERT_EQ(result.size(), 2) << "ないんだ should split into 2 tokens";

  EXPECT_EQ(result[0].surface, "ない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Auxiliary)
      << "ない should be Auxiliary";

  EXPECT_EQ(result[1].surface, "んだ");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary)
      << "んだ should be Auxiliary";
  EXPECT_EQ(result[1].lemma, "のだ") << "んだ lemma should be のだ";
}

TEST(AnalyzerTest, Regression_NaiNda_InSentence) {
  Suzume analyzer;
  auto result = analyzer.analyze("知らないんだ");
  ASSERT_GE(result.size(), 2);

  bool found_nda = false;
  for (const auto& mor : result) {
    if (mor.surface == "んだ") {
      found_nda = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "んだ should be Auxiliary";
      EXPECT_EQ(mor.lemma, "のだ") << "んだ lemma should be のだ";
    }
  }
  EXPECT_TRUE(found_nda) << "んだ should be found as separate token";
}

// =============================================================================
// Regression: Time noun separation (毎朝コーヒー)
// =============================================================================
// Bug: 毎朝コーヒー was merged as single noun
// Fix: Added 毎朝 to time_nouns.h with is_formal_noun=true

TEST(AnalyzerTest, Regression_TimeNoun_MainchoSplit) {
  Suzume analyzer;
  auto result = analyzer.analyze("毎朝コーヒー");
  ASSERT_GE(result.size(), 2) << "毎朝コーヒー should split into at least 2 tokens";

  bool found_maiasa = false;
  bool found_coffee = false;
  for (const auto& mor : result) {
    if (mor.surface == "毎朝") {
      found_maiasa = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "毎朝 should be Noun";
    }
    if (mor.surface == "コーヒー") {
      found_coffee = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "コーヒー should be Noun";
    }
  }
  EXPECT_TRUE(found_maiasa) << "毎朝 should be found as separate token";
  EXPECT_TRUE(found_coffee) << "コーヒー should be found as separate token";
}

TEST(AnalyzerTest, Regression_TimeNoun_FullSentence) {
  Suzume analyzer;
  auto result = analyzer.analyze("毎朝コーヒーを飲みながら新聞を読む");
  ASSERT_GE(result.size(), 6);

  bool found_maiasa = false;
  bool found_coffee = false;
  bool found_nominagara = false;
  for (const auto& mor : result) {
    if (mor.surface == "毎朝") found_maiasa = true;
    if (mor.surface == "コーヒー") found_coffee = true;
    if (mor.surface == "飲みながら") {
      found_nominagara = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb);
      EXPECT_EQ(mor.lemma, "飲む");
    }
  }
  EXPECT_TRUE(found_maiasa) << "毎朝 should be found";
  EXPECT_TRUE(found_coffee) << "コーヒー should be found";
  EXPECT_TRUE(found_nominagara) << "飲みながら should be found";
}

// =============================================================================
// Regression: Formal noun 付け separation
// =============================================================================
// Bug: 2024年12月23日付けで was being parsed with 付けで as VERB
// Fix: Added 付け to formal_nouns.h with is_formal_noun=true

TEST(AnalyzerTest, Regression_FormalNoun_TsukeSplit) {
  Suzume analyzer;
  auto result = analyzer.analyze("日付けで");
  ASSERT_GE(result.size(), 2) << "日付けで should split 付け from で";

  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle) {
      found_de = true;
    }
  }
  EXPECT_TRUE(found_de) << "で should be recognized as particle";
}

TEST(AnalyzerTest, Regression_FormalNoun_DateWithTsuke) {
  // Full date format: 2024年12月23日付けで
  Suzume analyzer;
  auto result = analyzer.analyze("2024年12月23日付けで");
  ASSERT_GE(result.size(), 2);

  bool found_tsuke = false;
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "付け") {
      found_tsuke = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "付け should be Noun";
    }
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle) {
      found_de = true;
    }
  }
  EXPECT_TRUE(found_tsuke) << "付け should be found as separate token";
  EXPECT_TRUE(found_de) << "で should be found as particle";
}

// =============================================================================
// Regression: Demonstrative adverb そう
// =============================================================================
// Bug: そう was parsed as VERB
// Fix: Added そう to adverbs.h

TEST(AnalyzerTest, Regression_Adverb_Sou) {
  Suzume analyzer;
  auto result = analyzer.analyze("そうですね");
  ASSERT_GE(result.size(), 2);

  bool found_sou = false;
  for (const auto& mor : result) {
    if (mor.surface == "そう") {
      found_sou = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adverb)
          << "そう should be Adverb, not Verb";
    }
  }
  EXPECT_TRUE(found_sou) << "そう should be found";
}

TEST(AnalyzerTest, Regression_Adverb_SouKamoshirenai) {
  Suzume analyzer;
  auto result = analyzer.analyze("そうかもしれません");
  ASSERT_GE(result.size(), 2);

  bool found_sou = false;
  bool found_kamoshirenai = false;
  for (const auto& mor : result) {
    if (mor.surface == "そう") {
      found_sou = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adverb)
          << "そう should be Adverb";
    }
    if (mor.surface == "かもしれません") {
      found_kamoshirenai = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "かもしれません should be Auxiliary";
    }
  }
  EXPECT_TRUE(found_sou) << "そう should be found";
  EXPECT_TRUE(found_kamoshirenai) << "かもしれません should be found";
}

// =============================================================================
// Regression: Auxiliary かもしれない
// =============================================================================
// Bug: もしれません was parsed as verb もしれる
// Fix: Added かもしれない patterns to auxiliaries.h

TEST(AnalyzerTest, Regression_Aux_Kamoshirenai) {
  Suzume analyzer;
  auto result = analyzer.analyze("かもしれない");
  ASSERT_EQ(result.size(), 1) << "かもしれない should be single token";

  EXPECT_EQ(result[0].surface, "かもしれない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Auxiliary)
      << "かもしれない should be Auxiliary";
}

TEST(AnalyzerTest, Regression_Aux_KamoshiremasenInSentence) {
  Suzume analyzer;
  auto result = analyzer.analyze("明日は雨かもしれません");
  ASSERT_GE(result.size(), 3);

  bool found_kamo = false;
  for (const auto& mor : result) {
    if (mor.surface == "かもしれません") {
      found_kamo = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "かもしれません should be Auxiliary";
      EXPECT_EQ(mor.lemma, "かもしれない")
          << "かもしれません lemma should be かもしれない";
    }
  }
  EXPECT_TRUE(found_kamo) << "かもしれません should be found";
}

// =============================================================================
// Regression: Conditional adverb もし
// =============================================================================
// Bug: もし was parsed as OTHER
// Fix: Added もし to adverbs.h

TEST(AnalyzerTest, Regression_Adverb_Moshi) {
  Suzume analyzer;
  auto result = analyzer.analyze("もし雨が降ったら");
  ASSERT_GE(result.size(), 4);

  bool found_moshi = false;
  for (const auto& mor : result) {
    if (mor.surface == "もし") {
      found_moshi = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adverb)
          << "もし should be Adverb";
    }
  }
  EXPECT_TRUE(found_moshi) << "もし should be found";
}

// =============================================================================
// Regression: それぞれ adverb recognition
// =============================================================================
// Bug: それぞれ was split into それ + ぞれ
// Fix: Added それぞれ to adverbs dictionary

TEST(AnalyzerTest, Regression_Sorezore_SingleToken) {
  Suzume analyzer;
  auto result = analyzer.analyze("それぞれの意見を述べる");
  ASSERT_GE(result.size(), 4);

  bool found_sorezore = false;
  for (const auto& mor : result) {
    if (mor.surface == "それぞれ") {
      found_sorezore = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adverb)
          << "それぞれ should be Adverb";
    }
  }
  EXPECT_TRUE(found_sorezore)
      << "それぞれ should be found as single token, not split";
}

// =============================================================================
// Regression: Adverbs should not be split
// =============================================================================
// Bug: たくさん was split into た + くさん
// Bug: いつも was split into いつ + も
// Bug: まず was recognized as OTHER instead of ADV
// Fix: Register these adverbs in Layer 1 with appropriate cost

TEST(AnalyzerTest, Regression_TakusanAdverb) {
  Suzume analyzer;
  auto result = analyzer.analyze("たくさんの本");
  ASSERT_GE(result.size(), 2);

  EXPECT_EQ(result[0].surface, "たくさん");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb)
      << "たくさん should be Adverb, not split";
}

TEST(AnalyzerTest, Regression_ItsumoAdverb) {
  Suzume analyzer;
  auto result = analyzer.analyze("いつも元気");
  ASSERT_GE(result.size(), 2);

  EXPECT_EQ(result[0].surface, "いつも");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb)
      << "いつも should be Adverb, not split into いつ+も";
}

TEST(AnalyzerTest, Regression_MazuAdverb) {
  Suzume analyzer;
  auto result = analyzer.analyze("まず確認する");
  ASSERT_GE(result.size(), 2);

  EXPECT_EQ(result[0].surface, "まず");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb)
      << "まず should be Adverb";
}

}  // namespace
}  // namespace suzume::analysis
