#ifndef SUZUME_ANALYSIS_SCORER_H_
#define SUZUME_ANALYSIS_SCORER_H_

#include <array>
#include <cmath>
#include <limits>

#include "analysis/candidate_options.h"
#include "core/lattice.h"
#include "core/types.h"
#include "grammar/inflection_scorer.h"

namespace suzume::analysis {

// Public POS values collapse to the 13 scoring profiles used by the static
// bigram matrix (Interjection, Other, and Unknown share the final profile).
inline constexpr size_t kScoringPosClassCount = 13;
using BigramOverrideCostTable = std::array<std::array<float, kScoringPosClassCount>, kScoringPosClassCount>;

/**
 * @brief Scoring options
 */
struct ScorerOptions {
  // POS priors
  float noun_prior = 0.0F;
  float verb_prior = 0.2F;
  float adj_prior = 0.3F;
  float adv_prior = 0.2F;  // Reduced from 0.4F to avoid penalizing common adverbs
  float particle_prior = 0.1F;
  float aux_prior = 0.2F;
  float pronoun_prior = 0.1F;

  // Bonuses
  float user_dict_bonus = -2.0F;

  // Bigram cost overrides (NaN = use default table value)
  // Only frequently-adjusted pairs are exposed for tuning
  // Format: {prev}_{next} where prev/next are POS categories
  struct BigramOverrides {
    // High-impact pairs (adjust with caution)
    float noun_to_suffix = std::numeric_limits<float>::quiet_NaN();  // default: -0.8
    float prefix_to_noun = std::numeric_limits<float>::quiet_NaN();  // default: -0.5
    float prefix_to_verb = std::numeric_limits<float>::quiet_NaN();  // default: -0.5
    float pron_to_aux = std::numeric_limits<float>::quiet_NaN();     // default: 0.2

    // Verb connections
    float verb_to_verb = std::numeric_limits<float>::quiet_NaN();  // default: 0.8
    float verb_to_noun = std::numeric_limits<float>::quiet_NaN();  // default: 0.2
    float verb_to_aux = std::numeric_limits<float>::quiet_NaN();   // default: 0.0

    // Adjective connections
    float adj_to_aux = std::numeric_limits<float>::quiet_NaN();   // default: 0.0
    float adj_to_verb = std::numeric_limits<float>::quiet_NaN();  // default: 0.5
    float adj_to_adj = std::numeric_limits<float>::quiet_NaN();   // default: 0.8

    // Particle connections
    float part_to_verb = std::numeric_limits<float>::quiet_NaN();  // default: 0.2
    float part_to_noun = std::numeric_limits<float>::quiet_NaN();  // default: 0.0

    // Auxiliary connections
    float aux_to_part = std::numeric_limits<float>::quiet_NaN();  // default: 0.0
    float aux_to_aux = std::numeric_limits<float>::quiet_NaN();   // default: 0.3
  } bigram;

  // Candidate generation options (join/split costs)
  // These can be loaded from JSON at runtime for parameter tuning
  CandidateOptions candidates;

  // Inflection scorer options (confidence adjustments)
  // These override values in inflection_scorer_constants.h
  // NaN = use default constexpr value
  // Defined in grammar/inflection_scorer.h
  grammar::InflectionScorerOptions inflection;
};

/**
 * @brief Scoring calculator for morphological analysis
 */
class Scorer {
 public:
  explicit Scorer(const ScorerOptions& options = {});
  ~Scorer() = default;

  // Non-copyable, non-movable
  Scorer(const Scorer&) = delete;
  Scorer& operator=(const Scorer&) = delete;
  Scorer(Scorer&&) = delete;
  Scorer& operator=(Scorer&&) = delete;

  /**
   * @brief Calculate word cost
   * @param edge Lattice edge
   * @return Word cost
   */
  float wordCost(const core::LatticeEdge& edge) const;

  /**
   * @brief Calculate connection cost
   * @param prev Previous edge
   * @param next Next edge
   * @return Connection cost
   */
  float connectionCost(const core::LatticeEdge& prev, const core::LatticeEdge& next) const;

  /**
   * @brief Calculate the connection cost of an edge that opens the sentence
   *
   * Mirror of connectionCost() for the BOS state, which has no arriving edge.
   * Penalizes a morpheme that cannot naturally lead a sentence.
   *
   * @param edge Edge starting at position 0
   * @return BOS connection cost
   */
  float bosCost(const core::LatticeEdge& edge) const;

  /**
   * @brief Calculate the extra cost of an edge that closes the sentence
   *
   * Symmetric to bosCost(): penalizes a morpheme that cannot naturally END a
   * sentence, so an isolated hiragana word is not carved into a stem plus a
   * dangling auxiliary/aspect.
   *
   * @param edge Edge ending at the last position
   * @param prev_extended_pos Category of the morpheme before it, or
   *                          ExtendedPOS::Unknown when the edge opens the
   *                          sentence as well as closing it
   * @param prev_length Length in codepoints of the morpheme before it, or 0
   *                    when there is none
   * @return EOS cost
   */
  float eosCost(const core::LatticeEdge& edge, core::ExtendedPOS prev_extended_pos = core::ExtendedPOS::Unknown,
                size_t prev_length = 0) const;

  /**
   * @brief Get POS prior
   * @param pos Part of speech
   * @return Prior cost
   */
  float posPrior(core::PartOfSpeech pos) const;

  /**
   * @brief Get join candidate options
   */
  const JoinOptions& joinOpts() const { return options_.candidates.join; }

  /**
   * @brief Get split candidate options
   */
  const SplitOptions& splitOpts() const { return options_.candidates.split; }

 private:
  ScorerOptions options_;
  BigramOverrideCostTable bigram_override_costs_;

  /**
   * @brief Calculate bigram connection cost
   * Uses BigramOverrides if set, otherwise falls back to default table
   */
  float bigramCost(core::PartOfSpeech prev, core::PartOfSpeech next) const;
};

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SCORER_H_
