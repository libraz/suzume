#include "core/types.h"

#include <gtest/gtest.h>

namespace suzume {
namespace core {
namespace {

TEST(TypesTest, PosToStringConvertsAllTypes) {
  EXPECT_EQ(posToString(PartOfSpeech::Noun), "NOUN");
  EXPECT_EQ(posToString(PartOfSpeech::Verb), "VERB");
  EXPECT_EQ(posToString(PartOfSpeech::Adjective), "ADJ");
  EXPECT_EQ(posToString(PartOfSpeech::Particle), "PARTICLE");
  EXPECT_EQ(posToString(PartOfSpeech::Auxiliary), "AUX");
  EXPECT_EQ(posToString(PartOfSpeech::Conjunction), "CONJ");
  EXPECT_EQ(posToString(PartOfSpeech::Adverb), "ADV");
  EXPECT_EQ(posToString(PartOfSpeech::Symbol), "SYMBOL");
  EXPECT_EQ(posToString(PartOfSpeech::Other), "OTHER");
}

TEST(TypesTest, StringToPosStrictConvertsAllTypes) {
  EXPECT_EQ(stringToPosStrict("NOUN"), PartOfSpeech::Noun);
  EXPECT_EQ(stringToPosStrict("名詞"), PartOfSpeech::Noun);
  EXPECT_EQ(stringToPosStrict("VERB"), PartOfSpeech::Verb);
  EXPECT_EQ(stringToPosStrict("動詞"), PartOfSpeech::Verb);
  EXPECT_EQ(stringToPosStrict("ADJ"), PartOfSpeech::Adjective);
  EXPECT_EQ(stringToPosStrict("形容詞"), PartOfSpeech::Adjective);
  EXPECT_EQ(stringToPosStrict("PARTICLE"), PartOfSpeech::Particle);
  EXPECT_EQ(stringToPosStrict("助詞"), PartOfSpeech::Particle);
  EXPECT_EQ(stringToPosStrict("AUX"), PartOfSpeech::Auxiliary);
  EXPECT_EQ(stringToPosStrict("助動詞"), PartOfSpeech::Auxiliary);
  EXPECT_EQ(stringToPosStrict("CONJ"), PartOfSpeech::Conjunction);
  EXPECT_EQ(stringToPosStrict("接続詞"), PartOfSpeech::Conjunction);
}

TEST(TypesTest, StringToPosStrictAcceptsLongAliases) {
  // Long-form English aliases must resolve.
  EXPECT_EQ(stringToPosStrict("ADJECTIVE"), PartOfSpeech::Adjective);
  EXPECT_EQ(stringToPosStrict("ADVERB"), PartOfSpeech::Adverb);
  EXPECT_EQ(stringToPosStrict("AUXILIARY"), PartOfSpeech::Auxiliary);
  EXPECT_EQ(stringToPosStrict("CONJUNCTION"), PartOfSpeech::Conjunction);
  EXPECT_EQ(stringToPosStrict("PRONOUN"), PartOfSpeech::Pronoun);
  EXPECT_EQ(stringToPosStrict("DETERMINER"), PartOfSpeech::Determiner);
  EXPECT_EQ(stringToPosStrict("INTERJECTION"), PartOfSpeech::Interjection);
  EXPECT_EQ(stringToPosStrict("PROPN"), PartOfSpeech::Noun);
}

TEST(TypesTest, StringToPosStrictRejectsUnknown) {
  // Strict variant distinguishes the OTHER aliases from genuinely invalid input.
  EXPECT_EQ(stringToPosStrict("OTHER"), PartOfSpeech::Other);
  EXPECT_EQ(stringToPosStrict("PHRASE"), PartOfSpeech::Other);
  EXPECT_EQ(stringToPosStrict("ADJECTIVE"), PartOfSpeech::Adjective);
  EXPECT_FALSE(stringToPosStrict("NOPE").has_value());
  EXPECT_FALSE(stringToPosStrict("").has_value());
}

TEST(TypesTest, AnalysisModeHasCorrectValues) {
  EXPECT_NE(AnalysisMode::Normal, AnalysisMode::Search);
  EXPECT_NE(AnalysisMode::Search, AnalysisMode::Split);
}

TEST(TypesTest, PosCountMatchesEnumRange) {
  EXPECT_EQ(static_cast<size_t>(PartOfSpeech::Count_), 15u);
  EXPECT_EQ(posToString(PartOfSpeech::Count_), "OTHER");
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Count_), ExtendedPOS::Other);
}

}  // namespace
}  // namespace core
}  // namespace suzume
