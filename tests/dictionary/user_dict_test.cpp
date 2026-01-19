#include "dictionary/user_dict.h"

#include <gtest/gtest.h>

namespace suzume {
namespace dictionary {
namespace {

TEST(UserDictTest, DefaultConstruction) {
  UserDictionary dict;
  EXPECT_EQ(dict.size(), 0);
}

TEST(UserDictTest, AddEntry) {
  UserDictionary dict;

  DictionaryEntry entry;
  entry.surface = "東京";
  entry.pos = core::PartOfSpeech::Noun;
  // v0.8: cost removed

  dict.addEntry(entry);
  EXPECT_EQ(dict.size(), 1);
}

TEST(UserDictTest, GetEntry) {
  UserDictionary dict;

  DictionaryEntry entry;
  entry.surface = "東京";
  entry.pos = core::PartOfSpeech::Noun;
  // v0.8: cost removed

  dict.addEntry(entry);

  const DictionaryEntry* result = dict.getEntry(0);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->surface, "東京");
  EXPECT_EQ(result->pos, core::PartOfSpeech::Noun);
}

TEST(UserDictTest, GetEntryOutOfRange) {
  UserDictionary dict;
  const DictionaryEntry* result = dict.getEntry(999);
  EXPECT_EQ(result, nullptr);
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

TEST(UserDictTest, Clear) {
  UserDictionary dict;

  DictionaryEntry entry;
  entry.surface = "東京";
  entry.pos = core::PartOfSpeech::Noun;
  dict.addEntry(entry);

  EXPECT_EQ(dict.size(), 1);
  dict.clear();
  EXPECT_EQ(dict.size(), 0);
}

TEST(UserDictTest, LoadFromMemoryEmpty) {
  UserDictionary dict;
  auto result = dict.loadFromMemory(nullptr, 0);
  EXPECT_FALSE(result.hasValue());
}

TEST(UserDictTest, LoadFromMemoryCSV) {
  UserDictionary dict;

  const char* csv_data =
      "東京,NOUN,0.5\n"
      "大阪,NOUN,0.5\n";

  auto result = dict.loadFromMemory(csv_data, strlen(csv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 2);
  EXPECT_EQ(dict.size(), 2);
}

TEST(UserDictTest, LoadFromMemoryTSV) {
  UserDictionary dict;

  const char* tsv_data =
      "東京\tNOUN\tトウキョウ\t0.5\n"
      "大阪\tNOUN\tオオサカ\t0.5\n";

  auto result = dict.loadFromMemory(tsv_data, strlen(tsv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 2);
  EXPECT_EQ(dict.size(), 2);
}

TEST(UserDictTest, LoadFromMemoryWithComments) {
  UserDictionary dict;

  const char* csv_data =
      "# This is a comment\n"
      "東京,NOUN,0.5\n"
      "\n"
      "# Another comment\n"
      "大阪,NOUN,0.5\n";

  auto result = dict.loadFromMemory(csv_data, strlen(csv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 2);
}

TEST(UserDictTest, LoadFromMemoryWithWhitespace) {
  UserDictionary dict;

  const char* csv_data =
      "  東京  ,  NOUN  ,  0.5  \n"
      "  大阪  ,  NOUN  ,  0.5  \n";

  auto result = dict.loadFromMemory(csv_data, strlen(csv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 2);
}

TEST(UserDictTest, LoadFromMemoryInvalidLine) {
  UserDictionary dict;

  // Single field lines should be skipped
  const char* csv_data =
      "東京\n"
      "大阪,NOUN,0.5\n";

  auto result = dict.loadFromMemory(csv_data, strlen(csv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 1);
}

TEST(UserDictTest, LoadFromMemoryVerbWithConjType) {
  UserDictionary dict;

  const char* tsv_data =
      "食べる\tVERB\tタベル\t0.5\tICHIDAN\n"
      "書く\tVERB\tカク\t0.5\tGODAN_KA\n";

  auto result = dict.loadFromMemory(tsv_data, strlen(tsv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 2);

  const DictionaryEntry* entry1 = dict.getEntry(0);
  ASSERT_NE(entry1, nullptr);
  // v0.8: conj_type removed from DictionaryEntry

  const DictionaryEntry* entry2 = dict.getEntry(1);
  ASSERT_NE(entry2, nullptr);
  // v0.8: conj_type removed from DictionaryEntry
}

TEST(UserDictTest, LoadFromMemoryAllConjTypes) {
  UserDictionary dict;

  const char* tsv_data =
      "話す\tVERB\t-\t0.5\tGODAN_SA\n"
      "立つ\tVERB\t-\t0.5\tGODAN_TA\n"
      "死ぬ\tVERB\t-\t0.5\tGODAN_NA\n"
      "遊ぶ\tVERB\t-\t0.5\tGODAN_BA\n"
      "読む\tVERB\t-\t0.5\tGODAN_MA\n"
      "走る\tVERB\t-\t0.5\tGODAN_RA\n"
      "買う\tVERB\t-\t0.5\tGODAN_WA\n"
      "泳ぐ\tVERB\t-\t0.5\tGODAN_GA\n"
      "する\tVERB\t-\t0.5\tSURU\n"
      "来る\tVERB\t-\t0.5\tKURU\n"
      "赤い\tADJ\t-\t0.5\tI_ADJ\n"
      "静か\tADJ\t-\t0.5\tNA_ADJ\n"
      "普通\tNOUN\t-\t0.5\tNONE\n";

  auto result = dict.loadFromMemory(tsv_data, strlen(tsv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 13);

  // v0.8: conj_type removed from DictionaryEntry
  // Just verify entries were loaded
  ASSERT_NE(dict.getEntry(0), nullptr);
  ASSERT_NE(dict.getEntry(12), nullptr);
}

TEST(UserDictTest, LoadFromMemoryWithLemma) {
  UserDictionary dict;

  const char* csv_data = "食べた,VERB,0.5,食べる\n";

  auto result = dict.loadFromMemory(csv_data, strlen(csv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 1);

  const DictionaryEntry* entry = dict.getEntry(0);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->lemma, "食べる");
}

// v0.8: LoadFromMemoryInvalidCost test removed (cost field no longer exists)

TEST(UserDictTest, LookupAtDifferentPositions) {
  UserDictionary dict;

  DictionaryEntry entry;
  entry.surface = "本";
  entry.pos = core::PartOfSpeech::Noun;
  dict.addEntry(entry);

  // Lookup at position 0 in "日本"
  auto results = dict.lookup("日本", 0);
  EXPECT_TRUE(results.empty());  // "日" != "本"

  // Note: lookup with start_pos works on character positions in the trie
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
  // v0.8: cost removed
  dict.addEntry(entry2);

  auto results = dict.lookup("東京", 0);
  EXPECT_EQ(results.size(), 2);
}

TEST(UserDictTest, LoadFromFileNotFound) {
  UserDictionary dict;
  auto result = dict.loadFromFile("/nonexistent/path/dict.csv");
  EXPECT_FALSE(result.hasValue());
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
