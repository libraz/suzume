#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

Suzume makeContractedCompletiveAnalyzer() {
  SuzumeOptions options;
  options.skip_bundled_user_dictionary = true;
  return Suzume(options);
}

TEST(ContractedCompletiveBoundary, KeepsUnvoicedPoliteAuxiliaryBoundary) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("食べちゃいます");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "食べ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[1].surface, "ちゃい");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxAspectShimau);
  EXPECT_EQ(result[1].lemma, "ちゃう");
  EXPECT_EQ(result[2].surface, "ます");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxTenseMasu);
}

TEST(ContractedCompletiveBoundary, KeepsVoicedPoliteAuxiliaryBoundary) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("遊んじゃいます");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "遊ん");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[1].surface, "じゃい");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxAspectShimau);
  EXPECT_EQ(result[1].lemma, "じゃう");
  EXPECT_EQ(result[2].surface, "ます");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxTenseMasu);
}

TEST(ContractedCompletiveBoundary, KeepsPoliteRenyokeiTypeBeforePast) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("食べちゃいました");

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[0].surface, "食べ");
  EXPECT_EQ(result[1].surface, "ちゃい");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxAspectShimau);
  EXPECT_EQ(result[2].surface, "まし");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxTenseMasu);
  EXPECT_EQ(result[2].lemma, "ます");
  EXPECT_EQ(result[3].surface, "た");
  EXPECT_EQ(result[3].extended_pos, core::ExtendedPOS::AuxTenseTa);
}

TEST(ContractedCompletiveBoundary, KeepsContractedCompletivePoliteNegative) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("食べちゃいません");

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[0].surface, "食べ");
  EXPECT_EQ(result[1].surface, "ちゃい");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxAspectShimau);
  EXPECT_EQ(result[2].surface, "ませ");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxTenseMasu);
  EXPECT_EQ(result[3].surface, "ん");
  EXPECT_EQ(result[3].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[3].extended_pos, core::ExtendedPOS::AuxNegativeNu);
}

TEST(ContractedCompletiveBoundary, KeepsOrdinaryPoliteNegativeChain) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("食べません");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "食べ");
  EXPECT_EQ(result[1].surface, "ませ");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxTenseMasu);
  EXPECT_EQ(result[2].surface, "ん");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxNegativeNu);
}

TEST(ContractedCompletiveBoundary, PreservesPoliteVolitionalChain) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("食べましょう");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "食べ");
  EXPECT_EQ(result[1].surface, "ましょ");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxTenseMasu);
  EXPECT_EQ(result[2].surface, "う");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxVolitional);
}

TEST(ContractedCompletiveBoundary, PreservesLexicalVolitionalChain) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("食べよう");

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].surface, "食べよ");
  EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::VerbMizenkei);
  EXPECT_EQ(result[1].surface, "う");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxVolitional);
}

TEST(ContractedCompletiveBoundary, LeavesIndependentTeaPhraseUntouched) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("お茶を買います");

  ASSERT_EQ(result.size(), 5U);
  EXPECT_EQ(result[0].surface, "お");
  EXPECT_EQ(result[1].surface, "茶");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[2].surface, "を");
  EXPECT_EQ(result[3].surface, "買い");
  EXPECT_EQ(result[3].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[4].surface, "ます");
  EXPECT_EQ(result[4].extended_pos, core::ExtendedPOS::AuxTenseMasu);
}

TEST(ContractedCompletiveBoundary, PreservesVerifiedCompoundGuard) {
  auto analyzer = makeContractedCompletiveAnalyzer();
  const auto result = analyzer.analyze("思い出します");

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].surface, "思い出し");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "思い出す");
  EXPECT_EQ(result[1].surface, "ます");
}

}  // namespace
}  // namespace suzume::analysis
