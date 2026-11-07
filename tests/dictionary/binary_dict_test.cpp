#include "dictionary/binary_dict.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>

#include "dictionary/dictionary.h"
#include "dictionary/user_dict.h"

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

size_t compactRecordOffset(const std::vector<uint8_t>& data) {
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  const size_t entry_offset = sizeof(BinaryDictHeader) + header->surface_size;
  const size_t palette_count = data[entry_offset];
  return entry_offset + 1 + palette_count * 2;
}

size_t compactStringOffset(const std::vector<uint8_t>& data) {
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  const size_t record_offset = compactRecordOffset(data);
  size_t record_size = 0;
  switch (header->flags & BinaryDictHeader::kEntryEncodingMask) {
    case BinaryDictHeader::kRecordPaletteEntries: {
      const size_t record_count = data[record_offset];
      return record_offset + 1 + record_count * sizeof(uint16_t) + header->entry_count;
    }
    case BinaryDictHeader::kGrammarOnlyEntries:
      record_size = 1;
      break;
    case BinaryDictHeader::kPackedEntries:
      record_size = 2;
      break;
    case BinaryDictHeader::kWideEntries:
      record_size = kWideCompactEntrySize;
      break;
  }
  return record_offset + header->entry_count * record_size;
}

uint16_t packedLemmaReference(const std::vector<uint8_t>& data, size_t entry_index) {
  const size_t offset = compactRecordOffset(data) + entry_index * 2;
  const uint16_t packed =
      static_cast<uint16_t>(data[offset]) | static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8U);
  return packed & 0x07FFU;
}

void setPackedLemmaReference(std::vector<uint8_t>& data, size_t entry_index, uint16_t reference) {
  const size_t offset = compactRecordOffset(data) + entry_index * 2;
  uint16_t packed =
      static_cast<uint16_t>(data[offset]) | static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8U);
  packed = static_cast<uint16_t>((packed & 0xF800U) | (reference & 0x07FFU));
  data[offset] = static_cast<uint8_t>(packed & 0xFFU);
  data[offset + 1] = static_cast<uint8_t>(packed >> 8U);
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

TEST_F(BinaryDictTest, LoadFromMemoryOwnsDecodedData) {
  BinaryDictionary dict;
  {
    auto data = buildTestDict("test", core::PartOfSpeech::Noun);
    auto load_result = dict.loadFromMemory(data.data(), data.size());
    ASSERT_TRUE(load_result.hasValue());
  }

  auto results = dict.lookup("test", 0);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].entry->surface, "test");
}

TEST_F(BinaryDictTest, CompactFormatStoresOnlyDifferingLemma) {
  auto same_lemma = buildTestDict("surface", core::PartOfSpeech::Noun);
  const auto* same_header = reinterpret_cast<const BinaryDictHeader*>(same_lemma.data());
  EXPECT_EQ(same_header->version, BinaryDictHeader::kVersion);
  EXPECT_EQ(same_header->flags, BinaryDictHeader::kGrammarOnlyEntries);
  EXPECT_EQ(compactStringOffset(same_lemma), same_lemma.size());
  EXPECT_EQ(compactStringOffset(same_lemma) - (sizeof(BinaryDictHeader) + same_header->surface_size), 1 + 2 + 1);

  BinaryDictWriter writer;
  DictionaryEntry entry;
  entry.surface = "changed";
  entry.lemma = "base";
  entry.pos = core::PartOfSpeech::Verb;
  writer.addEntry(entry);
  auto result = writer.build();
  ASSERT_TRUE(result.hasValue());
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(result.value().data());
  EXPECT_EQ(header->flags, BinaryDictHeader::kPackedEntries);
  EXPECT_EQ(compactStringOffset(result.value()) - (sizeof(BinaryDictHeader) + header->surface_size), 1 + 2 + 2);
  EXPECT_EQ(result.value().size() - compactStringOffset(result.value()), entry.lemma.size() + 1);
}

TEST_F(BinaryDictTest, UsesRecordPaletteForRepeatedPackedEntries) {
  BinaryDictWriter writer;
  for (size_t idx = 0; idx < 8; ++idx) {
    writer.addEntry(
        {"surface" + std::to_string(idx), core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "base"});
  }

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  const auto& data = build_result.value();
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  EXPECT_EQ(header->flags & BinaryDictHeader::kEntryEncodingMask, BinaryDictHeader::kRecordPaletteEntries);

  BinaryDictionary dict;
  ASSERT_TRUE(dict.loadFromMemory(data.data(), data.size()).hasValue());
  ASSERT_NE(dict.lookupExact("surface3"), nullptr);
  EXPECT_EQ(dict.lookupExact("surface3")->lemma, "base");

  auto invalid_index = data;
  const size_t palette_offset = compactRecordOffset(invalid_index);
  const size_t record_count = invalid_index[palette_offset];
  const size_t first_entry_offset = palette_offset + 1 + record_count * sizeof(uint16_t);
  invalid_index[first_entry_offset] = static_cast<uint8_t>(record_count);
  EXPECT_FALSE(dict.loadFromMemory(invalid_index.data(), invalid_index.size()).hasValue());
}

TEST_F(BinaryDictTest, UsesRelativeLemmaReferencesForwardBackwardAndSelf) {
  BinaryDictionary dict;
  {
    BinaryDictWriter writer;
    writer.addEntry({"a-form", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "root"});
    writer.addEntry({"root", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "root"});
    writer.addEntry({"z-form", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "root"});

    auto build_result = writer.build();
    ASSERT_TRUE(build_result.hasValue());
    const auto& data = build_result.value();
    const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
    EXPECT_EQ(header->flags, BinaryDictHeader::kPackedEntries | BinaryDictHeader::kRelativeLemmaRefs);
    EXPECT_EQ(compactStringOffset(data), data.size());
    EXPECT_EQ(packedLemmaReference(data, 0), 2u);  // +1
    EXPECT_EQ(packedLemmaReference(data, 1), 0u);  // self
    EXPECT_EQ(packedLemmaReference(data, 2), 1u);  // -1
    ASSERT_TRUE(dict.loadFromMemory(data.data(), data.size()).hasValue());
  }

  ASSERT_NE(dict.lookupExact("a-form"), nullptr);
  ASSERT_NE(dict.lookupExact("root"), nullptr);
  ASSERT_NE(dict.lookupExact("z-form"), nullptr);
  EXPECT_EQ(dict.lookupExact("a-form")->lemma, "root");
  EXPECT_EQ(dict.lookupExact("root")->lemma, "root");
  EXPECT_EQ(dict.lookupExact("z-form")->lemma, "root");
}

TEST_F(BinaryDictTest, RelativeLemmaReferencesSupportUtf8Surfaces) {
  BinaryDictWriter writer;
  writer.addEntry({"食べ", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "食べる"});
  writer.addEntry({"食べる", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "食べる"});
  writer.addEntry({"食べれ", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbMizenkei, "食べる"});

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  const auto& data = build_result.value();
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  EXPECT_NE(header->flags & BinaryDictHeader::kRelativeLemmaRefs, 0);

  BinaryDictionary dict;
  ASSERT_TRUE(dict.loadFromMemory(data.data(), data.size()).hasValue());
  ASSERT_NE(dict.lookupExact("食べ"), nullptr);
  ASSERT_NE(dict.lookupExact("食べれ"), nullptr);
  EXPECT_EQ(dict.lookupExact("食べ")->lemma, "食べる");
  EXPECT_EQ(dict.lookupExact("食べれ")->lemma, "食べる");
}

TEST_F(BinaryDictTest, FallsBackWhenRelativeLemmaDeltaIsOutOfRange) {
  BinaryDictWriter writer;
  for (size_t idx = 0; idx <= 1024; ++idx) {
    char surface[16];
    std::snprintf(surface, sizeof(surface), "key%04zu", idx);
    writer.addEntry({surface, core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, idx == 0 ? "key1024" : surface});
  }

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  const auto& data = build_result.value();
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  EXPECT_EQ(header->flags & BinaryDictHeader::kRelativeLemmaRefs, 0);
  EXPECT_EQ(header->flags & BinaryDictHeader::kEntryEncodingMask, BinaryDictHeader::kRecordPaletteEntries);
  EXPECT_LT(compactStringOffset(data), data.size());

  BinaryDictionary dict;
  ASSERT_TRUE(dict.loadFromMemory(data.data(), data.size()).hasValue());
  ASSERT_NE(dict.lookupExact("key0000"), nullptr);
  EXPECT_EQ(dict.lookupExact("key0000")->lemma, "key1024");
}

TEST_F(BinaryDictTest, UsesWideCompactEntriesForLargeGrammarPalette) {
  BinaryDictWriter writer;
  size_t entry_idx = 0;
  for (uint8_t pos = 0; pos < static_cast<uint8_t>(core::PartOfSpeech::Count_) && entry_idx < 33; ++pos) {
    for (uint8_t epos = 0; epos < static_cast<uint8_t>(core::ExtendedPOS::Count_) && entry_idx < 33;
         ++epos, ++entry_idx) {
      writer.addEntry({"test" + std::to_string(entry_idx), static_cast<core::PartOfSpeech>(pos),
                       static_cast<core::ExtendedPOS>(epos), "base"});
    }
  }

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(build_result.value().data());
  EXPECT_EQ(header->version, BinaryDictHeader::kVersion);
  EXPECT_EQ(header->flags, BinaryDictHeader::kWideEntries);

  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(build_result.value().data(), build_result.value().size());
  ASSERT_TRUE(load_result.hasValue());
  EXPECT_EQ(load_result.value(), 33u);
  ASSERT_NE(dict.lookupExact("test0"), nullptr);
  EXPECT_EQ(dict.lookupExact("test0")->lemma, "base");
}

TEST_F(BinaryDictTest, RejectsGrammarPaletteOverflow) {
  BinaryDictWriter writer;
  size_t entry_idx = 0;
  for (uint8_t pos = 0; pos < static_cast<uint8_t>(core::PartOfSpeech::Count_) && entry_idx < 256; ++pos) {
    for (uint8_t epos = static_cast<uint8_t>(core::ExtendedPOS::Unknown) + 1;
         epos < static_cast<uint8_t>(core::ExtendedPOS::Count_) && entry_idx < 256; ++epos, ++entry_idx) {
      writer.addEntry({"test" + std::to_string(entry_idx), static_cast<core::PartOfSpeech>(pos),
                       static_cast<core::ExtendedPOS>(epos), ""});
    }
  }
  ASSERT_EQ(entry_idx, 256u);

  auto build_result = writer.build();
  EXPECT_FALSE(build_result.hasValue());
}

TEST_F(BinaryDictTest, CanonicalizesUnspecifiedExtendedPosBeforeStorage) {
  BinaryDictWriter writer;
  writer.addEntry({"test", core::PartOfSpeech::Verb, core::ExtendedPOS::Unknown, ""});

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  BinaryDictionary dict;
  ASSERT_TRUE(dict.loadFromMemory(build_result.value().data(), build_result.value().size()).hasValue());

  const auto* entry = dict.lookupExact("test");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->extended_pos, core::ExtendedPOS::VerbShuushikei);
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

TEST_F(BinaryDictTest, FrontCodedSurfacesRoundTripLongPrefix) {
  const std::string shared_prefix(20, 'a');
  BinaryDictWriter writer;
  writer.addEntry({shared_prefix + "b", core::PartOfSpeech::Noun, core::ExtendedPOS::Unknown, ""});
  writer.addEntry({shared_prefix + "c", core::PartOfSpeech::Verb, core::ExtendedPOS::Unknown, ""});

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(build_result.value().data());
  EXPECT_LT(header->surface_size, 2 * (shared_prefix.size() + 1));

  BinaryDictionary dict;
  ASSERT_TRUE(dict.loadFromMemory(build_result.value().data(), build_result.value().size()).hasValue());
  EXPECT_NE(dict.lookupExact(shared_prefix + "b"), nullptr);
  EXPECT_NE(dict.lookupExact(shared_prefix + "c"), nullptr);
}

TEST_F(BinaryDictTest, FrontCodedJapanesePrefixesKeepCharacterLengths) {
  BinaryDictWriter writer;
  writer.addEntry({"東京", core::PartOfSpeech::Noun, core::ExtendedPOS::Unknown, ""});
  writer.addEntry({"東京都", core::PartOfSpeech::Noun, core::ExtendedPOS::Unknown, ""});

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  BinaryDictionary dict;
  ASSERT_TRUE(dict.loadFromMemory(build_result.value().data(), build_result.value().size()).hasValue());

  const auto results = dict.lookup("東京都内", 0);
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].length, 2u);
  EXPECT_EQ(results[1].length, 3u);
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

TEST_F(BinaryDictTest, WriteFailurePreservesDestinationAndRemovesTemporaryFile) {
  BinaryDictWriter writer;
  DictionaryEntry entry;
  entry.surface = "東京";
  entry.lemma = "東京";
  entry.pos = core::PartOfSpeech::Noun;
  writer.addEntry(entry);

  const auto directory_destination = std::filesystem::temp_directory_path() / "suzume_binary_dict_destination";
  std::filesystem::remove_all(directory_destination);
  ASSERT_TRUE(std::filesystem::create_directory(directory_destination));

  const auto write_result = writer.writeToFile(directory_destination.string());
  EXPECT_FALSE(write_result.hasValue());
  EXPECT_TRUE(std::filesystem::is_directory(directory_destination));
  EXPECT_FALSE(std::filesystem::exists(directory_destination.string() + ".tmp"));
  std::filesystem::remove_all(directory_destination);
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
  BinaryDictWriter writer;
  writer.addEntry({"changed", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "base"});
  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  auto data = std::move(build_result.value());
  const size_t record_offset = compactRecordOffset(data);
  data[record_offset] = 2;  // Only lemma index 1 exists.
  data[record_offset + 1] = 0;

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
}

TEST_F(BinaryDictTest, LoadRejectsOutOfRangeRelativeLemmaReference) {
  BinaryDictWriter writer;
  writer.addEntry({"a-form", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "root"});
  writer.addEntry({"root", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "root"});
  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  auto data = std::move(build_result.value());
  setPackedLemmaReference(data, 0, 1);  // -1 from the first entry.
  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("relative lemma reference"), std::string::npos);
}

TEST_F(BinaryDictTest, LoadRejectsInvalidRelativeLemmaFlagsAndTrailingData) {
  auto invalid_encoding = buildTestDict("test", core::PartOfSpeech::Noun);
  auto* invalid_header = reinterpret_cast<BinaryDictHeader*>(invalid_encoding.data());
  invalid_header->flags |= BinaryDictHeader::kRelativeLemmaRefs;
  BinaryDictionary dict;
  EXPECT_FALSE(dict.loadFromMemory(invalid_encoding.data(), invalid_encoding.size()).hasValue());

  auto unknown_flag = buildTestDict("test", core::PartOfSpeech::Noun);
  reinterpret_cast<BinaryDictHeader*>(unknown_flag.data())->flags |= 0x80U;
  EXPECT_FALSE(dict.loadFromMemory(unknown_flag.data(), unknown_flag.size()).hasValue());

  BinaryDictWriter writer;
  writer.addEntry({"a-form", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "root"});
  writer.addEntry({"root", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "root"});
  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());
  auto trailing_data = std::move(build_result.value());
  trailing_data.push_back(1);
  EXPECT_FALSE(dict.loadFromMemory(trailing_data.data(), trailing_data.size()).hasValue());
}

TEST_F(BinaryDictTest, LoadRejectsInvalidPosValue) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  const size_t entry_offset = sizeof(BinaryDictHeader) + header->surface_size;
  data[entry_offset + 1] = static_cast<uint8_t>(core::PartOfSpeech::Count_);

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Invalid dictionary grammar palette value"), std::string::npos);
}

TEST_F(BinaryDictTest, LoadRejectsInvalidExtendedPosValue) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data.data());
  const size_t entry_offset = sizeof(BinaryDictHeader) + header->surface_size;
  data[entry_offset + 2] = static_cast<uint8_t>(core::ExtendedPOS::Count_);

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Invalid dictionary grammar palette value"), std::string::npos);
}

TEST_F(BinaryDictTest, LoadRejectsZeroEntries) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  auto* header = reinterpret_cast<BinaryDictHeader*>(data.data());
  header->entry_count = 0;

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Invalid dictionary header"), std::string::npos);
}

TEST_F(BinaryDictTest, LoadRejectsInvalidFrontCodedSurfaceTable) {
  BinaryDictWriter writer;
  writer.addEntry({"a", core::PartOfSpeech::Noun, core::ExtendedPOS::Unknown, ""});
  writer.addEntry({"b", core::PartOfSpeech::Noun, core::ExtendedPOS::Unknown, ""});
  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue());

  auto invalid_prefix = build_result.value();
  invalid_prefix[sizeof(BinaryDictHeader)] |= 0x10U;  // First key cannot share a prefix.
  BinaryDictionary dict;
  EXPECT_FALSE(dict.loadFromMemory(invalid_prefix.data(), invalid_prefix.size()).hasValue());

  auto duplicate = build_result.value();
  duplicate[sizeof(BinaryDictHeader) + 3] = 'a';
  EXPECT_FALSE(dict.loadFromMemory(duplicate.data(), duplicate.size()).hasValue());

  auto invalid_length = build_result.value();
  invalid_length[sizeof(BinaryDictHeader)] |= 0x0FU;
  EXPECT_FALSE(dict.loadFromMemory(invalid_length.data(), invalid_length.size()).hasValue());
}

TEST_F(BinaryDictTest, LoadFailurePreservesExistingDictionary) {
  auto good_data = buildTestDict("keep", core::PartOfSpeech::Noun);
  auto bad_data = buildTestDict("bad", core::PartOfSpeech::Noun);
  bad_data[compactRecordOffset(bad_data)] = 1;  // Only grammar palette index 0 exists.

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

TEST_F(BinaryDictTest, LoadRejectsOtherFormatVersion) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  auto* header = reinterpret_cast<BinaryDictHeader*>(data.data());
  header->version = BinaryDictHeader::kVersion - 1;

  BinaryDictionary dict;
  auto result = dict.loadFromMemory(data.data(), data.size());
  EXPECT_FALSE(result.hasValue());
}

TEST_F(BinaryDictTest, LoadRejectsOversizedSurfaceTable) {
  auto data = buildTestDict("test", core::PartOfSpeech::Noun);
  auto* header = reinterpret_cast<BinaryDictHeader*>(data.data());
  header->surface_size = std::numeric_limits<uint32_t>::max();

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

TEST_F(BinaryDictTest, BuildRejectsEmbeddedNullAndExactDuplicateEntries) {
  BinaryDictWriter nul_writer;
  nul_writer.addEntry({std::string("東京\0大阪", 13), core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "東京"});
  auto nul_result = nul_writer.build();
  ASSERT_FALSE(nul_result.hasValue());
  EXPECT_NE(nul_result.error().message.find("embedded NUL"), std::string::npos);

  BinaryDictWriter duplicate_writer;
  duplicate_writer.addEntry({"検査", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "検査"});
  duplicate_writer.addEntry({"検査", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "検査"});
  auto duplicate_result = duplicate_writer.build();
  ASSERT_FALSE(duplicate_result.hasValue());
  EXPECT_NE(duplicate_result.error().message.find("Duplicate dictionary entry"), std::string::npos);
}

TEST_F(BinaryDictTest, GrammaticalHomographsRoundTripAndLookupByPos) {
  BinaryDictWriter writer;
  writer.addEntry({"最悪", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "最悪"});
  writer.addEntry({"最悪", core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjNaAdj, "最悪"});

  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue()) << build_result.error().message;

  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(build_result.value().data(), build_result.value().size());
  ASSERT_TRUE(load_result.hasValue()) << load_result.error().message;
  EXPECT_EQ(dict.size(), 2u);

  const auto results = dict.lookup("最悪な", 0);
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].entry->surface, "最悪");
  EXPECT_EQ(results[1].entry->surface, "最悪");
  ASSERT_NE(dict.lookupExact("最悪", core::PartOfSpeech::Noun), nullptr);
  ASSERT_NE(dict.lookupExact("最悪", core::PartOfSpeech::Adjective), nullptr);
  EXPECT_EQ(dict.lookupExact("最悪", core::PartOfSpeech::Noun)->extended_pos, core::ExtendedPOS::Noun);
  EXPECT_EQ(dict.lookupExact("最悪", core::PartOfSpeech::Adjective)->extended_pos, core::ExtendedPOS::AdjNaAdj);
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

TEST_F(BinaryDictTest, LoadRejectsInvalidUtf8InCompactLemmaPool) {
  BinaryDictWriter writer;
  writer.addEntry({"検査形", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "独自レンマ"});
  auto build_result = writer.build();
  ASSERT_TRUE(build_result.hasValue()) << build_result.error().message;

  auto data = std::move(build_result.value());
  const std::string lemma = "独自レンマ";
  auto lemma_pos = std::search(data.begin(), data.end(), lemma.begin(), lemma.end(),
                               [](uint8_t byte, char chr) { return byte == static_cast<uint8_t>(chr); });
  ASSERT_NE(lemma_pos, data.end());
  *lemma_pos = 0xFF;

  BinaryDictionary dict;
  auto load_result = dict.loadFromMemory(data.data(), data.size());
  ASSERT_FALSE(load_result.hasValue());
  EXPECT_NE(load_result.error().message.find("lemma is not valid UTF-8"), std::string::npos);
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

  bool loaded = manager.loadUserBinaryDictionaryFromMemoryResult(dict_data.data(), dict_data.size()).hasValue();
  EXPECT_TRUE(loaded);

  // Verify lookup works through manager
  auto results = manager.lookup("りんご", 0);
  bool found = false;
  for (const auto& res : results) {
    if (res.entry->surface == "りんご") {
      found = true;
      EXPECT_EQ(res.entry->pos, core::PartOfSpeech::Noun);
      EXPECT_TRUE(res.from_user_dict);
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(BinaryDictTest, DictionaryManagerPreservesBinaryDictionaryProvenance) {
  auto core_data = buildTestDict("coretestentry", core::PartOfSpeech::Noun);
  auto user_data = buildTestDict("usertestentry", core::PartOfSpeech::Noun);

  DictionaryManager manager;
  ASSERT_TRUE(manager.loadCoreDictionaryFromMemoryResult(core_data.data(), core_data.size()).hasValue());
  ASSERT_TRUE(manager.loadUserBinaryDictionaryFromMemoryResult(user_data.data(), user_data.size()).hasValue());

  const auto core_results = manager.lookup("coretestentry", 0);
  const auto core_result = std::find_if(core_results.begin(), core_results.end(), [](const LookupResult& result) {
    return result.entry->surface == "coretestentry";
  });
  ASSERT_NE(core_result, core_results.end());
  EXPECT_FALSE(core_result->from_user_dict);

  const auto user_results = manager.lookup("usertestentry", 0);
  const auto user_result = std::find_if(user_results.begin(), user_results.end(), [](const LookupResult& result) {
    return result.entry->surface == "usertestentry";
  });
  ASSERT_NE(user_result, user_results.end());
  EXPECT_TRUE(user_result->from_user_dict);
}

TEST_F(BinaryDictTest, DictionaryManagerLookupExactChecksSurfaceAndPos) {
  auto dict_data = buildTestDict("りんご", core::PartOfSpeech::Noun);
  DictionaryManager manager;
  ASSERT_TRUE(manager.loadUserBinaryDictionaryFromMemoryResult(dict_data.data(), dict_data.size()).hasValue());

  const auto* exact = manager.lookupExact("りんご", core::PartOfSpeech::Noun);
  ASSERT_NE(exact, nullptr);
  EXPECT_EQ(exact->surface, "りんご");
  EXPECT_EQ(manager.lookupExact("りん"), nullptr);
  EXPECT_EQ(manager.lookupExact("りんご", core::PartOfSpeech::Verb), nullptr);
  EXPECT_EQ(manager.lookupExact(""), nullptr);
}

TEST_F(BinaryDictTest, DictionaryManagerLoadFromMemoryInvalidData) {
  std::vector<uint8_t> bad_data(10, 0);

  DictionaryManager manager;
  EXPECT_FALSE(manager.loadUserBinaryDictionaryFromMemoryResult(bad_data.data(), bad_data.size()).hasValue());
}

TEST_F(BinaryDictTest, DictionaryManagerBinaryDictionariesAreAdditiveAndFailedLoadsPreserveThem) {
  auto first_data = buildTestDict("東京テスト", core::PartOfSpeech::Noun);
  auto second_data = buildTestDict("りんごテスト", core::PartOfSpeech::Noun);
  const std::vector<uint8_t> invalid_data(10, 0);

  DictionaryManager manager;
  ASSERT_TRUE(manager.loadUserBinaryDictionaryFromMemoryResult(first_data.data(), first_data.size()).hasValue());
  ASSERT_TRUE(manager.loadUserBinaryDictionaryFromMemoryResult(second_data.data(), second_data.size()).hasValue());
  EXPECT_FALSE(manager.loadUserBinaryDictionaryFromMemoryResult(invalid_data.data(), invalid_data.size()).hasValue());

  EXPECT_NE(manager.lookupExact("東京テスト"), nullptr);
  EXPECT_NE(manager.lookupExact("りんごテスト"), nullptr);
}

TEST_F(BinaryDictTest, DictionaryManagerClearRemovesEveryUserDictionaryButKeepsCoreDictionary) {
  auto core_data = buildTestDict("東京テスト", core::PartOfSpeech::Noun);
  auto binary_data = buildTestDict("りんごテスト", core::PartOfSpeech::Noun);
  auto source_dictionary = std::make_shared<UserDictionary>();
  source_dictionary->addEntry({"テスト公園", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "テスト公園"});

  DictionaryManager manager;
  ASSERT_TRUE(manager.loadCoreDictionaryFromMemoryResult(core_data.data(), core_data.size()).hasValue());
  ASSERT_TRUE(manager.loadUserBinaryDictionaryFromMemoryResult(binary_data.data(), binary_data.size()).hasValue());
  manager.addUserDictionary(source_dictionary);
  ASSERT_NE(manager.lookupExact("りんごテスト"), nullptr);
  ASSERT_NE(manager.lookupExact("テスト公園"), nullptr);

  manager.clearUserDictionaries();

  EXPECT_EQ(manager.lookupExact("りんごテスト"), nullptr);
  EXPECT_EQ(manager.lookupExact("テスト公園"), nullptr);
  EXPECT_NE(manager.lookupExact("東京テスト"), nullptr);
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
