#ifndef SUZUME_CORE_TYPES_H_
#define SUZUME_CORE_TYPES_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace suzume::core {

/**
 * @brief Part of speech types (minimal set for tag generation)
 */
enum class PartOfSpeech : uint8_t {
  Unknown,     // 不明
  Noun,        // 名詞
  Verb,        // 動詞
  Adjective,   // 形容詞
  Adverb,      // 副詞
  Particle,    // 助詞
  Auxiliary,   // 助動詞
  Conjunction, // 接続詞
  Determiner,  // 連体詞
  Pronoun,     // 代名詞
  Symbol,      // 記号
  Other        // その他
};

/**
 * @brief Analysis mode
 */
enum class AnalysisMode : uint8_t {
  Normal,  // Normal mode
  Search,  // Search mode (keep noun compounds)
  Split    // Split mode (fine-grained segmentation)
};

/**
 * @brief Convert part of speech to string
 */
std::string_view posToString(PartOfSpeech pos);

/**
 * @brief Convert string to part of speech
 */
PartOfSpeech stringToPos(std::string_view str);

/**
 * @brief Check if POS is taggable (content word)
 */
bool isTaggable(PartOfSpeech pos);

/**
 * @brief Check if POS is content word
 */
bool isContentWord(PartOfSpeech pos);

/**
 * @brief Check if POS is function word
 */
bool isFunctionWord(PartOfSpeech pos);

}  // namespace suzume::core

#endif  // SUZUME_CORE_TYPES_H_
