#ifndef SUZUME_NORMALIZE_UNICODE_TABLES_H_
#define SUZUME_NORMALIZE_UNICODE_TABLES_H_

#include <cstdint>

namespace suzume::normalize {

/**
 * @brief Unicode normalization tables
 *
 * This file contains lookup tables for Unicode normalization.
 * Tables are kept minimal to reduce binary size for WASM.
 */

// Combining dakuten (゛) codepoint
constexpr char32_t kCombiningDakuten = 0x3099;

// Combining handakuten (゜) codepoint
constexpr char32_t kCombiningHandakuten = 0x309A;

// Voiced sound mark (standalone)
constexpr char32_t kDakuten = 0x309B;

// Semi-voiced sound mark (standalone)
constexpr char32_t kHandakuten = 0x309C;

/**
 * @brief Check if codepoint can take dakuten
 */
constexpr bool canTakeDakuten(char32_t cp) {
  // Hiragana: か-と, は-ほ, う
  // Katakana: カ-ト, ハ-ホ, ウ
  // Simplified check
  return (cp >= 0x304B && cp <= 0x3062) ||  // か-ぢ
         (cp >= 0x3064 && cp <= 0x3069) ||  // つ-ど
         (cp >= 0x306F && cp <= 0x307D) ||  // は-ぽ
         (cp >= 0x30AB && cp <= 0x30C2) ||  // カ-ヂ
         (cp >= 0x30C4 && cp <= 0x30C9) ||  // ツ-ド
         (cp >= 0x30CF && cp <= 0x30DD) ||  // ハ-ポ
         cp == 0x3046 || cp == 0x30A6;      // う, ウ
}

/**
 * @brief Check if codepoint can take handakuten
 */
constexpr bool canTakeHandakuten(char32_t cp) {
  // は-ほ, ハ-ホ
  return (cp >= 0x306F && cp <= 0x307D && (cp - 0x306F) % 3 == 0) ||
         (cp >= 0x30CF && cp <= 0x30DD && (cp - 0x30CF) % 3 == 0);
}

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_UNICODE_TABLES_H_
