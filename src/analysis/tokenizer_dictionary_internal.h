#ifndef SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_
#define SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_

#include <cstddef>
#include <string_view>
#include <vector>

#include "core/lattice.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

void appendSpecialGrammarCandidates(core::Lattice& lattice, std::string_view text,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t byte_pos);

/**
 * @brief Add a closed-class dictionary edge whose lemma is its surface
 * @param flags Edge flags; defaults to a plain dictionary origin
 */
void addClosedClassEdge(core::Lattice& lattice, std::string_view surface, size_t start_pos, size_t end_pos,
                        core::PartOfSpeech pos, core::ExtendedPOS epos, std::string_view pattern,
                        uint8_t flags = core::LatticeEdge::kFromDictionary);

/**
 * @brief Add a context-licensed grammar edge at an explicit cost
 * @param lemma Lemma of the edge; the surface when empty
 */
void addGrammarEdge(core::Lattice& lattice, std::string_view surface, size_t start_pos, size_t end_pos,
                    core::PartOfSpeech pos, float cost, core::ExtendedPOS epos, std::string_view pattern,
                    std::string_view lemma = {},
                    dictionary::ConjugationType conj_type = dictionary::ConjugationType::None);

}  // namespace suzume::analysis::tokenizer_dictionary_detail

#endif  // SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_
