// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// News/Media use case analyzer tests (news, weather, sports, academic)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// Helper function to check if a particle exists in the result
bool hasParticle(const std::vector<core::Morpheme>& result,
                 const std::string& surface) {
  for (const auto& mor : result) {
    if (mor.surface == surface && mor.pos == core::PartOfSpeech::Particle) {
      return true;
    }
  }
  return false;
}

// Helper function to check if a surface exists in the result
bool hasSurface(const std::vector<core::Morpheme>& result,
                const std::string& surface) {
  for (const auto& mor : result) {
    if (mor.surface == surface) {
      return true;
    }
  }
  return false;
}

// ===== News/Article Style Tests (ニュース・記事) =====

class NewsTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(NewsTest, Announcement) {
  // News announcement pattern
  auto result = analyzer.analyze("政府は新しい政策を発表した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(NewsTest, Citation) {
  // Citation pattern
  auto result = analyzer.analyze("関係者によると問題はない");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 3) << "Should produce multiple tokens";
}

TEST_F(NewsTest, Event) {
  // Event description
  auto result = analyzer.analyze("昨日、記者会見が行われた");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "昨日")) << "Should recognize 昨日";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(NewsTest, Incident) {
  // Incident report
  auto result = analyzer.analyze("事故で3人が負傷した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(NewsTest, Investigation) {
  // Investigation report
  auto result = analyzer.analyze("警察は原因を調査している");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(NewsTest, Election) {
  // Election news
  auto result = analyzer.analyze("選挙で与党が過半数を獲得した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

// ===== Weather Forecast Tests (天気予報) =====

class WeatherTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(WeatherTest, Forecast) {
  // Weather forecast
  auto result = analyzer.analyze("明日は晴れのち曇りでしょう");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "明日")) << "Should recognize 明日";
}

TEST_F(WeatherTest, Warning) {
  // Weather warning
  auto result = analyzer.analyze("大雨警報が発令されました");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(WeatherTest, Temperature) {
  // Temperature description
  auto result = analyzer.analyze("最高気温は30度の予想です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(WeatherTest, Rain) {
  // Rain probability
  auto result = analyzer.analyze("降水確率は60%です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(WeatherTest, Wind) {
  // Wind information
  auto result = analyzer.analyze("北西の風が強く吹くでしょう");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

// ===== Sports Tests (スポーツ) =====

class SportsTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(SportsTest, GameResult) {
  // Game result
  auto result = analyzer.analyze("日本代表が2対1で勝利した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(SportsTest, PlayerComment) {
  // Player comment
  auto result = analyzer.analyze("チーム一丸となって戦いたい");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 3) << "Should produce multiple tokens";
}

TEST_F(SportsTest, Schedule) {
  // Game schedule
  auto result = analyzer.analyze("試合は午後7時から開始予定です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "から")) << "Should recognize から";
}

TEST_F(SportsTest, Ranking) {
  // Ranking
  auto result = analyzer.analyze("現在3位につけている");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(SportsTest, Injury) {
  // Injury report
  auto result = analyzer.analyze("選手は怪我のため欠場する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

// ===== Academic/Research Tests (学術・論文) =====

class AcademicTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(AcademicTest, Hypothesis) {
  // Academic hypothesis
  auto result = analyzer.analyze("本研究では以下の仮説を検証する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(AcademicTest, Result) {
  // Research result
  auto result = analyzer.analyze("実験の結果、有意な差が認められた");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(AcademicTest, Conclusion) {
  // Conclusion statement
  auto result = analyzer.analyze("以上の結果から次のように結論づけられる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "から")) << "Should recognize から";
}

TEST_F(AcademicTest, Method) {
  // Method description
  auto result = analyzer.analyze("本研究ではアンケート調査を実施した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(AcademicTest, Reference) {
  // Reference citation
  auto result = analyzer.analyze("先行研究によれば効果が確認されている");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(AcademicTest, Limitation) {
  // Limitation statement
  auto result = analyzer.analyze("本研究にはいくつかの限界がある");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

// ===== Long Sentence Tests (長文テスト) =====

class LongSentenceTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(LongSentenceTest, NewsArticle) {
  // News article style long sentence
  auto result = analyzer.analyze(
      "政府は昨日の閣議で、新しい経済政策を正式に決定したと発表した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
  EXPECT_TRUE(hasParticle(result, "と")) << "Should recognize と particle";
}

TEST_F(LongSentenceTest, Narrative) {
  // Narrative style
  auto result = analyzer.analyze(
      "彼は昔から音楽が好きで、毎日ピアノの練習を欠かさなかった");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 8) << "Should produce many tokens";
}

TEST_F(LongSentenceTest, Instructions) {
  // Multi-step instructions
  auto result = analyzer.analyze(
      "まず電源ボタンを押して起動し、次に設定画面から言語を選択してください");
  ASSERT_FALSE(result.empty());
  int wo_count = 0;
  for (const auto& mor : result) {
    if (mor.surface == "を") wo_count++;
  }
  EXPECT_GE(wo_count, 1) << "Should recognize multiple を particles";
}

TEST_F(LongSentenceTest, ComplexCondition) {
  // Complex conditional
  auto result = analyzer.analyze(
      "もし明日の天気が良ければ、公園でピクニックをしようと思っています");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(LongSentenceTest, Explanation) {
  // Explanation style
  auto result = analyzer.analyze(
      "この問題が発生する原因は、設定ファイルが正しく読み込まれていないことです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

// ===== Education Tests (教育) =====

class EducationTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(EducationTest, TeacherInstruction) {
  // Teacher instruction
  auto result = analyzer.analyze("教科書の35ページを開いてください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(EducationTest, StudentQuestion) {
  // Student question
  auto result = analyzer.analyze("この問題の解き方が分かりません");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(EducationTest, Assignment) {
  // Homework assignment
  auto result = analyzer.analyze("明日までに宿題を提出してください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "まで")) << "Should recognize まで";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(EducationTest, Explanation) {
  // Teacher explanation
  auto result = analyzer.analyze("この公式は次のように使います");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(EducationTest, GroupWork) {
  // Group work instruction
  auto result = analyzer.analyze("グループで話し合ってください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

}  // namespace
}  // namespace suzume::analysis
