#ifndef SUZUME_CORE_MORPHEME_H_
#define SUZUME_CORE_MORPHEME_H_

#include <cstddef>
#include <string>
#include <string_view>

#include "dictionary/dictionary.h"
#include "grammar/conjugation.h"
#include "types.h"

namespace suzume::core {

/**
 * @brief Morpheme information
 *
 * Holds morpheme information needed for tag generation
 */
struct Morpheme {
  std::string surface;                   // Surface string
  size_t start{0};                       // Start character index
  size_t end{0};                         // End character index
  PartOfSpeech pos{PartOfSpeech::Noun};  // Part of speech
  std::string lemma;                     // Lemma (for verbs/adjectives)
  std::string reading;                   // Reading in hiragana
  dictionary::ConjugationType conj_type{dictionary::ConjugationType::None};  // Conjugation type
  grammar::ConjForm conj_form{grammar::ConjForm::Base};  // Conjugation form

  // Aliases for compatibility
  size_t start_pos = 0;  // Alias for start
  size_t end_pos = 0;    // Alias for end
  bool is_from_dictionary = false;  // Dictionary match flag
  bool is_unknown = false;          // Unknown word flag

  // Auxiliary information
  struct Features {
    bool is_dictionary = false;   // Dictionary match flag
    bool is_user_dict = false;    // User dictionary match flag
    bool is_formal_noun = false;  // Formal noun flag
    bool is_low_info = false;     // Low information word flag
    float score = 0.0F;           // Score
  } features;

  /**
   * @brief Get surface string length (UTF-8 character count)
   */
  size_t length() const { return end - start; }

  /**
   * @brief Get lemma (surface if not set)
   */
  std::string_view getLemma() const { return lemma.empty() ? surface : lemma; }

  /**
   * @brief Sync alias fields after setting start/end
   */
  void syncPositions() {
    start_pos = start;
    end_pos = end;
    is_from_dictionary = features.is_dictionary;
  }
};

}  // namespace suzume::core

#endif  // SUZUME_CORE_MORPHEME_H_
