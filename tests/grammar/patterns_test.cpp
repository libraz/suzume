// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Pattern detection tests: verb negative patterns
//
// Tests for grammar/patterns.h functions that detect verb forms
// to help distinguish from adjective candidates.

#include "grammar/patterns.h"

#include <gtest/gtest.h>

namespace suzume::grammar {
namespace {

// ===== endsWithVerbNegative tests =====

class VerbNegativeTest : public ::testing::Test {};

// Godan verb mizenkei + ない patterns
TEST_F(VerbNegativeTest, Godan_Ka_Kanai) {
  EXPECT_TRUE(endsWithVerbNegative("書かない"));  // 書く + ない
  EXPECT_TRUE(endsWithVerbNegative("聞かない"));  // 聞く + ない
}

TEST_F(VerbNegativeTest, Godan_Ga_Ganai) {
  EXPECT_TRUE(endsWithVerbNegative("泳がない"));  // 泳ぐ + ない
  EXPECT_TRUE(endsWithVerbNegative("急がない"));  // 急ぐ + ない
}

TEST_F(VerbNegativeTest, Godan_Sa_Sanai) {
  EXPECT_TRUE(endsWithVerbNegative("話さない"));  // 話す + ない
  EXPECT_TRUE(endsWithVerbNegative("押さない"));  // 押す + ない
}

TEST_F(VerbNegativeTest, Godan_Ta_Tanai) {
  EXPECT_TRUE(endsWithVerbNegative("待たない"));  // 待つ + ない
  EXPECT_TRUE(endsWithVerbNegative("持たない"));  // 持つ + ない
}

TEST_F(VerbNegativeTest, Godan_Ba_Banai) {
  EXPECT_TRUE(endsWithVerbNegative("遊ばない"));  // 遊ぶ + ない
  EXPECT_TRUE(endsWithVerbNegative("呼ばない"));  // 呼ぶ + ない
}

TEST_F(VerbNegativeTest, Godan_Ma_Manai) {
  EXPECT_TRUE(endsWithVerbNegative("読まない"));  // 読む + ない
  EXPECT_TRUE(endsWithVerbNegative("飲まない"));  // 飲む + ない
}

TEST_F(VerbNegativeTest, Godan_Na_Nanai) {
  EXPECT_TRUE(endsWithVerbNegative("死なない"));  // 死ぬ + ない
}

TEST_F(VerbNegativeTest, Godan_Ra_Ranai) {
  EXPECT_TRUE(endsWithVerbNegative("取らない"));  // 取る + ない
  EXPECT_TRUE(endsWithVerbNegative("走らない"));  // 走る + ない
}

TEST_F(VerbNegativeTest, Godan_Wa_Wanai) {
  EXPECT_TRUE(endsWithVerbNegative("買わない"));  // 買う + ない
  EXPECT_TRUE(endsWithVerbNegative("言わない"));  // 言う + ない
}

// Ichidan verb + ない patterns
TEST_F(VerbNegativeTest, Ichidan_Benai) {
  EXPECT_TRUE(endsWithVerbNegative("食べない"));  // 食べる + ない
  EXPECT_TRUE(endsWithVerbNegative("調べない"));  // 調べる + ない
}

TEST_F(VerbNegativeTest, Ichidan_Menai) {
  EXPECT_TRUE(endsWithVerbNegative("見めない"));  // Pattern only
}

TEST_F(VerbNegativeTest, Ichidan_Renai) {
  EXPECT_TRUE(endsWithVerbNegative("忘れない"));  // 忘れる + ない
}

// Suru verb + ない
TEST_F(VerbNegativeTest, Suru_Shinai) {
  EXPECT_TRUE(endsWithVerbNegative("しない"));      // する + ない
  EXPECT_TRUE(endsWithVerbNegative("勉強しない"));  // 勉強する + ない
}

// Patterns that should NOT match
TEST_F(VerbNegativeTest, NotVerbNegative_Adjective) {
  EXPECT_FALSE(endsWithVerbNegative("美味しくない"));  // Adjective negative
  EXPECT_FALSE(endsWithVerbNegative("面白くない"));   // Adjective negative
}

TEST_F(VerbNegativeTest, NotVerbNegative_TooShort) {
  EXPECT_FALSE(endsWithVerbNegative("ない"));  // Too short
  EXPECT_FALSE(endsWithVerbNegative(""));      // Empty
}

// ===== endsWithPassiveCausativeNegativeRenyokei tests =====

class PassiveCausativeNegativeRenyokeiTest : public ::testing::Test {};

TEST_F(PassiveCausativeNegativeRenyokeiTest, RareNaku_Passive) {
  EXPECT_TRUE(endsWithPassiveCausativeNegativeRenyokei("食べられなく"));
  EXPECT_TRUE(endsWithPassiveCausativeNegativeRenyokei("読まれなく"));
}

TEST_F(PassiveCausativeNegativeRenyokeiTest, ReNaku_ShortPassive) {
  EXPECT_TRUE(endsWithPassiveCausativeNegativeRenyokei("使い切れなく"));
  EXPECT_TRUE(endsWithPassiveCausativeNegativeRenyokei("食べれなく"));
}

TEST_F(PassiveCausativeNegativeRenyokeiTest, SaseNaku_Causative) {
  EXPECT_TRUE(endsWithPassiveCausativeNegativeRenyokei("食べさせなく"));
  EXPECT_TRUE(endsWithPassiveCausativeNegativeRenyokei("働かせなく"));
}

TEST_F(PassiveCausativeNegativeRenyokeiTest, SeNaku_ShortCausative) {
  EXPECT_TRUE(endsWithPassiveCausativeNegativeRenyokei("書かせなく"));
}

TEST_F(PassiveCausativeNegativeRenyokeiTest, SareNaku_Passive) {
  EXPECT_TRUE(endsWithPassiveCausativeNegativeRenyokei("開催されなく"));
}

TEST_F(PassiveCausativeNegativeRenyokeiTest, NotMatch) {
  EXPECT_FALSE(endsWithPassiveCausativeNegativeRenyokei("美しく"));
  EXPECT_FALSE(endsWithPassiveCausativeNegativeRenyokei("なく"));
  EXPECT_FALSE(endsWithPassiveCausativeNegativeRenyokei(""));
}

// ===== endsWithNegativeBecomePattern tests =====

class NegativeBecomePatternTest : public ::testing::Test {};

TEST_F(NegativeBecomePatternTest, ReNakunatta) {
  EXPECT_TRUE(endsWithNegativeBecomePattern("読まれなくなった"));
}

TEST_F(NegativeBecomePatternTest, RareNakunatta) {
  EXPECT_TRUE(endsWithNegativeBecomePattern("食べられなくなった"));
}

TEST_F(NegativeBecomePatternTest, SaserareNakunatta) {
  EXPECT_TRUE(endsWithNegativeBecomePattern("食べさせられなくなった"));
}

TEST_F(NegativeBecomePatternTest, SerareNakunatta) {
  EXPECT_TRUE(endsWithNegativeBecomePattern("働かせられなくなった"));
}

TEST_F(NegativeBecomePatternTest, NotMatch) {
  EXPECT_FALSE(endsWithNegativeBecomePattern("なくなった"));
  EXPECT_FALSE(endsWithNegativeBecomePattern(""));
}

// ===== endsWithGodanNegativeRenyokei tests =====

class GodanNegativeRenyokeiTest : public ::testing::Test {};

TEST_F(GodanNegativeRenyokeiTest, KaNaku) {
  EXPECT_TRUE(endsWithGodanNegativeRenyokei("いかなく"));  // いく + ない連用形
  EXPECT_TRUE(endsWithGodanNegativeRenyokei("かかなく"));  // かく + ない連用形
}

TEST_F(GodanNegativeRenyokeiTest, NotMatch) {
  EXPECT_FALSE(endsWithGodanNegativeRenyokei("なく"));
  EXPECT_FALSE(endsWithGodanNegativeRenyokei("美しく"));
  EXPECT_FALSE(endsWithGodanNegativeRenyokei(""));
}

}  // namespace
}  // namespace suzume::grammar
