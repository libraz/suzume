#include "suzume.h"

#include <gtest/gtest.h>

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

TEST_F(SuzumeApiTest, AnalyzeSimpleText) {
  Suzume instance(makeTestOptions());
  // "Tokyo is beautiful"
  auto results = instance.analyze(
      "\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84");
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

TEST_F(SuzumeApiTest, GenerateTagsReturnsResults) {
  Suzume instance(makeTestOptions());
  // "Tokyo is beautiful"
  auto tags = instance.generateTags(
      "\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84");
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

  auto tags = instance.generateTags(
      "\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84",
      tag_opts);
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
  auto results = instance.analyze(
      "\xe9\xa3\x9f\xe3\x81\xb9\xe3\x81\x9f");
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, AnalyzeWithMergeCompoundsOption) {
  SuzumeOptions opts = makeTestOptions();
  opts.merge_compounds = true;
  Suzume instance(opts);

  auto results = instance.analyze(
      "\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84");
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromInvalidPath) {
  Suzume instance(makeTestOptions());
  bool result = instance.loadUserDictionary("/nonexistent/path/dict.csv");
  EXPECT_FALSE(result);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromNullMemory) {
  Suzume instance(makeTestOptions());
  bool result = instance.loadUserDictionaryFromMemory(nullptr, 0);
  EXPECT_FALSE(result);
}

TEST_F(SuzumeApiTest, LoadBinaryDictionaryFromInvalidMemory) {
  Suzume instance(makeTestOptions());
  const uint8_t bad_data[] = {0x00, 0x01, 0x02, 0x03};
  bool result = instance.loadBinaryDictionary(bad_data, sizeof(bad_data));
  EXPECT_FALSE(result);
}

}  // namespace
}  // namespace suzume
