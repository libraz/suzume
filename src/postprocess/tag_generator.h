#ifndef SUZUME_POSTPROCESS_TAG_GENERATOR_H_
#define SUZUME_POSTPROCESS_TAG_GENERATOR_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "core/morpheme.h"
#include "core/types.h"
#include "postprocess/postprocessor.h"

namespace suzume::postprocess {

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
   * @return Vector of tag strings
   */
  std::vector<std::string> generate(
      const std::vector<core::Morpheme>& morphemes) const;

  /**
   * @brief Generate tags from text using analyzer
   * @param text Input text
   * @return Vector of tag strings
   */
  static std::vector<std::string> generateFromText(std::string_view text);

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
