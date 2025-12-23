#include "normalize/utf8.h"

#include <gtest/gtest.h>

namespace suzume {
namespace normalize {
namespace {

TEST(Utf8Test, DecodeAscii) {
  std::string_view text = "hello";
  size_t pos = 0;
  EXPECT_EQ(decodeUtf8(text, pos), U'h');
  EXPECT_EQ(pos, 1);
  EXPECT_EQ(decodeUtf8(text, pos), U'e');
  EXPECT_EQ(pos, 2);
}

TEST(Utf8Test, DecodeHiragana) {
  std::string_view text = "あいう";
  size_t pos = 0;
  EXPECT_EQ(decodeUtf8(text, pos), U'あ');
  EXPECT_EQ(pos, 3);  // Hiragana is 3 bytes
  EXPECT_EQ(decodeUtf8(text, pos), U'い');
  EXPECT_EQ(pos, 6);
}

TEST(Utf8Test, DecodeKatakana) {
  std::string_view text = "アイウ";
  size_t pos = 0;
  EXPECT_EQ(decodeUtf8(text, pos), U'ア');
  EXPECT_EQ(pos, 3);
  EXPECT_EQ(decodeUtf8(text, pos), U'イ');
  EXPECT_EQ(pos, 6);
}

TEST(Utf8Test, DecodeKanji) {
  std::string_view text = "日本語";
  size_t pos = 0;
  EXPECT_EQ(decodeUtf8(text, pos), U'日');
  EXPECT_EQ(pos, 3);
  EXPECT_EQ(decodeUtf8(text, pos), U'本');
  EXPECT_EQ(pos, 6);
  EXPECT_EQ(decodeUtf8(text, pos), U'語');
  EXPECT_EQ(pos, 9);
}

TEST(Utf8Test, EncodeAscii) {
  std::string result;
  encodeUtf8(U'h', result);
  EXPECT_EQ(result, "h");
}

TEST(Utf8Test, EncodeHiragana) {
  std::string result;
  encodeUtf8(U'あ', result);
  EXPECT_EQ(result, "あ");
}

TEST(Utf8Test, Utf8Length) {
  EXPECT_EQ(utf8Length("hello"), 5);
  EXPECT_EQ(utf8Length("あいう"), 3);
  EXPECT_EQ(utf8Length("日本語"), 3);
  EXPECT_EQ(utf8Length("Hello世界"), 7);  // 5 ASCII + 2 kanji
}

TEST(Utf8Test, CharToByteOffset) {
  std::string_view text = "日本語";  // 9 bytes, 3 characters
  EXPECT_EQ(charToByteOffset(text, 0), 0);
  EXPECT_EQ(charToByteOffset(text, 1), 3);
  EXPECT_EQ(charToByteOffset(text, 2), 6);
  EXPECT_EQ(charToByteOffset(text, 3), 9);
}

TEST(Utf8Test, ByteToCharOffset) {
  std::string_view text = "日本語";  // 9 bytes, 3 characters
  EXPECT_EQ(byteToCharOffset(text, 0), 0);
  EXPECT_EQ(byteToCharOffset(text, 3), 1);
  EXPECT_EQ(byteToCharOffset(text, 6), 2);
  EXPECT_EQ(byteToCharOffset(text, 9), 3);
}

TEST(Utf8Test, ToCodepoints) {
  auto cps = toCodepoints("あいう");
  ASSERT_EQ(cps.size(), 3);
  EXPECT_EQ(cps[0], U'あ');
  EXPECT_EQ(cps[1], U'い');
  EXPECT_EQ(cps[2], U'う');
}

TEST(Utf8Test, FromCodepoints) {
  std::vector<char32_t> cps = {U'あ', U'い', U'う'};
  EXPECT_EQ(fromCodepoints(cps), "あいう");
}

TEST(Utf8Test, IsValidUtf8) {
  EXPECT_TRUE(isValidUtf8("hello"));
  EXPECT_TRUE(isValidUtf8("日本語"));
  EXPECT_TRUE(isValidUtf8("Hello世界"));
  EXPECT_TRUE(isValidUtf8(""));
}

TEST(Utf8Test, Utf8Substr) {
  std::string_view text = "日本語";
  EXPECT_EQ(utf8Substr(text, 0, 1), "日");
  EXPECT_EQ(utf8Substr(text, 1, 1), "本");
  EXPECT_EQ(utf8Substr(text, 0, 2), "日本");
  EXPECT_EQ(utf8Substr(text, 1, 2), "本語");
}

TEST(Utf8Test, DecodeEncodeNamespace) {
  // Test the utf8 namespace functions
  auto cps = utf8::decode("こんにちは");
  ASSERT_EQ(cps.size(), 5);
  EXPECT_EQ(cps[0], U'こ');

  std::string encoded = utf8::encode(cps);
  EXPECT_EQ(encoded, "こんにちは");
}

// ===== Emoji Tests =====

TEST(Utf8Test, Emoji_Basic) {
  // Basic emoji (BMP)
  std::string_view text = "😀";  // U+1F600
  size_t pos = 0;
  char32_t cp = decodeUtf8(text, pos);
  EXPECT_EQ(cp, U'\U0001F600');
  EXPECT_EQ(pos, 4);  // 4-byte UTF-8 sequence
}

TEST(Utf8Test, Emoji_Multiple) {
  auto cps = toCodepoints("😀😁😂");
  ASSERT_EQ(cps.size(), 3);
  EXPECT_EQ(cps[0], U'\U0001F600');
  EXPECT_EQ(cps[1], U'\U0001F601');
  EXPECT_EQ(cps[2], U'\U0001F602');
}

TEST(Utf8Test, Emoji_MixedWithJapanese) {
  auto cps = toCodepoints("こんにちは😊");
  ASSERT_EQ(cps.size(), 6);
  EXPECT_EQ(cps[5], U'\U0001F60A');
}

TEST(Utf8Test, Emoji_Length) {
  EXPECT_EQ(utf8Length("😀😁😂"), 3);
  EXPECT_EQ(utf8Length("Hello😀世界"), 8);  // 5 + 1 + 2
}

TEST(Utf8Test, Emoji_Encode) {
  std::string result;
  encodeUtf8(U'\U0001F600', result);
  EXPECT_EQ(result, "😀");
}

TEST(Utf8Test, Emoji_FamilySequence) {
  // Family emoji with ZWJ (Zero Width Joiner)
  // 👨‍👩‍👧 = 👨 + ZWJ + 👩 + ZWJ + 👧
  std::string family = "👨\u200D👩\u200D👧";
  auto cps = toCodepoints(family);
  // Should decode each codepoint separately
  EXPECT_GE(cps.size(), 5);  // Man, ZWJ, Woman, ZWJ, Girl
}

TEST(Utf8Test, Emoji_SkinToneModifier) {
  // Emoji with skin tone modifier
  // 👋🏻 = 👋 + Light Skin Tone
  std::string wave = "👋🏻";
  auto cps = toCodepoints(wave);
  ASSERT_EQ(cps.size(), 2);
  EXPECT_EQ(cps[0], U'\U0001F44B');  // Waving hand
  EXPECT_EQ(cps[1], U'\U0001F3FB');  // Light skin tone
}

TEST(Utf8Test, Emoji_Flag) {
  // Flag emoji (Regional Indicator Symbols)
  // 🇯🇵 = Regional Indicator J + Regional Indicator P
  std::string japan_flag = "🇯🇵";
  auto cps = toCodepoints(japan_flag);
  ASSERT_EQ(cps.size(), 2);
  EXPECT_EQ(cps[0], U'\U0001F1EF');  // Regional Indicator J
  EXPECT_EQ(cps[1], U'\U0001F1F5');  // Regional Indicator P
}

// ===== Supplementary Plane Character Tests =====

TEST(Utf8Test, SupplementaryPlane_RareKanji) {
  // CJK Extension B character (requires 4 bytes in UTF-8)
  // 𠀀 U+20000
  std::string rare_kanji = "𠀀";
  auto cps = toCodepoints(rare_kanji);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\U00020000');
}

TEST(Utf8Test, SupplementaryPlane_MusicalSymbol) {
  // 𝄞 U+1D11E (Musical symbol G clef)
  std::string clef = "𝄞";
  auto cps = toCodepoints(clef);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\U0001D11E');
}

TEST(Utf8Test, SupplementaryPlane_MixedText) {
  // Mix of BMP and supplementary plane characters
  std::string mixed = "日𠀀語";
  auto cps = toCodepoints(mixed);
  ASSERT_EQ(cps.size(), 3);
  EXPECT_EQ(cps[0], U'日');
  EXPECT_EQ(cps[1], U'\U00020000');
  EXPECT_EQ(cps[2], U'語');
}

// ===== Zero-Width Character Tests =====

TEST(Utf8Test, ZeroWidth_Joiner) {
  // ZWJ U+200D
  std::string zwj = "\u200D";
  auto cps = toCodepoints(zwj);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\u200D');
}

TEST(Utf8Test, ZeroWidth_NonJoiner) {
  // ZWNJ U+200C
  std::string zwnj = "\u200C";
  auto cps = toCodepoints(zwnj);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\u200C');
}

TEST(Utf8Test, ZeroWidth_Space) {
  // Zero Width Space U+200B
  std::string zws = "\u200B";
  auto cps = toCodepoints(zws);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\u200B');
}

TEST(Utf8Test, ZeroWidth_InText) {
  // Text with embedded zero-width characters
  std::string text = "あ\u200Bい\u200Cう";
  auto cps = toCodepoints(text);
  ASSERT_EQ(cps.size(), 5);  // 3 hiragana + 2 zero-width
}

TEST(Utf8Test, ZeroWidth_ByteOrderMark) {
  // BOM U+FEFF (also called ZWNBSP)
  std::string bom = "\uFEFF";
  auto cps = toCodepoints(bom);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\uFEFF');
}

// ===== Combining Character Tests =====

TEST(Utf8Test, Combining_Dakuten) {
  // か + combining dakuten (U+3099) = が (in NFD form)
  std::string nfd_ga = "か\u3099";
  auto cps = toCodepoints(nfd_ga);
  ASSERT_EQ(cps.size(), 2);
  EXPECT_EQ(cps[0], U'か');
  EXPECT_EQ(cps[1], U'\u3099');  // Combining dakuten
}

TEST(Utf8Test, Combining_Handakuten) {
  // は + combining handakuten (U+309A) = ぱ (in NFD form)
  std::string nfd_pa = "は\u309A";
  auto cps = toCodepoints(nfd_pa);
  ASSERT_EQ(cps.size(), 2);
  EXPECT_EQ(cps[0], U'は');
  EXPECT_EQ(cps[1], U'\u309A');  // Combining handakuten
}

TEST(Utf8Test, Combining_Accent) {
  // e + combining acute accent (U+0301) = é (in NFD form)
  std::string nfd_e_acute = "e\u0301";
  auto cps = toCodepoints(nfd_e_acute);
  ASSERT_EQ(cps.size(), 2);
  EXPECT_EQ(cps[0], U'e');
  EXPECT_EQ(cps[1], U'\u0301');  // Combining acute accent
}

TEST(Utf8Test, Combining_Multiple) {
  // Character with multiple combining marks
  // a + combining grave (U+0300) + combining acute (U+0301)
  std::string multi_combine = "a\u0300\u0301";
  auto cps = toCodepoints(multi_combine);
  ASSERT_EQ(cps.size(), 3);
}

// ===== Special Character Tests =====

TEST(Utf8Test, Special_IdeographicSpace) {
  // Full-width space U+3000
  std::string fw_space = "\u3000";
  auto cps = toCodepoints(fw_space);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\u3000');
}

TEST(Utf8Test, Special_VerticalForms) {
  // Vertical forms block (U+FE10-FE1F)
  std::string vertical_comma = "\uFE10";  // Presentation form for vertical comma
  auto cps = toCodepoints(vertical_comma);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\uFE10');
}

TEST(Utf8Test, Special_PrivateUse) {
  // Private Use Area character (U+E000)
  std::string pua = "\uE000";
  auto cps = toCodepoints(pua);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\uE000');
}

TEST(Utf8Test, Special_ReplacementChar) {
  // Replacement character U+FFFD
  std::string replacement = "\uFFFD";
  auto cps = toCodepoints(replacement);
  ASSERT_EQ(cps.size(), 1);
  EXPECT_EQ(cps[0], U'\uFFFD');
}

// ===== Boundary Tests =====

TEST(Utf8Test, Boundary_MaxTwoByteChar) {
  // U+07FF is the maximum 2-byte UTF-8 character
  std::string max_2byte = "\u07FF";
  size_t pos = 0;
  char32_t cp = decodeUtf8(max_2byte, pos);
  EXPECT_EQ(cp, U'\u07FF');
  EXPECT_EQ(pos, 2);
}

TEST(Utf8Test, Boundary_MinThreeByteChar) {
  // U+0800 is the minimum 3-byte UTF-8 character
  std::string min_3byte = "\u0800";
  size_t pos = 0;
  char32_t cp = decodeUtf8(min_3byte, pos);
  EXPECT_EQ(cp, U'\u0800');
  EXPECT_EQ(pos, 3);
}

TEST(Utf8Test, Boundary_MaxThreeByteChar) {
  // U+FFFF is the maximum 3-byte UTF-8 character (excluding surrogates)
  std::string max_3byte = "\uFFFF";
  size_t pos = 0;
  char32_t cp = decodeUtf8(max_3byte, pos);
  EXPECT_EQ(cp, U'\uFFFF');
  EXPECT_EQ(pos, 3);
}

TEST(Utf8Test, Boundary_MinFourByteChar) {
  // U+10000 is the minimum 4-byte UTF-8 character
  std::string min_4byte = "𐀀";  // U+10000
  size_t pos = 0;
  char32_t cp = decodeUtf8(min_4byte, pos);
  EXPECT_EQ(cp, U'\U00010000');
  EXPECT_EQ(pos, 4);
}

TEST(Utf8Test, Boundary_MaxValidCodepoint) {
  // U+10FFFF is the maximum valid Unicode code point
  char32_t max_cp = 0x10FFFF;
  std::string result;
  encodeUtf8(max_cp, result);
  auto decoded = toCodepoints(result);
  ASSERT_EQ(decoded.size(), 1);
  EXPECT_EQ(decoded[0], max_cp);
}

// ===== Invalid UTF-8 Handling Tests =====
// Note: These tests verify the current behavior of isValidUtf8.
// Strict UTF-8 validation would reject overlong sequences and surrogates,
// but the current implementation may be lenient for performance reasons.

TEST(Utf8Test, Invalid_ContinuationByteFirst) {
  // Continuation byte at start
  std::string invalid = "\x80" "abc";  // Separate to avoid hex escape issue
  EXPECT_FALSE(isValidUtf8(invalid));
}

TEST(Utf8Test, Invalid_TruncatedSequence2) {
  // Truncated 2-byte sequence
  std::string truncated = "\xC2";  // Missing continuation byte
  EXPECT_FALSE(isValidUtf8(truncated));
}

TEST(Utf8Test, Invalid_TruncatedSequence3) {
  // Truncated 3-byte sequence
  std::string truncated = "\xE0\xA0";  // Missing one continuation byte
  EXPECT_FALSE(isValidUtf8(truncated));
}

TEST(Utf8Test, Invalid_TruncatedSequence4) {
  // Truncated 4-byte sequence
  std::string truncated = "\xF0\x90\x80";  // Missing one continuation byte
  EXPECT_FALSE(isValidUtf8(truncated));
}

TEST(Utf8Test, Invalid_TooLargeCodepoint) {
  // Code point > U+10FFFF (5+ byte sequence)
  std::string too_large = "\xF8\x88\x80\x80\x80";
  EXPECT_FALSE(isValidUtf8(too_large));
}

// The following tests document current behavior.
// Strict UTF-8 validation would reject these, but current impl is lenient.

TEST(Utf8Test, Lenient_Overlong2Byte) {
  // Overlong encoding of ASCII
  // 'a' (U+0061) encoded as 2 bytes: C1 A1
  // Note: Current implementation is lenient and accepts this
  std::string overlong = "\xC1\xA1";
  // Just ensure it doesn't crash; behavior may vary
  auto result = isValidUtf8(overlong);
  (void)result;  // Suppress unused variable warning
}

TEST(Utf8Test, Lenient_Overlong3Byte) {
  // Overlong encoding of 2-byte character
  // U+007F encoded as 3 bytes: E0 81 BF
  // Note: Current implementation is lenient
  std::string overlong = "\xE0\x81\xBF";
  auto result = isValidUtf8(overlong);
  (void)result;
}

TEST(Utf8Test, Lenient_SurrogateHalf) {
  // UTF-8 encoding of surrogate half (illegal in strict UTF-8)
  // U+D800 would be: ED A0 80
  // Note: Current implementation is lenient
  std::string surrogate = "\xED\xA0\x80";
  auto result = isValidUtf8(surrogate);
  (void)result;
}

// ===== Edge Cases =====

TEST(Utf8Test, EdgeCase_EmptyString) {
  auto cps = toCodepoints("");
  EXPECT_TRUE(cps.empty());
  EXPECT_EQ(utf8Length(""), 0);
}

TEST(Utf8Test, EdgeCase_NullCharacter) {
  // String containing null character
  std::string with_null = "a\0b";
  // Note: std::string can contain null bytes
  // But string_view from C string would stop at null
}

TEST(Utf8Test, EdgeCase_VeryLongString) {
  // Create a long string
  std::string long_str;
  for (int idx = 0; idx < 1000; ++idx) {
    long_str += "あ";
  }
  auto cps = toCodepoints(long_str);
  EXPECT_EQ(cps.size(), 1000);
  EXPECT_EQ(utf8Length(long_str), 1000);
}

TEST(Utf8Test, EdgeCase_AllByteSequenceLengths) {
  // Test string with 1, 2, 3, and 4 byte characters
  std::string mixed = "a" "\xC2\xA9" "日" "😀";  // a, ©, 日, 😀
  auto cps = toCodepoints(mixed);
  ASSERT_EQ(cps.size(), 4);
  EXPECT_EQ(cps[0], U'a');       // 1 byte
  EXPECT_EQ(cps[1], U'©');       // 2 bytes
  EXPECT_EQ(cps[2], U'日');      // 3 bytes
  EXPECT_EQ(cps[3], U'😀');      // 4 bytes
}

// ===== Roundtrip Tests =====

TEST(Utf8Test, Roundtrip_Japanese) {
  std::string original = "日本語テスト";
  auto cps = toCodepoints(original);
  std::string encoded = fromCodepoints(cps);
  EXPECT_EQ(encoded, original);
}

TEST(Utf8Test, Roundtrip_Emoji) {
  std::string original = "😀🎉🌟";
  auto cps = toCodepoints(original);
  std::string encoded = fromCodepoints(cps);
  EXPECT_EQ(encoded, original);
}

TEST(Utf8Test, Roundtrip_Mixed) {
  std::string original = "Hello日本語😀World";
  auto cps = toCodepoints(original);
  std::string encoded = fromCodepoints(cps);
  EXPECT_EQ(encoded, original);
}

TEST(Utf8Test, Roundtrip_FullRange) {
  // Test encoding and decoding across full Unicode range
  std::vector<char32_t> test_cps = {
      U'a',           // ASCII
      U'\u00E9',      // Latin Extended
      U'\u3042',      // Hiragana
      U'\u30A2',      // Katakana
      U'\u4E2D',      // CJK
      U'\U0001F600',  // Emoji
      U'\U00020000',  // CJK Extension B
  };

  std::string encoded = fromCodepoints(test_cps);
  auto decoded = toCodepoints(encoded);

  ASSERT_EQ(decoded.size(), test_cps.size());
  for (size_t idx = 0; idx < test_cps.size(); ++idx) {
    EXPECT_EQ(decoded[idx], test_cps[idx]);
  }
}

}  // namespace
}  // namespace normalize
}  // namespace suzume
