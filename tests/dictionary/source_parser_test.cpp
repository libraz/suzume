#include "dictionary/source_parser.h"

#include <gtest/gtest.h>

namespace suzume {
namespace dictionary {
namespace {

TEST(SourceParserTest, ParsesBomTrimmedTsvAndCanonicalMarkers) {
  const std::string source =
      "\xEF\xBB\xBF  東京  \t PROPER_NOUN \t FAMILY \n"
      "  しほ  \t PROPER_NOUN \t GIVEN \n"
      "  なるほど  \t INTJ \n";

  auto result = parseDictionarySource(source);

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 3);
  EXPECT_EQ(result.value().entries[0].surface, "東京");
  EXPECT_EQ(result.value().entries[0].conj_type, ConjugationType::ProperFamily);
  EXPECT_EQ(result.value().entries[1].conj_type, ConjugationType::ProperGiven);
  EXPECT_EQ(result.value().entries[2].pos, core::PartOfSpeech::Interjection);
  EXPECT_EQ(result.value().entries[2].conj_type, ConjugationType::Interjection);
}

TEST(SourceParserTest, ParsesQuotedCsvEscapesAndEmbeddedNewline) {
  const std::string source =
      "\"東京,大阪\",NOUN,0.5,\"東\"\"京\"\n"
      "\"二行\n"
      "表記\",NOUN,0.5,\"基底\"\r\n";

  auto result = parseDictionarySource(source);

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 2);
  EXPECT_EQ(result.value().entries[0].surface, "東京,大阪");
  EXPECT_EQ(result.value().entries[0].lemma, "東\"京");
  EXPECT_EQ(result.value().entries[1].surface, "二行\n表記");
  EXPECT_EQ(result.value().entries[1].lemma, "基底");
  EXPECT_EQ(result.value().entries[1].line_number, 2);
  EXPECT_EQ(result.value().stats.empty_lines, 0);
}

TEST(SourceParserTest, PreservesLegacyTsvAndCurrentLemmaLayouts) {
  const std::string source =
      "読む\tVERB\tヨム\t0.5\tGODAN_MA\t読む\n"
      "東京\tNOUN\tトウキョウ\t0.5\n"
      "読んだ\tVERB\t読む\n";

  auto result = parseDictionarySource(source);

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 3);
  EXPECT_EQ(result.value().entries[0].conj_type, ConjugationType::GodanMa);
  EXPECT_EQ(result.value().entries[0].lemma, "読む");
  EXPECT_TRUE(result.value().entries[0].used_legacy_tsv_layout);
  EXPECT_TRUE(result.value().entries[1].lemma.empty());
  EXPECT_EQ(result.value().entries[2].lemma, "読む");
  ASSERT_EQ(result.value().warnings.size(), 2);
  EXPECT_NE(result.value().warnings[0].find("legacy TSV"), std::string::npos);
}

TEST(SourceParserTest, EmptySpreadsheetPaddingCannotChangeTheCurrentLayout) {
  auto result = parseDictionarySource("東京\tPROPER_NOUN\tFAMILY\t\t\n");

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 1);
  EXPECT_EQ(result.value().entries[0].conj_type, ConjugationType::ProperFamily);
  EXPECT_FALSE(result.value().entries[0].used_legacy_tsv_layout);
  EXPECT_TRUE(result.value().entries[0].ignored_empty_padding_columns);
  ASSERT_EQ(result.value().warnings.size(), 1);
  EXPECT_NE(result.value().warnings[0].find("padding"), std::string::npos);
}

TEST(SourceParserTest, WarnsWhenAnOpenDictionaryRegistersAClosedClass) {
  auto result = parseDictionarySource("ずら\tAUXILIARY\nべさ\tPARTICLE\n");

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().warnings.size(), 2);
  EXPECT_NE(result.value().warnings[0].find("closed-class POS AUX"), std::string::npos);
  EXPECT_NE(result.value().warnings[0].find("L1"), std::string::npos);
  EXPECT_NE(result.value().warnings[1].find("closed-class POS PARTICLE"), std::string::npos);
}

TEST(SourceParserTest, RejectsNonEmptyColumnsOutsideTheSelectedLayout) {
  auto current = parseDictionarySource("東京\tPROPER_NOUN\tFAMILY\t\tunexpected\n");
  auto legacy = parseDictionarySource("読む\tVERB\tヨム\t0.5\tGODAN_MA\t読む\tunexpected\n");
  auto ambiguous = parseDictionarySource("読んだ\tVERB\t読む\t別のレンマ\n");

  ASSERT_FALSE(current.hasValue());
  EXPECT_NE(current.error().message.find("column"), std::string::npos);
  ASSERT_FALSE(legacy.hasValue());
  EXPECT_NE(legacy.error().message.find("column"), std::string::npos);
  ASSERT_FALSE(ambiguous.hasValue());
  EXPECT_NE(ambiguous.error().message.find("columns 3 and 4"), std::string::npos);
}

TEST(SourceParserTest, PreservesLiteralQuoteInsideUnquotedTsvField) {
  auto result = parseDictionarySource(
      "引用\"符\tNOUN\n"
      "テスト\tNOUN\n");

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 2);
  EXPECT_EQ(result.value().entries[0].surface, "引用\"符");
  EXPECT_EQ(result.value().entries[1].surface, "テスト");
}

TEST(SourceParserTest, RejectsInvalidQuotedRecordWithoutPartialResult) {
  auto result = parseDictionarySource(
      "東京,NOUN,0.5\n"
      "\"大阪,NOUN,0.5\n");

  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("unterminated quoted field"), std::string::npos);
}

TEST(SourceParserTest, RejectsInvalidUtf8SurfaceAndLemma) {
  const std::string invalid_surface = std::string("\xE3\x81", 2) + "\tNOUN\n";
  const std::string invalid_lemma = std::string("検査\tNOUN\t") + std::string("\xE3\x81", 2) + "\n";

  auto surface_result = parseDictionarySource(invalid_surface);
  auto lemma_result = parseDictionarySource(invalid_lemma);

  ASSERT_FALSE(surface_result.hasValue());
  EXPECT_NE(surface_result.error().message.find("valid UTF-8"), std::string::npos);
  ASSERT_FALSE(lemma_result.hasValue());
  EXPECT_NE(lemma_result.error().message.find("valid UTF-8"), std::string::npos);
}

TEST(SourceParserTest, ConvertsMarkerToRuntimeExtendedPos) {
  auto parsed = parseDictionarySource(
      "東京\tPROPER_NOUN\n"
      "東京\tPROPER_NOUN\tFAMILY\n"
      "しほ\tPROPER_NOUN\tGIVEN\n"
      "ありがとう\tINTJ\n");

  ASSERT_TRUE(parsed.hasValue()) << parsed.error().message;
  ASSERT_EQ(parsed.value().entries.size(), 4);
  EXPECT_EQ(sourceToDictionaryEntry(parsed.value().entries[0]).extended_pos, core::ExtendedPOS::NounProper);
  EXPECT_EQ(sourceToDictionaryEntry(parsed.value().entries[1]).extended_pos, core::ExtendedPOS::NounProperFamily);
  EXPECT_EQ(sourceToDictionaryEntry(parsed.value().entries[2]).extended_pos, core::ExtendedPOS::NounProperGiven);
  EXPECT_EQ(sourceToDictionaryEntry(parsed.value().entries[3]).extended_pos, core::ExtendedPOS::InterjectionGreeting);
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
