#ifndef SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_
#define SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_

#include <cstddef>
#include <string_view>
#include <vector>

#include "analysis/tokenizer_utils.h"
#include "core/lattice.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

void appendSpecialGrammarCandidates(core::Lattice& lattice, std::string_view text,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t byte_pos);

/**
 * @brief Add an edge with Dictionary origin and the standard dictionary origin confidence
 * @param lemma Lemma of the edge
 * @param pattern Debug label recorded as the ExtendedPOS source
 */
void addDictionaryOriginEdge(core::Lattice& lattice, std::string_view surface, size_t start_pos, size_t end_pos,
                             core::PartOfSpeech pos, float cost, uint8_t flags, std::string_view lemma,
                             dictionary::ConjugationType conj_type, core::ExtendedPOS epos, std::string_view pattern);

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

/**
 * @brief Whether a dictionary interrogative pronoun ends exactly at @p end_pos
 */
bool hasInterrogativeEndingAt(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                              const ByteOffsets& byte_offsets, size_t end_pos);

/**
 * @brief Whether a completed attributive predicate ends at @p start_pos
 */
bool hasPrecedingAttributivePredicate(const core::Lattice& lattice, size_t start_pos);

/**
 * @brief Context flags the main dictionary pass reads after the contextual edges are added
 */
struct ContextualDictionaryCandidateState {
  bool has_attributive_temporal_ma{false};
  bool starts_shortened_causative_passive{false};
};

/**
 * @brief Add closed-class edges licensed only by the surrounding lattice context
 */
ContextualDictionaryCandidateState addContextualDictionaryCandidates(core::Lattice& lattice,
                                                                     const dictionary::DictionaryManager& dict_manager,
                                                                     std::string_view text,
                                                                     const std::vector<char32_t>& codepoints,
                                                                     const ByteOffsets& byte_offsets, size_t start_pos);

/**
 * @brief Add auxiliaries and held final particles spelled with interior prolonged sound marks
 */
void addElidedProlongedDictionaryCandidates(core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                            const std::vector<char32_t>& codepoints, size_t start_pos);

/**
 * @brief Add adverbs cut at the sokuon before their closing と
 */
void addTruncatedAdverbCandidates(core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                  const std::vector<char32_t>& codepoints, size_t start_pos);

/**
 * @brief Add greetings clipped of their final う or spelled with a prolonged mark
 */
void addClippedInterjectionCandidates(core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                      const std::vector<char32_t>& codepoints, size_t start_pos);

}  // namespace suzume::analysis::tokenizer_dictionary_detail

#endif  // SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_
