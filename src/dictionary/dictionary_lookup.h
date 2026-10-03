#ifndef SUZUME_DICTIONARY_DICTIONARY_LOOKUP_H_
#define SUZUME_DICTIONARY_DICTIONARY_LOOKUP_H_

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

// Bounds-checked entry access shared by every dictionary frontend.
const DictionaryEntry* entryAt(const std::vector<DictionaryEntry>& entries, uint32_t idx);

const DictionaryEntry* lookupExactByTrie(const DoubleArray& trie, const std::vector<DictionaryEntry>& entries,
                                         std::string_view surface, core::PartOfSpeech pos);

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_DICTIONARY_LOOKUP_H_
