#ifndef SUZUME_DICTIONARY_DICTIONARY_LOOKUP_H_
#define SUZUME_DICTIONARY_DICTIONARY_LOOKUP_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"
#include "dictionary/double_array.h"

namespace suzume::dictionary {

// Both embedded and binary dictionaries keep same-surface entries contiguous
// and point their DoubleArray trie at the first entry. Keep that decoding
// invariant in one owner so the two storage frontends cannot drift.
std::vector<LookupResult> lookupByTrie(const DoubleArray& trie, const std::vector<DictionaryEntry>& entries,
                                       std::string_view text, size_t start_pos);

// Builds the trie over the first entry of each run of equal surfaces. Entries
// must already be sorted by surface; surface_at(idx) returns entry idx's
// surface. Pairs with lookupByTrie, which expects first-occurrence values.
template <typename SurfaceAt>
bool buildFirstOccurrenceTrie(DoubleArray& trie, size_t entry_count, const SurfaceAt& surface_at) {
  std::vector<std::string> keys;
  std::vector<uint32_t> values;
  keys.reserve(entry_count);
  values.reserve(entry_count);
  for (size_t idx = 0; idx < entry_count; ++idx) {
    if (idx == 0 || surface_at(idx) != surface_at(idx - 1)) {
      keys.push_back(surface_at(idx));
      values.push_back(static_cast<uint32_t>(idx));
    }
  }
  return trie.build(keys, values);
}

// Bounds-checked entry access shared by every dictionary frontend.
const DictionaryEntry* entryAt(const std::vector<DictionaryEntry>& entries, uint32_t idx);

const DictionaryEntry* lookupExactByTrie(const DoubleArray& trie, const std::vector<DictionaryEntry>& entries,
                                         std::string_view surface, core::PartOfSpeech pos);

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_DICTIONARY_LOOKUP_H_
