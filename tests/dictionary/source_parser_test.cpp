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

TEST(SourceParserTest, ParsesQuotedTsvEscapesAndEmbeddedNewline) {
  const std::string source =
      "\"東京\t大阪\"\tNOUN\t\t\"東\"\"京\"\n"
      "\"二行\n"
      "表記\"\tNOUN\t\t\"基底\"\r\n";

  auto result = parseDictionarySource(source);

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 2);
  EXPECT_EQ(result.value().entries[0].surface, "東京\t大阪");
  EXPECT_EQ(result.value().entries[0].lemma, "東\"京");
  EXPECT_EQ(result.value().entries[1].surface, "二行\n表記");
  EXPECT_EQ(result.value().entries[1].lemma, "基底");
  EXPECT_EQ(result.value().entries[1].line_number, 2);
  EXPECT_EQ(result.value().stats.empty_lines, 0);
  EXPECT_TRUE(result.value().warnings.empty());
}

void expectParseError(std::string_view source, const std::string& message) {
  auto result = parseDictionarySource(source);
  ASSERT_FALSE(result.hasValue()) << source;
  EXPECT_EQ(result.error().code, core::ErrorCode::ParseError) << source;
  EXPECT_EQ(result.error().message, message) << source;
}

TEST(SourceParserTest, RejectsLegacyTsvLayoutWithNumericCostInColumnFour) {
  expectParseError("東京公園\tNOUN\n東京\tNOUN\tトウキョウ\t0.5\n",
                   "Removed legacy TSV layout at line 2: column 4 is a numeric cost; rows are surface, POS[, "
                   "conj_type][, base_form]");
  expectParseError("# comment\n読む\tVERB\tヨム\t0.5\tGODAN_MA\t読む\n",
                   "Removed legacy TSV layout at line 2: column 4 is a numeric cost; rows are surface, POS[, "
                   "conj_type][, base_form]");
}

TEST(SourceParserTest, RejectsNumericCostInColumnThree) {
  for (const char* source : {"東京公園\tNOUN\nword\tNOUN\t0.05\n", "東京公園\tNOUN\nword\tNOUN\t0.05\tword\n",
                             "東京公園\tNOUN\nword\tNOUN\t-1\t\n"}) {
    expectParseError(source,
                     "Removed cost column at line 2: column 3 is numeric; rows are surface, POS[, conj_type][, "
                     "base_form]");
  }
}

TEST(SourceParserTest, RejectsLegacyCsvLayout) {
  expectParseError("食べた,VERB,0.5,食べる\n", "Removed legacy CSV layout at line 1; use tab-separated rows");
  expectParseError("\n東京,NOUN\n", "Removed legacy CSV layout at line 2; use tab-separated rows");
}

TEST(SourceParserTest, ReadsNumericBaseFormFromColumnFour) {
  auto result = parseDictionarySource("三\tNOUN\t\t3\n読んだ\tVERB\t読む\n");

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 2u);
  EXPECT_EQ(result.value().entries[0].lemma, "3");
  EXPECT_EQ(result.value().entries[1].lemma, "読む");
  EXPECT_TRUE(result.value().warnings.empty());
}

TEST(SourceParserTest, EmptySpreadsheetPaddingCannotChangeTheCurrentLayout) {
  auto result = parseDictionarySource("東京\tPROPER_NOUN\tFAMILY\t\t\n");

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 1);
  EXPECT_EQ(result.value().entries[0].conj_type, ConjugationType::ProperFamily);
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
  auto ambiguous = parseDictionarySource("読んだ\tVERB\t読む\t別のレンマ\n");

  ASSERT_FALSE(current.hasValue());
  EXPECT_NE(current.error().message.find("column"), std::string::npos);
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
      "東京\tNOUN\n"
      "\"大阪\tNOUN\n");

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

TEST(SourceParserTest, SkipsCommentsBlankLinesAndHandlesCrLfWithoutFinalNewline) {
  const std::string source =
      "  # indented comment\n"
      "\t# tab-indented comment\n"
      "\n"
      "東京\tNOUN\r\n"
      "テスト\tNOUN";

  auto result = parseDictionarySource(source);

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  ASSERT_EQ(result.value().entries.size(), 2u);
  EXPECT_EQ(result.value().entries[0].surface, "東京");
  EXPECT_EQ(result.value().entries[1].surface, "テスト");
}

TEST(SourceParserTest, AcceptsPosAliases) {
  const std::string source =
      "東京\tPROPN\n"
      "静か\tADJECTIVE\n"
      "すぐ\tADVERB\n"
      "です\tAUXILIARY\n"
      "ね\tINTERJECTION\n"
      "記号\tSYM\n";

  auto result = parseDictionarySource(source);

  ASSERT_TRUE(result.hasValue()) << result.error().message;
  const auto& entries = result.value().entries;
  ASSERT_EQ(entries.size(), 6u);
  EXPECT_EQ(entries[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(entries[1].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(entries[2].pos, core::PartOfSpeech::Adverb);
  EXPECT_EQ(entries[3].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(entries[4].pos, core::PartOfSpeech::Interjection);
  EXPECT_EQ(entries[5].pos, core::PartOfSpeech::Symbol);
}

TEST(SourceParserTest, RejectsInvalidPosAndEmptyFields) {
  auto invalid_pos = parseDictionarySource("東京\tBADPOS\n");
  ASSERT_FALSE(invalid_pos.hasValue());
  EXPECT_NE(invalid_pos.error().message.find("Invalid POS at line 1: BADPOS"), std::string::npos);

  auto empty_surface = parseDictionarySource("\tNOUN\n");
  ASSERT_FALSE(empty_surface.hasValue());
  EXPECT_NE(empty_surface.error().message.find("Empty surface at line 1"), std::string::npos);

  auto quoted_empty_surface = parseDictionarySource("\"\"\tNOUN\n");
  ASSERT_FALSE(quoted_empty_surface.hasValue());
  EXPECT_NE(quoted_empty_surface.error().message.find("Empty surface at line 1"), std::string::npos);

  auto empty_pos = parseDictionarySource("東京\t\n");
  ASSERT_FALSE(empty_pos.hasValue());
  EXPECT_NE(empty_pos.error().message.find("Empty POS at line 1"), std::string::npos);
}

TEST(SourceParserTest, RejectsTextAfterClosingQuoteAndKeepsQuotedFields) {
  auto trailing = parseDictionarySource("\"東京\"bad\tNOUN\n");
  ASSERT_FALSE(trailing.hasValue());
  EXPECT_NE(trailing.error().message.find("unexpected character after closing quote"), std::string::npos);

  auto quoted = parseDictionarySource("\"東京\t大阪\"\tNOUN\t\t\"東\"\"阪\"\n");
  ASSERT_TRUE(quoted.hasValue()) << quoted.error().message;
  ASSERT_EQ(quoted.value().entries.size(), 1u);
  EXPECT_EQ(quoted.value().entries[0].surface, "東京\t大阪");
  EXPECT_EQ(quoted.value().entries[0].lemma, "東\"阪");
}

TEST(SourceParserTest, SkipsSingleFieldRecordsWhenRequested) {
  const std::string source =
      "東京\n"
      "大阪\tNOUN\n";

  SourceParseOptions skip;
  skip.skip_single_field_records = true;
  auto skipped = parseDictionarySource(source, skip);
  ASSERT_TRUE(skipped.hasValue()) << skipped.error().message;
  ASSERT_EQ(skipped.value().entries.size(), 1u);
  EXPECT_EQ(skipped.value().entries[0].surface, "大阪");
}

TEST(SourceParserTest, ReadsLemmaFromTsvButNotCompilerConjugationMarker) {
  auto tsv = parseDictionarySource("食べた\tVERB\t食べる\n");
  ASSERT_TRUE(tsv.hasValue()) << tsv.error().message;
  EXPECT_EQ(tsv.value().entries[0].lemma, "食べる");

  auto marker = parseDictionarySource("食べる\tVERB\tIchidan\n");
  ASSERT_TRUE(marker.hasValue()) << marker.error().message;
  EXPECT_TRUE(marker.value().entries[0].lemma.empty());
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
