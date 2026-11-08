#ifndef SUZUME_NORMALIZE_UNICODE_TABLES_H_
#define SUZUME_NORMALIZE_UNICODE_TABLES_H_

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

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_UNICODE_TABLES_H_
