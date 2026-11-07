#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "analysis/adjective_candidates.h"
#include "analysis/unknown.h"
#include "suzume.h"

namespace suzume {
namespace analysis {
namespace {

TEST(KanaUOnbinAdjective, RegisteredAdjectiveBeforeLexicalVerbHasContinuativeMetadata) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto* input :
       {"ようできました", "ようできた", "ようできます", "ようなりました", "よう出来ました", "よう見える"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0].surface, "よう");
    EXPECT_EQ(result[0].lemma, "よい");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
    EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjRenyokei);
    EXPECT_EQ(result[0].conj_type, dictionary::ConjugationType::IAdjective);
    EXPECT_EQ(result[0].conj_form, grammar::ConjForm::Renyokei);
    EXPECT_EQ(result[0].start, 0U);
    EXPECT_EQ(result[0].end, 2U);
    EXPECT_EQ(result[1].start, 2U);
    EXPECT_EQ(result[1].pos, core::PartOfSpeech::Verb);
  }
  const auto result = analyzer.analyze("すごう見える");
  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].surface, "すごう");
  EXPECT_EQ(result[0].lemma, "すごい");
  EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjRenyokei);
  EXPECT_EQ(result[0].end, 3U);
}

TEST(KanaUOnbinAdjective, PreservesNounsDemonstrativesAndReduplicatedAdverbs) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto* input : {"ようだ", "ようです"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[0].surface, "よう");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  }
  for (const auto* input : {"そのようにできました", "できるようになった"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 3U);
    EXPECT_EQ(result[1].surface, "よう");
    EXPECT_EQ(result[1].pos, core::PartOfSpeech::Noun);
    EXPECT_EQ(result[2].surface, "に");
  }
  for (const auto& [input, surface] : {std::pair{"こうできました", "こう"}, std::pair{"そう見える", "そう"},
                                       std::pair{"とうとうなりました", "とうとう"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0].surface, surface);
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adverb);
  }
  const auto lexical = analyzer.analyze("ことなりました");
  ASSERT_EQ(lexical.size(), 3U);
  EXPECT_EQ(lexical[0].surface, "ことなり");
  EXPECT_EQ(lexical[0].lemma, "ことなる");
  EXPECT_EQ(lexical[0].pos, core::PartOfSpeech::Verb);
}

TEST(KanaUOnbinAdjective, DoesNotInferPredicateAcrossPunctuationOrInsideKanaTail) {
  dictionary::DictionaryManager manager;
  grammar::Inflection inflection;
  for (const std::u32string text : {U"よう、見える", U"よう見えるめ", U"ようあいう", U"よう"}) {
    const std::vector<char32_t> codepoints(text.begin(), text.end());
    std::vector<UnknownCandidate> candidates;
    appendIAdjOnbinRenyokeiCandidates(codepoints, 0, 0, 2, inflection, &manager, candidates);
    EXPECT_TRUE(candidates.empty());
  }
  const std::u32string text = U"よう見える";
  const std::vector<char32_t> codepoints(text.begin(), text.end());
  std::vector<UnknownCandidate> candidates;
  appendIAdjOnbinRenyokeiCandidates(codepoints, 0, 0, 2, inflection, &manager, candidates);
  ASSERT_EQ(candidates.size(), 1U);
  EXPECT_TRUE(candidates[0].lemma_verified);
  EXPECT_EQ(candidates[0].lemma, "よい");
}

TEST(KanaUOnbinAdjective, PreservesKanjiAndGlideContinuatives) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto& [input, surface] :
       {std::pair{"良うできました", "良う"}, std::pair{"よろしゅうございます", "よろしゅう"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0].surface, surface);
    EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjRenyokei);
    EXPECT_EQ(result[0].conj_form, grammar::ConjForm::Renyokei);
  }
}

}  // namespace
}  // namespace analysis
}  // namespace suzume
