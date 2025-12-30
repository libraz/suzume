#ifndef SUZUME_ANALYSIS_CANDIDATE_OPTIONS_H_
#define SUZUME_ANALYSIS_CANDIDATE_OPTIONS_H_

// =============================================================================
// Candidate Generation Options
// =============================================================================
// This file defines a struct that holds all adjustable parameters for
// candidate generation (join and split candidates). Default values match
// the constexpr values from candidate_constants.h for backward compatibility.
//
// These options can be loaded from JSON for parameter tuning without rebuild.
// =============================================================================

namespace suzume::analysis {

/// Options for join candidate generation
struct JoinOptions {
  // Compound verb bonus (連用形 + 補助動詞)
  float compound_verb_bonus = -0.8F;

  // Verified Ichidan verb bonus
  float verified_v1_bonus = -0.3F;

  // Verified noun in compound bonus
  float verified_noun_bonus = -0.3F;

  // Te-form + auxiliary bonus
  float te_form_aux_bonus = -0.8F;
};

/// Options for split candidate generation
struct SplitOptions {
  // Alpha + Kanji split bonus
  float alpha_kanji_bonus = -0.3F;

  // Alpha + Katakana split bonus
  float alpha_katakana_bonus = -0.3F;

  // Digit + 1-kanji counter split bonus
  float digit_kanji_1_bonus = -0.2F;

  // Digit + 2-kanji counter split bonus
  float digit_kanji_2_bonus = -0.2F;

  // Digit + 3+ kanji penalty (rare, likely wrong)
  float digit_kanji_3_penalty = 0.5F;

  // Dictionary word split bonus
  float dict_split_bonus = -0.5F;

  // Base cost for split candidates
  float split_base_cost = 1.0F;

  // Noun + Verb split bonus
  float noun_verb_split_bonus = -1.0F;

  // Verified verb in split bonus
  float verified_verb_bonus = -0.8F;
};

/// Combined options for candidate generation
struct CandidateOptions {
  JoinOptions join;
  SplitOptions split;

  /// Create default options matching candidate_constants.h
  static CandidateOptions defaults() { return CandidateOptions{}; }
};

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_CANDIDATE_OPTIONS_H_
