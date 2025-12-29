#ifndef SUZUME_CORE_UTF8_CONSTANTS_H_
#define SUZUME_CORE_UTF8_CONSTANTS_H_

#include <cstddef>

namespace suzume::core {

// =============================================================================
// UTF-8 Byte Length Constants
// =============================================================================
// Japanese characters (hiragana, katakana, kanji) are encoded as 3 bytes in UTF-8.
// These constants make byte-level string operations self-documenting.
//
// UTF-8 encoding ranges:
// - U+3040-309F (Hiragana): 3 bytes each
// - U+30A0-30FF (Katakana): 3 bytes each
// - U+4E00-9FFF (CJK Unified Ideographs): 3 bytes each
// =============================================================================

/// Number of bytes for a single Japanese character in UTF-8
/// Applies to hiragana, katakana, and kanji
constexpr size_t kJapaneseCharBytes = 3;

/// Convenience aliases for specific character types (all equal to kJapaneseCharBytes)
constexpr size_t kHiraganaBytes = kJapaneseCharBytes;
constexpr size_t kKatakanaBytes = kJapaneseCharBytes;
constexpr size_t kKanjiBytes = kJapaneseCharBytes;

// =============================================================================
// Common Multi-Character Lengths
// =============================================================================

/// Length of two Japanese characters in bytes (e.g., "そう", "ない", "たい")
constexpr size_t kTwoJapaneseCharBytes = kJapaneseCharBytes * 2;  // 6

/// Length of three Japanese characters in bytes
constexpr size_t kThreeJapaneseCharBytes = kJapaneseCharBytes * 3;  // 9

// =============================================================================
// Helper Macros for Common Suffix Checks
// =============================================================================
// These are provided for readability in common patterns like:
//   if (surface.size() >= kTwoJapaneseCharBytes &&
//       surface.substr(surface.size() - kTwoJapaneseCharBytes) == "そう")

}  // namespace suzume::core

#endif  // SUZUME_CORE_UTF8_CONSTANTS_H_
