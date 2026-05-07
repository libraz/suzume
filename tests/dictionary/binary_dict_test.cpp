#include "dictionary/binary_dict.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "dictionary/dictionary.h"

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

// Helper to build a simple binary dictionary in memory
std::vector<uint8_t> buildTestDict(const std::string& surface, core::PartOfSpeech pos) {
  BinaryDictWriter writer;
  DictionaryEntry entry;
  entry.surface = surface;
  entry.lemma = surface;
  entry.pos = pos;
  writer.addEntry(entry);
  auto result = writer.build();
  return result.value();
}

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
  // v0.8: cost removed
  // v0.8: is_formal_noun removed (use extended_pos)
  // v0.8: is_low_info removed
  // v0.8: is_prefix removed

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
  // v0.8: cost check removed - cost is now derived from extended_pos
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
    // v0.8: cost removed
    writer.addEntry(entry);
  }

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  // Load from memory
  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(build_result.value().data(), build_result.value().size());
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
    // v0.8: cost removed
    writer.addEntry(entry);
  }

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(build_result.value().data(), build_result.value().size());
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
  // v0.8: cost removed
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

TEST_F(BinaryDictTest, LoadRejectsTruncatedEntryTable) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  auto* header = reinterpret_cast<BinaryDictHeader*>(data.data());
  header->entry_count += 100;

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
}

TEST_F(BinaryDictTest, LoadRejectsOutOfRangeStringReference) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  auto* entry = reinterpret_cast<BinaryDictEntry*>(data.data() + header->entry_offset);
  entry->surface_offset = static_cast<uint32_t>(data.size());

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
}

TEST_F(BinaryDictTest, LoadRejectsInvalidPosValue) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  auto* entry = reinterpret_cast<BinaryDictEntry*>(data.data() + header->entry_offset);
  entry->pos = static_cast<uint8_t>(core::PartOfSpeech::Count_);

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Invalid dictionary POS value"), std::string::npos);
}

TEST_F(BinaryDictTest, LoadRejectsInvalidExtendedPosValue) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  auto* entry = reinterpret_cast<BinaryDictEntry*>(data.data() + header->entry_offset);
  entry->extended_pos = static_cast<uint8_t>(core::ExtendedPOS::Count_);

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Invalid dictionary extended POS value"), std::string::npos);
}

TEST_F(BinaryDictTest, LoadRejectsEmptySurface) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  auto* entry = reinterpret_cast<BinaryDictEntry*>(data.data() + header->entry_offset);
  entry->surface_length = 0;

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("surface must not be empty"), std::string::npos);
}

TEST_F(BinaryDictTest, LoadFailurePreservesExistingDictionary) {
  auto good_data = buildTestDict("keep", core::PartOfSpeech::Noun);
  auto bad_data = buildTestDict("bad", core::PartOfSpeech::Noun);
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(bad_data.data());
  auto* entry = reinterpret_cast<BinaryDictEntry*>(bad_data.data() + header->entry_offset);
  entry->surface_offset = static_cast<uint32_t>(bad_data.size());

  BinaryDictionary dict;
  auto good_result = dict.loadFromMemory(good_data.data(), good_data.size());
  ASSERT_TRUE(good_result.hasValue());

  auto bad_result = dict.loadFromMemory(bad_data.data(), bad_data.size());
  EXPECT_FALSE(bad_result.hasValue());
  EXPECT_TRUE(dict.isLoaded());
  EXPECT_EQ(dict.size(), 1u);

  auto results = dict.lookup("keep", 0);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].entry->surface, "keep");
}

TEST_F(BinaryDictTest, LoadRejectsUnsupportedMinorVersion) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  auto* header = reinterpret_cast<BinaryDictHeader*>(data.data());
  header->version_minor = BinaryDictHeader::kVersionMinor + 1;

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
}

TEST_F(BinaryDictTest, LoadRejectsOverlappingSections) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  auto* header = reinterpret_cast<BinaryDictHeader*>(data.data());
  header->entry_offset = header->trie_offset;

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
}

TEST_F(BinaryDictTest, BuildRejectsTooLongSurfaceOrLemma) {
  BinaryDictWriter writer;

  DictionaryEntry long_surface;
  long_surface.surface = std::string(256, 'a');
  long_surface.lemma = long_surface.surface;
  long_surface.pos = core::PartOfSpeech::Noun;
  writer.addEntry(long_surface);

  auto surface_result = writer.build();
  EXPECT_FALSE(surface_result.hasValue());

  BinaryDictWriter lemma_writer;
  DictionaryEntry long_lemma;
  long_lemma.surface = "short";
  long_lemma.lemma = std::string(256, 'b');
  long_lemma.pos = core::PartOfSpeech::Noun;
  lemma_writer.addEntry(long_lemma);

  auto lemma_result = lemma_writer.build();
  EXPECT_FALSE(lemma_result.hasValue());
}

TEST_F(BinaryDictTest, BuildRejectsEmptySurface) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "";
  entry.lemma = "";
  entry.pos = core::PartOfSpeech::Noun;
  writer.addEntry(entry);

  auto result = writer.build();
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("surface must not be empty"), std::string::npos);
}

TEST_F(BinaryDictTest, BuildRejectsInvalidPosOrExtendedPos) {
  BinaryDictWriter invalid_pos_writer;
  DictionaryEntry invalid_pos;
  invalid_pos.surface = "bad-pos";
  invalid_pos.pos = core::PartOfSpeech::Count_;
  invalid_pos_writer.addEntry(invalid_pos);

  auto pos_result = invalid_pos_writer.build();
  EXPECT_FALSE(pos_result.hasValue());
  EXPECT_NE(pos_result.error().message.find("invalid POS"), std::string::npos);

  BinaryDictWriter invalid_epos_writer;
  DictionaryEntry invalid_epos;
  invalid_epos.surface = "bad-epos";
  invalid_epos.pos = core::PartOfSpeech::Noun;
  invalid_epos.extended_pos = core::ExtendedPOS::Count_;
  invalid_epos_writer.addEntry(invalid_epos);

  auto epos_result = invalid_epos_writer.build();
  EXPECT_FALSE(epos_result.hasValue());
  EXPECT_NE(epos_result.error().message.find("invalid extended POS"), std::string::npos);
}

TEST_F(BinaryDictTest, LemmaHandling) {
  BinaryDictWriter writer;

  // Entry with different lemma
  DictionaryEntry entry1;
  entry1.surface = "running";
  entry1.lemma = "run";
  entry1.pos = core::PartOfSpeech::Verb;
  // v0.8: cost removed
  writer.addEntry(entry1);

  // Entry with same lemma as surface
  DictionaryEntry entry2;
  entry2.surface = "walk";
  entry2.lemma = "walk";
  entry2.pos = core::PartOfSpeech::Verb;
  // v0.8: cost removed
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

TEST_F(BinaryDictTest, ExtendedPosRoundTrip) {
  BinaryDictWriter writer;

  DictionaryEntry verb;
  verb.surface = "食べ";
  verb.lemma = "食べる";
  verb.pos = core::PartOfSpeech::Verb;
  verb.extended_pos = core::ExtendedPOS::VerbRenyokei;
  writer.addEntry(verb);

  DictionaryEntry particle;
  particle.surface = "て";
  particle.lemma = "て";
  particle.pos = core::PartOfSpeech::Particle;
  particle.extended_pos = core::ExtendedPOS::ParticleConj;
  writer.addEntry(particle);

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(build_result.value().data(), build_result.value().size());
  ASSERT_TRUE(load_result.hasValue());

  auto verb_results = dict.lookup("食べて", 0);
  ASSERT_FALSE(verb_results.empty());
  EXPECT_EQ(verb_results[0].entry->extended_pos, core::ExtendedPOS::VerbRenyokei);

  auto particle_results = dict.lookup("て", 0);
  ASSERT_FALSE(particle_results.empty());
  EXPECT_EQ(particle_results[0].entry->extended_pos, core::ExtendedPOS::ParticleConj);
}

TEST_F(BinaryDictTest, FlagsHandling) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "flags";
  entry.lemma = "flags";
  entry.pos = core::PartOfSpeech::Noun;
  entry.extended_pos = core::ExtendedPOS::NounFormal;  // v0.8: use extended_pos
  writer.addEntry(entry);

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  BinaryDictionary dict;
  dict.loadFromMemory(build_result.value().data(), build_result.value().size());

  auto results = dict.lookup("flags", 0);
  ASSERT_EQ(results.size(), 1u);
  // v0.8: is_formal_noun/is_low_info/is_prefix replaced by extended_pos
  EXPECT_EQ(results[0].entry->extended_pos, core::ExtendedPOS::NounFormal);
}

TEST_F(BinaryDictTest, ConjugationType) {
  BinaryDictWriter writer;

  DictionaryEntry entry;
  entry.surface = "verb";
  entry.lemma = "verb";
  entry.pos = core::PartOfSpeech::Verb;
  // v0.8: cost and conj_type removed
  writer.addEntry(entry);

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
  // v0.8: cost removed
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
  // v0.8: cost removed
  writer.addEntry(entry);

  auto build_result = writer.build();
  BinaryDictionary dict;
  dict.loadFromMemory(build_result.value().data(), build_result.value().size());

  // Start position beyond text length
  auto results = dict.lookup("test", 100);
  EXPECT_TRUE(results.empty());
}

TEST_F(BinaryDictTest, DictionaryManagerLoadUserBinaryDictionaryFromMemory) {
  auto dict_data = buildTestDict("りんご", core::PartOfSpeech::Noun);

  DictionaryManager manager;
  EXPECT_FALSE(manager.hasUserBinaryDictionary());

  bool loaded = manager.loadUserBinaryDictionaryFromMemory(dict_data.data(), dict_data.size());
  EXPECT_TRUE(loaded);
  EXPECT_TRUE(manager.hasUserBinaryDictionary());

  // Verify lookup works through manager
  auto results = manager.lookup("りんご", 0);
  bool found = false;
  for (const auto& res : results) {
    if (res.entry->surface == "りんご") {
      found = true;
      EXPECT_EQ(res.entry->pos, core::PartOfSpeech::Noun);
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(BinaryDictTest, DictionaryManagerLoadFromMemoryInvalidData) {
  std::vector<uint8_t> bad_data(10, 0);

  DictionaryManager manager;
  EXPECT_FALSE(manager.loadUserBinaryDictionaryFromMemory(bad_data.data(), bad_data.size()));
  EXPECT_FALSE(manager.hasUserBinaryDictionary());
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
