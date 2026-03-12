/**
 * @file char_type_test.cpp
 * @brief Tests for character type classification and predicates
 */

#include "normalize/char_type.h"

#include <gtest/gtest.h>

namespace suzume {
namespace normalize {
namespace {

// ============================================================================
// classifyChar tests
// ============================================================================

TEST(CharTypeTest, ClassifyHiragana) {
  EXPECT_EQ(classifyChar(U'あ'), CharType::Hiragana);
  EXPECT_EQ(classifyChar(U'ん'), CharType::Hiragana);
  EXPECT_EQ(classifyChar(U'っ'), CharType::Hiragana);
  EXPECT_EQ(classifyChar(U'ぁ'), CharType::Hiragana);  // Small hiragana
  // Boundary values
  EXPECT_EQ(classifyChar(0x3040), CharType::Hiragana);  // Range start
  EXPECT_EQ(classifyChar(0x309F), CharType::Hiragana);  // Range end
}

TEST(CharTypeTest, ClassifyKatakana) {
  EXPECT_EQ(classifyChar(U'ア'), CharType::Katakana);
  EXPECT_EQ(classifyChar(U'ン'), CharType::Katakana);
  EXPECT_EQ(classifyChar(U'ー'), CharType::Katakana);  // Prolonged sound mark
  EXPECT_EQ(classifyChar(U'ァ'), CharType::Katakana);  // Small katakana
  // Boundary values
  EXPECT_EQ(classifyChar(0x30A0), CharType::Katakana);  // Range start
  EXPECT_EQ(classifyChar(0x30FF), CharType::Katakana);  // Range end
}

TEST(CharTypeTest, ClassifyHalfWidthKatakana) {
  EXPECT_EQ(classifyChar(0xFF66), CharType::Katakana);  // Half-width wo
  EXPECT_EQ(classifyChar(0xFF71), CharType::Katakana);  // Half-width a
  EXPECT_EQ(classifyChar(0xFF9F), CharType::Katakana);  // Range end
}

TEST(CharTypeTest, ClassifySmallKatakanaExtension) {
  EXPECT_EQ(classifyChar(0x31F0), CharType::Katakana);  // Range start
  EXPECT_EQ(classifyChar(0x31FF), CharType::Katakana);  // Range end
}

TEST(CharTypeTest, ClassifyIterationMark) {
  // U+3005 (々) should be Kanji, not Symbol
  EXPECT_EQ(classifyChar(0x3005), CharType::Kanji);
}

TEST(CharTypeTest, ClassifyCJKUnifiedIdeographs) {
  EXPECT_EQ(classifyChar(U'漢'), CharType::Kanji);
  EXPECT_EQ(classifyChar(U'字'), CharType::Kanji);
  EXPECT_EQ(classifyChar(0x4E00), CharType::Kanji);  // Range start
  EXPECT_EQ(classifyChar(0x9FFF), CharType::Kanji);  // Range end
}

TEST(CharTypeTest, ClassifyCJKExtensionA) {
  EXPECT_EQ(classifyChar(0x3400), CharType::Kanji);
  EXPECT_EQ(classifyChar(0x4DBF), CharType::Kanji);
}

TEST(CharTypeTest, ClassifyCJKExtensionB) {
  EXPECT_EQ(classifyChar(0x20000), CharType::Kanji);
  EXPECT_EQ(classifyChar(0x2A6DF), CharType::Kanji);
}

TEST(CharTypeTest, ClassifyCJKExtensionC) {
  EXPECT_EQ(classifyChar(0x2A700), CharType::Kanji);
  EXPECT_EQ(classifyChar(0x2B73F), CharType::Kanji);
}

TEST(CharTypeTest, ClassifyCJKExtensionD) {
  EXPECT_EQ(classifyChar(0x2B740), CharType::Kanji);
  EXPECT_EQ(classifyChar(0x2B81F), CharType::Kanji);
}

TEST(CharTypeTest, ClassifyCJKCompatibilityIdeographs) {
  EXPECT_EQ(classifyChar(0xF900), CharType::Kanji);
  EXPECT_EQ(classifyChar(0xFAFF), CharType::Kanji);
}

TEST(CharTypeTest, ClassifyKangxiRadicals) {
  EXPECT_EQ(classifyChar(0x2F00), CharType::Kanji);
  EXPECT_EQ(classifyChar(0x2FDF), CharType::Kanji);
}

TEST(CharTypeTest, ClassifyAsciiAlphabet) {
  EXPECT_EQ(classifyChar(U'A'), CharType::Alphabet);
  EXPECT_EQ(classifyChar(U'Z'), CharType::Alphabet);
  EXPECT_EQ(classifyChar(U'a'), CharType::Alphabet);
  EXPECT_EQ(classifyChar(U'z'), CharType::Alphabet);
  EXPECT_EQ(classifyChar(U'M'), CharType::Alphabet);
}

TEST(CharTypeTest, ClassifyFullWidthAlphabet) {
  EXPECT_EQ(classifyChar(0xFF21), CharType::Alphabet);  // Full-width A
  EXPECT_EQ(classifyChar(0xFF3A), CharType::Alphabet);  // Full-width Z
  EXPECT_EQ(classifyChar(0xFF41), CharType::Alphabet);  // Full-width a
  EXPECT_EQ(classifyChar(0xFF5A), CharType::Alphabet);  // Full-width z
}

TEST(CharTypeTest, ClassifyAsciiDigits) {
  EXPECT_EQ(classifyChar(U'0'), CharType::Digit);
  EXPECT_EQ(classifyChar(U'9'), CharType::Digit);
  EXPECT_EQ(classifyChar(U'5'), CharType::Digit);
}

TEST(CharTypeTest, ClassifyFullWidthDigits) {
  EXPECT_EQ(classifyChar(0xFF10), CharType::Digit);  // Full-width 0
  EXPECT_EQ(classifyChar(0xFF19), CharType::Digit);  // Full-width 9
}

TEST(CharTypeTest, ClassifyCJKSymbols) {
  EXPECT_EQ(classifyChar(0x3000), CharType::Symbol);  // Ideographic space
  EXPECT_EQ(classifyChar(0x3001), CharType::Symbol);  // Ideographic comma
  EXPECT_EQ(classifyChar(0x3002), CharType::Symbol);  // Ideographic period
  EXPECT_EQ(classifyChar(0x300C), CharType::Symbol);  // Left corner bracket
  // Skip 0x3005 (iteration mark, classified as Kanji)
  EXPECT_EQ(classifyChar(0x3010), CharType::Symbol);  // Left black bracket
  EXPECT_EQ(classifyChar(0x303F), CharType::Symbol);  // Range end
}

TEST(CharTypeTest, ClassifyAsciiPunctuation) {
  EXPECT_EQ(classifyChar(U' '), CharType::Symbol);   // Space
  EXPECT_EQ(classifyChar(U'!'), CharType::Symbol);
  EXPECT_EQ(classifyChar(U'.'), CharType::Symbol);
  EXPECT_EQ(classifyChar(U':'), CharType::Symbol);
  EXPECT_EQ(classifyChar(U'@'), CharType::Symbol);
  EXPECT_EQ(classifyChar(U'['), CharType::Symbol);
  EXPECT_EQ(classifyChar(U'{'), CharType::Symbol);
  EXPECT_EQ(classifyChar(U'~'), CharType::Symbol);
}

TEST(CharTypeTest, ClassifyEmoji) {
  EXPECT_EQ(classifyChar(0x1F600), CharType::Emoji);  // Grinning face
  EXPECT_EQ(classifyChar(0x1F4A9), CharType::Emoji);  // Misc symbols
  EXPECT_EQ(classifyChar(0x1F680), CharType::Emoji);  // Rocket
  EXPECT_EQ(classifyChar(0x2615), CharType::Emoji);   // Hot beverage
  EXPECT_EQ(classifyChar(0x2B50), CharType::Emoji);   // Star
  EXPECT_EQ(classifyChar(0x231A), CharType::Emoji);   // Watch
  EXPECT_EQ(classifyChar(0x1F1E6), CharType::Emoji);  // Regional indicator A
  EXPECT_EQ(classifyChar(0x1F3FB), CharType::Emoji);  // Skin tone modifier
}

TEST(CharTypeTest, ClassifyUnknown) {
  // Arabic character - not in any recognized range
  EXPECT_EQ(classifyChar(0x0627), CharType::Unknown);
  // Cyrillic
  EXPECT_EQ(classifyChar(0x0410), CharType::Unknown);
  // Thai
  EXPECT_EQ(classifyChar(0x0E01), CharType::Unknown);
}

// ============================================================================
// charTypeToString tests
// ============================================================================

TEST(CharTypeTest, TypeToString) {
  EXPECT_EQ(charTypeToString(CharType::Kanji), "KANJI");
  EXPECT_EQ(charTypeToString(CharType::Hiragana), "HIRAGANA");
  EXPECT_EQ(charTypeToString(CharType::Katakana), "KATAKANA");
  EXPECT_EQ(charTypeToString(CharType::Alphabet), "ALPHABET");
  EXPECT_EQ(charTypeToString(CharType::Digit), "DIGIT");
  EXPECT_EQ(charTypeToString(CharType::Symbol), "SYMBOL");
  EXPECT_EQ(charTypeToString(CharType::Emoji), "EMOJI");
  EXPECT_EQ(charTypeToString(CharType::Unknown), "UNKNOWN");
}

// ============================================================================
// canCombine tests
// ============================================================================

TEST(CharTypeTest, CanCombineSameType) {
  EXPECT_TRUE(canCombine(CharType::Kanji, CharType::Kanji));
  EXPECT_TRUE(canCombine(CharType::Hiragana, CharType::Hiragana));
  EXPECT_TRUE(canCombine(CharType::Katakana, CharType::Katakana));
  EXPECT_TRUE(canCombine(CharType::Alphabet, CharType::Alphabet));
  EXPECT_TRUE(canCombine(CharType::Digit, CharType::Digit));
  EXPECT_TRUE(canCombine(CharType::Symbol, CharType::Symbol));
  EXPECT_TRUE(canCombine(CharType::Emoji, CharType::Emoji));
}

TEST(CharTypeTest, CanCombineAlphabetDigit) {
  EXPECT_TRUE(canCombine(CharType::Alphabet, CharType::Digit));
  EXPECT_TRUE(canCombine(CharType::Digit, CharType::Alphabet));
}

TEST(CharTypeTest, CanCombineHiraganaKatakanaFalse) {
  EXPECT_FALSE(canCombine(CharType::Hiragana, CharType::Katakana));
  EXPECT_FALSE(canCombine(CharType::Katakana, CharType::Hiragana));
}

TEST(CharTypeTest, CanCombineCrossTypeFalse) {
  EXPECT_FALSE(canCombine(CharType::Kanji, CharType::Hiragana));
  EXPECT_FALSE(canCombine(CharType::Hiragana, CharType::Kanji));
  EXPECT_FALSE(canCombine(CharType::Kanji, CharType::Alphabet));
  EXPECT_FALSE(canCombine(CharType::Alphabet, CharType::Kanji));
  EXPECT_FALSE(canCombine(CharType::Digit, CharType::Kanji));
  EXPECT_FALSE(canCombine(CharType::Symbol, CharType::Hiragana));
  EXPECT_FALSE(canCombine(CharType::Emoji, CharType::Alphabet));
}

// ============================================================================
// isCommonParticle tests
// ============================================================================

TEST(CharTypeTest, IsCommonParticleTrue) {
  EXPECT_TRUE(isCommonParticle(U'を'));
  EXPECT_TRUE(isCommonParticle(U'が'));
  EXPECT_TRUE(isCommonParticle(U'は'));
  EXPECT_TRUE(isCommonParticle(U'に'));
  EXPECT_TRUE(isCommonParticle(U'へ'));
  EXPECT_TRUE(isCommonParticle(U'の'));
}

TEST(CharTypeTest, IsCommonParticleFalse) {
  EXPECT_FALSE(isCommonParticle(U'も'));
  EXPECT_FALSE(isCommonParticle(U'で'));
  EXPECT_FALSE(isCommonParticle(U'と'));
  EXPECT_FALSE(isCommonParticle(U'か'));
  EXPECT_FALSE(isCommonParticle(U'あ'));
  EXPECT_FALSE(isCommonParticle(U'A'));
}

// ============================================================================
// isNeverVerbStemAfterKanji tests
// ============================================================================

TEST(CharTypeTest, IsNeverVerbStemAfterKanjiTrue) {
  // All common particles
  EXPECT_TRUE(isNeverVerbStemAfterKanji(U'を'));
  EXPECT_TRUE(isNeverVerbStemAfterKanji(U'が'));
  EXPECT_TRUE(isNeverVerbStemAfterKanji(U'は'));
  EXPECT_TRUE(isNeverVerbStemAfterKanji(U'に'));
  EXPECT_TRUE(isNeverVerbStemAfterKanji(U'へ'));
  EXPECT_TRUE(isNeverVerbStemAfterKanji(U'の'));
  // Additional: も, や
  EXPECT_TRUE(isNeverVerbStemAfterKanji(U'も'));
  EXPECT_TRUE(isNeverVerbStemAfterKanji(U'や'));
}

TEST(CharTypeTest, IsNeverVerbStemAfterKanjiFalse) {
  // か can be part of verb conjugation (書かない)
  EXPECT_FALSE(isNeverVerbStemAfterKanji(U'か'));
  EXPECT_FALSE(isNeverVerbStemAfterKanji(U'き'));
  EXPECT_FALSE(isNeverVerbStemAfterKanji(U'く'));
  EXPECT_FALSE(isNeverVerbStemAfterKanji(U'て'));
}

// ============================================================================
// isNeverVerbStemAtStart tests
// ============================================================================

TEST(CharTypeTest, IsNeverVerbStemAtStartTrue) {
  EXPECT_TRUE(isNeverVerbStemAtStart(U'を'));
  EXPECT_TRUE(isNeverVerbStemAtStart(U'が'));
  EXPECT_TRUE(isNeverVerbStemAtStart(U'へ'));
  EXPECT_TRUE(isNeverVerbStemAtStart(U'の'));
  EXPECT_TRUE(isNeverVerbStemAtStart(U'よ'));
}

TEST(CharTypeTest, IsNeverVerbStemAtStartFalse) {
  // も can start verbs (もらう)
  EXPECT_FALSE(isNeverVerbStemAtStart(U'も'));
  // や can start verbs (やる)
  EXPECT_FALSE(isNeverVerbStemAtStart(U'や'));
  // ね can start verbs (寝る)
  EXPECT_FALSE(isNeverVerbStemAtStart(U'ね'));
  // に can start verbs (逃げる)
  EXPECT_FALSE(isNeverVerbStemAtStart(U'に'));
  // わ can start verbs (わかる)
  EXPECT_FALSE(isNeverVerbStemAtStart(U'わ'));
  // は can start verbs (はじまる)
  EXPECT_FALSE(isNeverVerbStemAtStart(U'は'));
}

// ============================================================================
// isDemonstrativeStart tests
// ============================================================================

TEST(CharTypeTest, IsDemonstrativeStartTrue) {
  // こ + れ/こ/ち
  EXPECT_TRUE(isDemonstrativeStart(U'こ', U'れ'));
  EXPECT_TRUE(isDemonstrativeStart(U'こ', U'こ'));
  EXPECT_TRUE(isDemonstrativeStart(U'こ', U'ち'));
  // そ + れ/こ/ち
  EXPECT_TRUE(isDemonstrativeStart(U'そ', U'れ'));
  EXPECT_TRUE(isDemonstrativeStart(U'そ', U'こ'));
  EXPECT_TRUE(isDemonstrativeStart(U'そ', U'ち'));
  // あ + れ/こ/ち
  EXPECT_TRUE(isDemonstrativeStart(U'あ', U'れ'));
  // ど + れ/こ/ち
  EXPECT_TRUE(isDemonstrativeStart(U'ど', U'れ'));
  EXPECT_TRUE(isDemonstrativeStart(U'ど', U'こ'));
  EXPECT_TRUE(isDemonstrativeStart(U'ど', U'ち'));
}

TEST(CharTypeTest, IsDemonstrativeStartFalse) {
  // Invalid first character
  EXPECT_FALSE(isDemonstrativeStart(U'か', U'れ'));
  EXPECT_FALSE(isDemonstrativeStart(U'た', U'こ'));
  // Invalid second character
  EXPECT_FALSE(isDemonstrativeStart(U'こ', U'の'));
  EXPECT_FALSE(isDemonstrativeStart(U'そ', U'な'));
}

// ============================================================================
// isNeverAdjectiveStemAfterKanji tests
// ============================================================================

TEST(CharTypeTest, IsNeverAdjectiveStemAfterKanjiTrue) {
  // Inherits from isNeverVerbStemAfterKanji
  EXPECT_TRUE(isNeverAdjectiveStemAfterKanji(U'を'));
  EXPECT_TRUE(isNeverAdjectiveStemAfterKanji(U'が'));
  EXPECT_TRUE(isNeverAdjectiveStemAfterKanji(U'も'));
  EXPECT_TRUE(isNeverAdjectiveStemAfterKanji(U'や'));
  // Additional: て, で
  EXPECT_TRUE(isNeverAdjectiveStemAfterKanji(U'て'));
  EXPECT_TRUE(isNeverAdjectiveStemAfterKanji(U'で'));
}

TEST(CharTypeTest, IsNeverAdjectiveStemAfterKanjiFalse) {
  EXPECT_FALSE(isNeverAdjectiveStemAfterKanji(U'い'));
  EXPECT_FALSE(isNeverAdjectiveStemAfterKanji(U'し'));
  EXPECT_FALSE(isNeverAdjectiveStemAfterKanji(U'く'));
}

// ============================================================================
// isExtendedParticle tests
// ============================================================================

TEST(CharTypeTest, IsExtendedParticleTrue) {
  // Common particles
  EXPECT_TRUE(isExtendedParticle(U'を'));
  EXPECT_TRUE(isExtendedParticle(U'が'));
  EXPECT_TRUE(isExtendedParticle(U'は'));
  EXPECT_TRUE(isExtendedParticle(U'に'));
  EXPECT_TRUE(isExtendedParticle(U'へ'));
  EXPECT_TRUE(isExtendedParticle(U'の'));
  // Sentence-final
  EXPECT_TRUE(isExtendedParticle(U'か'));
  EXPECT_TRUE(isExtendedParticle(U'ね'));
  EXPECT_TRUE(isExtendedParticle(U'よ'));
  EXPECT_TRUE(isExtendedParticle(U'わ'));
  // Additional
  EXPECT_TRUE(isExtendedParticle(U'で'));
  EXPECT_TRUE(isExtendedParticle(U'と'));
  EXPECT_TRUE(isExtendedParticle(U'も'));
}

TEST(CharTypeTest, IsExtendedParticleFalse) {
  EXPECT_FALSE(isExtendedParticle(U'や'));
  EXPECT_FALSE(isExtendedParticle(U'し'));
  EXPECT_FALSE(isExtendedParticle(U'あ'));
  EXPECT_FALSE(isExtendedParticle(U'A'));
}

// ============================================================================
// isProlongedSoundMark tests
// ============================================================================

TEST(CharTypeTest, IsProlongedSoundMark) {
  EXPECT_TRUE(isProlongedSoundMark(0x30FC));   // ー
  EXPECT_FALSE(isProlongedSoundMark(U'あ'));
  EXPECT_FALSE(isProlongedSoundMark(U'-'));     // ASCII hyphen
  EXPECT_FALSE(isProlongedSoundMark(0x2014));   // Em dash
}

// ============================================================================
// isEmojiModifier tests
// ============================================================================

TEST(CharTypeTest, IsEmojiModifier) {
  // ZWJ
  EXPECT_TRUE(isEmojiModifier(0x200D));
  // Variation selectors
  EXPECT_TRUE(isEmojiModifier(0xFE0E));
  EXPECT_TRUE(isEmojiModifier(0xFE0F));
  // Skin tone modifiers
  EXPECT_TRUE(isEmojiModifier(0x1F3FB));
  EXPECT_TRUE(isEmojiModifier(0x1F3FF));
  // Combining enclosing keycap
  EXPECT_TRUE(isEmojiModifier(0x20E3));
  // Tag characters
  EXPECT_TRUE(isEmojiModifier(0xE0020));
  EXPECT_TRUE(isEmojiModifier(0xE007F));
  // Non-modifiers
  EXPECT_FALSE(isEmojiModifier(U'A'));
  EXPECT_FALSE(isEmojiModifier(0x1F600));  // Emoji itself, not modifier
}

// ============================================================================
// isRegionalIndicator tests
// ============================================================================

TEST(CharTypeTest, IsRegionalIndicator) {
  EXPECT_TRUE(isRegionalIndicator(0x1F1E6));   // Regional A
  EXPECT_TRUE(isRegionalIndicator(0x1F1FF));   // Regional Z
  EXPECT_TRUE(isRegionalIndicator(0x1F1EF));   // Regional J
  EXPECT_FALSE(isRegionalIndicator(0x1F1E5));  // Before range
  EXPECT_FALSE(isRegionalIndicator(0x1F200));  // After range
  EXPECT_FALSE(isRegionalIndicator(U'A'));
}

// ============================================================================
// isIterationMark tests
// ============================================================================

TEST(CharTypeTest, IsIterationMark) {
  EXPECT_TRUE(isIterationMark(0x3005));   // 々
  EXPECT_FALSE(isIterationMark(U'人'));
  EXPECT_FALSE(isIterationMark(0x3006));
  EXPECT_FALSE(isIterationMark(0x3004));
}

// ============================================================================
// isARowHiragana through isORowHiragana - spot checks
// ============================================================================

TEST(CharTypeTest, IsARowHiragana) {
  EXPECT_TRUE(isARowHiragana(U'あ'));
  EXPECT_TRUE(isARowHiragana(U'か'));
  EXPECT_TRUE(isARowHiragana(U'が'));
  EXPECT_TRUE(isARowHiragana(U'さ'));
  EXPECT_TRUE(isARowHiragana(U'た'));
  EXPECT_TRUE(isARowHiragana(U'な'));
  EXPECT_TRUE(isARowHiragana(U'ま'));
  EXPECT_TRUE(isARowHiragana(U'や'));
  EXPECT_TRUE(isARowHiragana(U'ら'));
  EXPECT_TRUE(isARowHiragana(U'わ'));
  // Not a-row
  EXPECT_FALSE(isARowHiragana(U'い'));
  EXPECT_FALSE(isARowHiragana(U'う'));
  EXPECT_FALSE(isARowHiragana(U'え'));
  EXPECT_FALSE(isARowHiragana(U'お'));
}

TEST(CharTypeTest, IsIRowHiragana) {
  EXPECT_TRUE(isIRowHiragana(U'い'));
  EXPECT_TRUE(isIRowHiragana(U'き'));
  EXPECT_TRUE(isIRowHiragana(U'し'));
  EXPECT_TRUE(isIRowHiragana(U'ち'));
  EXPECT_TRUE(isIRowHiragana(U'に'));
  EXPECT_TRUE(isIRowHiragana(U'み'));
  EXPECT_TRUE(isIRowHiragana(U'り'));
  // Not i-row
  EXPECT_FALSE(isIRowHiragana(U'あ'));
  EXPECT_FALSE(isIRowHiragana(U'う'));
  EXPECT_FALSE(isIRowHiragana(U'え'));
}

TEST(CharTypeTest, IsURowHiragana) {
  EXPECT_TRUE(isURowHiragana(U'う'));
  EXPECT_TRUE(isURowHiragana(U'く'));
  EXPECT_TRUE(isURowHiragana(U'す'));
  EXPECT_TRUE(isURowHiragana(U'つ'));
  EXPECT_TRUE(isURowHiragana(U'ぬ'));
  EXPECT_TRUE(isURowHiragana(U'ふ'));
  EXPECT_TRUE(isURowHiragana(U'む'));
  EXPECT_TRUE(isURowHiragana(U'ゆ'));
  EXPECT_TRUE(isURowHiragana(U'る'));
  // Not u-row
  EXPECT_FALSE(isURowHiragana(U'あ'));
  EXPECT_FALSE(isURowHiragana(U'い'));
}

TEST(CharTypeTest, IsERowHiragana) {
  EXPECT_TRUE(isERowHiragana(U'え'));
  EXPECT_TRUE(isERowHiragana(U'け'));
  EXPECT_TRUE(isERowHiragana(U'せ'));
  EXPECT_TRUE(isERowHiragana(U'て'));
  EXPECT_TRUE(isERowHiragana(U'ね'));
  EXPECT_TRUE(isERowHiragana(U'め'));
  EXPECT_TRUE(isERowHiragana(U'れ'));
  // Not e-row
  EXPECT_FALSE(isERowHiragana(U'あ'));
  EXPECT_FALSE(isERowHiragana(U'い'));
  EXPECT_FALSE(isERowHiragana(U'お'));
}

TEST(CharTypeTest, IsORowHiragana) {
  EXPECT_TRUE(isORowHiragana(U'お'));
  EXPECT_TRUE(isORowHiragana(U'こ'));
  EXPECT_TRUE(isORowHiragana(U'そ'));
  EXPECT_TRUE(isORowHiragana(U'と'));
  EXPECT_TRUE(isORowHiragana(U'の'));
  EXPECT_TRUE(isORowHiragana(U'も'));
  EXPECT_TRUE(isORowHiragana(U'よ'));
  EXPECT_TRUE(isORowHiragana(U'ろ'));
  EXPECT_TRUE(isORowHiragana(U'を'));
  // Not o-row
  EXPECT_FALSE(isORowHiragana(U'あ'));
  EXPECT_FALSE(isORowHiragana(U'い'));
  EXPECT_FALSE(isORowHiragana(U'う'));
}

TEST(CharTypeTest, HiraganaRowMutualExclusion) {
  // Each hiragana should belong to exactly one row
  // Spot-check a few characters
  char32_t test_chars[] = {U'あ', U'い', U'う', U'え', U'お',
                           U'か', U'き', U'く', U'け', U'こ'};
  for (auto chr : test_chars) {
    int count = 0;
    if (isARowHiragana(chr)) count++;
    if (isIRowHiragana(chr)) count++;
    if (isURowHiragana(chr)) count++;
    if (isERowHiragana(chr)) count++;
    if (isORowHiragana(chr)) count++;
    EXPECT_EQ(count, 1) << "Character should belong to exactly one row";
  }
}

// ============================================================================
// isKanjiCodepoint tests
// ============================================================================

TEST(CharTypeTest, IsKanjiCodepoint) {
  // Main CJK range
  EXPECT_TRUE(isKanjiCodepoint(U'漢'));
  EXPECT_TRUE(isKanjiCodepoint(0x4E00));
  EXPECT_TRUE(isKanjiCodepoint(0x9FFF));
  // Extension A
  EXPECT_TRUE(isKanjiCodepoint(0x3400));
  EXPECT_TRUE(isKanjiCodepoint(0x4DBF));
  // Extension B
  EXPECT_TRUE(isKanjiCodepoint(0x20000));
  EXPECT_TRUE(isKanjiCodepoint(0x2A6DF));
  // Extension C
  EXPECT_TRUE(isKanjiCodepoint(0x2A700));
  // Extension D
  EXPECT_TRUE(isKanjiCodepoint(0x2B740));
  // CJK Compatibility
  EXPECT_TRUE(isKanjiCodepoint(0xF900));
  // Kangxi Radicals
  EXPECT_TRUE(isKanjiCodepoint(0x2F00));

  // Not kanji
  EXPECT_FALSE(isKanjiCodepoint(U'あ'));
  EXPECT_FALSE(isKanjiCodepoint(U'A'));
  EXPECT_FALSE(isKanjiCodepoint(0x3005));  // 々 is not in isKanjiCodepoint
}

// ============================================================================
// isCounterKanji tests
// ============================================================================

TEST(CharTypeTest, IsCounterKanjiTrue) {
  // Currency
  EXPECT_TRUE(isCounterKanji(U'円'));
  EXPECT_TRUE(isCounterKanji(U'万'));
  EXPECT_TRUE(isCounterKanji(U'億'));
  // Time
  EXPECT_TRUE(isCounterKanji(U'年'));
  EXPECT_TRUE(isCounterKanji(U'月'));
  EXPECT_TRUE(isCounterKanji(U'日'));
  EXPECT_TRUE(isCounterKanji(U'時'));
  EXPECT_TRUE(isCounterKanji(U'分'));
  EXPECT_TRUE(isCounterKanji(U'秒'));
  // General counters
  EXPECT_TRUE(isCounterKanji(U'個'));
  EXPECT_TRUE(isCounterKanji(U'人'));
  EXPECT_TRUE(isCounterKanji(U'回'));
  EXPECT_TRUE(isCounterKanji(U'歳'));
  EXPECT_TRUE(isCounterKanji(U'階'));
  // Units
  EXPECT_TRUE(isCounterKanji(U'度'));
  EXPECT_TRUE(isCounterKanji(U'倍'));
  // Compound second char
  EXPECT_TRUE(isCounterKanji(U'間'));
  EXPECT_TRUE(isCounterKanji(U'紀'));
  // Ordinal
  EXPECT_TRUE(isCounterKanji(U'次'));
}

TEST(CharTypeTest, IsCounterKanjiFalse) {
  EXPECT_FALSE(isCounterKanji(U'漢'));
  EXPECT_FALSE(isCounterKanji(U'字'));
  EXPECT_FALSE(isCounterKanji(U'山'));
  EXPECT_FALSE(isCounterKanji(U'川'));
  EXPECT_FALSE(isCounterKanji(U'A'));
}

}  // namespace
}  // namespace normalize
}  // namespace suzume
