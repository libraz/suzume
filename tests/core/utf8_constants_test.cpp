#include "core/utf8_constants.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "core/kana_constants.h"

namespace suzume {
namespace core {
namespace {

// U+20B9F, a CJK Extension B kanji (4-byte UTF-8).
constexpr std::string_view kSupplementaryKanji = "\xF0\xA0\xAE\x9F";

TEST(Utf8ConstantsTest, DecodesThreeByteCharactersUnchanged) {
  EXPECT_EQ(utf8::decodeFirstChar("漢字"), U'漢');
  EXPECT_EQ(utf8::decodeLastChar("漢字"), U'字');
  EXPECT_EQ(utf8::decodeFirstChar("あい"), U'あ');
  EXPECT_EQ(utf8::decodeLastChar("あい"), U'い');
}

TEST(Utf8ConstantsTest, DecodesSupplementaryPlaneKanji) {
  const std::string first = std::string(kSupplementaryKanji) + "る";
  const std::string last = std::string("お") + std::string(kSupplementaryKanji);
  EXPECT_EQ(utf8::decodeFirstChar(first), U'\U00020B9F');
  EXPECT_EQ(utf8::decodeLastChar(last), U'\U00020B9F');
  EXPECT_EQ(utf8::decodeLastChar(kSupplementaryKanji), U'\U00020B9F');
  EXPECT_TRUE(kana::isKanjiCodepoint(utf8::decodeFirstChar(first)));
  EXPECT_TRUE(kana::isKanjiCodepoint(utf8::decodeLastChar(last)));
}

TEST(Utf8ConstantsTest, RejectsTruncatedOrInvalidFourByteSequences) {
  const std::string_view truncated = kSupplementaryKanji.substr(0, 3);
  EXPECT_EQ(utf8::decodeFirstChar(truncated), 0U);
  EXPECT_EQ(utf8::decodeLastChar(std::string("あ") + std::string(truncated)), 0U);
  EXPECT_EQ(utf8::decodeFirstChar("\xF0\x41\x42\x43"), 0U);
  EXPECT_EQ(utf8::decodeLastChar("\xF0\x41\x42\x43"), 0U);
  EXPECT_EQ(utf8::decodeFirstChar("abc"), 0U);
  EXPECT_EQ(utf8::decodeLastChar(""), 0U);
}

}  // namespace
}  // namespace core
}  // namespace suzume
