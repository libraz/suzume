#ifndef SUZUME_ANALYSIS_SUFFIX_CANDIDATES_COMPOUND_INTERNAL_H_
#define SUZUME_ANALYSIS_SUFFIX_CANDIDATES_COMPOUND_INTERNAL_H_

#include <string>
#include <vector>

#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

/// Whether [start_pos, end_pos) is an adjective stem nominalized by a final さ.
inline bool isAdjectiveNominalizationSa(const dictionary::DictionaryManager* dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1 || codepoints[end_pos - 1] != U'さ') {
    return false;
  }
  const std::string stem = extractSubstring(codepoints, start_pos, end_pos - 1);
  if (normalize::utf8Length(stem) >= 2 && utf8::endsWith(stem, "し")) {
    return true;
  }
  return dict_manager->lookupExact(stem, core::PartOfSpeech::Adjective) != nullptr ||
         dict_manager->lookupExact(stem + "い", core::PartOfSpeech::Adjective) != nullptr;
}

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SUFFIX_CANDIDATES_COMPOUND_INTERNAL_H_
