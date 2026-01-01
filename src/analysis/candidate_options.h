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

/// Options for verb candidate generation
/// Controls confidence thresholds and base costs for verb candidate scoring
struct VerbCandidateOptions {
  // Confidence thresholds
  float confidence_low = 0.4F;              // Filter very low confidence (line 227)
  float confidence_standard = 0.48F;        // Standard acceptance threshold (lines 412, 441, 446, 573)
  float confidence_past_te = 0.25F;         // Threshold for past/te forms (line 947)
  float confidence_ichidan_dict = 0.28F;    // Threshold for ichidan dict forms (line 908, 948)
  float confidence_dict_verb = 0.35F;       // Threshold for dictionary verbs (line 946)
  float confidence_katakana = 0.5F;         // Threshold for katakana verbs (line 1172)
  float confidence_high = 0.7F;             // High confidence for short verbs (line 970)
  float confidence_very_high = 0.8F;        // Very high for long verbs (line 1038)

  // Base costs (lower = more preferred)
  float base_cost_standard = 0.4F;          // Standard base cost (line 503)
  float base_cost_high = 0.5F;              // Higher cost for uncertain (line 952)
  float base_cost_low = 0.3F;               // Low cost for good matches (line 255)
  float base_cost_verified = 0.1F;          // Very low for verified (line 532, 999)
  float base_cost_long_verified = 0.05F;    // Minimal for long verified (line 1055)

  // Bonuses (negative = preferred)
  float bonus_dict_match = -0.2F;           // Dictionary match bonus (line 518)
  float bonus_ichidan = -0.2F;              // Ichidan verb bonus (line 581)
  float bonus_long_dict = -0.3F;            // Long dictionary verb bonus (line 1018)
  float bonus_long_verified = -0.8F;        // Long verified verb bonus (line 1016)

  // Penalties (positive = discouraged)
  float penalty_single_char = 1.5F;         // Single character verb penalty (line 541)

  // Cost scaling factors for confidence
  float confidence_cost_scale = 0.3F;       // (1.0 - confidence) * scale (standard)
  float confidence_cost_scale_small = 0.1F; // Smaller scaling factor
  float confidence_cost_scale_medium = 0.2F; // Medium scaling factor
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
  VerbCandidateOptions verb;

  /// Create default options matching candidate_constants.h
  static CandidateOptions defaults() { return CandidateOptions{}; }
};

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_CANDIDATE_OPTIONS_H_
