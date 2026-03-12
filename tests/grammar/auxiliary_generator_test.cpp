
// Auxiliary generator tests
// Verifies generated auxiliary surfaces match expected patterns

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

// Verify generator produces expected number of unique surfaces
TEST_F(AuxiliaryGeneratorTest, GeneratesExpectedCount) {
  // Generator produces ~406 unique surfaces (conjugation forms of auxiliary
  // bases plus special patterns). Allow some tolerance for future additions.
  EXPECT_GE(generated_surfaces_.size(), 390);
}

// Verify multi-word constructions are NOT generated (they cause false verb
// absorption). These were intentionally removed.
TEST_F(AuxiliaryGeneratorTest, DoesNotGenerateMultiWordConstructions) {
  // Prohibition/Permission (て+は+いけない etc.)
  EXPECT_FALSE(hasSurface("はいけない"));
  EXPECT_FALSE(hasSurface("はいけなかった"));
  EXPECT_FALSE(hasSurface("はならない"));
  EXPECT_FALSE(hasSurface("はならなかった"));
  EXPECT_FALSE(hasSurface("はだめだ"));
  EXPECT_FALSE(hasSurface("もいい"));
  EXPECT_FALSE(hasSurface("もいいですか"));
  EXPECT_FALSE(hasSurface("もかまわない"));
  EXPECT_FALSE(hasSurface("もかまわなかった"));

  // Formal noun + copula
  EXPECT_FALSE(hasSurface("ところだ"));
  EXPECT_FALSE(hasSurface("ところだった"));
  EXPECT_FALSE(hasSurface("ところです"));

  // ばかり + copula
  EXPECT_FALSE(hasSurface("ばかりだ"));
  EXPECT_FALSE(hasSurface("ばかりだった"));
  EXPECT_FALSE(hasSurface("ばかりです"));
  EXPECT_FALSE(hasSurface("ばかりなのに"));

  // っぱなし + copula/particle
  EXPECT_FALSE(hasSurface("っぱなしだ"));
  EXPECT_FALSE(hasSurface("っぱなしで"));
  EXPECT_FALSE(hasSurface("っぱなしにする"));

  // Multi-particle chains
  EXPECT_FALSE(hasSurface("ざるを得ない"));
  EXPECT_FALSE(hasSurface("ざるを得なかった"));
  EXPECT_FALSE(hasSurface("ざるを得ません"));
  EXPECT_FALSE(hasSurface("ずにはいられない"));
  EXPECT_FALSE(hasSurface("ずにはいられなかった"));
  EXPECT_FALSE(hasSurface("わけにはいかない"));
  EXPECT_FALSE(hasSurface("わけにはいかなかった"));
  EXPECT_FALSE(hasSurface("わけにはいきません"));

  // Volitional + とする (split as う+と+する, よう+と+する)
  EXPECT_FALSE(hasSurface("うとする"));
  EXPECT_FALSE(hasSurface("うとした"));
  EXPECT_FALSE(hasSurface("うとして"));
  EXPECT_FALSE(hasSurface("ようとする"));
  EXPECT_FALSE(hasSurface("ようとした"));
  EXPECT_FALSE(hasSurface("ようとして"));

  // Volitional + ている (volitional + と+する+ている)
  EXPECT_FALSE(hasSurface("うとしている"));
  EXPECT_FALSE(hasSurface("うとしていた"));
  EXPECT_FALSE(hasSurface("ようとしている"));
  EXPECT_FALSE(hasSurface("ようとしていた"));

  // ようになる + ている
  EXPECT_FALSE(hasSurface("ようになっている"));
  EXPECT_FALSE(hasSurface("ようになってきた"));

  // べき + copula (split as べき+だ etc.)
  EXPECT_FALSE(hasSurface("べきだ"));
  EXPECT_FALSE(hasSurface("べきだった"));
  EXPECT_FALSE(hasSurface("べきです"));
  EXPECT_FALSE(hasSurface("べきではない"));
  EXPECT_FALSE(hasSurface("べきではなかった"));

  // のだ/んだ (split as の+だ, ん+だ)
  EXPECT_FALSE(hasSurface("のだ"));
  EXPECT_FALSE(hasSurface("のです"));
  EXPECT_FALSE(hasSurface("んだ"));
  EXPECT_FALSE(hasSurface("んです"));
}

// === Core conjugation patterns ===

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

TEST_F(AuxiliaryGeneratorTest, HasVolitionalForms) {
  EXPECT_TRUE(hasSurface("よう"));
  // Volitional + とする removed (multi-word: う+と+する should split)
  EXPECT_FALSE(hasSurface("ようとした"));
  EXPECT_FALSE(hasSurface("ようとして"));
  EXPECT_FALSE(hasSurface("ようとする"));
  EXPECT_TRUE(hasSurface("ようになった"));
  EXPECT_TRUE(hasSurface("ようになって"));
  EXPECT_TRUE(hasSurface("ようになる"));
}

TEST_F(AuxiliaryGeneratorTest, HasObligationForms) {
  EXPECT_TRUE(hasSurface("ないといけない"));
  EXPECT_TRUE(hasSurface("ないといけなかった"));
  EXPECT_TRUE(hasSurface("なければならない"));
  EXPECT_TRUE(hasSurface("なければならなかった"));
  EXPECT_TRUE(hasSurface("なくてはいけない"));
  EXPECT_TRUE(hasSurface("なくてはいけなかった"));
  EXPECT_TRUE(hasSurface("なきゃいけない"));
  EXPECT_TRUE(hasSurface("なきゃならない"));
}

TEST_F(AuxiliaryGeneratorTest, HasSouForms) {
  EXPECT_TRUE(hasSurface("そう"));
  EXPECT_TRUE(hasSurface("そうだ"));
  EXPECT_TRUE(hasSurface("そうだった"));
  EXPECT_TRUE(hasSurface("そうです"));
  EXPECT_TRUE(hasSurface("そうな"));
  EXPECT_TRUE(hasSurface("そうに"));
}

TEST_F(AuxiliaryGeneratorTest, HasColloquialContractions) {
  EXPECT_TRUE(hasSurface("てる"));   // ている contracted
  EXPECT_TRUE(hasSurface("てた"));   // ていた contracted
  EXPECT_TRUE(hasSurface("てない")); // ていない contracted
  EXPECT_TRUE(hasSurface("でる"));   // でいる contracted
  EXPECT_TRUE(hasSurface("でた"));   // でいた contracted
}

// ことができる forms removed (multi-word construction, not auxiliary suffix)
TEST_F(AuxiliaryGeneratorTest, KotogaDekiruFormsRemoved) {
  EXPECT_FALSE(hasSurface("ことができる"));
  EXPECT_FALSE(hasSurface("ことができた"));
  EXPECT_FALSE(hasSurface("ことができて"));
  EXPECT_FALSE(hasSurface("ことができない"));
  EXPECT_FALSE(hasSurface("ことができなかった"));
}

}  // namespace
}  // namespace suzume::grammar
