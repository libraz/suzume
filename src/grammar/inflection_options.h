#ifndef SUZUME_GRAMMAR_INFLECTION_OPTIONS_H_
#define SUZUME_GRAMMAR_INFLECTION_OPTIONS_H_

// =============================================================================
// Inflection Options
// =============================================================================
// Runtime-configurable parameters for inflection scoring.
// These can be loaded from JSON for parameter tuning without rebuild.
// =============================================================================

namespace suzume::grammar {

/// Options for inflection scoring (loaded from JSON)
struct InflectionOptions {
  // ==========================================================================
  // Stem Length Adjustments
  // ==========================================================================

  /// Penalty for very long stems (12+ bytes / 4+ chars)
  float penalty_stem_very_long = 0.25F;

  /// Penalty for long stems (9-11 bytes / 3 chars)
  float penalty_stem_long = 0.15F;

  /// Bonus for 2-char stems (6 bytes)
  float bonus_stem_two_char = 0.02F;

  /// Bonus for 1-char stems (3 bytes)
  float bonus_stem_one_char = 0.01F;

  /// Bonus per byte of auxiliary chain matched
  float bonus_aux_length_per_byte = 0.02F;

  // ==========================================================================
  // Ichidan Validation
  // ==========================================================================

  /// E-row ending confirms Ichidan
  float bonus_ichidan_e_row = 0.12F;

  /// Stem matches Godan conjugation pattern
  float penalty_ichidan_looks_godan = 0.15F;

  /// Kanji + single hiragana stem pattern
  float penalty_ichidan_kanji_hiragana_stem = 0.50F;

  /// Pure hiragana stem penalty (e.g., つかれる)
  /// Note: Lower value allows common hiragana verbs to be recognized
  float penalty_pure_hiragana_stem = 0.20F;

  /// Single hiragana particle stem in mizenkei context
  float penalty_ichidan_single_hiragana_particle_stem = 0.45F;

  // ==========================================================================
  // Godan Validation
  // ==========================================================================

  /// Single hiragana Godan stem penalty
  float penalty_godan_single_hiragana_stem = 0.40F;

  /// Godan (non-Ra) pure hiragana multi-char stem penalty
  float penalty_godan_non_ra_pure_hiragana_stem = 0.45F;

  /// Single hiragana GodanRa stem
  float penalty_godan_ra_single_hiragana = 0.30F;

  // ==========================================================================
  // Unknown Word Generation
  // ==========================================================================

  /// Confidence threshold for hiragana verb candidates (non-dictionary)
  float hiragana_verb_confidence_threshold = 0.40F;

  /// Confidence threshold for dictionary-verified hiragana verbs
  float hiragana_verb_dict_confidence_threshold = 0.35F;

  /// Create default options matching inflection_scorer_constants.h
  static InflectionOptions defaults() { return InflectionOptions{}; }
};

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_INFLECTION_OPTIONS_H_
