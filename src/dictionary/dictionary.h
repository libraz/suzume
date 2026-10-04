#ifndef SUZUME_DICTIONARY_DICTIONARY_H_
#define SUZUME_DICTIONARY_DICTIONARY_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.h"
#include "core/types.h"

namespace suzume::dictionary {

/**
 * @brief Conjugation type for verbs and adjectives
 *
 * Used for correct lemmatization of conjugated forms.
 */
enum class ConjugationType : uint8_t {
  None = 0,           // No conjugation (nouns, etc.)
  Ichidan = 1,        // Ichidan verb (食べる, 見る)
  GodanKa = 2,        // Godan K-row (書く, 歩く)
  GodanGa = 3,        // Godan G-row (泳ぐ, 急ぐ)
  GodanSa = 4,        // Godan S-row (話す, 出す)
  GodanTa = 5,        // Godan T-row (持つ, 待つ)
  GodanNa = 6,        // Godan N-row (死ぬ)
  GodanBa = 7,        // Godan B-row (遊ぶ, 飛ぶ)
  GodanMa = 8,        // Godan M-row (読む, 住む)
  GodanRa = 9,        // Godan R-row (取る, 走る)
  GodanWa = 10,       // Godan W-row (買う, 会う)
  Suru = 11,          // Suru verb (勉強する)
  Kuru = 12,          // Kuru verb (来る)
  IAdjective = 13,    // I-adjective (美しい, 高い)
  NaAdjective = 14,   // Na-adjective (静かだ)
  Interjection = 15,  // 感動詞 (何だ, ああ, おい)
  ProperFamily = 16,  // 固有名詞(姓): 優木, 田中
  ProperGiven = 17,   // 固有名詞(名): せつ菜, 太郎
};

/**
 * @brief Canonical serialized form of a conjugation type.
 *
 * Returns the short SCREAMING_SNAKE spelling used in TSV/CLI output
 * ("ICHIDAN", "GODAN_KA", ..., "I_ADJ", "NA_ADJ", "INTJ", "FAMILY", "GIVEN");
 * None serializes to an empty string.
 */
std::string_view conjTypeToCanonicalString(ConjugationType type);

/**
 * @brief Parse a conjugation type from its canonical spelling.
 *
 * Accepts the empty string and "NONE" (→ None) plus every short canonical form
 * produced by conjTypeToCanonicalString (including "INTJ"/"FAMILY"/"GIVEN").
 * Returns std::nullopt for anything else. Callers that must reject a subset
 * (e.g. proper-name or interjection markers) filter the result themselves.
 */
std::optional<ConjugationType> conjTypeFromCanonical(std::string_view str);

/**
 * @brief Parse a conjugation type from any long-form alias.
 *
 * Accepts the PascalCase enum names ("Ichidan", "GodanKa", "Interjection",
 * "ProperFamily", ...) and their long SCREAMING_SNAKE equivalents ("ICHIDAN",
 * "GODAN_KA", "INTERJECTION", "PROPER_FAMILY", ..., plus "I_ADJ"/"NA_ADJ").
 * Does NOT accept the short "INTJ"/"FAMILY"/"GIVEN" canonical forms. Returns
 * std::nullopt for anything else.
 */
std::optional<ConjugationType> conjTypeFromAnyAlias(std::string_view str);

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
  std::string surface;                                         // Surface string
  core::PartOfSpeech pos;                                      // Part of speech
  core::ExtendedPOS extended_pos{core::ExtendedPOS::Unknown};  // Extended POS
  std::string lemma;                                           // Lemma (optional)
};

/**
 * @brief Lookup result
 */
struct LookupResult {
  uint32_t entry_id;
  size_t length;  // Match length in characters
  const DictionaryEntry* entry;
  bool from_user_dict = false;  // True if from a user dictionary (Layer 3 or 4)
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
   * @brief Add a source user dictionary to the additive user layer
   * @param dict User dictionary to add
   */
  void addUserDictionary(std::shared_ptr<UserDictionary> dict);

  /**
   * @brief Remove every source and binary user dictionary
   *
   * Core dictionaries and the automatically loaded bundled user dictionary
   * are retained.
   */
  void clearUserDictionaries();

  /**
   * @brief Lookup entries from all dictionaries
   * @param text Text to search
   * @param start_pos Start position in bytes
   * @return Combined lookup results from core and user dictionaries
   */
  std::vector<LookupResult> lookup(std::string_view text, size_t start_pos) const;

  /**
   * @brief Lookup entries into reusable caller-owned storage.
   *
   * Clears @p out before appending matches from every dictionary layer. Hot
   * lattice builders can retain the vector's capacity across text positions.
   */
  void lookupInto(std::string_view text, size_t start_pos, std::vector<LookupResult>& out) const;

  /**
   * @brief Look up an exact-surface entry, optionally constrained by POS
   *
   * Checks each dictionary layer in lookup priority order and returns the first
   * entry whose surface equals @p surface exactly, so a shorter dictionary
   * prefix does not spuriously match. When @p pos is PartOfSpeech::Unknown the
   * first exact-surface entry of any POS is returned; otherwise only an entry
   * with the matching POS is returned.
   *
   * @return the matching entry, or nullptr if none exists
   */
  const DictionaryEntry* lookupExact(std::string_view surface,
                                     core::PartOfSpeech pos = core::PartOfSpeech::Unknown) const;

  /**
   * @brief Load core binary dictionary from file
   * @param path File path
   * @return true if loaded successfully
   */
  bool loadCoreDictionary(const std::string& path);

  /**
   * @brief Load core binary dictionary from file with error details
   */
  core::Expected<size_t, core::Error> loadCoreDictionaryResult(const std::string& path);

  /**
   * @brief Load core binary dictionary from memory
   */
  core::Expected<size_t, core::Error> loadCoreDictionaryFromMemoryResult(const uint8_t* data, size_t size);

  /**
   * @brief Check if core binary dictionary is loaded
   */
  bool hasCoreBinaryDictionary() const;

  /**
   * @brief Load user binary dictionary from memory
   * @param data Binary dictionary data (.dic format)
   * @param size Data size in bytes
   * @return true if loaded successfully
   */
  bool loadUserBinaryDictionaryFromMemory(const uint8_t* data, size_t size);

  /**
   * @brief Load user binary dictionary from memory with error details
   */
  core::Expected<size_t, core::Error> loadUserBinaryDictionaryFromMemoryResult(const uint8_t* data, size_t size);

  /**
   * @brief Load the automatically discovered bundled user dictionary from file
   *
   * Bundled dictionaries remain installed when clearUserDictionaries() removes
   * dictionaries explicitly added by the caller.
   */
  core::Expected<size_t, core::Error> loadBundledUserBinaryDictionaryResult(const std::string& path);

  /**
   * @brief Load an embedded bundled user dictionary from memory
   *
   * Bundled dictionaries remain installed when clearUserDictionaries() removes
   * dictionaries explicitly added by the caller.
   */
  core::Expected<size_t, core::Error> loadBundledUserBinaryDictionaryFromMemoryResult(const uint8_t* data, size_t size);

  /**
   * @brief Check if user binary dictionary is loaded
   */
  bool hasUserBinaryDictionary() const;

 private:
  BinaryDictionary& ensureCoreBinaryDict();
  core::Expected<size_t, core::Error> loadUserBinaryDictionaryResultInto(
      const std::string& path, std::vector<std::unique_ptr<BinaryDictionary>>& dictionaries);
  core::Expected<size_t, core::Error> loadUserBinaryDictionaryFromMemoryResultInto(
      const uint8_t* data, size_t size, std::vector<std::unique_ptr<BinaryDictionary>>& dictionaries);

  std::unique_ptr<CoreDictionary> core_dict_;
  std::unique_ptr<BinaryDictionary> core_binary_dict_;
  std::vector<std::unique_ptr<BinaryDictionary>> bundled_user_binary_dicts_;
  std::vector<std::unique_ptr<BinaryDictionary>> user_binary_dicts_;
  std::vector<std::shared_ptr<UserDictionary>> user_dicts_;
};

}  // namespace suzume::dictionary

#endif  // SUZUME_DICTIONARY_DICTIONARY_H_
