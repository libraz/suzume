// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Grammar-related analyzer tests (auxiliary verbs, keigo, counters, etc.)

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

// ===== Onomatopoeia (擬音語・擬態語) Tests =====

TEST(AnalyzerTest, Onomatopoeia_WakuWaku) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("わくわくする");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Onomatopoeia_KiraKira) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("キラキラ光る");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Onomatopoeia_GataGata) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ガタガタ揺れる");
  ASSERT_FALSE(result.empty());
}

// ===== Counter Tests (助数詞) =====

TEST(AnalyzerTest, Counter_Nin) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("三人の学生");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Counter_Hon) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("二本のペン");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Counter_Ko) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("五個のリンゴ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Counter_Mai) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("十枚の紙");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Counter_Satsu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("三冊の本");
  ASSERT_FALSE(result.empty());
}

// ===== Conjunction Tests (接続詞) =====

TEST(AnalyzerTest, Conjunction_Shikashi) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("しかし問題がある");
  bool found_shikashi = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "しかし" &&
        morpheme.pos == core::PartOfSpeech::Conjunction) {
      found_shikashi = true;
      break;
    }
  }
  EXPECT_TRUE(found_shikashi);
}

TEST(AnalyzerTest, Conjunction_Sorede) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("それで帰った");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Conjunction_Demo) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("でも大丈夫");
  ASSERT_FALSE(result.empty());
}

// ===== Sentence Pattern Tests =====

TEST(AnalyzerTest, Pattern_NounNaAdjective) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("静かな部屋");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_IAdjective) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("高い山");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_TeForm) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べて寝る");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_ConditionalBa) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行けば分かる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_ConditionalTara) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行ったら教えて");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_ConditionalNara) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("君なら大丈夫");
  ASSERT_FALSE(result.empty());
}

// ===== Complex Sentence Tests =====

TEST(AnalyzerTest, ComplexSentence_RelativeClause) {
  // 昨日買った本を読んでいる (reading the book I bought yesterday)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("昨日買った本を読んでいる");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5);  // 昨日 + 買った + 本 + を + 読んでいる
  // Verify time noun segmentation
  bool found_kinou = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "昨日" && morpheme.pos == core::PartOfSpeech::Noun) {
      found_kinou = true;
      break;
    }
  }
  EXPECT_TRUE(found_kinou) << "昨日 should be recognized as separate noun";
}

TEST(AnalyzerTest, ComplexSentence_Embedded) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("彼が来ることを知っている");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, ComplexSentence_MultipleClauses) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("雨が降ったので、家にいた");
  ASSERT_FALSE(result.empty());
}

// ===== Time Noun Tests (時間名詞) =====

TEST(AnalyzerTest, TimeNoun_Kinou) {
  // 昨日 (yesterday)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("昨日");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "昨日");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
}

TEST(AnalyzerTest, TimeNoun_Ashita) {
  // 明日 (tomorrow)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("明日行く");
  bool found_ashita = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "明日" && morpheme.pos == core::PartOfSpeech::Noun) {
      found_ashita = true;
      break;
    }
  }
  EXPECT_TRUE(found_ashita) << "明日 should be recognized as noun";
}

TEST(AnalyzerTest, TimeNoun_Kyou) {
  // 今日 (today)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今日は暑い");
  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "今日");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
}

// ===== Formal Noun Tests (形式名詞) =====

TEST(AnalyzerTest, FormalNoun_Koto) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("勉強すること");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, FormalNoun_Mono) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べるもの");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, FormalNoun_Tokoro) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べるところ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, FormalNoun_Wake) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("そういうわけ");
  ASSERT_FALSE(result.empty());
}

// ===== Loanword (外来語) Tests =====

TEST(AnalyzerTest, Loanword_Katakana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("コンピューター");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Loanword_Mixed) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("インターネット接続");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Loanword_WithParticle) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("メールを送る");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "を") {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo);
}

// ===== Abbreviation and Symbol Tests =====

TEST(AnalyzerTest, Abbreviation_JapaneseAbbrev) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("高校生");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Symbol_Parentheses) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("東京（とうきょう）");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Symbol_Brackets) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("「こんにちは」と言った");
  ASSERT_FALSE(result.empty());
}

// ===== Colloquial Expression Tests =====

TEST(AnalyzerTest, Colloquial_Tte) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行くって言った");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Colloquial_Jan) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("いいじゃん");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Colloquial_Cha) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行っちゃった");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Colloquial_Toku) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("やっとく");
  ASSERT_FALSE(result.empty());
}

// ===== Numeric Expression Tests =====

TEST(AnalyzerTest, Numeric_JapaneseNumbers) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("百二十三");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Numeric_MixedNumbers) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("3時間");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Numeric_OrdinalNumber) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("第一回");
  ASSERT_FALSE(result.empty());
}

}  // namespace
}  // namespace suzume::analysis
