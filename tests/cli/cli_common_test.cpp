#include "cli_common.h"

#include <gtest/gtest.h>

namespace suzume::cli {
namespace {

TEST(CliCommonTest, JsonEscapeEscapesSpecialCharacters) {
  EXPECT_EQ(jsonEscape("a\"b\\c"), "a\\\"b\\\\c");
  EXPECT_EQ(jsonEscape("line\nnext\tend"), "line\\nnext\\tend");
}

TEST(CliCommonTest, JsonEscapeEscapesControlCharacters) {
  std::string input;
  input.push_back('\x01');
  input += "ok";

  EXPECT_EQ(jsonEscape(input), "\\u0001ok");
}

TEST(CliCommonTest, ParseSizeOptionRejectsInvalidInput) {
  size_t value = 123;

  EXPECT_FALSE(parseSizeOption("", &value));
  EXPECT_FALSE(parseSizeOption("12x", &value));
  EXPECT_FALSE(parseSizeOption("-1", &value));
  EXPECT_FALSE(parseSizeOption("1.5", &value));
  EXPECT_EQ(value, 123u);
}

TEST(CliCommonTest, ParseSizeOptionAcceptsSize) {
  size_t value = 0;

  EXPECT_TRUE(parseSizeOption("42", &value));
  EXPECT_EQ(value, 42u);
}

}  // namespace
}  // namespace suzume::cli
