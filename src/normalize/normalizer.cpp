#include "normalizer.h"

#include "utf8.h"

namespace suzume::normalize {

namespace {

// Full-width ASCII to half-width
char32_t fullwidthToHalfwidth(char32_t codepoint) {
  // Full-width digits (０-９) -> half-width (0-9)
  if (codepoint >= 0xFF10 && codepoint <= 0xFF19) {
    return codepoint - 0xFF10 + '0';
  }
  // Full-width uppercase (Ａ-Ｚ) -> half-width lowercase (a-z)
  if (codepoint >= 0xFF21 && codepoint <= 0xFF3A) {
    return codepoint - 0xFF21 + 'a';
  }
  // Full-width lowercase (ａ-ｚ) -> half-width lowercase (a-z)
  if (codepoint >= 0xFF41 && codepoint <= 0xFF5A) {
    return codepoint - 0xFF41 + 'a';
  }
  // Half-width uppercase (A-Z) -> lowercase (a-z)
  if (codepoint >= 'A' && codepoint <= 'Z') {
    return codepoint - 'A' + 'a';
  }
  return codepoint;
}

// Half-width katakana to full-width
char32_t halfwidthKatakanaToFullwidth(char32_t codepoint) {
  // Half-width katakana range: U+FF66-U+FF9F
  if (codepoint >= 0xFF66 && codepoint <= 0xFF9F) {
    // Simplified mapping (main characters only)
    // Real implementation would need complete mapping table
    static const char32_t kMapping[] = {
        0x30F2,                                                          // ｦ -> ヲ
        0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9,                          // ｧｨｩｪｫ -> ァィゥェォ
        0x30E3, 0x30E5, 0x30E7,                                          // ｬｭｮ -> ャュョ
        0x30C3,                                                          // ｯ -> ッ
        0x30FC,                                                          // ｰ -> ー
        0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA,                          // ｱｲｳｴｵ -> アイウエオ
        0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3,                          // ｶｷｸｹｺ -> カキクケコ
        0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD,                          // ｻｼｽｾｿ -> サシスセソ
        0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8,                          // ﾀﾁﾂﾃﾄ -> タチツテト
        0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE,                          // ﾅﾆﾇﾈﾉ -> ナニヌネノ
        0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB,                          // ﾊﾋﾌﾍﾎ -> ハヒフヘホ
        0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2,                          // ﾏﾐﾑﾒﾓ -> マミムメモ
        0x30E4, 0x30E6, 0x30E8,                                          // ﾔﾕﾖ -> ヤユヨ
        0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED,                          // ﾗﾘﾙﾚﾛ -> ラリルレロ
        0x30EF, 0x30F3,                                                  // ﾜﾝ -> ワン
    };
    size_t idx = codepoint - 0xFF66;
    if (idx < sizeof(kMapping) / sizeof(kMapping[0])) {
      return kMapping[idx];
    }
  }
  return codepoint;
}

// Vu-series (ヴ) normalization
// ヴァ→バ, ヴィ→ビ, ヴ→ブ, ヴェ→ベ, ヴォ→ボ
constexpr char32_t kKatakanaVu = 0x30F4;      // ヴ
constexpr char32_t kKatakanaSmallA = 0x30A1;  // ァ
constexpr char32_t kKatakanaSmallI = 0x30A3;  // ィ
constexpr char32_t kKatakanaSmallU = 0x30A5;  // ゥ
constexpr char32_t kKatakanaSmallE = 0x30A7;  // ェ
constexpr char32_t kKatakanaSmallO = 0x30A9;  // ォ

constexpr char32_t kKatakanaBa = 0x30D0;  // バ
constexpr char32_t kKatakanaBi = 0x30D3;  // ビ
constexpr char32_t kKatakanaBu = 0x30D6;  // ブ
constexpr char32_t kKatakanaBe = 0x30D9;  // ベ
constexpr char32_t kKatakanaBo = 0x30DC;  // ボ

// Hiragana vu (rare but exists)
constexpr char32_t kHiraganaVu = 0x3094;      // ゔ
constexpr char32_t kHiraganaSmallA = 0x3041;  // ぁ
constexpr char32_t kHiraganaSmallI = 0x3043;  // ぃ
constexpr char32_t kHiraganaSmallU = 0x3045;  // ぅ
constexpr char32_t kHiraganaSmallE = 0x3047;  // ぇ
constexpr char32_t kHiraganaSmallO = 0x3049;  // ぉ

constexpr char32_t kHiraganaBa = 0x3070;  // ば
constexpr char32_t kHiraganaBi = 0x3073;  // び
constexpr char32_t kHiraganaBu = 0x3076;  // ぶ
constexpr char32_t kHiraganaBe = 0x3079;  // べ
constexpr char32_t kHiraganaBo = 0x307C;  // ぼ

// Half-width dakuten and handakuten
constexpr char32_t kHalfwidthDakuten = 0xFF9E;     // ﾞ
constexpr char32_t kHalfwidthHandakuten = 0xFF9F;  // ﾟ

// Combines full-width katakana with dakuten, returns combined char or 0 if not applicable
char32_t combineWithDakuten(char32_t base) {
  // Ka-row: カキクケコ -> ガギグゲゴ
  if (base >= 0x30AB && base <= 0x30B3 && ((base - 0x30AB) % 2 == 0)) {
    return base + 1;
  }
  // Sa-row: サシスセソ -> ザジズゼゾ
  if (base >= 0x30B5 && base <= 0x30BD && ((base - 0x30B5) % 2 == 0)) {
    return base + 1;
  }
  // Ta-row: タチツテト -> ダヂヅデド
  if (base >= 0x30BF && base <= 0x30C8) {
    // タ(0x30BF)->ダ, チ(0x30C1)->ヂ, ツ(0x30C4)->ヅ, テ(0x30C6)->デ, ト(0x30C8)->ド
    switch (base) {
      case 0x30BF:
        return 0x30C0;  // タ -> ダ
      case 0x30C1:
        return 0x30C2;  // チ -> ヂ
      case 0x30C4:
        return 0x30C5;  // ツ -> ヅ
      case 0x30C6:
        return 0x30C7;  // テ -> デ
      case 0x30C8:
        return 0x30C9;  // ト -> ド
      default:
        return 0;
    }
  }
  // Ha-row: ハヒフヘホ -> バビブベボ
  if (base >= 0x30CF && base <= 0x30DD && ((base - 0x30CF) % 3 == 0)) {
    return base + 1;
  }
  // ウ -> ヴ
  if (base == 0x30A6) {
    return 0x30F4;
  }
  // ワ -> ヷ (rare)
  if (base == 0x30EF) {
    return 0x30F7;
  }
  return 0;
}

// Combines full-width katakana with handakuten, returns combined char or 0 if not applicable
char32_t combineWithHandakuten(char32_t base) {
  // Ha-row only: ハヒフヘホ -> パピプペポ
  if (base >= 0x30CF && base <= 0x30DD && ((base - 0x30CF) % 3 == 0)) {
    return base + 2;
  }
  return 0;
}

// Returns normalized character for ヴ + small vowel, or 0 if not applicable
char32_t normalizeVuSequence(char32_t vu_char, char32_t next_char) {
  if (vu_char == kKatakanaVu) {
    switch (next_char) {
      case kKatakanaSmallA:
        return kKatakanaBa;
      case kKatakanaSmallI:
        return kKatakanaBi;
      case kKatakanaSmallU:
        return kKatakanaBu;
      case kKatakanaSmallE:
        return kKatakanaBe;
      case kKatakanaSmallO:
        return kKatakanaBo;
      default:
        return 0;
    }
  }
  if (vu_char == kHiraganaVu) {
    switch (next_char) {
      case kHiraganaSmallA:
        return kHiraganaBa;
      case kHiraganaSmallI:
        return kHiraganaBi;
      case kHiraganaSmallU:
        return kHiraganaBu;
      case kHiraganaSmallE:
        return kHiraganaBe;
      case kHiraganaSmallO:
        return kHiraganaBo;
      default:
        return 0;
    }
  }
  return 0;
}

}  // namespace

char32_t Normalizer::normalizeChar(char32_t codepoint) {
  codepoint = fullwidthToHalfwidth(codepoint);
  codepoint = halfwidthKatakanaToFullwidth(codepoint);
  return codepoint;
}

core::Result<std::string> Normalizer::normalize(std::string_view text) const {
  if (!isValidUtf8(text)) {
    return core::Error(core::ErrorCode::InvalidUtf8, "Invalid UTF-8 input");
  }

  std::string result;
  result.reserve(text.size());

  size_t pos = 0;
  while (pos < text.size()) {
    char32_t codepoint = decodeUtf8(text, pos);

    // Check for half-width dakuten/handakuten combining
    // Need to peek at next char before normalizing current
    char32_t normalized_cp = normalizeChar(codepoint);

    // Look ahead for half-width dakuten/handakuten
    size_t next_pos = pos;
    if (next_pos < text.size()) {
      char32_t next_cp = decodeUtf8(text, next_pos);

      // Check if next char is half-width dakuten or handakuten
      if (next_cp == kHalfwidthDakuten) {
        char32_t combined = combineWithDakuten(normalized_cp);
        if (combined != 0) {
          pos = next_pos;  // Consume the dakuten
          encodeUtf8(combined, result);
          continue;
        }
      } else if (next_cp == kHalfwidthHandakuten) {
        char32_t combined = combineWithHandakuten(normalized_cp);
        if (combined != 0) {
          pos = next_pos;  // Consume the handakuten
          encodeUtf8(combined, result);
          continue;
        }
      }
    }

    codepoint = normalized_cp;

    // Handle vu-series normalization (ヴァ→バ, etc.)
    if (codepoint == kKatakanaVu || codepoint == kHiraganaVu) {
      next_pos = pos;
      if (next_pos < text.size()) {
        char32_t next_cp = decodeUtf8(text, next_pos);
        next_cp = normalizeChar(next_cp);
        char32_t normalized = normalizeVuSequence(codepoint, next_cp);
        if (normalized != 0) {
          // Consume the small vowel and output normalized character
          pos = next_pos;
          encodeUtf8(normalized, result);
          continue;
        }
      }
      // No small vowel follows, convert ヴ→ブ or ゔ→ぶ
      codepoint = (codepoint == kKatakanaVu) ? kKatakanaBu : kHiraganaBu;
    }

    encodeUtf8(codepoint, result);
  }

  return result;
}

bool Normalizer::needsNormalization(std::string_view text) const {
  size_t pos = 0;
  while (pos < text.size()) {
    char32_t codepoint = decodeUtf8(text, pos);
    if (normalizeChar(codepoint) != codepoint) {
      return true;
    }
    // Check for vu-series characters
    if (codepoint == kKatakanaVu || codepoint == kHiraganaVu) {
      return true;
    }
    // Check for half-width dakuten/handakuten
    if (codepoint == kHalfwidthDakuten || codepoint == kHalfwidthHandakuten) {
      return true;
    }
  }
  return false;
}

}  // namespace suzume::normalize
