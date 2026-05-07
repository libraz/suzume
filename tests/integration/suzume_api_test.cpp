#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "suzume.h"

namespace suzume {
namespace {

class SuzumeApiTest : public ::testing::Test {
 protected:
  SuzumeOptions makeTestOptions() {
    SuzumeOptions opts;
    opts.skip_user_dictionary = true;
    return opts;
  }
};

TEST_F(SuzumeApiTest, DefaultConstructorCreatesInstance) {
  Suzume instance;
  // Should not crash, instance is valid
  EXPECT_FALSE(Suzume::version().empty());
}

TEST_F(SuzumeApiTest, ConstructWithOptions) {
  SuzumeOptions opts = makeTestOptions();
  opts.lemmatize = true;
  opts.merge_compounds = false;
  Suzume instance(opts);
  // Instance created successfully with custom options
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Normal);
}

TEST_F(SuzumeApiTest, ConstructWithSkipUserDictionary) {
  SuzumeOptions opts;
  opts.skip_user_dictionary = true;
  Suzume instance(opts);
  auto results = instance.analyze("\xe6\x9d\xb1\xe4\xba\xac");  // Tokyo
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, VersionReturnsNonEmptyString) {
  std::string ver = Suzume::version();
  EXPECT_FALSE(ver.empty());
}

TEST_F(SuzumeApiTest, ModeDefaultsToNormal) {
  Suzume instance(makeTestOptions());
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Normal);
}

TEST_F(SuzumeApiTest, SetModeRoundtrip) {
  Suzume instance(makeTestOptions());

  instance.setMode(core::AnalysisMode::Search);
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Search);

  instance.setMode(core::AnalysisMode::Split);
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Split);

  instance.setMode(core::AnalysisMode::Normal);
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Normal);
}

TEST_F(SuzumeApiTest, SplitModeDisablesMixedScriptJoinCandidates) {
  SuzumeOptions normal_opts = makeTestOptions();
  normal_opts.mode = core::AnalysisMode::Normal;
  Suzume normal(normal_opts);

  SuzumeOptions split_opts = makeTestOptions();
  split_opts.mode = core::AnalysisMode::Split;
  Suzume split(split_opts);

  auto normal_results = normal.analyze("API開発");
  auto split_results = split.analyze("API開発");

  ASSERT_FALSE(normal_results.empty());
  ASSERT_FALSE(split_results.empty());
  EXPECT_EQ(normal_results.front().surface, "API開発");
  EXPECT_GT(split_results.size(), normal_results.size());
}

TEST_F(SuzumeApiTest, SetModeUpdatesTokenizerAndPostprocessor) {
  Suzume instance(makeTestOptions());

  auto normal_results = instance.analyze("API開発");
  instance.setMode(core::AnalysisMode::Split);
  auto split_results = instance.analyze("API開発");

  ASSERT_FALSE(normal_results.empty());
  ASSERT_FALSE(split_results.empty());
  EXPECT_EQ(normal_results.front().surface, "API開発");
  EXPECT_GT(split_results.size(), normal_results.size());
}

TEST_F(SuzumeApiTest, AnalyzeSimpleText) {
  Suzume instance(makeTestOptions());
  // "Tokyo is beautiful"
  auto results = instance.analyze("\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84");
  EXPECT_FALSE(results.empty());

  // Check that surfaces are non-empty
  for (const auto& morpheme : results) {
    EXPECT_FALSE(morpheme.surface.empty());
  }
}

TEST_F(SuzumeApiTest, AnalyzeReturnsNonEmptyForJapanese) {
  Suzume instance(makeTestOptions());
  // "eat" (taberu)
  auto results = instance.analyze("\xe9\xa3\x9f\xe3\x81\xb9\xe3\x82\x8b");
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, AnalyzeEmptyTextReturnsEmpty) {
  Suzume instance(makeTestOptions());
  auto results = instance.analyze("");
  EXPECT_TRUE(results.empty());
}

TEST_F(SuzumeApiTest, AnalyzeSingleCharacter) {
  Suzume instance(makeTestOptions());
  // Single kanji "mountain"
  auto results = instance.analyze("\xe5\xb1\xb1");
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, PretokenizedMorphemesHaveExtendedPos) {
  Suzume instance(makeTestOptions());
  auto results = instance.analyze("https://example.com");
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(results[0].extended_pos, core::ExtendedPOS::Noun);
}

TEST_F(SuzumeApiTest, GenerateTagsReturnsResults) {
  Suzume instance(makeTestOptions());
  // "Tokyo is beautiful"
  auto tags = instance.generateTags("\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84");
  EXPECT_FALSE(tags.empty());

  // Tags should have non-empty tag strings
  for (const auto& entry : tags) {
    EXPECT_FALSE(entry.tag.empty());
  }
}

TEST_F(SuzumeApiTest, GenerateTagsWithCustomOptions) {
  Suzume instance(makeTestOptions());
  postprocess::TagGeneratorOptions tag_opts;
  tag_opts.exclude_particles = true;
  tag_opts.min_tag_length = 1;

  auto tags =
      instance.generateTags("\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84", tag_opts);
  EXPECT_FALSE(tags.empty());
}

TEST_F(SuzumeApiTest, GenerateTagsEmptyText) {
  Suzume instance(makeTestOptions());
  auto tags = instance.generateTags("");
  EXPECT_TRUE(tags.empty());
}

TEST_F(SuzumeApiTest, MoveConstruct) {
  Suzume src(makeTestOptions());
  Suzume dst = std::move(src);

  // Moved-to instance should work
  auto results = dst.analyze("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");  // test
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, MoveAssign) {
  Suzume src(makeTestOptions());
  Suzume dst(makeTestOptions());
  dst = std::move(src);

  // Moved-to instance should work
  auto results = dst.analyze("\xe3\x82\x8a\xe3\x82\x93\xe3\x81\x94");  // ringo
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, AnalyzeWithLemmatizeOption) {
  SuzumeOptions opts = makeTestOptions();
  opts.lemmatize = true;
  Suzume instance(opts);

  // "ate" (tabeta) - past tense of taberu
  auto results = instance.analyze("\xe9\xa3\x9f\xe3\x81\xb9\xe3\x81\x9f");
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, AnalyzeWithMergeCompoundsOption) {
  SuzumeOptions opts = makeTestOptions();
  opts.merge_compounds = true;
  Suzume instance(opts);

  auto results = instance.analyze("\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84");
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromInvalidPath) {
  Suzume instance(makeTestOptions());
  bool result = instance.loadUserDictionary("/nonexistent/path/dict.csv");
  EXPECT_FALSE(result);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryResultReportsInvalidPath) {
  Suzume instance(makeTestOptions());
  auto result = instance.loadUserDictionaryResult("/nonexistent/path/dict.csv");
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Failed to open dictionary file"), std::string::npos);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromNullMemory) {
  Suzume instance(makeTestOptions());
  bool result = instance.loadUserDictionaryFromMemory(nullptr, 0);
  EXPECT_FALSE(result);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromMemoryResultReportsParseError) {
  Suzume instance(makeTestOptions());
  const char* csv_data = "\"東京,NOUN,0.5\n";
  auto result = instance.loadUserDictionaryFromMemoryResult(csv_data, std::strlen(csv_data));
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("unterminated quoted field"), std::string::npos);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromMemoryResultReportsEntryCount) {
  Suzume instance(makeTestOptions());
  const char* csv_data =
      "東京,NOUN,0.5\n"
      "大阪,NOUN,0.5\n";
  auto result = instance.loadUserDictionaryFromMemoryResult(csv_data, std::strlen(csv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 2u);
}

TEST_F(SuzumeApiTest, LoadBinaryDictionaryFromInvalidMemory) {
  Suzume instance(makeTestOptions());
  const uint8_t bad_data[] = {0x00, 0x01, 0x02, 0x03};
  bool result = instance.loadBinaryDictionary(bad_data, sizeof(bad_data));
  EXPECT_FALSE(result);
}

TEST_F(SuzumeApiTest, LoadBinaryDictionaryResultReportsParseError) {
  Suzume instance(makeTestOptions());
  const uint8_t bad_data[] = {0x00, 0x01, 0x02, 0x03};
  auto result = instance.loadBinaryDictionaryResult(bad_data, sizeof(bad_data));
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Dictionary file too small"), std::string::npos);
}

TEST_F(SuzumeApiTest, AutoDictionaryLoadWarningsAreRecorded) {
#ifndef __EMSCRIPTEN__
  namespace fs = std::filesystem;

  const char* old_data_dir = std::getenv("SUZUME_DATA_DIR");
  std::string old_value = old_data_dir != nullptr ? old_data_dir : "";

  fs::path dir = fs::temp_directory_path() / "suzume_bad_dict_test";
  fs::create_directories(dir);
  {
    std::ofstream file(dir / "core.dic", std::ios::binary);
    const char bad_data[] = {0x00, 0x01, 0x02, 0x03};
    file.write(bad_data, sizeof(bad_data));
  }

  setenv("SUZUME_DATA_DIR", dir.string().c_str(), 1);
  SuzumeOptions opts = makeTestOptions();
  Suzume instance(opts);
  if (old_data_dir != nullptr) {
    setenv("SUZUME_DATA_DIR", old_value.c_str(), 1);
  } else {
    unsetenv("SUZUME_DATA_DIR");
  }
  fs::remove_all(dir);

  auto warnings = instance.dictionaryWarnings();
  ASSERT_FALSE(warnings.empty());
  EXPECT_NE(warnings.front().find("Failed to auto-load dictionary"), std::string::npos);
  EXPECT_NE(warnings.front().find("Dictionary file too small"), std::string::npos);
#endif
}

TEST_F(SuzumeApiTest, ScorerEnvWarningsAreSilentByDefault) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_UNARY_noun_prior", "not-a-number", 1);
  testing::internal::CaptureStderr();
  { Suzume instance(makeTestOptions()); }
  std::string stderr_output = testing::internal::GetCapturedStderr();
  unsetenv("SUZUME_SCORER_UNARY_noun_prior");

  EXPECT_TRUE(stderr_output.empty());
#endif
}

TEST_F(SuzumeApiTest, ScorerEnvWarningsCanBeReported) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_UNARY_noun_prior", "not-a-number", 1);
  testing::internal::CaptureStderr();
  {
    SuzumeOptions opts = makeTestOptions();
    opts.report_scorer_config = true;
    Suzume instance(opts);
  }
  std::string stderr_output = testing::internal::GetCapturedStderr();
  unsetenv("SUZUME_SCORER_UNARY_noun_prior");

  EXPECT_NE(stderr_output.find("Invalid value"), std::string::npos);
#endif
}

}  // namespace
}  // namespace suzume
