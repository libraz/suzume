
// Pretokenizer tests for text patterns (sentence boundary, hashtag, mention,
// complex cases, edge cases)

#include "pretokenizer/pretokenizer.h"

#include <gtest/gtest.h>

namespace suzume::pretokenizer {
namespace {

class PreTokenizerTextTest : public ::testing::Test {
 protected:
  PreTokenizer pretokenizer_;
};

// ===== Sentence Boundary Tests =====

TEST_F(PreTokenizerTextTest, SentenceBoundary_Japanese) {
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

TEST_F(PreTokenizerTextTest, SentenceBoundary_Exclamation) {
  auto result = pretokenizer_.process("すごい！本当に！");
  int boundary_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Boundary) {
      boundary_count++;
    }
  }
  EXPECT_GE(boundary_count, 2);
}

TEST_F(PreTokenizerTextTest, SentenceBoundary_Question) {
  auto result = pretokenizer_.process("本当？なぜ？");
  int boundary_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Boundary) {
      boundary_count++;
    }
  }
  EXPECT_GE(boundary_count, 2);
}

TEST_F(PreTokenizerTextTest, SentenceBoundary_Mixed) {
  auto result = pretokenizer_.process("行くの？行くよ！終わり。");
  int boundary_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Boundary) {
      boundary_count++;
    }
  }
  EXPECT_GE(boundary_count, 3);
}

// ===== Hashtag Tests =====

TEST_F(PreTokenizerTextTest, MatchHashtag_English) {
  auto result = pretokenizer_.process("#programming");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#programming");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTextTest, MatchHashtag_Japanese) {
  auto result = pretokenizer_.process("#プログラミング");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#プログラミング");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTextTest, MatchHashtag_Kanji) {
  auto result = pretokenizer_.process("#日本語");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#日本語");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTextTest, MatchHashtag_Mixed) {
  auto result = pretokenizer_.process("#C言語");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#C言語");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTextTest, MatchHashtag_WithUnderscore) {
  auto result = pretokenizer_.process("#hello_world");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "#hello_world");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTextTest, MatchHashtag_FullWidth) {
  auto result = pretokenizer_.process("＃タグ");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "＃タグ");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Hashtag);
}

TEST_F(PreTokenizerTextTest, MatchHashtag_InText) {
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

TEST_F(PreTokenizerTextTest, MatchHashtag_MultipleInText) {
  auto result = pretokenizer_.process("#hello #world #日本");
  int hashtag_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Hashtag) {
      hashtag_count++;
    }
  }
  EXPECT_EQ(hashtag_count, 3);
}

TEST_F(PreTokenizerTextTest, NoMatch_HashtagEmpty) {
  auto result = pretokenizer_.process("# ");
  bool has_hashtag = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Hashtag) {
      has_hashtag = true;
    }
  }
  EXPECT_FALSE(has_hashtag);
}

TEST_F(PreTokenizerTextTest, NoMatch_HashtagSymbolOnly) {
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

TEST_F(PreTokenizerTextTest, MatchMention_Basic) {
  auto result = pretokenizer_.process("@user");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "@user");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Mention);
}

TEST_F(PreTokenizerTextTest, MatchMention_WithUnderscore) {
  auto result = pretokenizer_.process("@user_name");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "@user_name");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Mention);
}

TEST_F(PreTokenizerTextTest, MatchMention_WithNumbers) {
  auto result = pretokenizer_.process("@user123");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "@user123");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Mention);
}

TEST_F(PreTokenizerTextTest, MatchMention_InText) {
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

TEST_F(PreTokenizerTextTest, MatchMention_InJapaneseText) {
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

TEST_F(PreTokenizerTextTest, MatchMention_MultipleInText) {
  auto result = pretokenizer_.process("@alice and @bob");
  int mention_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Mention) {
      mention_count++;
    }
  }
  EXPECT_EQ(mention_count, 2);
}

TEST_F(PreTokenizerTextTest, NoMatch_MentionEmpty) {
  auto result = pretokenizer_.process("@ ");
  bool has_mention = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Mention) {
      has_mention = true;
    }
  }
  EXPECT_FALSE(has_mention);
}

// ===== Complex Text Tests =====

TEST_F(PreTokenizerTextTest, Complex_TechnicalDocument) {
  auto result = pretokenizer_.process(
      "2024年12月にv2.0.1をリリース。https://example.com を参照");

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

TEST_F(PreTokenizerTextTest, Complex_TechnicalDocument2) {
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

TEST_F(PreTokenizerTextTest, Complex_NewsArticle) {
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

TEST_F(PreTokenizerTextTest, Complex_TechnicalDocumentWithEmail) {
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

TEST_F(PreTokenizerTextTest, Complex_ScheduleWithTime) {
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

TEST_F(PreTokenizerTextTest, Complex_AllPatterns) {
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

TEST_F(PreTokenizerTextTest, Complex_SNSPost) {
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

TEST_F(PreTokenizerTextTest, Complex_AllPatternsIncludingSNS) {
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

// ===== Edge Cases =====

TEST_F(PreTokenizerTextTest, EdgeCase_EmptyString) {
  auto result = pretokenizer_.process("");
  EXPECT_TRUE(result.tokens.empty());
}

TEST_F(PreTokenizerTextTest, EdgeCase_OnlyWhitespace) {
  auto result = pretokenizer_.process("   ");
  // Should handle gracefully - no crash
}

TEST_F(PreTokenizerTextTest, EdgeCase_OnlyPunctuation) {
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

TEST_F(PreTokenizerTextTest, EdgeCase_ConsecutiveCurrency) {
  auto result = pretokenizer_.process("100円200円300円");

  int currency_count = 0;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Currency) {
      currency_count++;
    }
  }

  EXPECT_GE(currency_count, 3);
}

TEST_F(PreTokenizerTextTest, EdgeCase_NestedPatterns) {
  // URL containing date-like pattern
  auto result =
      pretokenizer_.process("https://example.com/2024/12/23/article");
  ASSERT_GE(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Url);
}

TEST_F(PreTokenizerTextTest, EdgeCase_VersionLikeDate) {
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

// ===== NoMatch Tests =====

TEST_F(PreTokenizerTextTest, NoMatch_PlainText) {
  auto result = pretokenizer_.process("これは普通のテキスト");
  EXPECT_TRUE(result.tokens.empty());
  ASSERT_EQ(result.spans.size(), 1);
  EXPECT_EQ(result.spans[0].start, 0);
}

TEST_F(PreTokenizerTextTest, NoMatch_PlainNumber) {
  auto result = pretokenizer_.process("12345");
  // Plain number without unit should be in spans, not tokens
  // (unless Number type is implemented)
  EXPECT_FALSE(result.spans.empty());
}

}  // namespace
}  // namespace suzume::pretokenizer
