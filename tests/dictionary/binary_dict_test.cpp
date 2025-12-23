#include "dictionary/binary_dict.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace suzume {
namespace dictionary {
namespace {

class BinaryDictTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create temp file path
    temp_file_ = std::filesystem::temp_directory_path() / "test_dict.bin";
  }

  void TearDown() override {
    // Clean up temp file
    std::filesystem::remove(temp_file_);
  }

  std::filesystem::path temp_file_;
};

TEST_F(BinaryDictTest, WriteAndLoadEmpty) {
  BinaryDictWriter writer;

  // Writing empty dictionary should fail
  auto result = writer.build();
  EXPECT_FALSE(result.hasValue());
}

TEST_F(BinaryDictTest, WriteAndLoadSingleEntry) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "test";
  entry.lemma = "test";
  entry.pos = core::PartOfSpeech::Noun;
  entry.cost = 1.5f;
  entry.is_formal_noun = false;
  entry.is_low_info = false;
  entry.is_prefix = false;

  writer.addEntry(entry);

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  const auto& data = build_result.value();
  EXPECT_GT(data.size(), sizeof(BinaryDictHeader));

  // Load from memory
  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(data.data(), data.size());
  ASSERT_TRUE(load_result.hasValue());
  EXPECT_EQ(load_result.value(), 1u);

  EXPECT_TRUE(dict.isLoaded());
  EXPECT_EQ(dict.size(), 1u);

  // Lookup the entry
  auto results = dict.lookup("test", 0);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].length, 4u);
  EXPECT_EQ(results[0].entry->surface, "test");
  EXPECT_EQ(results[0].entry->pos, core::PartOfSpeech::Noun);
  EXPECT_NEAR(results[0].entry->cost, 1.5f, 0.01f);
}

TEST_F(BinaryDictTest, WriteAndLoadMultipleEntries) {
  BinaryDictWriter writer;

  // Add test entries
  std::vector<std::pair<std::string, core::PartOfSpeech>> entries = {
      {"a", core::PartOfSpeech::Symbol},
      {"abc", core::PartOfSpeech::Other},
      {"abcd", core::PartOfSpeech::Other},
  };

  for (const auto& pair : entries) {
    DictionaryEntry entry;
    entry.surface = pair.first;
    entry.lemma = pair.first;
    entry.pos = pair.second;
    entry.cost = 1.0f;
    writer.addEntry(entry);
  }

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  // Load from memory
  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(build_result.value().data(),
                                          build_result.value().size());
  ASSERT_TRUE(load_result.hasValue());
  EXPECT_EQ(load_result.value(), 3u);

  // Lookup with prefix search
  auto results = dict.lookup("abcdef", 0);
  EXPECT_GE(results.size(), 1u);

  // Should find "a", "abc", "abcd" as prefixes
  std::vector<std::string> found;
  for (const auto& res : results) {
    found.push_back(res.entry->surface);
  }
  std::sort(found.begin(), found.end());

  EXPECT_TRUE(std::find(found.begin(), found.end(), "a") != found.end());
  EXPECT_TRUE(std::find(found.begin(), found.end(), "abc") != found.end());
  EXPECT_TRUE(std::find(found.begin(), found.end(), "abcd") != found.end());
}

TEST_F(BinaryDictTest, WriteAndLoadJapanese) {
  BinaryDictWriter writer;

  std::vector<std::pair<std::string, core::PartOfSpeech>> entries = {
      {"ab", core::PartOfSpeech::Noun},
      {"abc", core::PartOfSpeech::Verb},
      {"b", core::PartOfSpeech::Adjective},
  };

  for (const auto& pair : entries) {
    DictionaryEntry entry;
    entry.surface = pair.first;
    entry.lemma = pair.first;
    entry.pos = pair.second;
    entry.cost = 1.0f;
    writer.addEntry(entry);
  }

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(build_result.value().data(),
                                          build_result.value().size());
  ASSERT_TRUE(load_result.hasValue());

  // Lookup from different position
  auto results = dict.lookup("abc", 0);
  EXPECT_GE(results.size(), 2u);  // "ab" and "abc"
}

TEST_F(BinaryDictTest, WriteToFileAndLoad) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "file";
  entry.lemma = "file";
  entry.pos = core::PartOfSpeech::Noun;
  entry.cost = 2.0f;
  writer.addEntry(entry);

  // Write to file
  auto write_result = writer.writeToFile(temp_file_.string());
  ASSERT_TRUE(write_result.hasValue());
  EXPECT_GT(write_result.value(), 0u);

  // Verify file exists
  EXPECT_TRUE(std::filesystem::exists(temp_file_));

  // Load from file
  BinaryDictionary dict;
  auto load_result = dict.loadFromFile(temp_file_.string());
  ASSERT_TRUE(load_result.hasValue());
  EXPECT_EQ(load_result.value(), 1u);

  auto results = dict.lookup("file", 0);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].entry->surface, "file");
}

TEST_F(BinaryDictTest, LoadInvalidFile) {
  BinaryDictionary dict;
  auto result = dict.loadFromFile("/nonexistent/path/dict.bin");
  EXPECT_FALSE(result.hasValue());
}

TEST_F(BinaryDictTest, LoadInvalidData) {
  BinaryDictionary dict;

  // Too small
  std::vector<uint8_t> small_data(10, 0);
  auto result1 = dict.loadFromMemory(small_data.data(), small_data.size());
  EXPECT_FALSE(result1.hasValue());

  // Wrong magic
  std::vector<uint8_t> bad_magic(sizeof(BinaryDictHeader), 0);
  bad_magic[0] = 'X';
  auto result2 = dict.loadFromMemory(bad_magic.data(), bad_magic.size());
  EXPECT_FALSE(result2.hasValue());
}

TEST_F(BinaryDictTest, LemmaHandling) {
  BinaryDictWriter writer;

  // Entry with different lemma
  DictionaryEntry entry1;
  entry1.surface = "running";
  entry1.lemma = "run";
  entry1.pos = core::PartOfSpeech::Verb;
  entry1.cost = 1.0f;
  writer.addEntry(entry1);

  // Entry with same lemma as surface
  DictionaryEntry entry2;
  entry2.surface = "walk";
  entry2.lemma = "walk";
  entry2.pos = core::PartOfSpeech::Verb;
  entry2.cost = 1.0f;
  writer.addEntry(entry2);

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  BinaryDictionary dict;
  dict.loadFromMemory(build_result.value().data(), build_result.value().size());

  // Check lemma handling
  auto results1 = dict.lookup("running", 0);
  ASSERT_EQ(results1.size(), 1u);
  EXPECT_EQ(results1[0].entry->lemma, "run");

  auto results2 = dict.lookup("walk", 0);
  ASSERT_EQ(results2.size(), 1u);
  EXPECT_EQ(results2[0].entry->lemma, "walk");
}

TEST_F(BinaryDictTest, FlagsHandling) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "flags";
  entry.lemma = "flags";
  entry.pos = core::PartOfSpeech::Noun;
  entry.cost = 1.0f;
  entry.is_formal_noun = true;
  entry.is_low_info = true;
  entry.is_prefix = true;
  writer.addEntry(entry);

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  BinaryDictionary dict;
  dict.loadFromMemory(build_result.value().data(), build_result.value().size());

  auto results = dict.lookup("flags", 0);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_TRUE(results[0].entry->is_formal_noun);
  EXPECT_TRUE(results[0].entry->is_low_info);
  EXPECT_TRUE(results[0].entry->is_prefix);
}

TEST_F(BinaryDictTest, ConjugationType) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "verb";
  entry.lemma = "verb";
  entry.pos = core::PartOfSpeech::Verb;
  entry.cost = 1.0f;
  writer.addEntry(entry, ConjugationType::Ichidan);

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  // Just verify it builds correctly - conjugation type is stored but not
  // exposed in DictionaryEntry (used for conjugation expansion)
  EXPECT_GT(build_result.value().size(), 0u);
}

TEST_F(BinaryDictTest, GetEntry) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "getentry";
  entry.lemma = "getentry";
  entry.pos = core::PartOfSpeech::Noun;
  entry.cost = 1.0f;
  writer.addEntry(entry);

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  BinaryDictionary dict;
  dict.loadFromMemory(build_result.value().data(), build_result.value().size());

  // Get by index
  const auto* ent = dict.getEntry(0);
  ASSERT_NE(ent, nullptr);
  EXPECT_EQ(ent->surface, "getentry");

  // Invalid index
  const auto* invalid = dict.getEntry(100);
  EXPECT_EQ(invalid, nullptr);
}

TEST_F(BinaryDictTest, LookupNotLoaded) {
  BinaryDictionary dict;
  EXPECT_FALSE(dict.isLoaded());

  auto results = dict.lookup("test", 0);
  EXPECT_TRUE(results.empty());
}

TEST_F(BinaryDictTest, LookupOutOfBounds) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "test";
  entry.lemma = "test";
  entry.pos = core::PartOfSpeech::Noun;
  entry.cost = 1.0f;
  writer.addEntry(entry);

  auto build_result = writer.build();
  BinaryDictionary dict;
  dict.loadFromMemory(build_result.value().data(), build_result.value().size());

  // Start position beyond text length
  auto results = dict.lookup("test", 100);
  EXPECT_TRUE(results.empty());
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
