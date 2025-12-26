// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Inflection tests: Compound verb patterns (temiru, teshimau, temorau, etc.),
// Triple/Quadruple compounds, Passive/Causative + compound

#include "grammar/inflection.h"

#include <gtest/gtest.h>

namespace suzume::grammar {
namespace {

class InflectionCompoundTest : public ::testing::Test {
 protected:
  Inflection inflection_;
};

// ===== Basic compound verb patterns =====

TEST_F(InflectionCompoundTest, CompoundTeMita) {
  auto result = inflection_.getBest("作ってみた");
  EXPECT_EQ(result.base_form, "作る");
  EXPECT_EQ(result.verb_type, VerbType::GodanRa);
}

TEST_F(InflectionCompoundTest, CompoundTeShimatta) {
  auto result = inflection_.getBest("忘れてしまった");
  EXPECT_EQ(result.base_form, "忘れる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, CompoundTeOita) {
  auto result = inflection_.getBest("準備しておいた");
  EXPECT_EQ(result.base_form, "準備する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

TEST_F(InflectionCompoundTest, CompoundCausativePassiveTeKita) {
  auto result = inflection_.getBest("いかされてきた");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

// ===== Compound verb: temorau =====

TEST_F(InflectionCompoundTest, CompoundTeMorattaGodanKa) {
  auto result = inflection_.getBest("書いてもらった");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionCompoundTest, CompoundTeMorattaIchidan) {
  auto result = inflection_.getBest("教えてもらった");
  EXPECT_EQ(result.base_form, "教える");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, CompoundTeMoratteiru) {
  auto result = inflection_.getBest("教えてもらっている");
  EXPECT_EQ(result.base_form, "教える");
}

// ===== Compound verb: tekureru =====

TEST_F(InflectionCompoundTest, CompoundTeKuretaGodanKa) {
  auto result = inflection_.getBest("書いてくれた");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionCompoundTest, CompoundTeKuretaIchidan) {
  auto result = inflection_.getBest("教えてくれた");
  EXPECT_EQ(result.base_form, "教える");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Compound verb: teageru =====

TEST_F(InflectionCompoundTest, CompoundTeAgetaIchidan) {
  auto result = inflection_.getBest("教えてあげた");
  EXPECT_EQ(result.base_form, "教える");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, CompoundTeAgetaGodanWa) {
  auto result = inflection_.getBest("買ってあげた");
  EXPECT_TRUE(result.base_form == "買う" || result.base_form == "買る" ||
              result.base_form == "買つ");
}

// ===== Compound verb: teoru =====

TEST_F(InflectionCompoundTest, CompoundTeOrimasu) {
  auto result = inflection_.getBest("待っております");
  EXPECT_TRUE(result.base_form == "待つ" || result.base_form == "待る" ||
              result.base_form == "待う");
}

TEST_F(InflectionCompoundTest, CompoundTeOrimasita) {
  auto result = inflection_.getBest("いただいておりました");
  EXPECT_EQ(result.base_form, "いただく");
}

// ===== Passive + compound =====

TEST_F(InflectionCompoundTest, PassiveTeShimatta) {
  auto result = inflection_.getBest("殺されてしまった");
  EXPECT_EQ(result.base_form, "殺す");
  EXPECT_EQ(result.verb_type, VerbType::GodanSa);
}

TEST_F(InflectionCompoundTest, PassiveTeKita) {
  auto result = inflection_.getBest("愛されてきた");
  EXPECT_EQ(result.base_form, "愛す");
  EXPECT_EQ(result.verb_type, VerbType::GodanSa);
}

// ===== Causative-passive + compound =====

TEST_F(InflectionCompoundTest, CausativePassiveTeKita) {
  auto result = inflection_.getBest("歩かされてきた");
  EXPECT_EQ(result.base_form, "歩く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionCompoundTest, CausativePassiveTeIta) {
  auto result = inflection_.getBest("待たされていた");
  EXPECT_EQ(result.base_form, "待つ");
  EXPECT_EQ(result.verb_type, VerbType::GodanTa);
}

// ===== Triple compound verbs =====

TEST_F(InflectionCompoundTest, TripleCompoundTeMiteOita) {
  auto result = inflection_.getBest("書いてみておいた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionCompoundTest, TripleCompoundTeShimatteIta) {
  auto result = inflection_.getBest("読んでしまっていた");
  EXPECT_EQ(result.base_form, "読む");
}

TEST_F(InflectionCompoundTest, TripleCompoundCausativePassiveTeShimatta) {
  auto result = inflection_.getBest("食べさせられてしまった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== teitadaku =====

TEST_F(InflectionCompoundTest, TeItadakuGodanKa) {
  auto result = inflection_.getBest("書いていただいた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionCompoundTest, TeItadakuIchidan) {
  auto result = inflection_.getBest("教えていただきました");
  EXPECT_EQ(result.base_form, "教える");
}

TEST_F(InflectionCompoundTest, TeItadakuSuru) {
  auto result = inflection_.getBest("説明していただけますか");
  EXPECT_EQ(result.base_form, "説明する");
}

// ===== tehoshii =====

TEST_F(InflectionCompoundTest, TeHoshiiGodanKa) {
  auto result = inflection_.getBest("書いてほしかった");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionCompoundTest, TeHoshiiIchidan) {
  auto result = inflection_.getBest("食べてほしい");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== tekudasaru =====

TEST_F(InflectionCompoundTest, TeKudasaruGodanKa) {
  auto result = inflection_.getBest("書いてくださった");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionCompoundTest, TeKudasaruIchidan) {
  auto result = inflection_.getBest("教えてくださいました");
  EXPECT_EQ(result.base_form, "教える");
}

// ===== Nested compound patterns =====

TEST_F(InflectionCompoundTest, NestedTeShimatteitaGodanKa) {
  auto result = inflection_.getBest("書いてしまっていた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionCompoundTest, NestedTeKiteiru) {
  auto result = inflection_.getBest("増えてきている");
  EXPECT_EQ(result.base_form, "増える");
}

TEST_F(InflectionCompoundTest, NestedTeOiteAru) {
  auto result = inflection_.getBest("書いておいてある");
  EXPECT_EQ(result.base_form, "書く");
}

// ===== Quadruple compound patterns =====

TEST_F(InflectionCompoundTest, QuadrupleCompoundGodanKa) {
  auto result = inflection_.getBest("書いてみてしまっておいた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionCompoundTest, QuadrupleCompoundIchidan) {
  auto result = inflection_.getBest("食べてみてもらっていた");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Passive + Causative + Compound =====

TEST_F(InflectionCompoundTest, PassiveCausativeCompound) {
  auto result = inflection_.getBest("書かせられてしまった");
  EXPECT_EQ(result.base_form, "書く");
}

// ===== Ichidan potential + nakunaru (stop being able to) =====

TEST_F(InflectionCompoundTest, IchidanPotentialNakunaru) {
  auto result = inflection_.getBest("食べられなくなる");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanPotentialNakunatta) {
  auto result = inflection_.getBest("食べられなくなった");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanPotentialNakunatte) {
  auto result = inflection_.getBest("食べられなくなって");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanPotentialNakunatteShimatta) {
  auto result = inflection_.getBest("食べられなくなってしまった");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanPotentialNakunaru_Miru) {
  auto result = inflection_.getBest("見られなくなる");
  EXPECT_EQ(result.base_form, "見る");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Ichidan mizenkei + なくなる (stop doing) =====
// 食べなくなった = stopped eating (食べ mizenkei + なくなった)

TEST_F(InflectionCompoundTest, IchidanMizenkeiNakunaru) {
  auto result = inflection_.getBest("食べなくなる");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanMizenkeiNakunatta) {
  auto result = inflection_.getBest("食べなくなった");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanMizenkeiNakunatte) {
  auto result = inflection_.getBest("食べなくなって");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanMizenkeiNakunatteShimatta) {
  auto result = inflection_.getBest("食べなくなってしまった");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanMizenkeiNakunaru_Miru) {
  auto result = inflection_.getBest("見なくなる");
  EXPECT_EQ(result.base_form, "見る");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionCompoundTest, IchidanMizenkeiNakunatta_Shiraberu) {
  // 調べなくなった = stopped investigating
  auto result = inflection_.getBest("調べなくなった");
  EXPECT_EQ(result.base_form, "調べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Godan potential + なくなる (become unable to) =====
// 話せなくなった = became unable to speak (話せ potential + なくなった)

TEST_F(InflectionCompoundTest, GodanPotentialNakunaru_Hanasu) {
  auto result = inflection_.getBest("話せなくなる");
  EXPECT_EQ(result.base_form, "話す");
  EXPECT_EQ(result.verb_type, VerbType::GodanSa);
}

TEST_F(InflectionCompoundTest, GodanPotentialNakunatta_Hanasu) {
  auto result = inflection_.getBest("話せなくなった");
  EXPECT_EQ(result.base_form, "話す");
  EXPECT_EQ(result.verb_type, VerbType::GodanSa);
}

TEST_F(InflectionCompoundTest, GodanPotentialNakunatta_Yomu) {
  auto result = inflection_.getBest("読めなくなった");
  EXPECT_EQ(result.base_form, "読む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionCompoundTest, GodanPotentialNakunatta_Kaku) {
  auto result = inflection_.getBest("書けなくなった");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionCompoundTest, GodanPotentialNakunatteShimatta_Hanasu) {
  auto result = inflection_.getBest("話せなくなってしまった");
  EXPECT_EQ(result.base_form, "話す");
  EXPECT_EQ(result.verb_type, VerbType::GodanSa);
}

}  // namespace
}  // namespace suzume::grammar
