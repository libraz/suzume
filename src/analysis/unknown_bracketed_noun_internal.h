#ifndef SUZUME_ANALYSIS_UNKNOWN_BRACKETED_NOUN_INTERNAL_H_
#define SUZUME_ANALYSIS_UNKNOWN_BRACKETED_NOUN_INTERNAL_H_

#include <cstddef>
#include <string>
#include <vector>

#include "analysis/unknown.h"

namespace suzume {
namespace analysis {
namespace bracketed_noun_detail {

// Inputs shared by every phase of the rescue at one run start.
struct BracketedNounContext {
  const std::vector<char32_t>& codepoints;
  const std::vector<normalize::CharType>& char_types;
  size_t start_pos;
  const dictionary::DictionaryManager* dict_manager;
  const grammar::Inflection& inflection;
  const VerbCandidateOptions& verb_options;
};

// How the run is bracketed on its left and how far the scan carried it; fixed
// for every run end the rescue offers.
struct BracketedScan {
  bool left_enumerating_bracket{false};
  bool left_particle_bracket{false};
  bool left_genitive_bracket{false};
  bool left_determiner_bracket{false};
  bool left_clause_bracket{false};
  bool left_attributive_bracket{false};
  bool left_te_bracket{false};
  size_t scan{0};
  size_t internal_particles{0};
  bool crossed_verified_predicate{false};
};

// The run [start_pos, scan) one emission weighs: what brackets it on the right
// and which readings its kana already have.
struct PromotedRun {
  size_t scan{0};
  size_t len{0};
  bool right_genitive_after_internal_particle{false};
  bool right_genitive_after_substantive_run{false};
  bool right_particle{false};
  bool right_sokuon_final_particle{false};
  bool right_clause{false};
  bool right_auxiliary{false};
  bool right_adjective_word{false};
  bool right_kanji_word{false};
  bool right_suffix{false};
  bool right_copula{false};
  bool right_particle_opens_copula{false};
  std::string promoted_surface;
  const dictionary::DictionaryEntry* promoted_dictionary_reading{nullptr};
  const dictionary::DictionaryEntry* short_right_particle{nullptr};
  const std::vector<grammar::InflectionCandidate>* promoted_inflections{nullptr};
  bool absorbs_copula_before_sokuon_final{false};
  bool has_exact_noun{false};
  bool has_competing_exact_predicate{false};
  bool copula_selected_predicate_homograph{false};
  bool right_short_genitive{false};
  bool unread_short_run_bracketed{false};
  size_t min_len{0};
  bool short_bos_preparatory_homograph{false};
  bool has_deverbal_noun_shape_before_genitive{false};
  bool has_inflected_predicate_reading{false};
};

bool isImpossibleHiraganaStart(char32_t code_point);
bool closesShortNounAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                       size_t pos);
size_t suffixLengthAt(const BracketedNounContext& ctx, size_t pos);
size_t pronounLengthAt(const BracketedNounContext& ctx, size_t pos);
bool closesTeFormAt(const BracketedNounContext& ctx, size_t pos);
bool registeredAdjectiveOpensAt(const BracketedNounContext& ctx, size_t pos);
BracketedScan classifyLeftBrackets(const BracketedNounContext& ctx);
void scanBracketedRun(const BracketedNounContext& ctx, size_t bracketed_noun_limit, BracketedScan& bracketed);
bool readsAsVerbTerminal(const grammar::Inflection& inflection, const std::string& surface);
bool continuesAuxiliaryChain(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                             core::ExtendedPOS previous, size_t start, size_t end);
bool endsOnPredicateTail(const BracketedNounContext& ctx, size_t run_end);
PromotedRun classifyRightBrackets(const BracketedNounContext& ctx, const BracketedScan& bracketed, size_t run_end);
void readPromotedSurface(const BracketedNounContext& ctx, const BracketedScan& bracketed, PromotedRun& promoted);
bool admitsPromotedRun(const BracketedNounContext& ctx, const BracketedScan& bracketed, const PromotedRun& promoted);
void appendPromotedRunCandidates(const BracketedNounContext& ctx, const BracketedScan& bracketed,
                                 const PromotedRun& promoted, float noun_cost, size_t longer_rescue_end,
                                 std::vector<UnknownCandidate>& candidates);

}  // namespace bracketed_noun_detail
}  // namespace analysis
}  // namespace suzume

#endif  // SUZUME_ANALYSIS_UNKNOWN_BRACKETED_NOUN_INTERNAL_H_
