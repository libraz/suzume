
// Business use case analyzer tests (emails, documents, finance, legal, etc.)
// Based on design_v2_practical.md and edge_cases.md

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::hasParticle;
using suzume::test::hasSurface;

// ===== Business Email Tests (ビジネスメール) =====

class BusinessEmailTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(BusinessEmailTest, Greeting_Osewa) {
  // Common business email opening: お世話になっております
  auto result = analyzer.analyze("お世話になっております");
  ASSERT_FALSE(result.empty());
  // Should contain the honorific prefix お or parse お世話 as unit
  bool found = hasSurface(result, "お") || hasSurface(result, "お世話");
  EXPECT_TRUE(found) << "Should recognize お prefix or お世話";
}

TEST_F(BusinessEmailTest, Greeting_Otsukaresama) {
  // Internal greeting: お疲れ様です
  auto result = analyzer.analyze("お疲れ様です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "です")) << "Should recognize です";
}

TEST_F(BusinessEmailTest, Request_GoKakunin) {
  // Polite request: ご確認ください
  auto result = analyzer.analyze("ご確認ください");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "ご") || hasSurface(result, "ご確認");
  EXPECT_TRUE(found) << "Should recognize ご prefix or ご確認";
}

TEST_F(BusinessEmailTest, Request_GoKentou) {
  // Polite request: ご検討のほどよろしくお願いいたします
  auto result = analyzer.analyze("ご検討のほどよろしくお願いいたします");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(BusinessEmailTest, Closing_Yoroshiku) {
  // Standard closing: よろしくお願いいたします
  auto result = analyzer.analyze("よろしくお願いいたします");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2);
}

TEST_F(BusinessEmailTest, Closing_Ijouyoroshiku) {
  // Closing: 以上、よろしくお願いします
  auto result = analyzer.analyze("以上、よろしくお願いします");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "以上")) << "Should recognize 以上";
}

TEST_F(BusinessEmailTest, Attachment_Tenpu) {
  // Attachment notification: 資料を添付いたしましたので、ご確認ください
  auto result =
      analyzer.analyze("資料を添付いたしましたので、ご確認ください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
  EXPECT_TRUE(hasParticle(result, "ので")) << "Should recognize ので";
}

TEST_F(BusinessEmailTest, Response_Request) {
  // Response request: ご返信いただけますと幸いです
  auto result = analyzer.analyze("ご返信いただけますと幸いです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "です")) << "Should recognize です";
}

TEST_F(BusinessEmailTest, Apology_Moushiwake) {
  // Apology: 申し訳ございませんが
  auto result = analyzer.analyze("申し訳ございませんが");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(BusinessEmailTest, Question_Ikaga) {
  // Inquiry: いかがでしょうか
  auto result = analyzer.analyze("いかがでしょうか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "か")) << "Should recognize か particle";
}

// ===== Business Document Tests (ビジネス文書) =====

class BusinessDocumentTest : public ::testing::Test {
 protected:
  // Use Suzume for full pipeline including postprocessor
  // This correctly handles date patterns with noun compound merging
  SuzumeOptions options;
  Suzume analyzer{options};
};

TEST_F(BusinessDocumentTest, Date_FullFormat) {
  // Full date format: 2024年12月23日付けで
  auto result = analyzer.analyze("2024年12月23日付けで");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(BusinessDocumentTest, Currency_Hyakuman) {
  // Currency: 100万円の請求書
  auto result = analyzer.analyze("100万円の請求書");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(BusinessDocumentTest, Currency_Oku) {
  // Large currency: 3億5000万円の売上
  auto result = analyzer.analyze("3億5000万円の売上");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(BusinessDocumentTest, Company_Kabushiki) {
  // Company name: 株式会社ABCの田中太郎様
  auto result = analyzer.analyze("株式会社ABCの田中様");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(BusinessDocumentTest, Address_Tokyo) {
  // Address: 東京都渋谷区に所在する
  auto result = analyzer.analyze("東京都渋谷区に所在する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(BusinessDocumentTest, Deadline_Made) {
  // Deadline: 今月末までに提出してください
  auto result = analyzer.analyze("今月末までに提出してください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "まで")) << "Should recognize まで";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(BusinessDocumentTest, NiTsuite_Topic) {
  // Topic marker: 契約内容について確認する
  auto result = analyzer.analyze("契約内容について確認する");
  ASSERT_FALSE(result.empty());
  // について should be recognized (compound particle or に+ついて)
  bool found = hasSurface(result, "について") || hasParticle(result, "に");
  EXPECT_TRUE(found) << "Should recognize について or に";
}

TEST_F(BusinessDocumentTest, NiKanshite_Topic) {
  // Topic marker: 本件に関して報告する
  auto result = analyzer.analyze("本件に関して報告する");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "に関して") || hasParticle(result, "に");
  EXPECT_TRUE(found) << "Should recognize に関して or に";
}

TEST_F(BusinessDocumentTest, Toshite_Capacity) {
  // Capacity: 代表者として署名する
  auto result = analyzer.analyze("代表者として署名する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "として")) << "Should recognize として particle";
}

// ===== Schedule/Meeting Tests (予定・会議) =====

class ScheduleTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(ScheduleTest, MeetingTime) {
  // Meeting time: 明日の10時に会議があります
  auto result = analyzer.analyze("明日の10時に会議があります");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "明日")) << "Should recognize 明日";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(ScheduleTest, NextWeek) {
  // Next week: 来週の月曜日に打ち合わせ
  auto result = analyzer.analyze("来週の月曜日に打ち合わせ");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "来週")) << "Should recognize 来週";
}

TEST_F(ScheduleTest, ThisMonth) {
  // This month: 今月の予定を確認する
  auto result = analyzer.analyze("今月の予定を確認する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "今月")) << "Should recognize 今月";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(ScheduleTest, Postpone) {
  // Postponement: 会議を来週に延期します
  auto result = analyzer.analyze("会議を来週に延期します");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(ScheduleTest, TimeRange) {
  // Time range: 14時から16時まで
  auto result = analyzer.analyze("14時から16時まで");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "から")) << "Should recognize から";
  EXPECT_TRUE(hasParticle(result, "まで")) << "Should recognize まで";
}

// ===== Finance Tests (金融・経理) =====

class FinanceTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(FinanceTest, Transaction) {
  // Transaction: お振込みは翌営業日に反映されます
  auto result = analyzer.analyze("お振込みは翌営業日に反映されます");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(FinanceTest, Interest) {
  // Interest rate: 金利は年率0.5%です
  auto result = analyzer.analyze("金利は年率0.5%です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(FinanceTest, Budget) {
  // Budget: 予算は500万円を予定しています
  auto result = analyzer.analyze("予算は500万円を予定しています");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(FinanceTest, Invoice) {
  // Invoice: 請求書を発行いたします
  auto result = analyzer.analyze("請求書を発行いたします");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(FinanceTest, Payment) {
  // Payment: お支払いは月末締め翌月払いです
  auto result = analyzer.analyze("お支払いは月末締め翌月払いです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

// ===== Legal/Contract Tests (法務・契約) =====

class LegalTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(LegalTest, TermsOfService) {
  // Terms: 本サービスの利用に際して
  auto result = analyzer.analyze("本サービスの利用に際して");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(LegalTest, Prohibition) {
  // Prohibition: 以下の行為を禁止します
  auto result = analyzer.analyze("以下の行為を禁止します");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(LegalTest, Contract) {
  // Contract: 甲は乙に対して責任を負う
  auto result = analyzer.analyze("甲は乙に対して責任を負う");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(LegalTest, NiYotte_Cause) {
  // Cause: 法律によって定められた
  auto result = analyzer.analyze("法律によって定められた");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "によって") || hasParticle(result, "に");
  EXPECT_TRUE(found) << "Should recognize によって or に";
}

TEST_F(LegalTest, NiOite_Place) {
  // Place: 本契約において定める
  auto result = analyzer.analyze("本契約において定める");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "において") || hasParticle(result, "に");
  EXPECT_TRUE(found) << "Should recognize において or に";
}

TEST_F(LegalTest, NiMotozuite_Basis) {
  // Basis: 規約に基づいて処理する
  auto result = analyzer.analyze("規約に基づいて処理する");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "に基づいて") || hasParticle(result, "に");
  EXPECT_TRUE(found) << "Should recognize に基づいて or に";
}

// ===== Customer Service Tests (カスタマーサービス) =====

class CustomerServiceTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(CustomerServiceTest, Inquiry) {
  // Inquiry: 商品がまだ届いていないのですが
  auto result = analyzer.analyze("商品がまだ届いていないのですが");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(CustomerServiceTest, Apology) {
  // Apology: 大変申し訳ございませんでした
  auto result = analyzer.analyze("大変申し訳ございませんでした");
  ASSERT_FALSE(result.empty());
}

TEST_F(CustomerServiceTest, ReturnRequest) {
  // Return: 返品の手続きについて教えてください
  auto result = analyzer.analyze("返品の手続きについて教えてください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(CustomerServiceTest, Confirmation) {
  // Confirmation: ご注文内容を確認させていただきます
  auto result = analyzer.analyze("ご注文内容を確認させていただきます");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(CustomerServiceTest, Contact) {
  // Contact: お問い合わせありがとうございます
  auto result = analyzer.analyze("お問い合わせありがとうございます");
  ASSERT_FALSE(result.empty());
}

// ===== Formal Announcement Tests (公式発表) =====

class AnnouncementTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(AnnouncementTest, Notice) {
  // Notice: 下記の通りお知らせいたします
  auto result = analyzer.analyze("下記の通りお知らせいたします");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(AnnouncementTest, Decision) {
  // Decision: 以下の事項を決定しました
  auto result = analyzer.analyze("以下の事項を決定しました");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(AnnouncementTest, NiYoruto_Citation) {
  // Citation: 報告によると問題はない
  auto result = analyzer.analyze("報告によると問題はない");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "によると") || hasParticle(result, "に");
  EXPECT_TRUE(found) << "Should recognize によると or に";
}

TEST_F(AnnouncementTest, NiTotte_Viewpoint) {
  // Viewpoint: 会社にとって重要な決定
  auto result = analyzer.analyze("会社にとって重要な決定");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "にとって") || hasParticle(result, "に");
  EXPECT_TRUE(found) << "Should recognize にとって or に";
}

// ===== Number + Counter Tests (数値+助数詞) =====
// From edge_cases.md Section 3

class NumberCounterTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(NumberCounterTest, People) {
  // People counter: 3人で行く
  auto result = analyzer.analyze("3人で行く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(NumberCounterTest, Times) {
  // Times counter: 5回目の挑戦
  auto result = analyzer.analyze("5回目の挑戦");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(NumberCounterTest, Items) {
  // Item counter: 10個の商品を
  auto result = analyzer.analyze("10個の商品を");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(NumberCounterTest, Percent) {
  // Percentage: 売上が20%増加した
  auto result = analyzer.analyze("売上が20%増加した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

// ===== Compound Particle Edge Cases (複合助詞) =====
// From edge_cases.md Section 2.2

class CompoundParticleTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(CompoundParticleTest, NitsuiteWa) {
  // については as compound: この件については後日
  auto result = analyzer.analyze("この件については後日");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(CompoundParticleTest, ToShite) {
  // として: 代表者として署名する
  auto result = analyzer.analyze("代表者として署名する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "として")) << "Should recognize として particle";
}

TEST_F(CompoundParticleTest, WoMotte) {
  // をもって: 本日をもって終了
  auto result = analyzer.analyze("本日をもって終了");
  ASSERT_FALSE(result.empty());
  bool found = hasSurface(result, "をもって") || hasParticle(result, "を");
  EXPECT_TRUE(found) << "Should recognize をもって or を";
}

}  // namespace
}  // namespace suzume::analysis
