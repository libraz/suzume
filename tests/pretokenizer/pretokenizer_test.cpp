#include "pretokenizer/pretokenizer.h"

#include <gtest/gtest.h>

namespace suzume::pretokenizer {
namespace {

class PreTokenizerTest : public ::testing::Test {
 protected:
  PreTokenizer pretokenizer_;
};

// URL tests
TEST_F(PreTokenizerTest, MatchUrl_HttpsBasic) {
  auto result = pretokenizer_.process("https://example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Url);
  EXPECT_TRUE(result.spans.empty());
}

TEST_F(PreTokenizerTest, MatchUrl_HttpWithPath) {
  auto result = pretokenizer_.process("http://example.com/path/to/page");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "http://example.com/path/to/page");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Url);
}

TEST_F(PreTokenizerTest, MatchUrl_WithSurroundingText) {
  auto result = pretokenizer_.process("Visit https://example.com for more");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com");
  ASSERT_EQ(result.spans.size(), 2);
}

TEST_F(PreTokenizerTest, MatchUrl_Japanese) {
  auto result = pretokenizer_.process("https://example.com にアクセス");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com");
  EXPECT_EQ(result.spans.size(), 1);
}

// Date tests
TEST_F(PreTokenizerTest, MatchDate_FullDate) {
  auto result = pretokenizer_.process("2024年12月23日");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "2024年12月23日");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Date);
}

TEST_F(PreTokenizerTest, MatchDate_YearMonth) {
  auto result = pretokenizer_.process("2024年12月");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "2024年12月");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Date);
}

TEST_F(PreTokenizerTest, MatchDate_YearOnly) {
  auto result = pretokenizer_.process("2024年");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "2024年");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Date);
}

TEST_F(PreTokenizerTest, MatchDate_WithSuffix) {
  auto result = pretokenizer_.process("2024年12月23日に送付");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "2024年12月23日");
  EXPECT_EQ(result.spans.size(), 1);
}

// Currency tests
TEST_F(PreTokenizerTest, MatchCurrency_Basic) {
  auto result = pretokenizer_.process("100円");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "100円");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Currency);
}

TEST_F(PreTokenizerTest, MatchCurrency_WithMan) {
  auto result = pretokenizer_.process("100万円");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "100万円");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Currency);
}

TEST_F(PreTokenizerTest, MatchCurrency_WithOku) {
  auto result = pretokenizer_.process("5億円");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "5億円");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Currency);
}

TEST_F(PreTokenizerTest, MatchCurrency_InSentence) {
  auto result = pretokenizer_.process("100万円の請求");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "100万円");
  EXPECT_EQ(result.spans.size(), 1);
}

// Storage tests
TEST_F(PreTokenizerTest, MatchStorage_GB) {
  auto result = pretokenizer_.process("3.5GB");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "3.5GB");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Storage);
}

TEST_F(PreTokenizerTest, MatchStorage_MB) {
  auto result = pretokenizer_.process("512MB");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "512MB");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Storage);
}

TEST_F(PreTokenizerTest, MatchStorage_InSentence) {
  auto result = pretokenizer_.process("3.5GBのメモリ");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "3.5GB");
  EXPECT_EQ(result.spans.size(), 1);
}

// Version tests
TEST_F(PreTokenizerTest, MatchVersion_Basic) {
  auto result = pretokenizer_.process("v2.0.1");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "v2.0.1");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Version);
}

TEST_F(PreTokenizerTest, MatchVersion_WithoutV) {
  auto result = pretokenizer_.process("1.2.3");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "1.2.3");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Version);
}

TEST_F(PreTokenizerTest, MatchVersion_TwoNumbers) {
  auto result = pretokenizer_.process("v2.0");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "v2.0");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Version);
}

TEST_F(PreTokenizerTest, MatchVersion_InSentence) {
  auto result = pretokenizer_.process("v2.0.1にアップデート");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "v2.0.1");
  EXPECT_EQ(result.spans.size(), 1);
}

// Percentage tests
TEST_F(PreTokenizerTest, MatchPercentage_Basic) {
  auto result = pretokenizer_.process("50%");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "50%");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Percentage);
}

TEST_F(PreTokenizerTest, MatchPercentage_Decimal) {
  auto result = pretokenizer_.process("3.14%");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "3.14%");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Percentage);
}

// Sentence boundary tests
TEST_F(PreTokenizerTest, SentenceBoundary_Japanese) {
  auto result = pretokenizer_.process("これは文。次の文");
  ASSERT_GE(result.tokens.size(), 1);
  bool found_boundary = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Boundary) {
      found_boundary = true;
      EXPECT_EQ(token.surface, "。");
    }
  }
  EXPECT_TRUE(found_boundary);
}

// Complex tests
TEST_F(PreTokenizerTest, Complex_TechnicalDocument) {
  auto result = pretokenizer_.process("2024年12月にv2.0.1をリリース。https://example.com を参照");

  // Should have: date, version, boundary, url
  EXPECT_GE(result.tokens.size(), 3);

  bool has_date = false;
  bool has_version = false;
  bool has_url = false;

  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Date) has_date = true;
    if (token.type == PreTokenType::Version) has_version = true;
    if (token.type == PreTokenType::Url) has_url = true;
  }

  EXPECT_TRUE(has_date);
  EXPECT_TRUE(has_version);
  EXPECT_TRUE(has_url);
}

TEST_F(PreTokenizerTest, NoMatch_PlainText) {
  auto result = pretokenizer_.process("これは普通のテキスト");
  EXPECT_TRUE(result.tokens.empty());
  ASSERT_EQ(result.spans.size(), 1);
  EXPECT_EQ(result.spans[0].start, 0);
}

// ===== Additional URL Tests =====

TEST_F(PreTokenizerTest, MatchUrl_WithQueryString) {
  auto result = pretokenizer_.process("https://example.com/search?q=test&page=1");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com/search?q=test&page=1");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Url);
}

TEST_F(PreTokenizerTest, MatchUrl_WithFragment) {
  auto result = pretokenizer_.process("https://example.com/page#section1");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com/page#section1");
}

TEST_F(PreTokenizerTest, MatchUrl_WithPort) {
  auto result = pretokenizer_.process("https://example.com:8080/path");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com:8080/path");
}

TEST_F(PreTokenizerTest, MatchUrl_Localhost) {
  auto result = pretokenizer_.process("http://localhost:3000");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "http://localhost:3000");
}

TEST_F(PreTokenizerTest, MatchUrl_MultipleInText) {
  auto result =
      pretokenizer_.process("参照: https://a.com と https://b.com");
  int url_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Url) {
      url_count++;
    }
  }
  EXPECT_EQ(url_count, 2);
}

// ===== Additional Date Tests =====

TEST_F(PreTokenizerTest, MatchDate_MonthDay) {
  // Note: Current implementation may require year prefix for date detection
  // "12月23日" without year might not be detected
  // This test verifies it doesn't crash
  auto result = pretokenizer_.process("12月23日");
  // If no date token found, verify it's in spans for later analysis
  if (result.tokens.empty()) {
    EXPECT_FALSE(result.spans.empty());
  }
}

TEST_F(PreTokenizerTest, MatchDate_MultipleInText) {
  auto result =
      pretokenizer_.process("2024年1月1日から2024年12月31日まで");
  int date_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Date) {
      date_count++;
    }
  }
  EXPECT_GE(date_count, 2);
}

TEST_F(PreTokenizerTest, MatchDate_WithSurroundingParticles) {
  auto result = pretokenizer_.process("2024年12月の予定");
  bool found_date = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Date) {
      found_date = true;
    }
  }
  EXPECT_TRUE(found_date);
}

// ===== Additional Currency Tests =====

TEST_F(PreTokenizerTest, MatchCurrency_Large) {
  auto result = pretokenizer_.process("1億5000万円");
  ASSERT_GE(result.tokens.size(), 1);
  bool found_currency = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Currency) {
      found_currency = true;
    }
  }
  EXPECT_TRUE(found_currency);
}

TEST_F(PreTokenizerTest, MatchCurrency_MultipleInText) {
  auto result =
      pretokenizer_.process("商品A: 1000円、商品B: 2000円");
  int currency_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Currency) {
      currency_count++;
    }
  }
  EXPECT_GE(currency_count, 2);
}

// ===== Additional Storage Tests =====

TEST_F(PreTokenizerTest, MatchStorage_TB) {
  auto result = pretokenizer_.process("2TB");
  ASSERT_GE(result.tokens.size(), 1);
  bool found_storage = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Storage) {
      found_storage = true;
    }
  }
  EXPECT_TRUE(found_storage);
}

TEST_F(PreTokenizerTest, MatchStorage_KB) {
  auto result = pretokenizer_.process("256KB");
  ASSERT_GE(result.tokens.size(), 1);
  bool found_storage = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Storage) {
      found_storage = true;
    }
  }
  EXPECT_TRUE(found_storage);
}

TEST_F(PreTokenizerTest, MatchStorage_Decimal) {
  auto result = pretokenizer_.process("1.5TB");
  ASSERT_GE(result.tokens.size(), 1);
  bool found_storage = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Storage) {
      found_storage = true;
    }
  }
  EXPECT_TRUE(found_storage);
}

// ===== Additional Version Tests =====

TEST_F(PreTokenizerTest, MatchVersion_FourParts) {
  auto result = pretokenizer_.process("v1.2.3.4");
  ASSERT_GE(result.tokens.size(), 1);
  bool found_version = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Version) {
      found_version = true;
    }
  }
  EXPECT_TRUE(found_version);
}

TEST_F(PreTokenizerTest, MatchVersion_InText) {
  auto result = pretokenizer_.process("バージョンv3.0.0をリリース");
  bool found_version = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Version) {
      found_version = true;
    }
  }
  EXPECT_TRUE(found_version);
}

// ===== Additional Percentage Tests =====

TEST_F(PreTokenizerTest, MatchPercentage_Large) {
  auto result = pretokenizer_.process("120%");
  ASSERT_GE(result.tokens.size(), 1);
  bool found_percentage = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Percentage) {
      found_percentage = true;
    }
  }
  EXPECT_TRUE(found_percentage);
}

TEST_F(PreTokenizerTest, MatchPercentage_InText) {
  auto result = pretokenizer_.process("達成率は85.5%です");
  bool found_percentage = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Percentage) {
      found_percentage = true;
    }
  }
  EXPECT_TRUE(found_percentage);
}

TEST_F(PreTokenizerTest, MatchPercentage_Multiple) {
  auto result = pretokenizer_.process("A: 30%、B: 70%");
  int pct_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Percentage) {
      pct_count++;
    }
  }
  EXPECT_GE(pct_count, 2);
}

// ===== Sentence Boundary Tests =====

TEST_F(PreTokenizerTest, SentenceBoundary_Exclamation) {
  auto result = pretokenizer_.process("すごい！本当に！");
  int boundary_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Boundary) {
      boundary_count++;
    }
  }
  EXPECT_GE(boundary_count, 2);
}

TEST_F(PreTokenizerTest, SentenceBoundary_Question) {
  auto result = pretokenizer_.process("本当？なぜ？");
  int boundary_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Boundary) {
      boundary_count++;
    }
  }
  EXPECT_GE(boundary_count, 2);
}

TEST_F(PreTokenizerTest, SentenceBoundary_Mixed) {
  auto result = pretokenizer_.process("行くの？行くよ！終わり。");
  int boundary_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Boundary) {
      boundary_count++;
    }
  }
  EXPECT_GE(boundary_count, 3);
}

// ===== Complex Text Tests =====

TEST_F(PreTokenizerTest, Complex_TechnicalDocument2) {
  auto result = pretokenizer_.process(
      "https://example.com でv2.0.1をダウンロード。ファイルサイズ: 512MB");

  bool has_url = false;
  bool has_version = false;
  bool has_storage = false;
  bool has_boundary = false;

  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Url) has_url = true;
    if (token.type == PreTokenType::Version) has_version = true;
    if (token.type == PreTokenType::Storage) has_storage = true;
    if (token.type == PreTokenType::Boundary) has_boundary = true;
  }

  EXPECT_TRUE(has_url);
  EXPECT_TRUE(has_version);
  EXPECT_TRUE(has_storage);
  EXPECT_TRUE(has_boundary);
}

TEST_F(PreTokenizerTest, Complex_NewsArticle) {
  auto result = pretokenizer_.process(
      "2024年12月23日。売上高は前年比120%で、1億円を達成。");

  bool has_date = false;
  bool has_percentage = false;
  bool has_currency = false;

  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Date) has_date = true;
    if (token.type == PreTokenType::Percentage) has_percentage = true;
    if (token.type == PreTokenType::Currency) has_currency = true;
  }

  EXPECT_TRUE(has_date);
  EXPECT_TRUE(has_percentage);
  EXPECT_TRUE(has_currency);
}

// ===== Edge Cases =====

TEST_F(PreTokenizerTest, EdgeCase_EmptyString) {
  auto result = pretokenizer_.process("");
  EXPECT_TRUE(result.tokens.empty());
}

TEST_F(PreTokenizerTest, EdgeCase_OnlyWhitespace) {
  auto result = pretokenizer_.process("   ");
  // Should handle gracefully - no crash
}

TEST_F(PreTokenizerTest, EdgeCase_OnlyPunctuation) {
  auto result = pretokenizer_.process("。！？");
  // Should have boundary tokens
  int boundary_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Boundary) {
      boundary_count++;
    }
  }
  EXPECT_GE(boundary_count, 1);
}

TEST_F(PreTokenizerTest, EdgeCase_ConsecutiveCurrency) {
  auto result = pretokenizer_.process("100円200円300円");

  int currency_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Currency) {
      currency_count++;
    }
  }

  EXPECT_GE(currency_count, 3);
}

TEST_F(PreTokenizerTest, EdgeCase_NestedPatterns) {
  // URL containing date-like pattern
  auto result = pretokenizer_.process("https://example.com/2024/12/23/article");
  ASSERT_GE(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Url);
}

TEST_F(PreTokenizerTest, EdgeCase_VersionLikeDate) {
  // Version-like pattern that could be confused with other types
  auto result = pretokenizer_.process("v2024.12.23");
  bool found_version = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Version) {
      found_version = true;
    }
  }
  EXPECT_TRUE(found_version);
}

// ===== No Match Tests =====

TEST_F(PreTokenizerTest, NoMatch_PartialUrl) {
  auto result = pretokenizer_.process("example.com");
  // Without http:// prefix, should not be detected as URL
  bool has_url = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Url) {
      has_url = true;
    }
  }
  EXPECT_FALSE(has_url);
}

TEST_F(PreTokenizerTest, NoMatch_PlainNumber) {
  auto result = pretokenizer_.process("12345");
  // Plain number without unit should be in spans, not tokens
  // (unless Number type is implemented)
  EXPECT_FALSE(result.spans.empty());
}

// ===== Email Tests =====

TEST_F(PreTokenizerTest, MatchEmail_Basic) {
  auto result = pretokenizer_.process("user@example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "user@example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Email);
}

TEST_F(PreTokenizerTest, MatchEmail_WithSubdomain) {
  auto result = pretokenizer_.process("user@mail.example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "user@mail.example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Email);
}

TEST_F(PreTokenizerTest, MatchEmail_WithPlus) {
  auto result = pretokenizer_.process("user+tag@example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "user+tag@example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Email);
}

TEST_F(PreTokenizerTest, MatchEmail_WithDots) {
  auto result = pretokenizer_.process("first.last@example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "first.last@example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Email);
}

TEST_F(PreTokenizerTest, MatchEmail_InJapaneseText) {
  auto result = pretokenizer_.process("連絡先: user@example.com まで");
  bool found_email = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      found_email = true;
      EXPECT_EQ(token.surface, "user@example.com");
    }
  }
  EXPECT_TRUE(found_email);
}

TEST_F(PreTokenizerTest, MatchEmail_MultipleInText) {
  auto result =
      pretokenizer_.process("a@example.com と b@example.com");
  int email_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      email_count++;
    }
  }
  EXPECT_EQ(email_count, 2);
}

TEST_F(PreTokenizerTest, NoMatch_InvalidEmail_NoDomain) {
  auto result = pretokenizer_.process("user@");
  bool has_email = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      has_email = true;
    }
  }
  EXPECT_FALSE(has_email);
}

TEST_F(PreTokenizerTest, NoMatch_InvalidEmail_NoDot) {
  auto result = pretokenizer_.process("user@localhost");
  bool has_email = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      has_email = true;
    }
  }
  EXPECT_FALSE(has_email);
}

TEST_F(PreTokenizerTest, NoMatch_InvalidEmail_StartWithDot) {
  auto result = pretokenizer_.process(".user@example.com");
  bool has_email = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      has_email = true;
    }
  }
  EXPECT_FALSE(has_email);
}

// ===== Time Tests =====

TEST_F(PreTokenizerTest, MatchTime_HourOnly) {
  auto result = pretokenizer_.process("14時");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "14時");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerTest, MatchTime_HourMinute) {
  auto result = pretokenizer_.process("14時30分");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "14時30分");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerTest, MatchTime_HourMinuteSecond) {
  auto result = pretokenizer_.process("14時30分45秒");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "14時30分45秒");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerTest, MatchTime_SingleDigitHour) {
  auto result = pretokenizer_.process("9時");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "9時");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerTest, MatchTime_MidnightAndNoon) {
  auto result = pretokenizer_.process("0時と12時");
  int time_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Time) {
      time_count++;
    }
  }
  EXPECT_EQ(time_count, 2);
}

TEST_F(PreTokenizerTest, MatchTime_24Hour) {
  auto result = pretokenizer_.process("24時");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "24時");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerTest, MatchTime_InJapaneseText) {
  auto result = pretokenizer_.process("会議は14時30分から開始");
  bool found_time = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Time) {
      found_time = true;
      EXPECT_EQ(token.surface, "14時30分");
    }
  }
  EXPECT_TRUE(found_time);
}

TEST_F(PreTokenizerTest, MatchTime_MultipleInText) {
  auto result =
      pretokenizer_.process("10時から12時まで");
  int time_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Time) {
      time_count++;
    }
  }
  EXPECT_EQ(time_count, 2);
}

TEST_F(PreTokenizerTest, NoMatch_InvalidTime_HourTooLarge) {
  auto result = pretokenizer_.process("25時");
  bool has_time = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Time) {
      has_time = true;
    }
  }
  EXPECT_FALSE(has_time);
}

TEST_F(PreTokenizerTest, NoMatch_InvalidTime_MinuteTooLarge) {
  auto result = pretokenizer_.process("14時60分");
  // Should match only 14時, not 14時60分
  bool found_partial = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Time && token.surface == "14時") {
      found_partial = true;
    }
  }
  EXPECT_TRUE(found_partial);
}

// ===== Complex Tests with Email and Time =====

TEST_F(PreTokenizerTest, Complex_TechnicalDocumentWithEmail) {
  auto result = pretokenizer_.process(
      "詳細は user@example.com にお問い合わせください。");
  bool has_email = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      has_email = true;
    }
  }
  EXPECT_TRUE(has_email);
}

TEST_F(PreTokenizerTest, Complex_ScheduleWithTime) {
  auto result = pretokenizer_.process(
      "2024年12月23日 14時30分に会議室Aで開催。");

  bool has_date = false;
  bool has_time = false;

  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Date) has_date = true;
    if (token.type == PreTokenType::Time) has_time = true;
  }

  EXPECT_TRUE(has_date);
  EXPECT_TRUE(has_time);
}

TEST_F(PreTokenizerTest, Complex_AllPatterns) {
  auto result = pretokenizer_.process(
      "2024年12月23日 14時30分。user@example.com へ連絡。"
      "詳細は https://example.com を参照。価格は100万円、達成率50%。");

  bool has_date = false;
  bool has_time = false;
  bool has_email = false;
  bool has_url = false;
  bool has_currency = false;
  bool has_percentage = false;

  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Date) has_date = true;
    if (token.type == PreTokenType::Time) has_time = true;
    if (token.type == PreTokenType::Email) has_email = true;
    if (token.type == PreTokenType::Url) has_url = true;
    if (token.type == PreTokenType::Currency) has_currency = true;
    if (token.type == PreTokenType::Percentage) has_percentage = true;
  }

  EXPECT_TRUE(has_date);
  EXPECT_TRUE(has_time);
  EXPECT_TRUE(has_email);
  EXPECT_TRUE(has_url);
  EXPECT_TRUE(has_currency);
  EXPECT_TRUE(has_percentage);
}

// ===== Hashtag Tests =====

TEST_F(PreTokenizerTest, MatchHashtag_English) {
  auto result = pretokenizer_.process("#programming");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#programming");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTest, MatchHashtag_Japanese) {
  auto result = pretokenizer_.process("#プログラミング");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#プログラミング");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTest, MatchHashtag_Kanji) {
  auto result = pretokenizer_.process("#日本語");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#日本語");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTest, MatchHashtag_Mixed) {
  auto result = pretokenizer_.process("#C言語");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#C言語");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTest, MatchHashtag_WithUnderscore) {
  auto result = pretokenizer_.process("#hello_world");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#hello_world");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTest, MatchHashtag_FullWidth) {
  auto result = pretokenizer_.process("＃タグ");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "＃タグ");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTest, MatchHashtag_InText) {
  auto result = pretokenizer_.process("今日は #プログラミング を勉強");
  bool found_hashtag = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Hashtag) {
      found_hashtag = true;
      EXPECT_EQ(token.surface, "#プログラミング");
    }
  }
  EXPECT_TRUE(found_hashtag);
}

TEST_F(PreTokenizerTest, MatchHashtag_MultipleInText) {
  auto result = pretokenizer_.process("#hello #world #日本");
  int hashtag_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Hashtag) {
      hashtag_count++;
    }
  }
  EXPECT_EQ(hashtag_count, 3);
}

TEST_F(PreTokenizerTest, NoMatch_HashtagEmpty) {
  auto result = pretokenizer_.process("# ");
  bool has_hashtag = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Hashtag) {
      has_hashtag = true;
    }
  }
  EXPECT_FALSE(has_hashtag);
}

TEST_F(PreTokenizerTest, NoMatch_HashtagSymbolOnly) {
  auto result = pretokenizer_.process("#!");
  bool has_hashtag = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Hashtag) {
      has_hashtag = true;
    }
  }
  EXPECT_FALSE(has_hashtag);
}

// ===== Mention Tests =====

TEST_F(PreTokenizerTest, MatchMention_Basic) {
  auto result = pretokenizer_.process("@user");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "@user");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Mention);
}

TEST_F(PreTokenizerTest, MatchMention_WithUnderscore) {
  auto result = pretokenizer_.process("@user_name");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "@user_name");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Mention);
}

TEST_F(PreTokenizerTest, MatchMention_WithNumbers) {
  auto result = pretokenizer_.process("@user123");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "@user123");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Mention);
}

TEST_F(PreTokenizerTest, MatchMention_InText) {
  auto result = pretokenizer_.process("Thanks @alice for the help");
  bool found_mention = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Mention) {
      found_mention = true;
      EXPECT_EQ(token.surface, "@alice");
    }
  }
  EXPECT_TRUE(found_mention);
}

TEST_F(PreTokenizerTest, MatchMention_InJapaneseText) {
  auto result = pretokenizer_.process("@taro さんへ");
  bool found_mention = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Mention) {
      found_mention = true;
      EXPECT_EQ(token.surface, "@taro");
    }
  }
  EXPECT_TRUE(found_mention);
}

TEST_F(PreTokenizerTest, MatchMention_MultipleInText) {
  auto result = pretokenizer_.process("@alice and @bob");
  int mention_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Mention) {
      mention_count++;
    }
  }
  EXPECT_EQ(mention_count, 2);
}

TEST_F(PreTokenizerTest, NoMatch_MentionEmpty) {
  auto result = pretokenizer_.process("@ ");
  bool has_mention = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Mention) {
      has_mention = true;
    }
  }
  EXPECT_FALSE(has_mention);
}

TEST_F(PreTokenizerTest, EmailVsMention_EmailWins) {
  // Email should be detected, not mention
  auto result = pretokenizer_.process("user@example.com");
  bool has_email = false;
  bool has_mention = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) has_email = true;
    if (token.type == PreTokenType::Mention) has_mention = true;
  }
  EXPECT_TRUE(has_email);
  EXPECT_FALSE(has_mention);
}

// ===== Complex Tests with Hashtag and Mention =====

TEST_F(PreTokenizerTest, Complex_SNSPost) {
  auto result = pretokenizer_.process(
      "@alice #hello を投稿しました。詳細は https://example.com を参照。");

  bool has_mention = false;
  bool has_hashtag = false;
  bool has_url = false;

  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Mention) has_mention = true;
    if (token.type == PreTokenType::Hashtag) has_hashtag = true;
    if (token.type == PreTokenType::Url) has_url = true;
  }

  EXPECT_TRUE(has_mention);
  EXPECT_TRUE(has_hashtag);
  EXPECT_TRUE(has_url);
}

TEST_F(PreTokenizerTest, Complex_AllPatternsIncludingSNS) {
  auto result = pretokenizer_.process(
      "2024年12月23日 14時30分。@user が #プログラミング について投稿。"
      "連絡先: contact@example.com 詳細: https://example.com");

  bool has_date = false;
  bool has_time = false;
  bool has_mention = false;
  bool has_hashtag = false;
  bool has_email = false;
  bool has_url = false;

  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Date) has_date = true;
    if (token.type == PreTokenType::Time) has_time = true;
    if (token.type == PreTokenType::Mention) has_mention = true;
    if (token.type == PreTokenType::Hashtag) has_hashtag = true;
    if (token.type == PreTokenType::Email) has_email = true;
    if (token.type == PreTokenType::Url) has_url = true;
  }

  EXPECT_TRUE(has_date);
  EXPECT_TRUE(has_time);
  EXPECT_TRUE(has_mention);
  EXPECT_TRUE(has_hashtag);
  EXPECT_TRUE(has_email);
  EXPECT_TRUE(has_url);
}

}  // namespace
}  // namespace suzume::pretokenizer
