#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "analysis/suffix_candidates.h"
#include "analysis/suffix_candidates_counter_internal.h"
#include "analysis/tokenizer.h"
#include "core/lattice.h"
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

TEST(SuffixCandidatesCounterTest, PreservesFiniteExistentialAfterCountedSubject) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  const auto existing_quantity = analyzer.analyze("3人いた");
  ASSERT_EQ(existing_quantity.size(), 3U);
  ASSERT_EQ(existing_quantity[1].lemma, "いる");
  ASSERT_EQ(existing_quantity[1].extended_pos, core::ExtendedPOS::VerbRenyokei);

  struct TestCase {
    std::string_view text;
    std::string_view quantity;
    std::string_view finite_auxiliary;
    bool has_final_particle;
  };
  for (const TestCase& test_case :
       {TestCase{"三人いた", "三人", "た", false}, TestCase{"二人いますか", "二人", "ます", true},
        TestCase{"何人いますか", "何人", "ます", true}, TestCase{"幾人いましたか", "幾人", "ました", true},
        TestCase{"何人いた", "何人", "た", false}}) {
    SCOPED_TRACE(test_case.text);
    const auto results = analyzer.analyze(test_case.text);
    ASSERT_GE(results.size(), test_case.has_final_particle ? 4U : 3U);
    EXPECT_EQ(results[0].surface, test_case.quantity);
    EXPECT_EQ(results[0].pos, core::PartOfSpeech::Noun);
    EXPECT_EQ(results[0].start, 0U);
    EXPECT_EQ(results[0].end, normalize::toCodepoints(test_case.quantity).size());

    const auto& existential = results[1];
    EXPECT_EQ(existential.surface, "い");
    EXPECT_EQ(existential.lemma, "いる");
    EXPECT_EQ(existential.pos, core::PartOfSpeech::Verb);
    EXPECT_EQ(existential.extended_pos, core::ExtendedPOS::VerbRenyokei);
    EXPECT_EQ(existential.conj_type, existing_quantity[1].conj_type);
    EXPECT_EQ(existential.conj_form, grammar::ConjForm::Renyokei);
    EXPECT_EQ(existential.start, results[0].end);
    EXPECT_EQ(existential.end, existential.start + 1);

    std::string auxiliary_surface;
    size_t next_start = existential.end;
    size_t result_idx = 2;
    while (result_idx < results.size() && results[result_idx].pos == core::PartOfSpeech::Auxiliary) {
      EXPECT_EQ(results[result_idx].start, next_start);
      next_start = results[result_idx].end;
      auxiliary_surface += results[result_idx].surface;
      ++result_idx;
    }
    EXPECT_EQ(auxiliary_surface, test_case.finite_auxiliary);
    if (test_case.has_final_particle) {
      ASSERT_EQ(result_idx + 1, results.size());
      EXPECT_EQ(results[result_idx].surface, "か");
      EXPECT_EQ(results[result_idx].pos, core::PartOfSpeech::Particle);
      EXPECT_EQ(results[result_idx].start, next_start);
    } else {
      EXPECT_EQ(result_idx, results.size());
    }
  }
}

TEST(SuffixCandidatesCounterTest, DirectInterrogativeCountersStopAtKanjiWordBoundary) {
  dictionary::DictionaryManager dictionary_manager;
  for (const auto& [text, expected_surface] :
       {std::pair<std::string_view, std::string_view>{"何人いますか", "何人"}, {"幾人いましたか", "幾人"}}) {
    const std::string_view expected_counter = expected_surface;
    const auto codepoints = normalize::toCodepoints(text);
    const auto char_types = normalize::classifyCodepoints(codepoints);
    std::vector<UnknownCandidate> candidates;
    counter_detail::appendStructuralCounterCandidates(codepoints, 0, char_types, &dictionary_manager, candidates);
    const auto counter = std::find_if(candidates.begin(), candidates.end(), [&](const auto& candidate) {
      return candidate.origin == core::CandidateOrigin::Counter && candidate.surface == expected_counter &&
             candidate.pos == core::PartOfSpeech::Noun && candidate.extended_pos == core::ExtendedPOS::Noun;
    });
    ASSERT_NE(counter, candidates.end()) << text;
    EXPECT_EQ(counter->start, 0U);
    EXPECT_EQ(counter->end, normalize::toCodepoints(expected_surface).size());
  }

  const auto lexical_text = normalize::toCodepoints("何人家族");
  const auto lexical_types = normalize::classifyCodepoints(lexical_text);
  std::vector<UnknownCandidate> lexical_candidates;
  counter_detail::appendStructuralCounterCandidates(lexical_text, 0, lexical_types, &dictionary_manager,
                                                    lexical_candidates);
  EXPECT_TRUE(std::none_of(lexical_candidates.begin(), lexical_candidates.end(), [](const auto& candidate) {
    return candidate.origin == core::CandidateOrigin::Counter && candidate.start == 0 && candidate.end > 2;
  }));
}

TEST(SuffixCandidatesCounterTest, LatticeMarksOnlyFiniteExistentialCounterContinuations) {
  dictionary::DictionaryManager dictionary_manager;
  ASSERT_TRUE(dictionary_manager.loadCoreDictionaryResult("data/core.dic").hasValue());
  const UnknownWordGenerator generator({}, &dictionary_manager);
  const Scorer scorer;
  const Tokenizer tokenizer(dictionary_manager, scorer, generator);

  struct TestCase {
    std::string_view text;
    std::string_view counter_surface;
    bool has_finite_existential_continuation;
  };
  for (const TestCase& test_case : {TestCase{"三人いた", "三人", true}, TestCase{"二人いますか", "二人", true},
                                    TestCase{"幾人いましたか", "幾人", true}, TestCase{"何人いた", "何人", true},
                                    TestCase{"二人いっしょ", "二人", false}, TestCase{"三人いたずら", "三人", false},
                                    TestCase{"三人いたい", "三人", false}, TestCase{"三人いまし", "三人", false}}) {
    SCOPED_TRACE(test_case.text);
    const auto codepoints = normalize::toCodepoints(test_case.text);
    const auto lattice = tokenizer.buildLattice(test_case.text, codepoints, normalize::classifyCodepoints(codepoints));
    const auto& starting_edges = lattice.edgeIdsAt(0);
    const auto counter_edge = std::find_if(starting_edges.begin(), starting_edges.end(), [&](uint32_t edge_id) {
      const auto& edge = lattice.getEdge(edge_id);
      return edge.surface == test_case.counter_surface && edge.pos == core::PartOfSpeech::Noun &&
             edge.origin == core::CandidateOrigin::Counter;
    });
    ASSERT_NE(counter_edge, starting_edges.end()) << "Counter noun edge should be present";
    EXPECT_EQ(lattice.getEdge(*counter_edge).has_finite_existential_continuation,
              test_case.has_finite_existential_continuation);
  }
}

TEST(SuffixCandidatesCounterTest, KeepsExistingVerbAndCounterControls) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  for (const auto& [text, first_surface, first_lemma] :
       {std::tuple<std::string_view, std::string_view, std::string_view>{"3人いた", "3人", "3人"},
        {"咲いた", "咲い", "咲く"},
        {"構います", "構い", "構う"}}) {
    SCOPED_TRACE(text);
    const auto results = analyzer.analyze(text);
    ASSERT_GE(results.size(), 2U);
    EXPECT_EQ(results[0].surface, first_surface);
    EXPECT_EQ(results[0].lemma, first_lemma);
  }
  const auto existential = analyzer.analyze("三人いる");
  ASSERT_EQ(existential.size(), 2U);
  EXPECT_EQ(existential[0].surface, "三人");
  EXPECT_EQ(existential[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(existential[1].surface, "いる");
  EXPECT_EQ(existential[1].lemma, "いる");
  EXPECT_EQ(existential[1].pos, core::PartOfSpeech::Verb);

  const auto negative_existential = analyzer.analyze("三人いない");
  ASSERT_EQ(negative_existential.size(), 3U);
  EXPECT_EQ(negative_existential[0].surface, "三人");
  EXPECT_EQ(negative_existential[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(negative_existential[1].surface, "い");
  EXPECT_EQ(negative_existential[1].lemma, "いる");
  EXPECT_EQ(negative_existential[1].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(negative_existential[2].surface, "ない");
  EXPECT_EQ(negative_existential[2].pos, core::PartOfSpeech::Auxiliary);

  const auto polite_adjective = analyzer.analyze("よろしゅうございます");
  ASSERT_EQ(polite_adjective.size(), 3U);
  EXPECT_EQ(polite_adjective[0].surface, "よろしゅう");
  EXPECT_EQ(polite_adjective[0].lemma, "よろしい");
  EXPECT_EQ(polite_adjective[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(polite_adjective[1].surface, "ござい");
  EXPECT_EQ(polite_adjective[1].lemma, "ござる");
  EXPECT_EQ(polite_adjective[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(polite_adjective[2].surface, "ます");
  EXPECT_EQ(polite_adjective[2].lemma, "ます");
  EXPECT_EQ(polite_adjective[2].pos, core::PartOfSpeech::Auxiliary);

  const auto honorific_verb = analyzer.analyze("伺います");
  ASSERT_EQ(honorific_verb.size(), 2U);
  EXPECT_EQ(honorific_verb[0].surface, "伺い");
  EXPECT_EQ(honorific_verb[0].lemma, "伺う");
  EXPECT_EQ(honorific_verb[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(honorific_verb[1].surface, "ます");
  EXPECT_EQ(honorific_verb[1].pos, core::PartOfSpeech::Auxiliary);
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

TEST(SuffixCandidatesCounterTest, MarksKatakanaQuantityOrdinalAsSuffix) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  for (const std::string_view text : {"三ページ目を読む", "五セット目"}) {
    const auto results = analyzer.analyze(text);
    ASSERT_FALSE(results.empty()) << text;
    const auto ordinal =
        std::find_if(results.begin(), results.end(), [](const auto& result) { return result.surface == "目"; });
    ASSERT_NE(ordinal, results.end()) << text;
    EXPECT_EQ(ordinal->pos, core::PartOfSpeech::Suffix) << text;
  }
}

TEST(SuffixCandidatesCounterTest, KeepsKatakanaQuantityAndLexicalKanjiTailBoundaries) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  const auto bare_unit = analyzer.analyze("ページ目");
  ASSERT_EQ(bare_unit.size(), 2u);
  EXPECT_EQ(bare_unit[0].surface, "ページ");
  EXPECT_EQ(bare_unit[1].surface, "目");
  EXPECT_EQ(bare_unit[1].pos, core::PartOfSpeech::Noun);

  for (const auto& [text, lexical_tail] :
       {std::pair<std::string_view, std::string_view>{"三ページ目標", "目標"}, {"三ページ目次", "目次"}}) {
    const auto results = analyzer.analyze(text);
    ASSERT_EQ(results.size(), 3u) << text;
    EXPECT_EQ(results[0].surface, "三") << text;
    EXPECT_EQ(results[1].surface, "ページ") << text;
    EXPECT_EQ(results[2].surface, lexical_tail) << text;
    EXPECT_EQ(results[2].pos, core::PartOfSpeech::Noun) << text;
  }

  const auto kanji_quantity = analyzer.analyze("三ページ");
  ASSERT_EQ(kanji_quantity.size(), 2u);
  EXPECT_EQ(kanji_quantity[0].surface, "三");
  EXPECT_EQ(kanji_quantity[1].surface, "ページ");

  const auto digit_quantity = analyzer.analyze("3ページ目");
  ASSERT_EQ(digit_quantity.size(), 1u);
  EXPECT_EQ(digit_quantity[0].surface, "3ページ目");
}

}  // namespace
}  // namespace suzume::analysis
