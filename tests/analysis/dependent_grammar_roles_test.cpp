#include <gtest/gtest.h>

#include <string_view>

#include "suzume.h"

namespace suzume::analysis {
namespace {

Suzume makeDependentGrammarAnalyzer() {
  SuzumeOptions options;
  options.skip_bundled_user_dictionary = true;
  return Suzume(options);
}

TEST(DependentGrammarRoles, RetagsDirectionalIkuAfterConnectiveParticle) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("話していく");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "話し");
  EXPECT_EQ(result[1].surface, "て");
  EXPECT_EQ(result[2].surface, "いく");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxAspectIku);
  EXPECT_EQ(result[2].lemma, "いく");
}

TEST(DependentGrammarRoles, KeepsIndependentIkuVerbal) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("学校にいく");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[2].surface, "いく");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Verb);
}

TEST(DependentGrammarRoles, KeepsCaseParticleBeforeIndependentPredicates) {
  auto analyzer = makeDependentGrammarAnalyzer();

  const auto katakana_result = analyzer.analyze("バスでいく");
  ASSERT_EQ(katakana_result.size(), 3U);
  EXPECT_EQ(katakana_result[0].surface, "バス");
  EXPECT_EQ(katakana_result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(katakana_result[1].surface, "で");
  EXPECT_EQ(katakana_result[1].extended_pos, core::ExtendedPOS::ParticleCase);
  EXPECT_EQ(katakana_result[2].surface, "いく");
  EXPECT_EQ(katakana_result[2].pos, core::PartOfSpeech::Verb);

  const auto pronoun_result = analyzer.analyze("そちらでやる");
  ASSERT_EQ(pronoun_result.size(), 3U);
  EXPECT_EQ(pronoun_result[0].surface, "そちら");
  EXPECT_EQ(pronoun_result[0].pos, core::PartOfSpeech::Pronoun);
  EXPECT_EQ(pronoun_result[1].surface, "で");
  EXPECT_EQ(pronoun_result[1].extended_pos, core::ExtendedPOS::ParticleCase);
  EXPECT_EQ(pronoun_result[2].surface, "やる");
  EXPECT_EQ(pronoun_result[2].pos, core::PartOfSpeech::Verb);
}

TEST(DependentGrammarRoles, KeepsOnbinConnectiveAndAuxiliaryBoundaries) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("本を読んでみる");

  ASSERT_EQ(result.size(), 5U);
  EXPECT_EQ(result[2].surface, "読ん");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::VerbOnbinkei);
  EXPECT_EQ(result[3].surface, "で");
  EXPECT_EQ(result[3].extended_pos, core::ExtendedPOS::ParticleConj);
  EXPECT_EQ(result[4].surface, "みる");
  EXPECT_EQ(result[4].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[4].extended_pos, core::ExtendedPOS::AuxAspectMiru);
}

TEST(DependentGrammarRoles, SelectsPastConditionalAfterIOnbin) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("泣いたら進む");

  ASSERT_GE(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "泣い");
  EXPECT_EQ(result[1].surface, "たら");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxTenseTa);
  EXPECT_EQ(result[1].lemma, "た");
}

TEST(DependentGrammarRoles, SelectsPastConditionalBeforeFollowingNounClause) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("泣いたら子供が進む");

  ASSERT_GE(result.size(), 5U);
  EXPECT_EQ(result[0].surface, "泣い");
  EXPECT_EQ(result[1].surface, "たら");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxTenseTa);
  EXPECT_EQ(result[1].lemma, "た");
}

TEST(DependentGrammarRoles, SelectsPastConditionalAfterNasalOnbin) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("読んだら進む");

  ASSERT_GE(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "読ん");
  EXPECT_EQ(result[1].surface, "だら");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxTenseTa);
}

TEST(DependentGrammarRoles, KeepsNegativeIntentQuotativeSuruBoundaries) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("忘れるまいとして進む");

  ASSERT_GE(result.size(), 6U);
  EXPECT_EQ(result[0].surface, "忘れる");
  EXPECT_EQ(result[1].surface, "まい");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxNegativeMai);
  EXPECT_EQ(result[2].surface, "と");
  EXPECT_EQ(result[3].surface, "し");
  EXPECT_EQ(result[4].surface, "て");
}

TEST(DependentGrammarRoles, NegativeIntentBeatsOverlappingAdjective) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("言うまいとして黙る");

  ASSERT_GE(result.size(), 6U);
  EXPECT_EQ(result[0].surface, "言う");
  EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::VerbShuushikei);
  EXPECT_EQ(result[1].surface, "まい");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxNegativeMai);
  EXPECT_EQ(result[2].surface, "と");
  EXPECT_EQ(result[3].surface, "し");
  EXPECT_EQ(result[4].surface, "て");
}

TEST(DependentGrammarRoles, KeepsProductiveAudienceSuffixBoundaryForOpenHosts) {
  auto analyzer = makeDependentGrammarAnalyzer();
  for (const std::string_view input : {"家庭向けの製品", "初心者向けの説明", "読者向けの記事"}) {
    const auto result = analyzer.analyze(input);

    ASSERT_GE(result.size(), 3U) << input;
    EXPECT_EQ(result[1].surface, "向け") << input;
    EXPECT_EQ(result[1].pos, core::PartOfSpeech::Suffix) << input;
    EXPECT_EQ(result[2].surface, "の") << input;
  }
}

TEST(DependentGrammarRoles, RetagsFocusedGodanContinuativeWithoutLexiconEntry) {
  auto analyzer = makeDependentGrammarAnalyzer();
  const auto result = analyzer.analyze("売上が減りはしなかった");

  ASSERT_GE(result.size(), 5U);
  EXPECT_EQ(result[2].surface, "減り");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[2].lemma, "減る");
  EXPECT_EQ(result[3].surface, "は");
  EXPECT_EQ(result[4].lemma, "する");
}

}  // namespace
}  // namespace suzume::analysis
