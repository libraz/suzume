#ifndef SUZUME_ANALYSIS_UNKNOWN_SAME_TYPE_INTERNAL_H_
#define SUZUME_ANALYSIS_UNKNOWN_SAME_TYPE_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "analysis/unknown.h"
#include "dictionary/dictionary.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"

namespace suzume::analysis {

/**
 * @brief A same-type run as the phases of generateBySameType share it
 *
 * The opening fields are set by openSameTypeRun, the extent by
 * findSameTypeRunEnd; the candidate phases only read it.
 */
struct UnknownWordGenerator::SameTypeRun {
  size_t start_pos{0};
  normalize::CharType start_type{normalize::CharType::Unknown};
  bool starts_non_word_run{false};
  // The run opens on a particle character that may still start a noun (はし).
  bool started_with_particle{false};
  size_t end_pos{0};
  // A single particle character the hiragana scan crossed (SIZE_MAX = none).
  size_t crossed_particle_pos{SIZE_MAX};
};

namespace same_type_detail {

// Inputs shared by every prefix length of one same-type run.
struct PrefixScanContext {
  const std::vector<char32_t>& codepoints;
  const std::vector<normalize::CharType>& char_types;
  size_t start_pos;
  normalize::CharType start_type;
  bool started_with_particle;
  size_t crossed_particle_pos;
  const dictionary::DictionaryManager* dict_manager;
  const grammar::Inflection& inflection;
  bool starts_at_dictionary_verb_continuative;
  bool starts_after_dictionary_adjective;
};

// The nominal readings that select a prefix before it is priced.
struct PrefixShape {
  bool closes_particle_bracketed_hiragana_noun{false};
  bool brackets_medial_particle_crossing{false};
  bool selects_past_tari_collision_noun{false};
  bool precedes_closed_native_number{false};
  bool closes_genitive_negative_noun{false};
};

PrefixShape classifyPrefixShape(const PrefixScanContext& ctx, size_t len, size_t candidate_end,
                                const std::string& surface);

bool priceKanjiRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, float& cost);

bool priceHiraganaRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, const PrefixShape& shape,
                      float& cost);

void appendInterjectionNominalCandidate(const PrefixScanContext& ctx, size_t len, size_t candidate_end,
                                        const std::string& surface, std::vector<UnknownCandidate>& candidates);

bool priceParticleStartedRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, float& cost,
                             bool& has_suffix);

// Whether a one-mora particle opens [start, end) and a registered pronoun fills
// the rest (は+いつ): the pronoun is the noun the particle stands in front of,
// so no opaque run spans the two.
bool opensOnParticleBeforePronoun(const dictionary::DictionaryManager* dict_manager,
                                  const std::vector<char32_t>& codepoints, size_t start, size_t end);

inline bool isNonWordType(normalize::CharType type) {
  return type == normalize::CharType::Symbol || type == normalize::CharType::Emoji;
}

// Particle that can immediately FOLLOW a content noun (…を, …が, …は). Used as the
// right bracket. の and end-of-input are excluded: の frequently follows a verb
// nominalization (食べるの) and would over-promote.
inline bool isRightBoundaryParticle(char32_t code_point) {
  switch (code_point) {
    case U'を':
    case U'が':
    case U'は':
    case U'も':
    case U'に':
    case U'で':
    case U'へ':
    case U'と':
    case U'ば':
      return true;
    default:
      return false;
  }
}

// Hiragana that reads as a particle when it appears WORD-INTERNALLY during a
// bracketed-noun scan. A native noun may span at most one of these (こども, ともだち);
// a second one marks a genuine particle chain and stops the scan. を remains a
// hard stop; が/の may occur inside native words (つながり, かけがえ) and are
// admitted only under the same one-internal-particle cap.
inline bool isInternalParticleChar(char32_t code_point) {
  return normalize::isParticleCodepoint(code_point) && code_point != U'を' && code_point != U'や';
}

// Length of a dictionary auxiliary starting at @p pos that is itself bound on
// its right, and 0 when there is none. An auxiliary is bound leftward, so it
// always brackets the kana in front of it; what it does not always show is that
// those kana ended a word, because a clause-final auxiliary is spelled exactly
// like the last mora of a noun (からだ, ありがち + だ). A boundary particle or a
// further auxiliary behind it removes that ambiguity: the auxiliary carries its
// own continuation, so it heads a predicate rather than closing a noun
// (りんご + だっ + た, りんご + だ + と).
struct BoundAuxiliary {
  size_t length{0};
  bool is_copula{false};
};

BoundAuxiliary boundAuxiliaryAt(const std::vector<char32_t>& codepoints, size_t pos,
                                const dictionary::DictionaryManager* dict_manager, bool clause_final_counts);

}  // namespace same_type_detail

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_UNKNOWN_SAME_TYPE_INTERNAL_H_
