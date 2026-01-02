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
  Prefix,      // 接頭辞
  Suffix,      // 接尾辞
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
 * @brief Origin of candidate generation (for debug)
 *
 * Tracks which generator produced a candidate for debugging purposes.
 */
enum class CandidateOrigin : uint8_t {
  Unknown = 0,
  Dictionary,          // 辞書からの直接候補
  VerbKanji,           // 漢字+ひらがな動詞 (食べる)
  VerbHiragana,        // ひらがな動詞 (いく, できる)
  VerbKatakana,        // カタカナ動詞 (バズる)
  VerbCompound,        // 複合動詞 (恐れ入る)
  AdjectiveI,          // イ形容詞 (美しい)
  AdjectiveIHiragana,  // ひらがなイ形容詞 (まずい)
  AdjectiveNa,         // ナ形容詞・的形容詞 (理性的)
  NominalizedNoun,     // 連用形転成名詞 (手助け)
  SuffixPattern,       // 接尾辞パターン (〜化, 〜性)
  SameType,            // 同一文字種 (漢字列, カタカナ列)
  Alphanumeric,        // 英数字
  Onomatopoeia,        // オノマトペ (わくわく)
  CharacterSpeech,     // キャラ語尾 (ナリ, ござる)
  Split,               // 分割候補 (NOUN+VERB)
  Join,                // 結合候補 (複合動詞結合)
  KanjiHiraganaCompound,  // 漢字+ひらがな複合名詞 (玉ねぎ)
  Counter,             // 数量詞パターン (一つ〜九つ)
  PrefixCompound,      // 接頭的複合語 (今日, 本日, 全国)
};

/**
 * @brief Convert CandidateOrigin to string for debug output
 */
const char* originToString(CandidateOrigin origin);

/**
 * @brief Convert part of speech to string (English)
 */
std::string_view posToString(PartOfSpeech pos);

/**
 * @brief Convert part of speech to Japanese string
 */
std::string_view posToJapanese(PartOfSpeech pos);

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
