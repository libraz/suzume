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
  // 遅刻しそう is correctly segmented as: 遅刻 (noun) + しそう (verb)
  // This is the correct analysis for SURU nouns - they are separate from verb forms
  Suzume analyzer;
  auto result = analyzer.analyze("遅刻しそう");
  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";

  // Check that 遅刻 is recognized as noun
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
  // 遅刻しそうです is correctly segmented as: 遅刻 (noun) + しそう (verb) + です (aux)
  // This is the correct analysis for SURU nouns - they are separate from verb forms
  Suzume analyzer;
  auto result = analyzer.analyze("遅刻しそうです");
  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";

  // Check that 遅刻 is recognized as noun and しそう as verb
  bool found_chikoku = false;
  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "遅刻" && mor.pos == core::PartOfSpeech::Noun) {
      found_chikoku = true;
    }
    if (mor.surface.find("しそう") != std::string::npos &&
        mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "する")
          << "しそう/しそうです verb lemma should be する";
    }
  }
  EXPECT_TRUE(found_chikoku) << "遅刻 should be recognized as noun";
  EXPECT_TRUE(found_verb) << "しそう should be recognized as verb";
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

// =============================================================================
// Regression: Famous tongue twister parsing
// =============================================================================
// すもももももももものうち (李も桃も桃のうち)
// This is a classic Japanese parsing challenge.
// Correct: すもも/も/もも/も/もも/の/うち

TEST(AnalyzerTest, Regression_SumomoMomo) {
  Suzume analyzer;
  auto result = analyzer.analyze("すもももももももものうち");

  // Expected: すもも + も + もも + も + もも + の + うち = 7 tokens
  ASSERT_EQ(result.size(), 7) << "Expected 7 tokens for すもももももももものうち";

  EXPECT_EQ(result[0].surface, "すもも");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);

  EXPECT_EQ(result[1].surface, "も");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);

  EXPECT_EQ(result[2].surface, "もも");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Noun);

  EXPECT_EQ(result[3].surface, "も");
  EXPECT_EQ(result[3].pos, core::PartOfSpeech::Particle);

  EXPECT_EQ(result[4].surface, "もも");
  EXPECT_EQ(result[4].pos, core::PartOfSpeech::Noun);

  EXPECT_EQ(result[5].surface, "の");
  EXPECT_EQ(result[5].pos, core::PartOfSpeech::Particle);

  EXPECT_EQ(result[6].surface, "うち");
  EXPECT_EQ(result[6].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[6].lemma, "うち") << "うち lemma should be うち, not うつ";
}

// =============================================================================
// Edge cases: そう patterns (verb vs adjective disambiguation)
// =============================================================================
// These tests ensure correct handling of 〜そう patterns after scorer changes

TEST(AnalyzerTest, EdgeCase_VerbSou_Hashirisou) {
  // 走りそう should split as 走り (verb renyokei) + そう (adverb)
  Suzume analyzer;
  auto result = analyzer.analyze("走りそう");
  ASSERT_GE(result.size(), 2) << "走りそう should split into 2 tokens";
  EXPECT_EQ(result[0].surface, "走り");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
}

TEST(AnalyzerTest, EdgeCase_VerbSou_Nomisou) {
  // 飲みそう should split as 飲み (verb renyokei) + そう (adverb)
  Suzume analyzer;
  auto result = analyzer.analyze("飲みそう");
  ASSERT_GE(result.size(), 2) << "飲みそう should split into 2 tokens";
  EXPECT_EQ(result[0].surface, "飲み");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
}

TEST(AnalyzerTest, EdgeCase_AdjSou_Muzukashisou) {
  // 難しそう should be single ADJ token with lemma 難しい
  Suzume analyzer;
  auto result = analyzer.analyze("難しそう");
  ASSERT_EQ(result.size(), 1) << "難しそう should be single token";
  EXPECT_EQ(result[0].surface, "難しそう");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "難しい");
}

TEST(AnalyzerTest, EdgeCase_AdjSou_Kanashisou) {
  // 悲しそう should be single ADJ token with lemma 悲しい
  Suzume analyzer;
  auto result = analyzer.analyze("悲しそう");
  ASSERT_EQ(result.size(), 1) << "悲しそう should be single token";
  EXPECT_EQ(result[0].surface, "悲しそう");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "悲しい");
}

TEST(AnalyzerTest, EdgeCase_VerbSouDesu) {
  // 食べそうです should parse as 食べそう (verb) + です (aux)
  // Copula penalty should not apply after 〜そう verbs
  Suzume analyzer;
  auto result = analyzer.analyze("食べそうです");
  ASSERT_GE(result.size(), 2);

  bool found_verb = false;
  bool found_desu = false;
  for (const auto& mor : result) {
    if (mor.surface == "食べそう" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
    }
    if (mor.surface == "です" && mor.pos == core::PartOfSpeech::Auxiliary) {
      found_desu = true;
    }
  }
  EXPECT_TRUE(found_verb) << "食べそう should be VERB";
  EXPECT_TRUE(found_desu) << "です should be AUX";
}

// =============================================================================
// Edge cases: Suffix recognition (たち, さん, etc.)
// =============================================================================

TEST(AnalyzerTest, EdgeCase_Suffix_Watashitachi) {
  // 私たち should be recognized as pronoun (compound) or noun + suffix
  Suzume analyzer;
  auto result = analyzer.analyze("私たち");
  ASSERT_GE(result.size(), 1);
  // Either single PRON token or PRON + OTHER(suffix)
  if (result.size() == 1) {
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Pronoun);
  } else {
    EXPECT_EQ(result[1].surface, "たち");
    EXPECT_EQ(result[1].pos, core::PartOfSpeech::Other);
  }
}

TEST(AnalyzerTest, EdgeCase_Suffix_SenseiSan) {
  // 先生さん should split as 先生 (noun) + さん (suffix)
  Suzume analyzer;
  auto result = analyzer.analyze("先生さん");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "先生");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[1].surface, "さん");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Other);
}

// =============================================================================
// Edge cases: いつも patterns
// =============================================================================

TEST(AnalyzerTest, EdgeCase_Itsumo_BeforeVerb) {
  // いつも来る should parse as いつも (adverb) + 来る (verb)
  Suzume analyzer;
  auto result = analyzer.analyze("いつも来る");
  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "いつも");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb);
}

TEST(AnalyzerTest, EdgeCase_Itsumo_BeforeAdj) {
  // いつも楽しい should parse as いつも (adverb) + 楽しい (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("いつも楽しい");
  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "いつも");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb);
}

TEST(AnalyzerTest, EdgeCase_Itsumo_No_Mise) {
  // いつもの店 should parse as いつも (adverb) + の (particle) + 店 (noun)
  // NOT as いつ (pronoun) + もの (noun) + 店 (noun)
  Suzume analyzer;
  auto result = analyzer.analyze("いつもの店");
  ASSERT_EQ(result.size(), 3);
  EXPECT_EQ(result[0].surface, "いつも");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb);
  EXPECT_EQ(result[1].surface, "の");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(result[2].surface, "店");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Noun);
}

// =============================================================================
// Edge cases: Character speech patterns
// =============================================================================

TEST(AnalyzerTest, EdgeCase_CharSpeech_Nya) {
  // だにゃ should be recognized as single AUX (character speech)
  // lemma should be だよ (にゃ functions like よ sentence-ending particle)
  Suzume analyzer;
  auto result = analyzer.analyze("猫だにゃ");
  ASSERT_EQ(result.size(), 2);

  EXPECT_EQ(result[0].surface, "猫");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[1].surface, "だにゃ");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[1].lemma, "だよ");
}

TEST(AnalyzerTest, EdgeCase_CharSpeech_Desuwa) {
  // ですわ should be recognized as AUX (ojou-sama speech)
  Suzume analyzer;
  auto result = analyzer.analyze("綺麗ですわ");
  ASSERT_GE(result.size(), 2);

  bool found_desuwa = false;
  for (const auto& mor : result) {
    if (mor.surface == "ですわ" && mor.pos == core::PartOfSpeech::Auxiliary) {
      found_desuwa = true;
    }
  }
  EXPECT_TRUE(found_desuwa) << "ですわ should be AUX";
}

}  // namespace
}  // namespace suzume::analysis
