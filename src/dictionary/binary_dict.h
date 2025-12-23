#ifndef SUZUME_DICTIONARY_BINARY_DICT_H_
#define SUZUME_DICTIONARY_BINARY_DICT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.h"
#include "core/types.h"
#include "dictionary/dictionary.h"
#include "dictionary/double_array.h"

namespace suzume::dictionary {

// ConjugationType is now defined in dictionary.h

/**
 * @brief Binary dictionary header
 */
struct BinaryDictHeader {
  uint32_t magic;           // "SZMD" (0x444D5A53)
  uint16_t version_major;   // Major version
  uint16_t version_minor;   // Minor version
  uint32_t entry_count;     // Number of entries
  uint32_t trie_offset;     // Offset to trie data
  uint32_t trie_size;       // Size of trie data
  uint32_t entry_offset;    // Offset to entry array
  uint32_t string_offset;   // Offset to string pool
  uint32_t flags;           // Reserved flags
  uint32_t checksum;        // CRC32 checksum (reserved)

  static constexpr uint32_t kMagic = 0x444D5A53;  // "SZMD"
  static constexpr uint16_t kVersionMajor = 1;
  static constexpr uint16_t kVersionMinor = 0;
};

/**
 * @brief Binary dictionary entry record (fixed size)
 */
struct BinaryDictEntry {
  uint32_t surface_offset;  // Surface offset in string pool
  uint16_t surface_length;  // Surface length
  uint8_t pos;              // Part of speech
  uint8_t conj_type;        // Conjugation type
  uint32_t lemma_offset;    // Lemma offset (0 = same as surface)
  uint16_t lemma_length;    // Lemma length
  int16_t cost;             // Cost (×100, integer)
  uint8_t flags;            // Flags (is_formal_noun, is_low_info, etc.)
  uint8_t reserved[3];      // Reserved for alignment
};

/**
 * @brief Binary dictionary (read-only, memory-mapped friendly)
 *
 * File format:
 *   [Header]
 *   [Double-Array Trie]
 *   [Entry Array]
 *   [String Pool]
 */
class BinaryDictionary : public IDictionary {
 public:
  BinaryDictionary();
  ~BinaryDictionary() override;

  /**
   * @brief Load dictionary from file
   * @param path File path
   * @return Number of entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadFromFile(const std::string& path);

  /**
   * @brief Load dictionary from memory (WASM compatible)
   * @param data Pointer to binary data
   * @param size Data size
   * @return Number of entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadFromMemory(const uint8_t* data,
                                                      size_t size);

  /**
   * @brief Lookup entries at position
   */
  std::vector<LookupResult> lookup(std::string_view text,
                                   size_t start_pos) const override;

  /**
   * @brief Get entry by ID
   */
  const DictionaryEntry* getEntry(uint32_t idx) const override;

  /**
   * @brief Get number of entries
   */
  size_t size() const override { return entries_.size(); }

  /**
   * @brief Check if dictionary is loaded
   */
  bool isLoaded() const { return !entries_.empty(); }

 private:
  DoubleArray trie_;
  std::vector<DictionaryEntry> entries_;
  std::vector<uint8_t> data_;  // Owned copy of binary data

  core::Expected<size_t, core::Error> parseData();
};

/**
 * @brief Binary dictionary writer (for compilation)
 */
class BinaryDictWriter {
 public:
  BinaryDictWriter();

  /**
   * @brief Add an entry
   */
  void addEntry(const DictionaryEntry& entry,
                ConjugationType conj_type = ConjugationType::None);

  /**
   * @brief Build and write to file
   * @param path Output file path
   * @return Number of bytes written on success, error on failure
   */
  core::Expected<size_t, core::Error> writeToFile(const std::string& path);

  /**
   * @brief Build and get binary data
   * @return Binary data
   */
  core::Expected<std::vector<uint8_t>, core::Error> build();

  /**
   * @brief Get number of entries
   */
  size_t size() const { return entries_.size(); }

 private:
  struct EntryData {
    DictionaryEntry entry;
    ConjugationType conj_type;
  };

  std::vector<EntryData> entries_;
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_BINARY_DICT_H_
