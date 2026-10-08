#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "analysis/suffix_candidates.h"
#include "analysis/suffix_candidates_counter_internal.h"
#include "dictionary/dictionary.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "suzume.h"

namespace suzume::analysis {
namespace {

TEST(SuffixCandidatesCounterTest, FindsRepeatedNumeralKanjiUnitsOfVariableLength) {
  struct TestCase {
    std::string_view text;
    size_t repeated_end;
  };
  for (const TestCase& test_case :
       {TestCase{"一件一件点検する", 4}, TestCase{"十一件十一件点検する", 6}, TestCase{"3冊3冊点検する", 4}}) {
    const auto codepoints = normalize::toCodepoints(test_case.text);
    const auto char_types = normalize::classifyCodepoints(codepoints);
    EXPECT_EQ(repeatedNumeralNounUnitEndAt(codepoints, char_types, 0), test_case.repeated_end) << test_case.text;
  }
}

TEST(SuffixCandidatesCounterTest, DoesNotTreatPureNumeralRepetitionAsDistributiveUnit) {
  const auto codepoints = normalize::toCodepoints("十一十一点検する");
  const auto char_types = normalize::classifyCodepoints(codepoints);

  EXPECT_EQ(repeatedNumeralNounUnitEndAt(codepoints, char_types, 0), 0u);
}

TEST(SuffixCandidatesCounterTest, TemporalCandidatesAcceptNonQuantityAtSentenceStart) {
  const auto codepoints = normalize::toCodepoints("ありがとう");
  const auto char_types = normalize::classifyCodepoints(codepoints);
  dictionary::DictionaryManager dictionary_manager;
  std::vector<UnknownCandidate> candidates;

  counter_detail::appendTemporalCounterCandidates(codepoints, 0, char_types, &dictionary_manager, candidates);
}

TEST(SuffixCandidatesCounterTest, BoundsKanaCounterProbesOnLongHiraganaRun) {
  std::string text;
  for (size_t idx = 0; idx < 4000; ++idx) {
    text += "あ";
  }
  const auto codepoints = normalize::toCodepoints(text);
  const auto char_types = normalize::classifyCodepoints(codepoints);
  dictionary::DictionaryManager dictionary_manager;
  std::vector<UnknownCandidate> candidates;

  for (size_t start_pos = 0; start_pos < codepoints.size(); ++start_pos) {
    counter_detail::appendStructuralCounterCandidates(codepoints, start_pos, char_types, &dictionary_manager,
                                                      candidates);
  }

  EXPECT_TRUE(candidates.empty());
}

TEST(SuffixCandidatesCounterTest, KeepsRepeatedQuantityTogetherBeforeSuruPredicate) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  for (const std::string_view text :
       {"一件一件点検する", "一冊一冊点検する", "一日一日確認する", "十一件十一件点検する", "一語一語確認する"}) {
    const auto results = analyzer.analyze(text);
    ASSERT_FALSE(results.empty()) << text;
    const auto codepoints = normalize::toCodepoints(text);
    const auto char_types = normalize::classifyCodepoints(codepoints);
    const size_t repeated_end = repeatedNumeralNounUnitEndAt(codepoints, char_types, 0);
    ASSERT_NE(repeated_end, 0u) << text;
    EXPECT_EQ(results.front().surface, normalize::encodeRange(codepoints, 0, repeated_end)) << text;
  }
}

TEST(SuffixCandidatesCounterTest, ExtendsKeCounterThroughFollowingKanji) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  for (const std::string_view text : {"5ヶ国", "10ヶ年"}) {
    const auto results = analyzer.analyze(text);
    ASSERT_EQ(results.size(), 1u) << text;
    EXPECT_EQ(results.front().surface, text);
    EXPECT_EQ(results.front().pos, core::PartOfSpeech::Noun);
  }
}

}  // namespace
}  // namespace suzume::analysis
