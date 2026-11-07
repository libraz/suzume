#include "dictionary/user_dict.h"

#include <gtest/gtest.h>

namespace suzume {
namespace dictionary {
namespace {

TEST(UserDictTest, DefaultConstructionIsEmpty) {
  UserDictionary dict;
  EXPECT_TRUE(dict.lookup("東京", 0).empty());
  EXPECT_EQ(dict.lookupExact("東京"), nullptr);
}

TEST(UserDictTest, AddEntry) {
  UserDictionary dict;

  DictionaryEntry entry;
  entry.surface = "東京";
  entry.pos = core::PartOfSpeech::Noun;

  EXPECT_TRUE(dict.addEntry(entry));
  const auto results = dict.lookup("東京", 0);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_NE(results[0].entry, nullptr);
  EXPECT_EQ(results[0].entry->surface, "東京");
  EXPECT_EQ(results[0].entry->pos, core::PartOfSpeech::Noun);
}

TEST(UserDictTest, AddEntryRejectsInvalidEntry) {
  UserDictionary dict;

  DictionaryEntry empty_surface;
  empty_surface.pos = core::PartOfSpeech::Noun;
  EXPECT_FALSE(dict.addEntry(empty_surface));

  DictionaryEntry invalid_pos;
  invalid_pos.surface = "壊れた";
  invalid_pos.pos = static_cast<core::PartOfSpeech>(255);
  EXPECT_FALSE(dict.addEntry(invalid_pos));

  DictionaryEntry invalid_extended_pos;
  invalid_extended_pos.surface = "壊れた";
  invalid_extended_pos.pos = core::PartOfSpeech::Noun;
  invalid_extended_pos.extended_pos = static_cast<core::ExtendedPOS>(255);
  EXPECT_FALSE(dict.addEntry(invalid_extended_pos));

  DictionaryEntry invalid_lemma{"検査", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, std::string("\xE3\x81", 2)};
  EXPECT_FALSE(dict.addEntry(invalid_lemma));

  EXPECT_TRUE(dict.lookup("壊れた", 0).empty());
  EXPECT_TRUE(dict.lookup("検査", 0).empty());
}

TEST(UserDictTest, RejectsAnOverlongSurfaceBeforeBuildingTheTrie) {
  UserDictionary dict;
  DictionaryEntry entry{std::string(256, 'a'), core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, ""};

  EXPECT_FALSE(dict.addEntry(entry));
  EXPECT_TRUE(dict.lookup(entry.surface, 0).empty());
}

TEST(UserDictTest, Lookup) {
  UserDictionary dict;

  DictionaryEntry entry1;
  entry1.surface = "東京";
  entry1.pos = core::PartOfSpeech::Noun;
  dict.addEntry(entry1);

  DictionaryEntry entry2;
  entry2.surface = "東京都";
  entry2.pos = core::PartOfSpeech::Noun;
  dict.addEntry(entry2);

  auto results = dict.lookup("東京都庁", 0);
  EXPECT_EQ(results.size(), 2);
}

TEST(UserDictTest, LookupNotFound) {
  UserDictionary dict;

  DictionaryEntry entry;
  entry.surface = "東京";
  entry.pos = core::PartOfSpeech::Noun;
  dict.addEntry(entry);

  auto results = dict.lookup("大阪", 0);
  EXPECT_TRUE(results.empty());
}

TEST(UserDictTest, LookupExactPreservesRegistrationOrderAndFiltersPos) {
  UserDictionary dict;
  dict.addEntry({"テスト", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, ""});
  dict.addEntry({"テスト", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, ""});

  const auto* first = dict.lookupExact("テスト");
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->pos, core::PartOfSpeech::Noun);

  const auto* verb = dict.lookupExact("テスト", core::PartOfSpeech::Verb);
  ASSERT_NE(verb, nullptr);
  EXPECT_EQ(verb->extended_pos, core::ExtendedPOS::VerbShuushikei);

  EXPECT_EQ(dict.lookupExact("テス"), nullptr);
  EXPECT_EQ(dict.lookupExact("テスト", core::PartOfSpeech::Adjective), nullptr);
  EXPECT_EQ(dict.lookupExact(""), nullptr);
}

TEST(UserDictTest, LookupAtDifferentPositions) {
  UserDictionary dict;

  DictionaryEntry entry;
  entry.surface = "本";
  entry.pos = core::PartOfSpeech::Noun;
  dict.addEntry(entry);

  // Lookup at position 0 in "日本"
  auto results = dict.lookup("日本", 0);
  EXPECT_TRUE(results.empty());  // "日" != "本"
}

TEST(UserDictTest, MultipleEntriesSameSurface) {
  UserDictionary dict;

  DictionaryEntry entry1;
  entry1.surface = "東京";
  entry1.pos = core::PartOfSpeech::Noun;
  dict.addEntry(entry1);

  DictionaryEntry entry2;
  entry2.surface = "東京";
  entry2.pos = core::PartOfSpeech::Noun;
  dict.addEntry(entry2);

  auto results = dict.lookup("東京", 0);
  EXPECT_EQ(results.size(), 2);
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
