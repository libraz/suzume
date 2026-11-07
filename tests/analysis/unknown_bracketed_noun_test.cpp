#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>

#include "grammar/char_patterns.h"
#include "suzume.h"

namespace suzume {
namespace analysis {
namespace {

TEST(NoncontinuativeTeNominal, ShapeRejectsRealContinuativesAndMalformedText) {
  for (const auto* surface : {"にがて", "かたて", "あらて", "おもて"}) {
    EXPECT_TRUE(grammar::hasNoncontinuativeTeEnding(surface)) << surface;
  }
  for (const auto* surface :
       {"", "て", "食べて", "つなぎて", "よくて", "こうて", "ようて", "読んで", "すべて", "はじめて", "Ａて", "Aて",
        "ｶﾀﾃ", "カタテ", "かーて", "👨‍👩‍👧て", "か\u3099て", "か\xE3\x81"}) {
    EXPECT_FALSE(grammar::hasNoncontinuativeTeEnding(surface)) << surface;
  }
  // Invalid continuation bytes must not be decoded as valid kana by a fast
  // three-byte-only reader.
  EXPECT_FALSE(grammar::hasNoncontinuativeTeEnding(std::string("\xE3\x41\x8B") + "て"));
}

TEST(NoncontinuativeTeNominal, FiniteCopulaSelectsCompleteKanaNoun) {
  for (const bool lemmatize : {true, false}) {
    SuzumeOptions options;
    options.skip_user_dictionary = true;
    options.lemmatize = lemmatize;
    Suzume analyzer(options);
    for (const auto& [input, surface] :
         {std::pair{"ねこがにがてだ", "にがて"}, std::pair{"ねこがにがてだった", "にがて"},
          std::pair{"これはにがてです", "にがて"}, std::pair{"これはにがてでした", "にがて"},
          std::pair{"泳ぐのがにがてだ", "にがて"}, std::pair{"かたてだ", "かたて"}, std::pair{"あらてだ", "あらて"},
          std::pair{"あらてだった", "あらて"}}) {
      SCOPED_TRACE(input);
      SCOPED_TRACE(lemmatize);
      const auto result = analyzer.analyze(input);
      const std::string expected_surface = surface;
      const auto noun = std::find_if(result.begin(), result.end(),
                                     [&](const core::Morpheme& token) { return token.surface == expected_surface; });
      ASSERT_NE(noun, result.end());
      EXPECT_EQ(noun->pos, core::PartOfSpeech::Noun);
      EXPECT_EQ(noun->extended_pos, core::ExtendedPOS::Noun);
      EXPECT_EQ(noun->lemma, surface);
      EXPECT_EQ(noun->conj_type, dictionary::ConjugationType::None);
      EXPECT_EQ(noun->conj_form, grammar::ConjForm::Base);
      const size_t start = std::string(input).find(surface) / 3;
      EXPECT_EQ(noun->start, start);
      EXPECT_EQ(noun->end, start + 3);
      ASSERT_NE(noun + 1, result.end());
      EXPECT_EQ((noun + 1)->start, noun->end);
      EXPECT_EQ((noun + 1)->pos, core::PartOfSpeech::Auxiliary);
    }
  }
}

TEST(NoncontinuativeTeNominal, KeepsDictionaryProvenHomographicVerb) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  const std::string source = "おもてだつ\tVERB\tGODAN_TA\n";
  ASSERT_TRUE(analyzer.loadUserDictionaryFromMemory(source.data(), source.size()));
  const auto result = analyzer.analyze("おもてだった");
  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].surface, "おもてだっ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "おもてだつ");
  EXPECT_EQ(result[0].conj_type, dictionary::ConjugationType::GodanTa);
}

TEST(NoncontinuativeTeNominal, PreservesLexicalRolesAndProductiveInflections) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto& [input, surface] : {std::pair{"苦手だ", "苦手"}, std::pair{"にがてな人", "にがて"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0].surface, surface);
    EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjNaAdj);
  }
  for (const auto& [input, surface] :
       {std::pair{"すべてだ", "すべて"}, std::pair{"はじめてだ", "はじめて"}, std::pair{"したてだ", "したて"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result[0].surface, surface);
  }
  for (const auto& [input, surface] : {std::pair{"読んだ", "読ん"}, std::pair{"泳いだ", "泳い"},
                                       std::pair{"食べてだ", "食べ"}, std::pair{"つなぎてだ", "つなぎ"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0].surface, surface);
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  }
  for (const auto* input : {"よくてだ", "こうてだ"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_FALSE(result.empty());
    EXPECT_FALSE(result[0].pos == core::PartOfSpeech::Noun && result[0].surface == std::string(input).substr(0, 9));
  }
}

}  // namespace
}  // namespace analysis
}  // namespace suzume
