#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_KANJI_INTERNAL_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_KANJI_INTERNAL_H_

#include "analysis/verb_candidates.h"

namespace suzume::analysis::kanji_verb_detail {

float getIchidanConfidence(const std::vector<grammar::InflectionCandidate>& candidates, float min_threshold);

// Append an Ichidan 仮定形 candidate when @p analyses confirm an Ichidan reading.
// Returns whether a candidate was appended.
bool appendIchidanKateikeiCandidate(std::vector<UnknownCandidate>& candidates,
                                    const std::vector<grammar::InflectionCandidate>& analyses,
                                    const std::string& surface, size_t start_pos, size_t end_pos,
                                    const std::string& lemma, const char* pattern, bool lemma_verified = false);

// Acceptance threshold for a mixed-script godan-ka stem (羽ばた+く): its complete
// い-onbin past/te cell clears the past/te bar, every other cell the low bar.
float mixedGodanKaStemThreshold(const grammar::InflectionCandidate& candidate, const VerbCandidateOptions& verb_opts);

// Whether an adjective that attaches to a verb continuative (にくい, やすい)
// starts at @p pos.
bool renyokeiAdjectiveFollowsAt(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t pos);

// Whether the pejorative auxiliary やがる starts at @p pos. It sits on a bare
// continuative (見+やがって) and opens with the particle や.
bool pejorativeAuxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager,
                                  const std::vector<char32_t>& codepoints, size_t pos);

// A multi-kanji godan-wa continuative ending in い before a kanji continuation
// (背負い+進む), which the inflection scorer underrates as an i-adjective shape.
bool isMultiKanjiGodanWaRenyokei(const grammar::InflectionCandidate& candidate, std::string_view surface,
                                 const std::vector<char32_t>& codepoints, size_t end_pos);

void appendGodanMizenkeiPassiveCausativeCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                   size_t kanji_end, size_t hiragana_end,
                                                   const grammar::Inflection& inflection,
                                                   const dictionary::DictionaryManager* dict_manager,
                                                   std::vector<UnknownCandidate>& candidates);
void appendSaRowContractedMizenkeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             size_t kanji_end, size_t hiragana_end,
                                             const grammar::Inflection& inflection,
                                             std::vector<UnknownCandidate>& candidates);
void appendGodanMizenkeiZuCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                     size_t hiragana_end, const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates);
void appendIchidanRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                     size_t hiragana_end, const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     const VerbCandidateOptions& verb_opts, std::vector<UnknownCandidate>& candidates);
void appendGodanSaRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                     size_t hiragana_end, const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     const VerbCandidateOptions& verb_opts, std::vector<UnknownCandidate>& candidates);
void appendIchidanKateikeiVolitionalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                               size_t kanji_end, size_t hiragana_end,
                                               const grammar::Inflection& inflection,
                                               const dictionary::DictionaryManager* dict_manager,
                                               std::vector<UnknownCandidate>& candidates);
void appendGodanPassiveRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                          size_t hiragana_end, const grammar::Inflection& inflection,
                                          const dictionary::DictionaryManager* dict_manager,
                                          const VerbCandidateOptions& verb_opts,
                                          std::vector<UnknownCandidate>& candidates);
void appendIchidanStemRareCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                     size_t hiragana_end, const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates);
void appendSingleKanjiIchidanCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                        size_t hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                        std::vector<UnknownCandidate>& candidates);
void appendAnalyzedKanjiVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       const VerbCandidateOptions& verb_opts, bool sokuonbin_stem_verified,
                                       const std::string& sokuonbin_lemma, std::vector<UnknownCandidate>& candidates);
void appendSelectedKanjiVerbCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                      size_t stem_end, size_t end_pos, const std::string& surface,
                                      const std::string& hiragana_part, const grammar::InflectionCandidate& best,
                                      bool is_dict_verified, bool follows_reduplicated_noun,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      const VerbCandidateOptions& verb_opts, bool sokuonbin_stem_verified,
                                      const std::string& sokuonbin_lemma, std::vector<UnknownCandidate>& candidates);
void appendKanjiMizenkeiStemCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates);
void appendKanjiOnbinCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                size_t hiragana_end, const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager, bool sokuonbin_stem_verified,
                                const std::string& sokuonbin_lemma, grammar::VerbType sokuonbin_verb_type,
                                std::vector<UnknownCandidate>& candidates);
void appendExtendedSokuonbinCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates);
void appendClassicalHaRowCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                    size_t hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                    std::vector<UnknownCandidate>& candidates);
/**
 * @brief Append the ク語法 nominalization of a kanji stem (言わく, 思わく).
 * @return End position of the licensed span, or @p start_pos when none was found.
 */
size_t appendKuNominalizationCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                        size_t hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                        std::vector<UnknownCandidate>& candidates);
void appendVerifiedTailGodanTaCompoundCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                 size_t kanji_end, const dictionary::DictionaryManager* dict_manager,
                                                 std::vector<UnknownCandidate>& candidates);

}  // namespace suzume::analysis::kanji_verb_detail

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_KANJI_INTERNAL_H_
