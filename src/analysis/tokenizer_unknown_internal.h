#ifndef SUZUME_ANALYSIS_TOKENIZER_UNKNOWN_INTERNAL_H_
#define SUZUME_ANALYSIS_TOKENIZER_UNKNOWN_INTERNAL_H_

#include <cstddef>
#include <string_view>
#include <vector>

#include "analysis/tokenizer_utils.h"
#include "analysis/unknown.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis::tokenizer_unknown_detail {

/**
 * @brief Inputs shared by every unknown-word candidate at one start position
 *
 * The kanji run [start_pos, kanji_end) closes on a verb continuative from
 * following_verb_start, and max_dict_length is the longest content-word lookup
 * result at the position.
 */
struct UnknownCandidateContext {
  const dictionary::DictionaryManager& dict_manager;
  const grammar::Inflection& inflection;
  const UnknownWordGenerator& unknown_gen;
  std::string_view text;
  const std::vector<char32_t>& codepoints;
  const ByteOffsets& byte_offsets;
  size_t start_pos;
  const std::vector<normalize::CharType>& char_types;
  const std::vector<dictionary::LookupResult>& dict_results;
  const std::vector<UnknownCandidate>& candidates;
  size_t kanji_end;
  size_t following_verb_start;
  size_t max_dict_length;
};

/**
 * @brief Whether a generated candidate is barred from the lattice by the
 *        dictionary entries, edges and sibling candidates around it
 */
bool isRejectedUnknownCandidate(const UnknownCandidateContext& ctx, const core::Lattice& lattice,
                                const UnknownCandidate& candidate);

/**
 * @brief Whether a candidate's span already decomposes into a verified left
 *        constituent and a complete closed right constituent (本+なし, す+べき)
 */
bool hasCompleteInternalConstituentBoundary(const core::Lattice& lattice,
                                            const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                            const ByteOffsets& byte_offsets,
                                            const std::vector<UnknownCandidate>& batch_candidates,
                                            const UnknownCandidate& candidate);

}  // namespace suzume::analysis::tokenizer_unknown_detail

#endif  // SUZUME_ANALYSIS_TOKENIZER_UNKNOWN_INTERNAL_H_
