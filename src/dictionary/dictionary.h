#ifndef SUZUME_DICTIONARY_DICTIONARY_H_
#define SUZUME_DICTIONARY_DICTIONARY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"

namespace suzume::dictionary {

/**
 * @brief Conjugation type for verbs and adjectives
 *
 * Used for correct lemmatization of conjugated forms.
 */
enum class ConjugationType : uint8_t {
  None = 0,         // No conjugation (nouns, etc.)
  Ichidan = 1,      // Ichidan verb (食べる, 見る)
  GodanKa = 2,      // Godan K-row (書く, 歩く)
  GodanGa = 3,      // Godan G-row (泳ぐ, 急ぐ)
  GodanSa = 4,      // Godan S-row (話す, 出す)
  GodanTa = 5,      // Godan T-row (持つ, 待つ)
  GodanNa = 6,      // Godan N-row (死ぬ)
  GodanBa = 7,      // Godan B-row (遊ぶ, 飛ぶ)
  GodanMa = 8,      // Godan M-row (読む, 住む)
  GodanRa = 9,      // Godan R-row (取る, 走る)
  GodanWa = 10,     // Godan W-row (買う, 会う)
  Suru = 11,        // Suru verb (勉強する)
  Kuru = 12,        // Kuru verb (来る)
  IAdjective = 13,  // I-adjective (美しい, 高い)
  NaAdjective = 14, // Na-adjective (静かだ)
  Interjection = 15, // 感動詞 (何だ, ああ, おい)
  ProperFamily = 16, // 固有名詞(姓): 優木, 田中
  ProperGiven = 17,  // 固有名詞(名): せつ菜, 太郎
};

/**
 * @brief Dictionary entry (simplified v0.8)
 *
 * Design principles:
 *   1. No per-entry cost - cost is derived from ExtendedPOS via getCategoryCost()
 *   2. No flags - use ExtendedPOS categories (e.g., NounFormal) instead
 *   3. No conjugation type - lemmatizer derives from surface/lemma
 *   4. No reading - not needed for core functionality
 */
struct DictionaryEntry {
  std::string surface;                // Surface string
  core::PartOfSpeech pos;             // Part of speech
  core::ExtendedPOS extended_pos{core::ExtendedPOS::Unknown};  // Extended POS
  std::string lemma;                  // Lemma (optional)
};

/**
 * @brief Lookup result
 */
struct LookupResult {
  uint32_t entry_id;
  size_t length;           // Match length in characters
  const DictionaryEntry* entry;
  bool from_user_dict = false;  // True if from user dictionary (Layer 4)
};

/**
 * @brief Dictionary interface
 */
class IDictionary {
 public:
  virtual ~IDictionary() = default;

  // Non-copyable, non-movable
  IDictionary(const IDictionary&) = delete;
  IDictionary& operator=(const IDictionary&) = delete;
  IDictionary(IDictionary&&) = delete;
  IDictionary& operator=(IDictionary&&) = delete;

 protected:
  IDictionary() = default;

  /**
   * @brief Lookup entries at position
   * @param text Text to search
   * @param start_pos Start position (character index)
   * @return Vector of lookup results
   */
  virtual std::vector<LookupResult> lookup(std::string_view text, size_t start_pos) const = 0;

  /**
   * @brief Get entry by ID
   * @param entry_id Entry ID
   * @return Entry pointer, or nullptr if not found
   */
  virtual const DictionaryEntry* getEntry(uint32_t entry_id) const = 0;

  /**
   * @brief Get number of entries
   */
  virtual size_t size() const = 0;
};

// Forward declarations
class CoreDictionary;
class UserDictionary;
class BinaryDictionary;

/**
 * @brief Dictionary manager that combines core and user dictionaries
 */
class DictionaryManager {
 public:
  DictionaryManager();
  ~DictionaryManager();

  // Non-copyable, movable
  DictionaryManager(const DictionaryManager&) = delete;
  DictionaryManager& operator=(const DictionaryManager&) = delete;
  DictionaryManager(DictionaryManager&&) noexcept;
  DictionaryManager& operator=(DictionaryManager&&) noexcept;

  /**
   * @brief Add a user dictionary
   * @param dict User dictionary to add
   */
  void addUserDictionary(std::shared_ptr<UserDictionary> dict);

  /**
   * @brief Lookup entries from all dictionaries
   * @param text Text to search
   * @param start_pos Start position (character index)
   * @return Combined lookup results from core and user dictionaries
   */
  std::vector<LookupResult> lookup(std::string_view text,
                                   size_t start_pos) const;

  /**
   * @brief Get the core dictionary
   */
  const CoreDictionary& coreDictionary() const;

  /**
   * @brief Load core binary dictionary from file
   * @param path File path
   * @return true if loaded successfully
   */
  bool loadCoreDictionary(const std::string& path);

  /**
   * @brief Check if core binary dictionary is loaded
   */
  bool hasCoreBinaryDictionary() const;

  /**
   * @brief Load user binary dictionary from file
   * @param path File path
   * @return true if loaded successfully
   */
  bool loadUserBinaryDictionary(const std::string& path);

  /**
   * @brief Load user binary dictionary from memory
   * @param data Binary dictionary data (.dic format)
   * @param size Data size in bytes
   * @return true if loaded successfully
   */
  bool loadUserBinaryDictionaryFromMemory(const uint8_t* data, size_t size);

  /**
   * @brief Check if user binary dictionary is loaded
   */
  bool hasUserBinaryDictionary() const;

  /**
   * @brief Try to auto-load core.dic from standard paths
   * @return true if loaded successfully
   *
   * Search order:
   * 1. $SUZUME_DATA_DIR/core.dic
   * 2. ./data/core.dic
   * 3. ~/.suzume/core.dic
   * 4. /usr/local/share/suzume/core.dic
   * 5. /usr/share/suzume/core.dic
   */
  bool tryAutoLoadCoreDictionary();

 private:
  std::unique_ptr<CoreDictionary> core_dict_;
  std::unique_ptr<BinaryDictionary> core_binary_dict_;
  std::unique_ptr<BinaryDictionary> user_binary_dict_;
  std::vector<std::shared_ptr<UserDictionary>> user_dicts_;
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_DICTIONARY_H_
