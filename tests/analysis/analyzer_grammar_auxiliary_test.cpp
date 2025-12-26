// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Grammar tests for auxiliary verbs and keigo (honorific expressions)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// ===== Auxiliary Verb Tests (助動詞) =====

TEST(AnalyzerTest, AuxiliaryVerb_Desu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("これは本です");
  bool found_desu = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "です") {
      found_desu = true;
      break;
    }
  }
  EXPECT_TRUE(found_desu);
}

TEST(AnalyzerTest, AuxiliaryVerb_Masu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, AuxiliaryVerb_Tai) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行きたい");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, AuxiliaryVerb_Nai) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行かない");
  ASSERT_FALSE(result.empty());
}

// ===== Keigo (敬語) Tests =====

TEST(AnalyzerTest, Keigo_Irassharu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("先生がいらっしゃる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Gozaimasu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ございます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Itadaku) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("いただきます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Kudasaru) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("教えてくださる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_OPrefix) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("お忙しいところ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_GoPrefix) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ご確認ください");
  ASSERT_FALSE(result.empty());
}

// ===== Kuruwa-kotoba (廓言葉) Tests =====

TEST(AnalyzerTest, Kuruwa_Arinsu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ここにありんす");
  bool found_arinsu = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "ありんす" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found_arinsu = true;
      EXPECT_EQ(morpheme.lemma, "ある");
      break;
    }
  }
  EXPECT_TRUE(found_arinsu);
}

TEST(AnalyzerTest, Kuruwa_DeArinsu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("そうでありんす");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "でありんす" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "だ");
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(AnalyzerTest, Kuruwa_Zansu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("よろしゅうざんす");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "ざんす" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "ある");
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(AnalyzerTest, Kuruwa_Arinshita) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("でありんしたか");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "でありんした" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "だ");
      break;
    }
  }
  EXPECT_TRUE(found);
}

// =============================================================================
// Character Speech Patterns (キャラクター語尾/役割語) Tests
// =============================================================================

// Cat-like (猫系)
// だにゃ is a compound form (だ + にゃ) and should be a single token
// lemma is だよ because にゃ functions like よ (sentence-ending particle)
TEST(AnalyzerTest, CharacterSpeech_Nya) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("猫だにゃ");
  bool found_danya = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "だにゃ" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found_danya = true;
      EXPECT_EQ(morpheme.lemma, "だよ");
      break;
    }
  }
  EXPECT_TRUE(found_danya) << "だにゃ should be found as single Auxiliary";
}

// にゃん alone (after verb) functions like よ
TEST(AnalyzerTest, CharacterSpeech_Nyan) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べるにゃん");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "にゃん" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "よ");
      break;
    }
  }
  EXPECT_TRUE(found) << "にゃん should be found as Auxiliary";
}

// Squid character (イカ娘)
TEST(AnalyzerTest, CharacterSpeech_Geso) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("吾輩は猫でゲソ");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "でゲソ" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "だ");
      break;
    }
  }
  EXPECT_TRUE(found) << "でゲソ should be found as Auxiliary";
}

// Ojou-sama speech (お嬢様言葉)
TEST(AnalyzerTest, CharacterSpeech_Desuwa) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("お嬢様ですわ");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "ですわ" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "です");
      break;
    }
  }
  EXPECT_TRUE(found) << "ですわ should be found as Auxiliary";
}

TEST(AnalyzerTest, CharacterSpeech_Dawa) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("そうだわ");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "だわ" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "だ");
      break;
    }
  }
  EXPECT_TRUE(found) << "だわ should be found as Auxiliary";
}

// Youth slang (若者言葉)
TEST(AnalyzerTest, CharacterSpeech_Ssu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("いいっす");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "っす" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "です");
      break;
    }
  }
  EXPECT_TRUE(found) << "っす should be found as Auxiliary";
}

// Ninja/Old-fashioned (忍者・古風)
TEST(AnalyzerTest, CharacterSpeech_Gozaru) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("これでござる");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "ござる" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "だ");
      break;
    }
  }
  EXPECT_TRUE(found) << "ござる should be found as Auxiliary";
}

// Elderly speech (老人語)
TEST(AnalyzerTest, CharacterSpeech_Jarou) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("そうじゃろう");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "じゃろう" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "だろう");
      break;
    }
  }
  EXPECT_TRUE(found) << "じゃろう should be found as Auxiliary";
}

// Regional dialect (方言系)
TEST(AnalyzerTest, CharacterSpeech_Yade) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("そうやで");
  bool found = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "やで" &&
        morpheme.pos == core::PartOfSpeech::Auxiliary) {
      found = true;
      EXPECT_EQ(morpheme.lemma, "だ");
      break;
    }
  }
  EXPECT_TRUE(found) << "やで should be found as Auxiliary";
}

// =============================================================================
// Auxiliary かもしれない patterns
// =============================================================================
// Moved from analyzer_regression_misc_test.cpp
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

}  // namespace
}  // namespace suzume::analysis
