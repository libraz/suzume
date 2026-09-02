#ifndef SUZUME_CORE_KANA_CONSTANTS_H_
#define SUZUME_CORE_KANA_CONSTANTS_H_

#include <cstddef>

namespace suzume::kana {

// =============================================================================
// Verb Conjugation Specific Endings
// =============================================================================

// 未然形 (Mizenkei) endings: A-row subset used in verb mizenkei
inline constexpr const char* kMizenkeiEndings[] = {"か", "が", "さ", "た", "な", "ば", "ま", "ら", "わ"};
inline constexpr size_t kMizenkeiCount = sizeof(kMizenkeiEndings) / sizeof(kMizenkeiEndings[0]);

// 連用形 (Renyokei) endings for Godan verbs: I-row without い
// (い is the renyokei of う-row verbs like 思う→思い)
inline constexpr const char* kRenyokeiEndings[] = {"き", "ぎ", "し", "ち", "に", "び", "み", "り"};
inline constexpr size_t kRenyokeiCount = sizeof(kRenyokeiEndings) / sizeof(kRenyokeiEndings[0]);

// =============================================================================
// Codepoint-based Row Checks
// =============================================================================
// Fast O(1) checks for hiragana vowel rows using codepoints.

inline bool isARowCodepoint(char32_t cp) {
  return cp == U'あ' || cp == U'か' || cp == U'が' || cp == U'さ' || cp == U'ざ' || cp == U'た' || cp == U'だ' ||
         cp == U'な' || cp == U'は' || cp == U'ば' || cp == U'ぱ' || cp == U'ま' || cp == U'や' || cp == U'ら' ||
         cp == U'わ';
}

inline bool isIRowCodepoint(char32_t cp) {
  return cp == U'い' || cp == U'き' || cp == U'ぎ' || cp == U'し' || cp == U'じ' || cp == U'ち' || cp == U'ぢ' ||
         cp == U'に' || cp == U'ひ' || cp == U'び' || cp == U'ぴ' || cp == U'み' || cp == U'り';
}

inline bool isURowCodepoint(char32_t cp) {
  return cp == U'う' || cp == U'く' || cp == U'ぐ' || cp == U'す' || cp == U'ず' || cp == U'つ' || cp == U'づ' ||
         cp == U'ぬ' || cp == U'ふ' || cp == U'ぶ' || cp == U'ぷ' || cp == U'む' || cp == U'ゆ' || cp == U'る';
}

inline bool isERowCodepoint(char32_t cp) {
  return cp == U'え' || cp == U'け' || cp == U'げ' || cp == U'せ' || cp == U'ぜ' || cp == U'て' || cp == U'で' ||
         cp == U'ね' || cp == U'へ' || cp == U'べ' || cp == U'ぺ' || cp == U'め' || cp == U'れ';
}

inline bool isORowCodepoint(char32_t cp) {
  return cp == U'お' || cp == U'こ' || cp == U'ご' || cp == U'そ' || cp == U'ぞ' || cp == U'と' || cp == U'ど' ||
         cp == U'の' || cp == U'ほ' || cp == U'ぼ' || cp == U'ぽ' || cp == U'も' || cp == U'よ' || cp == U'ろ' ||
         cp == U'を';
}

/**
 * @brief Whether a kana belongs to the ら column
 *
 * No native Japanese word starts on this column, mimetics included, so a
 * candidate that would open one there is reading a word boundary wrong.
 */
inline bool isRaColumnCodepoint(char32_t cp) {
  return cp == U'ら' || cp == U'り' || cp == U'る' || cp == U'れ' || cp == U'ろ';
}

// =============================================================================
// Character Type Checks (Codepoint-based)
// =============================================================================

inline bool isHiraganaCodepoint(char32_t cp) {
  return cp >= 0x3040 && cp <= 0x309F;
}

inline bool isKatakanaCodepoint(char32_t cp) {
  return (cp >= 0x30A0 && cp <= 0x30FF) || (cp >= 0x31F0 && cp <= 0x31FF);
}

inline bool isKanjiCodepoint(char32_t cp) {
  // Single source of truth for "is this codepoint a kanji". normalize:: forwards
  // to this so every layer (grammar compound-verb probes, analysis, char typing)
  // agrees on the extended/compatibility/radical ranges, not just the BMP core.
  return (cp >= 0x4E00 && cp <= 0x9FFF) ||    // CJK Unified Ideographs
         (cp >= 0x3400 && cp <= 0x4DBF) ||    // CJK Extension A
         (cp >= 0x20000 && cp <= 0x2A6DF) ||  // CJK Extension B
         (cp >= 0x2A700 && cp <= 0x2B73F) ||  // CJK Extension C
         (cp >= 0x2B740 && cp <= 0x2B81F) ||  // CJK Extension D
         (cp >= 0x2B820 && cp <= 0x2CEAF) ||  // CJK Extension E
         (cp >= 0x2CEB0 && cp <= 0x2EBEF) ||  // CJK Extension F
         (cp >= 0x2EBF0 && cp <= 0x2EE5F) ||  // CJK Extension I
         (cp >= 0x30000 && cp <= 0x3134F) ||  // CJK Extension G
         (cp >= 0x31350 && cp <= 0x323AF) ||  // CJK Extension H
         (cp >= 0x323B0 && cp <= 0x3347F) ||  // CJK Extension J
         (cp >= 0xF900 && cp <= 0xFAFF) ||    // CJK Compatibility Ideographs
         (cp >= 0x2F800 && cp <= 0x2FA1F) ||  // CJK Compatibility Ideographs Supplement
         (cp >= 0x2F00 && cp <= 0x2FDF);      // Kangxi Radicals
}

inline bool isOnbinCodepoint(char32_t cp) {
  return cp == U'い' || cp == U'っ' || cp == U'ん';
}

inline bool isSmallKanaCodepoint(char32_t cp) {
  // Hiragana small kana
  if (cp == U'ゃ' || cp == U'ゅ' || cp == U'ょ' || cp == U'ぁ' || cp == U'ぃ' || cp == U'ぅ' || cp == U'ぇ' ||
      cp == U'ぉ' || cp == U'っ' || cp == U'ゎ' || cp == U'ゕ' || cp == U'ゖ') {
    return true;
  }
  // Katakana small kana
  if (cp == U'ャ' || cp == U'ュ' || cp == U'ョ' || cp == U'ァ' || cp == U'ィ' || cp == U'ゥ' || cp == U'ェ' ||
      cp == U'ォ' || cp == U'ッ' || cp == U'ヮ' || cp == U'ヵ' || cp == U'ヶ') {
    return true;
  }
  // ヵ/ヶ can also act as counters, but their small-kana codepoint identity is
  // still needed when they occur at the start of an otherwise impossible word.
  return false;
}

}  // namespace suzume::kana

#endif  // SUZUME_CORE_KANA_CONSTANTS_H_
