#include <gtest/gtest.h>

#include <string_view>

#include "suzume.h"

namespace suzume {
namespace analysis {
namespace {

Suzume makeAnalyzer() {
  SuzumeOptions options;
  options.skip_bundled_user_dictionary = true;
  return Suzume(options);
}

// A listed Ichidan stem hosts the one-mora conjectural as its irrealis, as an
// unlisted stem does (食べ+む).
TEST(ListedIchidanIrrealis, StemHostsOneMoraConjectural) {
  auto analyzer = makeAnalyzer();
  for (const std::string_view input : {"秀でむ", "人に秀でむ", "食べむ"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result.back().surface, "む");
    EXPECT_EQ(result.back().pos, core::PartOfSpeech::Auxiliary);
    EXPECT_EQ(result[result.size() - 2].pos, core::PartOfSpeech::Verb);
  }
}

// A final-particle spelling of the negative (ね) says nothing about the stem.
TEST(ListedIchidanIrrealis, FinalParticleSpellingKeepsContinuative) {
  auto analyzer = makeAnalyzer();
  const auto result = analyzer.analyze("外でねて");
  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[2].surface, "ね");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Verb);
}

}  // namespace
}  // namespace analysis
}  // namespace suzume
