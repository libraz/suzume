/**
 * @file verb_candidates.h
 * @brief Verb-based unknown word candidate generation
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_H_

#include <vector>

#include "analysis/candidate_options.h"
#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

struct UnknownCandidate;

/**
 * @brief Generate compound verb candidates (e.g., 恐れ入ります, 差し上げます)
 *
 * Detects patterns like Kanji+Hiragana+Kanji+Hiragana and checks
 * if the base form exists in dictionary.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary manager for base form verification
 * @param candidates Buffer the generated candidates are appended to
 */
void generateCompoundVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const std::vector<normalize::CharType>& char_types,
                                    const grammar::Inflection& inflection,
                                    const dictionary::DictionaryManager* dict_manager,
                                    const VerbCandidateOptions& verb_opts, std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate verb candidates (kanji + conjugation endings)
 *
 * Detects patterns like 食べる, 書いた, 飲んでいる where kanji stem
 * is followed by hiragana conjugation endings.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary manager for suffix checking
 * @param candidates Output candidates, appended in generation order
 */
void generateVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                            const std::vector<normalize::CharType>& char_types, const grammar::Inflection& inflection,
                            const dictionary::DictionaryManager* dict_manager, const VerbCandidateOptions& verb_opts,
                            std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate hiragana verb candidates (pure hiragana verbs like いく, くる)
 *
 * Detects pure hiragana verbs and their conjugated forms
 * like いって, きた, できなくて.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param dict_manager Dictionary manager for base form verification
 * @return Vector of candidates
 */
std::vector<UnknownCandidate> generateHiraganaVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                             const std::vector<normalize::CharType>& char_types,
                                                             const grammar::Inflection& inflection,
                                                             const dictionary::DictionaryManager* dict_manager,
                                                             const VerbCandidateOptions& verb_opts = {});

/**
 * @brief Generate the colloquial contraction of the hypothetical form
 *
 * The conjunctive particle ば fuses into the preceding e-row mora of the
 * hypothetical, which shifts to the i-row and takes a palatal ゃ (行けば →
 * 行きゃ, 食べれば → 食べりゃ, すれば → すりゃ, 早ければ → 早けりゃ). The whole
 * contraction is one token, so it cannot be recovered by the paths that key on
 * a following ば. Script-independent: the fused mora is always kana while the
 * stem before it is not.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer, run on the reconstructed form
 * @param dict_manager Dictionary manager for base form verification
 * @param candidates Buffer the generated candidates are appended to
 */
void generateContractedConditionalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const grammar::Inflection& inflection,
                                             const dictionary::DictionaryManager* dict_manager,
                                             std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate the volitional う contracted to a geminate before か
 *
 * 行こ+っ+か and 帰っ+とこ+っ+か. The geminate is the auxiliary's own cell, but
 * a one-mora っ is the onbin everywhere else, so it is generated here, between
 * an o-row cell and the question particle, instead of being registered where
 * every dictionary probe for an auxiliary would also find it.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param candidates Buffer the generated candidate is appended to
 */
void generateContractedVolitionalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                            std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate the quotative って+いう contracted to っ+つー
 *
 * そうだ+っ+つー+の, やる+っ+つっ+たら: the quotative particle shrinks to its
 * geminate and いう takes the つ onset. Both pieces are one-mora or homographic
 * with ordinary kana, so they are generated only inside that sequence.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param candidates Buffer the generated candidates are appended to
 */
void generateContractedQuotativeCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           std::vector<UnknownCandidate>& candidates);

/**
 * @brief Check whether a span spells the colloquial hypothetical contraction
 *
 * The reconstruction and its acceptance test are the ones
 * generateContractedConditionalCandidates uses, so a rescue path that asks
 * whether a run already has a predicate reading agrees with the candidate that
 * would otherwise compete with it (やりゃ against a bracketed hiragana noun).
 * The contracted surface itself does not analyze as a conjugation, so the plain
 * inflection analyzer cannot answer this.
 */
bool spellsContractedHypothetical(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                  const grammar::Inflection& inflection,
                                  const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Generate katakana verb candidates (e.g., バズる, サボる, ググる)
 *
 * Detects patterns like Katakana + る/った/って/れる/らない etc.
 * where the katakana stem is followed by hiragana conjugation endings.
 * This handles slang/internet verbs that use katakana stems.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer for conjugation detection
 * @param candidates Buffer the generated candidates are appended to
 */
void generateKatakanaVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const std::vector<normalize::CharType>& char_types,
                                    const grammar::Inflection& inflection,
                                    const dictionary::DictionaryManager* dict_manager,
                                    const VerbCandidateOptions& verb_opts, std::vector<UnknownCandidate>& candidates);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_H_
