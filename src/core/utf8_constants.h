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

/// Length of four Japanese characters in bytes
constexpr size_t kFourJapaneseCharBytes = kJapaneseCharBytes * 4;  // 12

/// Length of five Japanese characters in bytes (e.g., "させられる", "させられた")
constexpr size_t kFiveJapaneseCharBytes = kJapaneseCharBytes * 5;  // 15

}  // namespace suzume::core

// =============================================================================
// UTF-8 String Utility Functions
// =============================================================================
// Zero-overhead inline functions for common Japanese string operations.
// These replace verbose patterns like:
//   surface.substr(surface.size() - kTwoJapaneseCharBytes) == "そう"
// With more readable:
//   utf8::endsWith(surface, "そう")

#include <string_view>

namespace utf8 {

using suzume::core::kJapaneseCharBytes;
using suzume::core::kTwoJapaneseCharBytes;
using suzume::core::kThreeJapaneseCharBytes;

/// Check if string ends with the given suffix
/// @param s The string to check
/// @param suffix The suffix to look for
/// @return true if s ends with suffix
[[nodiscard]] inline constexpr bool endsWith(std::string_view s,
                                              std::string_view suffix) noexcept {
  return s.size() >= suffix.size() &&
         s.substr(s.size() - suffix.size()) == suffix;
}

/// Get the last N bytes of a string as a string_view
/// @param s The source string
/// @param n Number of bytes to get
/// @return The last N bytes, or empty if s.size() < n
[[nodiscard]] inline constexpr std::string_view lastNBytes(
    std::string_view s, size_t n) noexcept {
  return s.size() >= n ? s.substr(s.size() - n) : std::string_view{};
}

/// Get the first N bytes of a string as a string_view
/// @param s The source string
/// @param n Number of bytes to get
/// @return The first N bytes, or entire string if s.size() < n
[[nodiscard]] inline constexpr std::string_view firstNBytes(
    std::string_view s, size_t n) noexcept {
  return s.substr(0, n);
}

/// Get string without the last N bytes
/// @param s The source string
/// @param n Number of bytes to drop from end
/// @return String without last N bytes, or empty if s.size() < n
[[nodiscard]] inline constexpr std::string_view dropLast(std::string_view s,
                                                          size_t n) noexcept {
  return s.size() >= n ? s.substr(0, s.size() - n) : std::string_view{};
}

/// Get string without the first N bytes
/// @param s The source string
/// @param n Number of bytes to drop from start
/// @return String without first N bytes, or empty if s.size() < n
[[nodiscard]] inline constexpr std::string_view dropFirst(std::string_view s,
                                                           size_t n) noexcept {
  return s.size() >= n ? s.substr(n) : std::string_view{};
}

// Convenience aliases for common Japanese character operations
// These use byte counts, not character counts

/// Get the last Japanese character (3 bytes)
[[nodiscard]] inline constexpr std::string_view lastChar(
    std::string_view s) noexcept {
  return lastNBytes(s, kJapaneseCharBytes);
}

/// Get the last 2 Japanese characters (6 bytes)
[[nodiscard]] inline constexpr std::string_view last2Chars(
    std::string_view s) noexcept {
  return lastNBytes(s, kTwoJapaneseCharBytes);
}

/// Get the last 3 Japanese characters (9 bytes)
[[nodiscard]] inline constexpr std::string_view last3Chars(
    std::string_view s) noexcept {
  return lastNBytes(s, kThreeJapaneseCharBytes);
}

/// Drop the last Japanese character (3 bytes)
[[nodiscard]] inline constexpr std::string_view dropLastChar(
    std::string_view s) noexcept {
  return dropLast(s, kJapaneseCharBytes);
}

/// Drop the last 2 Japanese characters (6 bytes)
[[nodiscard]] inline constexpr std::string_view dropLast2Chars(
    std::string_view s) noexcept {
  return dropLast(s, kTwoJapaneseCharBytes);
}

// =============================================================================
// UTF-8 Decoding Utilities for Japanese Characters
// =============================================================================
// These functions decode 3-byte UTF-8 sequences (Japanese characters).
// They replace the common pattern:
//   const unsigned char* ptr = reinterpret_cast<const unsigned char*>(s.data() + pos);
//   if ((ptr[0] & 0xF0) != 0xE0) { return false; }
//   char32_t cp = ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);

/// Check if byte at position starts a 3-byte UTF-8 sequence
/// @param s The string to check
/// @param pos Byte position
/// @return true if position starts a 3-byte sequence (Japanese character)
[[nodiscard]] inline bool is3ByteUtf8At(std::string_view s, size_t pos) noexcept {
  if (pos + kJapaneseCharBytes > s.size()) return false;
  auto byte = static_cast<unsigned char>(s[pos]);
  return (byte & 0xF0) == 0xE0;
}

/// Decode 3-byte UTF-8 at position (assumes valid 3-byte sequence)
/// @param s The string to decode from
/// @param pos Byte position (must be valid 3-byte start)
/// @return Unicode codepoint
[[nodiscard]] inline char32_t decode3ByteUtf8At(std::string_view s, size_t pos) noexcept {
  const auto* ptr = reinterpret_cast<const unsigned char*>(s.data() + pos);
  return static_cast<char32_t>(
      ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F));
}

/// Decode last Japanese character as codepoint
/// @param s The string (must have at least 3 bytes)
/// @return Unicode codepoint, or 0 if invalid
[[nodiscard]] inline char32_t decodeLastChar(std::string_view s) noexcept {
  if (s.size() < kJapaneseCharBytes) return 0;
  size_t pos = s.size() - kJapaneseCharBytes;
  if (!is3ByteUtf8At(s, pos)) return 0;
  return decode3ByteUtf8At(s, pos);
}

/// Decode first Japanese character as codepoint
/// @param s The string (must have at least 3 bytes)
/// @return Unicode codepoint, or 0 if invalid
[[nodiscard]] inline char32_t decodeFirstChar(std::string_view s) noexcept {
  if (!is3ByteUtf8At(s, 0)) return 0;
  return decode3ByteUtf8At(s, 0);
}

}  // namespace utf8

#endif  // SUZUME_CORE_UTF8_CONSTANTS_H_
