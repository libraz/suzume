#include "dictionary/dictionary_lookup.h"

#include "normalize/utf8.h"

namespace suzume::dictionary {

std::vector<LookupResult> lookupByTrie(const DoubleArray& trie, const std::vector<DictionaryEntry>& entries,
                                       std::string_view text, size_t start_pos) {
  std::vector<LookupResult> results;
  if (entries.empty() || start_pos >= text.size()) {
    return results;
  }

  for (const auto& trie_result : trie.commonPrefixSearch(text, start_pos)) {
    if (trie_result.value < 0 || static_cast<size_t>(trie_result.value) >= entries.size()) {
      continue;
    }

    const size_t first_idx = static_cast<size_t>(trie_result.value);
    const std::string& matched_surface = entries[first_idx].surface;
    const size_t char_length = normalize::utf8Length(text.substr(start_pos, trie_result.length));
    for (size_t entry_idx = first_idx; entry_idx < entries.size() && entries[entry_idx].surface == matched_surface;
         ++entry_idx) {
      LookupResult result{};
      result.entry_id = static_cast<uint32_t>(entry_idx);
      result.length = char_length;
      result.entry = &entries[entry_idx];
      results.push_back(result);
    }
  }
  return results;
}

const DictionaryEntry* entryAt(const std::vector<DictionaryEntry>& entries, uint32_t idx) {
  return idx < entries.size() ? &entries[idx] : nullptr;
}

const DictionaryEntry* lookupExactByTrie(const DoubleArray& trie, const std::vector<DictionaryEntry>& entries,
                                         std::string_view surface, core::PartOfSpeech pos) {
  const int32_t first_idx = trie.exactMatch(surface);
  if (first_idx < 0 || static_cast<size_t>(first_idx) >= entries.size()) {
    return nullptr;
  }

  for (size_t entry_idx = static_cast<size_t>(first_idx);
       entry_idx < entries.size() && entries[entry_idx].surface == surface; ++entry_idx) {
    if (pos == core::PartOfSpeech::Unknown || entries[entry_idx].pos == pos) {
      return &entries[entry_idx];
    }
  }
  return nullptr;
}

}  // namespace suzume::dictionary
