#ifndef SUZUME_CORE_KANA_CONSTANTS_H_
#define SUZUME_CORE_KANA_CONSTANTS_H_

#include <string_view>
#include <unordered_set>

namespace suzume::kana {

// =============================================================================
// Hiragana Vowel Row Constants (五十音 - 母音行)
// =============================================================================
// Each row contains all hiragana characters that share the same vowel ending.
// These are fundamental to Japanese verb conjugation patterns.

// あ段 (a-row): Characters ending with 'a' vowel
// Used for: Mizenkei (未然形), negative forms
inline constexpr const char* kARow[] = {
    "あ", "か", "が", "さ", "ざ", "た", "だ", "な", "は", "ば", "ぱ", "ま", "や", "ら", "わ"
};
inline constexpr size_t kARowCount = sizeof(kARow) / sizeof(kARow[0]);

// い段 (i-row): Characters ending with 'i' vowel
// Used for: Renyokei (連用形) of godan verbs, kami-ichidan verbs
inline constexpr const char* kIRow[] = {
    "い", "き", "ぎ", "し", "じ", "ち", "ぢ", "に", "ひ", "び", "ぴ", "み", "り"
};
inline constexpr size_t kIRowCount = sizeof(kIRow) / sizeof(kIRow[0]);

// う段 (u-row): Characters ending with 'u' vowel
// Used for: Dictionary form (終止形) of godan verbs
inline constexpr const char* kURow[] = {
    "う", "く", "ぐ", "す", "ず", "つ", "づ", "ぬ", "ふ", "ぶ", "ぷ", "む", "ゆ", "る"
};
inline constexpr size_t kURowCount = sizeof(kURow) / sizeof(kURow[0]);

// え段 (e-row): Characters ending with 'e' vowel
// Used for: Renyokei (連用形) of shimo-ichidan verbs, imperative, potential
inline constexpr const char* kERow[] = {
    "え", "け", "げ", "せ", "ぜ", "て", "で", "ね", "へ", "べ", "ぺ", "め", "れ"
};
inline constexpr size_t kERowCount = sizeof(kERow) / sizeof(kERow[0]);

// お段 (o-row): Characters ending with 'o' vowel
// Used for: Volitional (意志形)
inline constexpr const char* kORow[] = {
    "お", "こ", "ご", "そ", "ぞ", "と", "ど", "の", "ほ", "ぼ", "ぽ", "も", "よ", "ろ", "を"
};
inline constexpr size_t kORowCount = sizeof(kORow) / sizeof(kORow[0]);

// =============================================================================
// Onbin (音便) Endings
// =============================================================================
// Sound changes that occur in verb conjugations:
// - っ (促音便): From つ, く verbs before て/た
// - ん (撥音便): From む, ぬ, ぶ verbs before で/だ
// - い (イ音便): From く, ぐ verbs before て/た

inline constexpr const char* kOnbinEndings[] = {"い", "っ", "ん"};
inline constexpr size_t kOnbinCount = sizeof(kOnbinEndings) / sizeof(kOnbinEndings[0]);

// =============================================================================
// Verb Conjugation Specific Endings
// =============================================================================

// 未然形 (Mizenkei) endings: A-row subset used in verb mizenkei
inline constexpr const char* kMizenkeiEndings[] = {
    "か", "が", "さ", "た", "な", "ば", "ま", "ら", "わ"
};
inline constexpr size_t kMizenkeiCount = sizeof(kMizenkeiEndings) / sizeof(kMizenkeiEndings[0]);

// 連用形 (Renyokei) endings for Godan verbs: I-row without い
// (い is the renyokei of う-row verbs like 思う→思い)
inline constexpr const char* kRenyokeiEndings[] = {
    "き", "ぎ", "し", "ち", "に", "び", "み", "り"
};
inline constexpr size_t kRenyokeiCount = sizeof(kRenyokeiEndings) / sizeof(kRenyokeiEndings[0]);

// =============================================================================
// Small Kana (拗音・促音)
// =============================================================================
// These cannot start a word and are always part of compound sounds.

inline constexpr const char* kSmallHiragana[] = {
    "ゃ", "ゅ", "ょ", "ぁ", "ぃ", "ぅ", "ぇ", "ぉ", "っ"
};
inline constexpr size_t kSmallHiraganaCount = sizeof(kSmallHiragana) / sizeof(kSmallHiragana[0]);

inline constexpr const char* kSmallKatakana[] = {
    "ャ", "ュ", "ョ", "ァ", "ィ", "ゥ", "ェ", "ォ", "ッ"
};
inline constexpr size_t kSmallKatakanaCount = sizeof(kSmallKatakana) / sizeof(kSmallKatakana[0]);

// =============================================================================
// Common Particles (助詞)
// =============================================================================
// These are valid as single-character tokens.

// 格助詞 (Case particles)
inline constexpr const char* kCaseParticles[] = {
    "が", "を", "に", "で", "と", "へ", "の"
};
inline constexpr size_t kCaseParticleCount = sizeof(kCaseParticles) / sizeof(kCaseParticles[0]);

// 係助詞 (Binding/Topic particles)
inline constexpr const char* kBindingParticles[] = {
    "は", "も"
};
inline constexpr size_t kBindingParticleCount = sizeof(kBindingParticles) / sizeof(kBindingParticles[0]);

// 終助詞 (Final particles)
inline constexpr const char* kFinalParticles[] = {
    "か", "な", "ね", "よ", "わ"
};
inline constexpr size_t kFinalParticleCount = sizeof(kFinalParticles) / sizeof(kFinalParticles[0]);

// 接続助詞 (Conjunctive particles)
inline constexpr const char* kConjunctiveParticles[] = {
    "て", "ば"
};
inline constexpr size_t kConjunctiveParticleCount = sizeof(kConjunctiveParticles) / sizeof(kConjunctiveParticles[0]);

// =============================================================================
// Codepoint-based Row Checks
// =============================================================================
// Fast O(1) checks for hiragana vowel rows using codepoints.

inline bool isARowCodepoint(char32_t cp) {
  return cp == U'あ' || cp == U'か' || cp == U'が' || cp == U'さ' || cp == U'ざ' ||
         cp == U'た' || cp == U'だ' || cp == U'な' || cp == U'は' || cp == U'ば' ||
         cp == U'ぱ' || cp == U'ま' || cp == U'や' || cp == U'ら' || cp == U'わ';
}

inline bool isIRowCodepoint(char32_t cp) {
  return cp == U'い' || cp == U'き' || cp == U'ぎ' || cp == U'し' || cp == U'じ' ||
         cp == U'ち' || cp == U'ぢ' || cp == U'に' || cp == U'ひ' || cp == U'び' ||
         cp == U'ぴ' || cp == U'み' || cp == U'り';
}

inline bool isURowCodepoint(char32_t cp) {
  return cp == U'う' || cp == U'く' || cp == U'ぐ' || cp == U'す' || cp == U'ず' ||
         cp == U'つ' || cp == U'づ' || cp == U'ぬ' || cp == U'ふ' || cp == U'ぶ' ||
         cp == U'ぷ' || cp == U'む' || cp == U'ゆ' || cp == U'る';
}

inline bool isERowCodepoint(char32_t cp) {
  return cp == U'え' || cp == U'け' || cp == U'げ' || cp == U'せ' || cp == U'ぜ' ||
         cp == U'て' || cp == U'で' || cp == U'ね' || cp == U'へ' || cp == U'べ' ||
         cp == U'ぺ' || cp == U'め' || cp == U'れ';
}

inline bool isORowCodepoint(char32_t cp) {
  return cp == U'お' || cp == U'こ' || cp == U'ご' || cp == U'そ' || cp == U'ぞ' ||
         cp == U'と' || cp == U'ど' || cp == U'の' || cp == U'ほ' || cp == U'ぼ' ||
         cp == U'ぽ' || cp == U'も' || cp == U'よ' || cp == U'ろ' || cp == U'を';
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
  return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF);
}

inline bool isSmallKanaCodepoint(char32_t cp) {
  // Hiragana small kana
  if (cp == U'ゃ' || cp == U'ゅ' || cp == U'ょ' ||
      cp == U'ぁ' || cp == U'ぃ' || cp == U'ぅ' || cp == U'ぇ' || cp == U'ぉ' ||
      cp == U'っ') {
    return true;
  }
  // Katakana small kana
  if (cp == U'ャ' || cp == U'ュ' || cp == U'ョ' ||
      cp == U'ァ' || cp == U'ィ' || cp == U'ゥ' || cp == U'ェ' || cp == U'ォ' ||
      cp == U'ッ') {
    return true;
  }
  return false;
}

}  // namespace suzume::kana

#endif  // SUZUME_CORE_KANA_CONSTANTS_H_
