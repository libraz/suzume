
// Inflection tests: Imperative form (命令形) patterns
//
// Note: Standalone imperative matching (書け, 食べろ) via Inflection::analyze()
// is intentionally NOT supported because it causes regression with conditional
// forms (食べれば gets split as 食べれ + ば).
//
// Imperatives are handled via:
// 1. Dictionary lookup (しろ, やめろ in L1 entries)
// 2. Compound verb patterns (勉強しろ via auxiliary chain)
// 3. Conjugation generation (for display/generation, not parsing)

#include "grammar/inflection.h"

#include <gtest/gtest.h>

namespace suzume::grammar {
namespace {

class InflectionImperativeTest : public ::testing::Test {
 protected:
  Inflection inflection_;
};

// ===== Imperative vs Hypothetical distinction =====
// These tests ensure that hypothetical forms (仮定形) are correctly analyzed
// and NOT confused with imperative forms

TEST_F(InflectionImperativeTest, Hypothetical_Kakeba) {
  auto result = inflection_.getBest("書けば");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionImperativeTest, Hypothetical_Hashireba) {
  auto result = inflection_.getBest("走れば");
  EXPECT_EQ(result.base_form, "走る");
  EXPECT_EQ(result.verb_type, VerbType::GodanRa);
}

TEST_F(InflectionImperativeTest, Hypothetical_Yomeba) {
  auto result = inflection_.getBest("読めば");
  EXPECT_EQ(result.base_form, "読む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionImperativeTest, Hypothetical_Sureba) {
  auto result = inflection_.getBest("すれば");
  EXPECT_EQ(result.base_form, "する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

TEST_F(InflectionImperativeTest, Hypothetical_Kureba) {
  auto result = inflection_.getBest("くれば");
  EXPECT_EQ(result.base_form, "くる");
  EXPECT_EQ(result.verb_type, VerbType::Kuru);
}

TEST_F(InflectionImperativeTest, Hypothetical_Tabereba) {
  auto result = inflection_.getBest("食べれば");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionImperativeTest, Hypothetical_Okireba) {
  // Ichidan verb with i-row stem (起き + れば)
  auto result = inflection_.getBest("起きれば");
  EXPECT_EQ(result.base_form, "起きる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionImperativeTest, Hypothetical_Ikireba) {
  // Another Ichidan verb with i-row stem (生き + れば)
  auto result = inflection_.getBest("生きれば");
  EXPECT_EQ(result.base_form, "生きる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Compound verb imperatives (サ変) =====
// TODO: These require しろ/せよ to be added as auxiliaries
// See technical_debt_action_plan.md section 3.8

}  // namespace
}  // namespace suzume::grammar
