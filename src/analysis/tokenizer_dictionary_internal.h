#ifndef SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_
#define SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_

#include <cstddef>
#include <string_view>
#include <vector>

#include "analysis/tokenizer_utils.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"

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

/**
 * @brief Inputs shared by every dictionary result at one start position
 *
 * The longest_* fields hold the longest lookup result of each class at the
 * position, which the guards use to let a longer member own the span.
 */
struct DictionaryCandidateContext {
  const dictionary::DictionaryManager& dict_manager;
  const grammar::Inflection& inflection;
  std::string_view text;
  const std::vector<char32_t>& codepoints;
  const ByteOffsets& byte_offsets;
  size_t start_pos;
  size_t byte_pos;
  const std::vector<dictionary::LookupResult>& lookup_results;
  bool has_attributive_temporal_ma;
  bool starts_shortened_causative_passive;
  bool suppress_prefixed_noun_interior;
  size_t longest_conjunction;
  size_t longest_fixed_conjunction;
  size_t longest_interjection;
  size_t longest_adverb;
  size_t longest_noun;
  size_t longest_potential_benefactive;
};

/**
 * @brief Whether a dictionary reading cuts into a word or chain its context already establishes
 */
bool crossesEstablishedBoundary(const DictionaryCandidateContext& ctx, const core::Lattice& lattice,
                                const dictionary::LookupResult& result, size_t end_pos);

/**
 * @brief Whether a dictionary reading loses to a homograph the neighbouring category selects
 */
bool losesHomographReading(const DictionaryCandidateContext& ctx, const core::Lattice& lattice,
                           const dictionary::LookupResult& result, size_t end_pos, std::string_view following_text);

/**
 * @brief Whether a reading's word class cannot stand in its environment, or a
 *        longer member of the class owns the span
 */
bool isOutOfPlaceForWordClass(const DictionaryCandidateContext& ctx, const core::Lattice& lattice,
                              const dictionary::LookupResult& result, size_t end_pos);

/**
 * @brief Whether a function word or paradigm cell lacks the host or follower that licenses it
 *
 * Runs after the entry's derived verb-stem edges are added, so it suppresses
 * only the entry's own edge.
 */
bool lacksLicensingEnvironment(const DictionaryCandidateContext& ctx, const core::Lattice& lattice,
                               const dictionary::LookupResult& result, size_t end_pos, std::string_view following_text);

/**
 * @brief Whether a dictionary verb ends at @p end_pos while starting before @p start_pos
 */
bool endsDictionaryVerbSpanningBack(const dictionary::DictionaryManager& dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Whether @p surface is a dictionary Godan onbin stem plus its matching past allomorph
 */
bool isDictionaryOnbinPast(const dictionary::DictionaryManager& dict_manager, std::string_view surface);

/**
 * @brief Whether a noun or pronoun edge ends at @p start_pos
 */
inline bool hasPrecedingNominal(const core::Lattice& lattice, size_t start_pos) {
  return hasPrecedingPartOfSpeech(lattice, start_pos, kNounPronounMask);
}

/**
 * @brief Whether a multi-mora adverb ending in か before という has absorbed the
 *        question particle of a quoted question (なぜか+という against なぜ+か+という)
 */
inline bool adverbAbsorbsQuotedQuestion(const std::vector<char32_t>& codepoints, size_t length, size_t end_pos) {
  return length > 1 && codepoints[end_pos - 1] == U'か' && end_pos + 2 < codepoints.size() &&
         codepoints[end_pos] == U'と' && codepoints[end_pos + 1] == U'い' && codepoints[end_pos + 2] == U'う';
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail

#endif  // SUZUME_ANALYSIS_TOKENIZER_DICTIONARY_INTERNAL_H_
