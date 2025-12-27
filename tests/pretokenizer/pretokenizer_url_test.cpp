
// Pretokenizer tests for URL and Email patterns

#include "pretokenizer/pretokenizer.h"

#include <gtest/gtest.h>

namespace suzume::pretokenizer {
namespace {

class PreTokenizerUrlTest : public ::testing::Test {
 protected:
  PreTokenizer pretokenizer_;
};

// URL tests
TEST_F(PreTokenizerUrlTest, MatchUrl_HttpsBasic) {
  auto result = pretokenizer_.process("https://example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Url);
  EXPECT_TRUE(result.spans.empty());
}

TEST_F(PreTokenizerUrlTest, MatchUrl_HttpWithPath) {
  auto result = pretokenizer_.process("http://example.com/path/to/page");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "http://example.com/path/to/page");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Url);
}

TEST_F(PreTokenizerUrlTest, MatchUrl_WithSurroundingText) {
  auto result = pretokenizer_.process("Visit https://example.com for more");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com");
  ASSERT_EQ(result.spans.size(), 2);
}

TEST_F(PreTokenizerUrlTest, MatchUrl_Japanese) {
  auto result = pretokenizer_.process("https://example.com にアクセス");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com");
  EXPECT_EQ(result.spans.size(), 1);
}

TEST_F(PreTokenizerUrlTest, MatchUrl_WithQueryString) {
  auto result = pretokenizer_.process("https://example.com/search?q=test&page=1");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com/search?q=test&page=1");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Url);
}

TEST_F(PreTokenizerUrlTest, MatchUrl_WithFragment) {
  auto result = pretokenizer_.process("https://example.com/page#section1");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com/page#section1");
}

TEST_F(PreTokenizerUrlTest, MatchUrl_WithPort) {
  auto result = pretokenizer_.process("https://example.com:8080/path");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "https://example.com:8080/path");
}

TEST_F(PreTokenizerUrlTest, MatchUrl_Localhost) {
  auto result = pretokenizer_.process("http://localhost:3000");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "http://localhost:3000");
}

TEST_F(PreTokenizerUrlTest, MatchUrl_MultipleInText) {
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

TEST_F(PreTokenizerUrlTest, NoMatch_PartialUrl) {
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

// ===== Email Tests =====

TEST_F(PreTokenizerUrlTest, MatchEmail_Basic) {
  auto result = pretokenizer_.process("user@example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "user@example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Email);
}

TEST_F(PreTokenizerUrlTest, MatchEmail_WithSubdomain) {
  auto result = pretokenizer_.process("user@mail.example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "user@mail.example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Email);
}

TEST_F(PreTokenizerUrlTest, MatchEmail_WithPlus) {
  auto result = pretokenizer_.process("user+tag@example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "user+tag@example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Email);
}

TEST_F(PreTokenizerUrlTest, MatchEmail_WithDots) {
  auto result = pretokenizer_.process("first.last@example.com");
  ASSERT_EQ(result.tokens.size(), 1);
  EXPECT_EQ(result.tokens[0].surface, "first.last@example.com");
  EXPECT_EQ(result.tokens[0].type, PreTokenType::Email);
}

TEST_F(PreTokenizerUrlTest, MatchEmail_InJapaneseText) {
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

TEST_F(PreTokenizerUrlTest, MatchEmail_MultipleInText) {
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

TEST_F(PreTokenizerUrlTest, NoMatch_InvalidEmail_NoDomain) {
  auto result = pretokenizer_.process("user@");
  bool has_email = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      has_email = true;
    }
  }
  EXPECT_FALSE(has_email);
}

TEST_F(PreTokenizerUrlTest, NoMatch_InvalidEmail_NoDot) {
  auto result = pretokenizer_.process("user@localhost");
  bool has_email = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      has_email = true;
    }
  }
  EXPECT_FALSE(has_email);
}

TEST_F(PreTokenizerUrlTest, NoMatch_InvalidEmail_StartWithDot) {
  auto result = pretokenizer_.process(".user@example.com");
  bool has_email = false;
  for (const auto& token : result.tokens) {
    if (token.type == PreTokenType::Email) {
      has_email = true;
    }
  }
  EXPECT_FALSE(has_email);
}

TEST_F(PreTokenizerUrlTest, EmailVsMention_EmailWins) {
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

}  // namespace
}  // namespace suzume::pretokenizer
