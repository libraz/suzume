/**
 * @file adjective_candidates_internal.h
 * @brief Shared UnknownCandidate factory helpers for the adjective candidate generators
 */

#ifndef SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_INTERNAL_H_
#define SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_INTERNAL_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "unknown.h"

namespace suzume::dictionary {
class DictionaryManager;
}

namespace suzume::analysis::adj_detail {

// Shared surface inventory for i-adjective stems before appearance/excessive
// auxiliaries. Hiragana generation uses the first kHiraganaIAdjStemAuxPatternCount
// entries; kanji generation uses the complete inventory.
extern const std::array<std::string_view, 14> kIAdjStemAuxPatterns;
constexpr size_t kHiraganaIAdjStemAuxPatternCount = 6;

/**
 * @brief Rule for preserving a boundary inside an analyzed adjective span.
 *
 * Rules stay path-local because kanji, hiragana, and katakana candidates use
 * different costs and lexical guards for otherwise identical suffix shapes.
 */
struct TrimmedAdjVariantRule {
  constexpr TrimmedAdjVariantRule(std::string_view suffix_value, size_t char_trim_value, float cost_bonus_value,
                                  core::ExtendedPOS epos_value, uint8_t group_value,
                                  [[maybe_unused]] const char* pattern_value,
                                  bool reject_contracted_n_past_value = false, bool require_nonempty_stem_value = false,
                                  bool prefer_dictionary_lemma_value = false)
      : suffix(suffix_value),
        char_trim(char_trim_value),
        cost_bonus(cost_bonus_value),
        epos(epos_value),
        group(group_value),
        reject_contracted_n_past(reject_contracted_n_past_value),
        require_nonempty_stem(require_nonempty_stem_value),
        prefer_dictionary_lemma(prefer_dictionary_lemma_value) {
#ifdef SUZUME_DEBUG_INFO
    pattern = pattern_value;
#endif
  }

  std::string_view suffix;
  size_t char_trim;
  float cost_bonus;
  core::ExtendedPOS epos;
  uint8_t group;
  bool reject_contracted_n_past = false;
  bool require_nonempty_stem = false;
  bool prefer_dictionary_lemma = false;
#ifdef SUZUME_DEBUG_INFO
  const char* pattern = nullptr;
#endif
};

/**
 * @brief First confidence at or above a threshold for one inflection type.
 */
float firstConfidenceAtLeast(const std::vector<grammar::InflectionCandidate>& candidates, grammar::VerbType type,
                             float minimum);

/**
 * @brief Highest confidence among a small set of inflection types.
 */
float maxConfidenceFor(const std::vector<grammar::InflectionCandidate>& candidates,
                       std::initializer_list<grammar::VerbType> types);

/** Whether any non-adjective analysis has a dictionary-verified verb lemma. */
bool hasDictionaryVerbAnalysis(const std::vector<grammar::InflectionCandidate>& candidates,
                               const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Whether the character after い makes it a Godan onbin surface.
 */
bool isVerbOnbinContextAfterI(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether a codepoint range contains a prolonged sound mark.
 */
bool containsProlongedSoundMark(const std::vector<char32_t>& codepoints, size_t start, size_t end);

bool appendKanjiIAdjSpecialCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                      size_t hiragana_end, const std::vector<normalize::CharType>& char_types,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates);

// =============================================================================
// UnknownCandidate Factory Helpers
// =============================================================================

/**
 * @brief Detect i-adjective EPOS based on surface ending
 */
UnknownCandidate makeIAdjCandidate(const std::string& surface, size_t start, size_t end, const std::string& lemma,
                                   float cost, CandidateOrigin origin, float confidence, const char* pattern);

/**
 * @brief Create a na-adjective candidate.
 */
UnknownCandidate makeNaAdjCandidate(const std::string& surface, size_t start, size_t end, float cost, bool has_suffix,
                                    CandidateOrigin origin, float confidence, const char* pattern);

/**
 * @brief Create an i-adjective stem candidate (expects suffix)
 */
UnknownCandidate makeIAdjStemCandidate(const std::string& surface, size_t start, size_t end, const std::string& lemma,
                                       float cost, CandidateOrigin origin, float confidence, const char* pattern);

/**
 * @brief Derive a conjugated i-adjective variant by trimming trailing kana off
 *        an existing candidate.
 *
 * Spins the renyokei/katt/ke conjugation forms out of a full adjective surface
 * to preserve inflection/auxiliary boundaries (良くない → 良く + ない,
 * 美しかった → 美しかっ + た).
 * The variant keeps the source lemma/origin/confidence, drops @p char_trim
 * trailing characters (all such tails are 3-byte kana), applies @p cost_bonus on
 * top of the source cost, and carries the connection-form @p epos. Callers keep
 * their own endsWith() guard and may override the cost afterward (e.g. the
 * ke-form dictionary-adjective disambiguation).
 */
UnknownCandidate makeTrimmedAdjVariant(const UnknownCandidate& cand, size_t char_trim, float cost_bonus,
                                       core::ExtendedPOS epos, const char* pattern);

/**
 * @brief Append table-driven connection-form variants without temporary vectors.
 *
 * Rule groups are evaluated outermost to retain the historical candidate ordering.
 * Only candidates in the [first_index, size) sub-range appended by the current
 * generator are inspected, so unrelated candidates left in a shared buffer by
 * earlier generators are ignored and derived variants are never recursively
 * reprocessed.
 */
void appendTrimmedAdjVariants(std::vector<UnknownCandidate>& candidates, const TrimmedAdjVariantRule* rules,
                              size_t rule_count, size_t first_index,
                              const dictionary::DictionaryManager* dict_manager = nullptr);

/**
 * @brief Append post-scan inflection variants for a kanji i-adjective path.
 *
 * Emits emphatic, connection-form, conjectural, classical-negative, and
 * classical-attributive variants after the primary scan has completed.
 */
void appendKanjiIAdjPostVariants(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                 size_t hiragana_end, const grammar::Inflection& inflection,
                                 const dictionary::DictionaryManager* dict_manager,
                                 std::vector<UnknownCandidate>& candidates, size_t candidate_start);

/**
 * @brief Append compound i-adjective candidates for a kanji stem.
 */
void appendKanjiCompoundIAdjCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates, size_t candidate_start);

/**
 * @brief Check whether か at @p pos opens the i-adjective past connective かっ.
 *
 * The question particle and the past connective are spelled with the same mora,
 * and only the conjugation is followed by っ. A scanner that stops at every か
 * truncates the past form and leaves the stem to a fabricated godan verb
 * (うれし+かった read as a form of うれしかう). The negative auxiliary's own past
 * is excluded: 〜くなかった splits as く + なかっ + た, so a な directly before
 * かっ is a boundary after all.
 */
bool opensAdjectivePastConnective(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether an adjective productively forms compounds as a second element.
 *
 * These adjectives attach to a noun or a verb continuative to derive a new
 * adjective (息苦しい, 用心深い, 我慢強い, 読みにくい). Their compounds are built by
 * rule rather than listed, so a candidate spanning one is a derivation and not a
 * fabrication. Every other adjective is a complete word on its own, and material
 * in front of it belongs to a separate token.
 */
bool isCompoundFormingAdjective(const std::string& base_form);

/**
 * @brief Check whether a base form is a nominal host plus a productive second element.
 *
 * A derivation is its own evidence: the second element cannot take a host and
 * still be read as a separate word, so the whole span is one adjective however
 * unlikely the stem looks to the length-based confidence score (めんどくさい,
 * うそくさかった). The host must be a nominal — a run that is itself a function
 * word, or that ends in a particle binding a nominal phrase, is a phrase
 * boundary and the adjective after it is a predicate instead (ちょっと|くさい,
 * この魚は|くさい, その|くさい匂い). @p start_pos anchors the host in the
 * observed run, whose head the inflection analyzer leaves untouched.
 */
bool derivesFromCompoundFormingAdjective(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::string& base_form,
                                         const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Whether an analyzed span reaches past the end of the adjective paradigm.
 *
 * The engine reconstructs a base form, so what the span holds beyond that base's
 * stem is supposed to be one inflectional ending. The i-adjective endings are a
 * closed set, and two shapes are outside it however the analyzer got there. 様態
 * そう is a separate auxiliary and never an ending (優し|そう|だ), including for a
 * verb continuative whose hypothesized stem + い is a non-word (書きそう). The
 * connective て closes a clause and only ever sits at the end of an ending
 * (寒く|て), so a て with more span behind it means the analysis swallowed a whole
 * predicate: 悪くなってくる reads as a form of 悪い even though 悪く|なっ|て|くる is
 * four morphemes. Both leave the boundary to the stem path, which emits the stem
 * and lets the auxiliary or the particle attach on its own.
 */
bool spansPastAdjectiveEnding(const std::string& surface, const std::string& base_form);

/**
 * @brief Append surface-qualified pure-hiragana i-adjective candidates.
 */
void appendHiraganaIAdjSurfaceCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                         bool starts_with_particle, const grammar::Inflection& inflection,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates);

}  // namespace suzume::analysis::adj_detail

#endif  // SUZUME_ANALYSIS_ADJECTIVE_CANDIDATES_INTERNAL_H_
