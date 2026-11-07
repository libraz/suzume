#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "postprocess/postprocessor_resolvers_internal.h"
#include "suzume.h"

namespace suzume {
namespace postprocess {
namespace {

core::Morpheme renyokeiToken(std::string surface, std::string lemma, core::PartOfSpeech pos, core::ExtendedPOS epos,
                             size_t start, size_t end) {
  core::Morpheme result;
  result.surface = surface;
  result.lemma = lemma;
  result.pos = pos;
  result.extended_pos = epos;
  result.start = start;
  result.end = end;
  return result;
}

TEST(RenyokeiSuruNominalization, NominalizesContiguousActionNounWithoutChangingBoundaries) {
  for (const bool lemmatize : {true, false}) {
    SuzumeOptions options;
    options.skip_user_dictionary = true;
    options.lemmatize = lemmatize;
    Suzume analyzer(options);
    for (const auto* input : {"一人歩きする", "一人歩きした", "一人歩きしない"}) {
      SCOPED_TRACE(input);
      SCOPED_TRACE(lemmatize);
      const auto result = analyzer.analyze(input);
      ASSERT_GE(result.size(), 3U);
      EXPECT_EQ(result[0].surface, "一人");
      EXPECT_EQ(result[1].surface, "歩き");
      EXPECT_EQ(result[1].start, 2U);
      EXPECT_EQ(result[1].end, 4U);
      EXPECT_EQ(result[1].pos, core::PartOfSpeech::Noun);
      EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::Noun);
      EXPECT_EQ(result[1].conj_type, dictionary::ConjugationType::None);
      EXPECT_EQ(result[1].conj_form, grammar::ConjForm::Base);
      if (lemmatize) {
        EXPECT_EQ(result[1].lemma, "歩き");
      }
      EXPECT_EQ(result[2].start, 4U);
      EXPECT_EQ(result[2].pos, core::PartOfSpeech::Verb);
    }
  }
}

TEST(RenyokeiSuruNominalization, PreservesHonorificVerbStemsAndExistingNouns) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto* input : {"お読みする", "お読みします", "お話しする", "お見せする"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 3U);
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Prefix);
    EXPECT_EQ(result[1].pos, core::PartOfSpeech::Verb);
  }
  for (const auto* input : {"山歩きする", "立ち読みした"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  }
}

TEST(RenyokeiSuruNominalization, RequiresBareSuruVerbAndContiguity) {
  auto stem = renyokeiToken("読み", "読む", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, 3, 5);
  stem.conj_type = dictionary::ConjugationType::GodanMa;
  stem.conj_form = grammar::ConjForm::Renyokei;
  const auto suru = renyokeiToken("する", "する", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, 5, 7);
  for (const int variant : {0, 1, 2, 3, 4}) {
    SCOPED_TRACE(variant);
    std::vector<core::Morpheme> result{stem, suru};
    if (variant == 0) {
      result[1].start = 6;
      result[1].end = 8;
    } else if (variant == 1) {
      result[1].surface = "し";
      result[1].lemma = "き";
      result[1].pos = core::PartOfSpeech::Auxiliary;
    } else if (variant == 2) {
      result[1].lemma = "過ぎる";
    } else if (variant == 3) {
      result[1].lemma = "進める";
    } else {
      result[1].lemma = "勉強する";
      result[1].conj_type = dictionary::ConjugationType::Suru;
    }
    resolver::resolveNominalizedRenyokeiPredicate(result);
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
    EXPECT_EQ(result[0].lemma, "読む");
    EXPECT_EQ(result[0].conj_type, dictionary::ConjugationType::GodanMa);
  }
  std::vector<core::Morpheme> result{stem, suru};
  resolver::resolveNominalizedRenyokeiPredicate(result);
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(result[0].lemma, "読み");
  EXPECT_EQ(result[0].start, 3U);
  EXPECT_EQ(result[0].end, 5U);
}

}  // namespace
}  // namespace postprocess
}  // namespace suzume
