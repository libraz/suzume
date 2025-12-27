// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Casual/SNS use case analyzer tests (informal, social media, reviews)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::hasParticle;
using suzume::test::hasSurface;

// ===== Casual/SNS Style Tests (カジュアル/SNS) =====

class CasualTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(CasualTest, Fun) {
  // Casual expression of fun
  auto result = analyzer.analyze("めっちゃ楽しかった");
  ASSERT_FALSE(result.empty());
}

TEST_F(CasualTest, Really) {
  // Casual confirmation
  auto result = analyzer.analyze("本当にそうなの");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens";
}

TEST_F(CasualTest, Desire) {
  // Desire expression
  auto result = analyzer.analyze("ラーメン食べたい");
  ASSERT_FALSE(result.empty());
  bool found_tabetai = false;
  for (const auto& mor : result) {
    if (mor.surface.find("食べ") != std::string::npos ||
        mor.surface == "食べたい") {
      found_tabetai = true;
      break;
    }
  }
  EXPECT_TRUE(found_tabetai) << "Should recognize 食べたい";
}

TEST_F(CasualTest, Surprise) {
  // Surprise expression
  auto result = analyzer.analyze("まじで驚いた");
  ASSERT_FALSE(result.empty());
}

TEST_F(CasualTest, Informal_Omission) {
  // Omitted particle (casual)
  auto result = analyzer.analyze("今日学校行った");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "今日")) << "Should recognize 今日";
}

TEST_F(CasualTest, Contraction_Teru) {
  // Contraction てる
  auto result = analyzer.analyze("何してる");
  ASSERT_FALSE(result.empty());
}

TEST_F(CasualTest, Contraction_Chau) {
  // Contraction ちゃう
  auto result = analyzer.analyze("食べちゃった");
  ASSERT_FALSE(result.empty());
}

TEST_F(CasualTest, Contraction_Toku) {
  // Contraction とく
  auto result = analyzer.analyze("買っとくね");
  ASSERT_FALSE(result.empty());
}

TEST_F(CasualTest, FinalParticle_Ne) {
  // Final particle ね: いい天気だね
  auto result = analyzer.analyze("いい天気だね");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "ね")) << "Should recognize ね particle";
}

TEST_F(CasualTest, FinalParticle_Yo) {
  // Final particle よ
  auto result = analyzer.analyze("もう帰るよ");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "よ")) << "Should recognize よ particle";
}

TEST_F(CasualTest, FinalParticle_Na) {
  // Final particle な
  auto result = analyzer.analyze("面白いな");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "な")) << "Should recognize な particle";
}

// ===== Social Media Tests (SNS・ソーシャルメディア) =====

class SocialMediaTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(SocialMediaTest, Lunch) {
  // Post with hashtag-like content
  auto result = analyzer.analyze("今日のランチ美味しかった");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "今日")) << "Should recognize 今日";
}

TEST_F(SocialMediaTest, Reaction) {
  // Casual reaction
  auto result = analyzer.analyze("まじで嬉しい");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens";
}

TEST_F(SocialMediaTest, QuestionPost) {
  // Question post
  auto result = analyzer.analyze("これどこで買えるか知ってる人いる？");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(SocialMediaTest, Recommendation) {
  // Recommendation post
  auto result = analyzer.analyze("このお店マジでおすすめ");
  ASSERT_FALSE(result.empty());
}

TEST_F(SocialMediaTest, Announcement) {
  // Personal announcement
  auto result = analyzer.analyze("引っ越しました");
  ASSERT_FALSE(result.empty());
}

TEST_F(SocialMediaTest, Gratitude) {
  // Gratitude post
  auto result = analyzer.analyze("みんなありがとう");
  ASSERT_FALSE(result.empty());
}

// ===== Product Review Tests (商品レビュー) =====

class ReviewTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(ReviewTest, Positive) {
  // Positive review
  auto result = analyzer.analyze("とても使いやすくて満足しています");
  ASSERT_FALSE(result.empty());
  bool has_satisfaction = false;
  for (const auto& mor : result) {
    if (mor.surface.find("満足") != std::string::npos ||
        mor.surface.find("使") != std::string::npos) {
      has_satisfaction = true;
      break;
    }
  }
  EXPECT_TRUE(has_satisfaction) << "Should recognize key terms";
}

TEST_F(ReviewTest, Negative) {
  // Negative review
  auto result = analyzer.analyze("期待していたほどではなかった");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 3) << "Should produce multiple tokens";
}

TEST_F(ReviewTest, Comparison) {
  // Comparative review
  auto result = analyzer.analyze("前のモデルより性能が良くなった");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(ReviewTest, Recommendation) {
  // Recommendation
  auto result = analyzer.analyze("この商品はおすすめです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(ReviewTest, PriceFeedback) {
  // Price feedback
  auto result = analyzer.analyze("値段の割に品質が良い");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(ReviewTest, Durability) {
  // Durability comment
  auto result = analyzer.analyze("3年使っているが壊れない");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(ReviewTest, Issue) {
  // Issue report
  auto result = analyzer.analyze("サイズが思ったより小さかった");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

// ===== Colloquial Expression Tests (口語表現) =====
// From edge_cases.md Section 2.4, 2.5

class ColloquialTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(ColloquialTest, Shiteru) {
  // している → してる
  auto result = analyzer.analyze("今何してる");
  ASSERT_FALSE(result.empty());
}

TEST_F(ColloquialTest, Miteta) {
  // 見ていた → 見てた
  auto result = analyzer.analyze("テレビ見てた");
  ASSERT_FALSE(result.empty());
}

TEST_F(ColloquialTest, Itteta) {
  // 行っていた → 行ってた
  auto result = analyzer.analyze("学校に行ってた");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(ColloquialTest, Tabeteku) {
  // 食べていく → 食べてく
  auto result = analyzer.analyze("一緒に食べてく");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(ColloquialTest, Meccha) {
  // めっちゃ (colloquial adverb)
  auto result = analyzer.analyze("めっちゃ面白い");
  ASSERT_FALSE(result.empty());
}

TEST_F(ColloquialTest, Yabai) {
  // やばい (colloquial adjective)
  auto result = analyzer.analyze("これまじやばい");
  ASSERT_FALSE(result.empty());
}

TEST_F(ColloquialTest, Jan) {
  // じゃん (colloquial ending)
  auto result = analyzer.analyze("いいじゃん");
  ASSERT_FALSE(result.empty());
}

TEST_F(ColloquialTest, Kke) {
  // っけ (questioning past)
  auto result = analyzer.analyze("何時だっけ");
  ASSERT_FALSE(result.empty());
}

TEST_F(ColloquialTest, Kana) {
  // かな (wondering)
  auto result = analyzer.analyze("明日晴れるかな");
  ASSERT_FALSE(result.empty());
}

TEST_F(ColloquialTest, Noni) {
  // のに (despite)
  auto result = analyzer.analyze("頑張ったのに負けた");
  ASSERT_FALSE(result.empty());
  bool found = hasParticle(result, "のに") || hasSurface(result, "のに");
  EXPECT_TRUE(found) << "Should recognize のに";
}

}  // namespace
}  // namespace suzume::analysis
