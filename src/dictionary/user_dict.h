#ifndef SUZUME_DICTIONARY_USER_DICT_H_
#define SUZUME_DICTIONARY_USER_DICT_H_

#include <string_view>
#include <vector>

#include "dictionary/dictionary.h"
#include "dictionary/trie.h"

namespace suzume::dictionary {

/**
 * @brief User dictionary loaded at runtime
 *
 * Entries are installed one at a time through addEntry().
 */
class UserDictionary {
 public:
  UserDictionary();
  ~UserDictionary() = default;

  // Non-copyable, non-movable
  UserDictionary(const UserDictionary&) = delete;
  UserDictionary& operator=(const UserDictionary&) = delete;
  UserDictionary(UserDictionary&&) = delete;
  UserDictionary& operator=(UserDictionary&&) = delete;

  /**
   * @brief Add a single entry
   * @param entry Entry to add
   * @note Not thread-safe. Do not call during concurrent reads.
   */
  bool addEntry(const DictionaryEntry& entry);

  /**
   * @brief Lookup entries at position
   * @param text Text to search
   * @param start_pos Start position in bytes
   * @return Vector of lookup results
   */
  std::vector<LookupResult> lookup(std::string_view text, size_t start_pos) const;

  /**
   * @brief Look up the first exact-surface entry, optionally constrained by POS
   */
  const DictionaryEntry* lookupExact(std::string_view surface,
                                     core::PartOfSpeech pos = core::PartOfSpeech::Unknown) const;

 private:
  std::vector<DictionaryEntry> entries_;
  Trie trie_;
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_USER_DICT_H_
