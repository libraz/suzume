/**
 * @file interjection_quotative_test.cpp
 * @brief An interjection before the quotative って after a bare content word
 *
 * Where the interjection plus the sokuon is a dictionary verb's onbin cell, the
 * verb owns the span (手+はいっ+て). A quoted utterance after a vocative keeps
 * the interjection where no verb shares the surface.
 */

#include <gtest/gtest.h>

#include <string_view>

#include "core/types.h"
#include "suzume.h"

namespace suzume::analysis {
namespace {

Suzume makeAnalyzer() {
  SuzumeOptions options;
  options.skip_bundled_user_dictionary = true;
  return Suzume(options);
}

TEST(InterjectionQuotativeTest, VerbOnbinCellOwnsTheSpanAfterABareNoun) {
  auto analyzer = makeAnalyzer();
  for (const std::string_view text : {"手はいって", "水はいって"}) {
    const auto result = analyzer.analyze(text);
    ASSERT_EQ(result.size(), 3U) << text;
    EXPECT_EQ(result[1].surface, "はいっ") << text;
    EXPECT_EQ(result[1].pos, core::PartOfSpeech::Verb) << text;
    EXPECT_EQ(result[2].surface, "て") << text;
  }
}

TEST(InterjectionQuotativeTest, QuotedUtteranceAfterAVocativeKeepsTheInterjection) {
  auto analyzer = makeAnalyzer();
  for (const std::string_view text : {"先生おはようって言った", "部長おはようって言った", "こんにちはって"}) {
    const auto result = analyzer.analyze(text);
    bool has_interjection = false;
    for (const auto& morpheme : result) {
      has_interjection = has_interjection || morpheme.pos == core::PartOfSpeech::Interjection;
    }
    EXPECT_TRUE(has_interjection) << text;
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
