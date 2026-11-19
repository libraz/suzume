#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

Suzume makeDeverbalNounAnalyzer() {
  SuzumeOptions options;
  options.skip_bundled_user_dictionary = true;
  return Suzume(options);
}

TEST(DeverbalNounNaku, TreatsVerbRenyokeiAsNounBeforeIndependentNaku) {
  auto analyzer = makeDeverbalNounAnalyzer();
  const auto result = analyzer.analyze("休みなく働く");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "休み");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[0].lemma, "休み");
  EXPECT_EQ(result[1].surface, "なく");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[2].surface, "働く");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Verb);
}

TEST(DeverbalNounNaku, PreservesExistingHiraganaNominalAnalysis) {
  auto analyzer = makeDeverbalNounAnalyzer();
  const auto result = analyzer.analyze("たゆみなく進む");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "たゆみ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[1].surface, "なく");
  EXPECT_EQ(result[2].surface, "進む");
}

TEST(DeverbalNounNaku, KeepsNegativeAuxiliaryChainVerbal) {
  auto analyzer = makeDeverbalNounAnalyzer();
  const auto result = analyzer.analyze("食べなくて困る");

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[0].surface, "食べ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "食べる");
  EXPECT_EQ(result[1].surface, "なく");
  EXPECT_EQ(result[2].surface, "て");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(result[3].surface, "困る");
}

TEST(DeverbalNounNaku, TreatsVerbRenyokeiAsNounBeforeAdverbialParticle) {
  auto analyzer = makeDeverbalNounAnalyzer();
  const auto result = analyzer.analyze("答えだけを見る");

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[0].surface, "答え");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::NounVerbal);
  EXPECT_EQ(result[0].lemma, "答え");
  EXPECT_EQ(result[1].surface, "だけ");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::ParticleAdverbial);
  EXPECT_EQ(result[2].surface, "を");
  EXPECT_EQ(result[3].surface, "見る");
}

TEST(DeverbalNounNaku, DoesNotOverrideCompleteLexicalAdverb) {
  auto analyzer = makeDeverbalNounAnalyzer();
  const auto result = analyzer.analyze("必ずしも良いとは限らない");

  ASSERT_EQ(result.size(), 6U);
  EXPECT_EQ(result[0].surface, "必ずしも");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb);
  EXPECT_EQ(result[1].surface, "良い");
  EXPECT_EQ(result[2].surface, "と");
  EXPECT_EQ(result[3].surface, "は");
  EXPECT_EQ(result[4].surface, "限ら");
  EXPECT_EQ(result[5].surface, "ない");
}

TEST(DeverbalNounNaku, DoesNotCrossConnectiveOrFormalNounBoundary) {
  auto analyzer = makeDeverbalNounAnalyzer();

  const auto connective_result = analyzer.analyze("見てさえいない");
  ASSERT_EQ(connective_result.size(), 5U);
  EXPECT_EQ(connective_result[0].surface, "見");
  EXPECT_EQ(connective_result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(connective_result[1].surface, "て");
  EXPECT_EQ(connective_result[1].extended_pos, core::ExtendedPOS::ParticleConj);
  EXPECT_EQ(connective_result[2].surface, "さえ");
  EXPECT_EQ(connective_result[3].surface, "い");
  EXPECT_EQ(connective_result[3].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(connective_result[4].surface, "ない");

  const auto formal_noun_result = analyzer.analyze("読んでいるあいだに書く");
  ASSERT_EQ(formal_noun_result.size(), 6U);
  EXPECT_EQ(formal_noun_result[0].surface, "読ん");
  EXPECT_EQ(formal_noun_result[1].surface, "で");
  EXPECT_EQ(formal_noun_result[2].surface, "いる");
  EXPECT_EQ(formal_noun_result[2].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(formal_noun_result[3].surface, "あいだ");
  EXPECT_EQ(formal_noun_result[3].extended_pos, core::ExtendedPOS::NounFormal);
  EXPECT_EQ(formal_noun_result[4].surface, "に");
  EXPECT_EQ(formal_noun_result[5].surface, "書く");
}

TEST(DeverbalNounNaku, KeepsOtherProductiveAdverbialNominalizations) {
  auto analyzer = makeDeverbalNounAnalyzer();
  const auto result = analyzer.analyze("読みだけを比べる");

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[0].surface, "読み");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::NounVerbal);
  EXPECT_EQ(result[1].surface, "だけ");
  EXPECT_EQ(result[2].surface, "を");
  EXPECT_EQ(result[3].surface, "比べる");
}

}  // namespace
}  // namespace suzume::analysis
