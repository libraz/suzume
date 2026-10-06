#include "core/viterbi.h"

#include <gtest/gtest.h>

#include <string_view>

namespace suzume::core {
namespace {

// Minimal model of the Scorer concept Viterbi::solve is templated on: word,
// connection, and the two sentence-boundary costs.
struct ExtendedPosScorer {
  float wordCost(const LatticeEdge& edge) const { return edge.cost; }

  float bosCost(const LatticeEdge& /*edge*/) const { return 0.0F; }

  float eosCost(const LatticeEdge& /*edge*/, ExtendedPOS /*prev*/ = ExtendedPOS::Unknown,
                size_t /*prev_length*/ = 0) const {
    return 0.0F;
  }

  float connectionCost(const LatticeEdge& prev, const LatticeEdge& next) const {
    if (prev.extended_pos == ExtendedPOS::VerbRenyokei && next.extended_pos == ExtendedPOS::AuxTenseMasu) {
      return -10.0F;
    }
    return 0.0F;
  }
};

enum class IdentitySignal {
  Start,
  PartOfSpeech,
  FormalNoun,
  Origin,
  Lemma,
  Conjugation,
  Dictionary,
  VerifiedLemma,
};

struct IdentityScorer {
  IdentitySignal signal;

  float wordCost(const LatticeEdge& edge) const { return edge.cost; }
  float bosCost(const LatticeEdge&) const { return 0.0F; }
  float eosCost(const LatticeEdge&, ExtendedPOS = ExtendedPOS::Unknown, size_t = 0) const { return 0.0F; }

  float connectionCost(const LatticeEdge& prev, const LatticeEdge& next) const {
    if (next.surface != "tail") {
      return 0.0F;
    }
    bool preferred = false;
    switch (signal) {
      case IdentitySignal::Start:
        preferred = prev.start == 1;
        break;
      case IdentitySignal::PartOfSpeech:
        preferred = prev.pos == PartOfSpeech::Noun;
        break;
      case IdentitySignal::FormalNoun:
        preferred = prev.isFormalNoun();
        break;
      case IdentitySignal::Origin:
        preferred = prev.origin == CandidateOrigin::Dictionary;
        break;
      case IdentitySignal::Lemma:
        preferred = prev.lemma == "target";
        break;
      case IdentitySignal::Conjugation:
        preferred = prev.conj_type == dictionary::ConjugationType::GodanKa;
        break;
      case IdentitySignal::Dictionary:
        preferred = prev.fromDictionary();
        break;
      case IdentitySignal::VerifiedLemma:
        preferred = prev.lemmaVerified();
        break;
    }
    return preferred ? -10.0F : 0.0F;
  }
};

void expectIdentitySensitiveAlternativeWins(IdentitySignal signal, std::string_view cheap_lemma,
                                            std::string_view preferred_lemma, dictionary::ConjugationType cheap_conj,
                                            dictionary::ConjugationType preferred_conj, CandidateOrigin cheap_origin,
                                            CandidateOrigin preferred_origin, uint8_t cheap_flags,
                                            uint8_t preferred_flags, PartOfSpeech cheap_pos = PartOfSpeech::Verb,
                                            PartOfSpeech preferred_pos = PartOfSpeech::Verb) {
  Lattice lattice(2);
  const auto cheap = lattice.addEdge("same", 0, 1, cheap_pos, 0.0F, cheap_flags, cheap_lemma, cheap_conj, cheap_origin,
                                     0.0F, {}, ExtendedPOS::VerbRenyokei);
  const auto preferred = lattice.addEdge("same", 0, 1, preferred_pos, 1.0F, preferred_flags, preferred_lemma,
                                         preferred_conj, preferred_origin, 0.0F, {}, ExtendedPOS::VerbRenyokei);
  lattice.addEdge("tail", 1, 2, PartOfSpeech::Auxiliary, 0.0F, 0, {}, dictionary::ConjugationType::None,
                  CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::AuxTenseMasu);

  const auto result = Viterbi{}.solve(lattice, IdentityScorer{signal});
  ASSERT_EQ(result.path.size(), 2U);
  EXPECT_NE(result.path[0], cheap);
  EXPECT_EQ(result.path[0], preferred);
}

TEST(ViterbiTest, DedupPreservesPartOfSpeechDependentAlternative) {
  expectIdentitySensitiveAlternativeWins(IdentitySignal::PartOfSpeech, "same", "same",
                                         dictionary::ConjugationType::None, dictionary::ConjugationType::None,
                                         CandidateOrigin::Unknown, CandidateOrigin::Unknown, 0, 0, PartOfSpeech::Verb,
                                         PartOfSpeech::Noun);
}

TEST(ViterbiTest, DedupPreservesFormalNounDependentAlternative) {
  expectIdentitySensitiveAlternativeWins(IdentitySignal::FormalNoun, "same", "same", dictionary::ConjugationType::None,
                                         dictionary::ConjugationType::None, CandidateOrigin::Unknown,
                                         CandidateOrigin::Unknown, 0, static_cast<uint8_t>(EdgeFlags::IsFormalNoun),
                                         PartOfSpeech::Noun, PartOfSpeech::Noun);
}

TEST(ViterbiTest, DedupPreservesStartDependentAlternative) {
  Lattice lattice(3);
  lattice.addEdge("prefix", 0, 1, PartOfSpeech::Other, 0.0F, 0, {}, dictionary::ConjugationType::None,
                  CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::Other);
  const auto cheap = lattice.addEdge("same", 0, 2, PartOfSpeech::Verb, 0.0F, 0, {}, dictionary::ConjugationType::None,
                                     CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::VerbRenyokei);
  const auto preferred =
      lattice.addEdge("same", 1, 2, PartOfSpeech::Verb, 1.0F, 0, {}, dictionary::ConjugationType::None,
                      CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::VerbRenyokei);
  lattice.addEdge("tail", 2, 3, PartOfSpeech::Auxiliary, 0.0F, 0, {}, dictionary::ConjugationType::None,
                  CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::AuxTenseMasu);

  const auto result = Viterbi{}.solve(lattice, IdentityScorer{IdentitySignal::Start});
  ASSERT_EQ(result.path.size(), 3U);
  EXPECT_NE(result.path[1], cheap);
  EXPECT_EQ(result.path[1], preferred);
}

TEST(ViterbiTest, DedupPreservesOriginDependentAlternative) {
  expectIdentitySensitiveAlternativeWins(IdentitySignal::Origin, "same", "same", dictionary::ConjugationType::None,
                                         dictionary::ConjugationType::None, CandidateOrigin::VerbHiragana,
                                         CandidateOrigin::Dictionary, 0, 0);
}

TEST(ViterbiTest, DedupPreservesLemmaDependentAlternative) {
  expectIdentitySensitiveAlternativeWins(IdentitySignal::Lemma, "other", "target", dictionary::ConjugationType::None,
                                         dictionary::ConjugationType::None, CandidateOrigin::Unknown,
                                         CandidateOrigin::Unknown, 0, 0);
}

TEST(ViterbiTest, DedupPreservesConjugationDependentAlternative) {
  expectIdentitySensitiveAlternativeWins(IdentitySignal::Conjugation, "same", "same",
                                         dictionary::ConjugationType::Ichidan, dictionary::ConjugationType::GodanKa,
                                         CandidateOrigin::Unknown, CandidateOrigin::Unknown, 0, 0);
}

TEST(ViterbiTest, DedupPreservesDictionaryProvenanceAlternative) {
  expectIdentitySensitiveAlternativeWins(IdentitySignal::Dictionary, "same", "same", dictionary::ConjugationType::None,
                                         dictionary::ConjugationType::None, CandidateOrigin::Unknown,
                                         CandidateOrigin::Unknown, 0, static_cast<uint8_t>(EdgeFlags::FromDictionary));
}

TEST(ViterbiTest, DedupPreservesVerifiedLemmaAlternative) {
  expectIdentitySensitiveAlternativeWins(IdentitySignal::VerifiedLemma, "same", "same",
                                         dictionary::ConjugationType::None, dictionary::ConjugationType::None,
                                         CandidateOrigin::Unknown, CandidateOrigin::Unknown, 0,
                                         static_cast<uint8_t>(EdgeFlags::LemmaVerified));
}

struct BeamBoundaryScorer {
  float wordCost(const LatticeEdge& edge) const { return edge.cost; }
  float bosCost(const LatticeEdge&) const { return 0.0F; }
  float eosCost(const LatticeEdge&, ExtendedPOS = ExtendedPOS::Unknown, size_t = 0) const { return 0.0F; }
  float connectionCost(const LatticeEdge& prev, const LatticeEdge& next) const {
    return prev.surface == "second" && next.surface == "tail" ? -100.0F : 0.0F;
  }
};

TEST(ViterbiTest, BeamKeepsExactlyTwoLowestCostAlternativesPerKey) {
  Lattice lattice(2);
  const auto first = lattice.addEdge("first", 0, 1, PartOfSpeech::Verb, 0.0F, 0, {}, dictionary::ConjugationType::None,
                                     CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::VerbRenyokei);
  const auto second =
      lattice.addEdge("second", 0, 1, PartOfSpeech::Verb, 1.0F, 0, {}, dictionary::ConjugationType::None,
                      CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::VerbRenyokei);
  lattice.addEdge("third", 0, 1, PartOfSpeech::Verb, 2.0F, 0, {}, dictionary::ConjugationType::None,
                  CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::VerbRenyokei);
  lattice.addEdge("tail", 1, 2, PartOfSpeech::Auxiliary, 0.0F, 0, {}, dictionary::ConjugationType::None,
                  CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::AuxTenseMasu);

  const auto result = Viterbi{}.solve(lattice, BeamBoundaryScorer{});
  ASSERT_EQ(result.path.size(), 2U);
  EXPECT_NE(result.path[0], first);
  EXPECT_EQ(result.path[0], second);
}

TEST(ViterbiTest, KeepsDistinctExtendedPosStatesForSamePosAndEnd) {
  Lattice lattice(2);
  const auto onbin_id = lattice.addEdge("a", 0, 1, PartOfSpeech::Verb, 0.0F, 0, {}, dictionary::ConjugationType::None,
                                        CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::VerbOnbinkei);
  const auto renyokei_id =
      lattice.addEdge("b", 0, 1, PartOfSpeech::Verb, 1.0F, 0, {}, dictionary::ConjugationType::None,
                      CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::VerbRenyokei);
  const auto aux_id =
      lattice.addEdge("c", 1, 2, PartOfSpeech::Auxiliary, 0.0F, 0, {}, dictionary::ConjugationType::None,
                      CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::AuxTenseMasu);

  Viterbi viterbi;
  auto result = viterbi.solve(lattice, ExtendedPosScorer{});

  ASSERT_EQ(result.path.size(), 2u);
  EXPECT_NE(result.path[0], onbin_id);
  EXPECT_EQ(result.path[0], renyokei_id);
  EXPECT_EQ(result.path[1], aux_id);
}

// Boundary costs are supplied by the scorer, so Viterbi must apply bosCost only
// to an edge leaving the BOS state and eosCost only to one ending the sentence.
struct BoundaryScorer {
  float wordCost(const LatticeEdge& edge) const { return edge.cost; }

  float bosCost(const LatticeEdge& edge) const {
    return edge.extended_pos == ExtendedPOS::ParticleTopic ? kBoundaryPenalty : 0.0F;
  }

  float eosCost(const LatticeEdge& edge, ExtendedPOS /*prev*/ = ExtendedPOS::Unknown,
                size_t /*prev_length*/ = 0) const {
    return edge.extended_pos == ExtendedPOS::AuxAspectKuru ? kBoundaryPenalty : 0.0F;
  }

  float connectionCost(const LatticeEdge& /*prev*/, const LatticeEdge& /*next*/) const { return 0.0F; }

  static constexpr float kBoundaryPenalty = 10.0F;
};

TEST(ViterbiTest, AppliesScorerBosCostToTheSentenceOpeningEdge) {
  Lattice lattice(2);
  const auto topic_id =
      lattice.addEdge("a", 0, 1, PartOfSpeech::Particle, 0.0F, 0, {}, dictionary::ConjugationType::None,
                      CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::ParticleTopic);
  const auto verb_id = lattice.addEdge("b", 0, 1, PartOfSpeech::Verb, 1.0F, 0, {}, dictionary::ConjugationType::None,
                                       CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::VerbRenyokei);
  lattice.addEdge("c", 1, 2, PartOfSpeech::Noun, 0.0F, 0, {}, dictionary::ConjugationType::None,
                  CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::Noun);

  Viterbi viterbi;
  auto result = viterbi.solve(lattice, BoundaryScorer{});

  // The topic particle is cheaper by word cost but cannot open a sentence.
  ASSERT_EQ(result.path.size(), 2U);
  EXPECT_EQ(result.path[0], verb_id);
  EXPECT_NE(result.path[0], topic_id);
}

TEST(ViterbiTest, AppliesScorerEosCostOnlyToTheSentenceClosingEdge) {
  Lattice lattice(2);
  lattice.addEdge("a", 0, 1, PartOfSpeech::Noun, 0.0F, 0, {}, dictionary::ConjugationType::None,
                  CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::Noun);
  const auto aspect_id =
      lattice.addEdge("b", 1, 2, PartOfSpeech::Auxiliary, 0.0F, 0, {}, dictionary::ConjugationType::None,
                      CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::AuxAspectKuru);
  const auto noun_id = lattice.addEdge("c", 1, 2, PartOfSpeech::Noun, 1.0F, 0, {}, dictionary::ConjugationType::None,
                                       CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::Noun);

  Viterbi viterbi;
  auto result = viterbi.solve(lattice, BoundaryScorer{});

  ASSERT_EQ(result.path.size(), 2U);
  EXPECT_EQ(result.path[1], noun_id);
  EXPECT_NE(result.path[1], aspect_id);
}

TEST(ViterbiTest, LeavesTheSameAspectEdgeAloneWhenItDoesNotEndTheSentence) {
  Lattice lattice(2);
  const auto aspect_id =
      lattice.addEdge("a", 0, 1, PartOfSpeech::Auxiliary, 0.0F, 0, {}, dictionary::ConjugationType::None,
                      CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::AuxAspectKuru);
  const auto noun_id = lattice.addEdge("b", 0, 1, PartOfSpeech::Noun, 1.0F, 0, {}, dictionary::ConjugationType::None,
                                       CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::Noun);
  lattice.addEdge("c", 1, 2, PartOfSpeech::Noun, 0.0F, 0, {}, dictionary::ConjugationType::None,
                  CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::Noun);

  Viterbi viterbi;
  auto result = viterbi.solve(lattice, BoundaryScorer{});

  // Same ExtendedPOS as the penalized edge above, but it is followed by a noun,
  // so no EOS cost applies and its lower word cost wins.
  ASSERT_EQ(result.path.size(), 2U);
  EXPECT_EQ(result.path[0], aspect_id);
  EXPECT_NE(result.path[0], noun_id);
}

}  // namespace
}  // namespace suzume::core
