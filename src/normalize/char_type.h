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

/**
 * @brief Check if character is in the A-row (あ段) of hiragana
 *
 * A-row hiragana: あ, か, が, さ, ざ, た, だ, な, は, ば, ぱ, ま, や, ら, わ
 * Used for 未然形 (mizenkei) detection in verb conjugation.
 *
 * @param ch Unicode codepoint
 * @return true if character is in the A-row
 */
bool isARowHiragana(char32_t ch);

/**
 * @brief Check if character is in the I-row (い段) of hiragana
 *
 * I-row hiragana: い, き, ぎ, し, じ, ち, ぢ, に, ひ, び, ぴ, み, り
 * Used for 連用形 (renyoukei) detection in verb conjugation.
 *
 * @param ch Unicode codepoint
 * @return true if character is in the I-row
 */
bool isIRowHiragana(char32_t ch);

/**
 * @brief Check if character is in the U-row (う段) of hiragana
 *
 * U-row hiragana: う, く, ぐ, す, ず, つ, づ, ぬ, ふ, ぶ, ぷ, む, ゆ, る
 * Used for 終止形/連体形 detection (dictionary form).
 *
 * @param ch Unicode codepoint
 * @return true if character is in the U-row
 */
bool isURowHiragana(char32_t ch);

/**
 * @brief Check if character is in the E-row (え段) of hiragana
 *
 * E-row hiragana: え, け, げ, せ, ぜ, て, で, ね, へ, べ, ぺ, め, れ
 * Used for 一段語幹 (ichidan stem) and 仮定形 (hypothetical) detection.
 *
 * @param ch Unicode codepoint
 * @return true if character is in the E-row
 */
bool isERowHiragana(char32_t ch);

/**
 * @brief Check if character is in the O-row (お段) of hiragana
 *
 * O-row hiragana: お, こ, ご, そ, ぞ, と, ど, の, ほ, ぼ, ぽ, も, よ, ろ, を
 * Used for 意志形 (volitional) detection.
 *
 * @param ch Unicode codepoint
 * @return true if character is in the O-row
 */
bool isORowHiragana(char32_t ch);

/**
 * @brief Check if character is a CJK Unified Ideograph (kanji)
 *
 * Checks all CJK ranges including:
 * - CJK Unified Ideographs (U+4E00-U+9FFF)
 * - CJK Extension A-D
 * - CJK Compatibility Ideographs
 * - Kangxi Radicals
 *
 * @param ch Unicode codepoint
 * @return true if character is a kanji
 */
bool isKanjiCodepoint(char32_t ch);

/**
 * @brief Check if a kanji codepoint is a counter/unit (助数詞・単位)
 *
 * Counter kanji that naturally follow digits: 円, 分, 個, 人, etc.
 * Used to determine if a digit+kanji merge is valid.
 *
 * @param cp Unicode codepoint
 * @return true if codepoint is a counter/unit kanji
 */
bool isCounterKanji(char32_t cp);

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_CHAR_TYPE_H_
