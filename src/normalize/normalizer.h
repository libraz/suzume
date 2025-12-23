#ifndef SUZUME_NORMALIZE_NORMALIZER_H_
#define SUZUME_NORMALIZE_NORMALIZER_H_

#include <string>
#include <string_view>

#include "core/error.h"

namespace suzume::normalize {

/**
 * @brief Text normalizer for Japanese text
 *
 * Performs:
 * - Full-width to half-width conversion (alphanumeric)
 * - Half-width to full-width katakana conversion
 * - Combining dakuten normalization
 * - Case normalization (lowercase)
 */
class Normalizer {
 public:
  Normalizer() = default;
  ~Normalizer() = default;

  // Copyable and movable (stateless)
  Normalizer(const Normalizer&) = default;
  Normalizer& operator=(const Normalizer&) = default;
  Normalizer(Normalizer&&) = default;
  Normalizer& operator=(Normalizer&&) = default;

  /**
   * @brief Normalize text
   * @param text Input text
   * @return Normalized text or error
   */
  core::Result<std::string> normalize(std::string_view text) const;

  /**
   * @brief Normalize a single codepoint
   * @param codepoint Unicode codepoint
   * @return Normalized codepoint
   */
  static char32_t normalizeChar(char32_t codepoint);

  /**
   * @brief Check if text needs normalization
   * @param text Input text
   * @return true if normalization would change the text
   */
  bool needsNormalization(std::string_view text) const;
};

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_NORMALIZER_H_
