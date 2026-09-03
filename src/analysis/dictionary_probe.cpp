/**
 * @file dictionary_probe.cpp
 * @brief Bounded dictionary lookups over a codepoint window
 */

#include "analysis/dictionary_probe.h"

#include <algorithm>

#include "analysis/tokenizer_utils.h"

namespace suzume::analysis {

const dictionary::DictionaryEntry* lookupEntryInRange(const dictionary::DictionaryManager& dict_manager,
                                                      const std::vector<char32_t>& codepoints, size_t start, size_t end,
                                                      core::PartOfSpeech pos) {
  return dict_manager.lookupExact(extractSubstring(codepoints, start, end), pos);
}

size_t acceptedDictionaryEntryLength(const dictionary::DictionaryManager* dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t start, size_t min_len,
                                     size_t max_len, core::PartOfSpeech pos, EntryAccept accept, bool longest_first) {
  if (dict_manager == nullptr || start >= codepoints.size() || min_len == 0) {
    return 0;
  }
  const size_t longest = std::min(max_len, codepoints.size() - start);
  if (longest < min_len) {
    return 0;
  }
  for (size_t step = 0; step <= longest - min_len; ++step) {
    const size_t len = longest_first ? longest - step : min_len + step;
    const auto* entry = lookupEntryInRange(*dict_manager, codepoints, start, start + len, pos);
    if (entry != nullptr && (accept == nullptr || accept(*entry))) {
      return len;
    }
  }
  return 0;
}

}  // namespace suzume::analysis
