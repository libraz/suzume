#ifndef SUZUME_POSTPROCESS_TAG_GENERATOR_H_
#define SUZUME_POSTPROCESS_TAG_GENERATOR_H_

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/morpheme.h"
#include "core/types.h"
#include "postprocess/postprocessor.h"

namespace suzume::postprocess {

// POS filter bit constants for TagGeneratorOptions::pos_filter
static constexpr uint8_t kTagPosNoun = 1 << 0;      // NOLINT(readability-magic-numbers): bit flag
static constexpr uint8_t kTagPosVerb = 1 << 1;      // NOLINT(readability-magic-numbers): bit flag
static constexpr uint8_t kTagPosAdjective = 1 << 2;  // NOLINT(readability-magic-numbers): bit flag
static constexpr uint8_t kTagPosAdverb = 1 << 3;    // NOLINT(readability-magic-numbers): bit flag

/**
 * @brief Tag generation options
 */
struct TagGeneratorOptions {
  bool use_lemma = true;              // Use lemma instead of surface
  bool exclude_particles = true;      // Exclude particles
  bool exclude_auxiliaries = true;    // Exclude auxiliary verbs
  bool exclude_formal_nouns = true;   // Exclude formal nouns
  bool exclude_low_info = true;       // Exclude low info words
  bool remove_duplicates = true;      // Remove duplicate tags
  size_t min_tag_length = 2;          // Minimum tag length (characters)
  size_t max_tags = 0;                // Maximum number of tags (0 = unlimited)

  // POS filter (if any bit is set, only include matching POS)
  uint8_t pos_filter = 0;            // 0 = include all content words
  bool exclude_basic = false;        // Exclude basic words (hiragana-only lemma)
};

/**
 * @brief Tag entry with POS information
 */
struct TagEntry {
  std::string tag;
  core::PartOfSpeech pos;
};

/**
 * @brief Tag generator from morphemes
 */
class TagGenerator {
 public:
  explicit TagGenerator(const TagGeneratorOptions& options = {});
  ~TagGenerator() = default;

  /**
   * @brief Generate tags from morphemes
   * @param morphemes Input morphemes
   * @return Vector of tag entries with POS information
   */
  std::vector<TagEntry> generate(
      const std::vector<core::Morpheme>& morphemes) const;

  /**
   * @brief Generate tags from text using analyzer
   * @param text Input text
   * @return Vector of tag entries with POS information
   */
  static std::vector<TagEntry> generateFromText(std::string_view text);

 private:
  TagGeneratorOptions options_;
  Postprocessor postprocessor_;

  /**
   * @brief Check if morpheme should be included as tag
   */
  bool shouldInclude(const core::Morpheme& morpheme) const;

  /**
   * @brief Get tag string from morpheme
   */
  std::string getTagString(const core::Morpheme& morpheme) const;

  /**
   * @brief Count UTF-8 characters
   */
  static size_t countChars(std::string_view str);
};

}  // namespace suzume::postprocess

#endif  // SUZUME_POSTPROCESS_TAG_GENERATOR_H_
