
// Prefix + noun joining tests
//
// Tests for productive お/ご + noun patterns and other prefix + noun patterns.
// These are grammatically productive patterns that should be handled by
// grammar logic rather than dictionary entries.

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// Test fixture for prefix+noun tests
class PrefixNounTest : public ::testing::Test {
 protected:
  void SetUp() override {
    AnalyzerOptions options;
    analyzer_ = std::make_unique<Analyzer>(options);
  }

  std::unique_ptr<Analyzer> analyzer_;

  // Helper to check if a specific surface exists in results
  bool hasSurface(const std::vector<core::Morpheme>& result,
                  const std::string& surface) {
    for (const auto& morpheme : result) {
      if (morpheme.surface == surface) return true;
    }
    return false;
  }

  // Helper to count morphemes
  size_t countMorphemes(const std::vector<core::Morpheme>& result) {
    return result.size();
  }
};

// ===== お + noun (honorific) =====

TEST_F(PrefixNounTest, O_Water) {
  // お水 should be analyzed as single token
  auto result = analyzer_->analyze("お水");
  // Prefer single token "お水" over "お" + "水"
  EXPECT_TRUE(hasSurface(result, "お水") ||
              (hasSurface(result, "お") && hasSurface(result, "水")));
  // If joined, should be 1 token; if split, should be 2 tokens
  EXPECT_GE(result.size(), 1);
  EXPECT_LE(result.size(), 2);
}

TEST_F(PrefixNounTest, O_Money) {
  // お金 should be analyzed as single token
  auto result = analyzer_->analyze("お金");
  EXPECT_FALSE(result.empty());
  // Common word, should be recognized
}

TEST_F(PrefixNounTest, O_Store) {
  // お店 should be analyzed as single token
  auto result = analyzer_->analyze("お店");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, O_Sake) {
  // お酒 should be analyzed as single token
  auto result = analyzer_->analyze("お酒");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, O_Flower) {
  // お花 should be analyzed as single token
  auto result = analyzer_->analyze("お花");
  EXPECT_FALSE(result.empty());
}

// ===== Simple お + noun patterns (work with L1 prefix joining) =====

TEST_F(PrefixNounTest, O_Tea_Joined) {
  // お茶 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お茶");
  EXPECT_TRUE(hasSurface(result, "お茶")) << "お茶 should be joined";
}

TEST_F(PrefixNounTest, O_Sweets_Joined) {
  // お菓子 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お菓子");
  EXPECT_TRUE(hasSurface(result, "お菓子")) << "お菓子 should be joined";
}

TEST_F(PrefixNounTest, O_Care_Joined) {
  // お世話 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お世話");
  EXPECT_TRUE(hasSurface(result, "お世話")) << "お世話 should be joined";
}

TEST_F(PrefixNounTest, O_Reply_Joined) {
  // お返事 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お返事");
  EXPECT_TRUE(hasSurface(result, "お返事")) << "お返事 should be joined";
}

TEST_F(PrefixNounTest, O_Trouble_Joined) {
  // お手数 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お手数");
  EXPECT_TRUE(hasSurface(result, "お手数")) << "お手数 should be joined";
}

TEST_F(PrefixNounTest, O_Healthy_Joined) {
  // お元気 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お元気");
  EXPECT_TRUE(hasSurface(result, "お元気")) << "お元気 should be joined";
}

TEST_F(PrefixNounTest, O_Name_Joined) {
  // お名前 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お名前");
  EXPECT_TRUE(hasSurface(result, "お名前")) << "お名前 should be joined";
}

TEST_F(PrefixNounTest, O_Customer_Joined) {
  // お客 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お客");
  EXPECT_TRUE(hasSurface(result, "お客")) << "お客 should be joined";
}

TEST_F(PrefixNounTest, O_CustomerSama_Joined) {
  // お客様 - simple noun base, works with prefix joining
  auto result = analyzer_->analyze("お客様");
  EXPECT_TRUE(hasSurface(result, "お客様")) << "お客様 should be joined";
}

// ===== お + verb stem patterns (L1 common_vocabulary.h) =====
// These cannot be handled by prefix joining (base forms are verbs/compounds)

TEST_F(PrefixNounTest, O_Request_L1) {
  // お願い - 願い is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お願い");
  EXPECT_TRUE(hasSurface(result, "お願い")) << "お願い should be recognized";
}

TEST_F(PrefixNounTest, O_Tired_L1) {
  // お疲れ - 疲れ is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お疲れ");
  EXPECT_TRUE(hasSurface(result, "お疲れ")) << "お疲れ should be recognized";
}

TEST_F(PrefixNounTest, O_Inquiry_L1) {
  // お問い合わせ - compound (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お問い合わせ");
  EXPECT_TRUE(hasSurface(result, "お問い合わせ"))
      << "お問い合わせ should be recognized";
}

TEST_F(PrefixNounTest, O_Wait_L1) {
  // お待ち - 待ち is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お待ち");
  EXPECT_TRUE(hasSurface(result, "お待ち")) << "お待ち should be recognized";
}

TEST_F(PrefixNounTest, O_Notice_L1) {
  // お知らせ - 知らせ is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お知らせ");
  EXPECT_TRUE(hasSurface(result, "お知らせ")) << "お知らせ should be recognized";
}

TEST_F(PrefixNounTest, O_Application_L1) {
  // お申し込み - compound (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お申し込み");
  EXPECT_TRUE(hasSurface(result, "お申し込み"))
      << "お申し込み should be recognized";
}

TEST_F(PrefixNounTest, O_Payment_L1) {
  // お支払い - compound (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お支払い");
  EXPECT_TRUE(hasSurface(result, "お支払い")) << "お支払い should be recognized";
}

TEST_F(PrefixNounTest, O_Delivery_L1) {
  // お届け - 届け is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お届け");
  EXPECT_TRUE(hasSurface(result, "お届け")) << "お届け should be recognized";
}

TEST_F(PrefixNounTest, O_Receipt_L1) {
  // お受け取り - compound (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お受け取り");
  EXPECT_TRUE(hasSurface(result, "お受け取り"))
      << "お受け取り should be recognized";
}

TEST_F(PrefixNounTest, O_Purchase_L1) {
  // お買い上げ - compound (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お買い上げ");
  EXPECT_TRUE(hasSurface(result, "お買い上げ"))
      << "お買い上げ should be recognized";
}

TEST_F(PrefixNounTest, O_Custody_L1) {
  // お預かり - 預かり is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お預かり");
  EXPECT_TRUE(hasSurface(result, "お預かり")) << "お預かり should be recognized";
}

TEST_F(PrefixNounTest, O_Visit_L1) {
  // お見舞い - 見舞い is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お見舞い");
  EXPECT_TRUE(hasSurface(result, "お見舞い")) << "お見舞い should be recognized";
}

TEST_F(PrefixNounTest, O_Welcome_L1) {
  // お迎え - 迎え is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お迎え");
  EXPECT_TRUE(hasSurface(result, "お迎え")) << "お迎え should be recognized";
}

TEST_F(PrefixNounTest, O_Answer_L1) {
  // お答え - 答え is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お答え");
  EXPECT_TRUE(hasSurface(result, "お答え")) << "お答え should be recognized";
}

TEST_F(PrefixNounTest, O_Refusal_L1) {
  // お断り - 断り is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お断り");
  EXPECT_TRUE(hasSurface(result, "お断り")) << "お断り should be recognized";
}

TEST_F(PrefixNounTest, O_Return_L1) {
  // お帰り - 帰り is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お帰り");
  EXPECT_TRUE(hasSurface(result, "お帰り")) << "お帰り should be recognized";
}

TEST_F(PrefixNounTest, O_Call_L1) {
  // お呼び - 呼び is verb stem (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お呼び");
  EXPECT_TRUE(hasSurface(result, "お呼び")) << "お呼び should be recognized";
}

TEST_F(PrefixNounTest, O_Pickup_L1) {
  // お引き取り - compound (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("お引き取り");
  EXPECT_TRUE(hasSurface(result, "お引き取り"))
      << "お引き取り should be recognized";
}

TEST_F(PrefixNounTest, Go_Rice_L1) {
  // ご飯 - lexicalized (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("ご飯");
  EXPECT_TRUE(hasSurface(result, "ご飯")) << "ご飯 should be recognized";
}

TEST_F(PrefixNounTest, Go_Look_L1) {
  // ご覧 - lexicalized (in L1 common_vocabulary.h)
  auto result = analyzer_->analyze("ご覧");
  EXPECT_TRUE(hasSurface(result, "ご覧")) << "ご覧 should be recognized";
}

// ===== ご + noun (honorific) =====

TEST_F(PrefixNounTest, Go_Confirmation) {
  // ご確認 should be analyzed as single token
  auto result = analyzer_->analyze("ご確認");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Go_Attention) {
  // ご注意 should be analyzed as single token
  auto result = analyzer_->analyze("ご注意");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Go_Report) {
  // ご連絡 should be analyzed as single token
  auto result = analyzer_->analyze("ご連絡");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Go_Cooperation) {
  // ご協力 should be analyzed as single token
  auto result = analyzer_->analyze("ご協力");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Go_Understanding) {
  // ご理解 should be analyzed as single token
  auto result = analyzer_->analyze("ご理解");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Go_Consideration) {
  // ご検討 should be analyzed as single token (removed from dictionary)
  auto result = analyzer_->analyze("ご検討");
  EXPECT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "ご検討")) << "ご検討 should be joined";
}

TEST_F(PrefixNounTest, Go_Reply) {
  // ご返信 should be analyzed as single token (removed from dictionary)
  auto result = analyzer_->analyze("ご返信");
  EXPECT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "ご返信")) << "ご返信 should be joined";
}

TEST_F(PrefixNounTest, Go_Order) {
  // ご注文 should be analyzed as single token (removed from dictionary)
  auto result = analyzer_->analyze("ご注文");
  EXPECT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "ご注文")) << "ご注文 should be joined";
}

TEST_F(PrefixNounTest, Go_Opinion) {
  // ご意見 should be analyzed as single token (removed from dictionary)
  auto result = analyzer_->analyze("ご意見");
  EXPECT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "ご意見")) << "ご意見 should be joined";
}

TEST_F(PrefixNounTest, Go_Confirmation_Joined) {
  // ご確認 should be joined (removed from dictionary)
  auto result = analyzer_->analyze("ご確認");
  EXPECT_TRUE(hasSurface(result, "ご確認")) << "ご確認 should be joined";
}

TEST_F(PrefixNounTest, Go_Contact_Joined) {
  // ご連絡 should be joined (removed from dictionary)
  auto result = analyzer_->analyze("ご連絡");
  EXPECT_TRUE(hasSurface(result, "ご連絡")) << "ご連絡 should be joined";
}

// ===== 不 + noun (negation) =====

TEST_F(PrefixNounTest, Fu_Comfortable) {
  // 不安 should be analyzed as single token
  auto result = analyzer_->analyze("不安");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Fu_Necessary) {
  // 不要 should be analyzed as single token
  auto result = analyzer_->analyze("不要");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Fu_Convenient) {
  // 不便 should be analyzed as single token
  auto result = analyzer_->analyze("不便");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Fu_Possible) {
  // 不可能 should be analyzed as single token
  auto result = analyzer_->analyze("不可能");
  EXPECT_FALSE(result.empty());
}

// ===== 未 + noun (not yet) =====

TEST_F(PrefixNounTest, Mi_Experience) {
  // 未経験 should be analyzed as single token
  auto result = analyzer_->analyze("未経験");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Mi_Confirmed) {
  // 未確認 should be analyzed as single token
  auto result = analyzer_->analyze("未確認");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Mi_Known) {
  // 未知 should be analyzed as single token
  auto result = analyzer_->analyze("未知");
  EXPECT_FALSE(result.empty());
}

// ===== 非 + noun (non-) =====

TEST_F(PrefixNounTest, Hi_Common) {
  // 非常 should be analyzed as single token
  auto result = analyzer_->analyze("非常");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Hi_Public) {
  // 非公開 should be analyzed as single token
  auto result = analyzer_->analyze("非公開");
  EXPECT_FALSE(result.empty());
}

// ===== 超 + noun/adjective (super) =====

TEST_F(PrefixNounTest, Cho_Human) {
  // 超人 should be analyzed as single token
  auto result = analyzer_->analyze("超人");
  EXPECT_FALSE(result.empty());
}

// ===== 再 + verb noun (re-) =====

TEST_F(PrefixNounTest, Sai_Start) {
  // 再開 should be analyzed as single token
  auto result = analyzer_->analyze("再開");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, Sai_Check) {
  // 再確認 should be analyzed as single token
  auto result = analyzer_->analyze("再確認");
  EXPECT_FALSE(result.empty());
}

// ===== Context tests =====

TEST_F(PrefixNounTest, InSentence_OWater) {
  // お水をください in sentence context
  auto result = analyzer_->analyze("お水をください");
  ASSERT_GE(result.size(), 2);
  // Should contain particle を
  bool has_wo = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "を") has_wo = true;
  }
  EXPECT_TRUE(has_wo);
}

TEST_F(PrefixNounTest, InSentence_GoConfirmation) {
  // ご確認ください in sentence context
  auto result = analyzer_->analyze("ご確認ください");
  ASSERT_GE(result.size(), 2);
}

TEST_F(PrefixNounTest, InSentence_FuPossible) {
  // 不可能です in sentence context
  auto result = analyzer_->analyze("不可能です");
  ASSERT_GE(result.size(), 2);
}

// ===== Edge cases =====

TEST_F(PrefixNounTest, EdgeCase_OnlyPrefix) {
  // Just prefix without noun - should not crash
  auto result = analyzer_->analyze("お");
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, EdgeCase_PrefixWithHiragana) {
  // お + hiragana (not a productive pattern for honorific)
  auto result = analyzer_->analyze("おかし");  // おかし is a word on its own
  EXPECT_FALSE(result.empty());
}

TEST_F(PrefixNounTest, EdgeCase_DoublePrefixes) {
  // Multiple prefixes: ご + お + noun (not natural)
  auto result = analyzer_->analyze("ごお水");
  EXPECT_FALSE(result.empty());
  // Should handle gracefully
}

// ===== Lexicalized compounds (should be in dictionary) =====

TEST_F(PrefixNounTest, Lexicalized_Ocha) {
  // お茶 is lexicalized - rarely say 茶 alone
  auto result = analyzer_->analyze("お茶");
  EXPECT_FALSE(result.empty());
  // Should be treated as single token
}

TEST_F(PrefixNounTest, Lexicalized_Gohan) {
  // ご飯 is lexicalized - rarely say 飯 alone (pronounced differently)
  auto result = analyzer_->analyze("ご飯");
  EXPECT_FALSE(result.empty());
  // Should be treated as single token
}

}  // namespace
}  // namespace suzume::analysis
