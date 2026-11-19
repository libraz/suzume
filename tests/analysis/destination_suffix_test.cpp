#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

Suzume makeAnalyzer() {
  SuzumeOptions options;
  options.skip_bundled_user_dictionary = true;
  return Suzume(options);
}

TEST(DestinationSuffix, JoinsDictionaryNounHostIntoSearchUnit) {
  auto analyzer = makeAnalyzer();
  const auto result = analyzer.analyze("学校行きの便");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "学校行き");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[1].surface, "の");
  EXPECT_EQ(result[2].surface, "便");
}

TEST(DestinationSuffix, JoinsUnknownNounHostWithoutPlaceNameEntry) {
  auto analyzer = makeAnalyzer();
  const auto result = analyzer.analyze("東京行き");

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].surface, "東京行き");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
}

TEST(DestinationSuffix, KeepsCaseMarkedMovementVerbSeparate) {
  auto analyzer = makeAnalyzer();
  const auto result = analyzer.analyze("東京へ行きます");

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[0].surface, "東京");
  EXPECT_EQ(result[1].surface, "へ");
  EXPECT_EQ(result[2].surface, "行き");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[3].surface, "ます");
}

TEST(DestinationSuffix, KeepsStandaloneRenyokeiVerb) {
  auto analyzer = makeAnalyzer();
  const auto result = analyzer.analyze("行き");

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].surface, "行き");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "行く");
}

}  // namespace
}  // namespace suzume::analysis
