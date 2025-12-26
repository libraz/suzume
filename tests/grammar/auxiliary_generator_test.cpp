// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Auxiliary generator compatibility tests
// Ensures new generator produces all patterns from old hardcoded implementation

#include "grammar/auxiliary_generator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace suzume::grammar {
namespace {

class AuxiliaryGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto entries = generateAllAuxiliaries();
    for (const auto& entry : entries) {
      generated_surfaces_.insert(entry.surface);
    }
  }

  bool hasSurface(const std::string& surface) const {
    return generated_surfaces_.count(surface) > 0;
  }

  std::set<std::string> generated_surfaces_;
};

// Old implementation surfaces (309 unique patterns)
// Extracted from git show HEAD:src/grammar/auxiliaries.cpp
const std::vector<std::string> kOldSurfaces = {
    "あげた", "あげます", "あげる", "あった", "あります", "ある",
    "い", "いく", "いた", "いただいた", "いただいて", "いただきました",
    "いただきます", "いただく", "いただけます", "いただけますか", "いただける",
    "いった", "いって", "いて", "いました", "います", "いる",
    "う", "うとした", "うとして", "うとしていた", "うとしている", "うとする",
    "おいた", "おいて", "おきます", "おく", "おった", "おりました",
    "おりまして", "おります", "おる",
    "かけた", "かけて", "かけている", "かける", "かった", "かったら",
    "きた", "きて", "きます",
    "く", "ください", "くださいました", "くださいます", "くださった", "くださって",
    "くださる", "くて", "くない", "くなかった", "くなった", "くなって",
    "くなる", "くる", "くれた", "くれます", "くれる", "ければ",
    "ことができた", "ことができて", "ことができない", "ことができなかった", "ことができる",
    "さ", "させた", "させて", "させない", "させなかった", "させなくて",
    "させます", "させられた", "させられたい", "させられたかった", "させられたくて",
    "させられたくない", "させられたくなかった", "させられて", "させられない",
    "させられなくて", "させられなくなった", "させられなくなって", "させられなくなる",
    "させられます", "させられる", "させる",
    "された", "されて", "されない", "されなかった", "されなくて",
    "されました", "されます", "されません", "される",
    "ざるを得ない", "ざるを得なかった", "ざるを得ません",
    "しまいます", "しまう", "しまった", "しまって",
    "じゃう", "じゃった", "じゃって",
    "すぎた", "すぎて", "すぎている", "すぎない", "すぎなかった",
    "すぎました", "すぎます", "すぎる",
    "ずにはいられない", "ずにはいられなかった",
    "せた", "せて", "せない", "せなかった", "せなくて", "せます",
    "せられた", "せられたい", "せられたかった", "せられたくて", "せられたくない",
    "せられたくなかった", "せられて", "せられない", "せられなくて",
    "せられなくなった", "せられなくなって", "せられなくなる",
    "せられました", "せられます", "せられません", "せられる", "せる",
    "そう", "そうだ", "そうだった", "そうでした", "そうです", "そうな", "そうに",
    "た", "たい", "たかった", "たくて", "たくない", "たくなかった",
    "たら", "たり", "たりした", "たりして", "たりする",
    "だ", "だら", "だり", "だりした", "だりして", "だりする",
    "ちゃう", "ちゃった", "ちゃって",
    "っぱなしだ", "っぱなしで", "っぱなしにする",
    "て", "で",
    "といた", "とく",
    "ところだ", "ところだった", "ところです",
    "ない", "ないで", "ないでいた", "ないでいる",
    "ないといけない", "ないといけなかった",
    "なかった", "ながら",
    "なきゃ", "なきゃいけない", "なきゃならない",
    "なくちゃ", "なくて", "なくてはいけない", "なくてはいけなかった",
    "なくなった", "なくなって", "なくなってしまう", "なくなってしまった", "なくなる",
    "なければ", "なければならない", "なければならなかった",
    "にくい", "にくかった", "にくく", "にくくて",
    "のだ", "のです",
    "はいけない", "はいけなかった", "はだめだ", "はならない", "はならなかった",
    "ば",
    "ばかりだ", "ばかりだった", "ばかりです", "ばかりなのに",
    "べきだ", "べきだった", "べきです", "べきではない", "べきではなかった",
    "ほしい", "ほしかった", "ほしくない",
    "ました", "ましょう", "ます", "ません", "ませんでした",
    "みた", "みたら", "みて", "みます", "みる", "みれば",
    "もいい", "もいいですか", "もかまわない", "もかまわなかった",
    "もらいます", "もらう", "もらった", "もらって",
    "やすい", "やすかった", "やすく", "やすくて",
    "よう", "ようとした", "ようとして", "ようとしていた", "ようとしている", "ようとする",
    "ようになった", "ようになって", "ようになっている", "ようになってきた", "ようになる",
    "られた", "られて", "られない", "られなかった", "られなくて",
    "られなくなった", "られなくなって", "られなくなってしまう", "られなくなってしまった",
    "られなくなる", "られます", "られる",
    "る",
    "れた", "れて", "れない", "れなかった", "れなくて",
    "れなくなった", "れなくなって", "れなくなる", "れます", "れる",
    "わけにはいかない", "わけにはいかなかった", "わけにはいきません",
    "んだ", "んだもの", "んだもん", "んです",
    "出した", "出して", "出す",
    "直した", "直して", "直している", "直す",
    "終えた", "終えて", "終える",
    "終わった", "終わって", "終わる",
    "続けた", "続けて", "続けている", "続ける",
};

// Test that generator produces all old surfaces
TEST_F(AuxiliaryGeneratorTest, CoversAllOldSurfaces) {
  std::vector<std::string> missing;
  for (const auto& surface : kOldSurfaces) {
    if (!hasSurface(surface)) {
      missing.push_back(surface);
    }
  }

  if (!missing.empty()) {
    std::string msg = "Missing surfaces (" + std::to_string(missing.size()) + "):\n";
    for (const auto& s : missing) {
      msg += "  " + s + "\n";
    }
    FAIL() << msg;
  }
}

// Test generator produces reasonable number of entries
TEST_F(AuxiliaryGeneratorTest, GeneratesReasonableCount) {
  // Old implementation had 345 entries (with duplicates)
  // New generator should produce at least 309 unique surfaces
  EXPECT_GE(generated_surfaces_.size(), 300);
}

// Test specific critical patterns exist
TEST_F(AuxiliaryGeneratorTest, HasMasuForms) {
  EXPECT_TRUE(hasSurface("ます"));
  EXPECT_TRUE(hasSurface("ました"));
  EXPECT_TRUE(hasSurface("ません"));
  EXPECT_TRUE(hasSurface("ましょう"));
  EXPECT_TRUE(hasSurface("ませんでした"));
}

TEST_F(AuxiliaryGeneratorTest, HasTeForms) {
  EXPECT_TRUE(hasSurface("て"));
  EXPECT_TRUE(hasSurface("で"));
  EXPECT_TRUE(hasSurface("た"));
  EXPECT_TRUE(hasSurface("だ"));
  EXPECT_TRUE(hasSurface("たら"));
  EXPECT_TRUE(hasSurface("だら"));
}

TEST_F(AuxiliaryGeneratorTest, HasTeiruForms) {
  EXPECT_TRUE(hasSurface("いる"));
  EXPECT_TRUE(hasSurface("いた"));
  EXPECT_TRUE(hasSurface("いて"));
  EXPECT_TRUE(hasSurface("います"));
  EXPECT_TRUE(hasSurface("いました"));
}

TEST_F(AuxiliaryGeneratorTest, HasNaiForms) {
  EXPECT_TRUE(hasSurface("ない"));
  EXPECT_TRUE(hasSurface("なかった"));
  EXPECT_TRUE(hasSurface("なくて"));
  EXPECT_TRUE(hasSurface("なければ"));
}

TEST_F(AuxiliaryGeneratorTest, HasTaiForms) {
  EXPECT_TRUE(hasSurface("たい"));
  EXPECT_TRUE(hasSurface("たかった"));
  EXPECT_TRUE(hasSurface("たくて"));
  EXPECT_TRUE(hasSurface("たくない"));
  EXPECT_TRUE(hasSurface("たくなかった"));
}

TEST_F(AuxiliaryGeneratorTest, HasCausativeForms) {
  EXPECT_TRUE(hasSurface("させる"));
  EXPECT_TRUE(hasSurface("させた"));
  EXPECT_TRUE(hasSurface("させて"));
  EXPECT_TRUE(hasSurface("させない"));
  EXPECT_TRUE(hasSurface("せる"));
  EXPECT_TRUE(hasSurface("せた"));
  EXPECT_TRUE(hasSurface("せて"));
}

TEST_F(AuxiliaryGeneratorTest, HasPassiveForms) {
  EXPECT_TRUE(hasSurface("られる"));
  EXPECT_TRUE(hasSurface("られた"));
  EXPECT_TRUE(hasSurface("られて"));
  EXPECT_TRUE(hasSurface("られない"));
  EXPECT_TRUE(hasSurface("れる"));
  EXPECT_TRUE(hasSurface("れた"));
  EXPECT_TRUE(hasSurface("れて"));
}

TEST_F(AuxiliaryGeneratorTest, HasCausativePassiveForms) {
  EXPECT_TRUE(hasSurface("させられる"));
  EXPECT_TRUE(hasSurface("させられた"));
  EXPECT_TRUE(hasSurface("させられて"));
  EXPECT_TRUE(hasSurface("せられる"));
  EXPECT_TRUE(hasSurface("せられた"));
  EXPECT_TRUE(hasSurface("せられて"));
}

TEST_F(AuxiliaryGeneratorTest, HasTeKuruForms) {
  EXPECT_TRUE(hasSurface("くる"));
  EXPECT_TRUE(hasSurface("きた"));
  EXPECT_TRUE(hasSurface("きて"));
  EXPECT_TRUE(hasSurface("きます"));
}

TEST_F(AuxiliaryGeneratorTest, HasTeShimauForms) {
  EXPECT_TRUE(hasSurface("しまう"));
  EXPECT_TRUE(hasSurface("しまった"));
  EXPECT_TRUE(hasSurface("しまって"));
  EXPECT_TRUE(hasSurface("ちゃう"));
  EXPECT_TRUE(hasSurface("ちゃった"));
  EXPECT_TRUE(hasSurface("じゃう"));
  EXPECT_TRUE(hasSurface("じゃった"));
}

TEST_F(AuxiliaryGeneratorTest, HasCompoundVerbForms) {
  EXPECT_TRUE(hasSurface("出す"));
  EXPECT_TRUE(hasSurface("出した"));
  EXPECT_TRUE(hasSurface("出して"));
  EXPECT_TRUE(hasSurface("終わる"));
  EXPECT_TRUE(hasSurface("終わった"));
  EXPECT_TRUE(hasSurface("続ける"));
  EXPECT_TRUE(hasSurface("続けた"));
}

TEST_F(AuxiliaryGeneratorTest, HasYasuiNikuiForms) {
  EXPECT_TRUE(hasSurface("やすい"));
  EXPECT_TRUE(hasSurface("やすかった"));
  EXPECT_TRUE(hasSurface("やすく"));
  EXPECT_TRUE(hasSurface("やすくて"));
  EXPECT_TRUE(hasSurface("にくい"));
  EXPECT_TRUE(hasSurface("にくかった"));
  EXPECT_TRUE(hasSurface("にくく"));
  EXPECT_TRUE(hasSurface("にくくて"));
}

TEST_F(AuxiliaryGeneratorTest, HasSugiruForms) {
  EXPECT_TRUE(hasSurface("すぎる"));
  EXPECT_TRUE(hasSurface("すぎた"));
  EXPECT_TRUE(hasSurface("すぎて"));
  EXPECT_TRUE(hasSurface("すぎます"));
}

}  // namespace
}  // namespace suzume::grammar
