#ifndef SUZUME_ANALYSIS_SCORER_H_
#define SUZUME_ANALYSIS_SCORER_H_

#include "analysis/candidate_options.h"
#include "analysis/connection_rule_options.h"
#include "analysis/interfaces.h"
#include "core/lattice.h"
#include "core/types.h"

namespace suzume::analysis {

/**
 * @brief Scoring options
 */
struct ScorerOptions {
  // POS priors
  float noun_prior = 0.0F;
  float verb_prior = 0.2F;
  float adj_prior = 0.3F;
  float adv_prior = 0.4F;
  float particle_prior = 0.1F;
  float aux_prior = 0.2F;
  float pronoun_prior = 0.1F;

  // Penalties
  float single_kanji_penalty = 2.0F;
  float single_hiragana_penalty = 1.5F;
  float symbol_penalty = 1.0F;
  float formal_noun_penalty = 1.0F;
  float low_info_penalty = 0.5F;

  // Bonuses
  float dictionary_bonus = -1.0F;
  float user_dict_bonus = -2.0F;
  float optimal_length_bonus = -0.5F;

  // Optimal length range
  struct OptimalLength {
    size_t noun_min = 2;
    size_t noun_max = 6;
    size_t verb_min = 3;  // Increased from 2 to prevent short verbs like して from getting bonus
    size_t verb_max = 12; // Increased to accommodate long conjugated forms
                          // e.g., かけられなくなった (9 chars), 食べさせられなくなった (10 chars)
    size_t adj_min = 2;
    size_t adj_max = 6;
    size_t katakana_min = 3;
    size_t katakana_max = 12;
  } optimal_length;

  // Connection rule options (edge costs and connection costs)
  // These can be loaded from JSON at runtime for parameter tuning
  ConnectionRuleOptions connection_rules;

  // Candidate generation options (join/split costs)
  // These can be loaded from JSON at runtime for parameter tuning
  CandidateOptions candidates;
};

/**
 * @brief Scoring calculator for morphological analysis
 */
class Scorer : public IScorer {
 public:
  explicit Scorer(const ScorerOptions& options = {});
  ~Scorer() override = default;

  // Non-copyable, non-movable (inherits from IScorer)
  Scorer(const Scorer&) = delete;
  Scorer& operator=(const Scorer&) = delete;
  Scorer(Scorer&&) = delete;
  Scorer& operator=(Scorer&&) = delete;

  /**
   * @brief Calculate word cost
   * @param edge Lattice edge
   * @return Word cost
   */
  float wordCost(const core::LatticeEdge& edge) const override;

  /**
   * @brief Calculate connection cost
   * @param prev Previous edge
   * @param next Next edge
   * @return Connection cost
   */
  float connectionCost(const core::LatticeEdge& prev,
                       const core::LatticeEdge& next) const override;

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

  /**
   * @brief Get edge options for word cost calculation
   */
  const EdgeOptions& edgeOpts() const { return options_.connection_rules.edge; }

  /**
   * @brief Get connection options for connection cost calculation
   */
  const ConnectionOptions& connOpts() const { return options_.connection_rules.connection; }

  /**
   * @brief Calculate bigram connection cost
   */
  static float bigramCost(core::PartOfSpeech prev, core::PartOfSpeech next);

  /**
   * @brief Check if edge has optimal length
   */
  bool isOptimalLength(const core::LatticeEdge& edge) const;

  /**
   * @brief Log adjustment for debug output
   * @param amount Adjustment amount (negative = bonus, positive = penalty)
   * @param reason Description of the adjustment
   */
  static void logAdjustment(float amount, const char* reason);
};

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SCORER_H_
