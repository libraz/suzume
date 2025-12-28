#ifndef SUZUME_POSTPROCESS_POSTPROCESSOR_H_
#define SUZUME_POSTPROCESS_POSTPROCESSOR_H_

#include <vector>

#include "core/morpheme.h"
#include "core/types.h"
#include "dictionary/dictionary.h"
#include "postprocess/lemmatizer.h"

namespace suzume::postprocess {

/**
 * @brief Post-processing options
 */
struct PostprocessOptions {
  bool merge_noun_compounds = false;  // Merge consecutive nouns
  bool lemmatize = true;             // Apply lemmatization
  bool remove_symbols = true;        // Remove symbol-only morphemes
  size_t min_surface_length = 1;     // Minimum surface length to keep
};

/**
 * @brief Post-processor for morpheme sequences
 */
class Postprocessor {
 public:
  explicit Postprocessor(const PostprocessOptions& options = {});

  /**
   * @brief Construct with dictionary for lemmatization verification
   * @param dict_manager Dictionary manager for verifying lemma candidates
   * @param options Post-processing options
   */
  Postprocessor(const dictionary::DictionaryManager* dict_manager,
                const PostprocessOptions& options = {});

  ~Postprocessor() = default;

  /**
   * @brief Process morpheme sequence
   * @param morphemes Input morphemes
   * @return Processed morphemes
   */
  std::vector<core::Morpheme> process(
      const std::vector<core::Morpheme>& morphemes) const;

 private:
  PostprocessOptions options_;
  Lemmatizer lemmatizer_;

  /**
   * @brief Merge consecutive nouns
   */
  static std::vector<core::Morpheme> mergeNounCompounds(const std::vector<core::Morpheme>& morphemes);

  /**
   * @brief Merge NOUN/PRONOUN + SUFFIX into compound noun
   */
  static std::vector<core::Morpheme> mergeNounSuffix(const std::vector<core::Morpheme>& morphemes);

  /**
   * @brief Merge consecutive numeric expressions (e.g., 3億 + 5000万円 → 3億5000万円)
   */
  static std::vector<core::Morpheme> mergeNumericExpressions(const std::vector<core::Morpheme>& morphemes);

  /**
   * @brief Merge na-adjective + な into attributive form (e.g., 静か + な → 静かな)
   */
  static std::vector<core::Morpheme> mergeNaAdjectiveNa(const std::vector<core::Morpheme>& morphemes);

  /**
   * @brief Remove unwanted morphemes
   */
  std::vector<core::Morpheme> filterMorphemes(
      const std::vector<core::Morpheme>& morphemes) const;
};

}  // namespace suzume::postprocess

#endif  // SUZUME_POSTPROCESS_POSTPROCESSOR_H_
