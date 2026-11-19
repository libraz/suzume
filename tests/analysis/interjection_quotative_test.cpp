/**
 * @file interjection_quotative_test.cpp
 * @brief An interjection before the quotative って after a bare content word
 *
 * Where the interjection plus the sokuon is a dictionary verb's onbin cell, the
 * interjection is refused (手はいって is 手+は+いっ+て or 手+はいっ+て). A
 * quoted utterance after a vocative keeps the interjection where no verb
 * shares the surface.
 */

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "core/types.h"
#include "suzume.h"

namespace suzume::analysis {
namespace {

Suzume makeAnalyzer() {
  SuzumeOptions options;
  options.skip_bundled_user_dictionary = true;
  return Suzume(options);
}

bool hasInterjection(const std::vector<core::Morpheme>& result) {
  for (const auto& morpheme : result) {
    if (morpheme.pos == core::PartOfSpeech::Interjection) {
      return true;
    }
  }
  return false;
}

// 手はいって reads either 手+は+いっ+て or 手+はいっ+て; the guard only refuses
// the interjection, so the test does not choose between the two.
TEST(InterjectionQuotativeTest, NoInterjectionBeforeTheQuotativeAfterABareNoun) {
  auto analyzer = makeAnalyzer();
  for (const std::string_view text : {"手はいって", "水はいって"}) {
    EXPECT_FALSE(hasInterjection(analyzer.analyze(text))) << text;
  }
}

TEST(InterjectionQuotativeTest, QuotedUtteranceAfterAVocativeKeepsTheInterjection) {
  auto analyzer = makeAnalyzer();
  for (const std::string_view text : {"先生おはようって言った", "部長おはようって言った", "こんにちはって"}) {
    EXPECT_TRUE(hasInterjection(analyzer.analyze(text))) << text;
  }
}

TEST(InterjectionQuotativeTest, VocativeFollowUpWithoutTheQuotativeKeepsTheInterjection) {
  auto analyzer = makeAnalyzer();
  const auto result = analyzer.analyze("先生はい");
  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Interjection);
}

}  // namespace
}  // namespace suzume::analysis
