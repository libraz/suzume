#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_HIRAGANA_INTERNAL_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_HIRAGANA_INTERNAL_H_

#include <string>
#include <string_view>
#include <vector>

#include "analysis/verb_candidates.h"
#include "normalize/char_type.h"

namespace suzume::analysis::hiragana_verb_detail {

// Longest kana run a hiragana verb scan examines.
inline constexpr size_t kPredicateRunMax = 12;

// Guard (fabricated closed-class absorption family, tail class): true when the
// hiragana run [start_pos, end_pos) is a verb prefix followed by a 副助詞
// (しか/さえ/すら), so the run is verb + particle rather than a single fabricated
// 未然形 (やるしか → や|る|しか, never a form of the non-word 〜しく). Declared
// here so the guard can be exercised directly by characterization tests; see the
// guard-family note in verb_candidates_helpers.h.
bool endsWithParticleAfterVerb(const dictionary::DictionaryManager* dict_manager, const grammar::Inflection& inflection,
                               const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

// True when start_pos opens a predicate slot: text start, a symbol, or a particle.
bool opensPredicateSlot(const std::vector<char32_t>& codepoints, size_t start_pos);
bool pronounEndsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                   size_t pos);
// True when start_pos follows a case particle whose host is a kanji or a
// dictionary noun/pronoun, which fixes start_pos as a predicate slot.
bool followsKanjiOrNominalHostBeforeCaseParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                 const dictionary::DictionaryManager* dict_manager);

struct GodanMizenkeiForms {
  char32_t a_row_char;
  grammar::VerbType verb_type;
  std::string_view base_suffix;
  std::string mizenkei_surface;
  std::string stem;
  std::string base_form;
};

bool deriveGodanMizenkeiForms(const std::vector<char32_t>& codepoints, size_t start_pos, size_t mizenkei_end,
                              GodanMizenkeiForms& out);

void appendPassiveMizenkeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates);
void appendIchidanRareruCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                   const grammar::Inflection& inflection,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates);
void appendMizenkeiNCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                               const dictionary::DictionaryManager* dict_manager,
                               std::vector<UnknownCandidate>& candidates);
void appendMizenkeiNegativeCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates);
void appendMizenkeiNakyaCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                   const grammar::Inflection& inflection,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates);
void appendNOnbinNaiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                               const grammar::Inflection& inflection, const dictionary::DictionaryManager* dict_manager,
                               std::vector<UnknownCandidate>& candidates);
void appendOnbinContractionCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates);
void appendKuruMizenkeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                  std::vector<UnknownCandidate>& candidates);
void appendKuruRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                  const dictionary::DictionaryManager* dict_manager,
                                  std::vector<UnknownCandidate>& candidates);
void appendKkoNominalizerCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    std::vector<UnknownCandidate>& candidates);
void appendSuruInabilityCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                   std::vector<UnknownCandidate>& candidates);
void appendEruObligationCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                   std::vector<UnknownCandidate>& candidates);
void appendSuruSubsidiaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const dictionary::DictionaryManager* dict_manager,
                                    std::vector<UnknownCandidate>& candidates);
bool isClearTeFormBeforeSubsidiary(const std::vector<char32_t>& codepoints, size_t start_pos, bool allow_emphatic_mo);
void appendContextualSubsidiaryCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                         std::string_view lemma, dictionary::ConjugationType conj_type,
                                         core::ExtendedPOS extended_pos, const char* pattern, float candidate_cost,
                                         std::vector<UnknownCandidate>& candidates,
                                         core::PartOfSpeech pos = core::PartOfSpeech::Auxiliary);
void appendIkuAuxiliaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                  std::vector<UnknownCandidate>& candidates);
void appendYaruBenefactiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     std::vector<UnknownCandidate>& candidates);
void appendMiruAuxiliaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates);
void appendMiseruAuxiliaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates);
void appendAgeruBenefactiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates);
void appendOkuAuxiliaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                  std::vector<UnknownCandidate>& candidates);
void appendIchidanRenyokei1CharCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                          const dictionary::DictionaryManager* dict_manager,
                                          std::vector<UnknownCandidate>& candidates);
void appendHiraganaDerivedCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                     const std::vector<normalize::CharType>& char_types,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates);
bool appendInflectedHiraganaVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           size_t hiragana_end, char32_t first_char,
                                           const std::vector<normalize::CharType>& char_types,
                                           const grammar::Inflection& inflection,
                                           const dictionary::DictionaryManager* dict_manager,
                                           const VerbCandidateOptions& verb_opts, bool has_complete_godan_wa_terminal,
                                           bool has_complete_case_particle_terminal,
                                           std::vector<UnknownCandidate>& candidates);

}  // namespace suzume::analysis::hiragana_verb_detail

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_HIRAGANA_INTERNAL_H_
