#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "analysis/unknown.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "normalize/utf8.h"
#include "suzume.h"

namespace suzume::analysis {
namespace {

Suzume makeHiraganaGodanAnalyzer() {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  return Suzume(options);
}

TEST(HiraganaOnbinContraction, RequiresAContractedAspectOrCopulaAuxiliary) {
  dictionary::DictionaryManager dictionary_manager;
  ASSERT_TRUE(dictionary_manager.loadCoreDictionaryResult("data/core.dic").hasValue());
  const grammar::Inflection inflection;
  const auto generate = [&](std::string_view text) {
    const auto codepoints = normalize::toCodepoints(text);
    std::vector<UnknownCandidate> candidates;
    hiragana_verb_detail::appendOnbinContractionCandidates(codepoints, 0, codepoints.size(), inflection,
                                                           &dictionary_manager, candidates);
    return candidates;
  };
  for (const auto& [text, stem] : {std::pair<std::string_view, std::string_view>{"よんじゃう", "よん"},
                                   {"よんじゃだめ", "よん"},
                                   {"よんどく", "よん"},
                                   {"かっとく", "かっ"},
                                   {"かっちゃう", "かっ"},
                                   {"よんだ", "よん"},
                                   {"よんで", "よん"}}) {
    SCOPED_TRACE(text);
    const auto candidates = generate(text);
    const std::string_view expected_stem = stem;
    EXPECT_TRUE(std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& candidate) {
      return candidate.surface == expected_stem && candidate.origin == core::CandidateOrigin::VerbHiragana &&
             candidate.extended_pos == core::ExtendedPOS::VerbOnbinkei;
    }));
  }
  for (const std::string_view text : {"よんじ", "よんじゅうばんだ"}) {
    SCOPED_TRACE(text);
    const auto candidates = generate(text);
    EXPECT_TRUE(std::none_of(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
      return candidate.surface == "よん" && candidate.origin == core::CandidateOrigin::VerbHiragana;
    }));
  }
}

TEST(HiraganaGodanTerminal, KeepsCompleteWaRowPredicateBeforeNoun) {
  auto analyzer = makeHiraganaGodanAnalyzer();
  const auto result = analyzer.analyze("そこなう可能性がある");

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[0].surface, "そこなう");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "そこなう");
  EXPECT_EQ(result[1].surface, "可能性");
}

TEST(HiraganaGodanTerminal, GeneralizesAcrossUnknownWaRowStems) {
  auto analyzer = makeHiraganaGodanAnalyzer();

  const auto first = analyzer.analyze("たたかう可能性がある");
  ASSERT_FALSE(first.empty());
  EXPECT_EQ(first[0].surface, "たたかう");
  EXPECT_EQ(first[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(first[0].lemma, "たたかう");

  const auto second = analyzer.analyze("計画をまかなう方法");
  ASSERT_EQ(second.size(), 4U);
  EXPECT_EQ(second[2].surface, "まかなう");
  EXPECT_EQ(second[2].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(second[2].lemma, "まかなう");
}

TEST(HiraganaGodanTerminal, KeepsDependentSameSurfaceAsAuxiliary) {
  auto analyzer = makeHiraganaGodanAnalyzer();
  const auto result = analyzer.analyze("読みそこなう");

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].surface, "読み");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "読む");
  EXPECT_EQ(result[1].surface, "そこなう");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[1].lemma, "そこなう");
}

TEST(HiraganaGodanTerminal, PreservesDemonstrativePronounBoundaries) {
  auto analyzer = makeHiraganaGodanAnalyzer();
  const auto result = analyzer.analyze("そこにある");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "そこ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Pronoun);
  EXPECT_EQ(result[1].surface, "に");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(result[2].surface, "ある");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Verb);
}

TEST(GodanTaRenyokei, UsesGodanLemmaBeforeDeclaredRenyokeiAuxiliaries) {
  auto analyzer = makeHiraganaGodanAnalyzer();

  struct Case {
    const char* input;
    const char* verb_surface;
    const char* lemma;
    const char* auxiliary_surface;
  };
  constexpr Case kCases[] = {
      {"もちたい", "もち", "もつ", "たい"}, {"まちます", "まち", "まつ", "ます"},
      {"たちたい", "たち", "たつ", "たい"}, {"うちます", "うち", "うつ", "ます"},
      {"かちたい", "かち", "かつ", "たい"}, {"そだちます", "そだち", "そだつ", "ます"},
      {"保ちたい", "保ち", "保つ", "たい"},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.input);
    const auto result = analyzer.analyze(test_case.input);
    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[0].surface, test_case.verb_surface);
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
    EXPECT_EQ(result[0].lemma, test_case.lemma);
    EXPECT_EQ(result[1].surface, test_case.auxiliary_surface);
    EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  }
}

TEST(GodanTaRenyokei, DoesNotReclassifyClosedAuxiliaryBeforeRenyokeiAuxiliary) {
  auto analyzer = makeHiraganaGodanAnalyzer();
  const auto result = analyzer.analyze("食べられちゃう");

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "食べ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[1].surface, "られ");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[2].surface, "ちゃう");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Auxiliary);
}

}  // namespace
}  // namespace suzume::analysis
