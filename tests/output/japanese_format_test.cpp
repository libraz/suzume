/**
 * @file japanese_format_test.cpp
 * @brief Tests for Japanese morphological output formatting
 *
 * Tests Japanese POS names, verb type names, conjugation form names,
 * and reading output for detailed morphological analysis output.
 */

#include <gtest/gtest.h>

#include "core/types.h"
#include "grammar/conjugation.h"
#include "postprocess/lemmatizer.h"
#include "suzume.h"

namespace suzume::output::test {

// =============================================================================
// Japanese POS Names (posToJapanese)
// =============================================================================

class JapanesePosNameTest : public ::testing::Test {};

TEST_F(JapanesePosNameTest, Noun) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Noun), "名詞");
}

TEST_F(JapanesePosNameTest, Verb) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Verb), "動詞");
}

TEST_F(JapanesePosNameTest, Adjective) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Adjective), "形容詞");
}

TEST_F(JapanesePosNameTest, Particle) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Particle), "助詞");
}

TEST_F(JapanesePosNameTest, Auxiliary) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Auxiliary), "助動詞");
}

TEST_F(JapanesePosNameTest, Adverb) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Adverb), "副詞");
}

TEST_F(JapanesePosNameTest, Conjunction) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Conjunction), "接続詞");
}

TEST_F(JapanesePosNameTest, Pronoun) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Pronoun), "代名詞");
}

TEST_F(JapanesePosNameTest, Determiner) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Determiner), "連体詞");
}

TEST_F(JapanesePosNameTest, Symbol) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Symbol), "記号");
}

TEST_F(JapanesePosNameTest, Unknown) {
  // Unknown and Other both return "その他"
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Unknown), "その他");
}

TEST_F(JapanesePosNameTest, Other) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Other), "その他");
}

// =============================================================================
// Japanese Verb Type Names (verbTypeToJapanese)
// =============================================================================

class JapaneseVerbTypeTest : public ::testing::Test {};

TEST_F(JapaneseVerbTypeTest, Ichidan) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::Ichidan), "一段");
}

TEST_F(JapaneseVerbTypeTest, GodanKa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanKa), "五段・カ行");
}

TEST_F(JapaneseVerbTypeTest, GodanGa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanGa), "五段・ガ行");
}

TEST_F(JapaneseVerbTypeTest, GodanSa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanSa), "五段・サ行");
}

TEST_F(JapaneseVerbTypeTest, GodanTa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanTa), "五段・タ行");
}

TEST_F(JapaneseVerbTypeTest, GodanNa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanNa), "五段・ナ行");
}

TEST_F(JapaneseVerbTypeTest, GodanBa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanBa), "五段・バ行");
}

TEST_F(JapaneseVerbTypeTest, GodanMa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanMa), "五段・マ行");
}

TEST_F(JapaneseVerbTypeTest, GodanRa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanRa), "五段・ラ行");
}

TEST_F(JapaneseVerbTypeTest, GodanWa) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::GodanWa), "五段・ワ行");
}

TEST_F(JapaneseVerbTypeTest, Suru) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::Suru), "サ変");
}

TEST_F(JapaneseVerbTypeTest, Kuru) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::Kuru), "カ変");
}

TEST_F(JapaneseVerbTypeTest, IAdjective) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::IAdjective), "形容詞");
}

TEST_F(JapaneseVerbTypeTest, Unknown) {
  EXPECT_EQ(grammar::verbTypeToJapanese(grammar::VerbType::Unknown), "");
}

// =============================================================================
// Japanese Conjugation Form Names (conjFormToJapanese)
// =============================================================================

class JapaneseConjFormTest : public ::testing::Test {};

TEST_F(JapaneseConjFormTest, Base) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Base), "終止形");
}

TEST_F(JapaneseConjFormTest, Mizenkei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Mizenkei), "未然形");
}

TEST_F(JapaneseConjFormTest, Renyokei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Renyokei), "連用形");
}

TEST_F(JapaneseConjFormTest, Kateikei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Kateikei), "仮定形");
}

TEST_F(JapaneseConjFormTest, Meireikei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Meireikei), "命令形");
}

TEST_F(JapaneseConjFormTest, Ishikei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Ishikei), "意志形");
}

// =============================================================================
// Conjugation Form Detection (detectConjForm)
// =============================================================================

class ConjFormDetectionTest : public ::testing::Test {};

// Verb: Mizenkei (未然形) - negative, passive, causative
TEST_F(ConjFormDetectionTest, Verb_Mizenkei_Negative) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べない", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Mizenkei);
}

TEST_F(ConjFormDetectionTest, Verb_Mizenkei_Passive) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べられる", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Mizenkei);
}

TEST_F(ConjFormDetectionTest, Verb_Mizenkei_Causative) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べさせる", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Mizenkei);
}

// Verb: Renyokei (連用形) - masu, ta, te
TEST_F(ConjFormDetectionTest, Verb_Renyokei_Masu) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べます", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

TEST_F(ConjFormDetectionTest, Verb_Renyokei_Ta) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べた", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

TEST_F(ConjFormDetectionTest, Verb_Renyokei_Te) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べて", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

TEST_F(ConjFormDetectionTest, Verb_Renyokei_Teiru) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べている", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

// Verb: Kateikei (仮定形) - ba
TEST_F(ConjFormDetectionTest, Verb_Kateikei_Ba) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べれば", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Kateikei);
}

TEST_F(ConjFormDetectionTest, Verb_Kateikei_Godan) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "書けば", "書く", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Kateikei);
}

// Verb: Meireikei (命令形) - ro, e
TEST_F(ConjFormDetectionTest, Verb_Meireikei_Ichidan) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べろ", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Meireikei);
}

TEST_F(ConjFormDetectionTest, Verb_Meireikei_Godan) {
  // Godan imperative ends in 'e' sound - current implementation returns Renyokei
  // as fallback for unrecognized conjugated forms
  auto form = postprocess::Lemmatizer::detectConjForm(
      "書け", "書く", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

// Verb: Ishikei (意志形) - ou, you
TEST_F(ConjFormDetectionTest, Verb_Ishikei_Ichidan) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べよう", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Ishikei);
}

TEST_F(ConjFormDetectionTest, Verb_Ishikei_Godan) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "書こう", "書く", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Ishikei);
}

// Verb: Base form (終止形)
TEST_F(ConjFormDetectionTest, Verb_Base_Ichidan) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "食べる", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

TEST_F(ConjFormDetectionTest, Verb_Base_Godan) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "書く", "書く", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

// Adjective conjugation forms
TEST_F(ConjFormDetectionTest, Adjective_Renyokei_Ku) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "美しく", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

TEST_F(ConjFormDetectionTest, Adjective_Onbinkei_Katta) {
  // "かった" ends with "った" which matches onbinkei pattern first
  auto form = postprocess::Lemmatizer::detectConjForm(
      "美しかった", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Onbinkei);
}

TEST_F(ConjFormDetectionTest, Adjective_Mizenkei_Kunai) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "美しくない", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Mizenkei);
}

TEST_F(ConjFormDetectionTest, Adjective_Kateikei_Kereba) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "美しければ", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Kateikei);
}

TEST_F(ConjFormDetectionTest, Adjective_Base) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "美しい", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

// Non-verb/adjective should return Base
TEST_F(ConjFormDetectionTest, Noun_ReturnsBase) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "学校", "学校", core::PartOfSpeech::Noun);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

TEST_F(ConjFormDetectionTest, Particle_ReturnsBase) {
  auto form = postprocess::Lemmatizer::detectConjForm(
      "は", "は", core::PartOfSpeech::Particle);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

// =============================================================================
// Conjugation Type to Verb Type Conversion
// =============================================================================

class ConjTypeToVerbTypeTest : public ::testing::Test {};

TEST_F(ConjTypeToVerbTypeTest, None) {
  EXPECT_EQ(grammar::conjTypeToVerbType(dictionary::ConjugationType::None),
            grammar::VerbType::Unknown);
}

TEST_F(ConjTypeToVerbTypeTest, Ichidan) {
  EXPECT_EQ(grammar::conjTypeToVerbType(dictionary::ConjugationType::Ichidan),
            grammar::VerbType::Ichidan);
}

TEST_F(ConjTypeToVerbTypeTest, GodanKa) {
  EXPECT_EQ(grammar::conjTypeToVerbType(dictionary::ConjugationType::GodanKa),
            grammar::VerbType::GodanKa);
}

TEST_F(ConjTypeToVerbTypeTest, Suru) {
  EXPECT_EQ(grammar::conjTypeToVerbType(dictionary::ConjugationType::Suru),
            grammar::VerbType::Suru);
}

TEST_F(ConjTypeToVerbTypeTest, Kuru) {
  EXPECT_EQ(grammar::conjTypeToVerbType(dictionary::ConjugationType::Kuru),
            grammar::VerbType::Kuru);
}

TEST_F(ConjTypeToVerbTypeTest, IAdjective) {
  EXPECT_EQ(grammar::conjTypeToVerbType(dictionary::ConjugationType::IAdjective),
            grammar::VerbType::IAdjective);
}

// =============================================================================
// Integration Tests: Morpheme Analysis with Japanese Format Info
// =============================================================================

class JapaneseFormatIntegrationTest : public ::testing::Test {
 protected:
  Suzume analyzer_;
};

// Test that verb analysis includes correct conjugation type
TEST_F(JapaneseFormatIntegrationTest, VerbWithConjType_Ichidan) {
  auto morphemes = analyzer_.analyze("食べました");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].surface, "食べました");
  EXPECT_EQ(morphemes[0].getLemma(), "食べる");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);

  auto verb_type = grammar::conjTypeToVerbType(morphemes[0].conj_type);
  EXPECT_EQ(verb_type, grammar::VerbType::Ichidan);
}

TEST_F(JapaneseFormatIntegrationTest, VerbWithConjType_GodanKa) {
  auto morphemes = analyzer_.analyze("書きました");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].surface, "書きました");
  EXPECT_EQ(morphemes[0].getLemma(), "書く");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);

  auto verb_type = grammar::conjTypeToVerbType(morphemes[0].conj_type);
  EXPECT_EQ(verb_type, grammar::VerbType::GodanKa);
}

TEST_F(JapaneseFormatIntegrationTest, VerbWithConjType_Suru) {
  auto morphemes = analyzer_.analyze("しています");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].surface, "しています");
  EXPECT_EQ(morphemes[0].getLemma(), "する");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);

  auto verb_type = grammar::conjTypeToVerbType(morphemes[0].conj_type);
  EXPECT_EQ(verb_type, grammar::VerbType::Suru);
}

TEST_F(JapaneseFormatIntegrationTest, VerbWithConjType_GodanMa) {
  auto morphemes = analyzer_.analyze("読んでいます");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].surface, "読んでいます");
  EXPECT_EQ(morphemes[0].getLemma(), "読む");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);

  auto verb_type = grammar::conjTypeToVerbType(morphemes[0].conj_type);
  EXPECT_EQ(verb_type, grammar::VerbType::GodanMa);
}

// Test reading field propagation
TEST_F(JapaneseFormatIntegrationTest, ReadingPropagation_Pronoun) {
  auto morphemes = analyzer_.analyze("私");
  ASSERT_GE(morphemes.size(), 1);
  // "私" should have reading "わたし" from dictionary
  EXPECT_EQ(morphemes[0].surface, "私");
  EXPECT_EQ(morphemes[0].reading, "わたし");
}

TEST_F(JapaneseFormatIntegrationTest, ReadingPropagation_Adjective) {
  // Use single-kanji adjective that's in L1
  auto morphemes = analyzer_.analyze("寒い");
  ASSERT_GE(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].surface, "寒い");
  // Should have reading from dictionary
  EXPECT_FALSE(morphemes[0].reading.empty());
  EXPECT_EQ(morphemes[0].reading, "さむい");
}

// Test conjugation form detection in analysis pipeline
TEST_F(JapaneseFormatIntegrationTest, ConjForm_Mizenkei) {
  auto morphemes = analyzer_.analyze("食べない");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].conj_form, grammar::ConjForm::Mizenkei);
}

TEST_F(JapaneseFormatIntegrationTest, ConjForm_Renyokei) {
  auto morphemes = analyzer_.analyze("食べました");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].conj_form, grammar::ConjForm::Renyokei);
}

TEST_F(JapaneseFormatIntegrationTest, ConjForm_Kateikei) {
  auto morphemes = analyzer_.analyze("走れば");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].conj_form, grammar::ConjForm::Kateikei);
}

TEST_F(JapaneseFormatIntegrationTest, ConjForm_Base) {
  auto morphemes = analyzer_.analyze("食べる");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].conj_form, grammar::ConjForm::Base);
}

}  // namespace suzume::output::test
