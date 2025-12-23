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

TEST(TypesTest, StringToPosConvertsAllTypes) {
  EXPECT_EQ(stringToPos("NOUN"), PartOfSpeech::Noun);
  EXPECT_EQ(stringToPos("名詞"), PartOfSpeech::Noun);
  EXPECT_EQ(stringToPos("VERB"), PartOfSpeech::Verb);
  EXPECT_EQ(stringToPos("動詞"), PartOfSpeech::Verb);
  EXPECT_EQ(stringToPos("ADJ"), PartOfSpeech::Adjective);
  EXPECT_EQ(stringToPos("形容詞"), PartOfSpeech::Adjective);
  EXPECT_EQ(stringToPos("PARTICLE"), PartOfSpeech::Particle);
  EXPECT_EQ(stringToPos("助詞"), PartOfSpeech::Particle);
  EXPECT_EQ(stringToPos("AUX"), PartOfSpeech::Auxiliary);
  EXPECT_EQ(stringToPos("助動詞"), PartOfSpeech::Auxiliary);
  EXPECT_EQ(stringToPos("CONJ"), PartOfSpeech::Conjunction);
  EXPECT_EQ(stringToPos("接続詞"), PartOfSpeech::Conjunction);
  EXPECT_EQ(stringToPos("UNKNOWN"), PartOfSpeech::Other);
}

TEST(TypesTest, AnalysisModeHasCorrectValues) {
  EXPECT_NE(AnalysisMode::Normal, AnalysisMode::Search);
  EXPECT_NE(AnalysisMode::Search, AnalysisMode::Split);
}

}  // namespace
}  // namespace core
}  // namespace suzume
