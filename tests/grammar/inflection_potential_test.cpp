// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Inflection tests: Potential forms, Potential negative/past,
// Potential + naru, Potential form ambiguity

#include "grammar/inflection.h"

#include <gtest/gtest.h>

namespace suzume::grammar {
namespace {

class InflectionPotentialTest : public ::testing::Test {
 protected:
  Inflection inflection_;
};

// ===== Potential negative/past forms =====

TEST_F(InflectionPotentialTest, PotentialNegativePastKaRow) {
  auto result = inflection_.getBest("書けなかった");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionPotentialTest, PotentialNegativePastMaRow) {
  auto result = inflection_.getBest("読めなかった");
  EXPECT_EQ(result.base_form, "読む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionPotentialTest, PotentialNegativePastWaRow) {
  auto result = inflection_.getBest("もらえなかった");
  EXPECT_EQ(result.base_form, "もらう");
  EXPECT_EQ(result.verb_type, VerbType::GodanWa);
}

TEST_F(InflectionPotentialTest, PotentialPoliteNegativePast) {
  auto result = inflection_.getBest("書けませんでした");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

// ===== Potential + naru patterns =====

TEST_F(InflectionPotentialTest, PotentialNaruGodanMa) {
  auto result = inflection_.getBest("読めるようになった");
  EXPECT_EQ(result.base_form, "読む");
}

TEST_F(InflectionPotentialTest, PotentialNaruTeKita) {
  auto result = inflection_.getBest("書けるようになってきた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionPotentialTest, PotentialNegativeNaruTeShimatta) {
  auto result = inflection_.getBest("話せなくなってしまった");
  EXPECT_EQ(result.base_form, "話す");
}

// ===== Potential form ambiguity =====

TEST_F(InflectionPotentialTest, GodanPotentialVsIchidan_KaRow) {
  auto result = inflection_.getBest("書けない");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionPotentialTest, GodanPotentialVsIchidan_MaRow) {
  auto result = inflection_.getBest("読めない");
  EXPECT_EQ(result.base_form, "読む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionPotentialTest, GodanPotentialVsIchidan_SaRow) {
  auto result = inflection_.getBest("話せない");
  EXPECT_EQ(result.base_form, "話す");
  EXPECT_EQ(result.verb_type, VerbType::GodanSa);
}

// Ensure valid Ichidan verbs are not incorrectly analyzed as Godan potential

TEST_F(InflectionPotentialTest, IchidanNotMistakenForPotential_Taberu) {
  auto result = inflection_.getBest("食べない");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionPotentialTest, IchidanNotMistakenForPotential_Kangaeru) {
  auto result = inflection_.getBest("考えない");
  EXPECT_EQ(result.base_form, "考える");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionPotentialTest, IchidanNotMistakenForPotential_Kotaeru) {
  auto result = inflection_.getBest("答えない");
  EXPECT_EQ(result.base_form, "答える");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Passive/Potential negative te-form =====

TEST_F(InflectionPotentialTest, PassivePotentialNegativeTe_Ichidan) {
  auto result = inflection_.getBest("食べられなくて");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionPotentialTest, CausativeNegativeTe_Ichidan) {
  auto result = inflection_.getBest("食べさせなくて");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

}  // namespace
}  // namespace suzume::grammar
