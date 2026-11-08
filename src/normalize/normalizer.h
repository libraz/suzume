#ifndef SUZUME_NORMALIZE_NORMALIZER_H_
#define SUZUME_NORMALIZE_NORMALIZER_H_

#include <string>
#include <string_view>

#include "core/error.h"

namespace suzume::normalize {

/**
 * @brief Normalization options
 */
struct NormalizeOptions {
  // Preserve ヴ (vu) characters instead of converting to バビブベボ
  bool preserve_vu = true;

  // Preserve case (don't convert to lowercase)
  bool preserve_case = true;

  // Collapse repeated long-vowel marks only at a kanji boundary. This is a
  // lossy cleanup for noisy text; disable it when normalized_text must retain
  // every input codepoint.
  bool collapse_repeated_prolonged_marks = true;
};

/**
 * @brief Text normalizer for Japanese text
 *
 * Performs:
 * - Full-width to half-width conversion (alphanumeric)
 * - Half-width to full-width katakana conversion
 * - Combining dakuten normalization
 * - Case normalization (lowercase) - controllable via options
 * - Vu-series normalization (ヴ→ブ) - controllable via options
 * - Repeated long-vowel cleanup before kanji - controllable via options
 */
class Normalizer {
 public:
  Normalizer() = default;
  explicit Normalizer(const NormalizeOptions& options) : options_(options) {}
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
  core::Expected<std::string, core::Error> normalize(std::string_view text) const;

  /**
   * @brief Get current options
   */
  const NormalizeOptions& options() const { return options_; }

  /**
   * @brief Set options
   */
  void setOptions(const NormalizeOptions& options) { options_ = options; }

 private:
  NormalizeOptions options_;
};

}  // namespace suzume::normalize

#endif  // SUZUME_NORMALIZE_NORMALIZER_H_
