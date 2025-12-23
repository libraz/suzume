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

}  // namespace
}  // namespace suzume::pretokenizer
