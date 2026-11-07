#include <gtest/gtest.h>

#include <array>
#include <string_view>
#include <vector>

#include "suzume.h"

namespace suzume {
namespace analysis {
namespace {

Suzume makeShortIchidanAnalyzer() {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  return Suzume(options);
}

void expectSurfaces(const std::vector<core::Morpheme>& results,
                    const std::vector<std::string_view>& expected_surfaces) {
  ASSERT_EQ(results.size(), expected_surfaces.size());
  for (size_t idx = 0; idx < expected_surfaces.size(); ++idx) {
    EXPECT_EQ(results[idx].surface, expected_surfaces[idx]) << "token index " << idx;
  }
}

TEST(ShortIchidanRelativeClause, SplitsOneMoraVerbBeforePastAndFormalNoun) {
  auto analyzer = makeShortIchidanAnalyzer();

  const auto first = analyzer.analyze("私のみたものを");
  expectSurfaces(first, {"私", "の", "み", "た", "もの", "を"});
  ASSERT_EQ(first.size(), 6u);
  EXPECT_EQ(first[2].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(first[2].lemma, "みる");
  EXPECT_EQ(first[3].pos, core::PartOfSpeech::Auxiliary);

  const auto second = analyzer.analyze("私のにたものを");
  expectSurfaces(second, {"私", "の", "に", "た", "もの", "を"});
  ASSERT_EQ(second.size(), 6u);
  EXPECT_EQ(second[2].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(second[2].lemma, "にる");
  EXPECT_EQ(second[3].pos, core::PartOfSpeech::Auxiliary);
}

TEST(ShortIchidanRelativeClause, KeepsNonPredicateAndNonFormalNounControls) {
  auto analyzer = makeShortIchidanAnalyzer();

  struct TestCase {
    std::string_view input;
    std::vector<std::string_view> expected_surfaces;
  };
  const std::array<TestCase, 6> cases = {{
      {"水をのむ", {"水", "を", "のむ"}},
      {"悲しみ", {"悲しみ"}},
      {"みどり", {"みどり"}},
      {"読んでみる", {"読ん", "で", "みる"}},
      {"私のきもの", {"私", "の", "きもの"}},
      {"私のくだもの", {"私", "の", "くだもの"}},
  }};

  for (const TestCase& test_case : cases) {
    SCOPED_TRACE(test_case.input);
    expectSurfaces(analyzer.analyze(test_case.input), test_case.expected_surfaces);
  }
}

}  // namespace
}  // namespace analysis
}  // namespace suzume
