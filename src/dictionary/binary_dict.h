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
  uint32_t magic;         // "SZMD" (0x444D5A53)
  uint32_t entry_count;   // Number of entries
  uint32_t surface_size;  // Size of the front-coded surface table
  uint8_t version;        // Format version
  uint8_t flags;          // Entry encoding
  uint16_t reserved;      // Must be zero

  static constexpr uint32_t kMagic = 0x444D5A53;  // "SZMD"
  // v4 allows consecutive entries to share one surface so grammatical
  // homographs retain every POS/ExtendedPOS alternative.
  static constexpr uint8_t kVersion = 4;
  static constexpr uint8_t kEntryEncodingMask = 0x03;
  static constexpr uint8_t kRecordPaletteEntries = 0x00;  // 1-byte indexes into packed records
  static constexpr uint8_t kGrammarOnlyEntries = 0x01;    // 1 byte/entry, no differing lemmas
  static constexpr uint8_t kPackedEntries = 0x02;         // 2 bytes/entry, 11-bit lemma + 5-bit grammar
  static constexpr uint8_t kWideEntries = 0x03;           // 3 bytes/entry, 16-bit lemma + 8-bit grammar
  static constexpr uint8_t kRelativeLemmaRefs = 0x04;     // Lemmas are signed deltas into the surface table
  static constexpr uint8_t kKnownFlags = kEntryEncodingMask | kRelativeLemmaRefs;
};
static_assert(sizeof(BinaryDictHeader) == 16);

/**
 * The format stores front-coded sorted UTF-8 surfaces, a POS/ExtendedPOS palette, an
 * adaptive entry array, and an optional length-prefixed, deduplicated lemma table.
 * A zero-length suffix repeats the immediately preceding surface for a grammatical
 * homograph; the runtime trie points at the first entry in each consecutive group.
 * Repeated packed records use a byte-indexed palette. When every lemma is also a
 * nearby surface, packed entries store a signed surface-index delta instead. The
 * runtime DoubleArray is rebuilt while loading.
 *
 * Surface and lemma bytes intentionally remain UTF-8: the trie and validation
 * paths consume UTF-8 directly, while a Japanese-specific legacy/custom encoding
 * would need another decoder and would complicate ASCII, symbols, and supplementary
 * characters. Likewise, do not deflate/Huffman-wrap the embedded dictionaries for
 * a gzip-distributed build without measuring the whole artifact; experiments cut
 * raw data but increased final gzip because of decoder code and double compression.
 */
inline constexpr size_t kWideCompactEntrySize = 3;

/**
 * @brief Binary dictionary (read-only, memory-mapped friendly)
 *
 * File format:
 *   [Header]
 *   [Front-coded Surface Table]
 *   [Entry Array]
 *   [String Pool]
 */
class BinaryDictionary {
 public:
  BinaryDictionary();
  ~BinaryDictionary();

  /**
   * @brief Load dictionary from file
   * @param path File path
   * @return Number of entries on success, error on failure
   * @note Native-only I/O. WASM builds return an error; use loadFromMemory().
   */
  core::Expected<size_t, core::Error> loadFromFile(const std::string& path);

  /**
   * @brief Load dictionary from memory (WASM compatible)
   * @param data Pointer to binary data
   * @param size Data size
   * @return Number of entries on success, error on failure
   */
  core::Expected<size_t, core::Error> loadFromMemory(const uint8_t* data, size_t size);

  /**
   * @brief Lookup entries at position
   */
  std::vector<LookupResult> lookup(std::string_view text, size_t start_pos) const;

  /**
   * @brief Look up an exact-surface entry, optionally constrained by POS
   */
  const DictionaryEntry* lookupExact(std::string_view surface,
                                     core::PartOfSpeech pos = core::PartOfSpeech::Unknown) const;

  /**
   * @brief Get entry by ID
   */
  const DictionaryEntry* getEntry(uint32_t idx) const;

  /**
   * @brief Get number of entries
   */
  size_t size() const { return entries_.size(); }

  /**
   * @brief Check if dictionary is loaded
   */
  bool isLoaded() const { return !entries_.empty(); }

 private:
  DoubleArray trie_;
  std::vector<DictionaryEntry> entries_;

  core::Expected<size_t, core::Error> parseData(const uint8_t* data, size_t size, DoubleArray& trie,
                                                std::vector<DictionaryEntry>& entries);
};

/**
 * @brief Binary dictionary writer (for compilation)
 */
class BinaryDictWriter {
 public:
  BinaryDictWriter();

  /**
   * @brief Add an entry
   * v0.8: conj_type parameter removed
   */
  void addEntry(const DictionaryEntry& entry);

  /**
   * @brief Build and write to file
   * @param path Output file path
   * @return Number of bytes written on success, error on failure
   * @note Native-only I/O. WASM builds return an error; use build().
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
  std::vector<DictionaryEntry> entries_;
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_BINARY_DICT_H_
