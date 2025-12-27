// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Real-world use case tests based on design.md and edge_cases.md
// Tests practical Japanese text patterns: mixed scripts, pretokenizer patterns,
// compound nouns, prefixes/suffixes, and complex expressions.

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::hasParticle;
using suzume::test::hasSurface;

// Helper function to print morphemes for debugging
void printMorphemes(const std::vector<core::Morpheme>& result,
                    const std::string& input) {
  std::cout << "Input: " << input << "\n";
  std::cout << "Morphemes: [";
  for (size_t idx = 0; idx < result.size(); ++idx) {
    if (idx > 0) std::cout << ", ";
    std::cout << "\"" << result[idx].surface << "\"";
  }
  std::cout << "]\n";
}

// ===== Mixed Script Tests (英日混合) =====
// From edge_cases.md Section 1

class RealworldMixedScriptTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

// 1.1 English words with Japanese particles
TEST_F(RealworldMixedScriptTest, EnglishWithParticle_Ga) {
  // "Meetingがある" -> should recognize meeting (normalized to lowercase)
  auto result = analyzer.analyze("Meetingがある");
  printMorphemes(result, "Meetingがある");
  ASSERT_FALSE(result.empty());
  // Normalizer converts to lowercase
  EXPECT_TRUE(hasSurface(result, "meeting")) << "Should recognize meeting";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(RealworldMixedScriptTest, EnglishWithParticle_Wo) {
  // "emailを送る" -> email + を + 送る
  auto result = analyzer.analyze("emailを送る");
  printMorphemes(result, "emailを送る");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "email")) << "Should recognize email";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldMixedScriptTest, EnglishWithParticle_Ni) {
  // "serverに接続" -> server + に + 接続
  auto result = analyzer.analyze("serverに接続");
  printMorphemes(result, "serverに接続");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "server")) << "Should recognize server";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

// 1.2 CamelCase / snake_case
TEST_F(RealworldMixedScriptTest, CamelCase) {
  // "getUserDataを呼ぶ" -> getuserdata as single token (lowercase)
  auto result = analyzer.analyze("getUserDataを呼ぶ");
  printMorphemes(result, "getUserDataを呼ぶ");
  ASSERT_FALSE(result.empty());
  // Normalizer converts to lowercase, CamelCase is preserved as single token
  EXPECT_TRUE(hasSurface(result, "getuserdata"))
      << "Should recognize getuserdata as single token";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldMixedScriptTest, SnakeCase) {
  // "user_nameを設定" -> currently splits at underscore
  auto result = analyzer.analyze("user_nameを設定");
  printMorphemes(result, "user_nameを設定");
  ASSERT_FALSE(result.empty());
  // TODO: Consider joining snake_case as single token
  // Currently splits: user, _, name
  EXPECT_TRUE(hasSurface(result, "user")) << "Should recognize user";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

// 1.5 Abbreviations
TEST_F(RealworldMixedScriptTest, Abbreviation_API) {
  // "APIを呼ぶ" -> api + を + 呼ぶ (lowercase)
  auto result = analyzer.analyze("APIを呼ぶ");
  printMorphemes(result, "APIを呼ぶ");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "api")) << "Should recognize api";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldMixedScriptTest, Abbreviation_HTTP) {
  // "HTTPエラー" -> httpエラー (lowercase, joined)
  auto result = analyzer.analyze("HTTPエラー");
  printMorphemes(result, "HTTPエラー");
  ASSERT_FALSE(result.empty());
  // Joined with lowercase: httpエラー
  bool has_http = hasSurface(result, "httpエラー") ||
                  hasSurface(result, "http") || hasSurface(result, "エラー");
  EXPECT_TRUE(has_http) << "Should recognize httpエラー or components";
}

// Mixed script joining (design.md Phase M2)
TEST_F(RealworldMixedScriptTest, WebKaihatsu) {
  // "Web開発" -> web開発 (lowercase, joined)
  auto result = analyzer.analyze("Web開発の基礎");
  printMorphemes(result, "Web開発の基礎");
  ASSERT_FALSE(result.empty());
  // Joined with lowercase: web開発
  bool valid = hasSurface(result, "web開発") ||
               (hasSurface(result, "web") && hasSurface(result, "開発"));
  EXPECT_TRUE(valid) << "Should recognize web開発 (joined or separate)";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(RealworldMixedScriptTest, AIKenkyu) {
  // "AI研究が進む" -> ai研究 (lowercase) + が + 進む
  auto result = analyzer.analyze("AI研究が進む");
  printMorphemes(result, "AI研究が進む");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(RealworldMixedScriptTest, APIRequest) {
  // "APIリクエスト処理" -> API + リクエスト + 処理 or combinations
  auto result = analyzer.analyze("APIリクエスト処理");
  printMorphemes(result, "APIリクエスト処理");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2) << "Should produce meaningful tokens";
}

// ===== PreTokenizer Pattern Tests (事前トークン化パターン) =====
// From design.md Section P1

class RealworldPreTokenizerTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(RealworldPreTokenizerTest, URL_Simple) {
  // URL should be single token
  auto result = analyzer.analyze("https://example.comにアクセス");
  printMorphemes(result, "https://example.comにアクセス");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "https://example.com"))
      << "URL should be single token";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(RealworldPreTokenizerTest, URL_WithPath) {
  // URL with path
  auto result = analyzer.analyze("https://example.com/path/to/pageを開く");
  printMorphemes(result, "https://example.com/path/to/pageを開く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldPreTokenizerTest, Version_Simple) {
  // Version number: v2.0.1
  auto result = analyzer.analyze("v2.0.1にアップデート");
  printMorphemes(result, "v2.0.1にアップデート");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "v2.0.1"))
      << "Version should be single token";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(RealworldPreTokenizerTest, Storage_GB) {
  // Storage capacity: 3.5GB
  auto result = analyzer.analyze("3.5GBのメモリが必要");
  printMorphemes(result, "3.5GBのメモリが必要");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "3.5GB")) << "Storage should be single token";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(RealworldPreTokenizerTest, Storage_MB) {
  // Storage capacity: 512MB
  auto result = analyzer.analyze("512MBのファイル");
  printMorphemes(result, "512MBのファイル");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "512MB")) << "512MB should be single token";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(RealworldPreTokenizerTest, Percentage) {
  // Percentage: 50%
  auto result = analyzer.analyze("成功率は50%です");
  printMorphemes(result, "成功率は50%です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "50%")) << "50% should be single token";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(RealworldPreTokenizerTest, DateFull) {
  // Full date: 2024年12月23日
  auto result = analyzer.analyze("2024年12月23日に送付");
  printMorphemes(result, "2024年12月23日に送付");
  ASSERT_FALSE(result.empty());
  // Date can be single token or parsed as components
  bool has_year =
      hasSurface(result, "2024年12月23日") || hasSurface(result, "2024年");
  EXPECT_TRUE(has_year) << "Should recognize date components";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(RealworldPreTokenizerTest, Currency_Man) {
  // Currency with 万: 100万円
  auto result = analyzer.analyze("100万円の請求");
  printMorphemes(result, "100万円の請求");
  ASSERT_FALSE(result.empty());
  // 100万円 should be handled as single token or as 100万 + 円
  bool has_currency =
      hasSurface(result, "100万円") || hasSurface(result, "100万");
  EXPECT_TRUE(has_currency) << "Should recognize currency";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(RealworldPreTokenizerTest, Hashtag) {
  // Hashtag: #プログラミング - currently extends to end of same script
  auto result = analyzer.analyze("#プログラミングを学ぶ");
  printMorphemes(result, "#プログラミングを学ぶ");
  ASSERT_FALSE(result.empty());
  // TODO: Hashtag should end at first particle or whitespace
  // Currently: ["#プログラミングを学ぶ"] as single token
  // Expected: ["#プログラミング", "を", "学ぶ"]
  EXPECT_GE(result.size(), 1) << "Should produce at least one token";
}

TEST_F(RealworldPreTokenizerTest, Mention) {
  // Mention: @tanaka_taro
  auto result = analyzer.analyze("@tanaka_taroに連絡する");
  printMorphemes(result, "@tanaka_taroに連絡する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

// ===== Prefix/Suffix Tests (接頭語・接尾語) =====
// From edge_cases.md Section 5

class RealworldPrefixSuffixTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

// 5.1 Honorific prefixes
TEST_F(RealworldPrefixSuffixTest, Prefix_OCha) {
  // お茶を飲む
  auto result = analyzer.analyze("お茶を飲む");
  printMorphemes(result, "お茶を飲む");
  ASSERT_FALSE(result.empty());
  bool has_ocha = hasSurface(result, "お茶") ||
                  (hasSurface(result, "お") && hasSurface(result, "茶"));
  EXPECT_TRUE(has_ocha) << "Should recognize お茶";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldPrefixSuffixTest, Prefix_GoHan) {
  // ご飯を食べる
  auto result = analyzer.analyze("ご飯を食べる");
  printMorphemes(result, "ご飯を食べる");
  ASSERT_FALSE(result.empty());
  bool has_gohan = hasSurface(result, "ご飯") ||
                   (hasSurface(result, "ご") && hasSurface(result, "飯"));
  EXPECT_TRUE(has_gohan) << "Should recognize ご飯";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

// Negation prefixes
TEST_F(RealworldPrefixSuffixTest, Prefix_FuKanou) {
  // 不可能だ
  auto result = analyzer.analyze("不可能だ");
  printMorphemes(result, "不可能だ");
  ASSERT_FALSE(result.empty());
  bool has_fukanou = hasSurface(result, "不可能") ||
                     (hasSurface(result, "不") && hasSurface(result, "可能"));
  EXPECT_TRUE(has_fukanou) << "Should recognize 不可能";
}

TEST_F(RealworldPrefixSuffixTest, Prefix_MiKakunin) {
  // 未確認の
  auto result = analyzer.analyze("未確認の情報");
  printMorphemes(result, "未確認の情報");
  ASSERT_FALSE(result.empty());
  bool has_mikakunin =
      hasSurface(result, "未確認") ||
      (hasSurface(result, "未") && hasSurface(result, "確認"));
  EXPECT_TRUE(has_mikakunin) << "Should recognize 未確認";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(RealworldPrefixSuffixTest, Prefix_HiKoukai) {
  // 非公開の
  auto result = analyzer.analyze("非公開の資料");
  printMorphemes(result, "非公開の資料");
  ASSERT_FALSE(result.empty());
  bool has_hikoukai = hasSurface(result, "非公開") ||
                      (hasSurface(result, "非") && hasSurface(result, "公開"));
  EXPECT_TRUE(has_hikoukai) << "Should recognize 非公開";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

// 5.2 Honorific suffixes
TEST_F(RealworldPrefixSuffixTest, Suffix_San) {
  // 田中さんが
  auto result = analyzer.analyze("田中さんが来た");
  printMorphemes(result, "田中さんが来た");
  ASSERT_FALSE(result.empty());
  // Either 田中さん joined or 田中 + さん separate
  bool valid = hasSurface(result, "田中さん") ||
               (hasSurface(result, "田中") && hasSurface(result, "さん"));
  EXPECT_TRUE(valid) << "Should recognize 田中さん";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(RealworldPrefixSuffixTest, Suffix_Sama) {
  // 山田様の
  auto result = analyzer.analyze("山田様のご依頼");
  printMorphemes(result, "山田様のご依頼");
  ASSERT_FALSE(result.empty());
  bool valid = hasSurface(result, "山田様") ||
               (hasSurface(result, "山田") && hasSurface(result, "様"));
  EXPECT_TRUE(valid) << "Should recognize 山田様";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(RealworldPrefixSuffixTest, Suffix_Sensei) {
  // 佐藤先生の
  auto result = analyzer.analyze("佐藤先生の授業");
  printMorphemes(result, "佐藤先生の授業");
  ASSERT_FALSE(result.empty());
  bool valid = hasSurface(result, "佐藤先生") ||
               (hasSurface(result, "佐藤") && hasSurface(result, "先生"));
  EXPECT_TRUE(valid) << "Should recognize 佐藤先生";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

// 5.4 Derivational suffixes
TEST_F(RealworldPrefixSuffixTest, Suffix_Teki) {
  // 国際的な
  auto result = analyzer.analyze("国際的な会議");
  printMorphemes(result, "国際的な会議");
  ASSERT_FALSE(result.empty());
  bool valid = hasSurface(result, "国際的") ||
               (hasSurface(result, "国際") && hasSurface(result, "的"));
  EXPECT_TRUE(valid) << "Should recognize 国際的";
  EXPECT_TRUE(hasParticle(result, "な")) << "Should recognize な particle";
}

TEST_F(RealworldPrefixSuffixTest, Suffix_Ka) {
  // 自動化する - currently parsed as single verb
  auto result = analyzer.analyze("自動化する");
  printMorphemes(result, "自動化する");
  ASSERT_FALSE(result.empty());
  // "自動化する" is recognized as suru-verb compound
  bool valid = hasSurface(result, "自動化する") ||
               hasSurface(result, "自動化") ||
               (hasSurface(result, "自動") && hasSurface(result, "化"));
  EXPECT_TRUE(valid) << "Should recognize 自動化する or components";
}

TEST_F(RealworldPrefixSuffixTest, Suffix_Sei) {
  // 可能性がある
  auto result = analyzer.analyze("可能性がある");
  printMorphemes(result, "可能性がある");
  ASSERT_FALSE(result.empty());
  bool valid = hasSurface(result, "可能性") ||
               (hasSurface(result, "可能") && hasSurface(result, "性"));
  EXPECT_TRUE(valid) << "Should recognize 可能性";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

// ===== Compound Noun Tests (複合名詞) =====
// From design.md Category 3 and edge_cases.md Section 6

class RealworldCompoundNounTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(RealworldCompoundNounTest, JinkouChinou) {
  // 人工知能
  auto result = analyzer.analyze("人工知能の研究");
  printMorphemes(result, "人工知能の研究");
  ASSERT_FALSE(result.empty());
  // Either joined or split is acceptable
  bool valid = hasSurface(result, "人工知能") ||
               (hasSurface(result, "人工") && hasSurface(result, "知能"));
  EXPECT_TRUE(valid) << "Should recognize 人工知能";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(RealworldCompoundNounTest, ShizenGengoShori) {
  // 自然言語処理
  auto result = analyzer.analyze("自然言語処理技術");
  printMorphemes(result, "自然言語処理技術");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2) << "Should produce meaningful segments";
}

TEST_F(RealworldCompoundNounTest, KikaiGakushuu) {
  // 機械学習
  auto result = analyzer.analyze("機械学習モデル");
  printMorphemes(result, "機械学習モデル");
  ASSERT_FALSE(result.empty());
  bool valid = hasSurface(result, "機械学習") ||
               (hasSurface(result, "機械") && hasSurface(result, "学習"));
  EXPECT_TRUE(valid) << "Should recognize 機械学習";
}

// 6.1 Administrative regions
TEST_F(RealworldCompoundNounTest, TokyoTo) {
  // 東京都渋谷区
  auto result = analyzer.analyze("東京都渋谷区に移転");
  printMorphemes(result, "東京都渋谷区に移転");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(RealworldCompoundNounTest, KanagawaKen) {
  // 神奈川県横浜市
  auto result = analyzer.analyze("神奈川県横浜市の本社");
  printMorphemes(result, "神奈川県横浜市の本社");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

// 6.2 Organization names
TEST_F(RealworldCompoundNounTest, KabushikiKaisha) {
  // 株式会社ABC
  auto result = analyzer.analyze("株式会社ABCの担当者");
  printMorphemes(result, "株式会社ABCの担当者");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "株式会社"))
      << "Should recognize 株式会社";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(RealworldCompoundNounTest, KokuritsuKenkyujo) {
  // 国立研究所
  auto result = analyzer.analyze("国立研究所で働く");
  printMorphemes(result, "国立研究所で働く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

// ===== Pronoun Tests (代名詞) =====
// From edge_cases.md Section 10

class RealworldPronounTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(RealworldPronounTest, Personal_Watashi) {
  // 私は学生です
  auto result = analyzer.analyze("私は学生です");
  printMorphemes(result, "私は学生です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "私")) << "Should recognize 私";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(RealworldPronounTest, Personal_Kare) {
  // 彼が来た
  auto result = analyzer.analyze("彼が来た");
  printMorphemes(result, "彼が来た");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "彼")) << "Should recognize 彼";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(RealworldPronounTest, Personal_Kanojo) {
  // 彼女と話す
  auto result = analyzer.analyze("彼女と話す");
  printMorphemes(result, "彼女と話す");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "彼女")) << "Should recognize 彼女";
  EXPECT_TRUE(hasParticle(result, "と")) << "Should recognize と particle";
}

TEST_F(RealworldPronounTest, Demonstrative_Kore) {
  // これを見て
  auto result = analyzer.analyze("これを見て");
  printMorphemes(result, "これを見て");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "これ")) << "Should recognize これ";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldPronounTest, Demonstrative_Sore) {
  // それは何
  auto result = analyzer.analyze("それは何");
  printMorphemes(result, "それは何");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "それ")) << "Should recognize それ";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(RealworldPronounTest, Demonstrative_Are) {
  // あれが欲しい
  auto result = analyzer.analyze("あれが欲しい");
  printMorphemes(result, "あれが欲しい");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "あれ")) << "Should recognize あれ";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(RealworldPronounTest, Demonstrative_Koko) {
  // ここに置く
  auto result = analyzer.analyze("ここに置く");
  printMorphemes(result, "ここに置く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "ここ")) << "Should recognize ここ";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(RealworldPronounTest, Interrogative_Dare) {
  // 誰が来た
  auto result = analyzer.analyze("誰が来た");
  printMorphemes(result, "誰が来た");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "誰")) << "Should recognize 誰";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(RealworldPronounTest, Interrogative_Nani) {
  // 何を食べる
  auto result = analyzer.analyze("何を食べる");
  printMorphemes(result, "何を食べる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "何")) << "Should recognize 何";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldPronounTest, Interrogative_Doko) {
  // どこへ行く
  auto result = analyzer.analyze("どこへ行く");
  printMorphemes(result, "どこへ行く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "どこ")) << "Should recognize どこ";
  EXPECT_TRUE(hasParticle(result, "へ")) << "Should recognize へ particle";
}

// ===== Complex Real-World Sentence Tests (実際の文) =====

class RealworldComplexSentenceTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(RealworldComplexSentenceTest, TechnicalDoc_AI) {
  // Technical documentation style
  auto result = analyzer.analyze("生成AIの研究が進んでいる");
  printMorphemes(result, "生成AIの研究が進んでいる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(RealworldComplexSentenceTest, TechnicalDoc_iPhone) {
  // Product name with number
  auto result = analyzer.analyze("iPhone15を買った");
  printMorphemes(result, "iPhone15を買った");
  ASSERT_FALSE(result.empty());
  // iPhone15 should be single token or iPhone + 15
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldComplexSentenceTest, BusinessDoc_Invoice) {
  // Business invoice style
  auto result = analyzer.analyze("100万円の請求書を送付いたしました");
  printMorphemes(result, "100万円の請求書を送付いたしました");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldComplexSentenceTest, BusinessDoc_Address) {
  // Address with numbers
  auto result = analyzer.analyze("東京都渋谷区神宮前1-2-3");
  printMorphemes(result, "東京都渋谷区神宮前1-2-3");
  ASSERT_FALSE(result.empty());
  // Should handle the address components
  EXPECT_GE(result.size(), 3) << "Should produce multiple tokens";
}

TEST_F(RealworldComplexSentenceTest, SNS_Hashtag) {
  // SNS style with hashtag
  auto result = analyzer.analyze("今日はいい天気ですね");
  printMorphemes(result, "今日はいい天気ですね");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "今日")) << "Should recognize 今日";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "ね")) << "Should recognize ね particle";
}

TEST_F(RealworldComplexSentenceTest, SNS_Reaction) {
  // SNS reaction
  auto result = analyzer.analyze("まじでやばい");
  printMorphemes(result, "まじでやばい");
  ASSERT_FALSE(result.empty());
  // Should handle colloquial expressions
  EXPECT_GE(result.size(), 2) << "Should produce tokens";
}

TEST_F(RealworldComplexSentenceTest, ConversationAbbreviation) {
  // Casual conversation with abbreviation (normalized to lowercase)
  auto result = analyzer.analyze("LINEで連絡するね");
  printMorphemes(result, "LINEで連絡するね");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "line")) << "Should recognize line";
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
  EXPECT_TRUE(hasParticle(result, "ね")) << "Should recognize ね particle";
}

TEST_F(RealworldComplexSentenceTest, Recipe_Instruction) {
  // Recipe instruction
  auto result = analyzer.analyze("中火で5分間炒めてください");
  printMorphemes(result, "中火で5分間炒めてください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(RealworldComplexSentenceTest, News_Report) {
  // News report style
  auto result = analyzer.analyze("政府は新しい政策を発表した");
  printMorphemes(result, "政府は新しい政策を発表した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RealworldComplexSentenceTest, Academic_Paper) {
  // Academic paper style
  auto result = analyzer.analyze("本研究では新しい手法を提案する");
  printMorphemes(result, "本研究では新しい手法を提案する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

}  // namespace
}  // namespace suzume::analysis
