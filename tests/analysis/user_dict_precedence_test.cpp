/**
 * @file user_dict_precedence_test.cpp
 * @brief A user dictionary entry outranks unknown-word heuristics on its span.
 *
 * Unknown-word generators may hand out lexical bonuses stronger than any
 * dictionary edge — the exact-reduplication mimetic adverb (ワクワク, キラキラ)
 * is the strongest of them. Those bonuses must not survive inside a span the
 * user registered, otherwise a registration covering a reduplicated prefix can
 * never be selected and the cost column offers no way out (it is ignored by
 * design).
 *
 * A failure here means a heuristic candidate regained precedence over an
 * explicit registration; fix the candidate cost, not the expectation.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "suzume.h"

namespace suzume {
namespace {

SuzumeOptions userDictOptions() {
  SuzumeOptions opts;
  opts.skip_user_dictionary = true;  // Ignore data/user.dic; load entries explicitly
  return opts;
}

std::vector<std::string> surfacesOf(Suzume& instance, const std::string& text) {
  std::vector<std::string> surfaces;
  for (const auto& token : instance.analyze(text)) {
    surfaces.emplace_back(token.surface);
  }
  return surfaces;
}

bool loadEntries(Suzume& instance, const char* csv) {
  return instance.loadUserDictionaryFromMemoryResult(csv, std::strlen(csv)).hasValue();
}

TEST(UserDictPrecedenceTest, KatakanaReduplicationPrefixEntryWins) {
  Suzume instance(userDictOptions());
  ASSERT_TRUE(loadEntries(instance, "ワクワク商会,NOUN,0.5,ワクワク商会\n"));

  EXPECT_EQ(surfacesOf(instance, "ワクワク商会"), (std::vector<std::string>{"ワクワク商会"}));
}

TEST(UserDictPrecedenceTest, HiraganaReduplicationPrefixEntryWins) {
  Suzume instance(userDictOptions());
  ASSERT_TRUE(loadEntries(instance, "わくわく商会,NOUN,0.5,わくわく商会\n"));

  EXPECT_EQ(surfacesOf(instance, "わくわく商会"), (std::vector<std::string>{"わくわく商会"}));
}

TEST(UserDictPrecedenceTest, EntryCoveringExactlyTheMimeticSpanWins) {
  Suzume instance(userDictOptions());
  ASSERT_TRUE(loadEntries(instance, "キラキラ,NOUN,0.5,キラキラ\n"));

  const auto tokens = instance.analyze("キラキラ");
  ASSERT_EQ(tokens.size(), 1U);
  EXPECT_EQ(tokens[0].surface, "キラキラ");
  EXPECT_EQ(tokens[0].pos, core::PartOfSpeech::Noun);
}

TEST(UserDictPrecedenceTest, EntryWinsAtNonZeroOffset) {
  Suzume instance(userDictOptions());
  ASSERT_TRUE(loadEntries(instance, "ワクワク商会,NOUN,0.5,ワクワク商会\n"));

  const auto surfaces = surfacesOf(instance, "昨日ワクワク商会に行った");
  ASSERT_FALSE(surfaces.empty());
  EXPECT_NE(std::find(surfaces.begin(), surfaces.end(), "ワクワク商会"), surfaces.end());
}

TEST(UserDictPrecedenceTest, UnregisteredMimeticStillSplitsOffAsAdverb) {
  // The clamp is scoped to registered spans: without a covering entry the
  // mimetic adverb keeps its bonus and the reduplication stays a separate unit.
  Suzume instance(userDictOptions());

  EXPECT_EQ(surfacesOf(instance, "ワクワク商会"), (std::vector<std::string>{"ワクワク", "商会"}));
}

TEST(UserDictPrecedenceTest, EntryDoesNotAbsorbTextOutsideItsSpan) {
  Suzume instance(userDictOptions());
  ASSERT_TRUE(loadEntries(instance, "ワクワク商会,NOUN,0.5,ワクワク商会\n"));

  const auto surfaces = surfacesOf(instance, "ワクワク商店");
  EXPECT_EQ(surfaces, (std::vector<std::string>{"ワクワク", "商店"}));
}

}  // namespace
}  // namespace suzume
