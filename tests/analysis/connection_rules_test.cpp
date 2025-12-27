/**
 * @file connection_rules_test.cpp
 * @brief Tests for connection rules module
 */

#include "analysis/connection_rules.h"

#include <gtest/gtest.h>

#include "analysis/scorer_constants.h"

namespace suzume::analysis {
namespace {

// Helper to create a test edge
core::LatticeEdge makeEdge(std::string_view surface, core::PartOfSpeech pos,
                           std::string_view lemma = "") {
  core::LatticeEdge edge;
  edge.surface = surface;
  edge.pos = pos;
  edge.lemma = lemma;
  return edge;
}

// =============================================================================
// Pattern Detection Tests
// =============================================================================

class PatternDetectionTest : public ::testing::Test {};

TEST_F(PatternDetectionTest, EndsWithIRow) {
  // Positive cases
  EXPECT_TRUE(endsWithIRow("み"));
  EXPECT_TRUE(endsWithIRow("書き"));
  EXPECT_TRUE(endsWithIRow("読み"));
  EXPECT_TRUE(endsWithIRow("話し"));
  EXPECT_TRUE(endsWithIRow("走り"));

  // Negative cases
  EXPECT_FALSE(endsWithIRow(""));
  EXPECT_FALSE(endsWithIRow("食べ"));  // e-row
  EXPECT_FALSE(endsWithIRow("く"));     // not i-row
  EXPECT_FALSE(endsWithIRow("書か"));  // a-row
}

TEST_F(PatternDetectionTest, EndsWithERow) {
  // Positive cases
  EXPECT_TRUE(endsWithERow("べ"));
  EXPECT_TRUE(endsWithERow("食べ"));
  EXPECT_TRUE(endsWithERow("教え"));
  EXPECT_TRUE(endsWithERow("見せ"));

  // Negative cases
  EXPECT_FALSE(endsWithERow(""));
  EXPECT_FALSE(endsWithERow("書き"));  // i-row
  EXPECT_FALSE(endsWithERow("読む"));  // u-row
}

TEST_F(PatternDetectionTest, EndsWithRenyokeiMarker) {
  // i-row (godan renyokei)
  EXPECT_TRUE(endsWithRenyokeiMarker("書き"));
  EXPECT_TRUE(endsWithRenyokeiMarker("読み"));

  // e-row (ichidan renyokei)
  EXPECT_TRUE(endsWithRenyokeiMarker("食べ"));
  EXPECT_TRUE(endsWithRenyokeiMarker("教え"));

  // Negative cases
  EXPECT_FALSE(endsWithRenyokeiMarker(""));
  EXPECT_FALSE(endsWithRenyokeiMarker("書く"));  // u-row
}

TEST_F(PatternDetectionTest, EndsWithOnbinMarker) {
  // Positive cases
  EXPECT_TRUE(endsWithOnbinMarker("書い"));   // イ音便
  EXPECT_TRUE(endsWithOnbinMarker("走っ"));   // 促音便
  EXPECT_TRUE(endsWithOnbinMarker("読ん"));   // 撥音便

  // Negative cases
  EXPECT_FALSE(endsWithOnbinMarker(""));
  EXPECT_FALSE(endsWithOnbinMarker("書き"));  // not onbin
  EXPECT_FALSE(endsWithOnbinMarker("食べ"));  // not onbin
}

TEST_F(PatternDetectionTest, EndsWithKuForm) {
  // Positive cases
  EXPECT_TRUE(endsWithKuForm("美しく"));
  EXPECT_TRUE(endsWithKuForm("高く"));
  EXPECT_TRUE(endsWithKuForm("く"));

  // Negative cases
  EXPECT_FALSE(endsWithKuForm(""));
  EXPECT_FALSE(endsWithKuForm("美しい"));
  EXPECT_FALSE(endsWithKuForm("高"));
}

TEST_F(PatternDetectionTest, StartsWithTe) {
  // Positive cases
  EXPECT_TRUE(startsWithTe("て"));
  EXPECT_TRUE(startsWithTe("てくれた"));
  EXPECT_TRUE(startsWithTe("てもらう"));
  EXPECT_TRUE(startsWithTe("で"));
  EXPECT_TRUE(startsWithTe("でいる"));

  // Negative cases
  EXPECT_FALSE(startsWithTe(""));
  EXPECT_FALSE(startsWithTe("た"));
  // Note: startsWithTe("ている") is TRUE because it starts with "て"
}

TEST_F(PatternDetectionTest, EndsWithSou) {
  // Positive cases
  EXPECT_TRUE(endsWithSou("食べそう"));
  EXPECT_TRUE(endsWithSou("しそう"));
  EXPECT_TRUE(endsWithSou("そう"));

  // Negative cases (too short)
  EXPECT_FALSE(endsWithSou(""));
  EXPECT_FALSE(endsWithSou("そ"));
  EXPECT_FALSE(endsWithSou("う"));
}

// =============================================================================
// Connection Rule Tests
// =============================================================================

class ConnectionRuleTest : public ::testing::Test {};

TEST_F(ConnectionRuleTest, CopulaAfterVerb_Penalty) {
  // Verb + だ should be penalized
  auto prev = makeEdge("食べた", core::PartOfSpeech::Verb);
  auto next = makeEdge("だ", core::PartOfSpeech::Auxiliary);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::CopulaAfterVerb);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltyCopulaAfterVerb);
}

TEST_F(ConnectionRuleTest, CopulaAfterVerb_SouException) {
  // Verb ending with そう + です should not be penalized
  auto prev = makeEdge("食べそう", core::PartOfSpeech::Verb);
  auto next = makeEdge("です", core::PartOfSpeech::Auxiliary);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::None);
  EXPECT_FLOAT_EQ(result.adjustment, 0.0F);
}

TEST_F(ConnectionRuleTest, IchidanRenyokeiTe_Penalty) {
  // Ichidan renyokei + て (particle) should be penalized
  auto prev = makeEdge("食べ", core::PartOfSpeech::Verb);
  auto next = makeEdge("て", core::PartOfSpeech::Particle);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::IchidanRenyokeiTe);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltyIchidanRenyokeiTe);
}

TEST_F(ConnectionRuleTest, IchidanRenyokeiTe_VerbCompound) {
  // Ichidan renyokei + てくれた (verb) should be penalized
  auto prev = makeEdge("教え", core::PartOfSpeech::Verb);
  auto next = makeEdge("てくれた", core::PartOfSpeech::Verb);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::IchidanRenyokeiTe);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltyIchidanRenyokeiTe);
}

TEST_F(ConnectionRuleTest, TeFormSplit_GodanOnbin) {
  // Godan onbin + て should be penalized
  auto prev = makeEdge("書い", core::PartOfSpeech::Noun);
  auto next = makeEdge("て", core::PartOfSpeech::Particle);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::TeFormSplit);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltyTeFormSplit);
}

TEST_F(ConnectionRuleTest, TaiAfterRenyokei_Bonus) {
  // Verb renyokei + たく (adjective with lemma たい) should get bonus
  auto prev = makeEdge("読み", core::PartOfSpeech::Verb);
  auto next = makeEdge("たくない", core::PartOfSpeech::Adjective, "たい");

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::TaiAfterRenyokei);
  EXPECT_FLOAT_EQ(result.adjustment, -scorer::kBonusTaiAfterRenyokei);
}

TEST_F(ConnectionRuleTest, YasuiAfterRenyokei_Penalty) {
  // Noun that looks like renyokei + やすい (安い) should be penalized
  auto prev = makeEdge("読み", core::PartOfSpeech::Noun);
  auto next = makeEdge("やすい", core::PartOfSpeech::Adjective, "安い");

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::YasuiAfterRenyokei);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltyYasuiAfterRenyokei);
}

TEST_F(ConnectionRuleTest, NagaraSplit_Penalty) {
  // Verb renyokei + ながら should be penalized
  auto prev = makeEdge("飲み", core::PartOfSpeech::Verb);
  auto next = makeEdge("ながら", core::PartOfSpeech::Particle);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::NagaraSplit);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltyNagaraSplit);
}

TEST_F(ConnectionRuleTest, SouAfterRenyokei_Penalty) {
  // Noun that looks like renyokei + そう should be penalized
  auto prev = makeEdge("話し", core::PartOfSpeech::Noun);
  auto next = makeEdge("そう", core::PartOfSpeech::Adverb);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::SouAfterRenyokei);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltySouAfterRenyokei);
}

TEST_F(ConnectionRuleTest, CharacterSpeechSplit_Penalty) {
  // だ + にゃ should be penalized
  auto prev = makeEdge("だ", core::PartOfSpeech::Auxiliary);
  auto next = makeEdge("にゃ", core::PartOfSpeech::Auxiliary);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::CharacterSpeechSplit);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltyCharacterSpeechSplit);
}

TEST_F(ConnectionRuleTest, AdjKuNaru_Bonus) {
  // 美しく + なる should get bonus
  auto prev = makeEdge("美しく", core::PartOfSpeech::Adjective);
  auto next = makeEdge("なる", core::PartOfSpeech::Verb, "なる");

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::AdjKuNaru);
  EXPECT_FLOAT_EQ(result.adjustment, -scorer::kBonusAdjKuNaru);
}

TEST_F(ConnectionRuleTest, CompoundAuxAfterRenyokei_Penalty) {
  // Noun that looks like renyokei + 終わる should be penalized
  auto prev = makeEdge("読み", core::PartOfSpeech::Noun);
  auto next = makeEdge("終わる", core::PartOfSpeech::Verb);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::CompoundAuxAfterRenyokei);
  EXPECT_FLOAT_EQ(result.adjustment, scorer::kPenaltyCompoundAuxAfterRenyokei);
}

TEST_F(ConnectionRuleTest, NoMatch_ReturnsNone) {
  // Normal connection should return None
  auto prev = makeEdge("本", core::PartOfSpeech::Noun);
  auto next = makeEdge("を", core::PartOfSpeech::Particle);

  auto result = evaluateConnectionRules(prev, next);
  EXPECT_EQ(result.pattern, ConnectionPattern::None);
  EXPECT_FLOAT_EQ(result.adjustment, 0.0F);
  EXPECT_EQ(result.description, nullptr);
}

}  // namespace
}  // namespace suzume::analysis
