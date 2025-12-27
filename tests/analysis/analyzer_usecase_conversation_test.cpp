
// Conversation use case analyzer tests (daily life, schedule, shopping, etc.)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::hasParticle;
using suzume::test::hasSurface;

// ===== Everyday Conversation Tests (日常会話) =====

class ConversationTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(ConversationTest, Weather) {
  // Weather small talk
  auto result = analyzer.analyze("今日は寒いですね");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "今日")) << "Should recognize 今日";
  EXPECT_TRUE(hasSurface(result, "寒い")) << "Should recognize 寒い";
}

TEST_F(ConversationTest, AskingDirections) {
  // Asking for directions
  auto result = analyzer.analyze("駅までどうやって行きますか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "駅")) << "Should recognize 駅 as noun";
  EXPECT_TRUE(hasParticle(result, "まで")) << "Should recognize まで";
}

TEST_F(ConversationTest, PoliteRequest) {
  // Polite request
  auto result = analyzer.analyze("ちょっと待ってください");
  ASSERT_FALSE(result.empty());
  bool found_matte = false;
  for (const auto& mor : result) {
    if (mor.surface.find("待") != std::string::npos) {
      found_matte = true;
      break;
    }
  }
  EXPECT_TRUE(found_matte) << "Should recognize waiting verb";
}

TEST_F(ConversationTest, ThankYou) {
  // Thank you variations
  auto result = analyzer.analyze("ありがとうございます");
  ASSERT_FALSE(result.empty());
}

TEST_F(ConversationTest, Greeting_Ohayou) {
  // Morning greeting
  auto result = analyzer.analyze("おはようございます");
  ASSERT_FALSE(result.empty());
}

TEST_F(ConversationTest, Greeting_Konnichiwa) {
  // Daytime greeting
  auto result = analyzer.analyze("こんにちは");
  ASSERT_FALSE(result.empty());
}

TEST_F(ConversationTest, Apology) {
  // Apology
  auto result = analyzer.analyze("すみませんでした");
  ASSERT_FALSE(result.empty());
}

TEST_F(ConversationTest, Question_Where) {
  // Where question
  auto result = analyzer.analyze("トイレはどこですか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "か")) << "Should recognize か particle";
}

TEST_F(ConversationTest, Question_What) {
  // What question
  auto result = analyzer.analyze("これは何ですか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "か")) << "Should recognize か particle";
}

TEST_F(ConversationTest, Desire_Want) {
  // Want expression
  auto result = analyzer.analyze("水が欲しいです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

// ===== Schedule/Appointment Tests (予定・約束) =====

class ScheduleConversationTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(ScheduleConversationTest, MeetingTime) {
  // Meeting schedule
  auto result = analyzer.analyze("明日の10時に会議があります");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "明日")) << "Should recognize 明日";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(ScheduleConversationTest, NextWeek) {
  // Next week appointment
  auto result = analyzer.analyze("来週の金曜日はいかがですか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "来週")) << "Should recognize 来週";
}

TEST_F(ScheduleConversationTest, Busy) {
  // Expressing busy schedule
  auto result = analyzer.analyze("今週は忙しいので来週にしましょう");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "今週")) << "Should recognize 今週";
  EXPECT_TRUE(hasSurface(result, "来週")) << "Should recognize 来週";
}

TEST_F(ScheduleConversationTest, Suggestion) {
  // Suggesting time
  auto result = analyzer.analyze("3時ごろはどうですか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(ScheduleConversationTest, Confirmation) {
  // Confirming appointment
  auto result = analyzer.analyze("明日の約束を確認したいのですが");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(ScheduleConversationTest, Cancel) {
  // Cancellation
  auto result = analyzer.analyze("予定をキャンセルしてもいいですか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

// ===== Shopping/Transaction Tests (買い物・取引) =====

class ShoppingTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(ShoppingTest, Price) {
  // Asking price
  auto result = analyzer.analyze("これはいくらですか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "か")) << "Should recognize か particle";
}

TEST_F(ShoppingTest, Payment) {
  // Payment method
  auto result = analyzer.analyze("カードで払えますか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(ShoppingTest, Quantity) {
  // Ordering quantity
  auto result = analyzer.analyze("これを3つください");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens";
}

TEST_F(ShoppingTest, Bag) {
  // Asking for bag
  auto result = analyzer.analyze("袋をお願いします");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(ShoppingTest, Receipt) {
  // Asking for receipt: 領収書をください
  auto result = analyzer.analyze("領収書をください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(ShoppingTest, Discount) {
  // Asking for discount
  auto result = analyzer.analyze("もう少し安くなりますか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "か")) << "Should recognize か particle";
}

TEST_F(ShoppingTest, Size) {
  // Size inquiry
  auto result = analyzer.analyze("大きいサイズはありますか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(ShoppingTest, Color) {
  // Color inquiry
  auto result = analyzer.analyze("他の色はありますか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

// ===== Travel/Transportation Tests (旅行・交通) =====

class TravelConversationTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(TravelConversationTest, Reservation) {
  // Reservation request
  auto result = analyzer.analyze("来週の金曜日に二名で予約したいのですが");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(TravelConversationTest, TrainAnnouncement) {
  // Train announcement
  auto result = analyzer.analyze("次は新宿、新宿です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "です")) << "Should recognize です";
}

TEST_F(TravelConversationTest, Delay) {
  // Delay announcement
  auto result = analyzer.analyze("電車が10分ほど遅れております");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(TravelConversationTest, Platform) {
  // Platform inquiry
  auto result = analyzer.analyze("東京行きは何番線ですか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(TravelConversationTest, Ticket) {
  // Ticket purchase
  auto result = analyzer.analyze("大阪までの切符を一枚ください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "まで")) << "Should recognize まで";
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(TravelConversationTest, Hotel) {
  // Hotel inquiry
  auto result = analyzer.analyze("今夜泊まれる部屋はありますか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

// ===== Restaurant Tests (レストラン) =====

class RestaurantTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(RestaurantTest, Order) {
  // Order
  auto result = analyzer.analyze("ラーメンを一つお願いします");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RestaurantTest, Menu) {
  // Menu inquiry
  auto result = analyzer.analyze("メニューを見せてください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(RestaurantTest, Recommendation) {
  // Recommendation
  auto result = analyzer.analyze("おすすめは何ですか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(RestaurantTest, Allergy) {
  // Allergy inquiry
  auto result = analyzer.analyze("卵は入っていますか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(RestaurantTest, Bill) {
  // Bill request
  auto result = analyzer.analyze("お会計をお願いします");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

}  // namespace
}  // namespace suzume::analysis
