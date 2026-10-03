#include "tsv_parser.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

#include "dictionary/user_dict.h"

namespace suzume {
namespace cli {
namespace {

TEST(TsvParserTest, CliAndRuntimeShareBomAndMarkerParsing) {
  const std::string source =
      "\xEF\xBB\xBF  東京  \t PROPER_NOUN \t FAMILY \n"
      "しほ\tPROPER_NOUN\tGIVEN\n"
      "ありがとう\tINTJ\n";

  TsvParser parser;
  auto cli_result = parser.parseString(source);
  ASSERT_TRUE(cli_result.hasValue()) << cli_result.error().message;

  dictionary::UserDictionary runtime_dict;
  auto runtime_result = runtime_dict.loadFromMemory(source.data(), source.size());
  ASSERT_TRUE(runtime_result.hasValue()) << runtime_result.error().message;

  ASSERT_EQ(cli_result.value().size(), runtime_result.value());
  ASSERT_NE(runtime_dict.getEntry(0), nullptr);
  EXPECT_EQ(runtime_dict.getEntry(0)->surface, cli_result.value()[0].surface);
  EXPECT_EQ(runtime_dict.getEntry(0)->extended_pos, core::ExtendedPOS::NounProperFamily);
  ASSERT_NE(runtime_dict.getEntry(1), nullptr);
  EXPECT_EQ(runtime_dict.getEntry(1)->extended_pos, core::ExtendedPOS::NounProperGiven);
  ASSERT_NE(runtime_dict.getEntry(2), nullptr);
  EXPECT_EQ(runtime_dict.getEntry(2)->extended_pos, core::ExtendedPOS::InterjectionGreeting);
}

TEST(TsvParserTest, WritePreservesCanonicalMarkerAndLemma) {
  const std::filesystem::path output = std::filesystem::temp_directory_path() / "suzume_tsv_parser_roundtrip.tsv";
  const std::vector<TsvEntry> entries = {
      {"東京", core::PartOfSpeech::Noun, dictionary::ConjugationType::ProperFamily, "東京", 1},
      {"なるほど", core::PartOfSpeech::Interjection, dictionary::ConjugationType::Interjection, "なるほど", 2},
  };

  auto write_result = writeTsvFile(output.string(), entries);
  ASSERT_TRUE(write_result.hasValue()) << write_result.error().message;

  std::ifstream file(output);
  const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  std::filesystem::remove(output);

  TsvParser parser;
  auto parsed = parser.parseString(source);
  ASSERT_TRUE(parsed.hasValue()) << parsed.error().message;
  ASSERT_EQ(parsed.value().size(), entries.size());
  EXPECT_EQ(parsed.value()[0].conj_type, dictionary::ConjugationType::ProperFamily);
  EXPECT_EQ(parsed.value()[0].lemma, "東京");
  EXPECT_EQ(parsed.value()[1].conj_type, dictionary::ConjugationType::Interjection);
  EXPECT_EQ(parsed.value()[1].lemma, "なるほど");
}

TEST(TsvParserTest, ValidateReportsSpreadsheetPaddingColumns) {
  TsvParser parser;
  auto parsed = parser.parseString("東京\tPROPER_NOUN\tFAMILY\t\t\n");
  ASSERT_TRUE(parsed.hasValue()) << parsed.error().message;

  std::vector<std::string> issues;
  EXPECT_EQ(TsvParser::validate(parsed.value(), &issues), 1);
  ASSERT_EQ(issues.size(), 1);
  EXPECT_NE(issues[0].find("padding columns"), std::string::npos);
}

TEST(TsvParserTest, ValidateOmitsSyntheticLineZero) {
  const std::vector<TsvEntry> entries = {
      {"検査する", core::PartOfSpeech::Verb, dictionary::ConjugationType::None, "", 0},
  };
  std::vector<std::string> issues;
  EXPECT_EQ(TsvParser::validate(entries, &issues), 1);
  ASSERT_EQ(issues.size(), 1);
  EXPECT_EQ(issues[0], "Missing conjugation type: 検査する");
}

TEST(TsvParserTest, WriteFailurePreservesDestinationAndRemovesTemporaryFile) {
  const std::filesystem::path output =
      std::filesystem::temp_directory_path() / "suzume_tsv_parser_directory_destination";
  std::filesystem::remove_all(output);
  ASSERT_TRUE(std::filesystem::create_directory(output));

  const std::vector<TsvEntry> entries = {
      {"東京", core::PartOfSpeech::Noun, dictionary::ConjugationType::None, "", 1},
  };
  const auto write_result = writeTsvFile(output.string(), entries);

  EXPECT_FALSE(write_result.hasValue());
  EXPECT_TRUE(std::filesystem::is_directory(output));
  EXPECT_FALSE(std::filesystem::exists(output.string() + ".tmp"));
  std::filesystem::remove_all(output);
}

TEST(TsvParserTest, WriterRejectsFieldsThatWouldNotRoundTrip) {
  const std::filesystem::path output = std::filesystem::temp_directory_path() / "suzume_tsv_parser_invalid.tsv";
  std::filesystem::remove(output);

  const std::vector<TsvEntry> invalid_surface = {
      {"#見出し", core::PartOfSpeech::Noun, dictionary::ConjugationType::None, "", 1},
  };
  const auto surface_result = writeTsvFile(output.string(), invalid_surface);
  ASSERT_FALSE(surface_result.hasValue());
  EXPECT_NE(surface_result.error().message.find("cannot begin with #"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(output));

  const std::vector<TsvEntry> invalid_lemma = {
      {"見出し", core::PartOfSpeech::Noun, dictionary::ConjugationType::None, "別\t表記", 1},
  };
  const auto lemma_result = writeTsvFile(output.string(), invalid_lemma);
  ASSERT_FALSE(lemma_result.hasValue());
  EXPECT_NE(lemma_result.error().message.find("cannot contain a tab"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(output));
}

}  // namespace
}  // namespace cli
}  // namespace suzume
