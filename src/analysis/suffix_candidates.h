/**
 * @file suffix_candidates.h
 * @brief Suffix-based unknown word candidate generation
 */

#ifndef SUZUME_ANALYSIS_SUFFIX_CANDIDATES_H_
#define SUZUME_ANALYSIS_SUFFIX_CANDIDATES_H_

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "normalize/char_type.h"

namespace suzume::dictionary {
class DictionaryManager;
}

namespace suzume::grammar {
class Inflection;
}

namespace suzume::analysis {

struct UnknownCandidate;
struct UnknownOptions;

/**
 * @brief Suffix entry for kanji compounds
 */
struct SuffixEntry {
  std::string_view suffix;
  // Derivational suffixes create a lexical compound noun (新制度, 安全性).
  // Relational suffixes such as 末 retain their compositional boundary.
  bool forms_derived_compound;
};

class SuffixEntryRange {
 public:
  constexpr SuffixEntryRange(const SuffixEntry* data, size_t size) : data_(data), size_(size) {}

  constexpr const SuffixEntry* begin() const { return data_; }
  constexpr const SuffixEntry* end() const { return data_ + size_; }

 private:
  const SuffixEntry* data_;
  size_t size_;
};

/**
 * @brief Get list of kanji compound suffixes
 */
SuffixEntryRange getSuffixEntries();

/**
 * @brief Get list of na-adjective forming suffixes (的, etc.)
 */
const std::array<std::string_view, 1>& getNaAdjSuffixes();

/**
 * Return the end of an identical repeated numeral+one-kanji unit, or zero.
 *
 * The numeral may contain multiple codepoints (十一件十一件), while the unit
 * must be a non-numeral kanji. This excludes pure numeral repetition such as
 * 十一十一.
 */
size_t repeatedNumeralNounUnitEndAt(const std::vector<char32_t>& codepoints,
                                    const std::vector<normalize::CharType>& char_types, size_t start_pos);

/** Return true when the unit at start_pos is either half of a repeated unit before a kanji+する predicate. */
bool isRepeatedNumeralNounPredicateUnitAt(const std::vector<char32_t>& codepoints,
                                          const std::vector<normalize::CharType>& char_types, size_t start_pos);

/**
 * @brief Generate candidates with suffix separation
 *
 * Detects kanji compounds that end with common suffixes (化, 性, 者, etc.)
 * and generates both the full compound and the stem as candidates.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param options Unknown word generation options
 * @param candidates Buffer the generated candidates are appended to
 */
void generateWithSuffix(const std::vector<char32_t>& codepoints, size_t start_pos,
                        const std::vector<normalize::CharType>& char_types, const UnknownOptions& options,
                        std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate nominalized noun candidates
 *
 * Detects nominalized verb stems (連用形転成名詞) like:
 *   - 手助け (from 手助ける)
 *   - 片付け (from 片付ける)
 *   - 引き上げ (from 引き上げる)
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer used to preserve the longest verified
 *        continuative boundary
 * @param dict_manager Dictionary manager used to reject spans already owned by
 *        exact nouns or suffix decompositions (may be null)
 * @param candidates Output candidates, appended in generation order
 */
void generateNominalizedNounCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                       const std::vector<normalize::CharType>& char_types,
                                       const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate reciprocal-action deverbal noun candidates (連用形 + っこ)
 *
 * The nominalizer っこ turns a continuative into the name of a reciprocal action
 * (にらめっこ, かけっこ, ぶつけっこ) and the result is one search unit. A span this
 * long written entirely in hiragana is otherwise priced by the generic unknown
 * model, which fragments it, so the whole construction needs its own candidate.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param dict_manager Dictionary manager used to verify the verb stem
 * @param candidates Output candidates, appended in generation order
 */
void generateReciprocalActionNounCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                            const std::vector<normalize::CharType>& char_types,
                                            const dictionary::DictionaryManager* dict_manager,
                                            std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate the deverbal nominal of the humble 敬語接頭辞 + V連用形 + する frame
 *
 * The honorific prefix and following する form delimit a closed frame in which
 * the continuative is a deverbal noun rather than a finite predicate.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param inflection Inflection analyzer used to verify the continuative
 * @param dict_manager Reserved dictionary context (currently unused; may be null)
 * @param candidates Output candidates, appended in generation order
 */
void generateHumbleNominalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate kanji + hiragana compound noun candidates
 *
 * Detects compound nouns with kanji prefix and hiragana suffix:
 *   - 玉ねぎ (tamanegi - onion)
 *   - 水たまり (mizutamari - puddle)
 *   - 雨だれ (amadare - raindrop)
 *
 * Distinguished from verb conjugations by requiring longer hiragana
 * portions that don't match typical conjugation patterns.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @return Vector of candidates
 */
void generateKanjiHiraganaCompoundCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const dictionary::DictionaryManager* dict_manager,
                                             std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate a short noun head selected by a verified left modifier and
 * closed by a nominal particle on the right.
 *
 * Left evidence is either genitive の or a complete determiner/i-adjective.
 * The generated span contains the head only; it never absorbs the selector.
 */
void generateSelectedNominalHeadCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           const std::vector<normalize::CharType>& char_types,
                                           const grammar::Inflection& inflection,
                                           const dictionary::DictionaryManager* dict_manager,
                                           std::vector<UnknownCandidate>& candidates);

/**
 * @brief Return true when a span is exactly two registered auxiliaries (ぬ+べし)
 *
 * A chain of closed-class forms spells no open-class word, so a predicate
 * fabricated over one is an artefact of the paradigm tables rather than a
 * lexical reading.
 */
bool hasAuxiliaryChainDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                    const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Return true when a span is exactly a closed auxiliary+particle chain
 *
 * Three morae is the floor, for the reason its auxiliary+auxiliary sibling above
 * states: two morae are spelled by too many one-mora closed-class forms to be
 * evidence of anything, and an ordinary noun splits that way by accident
 * (く+も, ひ+も, か+も).
 */
bool hasAuxiliaryParticleDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                       const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Return true when a span is a focus particle or pronoun followed by more
 *        function words (ほど+で, ばかり+に, なに+が).
 *
 * A nominal promotion of such a span is not an unknown-word rescue: every piece
 * is already a dictionary entry. The head has to be a focus particle or a
 * pronoun and span two or more characters, because a case particle or a single
 * mora is just as often the opening of a lexical word whose remainder happens to
 * be a function word too (から+だ, けん+か) — those runs are what the rescue path
 * is for.
 */
bool hasFunctionWordChainDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                       const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Generate productive suffix candidates for hiragana sequences
 *
 * Detects productive suffix patterns attached to verb stems:
 *   - V連用形 + がち (tendency): ありがち、なりがち
 *   - V連用形 + っぽい (resemblance): 忘れっぽい、怒りっぽい
 *
 * These patterns allow recognition without explicit dictionary entries.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param candidates Output candidates, appended in generation order
 */
void generateProductiveSuffixCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                        const std::vector<normalize::CharType>& char_types,
                                        std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate productive suffix-verb candidates from nominal bases.
 *
 * Detects the Godan-ka suffix verb めく and its inflections:
 *   - 春めく、謎めいた、色めいて
 */
void generateProductiveSuffixVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                            const std::vector<normalize::CharType>& char_types,
                                            std::vector<UnknownCandidate>& candidates);

/**
 * @brief True when @p okurigana spells a cell of the Godan-ma suffix verb ばむ.
 *
 * That paradigm is the one productive verbalizing suffix whose cells collide
 * with a ma-row irrealis plus a classical auxiliary (黄ばむ against 呼ばむ), so
 * a kanji verb generator has to recognize the homography before it proposes an
 * irrealis boundary. Answering from the paradigm's own table keeps the two
 * sites from drifting apart.
 */
bool spellsGodanMaSuffixVerbCell(std::string_view okurigana);

/**
 * @brief Generate counter candidates for numeral + つ patterns
 *
 * Detects closed-class counter patterns:
 *   - 一つ (hitotsu), 二つ (futatsu), ..., 九つ (kokonotsu)
 *
 * This is a closed class of 9 patterns, recognized as grammatical pattern
 * rather than dictionary entries.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param candidates Output candidates, appended in generation order
 */
void generateCounterCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                               const std::vector<normalize::CharType>& char_types,
                               const dictionary::DictionaryManager* dict_manager,
                               std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate prefix + single kanji compound candidates
 *
 * Detects temporal/prefix compounds:
 *   - 今日 (kyou - today), 今週 (konshuu - this week)
 *   - 来月 (raigetsu - next month), 翌日 (yokujitsu - following day)
 *
 * The generated compound competes with split analysis.
 * Interrogatives (何, 誰, etc.) act as anchors to prevent over-concatenation.
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param inflection Inflection analyzer, used to suppress the compound when the
 *        second kanji heads a verb continuing into the following hiragana
 *        (今食べてる → 今|食べ|てる, not 今食|べてる)
 * @param candidates Output candidates, appended in generation order
 */
void generatePrefixCompoundCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                      const std::vector<normalize::CharType>& char_types,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates);

/**
 * @brief Generate a temporal-noun boundary split candidate
 *
 * Detects an adverbial temporal noun (現在, 昨日) heading a longer kanji run and
 * discounts the split point so it beats the unknown-word kanji-run merge:
 *   - 現在担当者 (genzai tantousha) -> 現在 | 担当者
 *   - 昨日会議 (kinou kaigi) -> 昨日 | 会議
 *
 * @param codepoints Text as codepoints
 * @param start_pos Start position (character index)
 * @param char_types Character types for each position
 * @param candidates Output candidates, appended in generation order
 */
void generateTemporalNounBoundaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                            const std::vector<normalize::CharType>& char_types,
                                            std::vector<UnknownCandidate>& candidates);

/**
 * @brief Check if a codepoint is a prefix-like kanji
 *
 * Returns true for kanji that productively head temporal compounds:
 * 今, 来, 先, 昨, 翌, 毎
 *
 * @param cp Unicode codepoint to check
 * @return true if prefix-like kanji
 */
bool isPrefixLikeKanji(char32_t cp);

/**
 * @brief Check if a codepoint is an interrogative kanji
 *
 * Returns true for kanji that are interrogative words:
 * 何, 誰, 幾
 * These should not form verb compounds (e.g., 何してる should split as 何|し|てる)
 *
 * @param cp Unicode codepoint to check
 * @return true if interrogative kanji
 */
bool isInterrogativeKanji(char32_t cp);

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SUFFIX_CANDIDATES_H_
