#ifndef SUZUME_POSTPROCESS_LEMMATIZER_H_
#define SUZUME_POSTPROCESS_LEMMATIZER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/morpheme.h"
#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"

namespace suzume::postprocess {

/**
 * @brief Lemmatizer for converting inflected forms to base forms
 *
 * Uses grammar-based inflection analysis with optional dictionary verification.
 */
class Lemmatizer {
 public:
  /**
   * @brief Construct a lemmatizer without dictionary verification
   */
  Lemmatizer();

  /**
   * @brief Construct a lemmatizer with dictionary verification
   * @param dict_manager Dictionary manager for verifying candidate base forms
   */
  explicit Lemmatizer(const dictionary::DictionaryManager* dict_manager);

  ~Lemmatizer() = default;

  /**
   * @brief Lemmatize a single morpheme
   * @param morpheme Morpheme to lemmatize
   * @return Lemmatized surface form
   */
  std::string lemmatize(const core::Morpheme& morpheme) const;

  /**
   * @brief Lemmatize all morphemes in a vector
   * @param morphemes Vector of morphemes
   */
  void lemmatizeAll(std::vector<core::Morpheme>& morphemes) const;

  /**
   * @brief Detect conjugation form from surface and lemma
   * @param surface Surface form (conjugated)
   * @param lemma Base form
   * @param pos Part of speech
   * @param next_lemma Optional: lemma of the next morpheme (for context-dependent forms)
   * @return Detected conjugation form
   */
  static grammar::ConjForm detectConjForm(std::string_view surface,
                                          std::string_view lemma,
                                          core::PartOfSpeech pos,
                                          std::string_view next_lemma = "");

 private:
  grammar::Inflection inflection_;
  const dictionary::DictionaryManager* dict_manager_{nullptr};

  /**
   * @brief Lemmatize using grammar-based inflection analysis
   *
   * If dictionary is available, verifies candidates against it.
   * @param surface The surface form to lemmatize
   * @param pos Optional POS to filter candidates (Adjective filters to IAdjective only)
   * @param conj_type Optional conjugation type hint to prefer matching candidates
   */
  std::string lemmatizeByGrammar(std::string_view surface,
                                  core::PartOfSpeech pos = core::PartOfSpeech::Other,
                                  dictionary::ConjugationType conj_type = dictionary::ConjugationType::None) const;

  /**
   * @brief Verify candidate base form against dictionary
   * @param candidate Inflection candidate to verify
   * @return true if candidate matches dictionary entry with correct conj_type
   */
  bool verifyCandidateWithDictionary(
      const grammar::InflectionCandidate& candidate) const;

  /**
   * @brief Check if string ends with suffix
   */
  static bool endsWith(std::string_view str, std::string_view suffix);

  /**
   * @brief Lemmatize verb (fallback)
   */
  static std::string lemmatizeVerb(std::string_view surface);

  /**
   * @brief Lemmatize adjective (fallback)
   */
  static std::string lemmatizeAdjective(std::string_view surface);
};

}  // namespace suzume::postprocess

#endif  // SUZUME_POSTPROCESS_LEMMATIZER_H_
