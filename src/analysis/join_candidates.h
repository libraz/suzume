/**
 * @file join_candidates.h
 * @brief Join-based candidate generation for tokenizer
 *
 * Functions for generating join candidates during tokenization:
 * - Compound verb joining (e.g., 飛び込む = 飛ぶ + 込む)
 * - Prefix+noun joining (e.g., お水 = お + 水)
 */

#ifndef SUZUME_ANALYSIS_JOIN_CANDIDATES_H_
#define SUZUME_ANALYSIS_JOIN_CANDIDATES_H_

#include <string_view>
#include <vector>

#include "analysis/scorer.h"
#include "analysis/tokenizer_utils.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

/**
 * @brief Add compound verb join candidates
 *
 * Detects V1連用形 + V2 patterns and generates compound verb candidates.
 * V1 = base verb in continuative form (連用形)
 * V2 = subsidiary verb (出す, 込む, 始める, etc.)
 *
 * Examples:
 *   "飛び込む" → compound verb (飛ぶ + 込む)
 *   "読み込む" → compound verb (読む + 込む)
 *   "書き出す" → compound verb (書く + 出す)
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 * @param scorer Scorer for POS priors
 */
void addCompoundVerbJoinCandidates(core::Lattice& lattice, std::string_view text,
                                   const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                   size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                   const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                   const grammar::Inflection& inflection);

/**
 * @brief Add hiragana compound verb join candidates
 *
 * Detects all-hiragana V1連用形 + V2 patterns where V1 is a known dictionary verb.
 * This handles compound verbs written entirely in hiragana like やりなおす.
 *
 * Examples:
 *   "やりなおす" → compound verb (やる + なおす)
 *   "やりなおしたい" → やりなおし + たい
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 * @param scorer Scorer for POS priors
 */
void addHiraganaCompoundVerbJoinCandidates(core::Lattice& lattice, std::string_view text,
                                           const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                           size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                           const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                           const grammar::Inflection& inflection);

/**
 * @brief Add prefix + noun join candidates
 *
 * Detects productive prefix + noun patterns and generates merged candidates.
 * Prefixes include: お/ご (honorific), 不/未/非/無 (negation), 超/再/準, etc.
 *
 * Examples:
 *   "お水" → merged as single noun (お + 水)
 *   "ご確認" → merged as single noun (ご + 確認)
 *   "不安" → merged as single noun (不 + 安)
 *   "未経験" → merged as single noun (未 + 経験)
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 * @param scorer Scorer for POS priors
 */
void addPrefixNounJoinCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                                 const ByteOffsets& byte_offsets, size_t start_pos,
                                 const std::vector<normalize::CharType>& char_types,
                                 const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                 const grammar::Inflection& inflection);

/**
 * @brief Add pronoun + plural-suffix join candidates
 *
 * Combines a dictionary-backed pronoun with the closed plural suffix ら into
 * one pronoun search unit, e.g. これら and 彼ら.
 */
void addPronounPluralJoinCandidates(core::Lattice& lattice, std::string_view text,
                                    const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                    size_t start_pos, const dictionary::DictionaryManager& dict_manager,
                                    const Scorer& scorer);

/**
 * @brief Add noun + destination 行き join candidates
 *
 * Combines a noun host directly followed by the bound destination use of 行き
 * into one noun search unit, e.g. 学校行き and 東京行き. A following verbal
 * continuation such as ます prevents the join, so 東京へ行きます and colloquial
 * 学校行きます retain the independent verb reading.
 */
void addDestinationSuffixNounJoinCandidates(core::Lattice& lattice, std::string_view text,
                                            const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                            size_t start_pos, const dictionary::DictionaryManager& dict_manager,
                                            const Scorer& scorer);

/**
 * @brief Add a deverbal-noun reading before independent adjective なく
 *
 * A verb continuative can productively function as a noun in the adverbial
 * absence construction (休み + なく). A particle or auxiliary immediately
 * after なく instead proves the ordinary negative-auxiliary chain.
 */
void addDeverbalNounBeforeIndependentNakuCandidates(core::Lattice& lattice, std::string_view text,
                                                    const std::vector<char32_t>& codepoints,
                                                    const ByteOffsets& byte_offsets, size_t start_pos,
                                                    const dictionary::DictionaryManager& dict_manager,
                                                    const Scorer& scorer);

/**
 * @brief Add taru-adjective adverb join candidates
 *
 * Detects X然と patterns (taru-adjectives in adverbial form) and generates
 * single adverb candidates. These are words like 毅然と, 平然と, 悠然と that
 * should be treated as single adverbs rather than split as noun + particle.
 *
 * Examples:
 *   "毅然と" → single adverb (not 毅然 + と)
 *   "平然と" → single adverb (not 平然 + と)
 *   "悠然と" → single adverb (not 悠然 + と)
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param scorer Scorer for POS priors
 */
void addTaruAdjectiveJoinCandidates(core::Lattice& lattice, std::string_view text,
                                    const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                    size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                    const dictionary::DictionaryManager& dict_manager, const Scorer& scorer);

/**
 * @brief Add verb renyokei + suffix noun join candidates
 *
 * Detects V連用形 + suffix patterns and generates compound noun candidates.
 * Suffixes include: 物 (mono), 方 (kata/hou), 所 (tokoro), etc.
 *
 * Examples:
 *   "食べ物" → compound noun (食べ + 物)
 *   "飲み物" → compound noun (飲む + 物)
 *   "読み方" → compound noun (読む + 方)
 *
 * @param lattice Lattice to add candidates to
 * @param text Original text
 * @param codepoints Unicode codepoints
 * @param start_pos Starting position in codepoints
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager for lookups
 * @param scorer Scorer for POS priors
 */
void addVerbSuffixNounJoinCandidates(core::Lattice& lattice, std::string_view text,
                                     const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                     size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                     const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                     const grammar::Inflection& inflection);

/**
 * @brief Whether a listed subsidiary verb's continuative starts at @p pos
 *
 * The kana spelling of a compound's V2 behind a voice auxiliary, where no kanji
 * marks the word (書か+さ+れ+かけ+た). The subsidiary table is the evidence, so
 * the answer agrees with what the compound matcher would accept as a V2.
 */
bool subsidiaryVerbContinuativeAt(const std::vector<char32_t>& codepoints, size_t pos);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_JOIN_CANDIDATES_H_
