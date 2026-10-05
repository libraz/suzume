/**
 * @file adjective_candidates.h
 * @brief Adjective-based unknown word candidate generation
 */

#ifndef SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_H_
#define SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_H_

#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

struct UnknownCandidate;
struct UnknownOptions;

/**
 * @brief Generate i-adjective candidates (kanji + conjugation endings)
 *
 * Detects patterns like 寒い, 美しい, 面白かった where kanji stem
 * is followed by hiragana conjugation endings.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary manager for base form validation (optional)
 */
void generateAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                 const std::vector<normalize::CharType>& char_types,
                                 const grammar::Inflection& inflection,
                                 const dictionary::DictionaryManager* dict_manager,
                                 std::vector<UnknownCandidate>& candidates);

/**
 * @brief Extend a generated nominal host with the closed adjective suffix
 *        がまし〜.
 *
 * Mixed-script hosts such as 言い訳 and 差し出 are not visible to the ordinary
 * leading-kanji adjective scan. This pass runs after nominal candidates exist,
 * and therefore reuses the tokenizer's structural evidence for the whole host.
 */
void generateGaMashiiHostAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const grammar::Inflection& inflection,
                                             std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate na-adjective candidates (〜的 patterns)
 *
 * Detects kanji compounds ending with 的 (teki) which form
 * na-adjectives like 理性的, 論理的, 感情的.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param options Unknown word generation options
 */
void generateNaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                   const std::vector<normalize::CharType>& char_types, const UnknownOptions& options,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate hiragana i-adjective candidates (pure hiragana like まずい)
 *
 * Detects pure hiragana i-adjectives and their conjugated forms
 * like まずい, おいしい, まずかった.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary, used to reject particle + verb sequences
 *        misread as adjectives (にかかった → に + かかる, not にかい)
 */
void generateHiraganaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::vector<normalize::CharType>& char_types,
                                         const grammar::Inflection& inflection,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate katakana i-adjective candidates (e.g., エモい, キモい, ウザい)
 *
 * Detects patterns like Katakana + い/かった/くない etc.
 * where the katakana stem is followed by i-adjective conjugation endings.
 * This handles slang/internet adjectives that use katakana stems.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary that vetoes a coined stem running into a registered adjective
 */
void generateKatakanaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::vector<normalize::CharType>& char_types,
                                         const grammar::Inflection& inflection,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate i-adjective STEM candidates (e.g., 難し, 美し, 楽し)
 *
 * Detects i-adjective stems when followed by auxiliary patterns like そう, すぎる.
 * MeCab splits these as: 難しそう → 難し(ADJ) + そう(SUFFIX)
 * This function generates the stem (難し) as an ADJ candidate so the lattice
 * can prefer ADJ+AUX split over ADJ一体.
 *
 * Patterns detected:
 * - 〜しそう (難しそう → 難し + そう)
 * - 〜しすぎる (難しすぎる → 難し + すぎる)
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for stem validation
 * @param dict_manager Dictionary manager for verb lookup (to filter verb renyokei)
 */
void generateAdjectiveStemCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const std::vector<normalize::CharType>& char_types,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates);

/**
 * @brief Append i-adjective 未然形 conjectural candidates (stem + かろ + う)
 *
 * The presumptive form 高かろう / うれしかろう / よかろう is not produced by
 * inflection analysis, and the surface Xかろ is homographic with a verb
 * volitional stem (分かろう ← 分かる). A candidate is therefore emitted only when
 * the reconstructed base (stem + い) is a decisive i-adjective — a dictionary
 * adjective, or one the inflection analyzer recognizes with adjective-level
 * confidence — and a う follows. Shared by the kanji and pure-hiragana adjective
 * generators.
 *
 * @param scan_start First index where the trailing かろ may begin (kanji_end for
 *        a kanji stem, start_pos for a pure-hiragana stem); the stem before かろ
 *        must be non-empty.
 * @param scan_end   One past the last index to scan (hiragana region end).
 */
void appendIAdjKaroCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                              size_t scan_end, const grammar::Inflection& inflection,
                              const dictionary::DictionaryManager* dict_manager,
                              std::vector<UnknownCandidate>& candidates);

/**
 * @brief Append the polite continuative cell of an i-adjective (よろしゅう, 高う)
 *
 * The cell replaces the く of the renyokei with う; an i-row kana in front of it
 * grows the ゅ digraph that vowel change produces, while a kanji stem keeps its
 * spelling. Scanned the same way as the かろ cell above.
 */
void appendIAdjOnbinRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                       size_t scan_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates);

/**
 * @brief Append classical i-adjective terminal candidates (stem + し)
 *
 * The classical 終止形 closes a clause where the modern base would stand
 * (山高し, 見まほし). Both paradigms spell it with a final し, so the candidate
 * is emitted only when the reconstructed modern base is a dictionary adjective
 * and no su-row verb claims the same spelling.
 *
 * @param scan_start First index where the terminal し may sit (kanji_end for a
 *        kanji stem, start_pos for a pure-hiragana stem).
 * @param scan_end   One past the last index to scan (hiragana region end).
 */
void appendIAdjClassicalTerminalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                           size_t scan_end, const dictionary::DictionaryManager* dict_manager,
                                           std::vector<UnknownCandidate>& candidates);

/**
 * @brief Append i-adjective classical attributive candidates (stem + き)
 *
 * 美しき花 / よき日: emitted only when the reconstructed base (stem + い) is a
 * dictionary adjective, or a シク stem derived from a godan irrealis before a
 * nominal head, and no verb claims the same spelling.
 *
 * @param scan_start First index where き may sit (kanji_end for a kanji stem,
 *        start_pos + 1 for a pure-hiragana stem).
 * @param scan_end   One past the last index to scan (hiragana region end).
 */
void appendIAdjClassicalAttributiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                              size_t scan_start, size_t scan_end,
                                              const dictionary::DictionaryManager* dict_manager,
                                              std::vector<UnknownCandidate>& candidates);

/**
 * @brief Append i-adjective classical negative-stem candidates (stem + から + ず)
 *
 * The classical negative form 高からず / 美しからず has a verb-like から
 * surface.  A candidate is emitted only when the reconstructed modern base
 * (stem + い) is a decisive i-adjective and classical negative ず follows.
 */
void appendIAdjKaraZuCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                size_t scan_end, const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager,
                                std::vector<UnknownCandidate>& candidates);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_H_
