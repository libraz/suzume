#ifndef SUZUME_DICTIONARY_CORE_DICT_H_
#define SUZUME_DICTIONARY_CORE_DICT_H_

#include "dictionary/dictionary.h"
#include "dictionary/trie.h"

#include <memory>
#include <vector>

namespace suzume::dictionary {

/**
 * @brief Core dictionary with hardcoded entries
 *
 * Contains particles, auxiliary verbs, conjunctions,
 * formal nouns, and low information words.
 * Compiled into the binary for WASM compatibility.
 */
class CoreDictionary : public IDictionary {
 public:
  CoreDictionary();
  ~CoreDictionary() override = default;

  /**
   * @brief Lookup entries at position
   * @param text Text to search
   * @param start_pos Start position (character index)
   * @return Vector of lookup results
   */
  std::vector<LookupResult> lookup(std::string_view text,
                                   size_t start_pos) const override;

  /**
   * @brief Get entry by ID
   * @param idx Entry ID
   * @return Entry pointer, or nullptr if not found
   */
  const DictionaryEntry* getEntry(uint32_t idx) const override;

  /**
   * @brief Get number of entries
   */
  size_t size() const override { return entries_.size(); }

 private:
  std::vector<DictionaryEntry> entries_;
  Trie trie_;

  /**
   * @brief Initialize entries from hardcoded data
   */
  void initializeEntries();
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_CORE_DICT_H_
