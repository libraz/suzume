#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

Suzume makeNagaraAnalyzer() {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  return Suzume(options);
}

TEST(NounNagaraNi, PreservesClosedParticleBoundaries) {
  auto analyzer = makeNagaraAnalyzer();
  const auto result = analyzer.analyze("涙ながらに訴えた");

  ASSERT_EQ(result.size(), 5U);
  EXPECT_EQ(result[0].surface, "涙");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[1].surface, "ながら");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(result[2].surface, "に");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(result[3].surface, "訴え");
  EXPECT_EQ(result[3].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[3].lemma, "訴える");
  EXPECT_EQ(result[4].surface, "た");
  EXPECT_EQ(result[4].pos, core::PartOfSpeech::Auxiliary);
}

// Before の the same sequence is an adnominal search unit rather than a noun
// plus a conjunctive particle, so it stays whole.
TEST(NounNagaraNi, KeepsAdnominalSearchUnitBeforeNo) {
  auto analyzer = makeNagaraAnalyzer();
  const auto result = analyzer.analyze("昔ながらの方法");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "昔ながら");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[1].surface, "の");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(result[2].surface, "方法");
}

TEST(NounNagaraNi, KeepsVerbRenyokeiConjunctiveParticle) {
  auto analyzer = makeNagaraAnalyzer();
  const auto result = analyzer.analyze("働きながら学ぶ");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "働き");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[1].surface, "ながら");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(result[2].surface, "学ぶ");
}

TEST(NounNagaraNi, KeepsNaAdjectiveConcessiveParticle) {
  auto analyzer = makeNagaraAnalyzer();
  const auto result = analyzer.analyze("残念ながら難しい");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "残念");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[1].surface, "ながら");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(result[2].surface, "難しい");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Adjective);
}

}  // namespace
}  // namespace suzume::analysis
