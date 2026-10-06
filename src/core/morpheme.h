#ifndef SUZUME_CORE_MORPHEME_H_
#define SUZUME_CORE_MORPHEME_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dictionary/dictionary.h"
#include "edge_flags.h"
#include "error.h"
#include "grammar/conjugation.h"
#include "types.h"

namespace suzume::core {

/**
 * @brief Morpheme information
 *
 * Holds morpheme information needed for tag generation
 */
struct Morpheme {
  std::string surface;                                                       // Surface string
  size_t start{0};                                                           // Start character index
  size_t end{0};                                                             // End character index
  PartOfSpeech pos{PartOfSpeech::Noun};                                      // Part of speech
  ExtendedPOS extended_pos{ExtendedPOS::Unknown};                            // Extended (fine-grained) POS
  std::string lemma;                                                         // Lemma (for verbs/adjectives)
  dictionary::ConjugationType conj_type{dictionary::ConjugationType::None};  // Conjugation type
  grammar::ConjForm conj_form{grammar::ConjForm::Base};                      // Conjugation form
  EdgeFlags flags{EdgeFlags::None};                                          // Candidate provenance and annotations
  CandidateOrigin origin{CandidateOrigin::Unknown};                          // Candidate generator
  float score{0.0F};                                                         // Candidate score/cost

  /**
   * @brief Get surface string length (UTF-8 character count)
   */
  size_t length() const { return end - start; }

  /**
   * @brief Get lemma (surface if not set)
   */
  std::string_view getLemma() const { return lemma.empty() ? surface : lemma; }

  bool fromDictionary() const { return hasFlag(flags, EdgeFlags::FromDictionary); }
  bool fromUserDict() const { return hasFlag(flags, EdgeFlags::FromUserDict); }
  bool isFormalNoun() const { return hasFlag(flags, EdgeFlags::IsFormalNoun); }
  bool isUnknown() const { return hasFlag(flags, EdgeFlags::IsUnknown); }
  bool isLowInformation() const { return core::isLowInformation(extended_pos); }
};

/**
 * @brief Analysis output in the coordinate space used by morpheme offsets.
 *
 * Normalization can change both spelling and character count. Keeping the
 * normalized text beside the morphemes makes their start/end offsets usable
 * without pretending that they address the original input.
 */
struct AnalysisOutput {
  std::string normalized_text;
  std::vector<Morpheme> morphemes;
};

/// The morphemes of an analysis result, or its error.
inline Expected<std::vector<Morpheme>, Error> takeMorphemes(Expected<AnalysisOutput, Error>&& analyzed) {
  if (!analyzed.hasValue()) {
    return makeUnexpected(analyzed.error());
  }
  return std::move(analyzed).value().morphemes;
}

}  // namespace suzume::core

#endif  // SUZUME_CORE_MORPHEME_H_
