#ifndef SUZUME_NORMALIZE_CHAR_TYPE_H_
#define SUZUME_NORMALIZE_CHAR_TYPE_H_

#include <cstdint>
#include <string_view>

namespace suzume::normalize {

/**
 * @brief Character type classification
 */
enum class CharType : uint8_t {
  Kanji,     // 漢字
  Hiragana,  // ひらがな
  Katakana,  // カタカナ
  Alphabet,  // 英字
  Digit,     // 数字
  Symbol,    // 記号
  Emoji,     // 絵文字
  Unknown    // 不明
};

/**
 * @brief Classify a Unicode codepoint
 * @param codepoint Unicode codepoint
 * @return Character type
 */
CharType classifyChar(char32_t codepoint);

/**
 * @brief Convert character type to string
 */
std::string_view charTypeToString(CharType type);

/**
 * @brief Check if character type can be combined
 * @param first_type First character type
 * @param second_type Second character type
 * @return true if can be combined as unknown word
 */
bool canCombine(CharType first_type, CharType second_type);

/**
 * @brief Check if character is a common particle (助詞)
 *
 * Common particles that never form verb stems: を, が, は, に, へ, の
 * These appear in multiple contexts and warrant a shared predicate.
 *
 * @param ch Unicode codepoint
 * @return true if character is a common particle
 */
bool isCommonParticle(char32_t ch);

/**
 * @brief Check if character cannot be verb stem after kanji
 *
 * In patterns like 漢字+ひらがな, these characters indicate a particle
 * follows the noun rather than a verb conjugation.
 * Includes common particles + も, や
 *
 * @param ch Unicode codepoint
 * @return true if character is never a verb stem after kanji
 */
bool isNeverVerbStemAfterKanji(char32_t ch);

/**
 * @brief Check if character cannot be verb stem at start of hiragana word
 *
 * These characters cannot begin hiragana verb stems.
 * Includes common particles + ね, よ, わ (sentence-final particles)
 *
 * @param ch Unicode codepoint
 * @return true if character cannot start a hiragana verb stem
 */
bool isNeverVerbStemAtStart(char32_t ch);

/**
 * @brief Check if pair starts a demonstrative pronoun
 *
 * Checks for patterns: こ/そ/あ/ど + れ/こ/ち
 * These are demonstrative pronouns like これ, それ, あれ, どれ, etc.
 *
 * @param first First character
 * @param second Second character
 * @return true if pair starts a demonstrative pronoun
 */
bool isDemonstrativeStart(char32_t first, char32_t second);

/**
 * @brief Check if character is never an adjective stem after kanji
 *
 * In patterns like 漢字+ひらがな, these characters indicate a particle
 * follows rather than an adjective conjugation.
 * Includes isNeverVerbStemAfterKanji + て, で (te-form particles)
 *
 * @param ch Unicode codepoint
 * @return true if character cannot start an adjective stem after kanji
 */
bool isNeverAdjectiveStemAfterKanji(char32_t ch);

/**
 * @brief Check if character is a sentence-final or common particle
 *
 * Extended particle check including common particles and sentence-final particles.
 * Includes: を, が, は, に, へ, の, か, ね, よ, わ, で, と, も
 *
 * @param ch Unicode codepoint
 * @return true if character is an extended particle
 */
bool isExtendedParticle(char32_t ch);

/**
 * @brief Check if character is the prolonged sound mark (長音符)
 *
 * The prolonged sound mark (ー, U+30FC) is used to extend vowel sounds.
 * It appears in katakana words but is also commonly used in
 * colloquial hiragana (すごーい, やばーい).
 *
 * @param ch Unicode codepoint
 * @return true if character is the prolonged sound mark
 */
bool isProlongedSoundMark(char32_t ch);

/**
 * @brief Check if character is an emoji modifier
 *
 * Emoji modifiers include:
 * - ZWJ (Zero Width Joiner, U+200D): combines emojis (👨‍👩‍👧)
 * - Variation Selectors (U+FE0E-FE0F): text vs emoji presentation
 * - Skin tone modifiers (U+1F3FB-1F3FF): 🏻🏼🏽🏾🏿
 * - Combining Enclosing Keycap (U+20E3): keycap emojis (1️⃣)
 * - Tag characters (U+E0020-E007F): regional flags (🏴󠁧󠁢󠁥󠁮󠁧󠁿)
 *
 * @param ch Unicode codepoint
 * @return true if character is an emoji modifier
 */
bool isEmojiModifier(char32_t ch);

/**
 * @brief Check if character is a Regional Indicator Symbol
 *
 * Regional indicators (U+1F1E6-1F1FF) are used in pairs to form
 * country flag emojis (e.g., 🇯🇵 = U+1F1EF U+1F1F5).
 *
 * @param ch Unicode codepoint
 * @return true if character is a regional indicator
 */
bool isRegionalIndicator(char32_t ch);

/**
 * @brief Check if character is an ideographic iteration mark (踊り字)
 *
 * The iteration mark (々, U+3005) repeats the preceding kanji.
 * It's used in words like 人々, 日々, 堂々, 時々.
 * When following a kanji, it should be grouped as part of the kanji sequence.
 *
 * @param ch Unicode codepoint
 * @return true if character is the iteration mark
 */
bool isIterationMark(char32_t ch);

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_CHAR_TYPE_H_
