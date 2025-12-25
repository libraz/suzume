// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Inflection tests: Basic conjugations, Passive, Causative, Iku irregular,
// I-adjective, Analyze, LooksConjugated, Honorific

#include "grammar/inflection.h"

#include <gtest/gtest.h>

namespace suzume::grammar {
namespace {

class InflectionBasicTest : public ::testing::Test {
 protected:
  Inflection inflection_;
};

// ===== Basic verb conjugations =====

TEST_F(InflectionBasicTest, GodanVerbTeForm) {
  auto result = inflection_.getBest("書いて");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, GodanVerbTaForm) {
  auto result = inflection_.getBest("読んだ");
  EXPECT_EQ(result.base_form, "読む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionBasicTest, IchidanVerbTeForm) {
  auto result = inflection_.getBest("食べている");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Passive forms =====

TEST_F(InflectionBasicTest, GodanPassiveForm) {
  auto result = inflection_.getBest("奪われた");
  EXPECT_EQ(result.base_form, "奪う");
  EXPECT_EQ(result.verb_type, VerbType::GodanWa);
}

TEST_F(InflectionBasicTest, IchidanPassiveForm) {
  auto result = inflection_.getBest("見られた");
  EXPECT_EQ(result.base_form, "見る");
}

// ===== Causative forms =====

TEST_F(InflectionBasicTest, GodanCausativeForm) {
  auto result = inflection_.getBest("書かせた");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IchidanCausativeForm) {
  auto result = inflection_.getBest("食べさせている");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Causative-passive forms =====

TEST_F(InflectionBasicTest, IchidanCausativePassiveForm) {
  auto result = inflection_.getBest("食べさせられた");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionBasicTest, IchidanCausativePassiveFormMiru) {
  auto result = inflection_.getBest("見させられた");
  EXPECT_EQ(result.base_form, "見る");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Iku irregular verb =====

TEST_F(InflectionBasicTest, IkuTeForm) {
  auto result = inflection_.getBest("いって");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTaForm) {
  auto result = inflection_.getBest("いった");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTeIruForm) {
  auto result = inflection_.getBest("いっている");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTeShimattaForm) {
  auto result = inflection_.getBest("いってしまった");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTeKitaForm) {
  auto result = inflection_.getBest("いってきた");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTeMitaForm) {
  auto result = inflection_.getBest("いってみた");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

// ===== I-adjective patterns =====

TEST_F(InflectionBasicTest, IAdjPastForm) {
  auto result = inflection_.getBest("美しかった");
  EXPECT_EQ(result.base_form, "美しい");
  EXPECT_EQ(result.verb_type, VerbType::IAdjective);
}

TEST_F(InflectionBasicTest, IAdjNegativeForm) {
  auto result = inflection_.getBest("美しくない");
  EXPECT_EQ(result.base_form, "美しい");
  EXPECT_EQ(result.verb_type, VerbType::IAdjective);
}

// ===== Analyze returns multiple candidates =====

TEST_F(InflectionBasicTest, AnalyzeReturnsMultipleCandidates) {
  auto candidates = inflection_.analyze("書いた");
  EXPECT_GT(candidates.size(), 1);
  EXPECT_EQ(candidates[0].base_form, "書く");
}

TEST_F(InflectionBasicTest, AnalyzeSortsByConfidence) {
  auto candidates = inflection_.analyze("作ってみた");
  EXPECT_GT(candidates.size(), 1);
  for (size_t idx = 1; idx < candidates.size(); ++idx) {
    EXPECT_GE(candidates[idx - 1].confidence, candidates[idx].confidence);
  }
}

// ===== LooksConjugated =====

TEST_F(InflectionBasicTest, LooksConjugatedTrue) {
  EXPECT_TRUE(inflection_.looksConjugated("食べた"));
  EXPECT_TRUE(inflection_.looksConjugated("書いている"));
  EXPECT_TRUE(inflection_.looksConjugated("読めなかった"));
}

TEST_F(InflectionBasicTest, LooksConjugatedFalse) {
  EXPECT_FALSE(inflection_.looksConjugated("あ"));
  EXPECT_FALSE(inflection_.looksConjugated(""));
}

// ===== Honorific verb forms =====

TEST_F(InflectionBasicTest, HonorificIrasshattaForm) {
  auto result = inflection_.getBest("いらっしゃった");
  EXPECT_EQ(result.base_form, "いらっしゃる");
}

TEST_F(InflectionBasicTest, HonorificOsshatteitaForm) {
  auto result = inflection_.getBest("おっしゃっていた");
  EXPECT_EQ(result.base_form, "おっしゃる");
}

TEST_F(InflectionBasicTest, HonorificKudasattaForm) {
  auto result = inflection_.getBest("くださった");
  EXPECT_EQ(result.base_form, "くださる");
}

TEST_F(InflectionBasicTest, HonorificNasattaForm) {
  auto result = inflection_.getBest("なさった");
  EXPECT_EQ(result.base_form, "なさる");
}

// ===== Negative progressive forms =====

TEST_F(InflectionBasicTest, NegativeProgressiveIchidan) {
  auto result = inflection_.getBest("食べないでいた");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionBasicTest, NegativeProgressiveGodanKa) {
  auto result = inflection_.getBest("書かないでいた");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, NegativeProgressiveSuru) {
  auto result = inflection_.getBest("勉強しないでいた");
  EXPECT_EQ(result.base_form, "勉強する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

// ===== Suru verb renyokei =====

TEST_F(InflectionBasicTest, SuruRenyokeiBunkatsu) {
  auto result = inflection_.getBest("分割し");
  EXPECT_EQ(result.base_form, "分割する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

TEST_F(InflectionBasicTest, SuruRenyokeiBenkyo) {
  auto result = inflection_.getBest("勉強し");
  EXPECT_EQ(result.base_form, "勉強する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

// ===== Conditional form =====

TEST_F(InflectionBasicTest, ConditionalBa_TwoKanjiStem) {
  auto result = inflection_.getBest("頑張れば");
  EXPECT_EQ(result.base_form, "頑張る");
  EXPECT_EQ(result.verb_type, VerbType::GodanRa);
}

// ===== Suru passive negative past =====

TEST_F(InflectionBasicTest, SuruPassiveNegativePast) {
  auto result = inflection_.getBest("されなかった");
  EXPECT_EQ(result.base_form, "される");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionBasicTest, SuruPassiveNegativePast_Compound) {
  auto result = inflection_.getBest("開催されなかった");
  EXPECT_EQ(result.base_form, "開催する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

}  // namespace
}  // namespace suzume::grammar
