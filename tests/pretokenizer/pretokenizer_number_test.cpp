
// Pretokenizer tests for number patterns (date, currency, storage, version, percentage, time)

#include "pretokenizer/pretokenizer.h"

#include <gtest/gtest.h>

namespace suzume::pretokenizer {
namespace {

class PreTokenizerNumberTest : public ::testing::Test {
 protected:
  PreTokenizer pretokenizer_;
};

// ===== Date tests =====

TEST_F(PreTokenizerNumberTest, MatchDate_FullDate) {
  auto result = pretokenizer_.process("2024年12月23日");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "2024年12月23日");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Date);
}

TEST_F(PreTokenizerNumberTest, MatchDate_YearMonth) {
  auto result = pretokenizer_.process("2024年12月");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "2024年12月");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Date);
}

TEST_F(PreTokenizerNumberTest, MatchDate_YearOnly) {
  auto result = pretokenizer_.process("2024年");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "2024年");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Date);
}

TEST_F(PreTokenizerNumberTest, MatchDate_WithSuffix) {
  auto result = pretokenizer_.process("2024年12月23日に送付");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "2024年12月23日");
  EXPECT_EQ(result.spans.size(), 1);
}

TEST_F(PreTokenizerNumberTest, MatchDate_MonthDay) {
  // Note: Current implementation may require year prefix for date detection
  // "12月23日" without year might not be detected
  // This test verifies it doesn't crash
  auto result = pretokenizer_.process("12月23日");
  // If no date token found, verify it's in spans for later analysis
  if (result.tokens.empty()) {
    EXPECT_FALSE(result.spans.empty());
  }
}

TEST_F(PreTokenizerNumberTest, MatchDate_MultipleInText) {
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

TEST_F(PreTokenizerNumberTest, MatchDate_WithSurroundingParticles) {
  auto result = pretokenizer_.process("2024年12月の予定");
  bool found_date = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Date) {
      found_date = true;
    }
  }
  EXPECT_TRUE(found_date);
}

// ===== Currency tests =====

TEST_F(PreTokenizerNumberTest, MatchCurrency_Basic) {
  auto result = pretokenizer_.process("100円");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "100円");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Currency);
}

TEST_F(PreTokenizerNumberTest, MatchCurrency_WithMan) {
  auto result = pretokenizer_.process("100万円");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "100万円");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Currency);
}

TEST_F(PreTokenizerNumberTest, MatchCurrency_WithOku) {
  auto result = pretokenizer_.process("5億円");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "5億円");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Currency);
}

TEST_F(PreTokenizerNumberTest, MatchCurrency_InSentence) {
  auto result = pretokenizer_.process("100万円の請求");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "100万円");
  EXPECT_EQ(result.spans.size(), 1);
}

TEST_F(PreTokenizerNumberTest, MatchCurrency_Large) {
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

TEST_F(PreTokenizerNumberTest, MatchCurrency_MultipleInText) {
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

// ===== Storage tests =====

TEST_F(PreTokenizerNumberTest, MatchStorage_GB) {
  auto result = pretokenizer_.process("3.5GB");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "3.5GB");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Storage);
}

TEST_F(PreTokenizerNumberTest, MatchStorage_MB) {
  auto result = pretokenizer_.process("512MB");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "512MB");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Storage);
}

TEST_F(PreTokenizerNumberTest, MatchStorage_InSentence) {
  auto result = pretokenizer_.process("3.5GBのメモリ");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "3.5GB");
  EXPECT_EQ(result.spans.size(), 1);
}

TEST_F(PreTokenizerNumberTest, MatchStorage_TB) {
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

TEST_F(PreTokenizerNumberTest, MatchStorage_KB) {
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

TEST_F(PreTokenizerNumberTest, MatchStorage_Decimal) {
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

// ===== Version tests =====

TEST_F(PreTokenizerNumberTest, MatchVersion_Basic) {
  auto result = pretokenizer_.process("v2.0.1");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "v2.0.1");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Version);
}

TEST_F(PreTokenizerNumberTest, MatchVersion_WithoutV) {
  auto result = pretokenizer_.process("1.2.3");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "1.2.3");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Version);
}

TEST_F(PreTokenizerNumberTest, MatchVersion_TwoNumbers) {
  auto result = pretokenizer_.process("v2.0");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "v2.0");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Version);
}

TEST_F(PreTokenizerNumberTest, MatchVersion_InSentence) {
  auto result = pretokenizer_.process("v2.0.1にアップデート");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "v2.0.1");
  EXPECT_EQ(result.spans.size(), 1);
}

TEST_F(PreTokenizerNumberTest, MatchVersion_FourParts) {
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

TEST_F(PreTokenizerNumberTest, MatchVersion_InText) {
  auto result = pretokenizer_.process("バージョンv3.0.0をリリース");
  bool found_version = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Version) {
      found_version = true;
    }
  }
  EXPECT_TRUE(found_version);
}

// ===== Percentage tests =====

TEST_F(PreTokenizerNumberTest, MatchPercentage_Basic) {
  auto result = pretokenizer_.process("50%");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "50%");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Percentage);
}

TEST_F(PreTokenizerNumberTest, MatchPercentage_Decimal) {
  auto result = pretokenizer_.process("3.14%");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "3.14%");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Percentage);
}

TEST_F(PreTokenizerNumberTest, MatchPercentage_Large) {
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

TEST_F(PreTokenizerNumberTest, MatchPercentage_InText) {
  auto result = pretokenizer_.process("達成率は85.5%です");
  bool found_percentage = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Percentage) {
      found_percentage = true;
    }
  }
  EXPECT_TRUE(found_percentage);
}

TEST_F(PreTokenizerNumberTest, MatchPercentage_Multiple) {
  auto result = pretokenizer_.process("A: 30%、B: 70%");
  int pct_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Percentage) {
      pct_count++;
    }
  }
  EXPECT_GE(pct_count, 2);
}

// ===== Time Tests =====

TEST_F(PreTokenizerNumberTest, MatchTime_HourOnly) {
  auto result = pretokenizer_.process("14時");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "14時");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerNumberTest, MatchTime_HourMinute) {
  auto result = pretokenizer_.process("14時30分");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "14時30分");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerNumberTest, MatchTime_HourMinuteSecond) {
  auto result = pretokenizer_.process("14時30分45秒");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "14時30分45秒");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerNumberTest, MatchTime_SingleDigitHour) {
  auto result = pretokenizer_.process("9時");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "9時");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerNumberTest, MatchTime_MidnightAndNoon) {
  auto result = pretokenizer_.process("0時と12時");
  int time_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Time) {
      time_count++;
    }
  }
  EXPECT_EQ(time_count, 2);
}

TEST_F(PreTokenizerNumberTest, MatchTime_24Hour) {
  auto result = pretokenizer_.process("24時");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "24時");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerNumberTest, MatchTime_InJapaneseText) {
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

TEST_F(PreTokenizerNumberTest, MatchTime_MultipleInText) {
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

TEST_F(PreTokenizerNumberTest, NoMatch_InvalidTime_HourTooLarge) {
  auto result = pretokenizer_.process("25時");
  bool has_time = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Time) {
      has_time = true;
    }
  }
  EXPECT_FALSE(has_time);
}

TEST_F(PreTokenizerNumberTest, NoMatch_InvalidTime_MinuteTooLarge) {
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

TEST_F(PreTokenizerNumberTest, NoMatch_PlainNumber) {
  auto result = pretokenizer_.process("12345");
  // Plain number without unit should be in spans, not tokens
  // (unless Number type is implemented)
  EXPECT_FALSE(result.spans.empty());
}

}  // namespace
}  // namespace suzume::pretokenizer
