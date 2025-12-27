
// Mixed script and edge case analyzer tests
// Based on design_v2_practical.md and edge_cases.md

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::hasParticle;
using suzume::test::hasSurface;

// ===== Mixed Script Joining Tests (Phase M2) =====
// From design_v2_practical.md

class MixedScriptTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(MixedScriptTest, AlphabetKanji) {
  // Test: "Web開発" - alphabet + kanji
  auto result = analyzer.analyze("Web開発の基礎");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(MixedScriptTest, AlphabetKatakana) {
  // Test: "APIリクエスト" - alphabet + katakana
  auto result = analyzer.analyze("APIリクエスト処理");
  ASSERT_FALSE(result.empty());
  EXPECT_GT(result.size(), 0);
}

TEST_F(MixedScriptTest, DigitKanji) {
  // Test: "3月" - digit + kanji
  auto result = analyzer.analyze("3月の予定");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(MixedScriptTest, MultipleDigitKanji) {
  // Test: "100人" - multiple digits + kanji
  auto result = analyzer.analyze("100人が参加");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

// ===== English in Japanese Tests =====
// From edge_cases.md Section 1

class EnglishInJapaneseTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(EnglishInJapaneseTest, EnglishWithParticle) {
  // English word followed by particle
  auto result = analyzer.analyze("今日はMeetingがあります");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(EnglishInJapaneseTest, CamelCase) {
  // CamelCase should be preserved
  auto result = analyzer.analyze("getUserDataを呼び出す");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(EnglishInJapaneseTest, SnakeCase) {
  // snake_case should be preserved
  auto result = analyzer.analyze("user_nameを設定");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(EnglishInJapaneseTest, Abbreviation) {
  // Abbreviation
  auto result = analyzer.analyze("APIを呼ぶ");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(EnglishInJapaneseTest, TechnicalTerm) {
  // Technical term with Japanese
  auto result = analyzer.analyze("APIを使ってデータを取得する");
  ASSERT_FALSE(result.empty());
  int wo_count = 0;
  for (const auto& mor : result) {
    if (mor.surface == "を") wo_count++;
  }
  EXPECT_GE(wo_count, 1) << "Should recognize を particles";
}

TEST_F(EnglishInJapaneseTest, BrandName) {
  // Brand name in sentence
  auto result = analyzer.analyze("iPhoneを買いました");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(EnglishInJapaneseTest, EmailSend) {
  // email + particle
  auto result = analyzer.analyze("emailを送る");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(EnglishInJapaneseTest, ServerConnect) {
  // server + particle
  auto result = analyzer.analyze("serverに接続");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

// ===== Compound Noun Splitting Tests (Phase M3) =====
// From design_v2_practical.md

class CompoundNounTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(CompoundNounTest, FourKanji) {
  // 4 kanji compound
  auto result = analyzer.analyze("人工知能の研究");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(CompoundNounTest, LongKanji) {
  // Long kanji compound
  auto result = analyzer.analyze("東京都知事選挙");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 1);
}

TEST_F(CompoundNounTest, WithParticle) {
  // Compound noun followed by particle
  auto result = analyzer.analyze("情報処理技術者が");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(CompoundNounTest, NaturalLanguage) {
  // Natural language processing
  auto result = analyzer.analyze("自然言語処理技術");
  ASSERT_FALSE(result.empty());
}

TEST_F(CompoundNounTest, Organization) {
  // Organization name
  auto result = analyzer.analyze("国立研究所で働く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

// ===== Compound Expression Tests (複合表現) =====

class CompoundExpressionTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(CompoundExpressionTest, NiTsuite) {
  // について (regarding)
  auto result = analyzer.analyze("日本の文化について話す");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "について") || hasParticle(result, "に");
  EXPECT_TRUE(found) << "Should recognize について or に";
}

TEST_F(CompoundExpressionTest, NiYotte) {
  // によって (by means of)
  auto result = analyzer.analyze("場合によって対応が変わる");
  ASSERT_FALSE(result.empty());
}

TEST_F(CompoundExpressionTest, ToShite) {
  // として (as)
  auto result = analyzer.analyze("教師として働いている");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens";
}

// ===== Prefix/Suffix Tests (接辞) =====
// From edge_cases.md Section 5

class PrefixSuffixTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(PrefixSuffixTest, Honorific_O) {
  // お prefix
  auto result = analyzer.analyze("お茶を飲む");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(PrefixSuffixTest, Honorific_Go) {
  // ご prefix
  auto result = analyzer.analyze("ご飯を食べる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(PrefixSuffixTest, Negation_Fu) {
  // 不 prefix
  auto result = analyzer.analyze("不可能だ");
  ASSERT_FALSE(result.empty());
}

TEST_F(PrefixSuffixTest, Negation_Mi) {
  // 未 prefix
  auto result = analyzer.analyze("未確認の情報");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(PrefixSuffixTest, Negation_Hi) {
  // 非 prefix
  auto result = analyzer.analyze("非公開の資料");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(PrefixSuffixTest, Suffix_Teki) {
  // 的 suffix
  auto result = analyzer.analyze("国際的な問題");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "な")) << "Should recognize な particle";
}

TEST_F(PrefixSuffixTest, Suffix_Ka) {
  // 化 suffix
  auto result = analyzer.analyze("自動化する");
  ASSERT_FALSE(result.empty());
}

TEST_F(PrefixSuffixTest, Suffix_Sei) {
  // 性 suffix
  auto result = analyzer.analyze("可能性がある");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(PrefixSuffixTest, Honorific_San) {
  // さん suffix
  auto result = analyzer.analyze("田中さんが来た");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(PrefixSuffixTest, Title_Sensei) {
  // 先生 suffix
  auto result = analyzer.analyze("山田先生の授業");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

// ===== Number and Special Characters =====
// From edge_cases.md Section 3

class NumberSpecialTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(NumberSpecialTest, WithEmoji) {
  // Text after emoji context
  auto result = analyzer.analyze("今日も頑張ろう");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "今日")) << "Should recognize 今日";
}

TEST_F(NumberSpecialTest, NumbersAndUnits) {
  // Numbers with Japanese units
  auto result = analyzer.analyze("体重が3キロ減った");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(NumberSpecialTest, URLLike) {
  // URL-like mixed content
  auto result = analyzer.analyze("example.comで登録してください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(NumberSpecialTest, CounterNin) {
  // People counter
  auto result = analyzer.analyze("3人で行く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(NumberSpecialTest, CounterKai) {
  // Times counter
  auto result = analyzer.analyze("5回目の挑戦");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(NumberSpecialTest, Currency) {
  // Currency
  auto result = analyzer.analyze("100万円の買い物");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(NumberSpecialTest, Percent) {
  // Percentage
  auto result = analyzer.analyze("売上が50%増加");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

// ===== Pronoun Tests (代名詞) =====
// From edge_cases.md Section 10

class PronounTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(PronounTest, Personal_Watashi) {
  // 私
  auto result = analyzer.analyze("私は学生です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "私")) << "Should recognize 私";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(PronounTest, Demonstrative_Kore) {
  // これ
  auto result = analyzer.analyze("これを見て");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "これ")) << "Should recognize これ";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(PronounTest, Demonstrative_Sore) {
  // それ
  auto result = analyzer.analyze("それは違う");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "それ")) << "Should recognize それ";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(PronounTest, Demonstrative_Are) {
  // あれ
  auto result = analyzer.analyze("あれが欲しい");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "あれ")) << "Should recognize あれ";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(PronounTest, Interrogative_Dare) {
  // 誰
  auto result = analyzer.analyze("誰が来た");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "誰")) << "Should recognize 誰";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(PronounTest, Interrogative_Nani) {
  // 何
  auto result = analyzer.analyze("何を食べる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "何")) << "Should recognize 何";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

// ===== Symbol Tests (記号) =====
// From edge_cases.md Section 4

class SymbolTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(SymbolTest, Brackets) {
  // Brackets
  auto result = analyzer.analyze("AI（人工知能）の発展");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(SymbolTest, JapaneseQuotes) {
  // Japanese quotes
  auto result = analyzer.analyze("「こんにちは」と言った");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "と")) << "Should recognize と particle";
}

// ===== Administrative Division Tests (行政区画) =====
// From edge_cases.md Section 6

class AdministrativeTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(AdministrativeTest, Prefecture) {
  // Prefecture
  auto result = analyzer.analyze("東京都に住む");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(AdministrativeTest, City) {
  // City
  auto result = analyzer.analyze("横浜市で働く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(AdministrativeTest, Ward) {
  // Ward
  auto result = analyzer.analyze("渋谷区から来た");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "から")) << "Should recognize から";
}

}  // namespace
}  // namespace suzume::analysis
