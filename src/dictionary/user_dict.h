#ifndef SUZUME_DICTIONARY_USER_DICT_H_
#define SUZUME_DICTIONARY_USER_DICT_H_

#include "core/error.h"
#include "dictionary/dictionary.h"
#include "dictionary/trie.h"

#include <memory>
#include <string>
#include <vector>

namespace suzume::dictionary {

/**
 * @brief User dictionary loaded at runtime
 *
 * Supports loading from file (native) or memory (WASM).
 * Format: surface,pos,cost,lemma (CSV)
 */
class UserDictionary : public IDictionary {
 public:
  UserDictionary();
  ~UserDictionary() override = default;

  // Non-copyable, non-movable (inherits from IDictionary)
  UserDictionary(const UserDictionary&) = delete;
  UserDictionary& operator=(const UserDictionary&) = delete;
  UserDictionary(UserDictionary&&) = delete;
  UserDictionary& operator=(UserDictionary&&) = delete;

  /**
   * @brief Load dictionary from file (native)
   * @param path File path
   * @return Number of entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadFromFile(const std::string& path);

  /**
   * @brief Load dictionary from memory (WASM)
   * @param data Pointer to CSV data
   * @param size Data size
   * @return Number of entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadFromMemory(const char* data,
                                                     size_t size);

  /**
   * @brief Add a single entry
   * @param entry Entry to add
   * @note Not thread-safe. Do not call during concurrent reads.
   */
  void addEntry(const DictionaryEntry& entry);

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

  /**
   * @brief Clear all entries
   */
  void clear();

 private:
  std::vector<DictionaryEntry> entries_;
  Trie trie_;

  /**
   * @brief Parse CSV data and add entries
   * @param csv_data CSV data string
   * @return Number of entries parsed
   */
  core::Expected<size_t, core::Error> parseCSV(std::string_view csv_data);

  /**
   * @brief Rebuild trie from entries
   */
  void rebuildTrie();
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_USER_DICT_H_
