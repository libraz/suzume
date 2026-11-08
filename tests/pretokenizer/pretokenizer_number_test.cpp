
// Pretokenizer tests for number patterns (date, currency, storage, version, percentage, time)

#include <gtest/gtest.h>

#include "normalize/normalizer.h"
#include "pretokenizer/pretokenizer.h"

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
  auto result = pretokenizer_.process("12月23日");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "12月23日");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Date);
}

TEST_F(PreTokenizerNumberTest, MatchDate_MonthDayRejectsInvalidMonth) {
  auto result = pretokenizer_.process("13月1日");
  EXPECT_TRUE(result.tokens.empty());
}

TEST_F(PreTokenizerNumberTest, MatchDate_ValidatesMonthAndDayWithOrWithoutYear) {
  struct DateCase {
    std::string_view text;
    std::string_view expected;
  };
  constexpr DateCase kCases[] = {
      {"1月1日", "1月1日"},
      {"12月31日", "12月31日"},
      {"0月1日", ""},
      {"13月1日", ""},
      {"1月0日", ""},
      {"1月32日", ""},
      {"2024年1月1日", "2024年1月1日"},
      {"2024年12月31日", "2024年12月31日"},
      {"2024年0月1日", "2024年"},
      {"2024年13月1日", "2024年"},
      {"2024年1月0日", "2024年1月"},
      {"2024年1月32日", "2024年1月"},
  };

  for (const auto& test_case : kCases) {
    auto result = pretokenizer_.process(test_case.text);
    if (test_case.expected.empty()) {
      EXPECT_TRUE(result.tokens.empty()) << test_case.text;
    } else {
      ASSERT_FALSE(result.tokens.empty()) << test_case.text;
      EXPECT_EQ(result.tokens.front().surface, test_case.expected) << test_case.text;
      EXPECT_EQ(result.tokens.front().type, PreTokenType::Date) << test_case.text;
    }
  }
}

TEST_F(PreTokenizerNumberTest, MatchTime_DurationWithMinutes) {
  auto result = pretokenizer_.process("1時間15分");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "1時間15分");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Time);
}

TEST_F(PreTokenizerNumberTest, MatchDate_MultipleInText) {
  auto result = pretokenizer_.process("2024年1月1日から2024年12月31日まで");
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

TEST_F(PreTokenizerNumberTest, MatchDate_DoesNotAcceptDecimalYear) {
  auto result = pretokenizer_.process("20.24年の予定");
  for (const auto& token : result.tokens) {
    EXPECT_NE(token.type, PreTokenType::Date);
  }
}

TEST_F(PreTokenizerNumberTest, MatchAddressNumberSurfaceMatchesNormalizedRange) {
  constexpr std::string_view kAddresses[] = {"1-2-3", "１-２-３", "1-２-3"};
  for (std::string_view address : kAddresses) {
    const auto normalized = normalize::Normalizer().normalize(address);
    ASSERT_TRUE(normalized.hasValue()) << address;
    const auto& text = normalized.value();
    auto result = pretokenizer_.process(text);
    ASSERT_EQ(result.tokens.size(), 1) << address;
    const auto& token = result.tokens.front();
    EXPECT_EQ(token.type, PreTokenType::Number) << address;
    EXPECT_EQ(token.surface, "1-2-3") << address;
    EXPECT_EQ(token.surface, text.substr(token.start, token.end - token.start)) << address;
  }
}

TEST_F(PreTokenizerNumberTest, MatchCounter_MonthAndPlaceSpellings) {
  constexpr std::string_view kCounters[] = {
      "1か月", "12か月", "1ヶ月", "12ヶ月", "1ヵ月", "12ヵ月", "1ケ月", "1箇月", "1か所", "12か所", "1ヶ所", "1箇所",
  };

  for (std::string_view counter : kCounters) {
    auto result = pretokenizer_.process(counter);
    ASSERT_EQ(result.tokens.size(), 1) << counter;
    EXPECT_EQ(result.tokens[0].surface, counter);
    EXPECT_EQ(result.tokens[0].type, PreTokenType::Counter);
  }
}

TEST_F(PreTokenizerNumberTest, MatchCounter_LeavesFollowingSuffixInSpan) {
  auto result = pretokenizer_.process("3か月後");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "3か月");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Counter);
  ASSERT_EQ(result.spans.size(), 1);
}

// ===== Currency tests =====

TEST_F(PreTokenizerNumberTest, MatchCurrency_Basic) {
  auto result = pretokenizer_.process("100円");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "100円");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Currency);
}

TEST_F(PreTokenizerNumberTest, MatchCurrency_ValidatesThousandsSeparator) {
  auto valid = pretokenizer_.process("1,234円");
  ASSERT_EQ(valid.tokens.size(), 1);
  EXPECT_EQ(valid.tokens[0].surface, "1,234円");
  EXPECT_EQ(valid.tokens[0].type, PreTokenType::Currency);

  auto invalid = pretokenizer_.process("1,23円");
  for (const auto& token : invalid.tokens) {
    EXPECT_NE(token.surface, "1,23円");
  }
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
  auto result = pretokenizer_.process("商品A: 1000円、商品B: 2000円");
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
  auto result = pretokenizer_.process("10時から12時まで");
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

TEST_F(PreTokenizerNumberTest, LongPlainDigitRunsRemainSingleSpans) {
  const std::string ascii_digits(32768, '1');
  const auto ascii_result = pretokenizer_.process(ascii_digits);
  ASSERT_EQ(ascii_result.spans.size(), 1U);
  EXPECT_EQ(ascii_result.spans[0].start, 0U);
  EXPECT_EQ(ascii_result.spans[0].end, ascii_digits.size());
  EXPECT_TRUE(ascii_result.tokens.empty());

  std::string fullwidth_digits;
  fullwidth_digits.reserve(4096 * std::string("１").size());
  for (size_t index = 0; index < 4096; ++index) {
    fullwidth_digits += "１";
  }
  const auto fullwidth_result = pretokenizer_.process(fullwidth_digits);
  ASSERT_EQ(fullwidth_result.spans.size(), 1U);
  EXPECT_EQ(fullwidth_result.spans[0].start, 0U);
  EXPECT_EQ(fullwidth_result.spans[0].end, fullwidth_digits.size());
  EXPECT_TRUE(fullwidth_result.tokens.empty());
}

}  // namespace
}  // namespace suzume::pretokenizer
