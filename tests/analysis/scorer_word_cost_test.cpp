/**
 * @file scorer_word_cost_test.cpp
 * @brief Regression coverage for data-driven word-cost rules.
 */

#include <gtest/gtest.h>

#include <array>

#include "analysis/bigram_table.h"
#include "analysis/scorer.h"
#include "analysis/scorer_constants.h"
#include "normalize/utf8.h"

namespace suzume::analysis {
namespace {

struct VerbEndingPenaltyCase {
  const char* surface;
  float expected_penalty;
};

core::LatticeEdge makeUnknownHiraganaVerb(std::string_view surface) {
  core::LatticeEdge edge;
  edge.surface = surface;
  edge.pos = core::PartOfSpeech::Verb;
  edge.extended_pos = core::ExtendedPOS::VerbShuushikei;
  return edge;
}

core::LatticeEdge makeBoundaryEdge(core::ExtendedPOS extended_pos, std::string_view surface = {}) {
  core::LatticeEdge edge;
  edge.surface = surface;
  edge.extended_pos = extended_pos;
  edge.pos = core::extendedPosToPos(extended_pos);
  edge.end = static_cast<uint32_t>(suzume::normalize::utf8Length(surface));
  return edge;
}

TEST(ScorerWordCostTest, HiraganaVerbEndingPenaltiesAreTableDriven) {
  constexpr std::array<VerbEndingPenaltyCase, 4> kCases = {{
      {"あそう", bigram_cost::kRare},
      {"あてき", bigram_cost::kVeryRare},
      {"あまし", bigram_cost::kVeryRare},
      {"あてい", bigram_cost::kVeryRare},
  }};

  const Scorer scorer;
  const float baseline_cost = scorer.wordCost(makeUnknownHiraganaVerb("あいう"));
  for (const VerbEndingPenaltyCase& test_case : kCases) {
    EXPECT_FLOAT_EQ(scorer.wordCost(makeUnknownHiraganaVerb(test_case.surface)) - baseline_cost,
                    test_case.expected_penalty);
  }
}

TEST(ScorerWordCostTest, SingleSupplementaryKanjiAdjectiveGetsKanjiIBonus) {
  core::LatticeEdge bmp = makeBoundaryEdge(core::ExtendedPOS::AdjBasic, "暑い");
  bmp.pos = core::PartOfSpeech::Adjective;
  bmp.origin = core::CandidateOrigin::Dictionary;
  core::LatticeEdge supplementary = makeBoundaryEdge(core::ExtendedPOS::AdjBasic, "𠀀い");
  supplementary.pos = core::PartOfSpeech::Adjective;
  supplementary.origin = core::CandidateOrigin::Dictionary;

  const Scorer scorer;
  EXPECT_FLOAT_EQ(scorer.wordCost(supplementary), scorer.wordCost(bmp));
}

TEST(ScorerBoundaryCostTest, FixedBosAndEosAdjustmentsComeFromOneExtendedPosTable) {
  struct BoundaryCostCase {
    core::ExtendedPOS extended_pos;
    float expected_bos;
    float expected_eos;
  };
  constexpr std::array<BoundaryCostCase, 17> kCases = {{
      {core::ExtendedPOS::Conjunction, scorer::kBosConjunctionBonus, scorer::kEosConjunctionPenalty},
      {core::ExtendedPOS::VerbMizenkei, 0.0F, scorer::kEosMizenkeiPenalty},
      {core::ExtendedPOS::Suffix, scorer::kBosSuffixPenalty, 0.0F},
      {core::ExtendedPOS::AuxAppearanceSou, scorer::kBosAppearanceSouPenalty, 0.0F},
      {core::ExtendedPOS::AuxAspectIru, scorer::kBosAspectIruPenalty, 0.0F},
      {core::ExtendedPOS::AuxAspectShimau, scorer::kBosAspectShimauPenalty, 0.0F},
      {core::ExtendedPOS::AuxAspectIku, scorer::kBosAspectIkuPenalty, 0.0F},
      {core::ExtendedPOS::AuxAspectKuru, scorer::kBosAspectKuruPenalty, scorer::kEosAspectKuruPenalty},
      {core::ExtendedPOS::AuxTenseTa, scorer::kBosTensePenalty, 0.0F},
      {core::ExtendedPOS::AuxHonorific, scorer::kBosHonorificAuxPenalty, 0.0F},
      {core::ExtendedPOS::ParticleFinal, scorer::kBosFinalParticlePenalty, 0.0F},
      {core::ExtendedPOS::ParticleTopic, scorer::kBosTopicParticlePenalty, 0.0F},
      {core::ExtendedPOS::ParticleConj, scorer::kBosConjunctiveParticlePenalty, scorer::kEosListingParticlePenalty},
      {core::ExtendedPOS::ParticleBinding, scorer::kBosBindingParticlePenalty, scorer::kEosBindingParticleBonus},
      {core::ExtendedPOS::Prefix, 0.0F, scorer::kEosPrefixPenalty},
      {core::ExtendedPOS::Noun, 0.0F, 0.0F},
      {core::ExtendedPOS::Unknown, 0.0F, 0.0F},
  }};

  for (const BoundaryCostCase& test_case : kCases) {
    const scorer::BoundaryCost boundary_cost = scorer::getBoundaryCost(test_case.extended_pos);
    EXPECT_FLOAT_EQ(boundary_cost.bos, test_case.expected_bos);
    EXPECT_FLOAT_EQ(boundary_cost.eos, test_case.expected_eos);
  }
}

TEST(ScorerBoundaryCostTest, AppliesSurfaceGatesAfterExtendedPosLookup) {
  const Scorer scorer;

  EXPECT_FLOAT_EQ(scorer.bosCost(makeBoundaryEdge(core::ExtendedPOS::AuxAspectIru, "い")),
                  scorer::kBosAspectIruPenalty);
  EXPECT_FLOAT_EQ(scorer.bosCost(makeBoundaryEdge(core::ExtendedPOS::AuxAspectShimau, "しまう")),
                  scorer::kBosAspectShimauPenalty);
  EXPECT_FLOAT_EQ(scorer.bosCost(makeBoundaryEdge(core::ExtendedPOS::ParticleBinding, "さえ")),
                  scorer::kBosBindingParticlePenalty);
  EXPECT_FLOAT_EQ(scorer.bosCost(makeBoundaryEdge(core::ExtendedPOS::Conjunction, "でも")),
                  scorer::kBosDemoConjunctionBonus);
  EXPECT_FLOAT_EQ(scorer.bosCost(makeBoundaryEdge(core::ExtendedPOS::Conjunction, "しかし")),
                  scorer::kBosConjunctionBonus);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::ParticleBinding, "さえ")),
                  scorer::kEosBindingParticleBonus);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::Prefix, "未")), scorer::kEosPrefixPenalty);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::AuxAspectKuru, "き")),
                  scorer::kEosAspectKuruPenalty);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::AuxAspectKuru, "くる")), bigram_cost::kNeutral);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::ParticleConj, "たり")),
                  scorer::kEosListingParticlePenalty);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::ParticleConj, "ので")), bigram_cost::kNeutral);

  // A conjunction closing a sentence that already has content is penalized,
  // while the same word opening an utterance or a post-punctuation fragment is
  // exempt. The sentence start reaches eosCost as Unknown.
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::Conjunction, "ないし"), core::ExtendedPOS::Noun),
                  scorer::kEosConjunctionPenalty);
  EXPECT_FLOAT_EQ(
      scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::Conjunction, "しかし"), core::ExtendedPOS::Unknown),
      bigram_cost::kNeutral);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::Conjunction, "しかし"), core::ExtendedPOS::Symbol),
                  bigram_cost::kNeutral);

  // An irrealis form can never close a sentence, whatever its length: the
  // auxiliary slot it opened has to be filled.
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::VerbMizenkei, "なら")),
                  scorer::kEosMizenkeiPenalty);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::VerbMizenkei, "ならば")),
                  scorer::kEosMizenkeiPenalty);

  core::LatticeEdge registered_determiner = makeBoundaryEdge(core::ExtendedPOS::Determiner, "こういう");
  registered_determiner.flags = core::EdgeFlags::FromDictionary;
  EXPECT_FLOAT_EQ(scorer.eosCost(registered_determiner), bigram_cost::kNeutral);
  EXPECT_FLOAT_EQ(scorer.eosCost(makeBoundaryEdge(core::ExtendedPOS::Determiner, "とんだ")),
                  scorer::kEosDeterminerPenalty);
}

TEST(ScorerBoundaryCostTest, BoundaryOnlyRulesDoNotLeakIntoWordCost) {
  const Scorer scorer;

  core::LatticeEdge binding = makeBoundaryEdge(core::ExtendedPOS::ParticleBinding, "さえ");
  core::LatticeEdge completive = makeBoundaryEdge(core::ExtendedPOS::AuxAspectShimau, "しまう");
  const float binding_cost = scorer.wordCost(binding);
  const float completive_cost = scorer.wordCost(completive);

  binding.start = 1;
  binding.end += 1;
  completive.start = 1;
  completive.end += 1;
  EXPECT_FLOAT_EQ(scorer.wordCost(binding), binding_cost);
  EXPECT_FLOAT_EQ(scorer.wordCost(completive), completive_cost);
}

}  // namespace
}  // namespace suzume::analysis
