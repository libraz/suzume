#include "dictionary/binary_dict.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace suzume::dictionary {

namespace {

// Flag bits
constexpr uint8_t kFlagFormalNoun = 0x01;
constexpr uint8_t kFlagLowInfo = 0x02;
constexpr uint8_t kFlagPrefix = 0x04;

int16_t floatToCost(float cost) {
  return static_cast<int16_t>(cost * 100.0F);
}

float costToFloat(int16_t cost) {
  return static_cast<float>(cost) / 100.0F;
}

uint8_t posToUint8(core::PartOfSpeech pos) {
  return static_cast<uint8_t>(pos);
}

core::PartOfSpeech uint8ToPos(uint8_t val) {
  return static_cast<core::PartOfSpeech>(val);
}

}  // namespace

// BinaryDictionary implementation

BinaryDictionary::BinaryDictionary() = default;
BinaryDictionary::~BinaryDictionary() = default;

core::Expected<size_t, core::Error> BinaryDictionary::loadFromFile(
    const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::FileNotFound,
                    "Failed to open dictionary file: " + path));
  }

  size_t file_size = static_cast<size_t>(file.tellg());
  file.seekg(0);

  data_.resize(file_size);
  if (!file.read(reinterpret_cast<char*>(data_.data()), file_size)) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError,
                    "Failed to read dictionary file: " + path));
  }

  return parseData();
}

core::Expected<size_t, core::Error> BinaryDictionary::loadFromMemory(
    const uint8_t* data, size_t size) {
  data_.assign(data, data + size);
  return parseData();
}

core::Expected<size_t, core::Error> BinaryDictionary::parseData() {
  if (data_.size() < sizeof(BinaryDictHeader)) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput,
                    "Dictionary file too small"));
  }

  const auto* header = reinterpret_cast<const BinaryDictHeader*>(data_.data());

  // Validate magic
  if (header->magic != BinaryDictHeader::kMagic) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput,
                    "Invalid dictionary magic number"));
  }

  // Validate version
  if (header->version_major != BinaryDictHeader::kVersionMajor) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput,
                    "Unsupported dictionary version"));
  }

  // Validate offsets
  if (header->trie_offset + header->trie_size > data_.size() ||
      header->entry_offset > data_.size() ||
      header->string_offset > data_.size()) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput,
                    "Invalid dictionary offsets"));
  }

  // Load trie
  if (!trie_.deserialize(data_.data() + header->trie_offset,
                         header->trie_size)) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput,
                    "Failed to load dictionary trie"));
  }

  // Load entries
  const auto* entry_data = reinterpret_cast<const BinaryDictEntry*>(
      data_.data() + header->entry_offset);
  const char* string_pool = reinterpret_cast<const char*>(
      data_.data() + header->string_offset);

  entries_.clear();
  entries_.reserve(header->entry_count);

  for (uint32_t idx = 0; idx < header->entry_count; ++idx) {
    const auto& rec = entry_data[idx];

    DictionaryEntry entry;
    entry.surface = std::string(string_pool + rec.surface_offset,
                                rec.surface_length);
    entry.pos = uint8ToPos(rec.pos);
    entry.cost = costToFloat(rec.cost);

    if (rec.lemma_length > 0) {
      entry.lemma = std::string(string_pool + rec.lemma_offset,
                                rec.lemma_length);
    } else {
      entry.lemma = entry.surface;
    }

    entry.is_formal_noun = (rec.flags & kFlagFormalNoun) != 0;
    entry.is_low_info = (rec.flags & kFlagLowInfo) != 0;
    entry.is_prefix = (rec.flags & kFlagPrefix) != 0;
    entry.conj_type = static_cast<ConjugationType>(rec.conj_type);

    entries_.push_back(std::move(entry));
  }

  return entries_.size();
}

std::vector<LookupResult> BinaryDictionary::lookup(std::string_view text,
                                                    size_t start_pos) const {
  std::vector<LookupResult> results;

  if (!isLoaded() || start_pos >= text.size()) {
    return results;
  }

  auto trie_results = trie_.commonPrefixSearch(text, start_pos);

  for (const auto& tres : trie_results) {
    if (tres.value >= 0 && static_cast<size_t>(tres.value) < entries_.size()) {
      LookupResult result{};
      result.entry_id = static_cast<uint32_t>(tres.value);
      result.length = tres.length;
      result.entry = &entries_[tres.value];
      results.push_back(result);
    }
  }

  return results;
}

const DictionaryEntry* BinaryDictionary::getEntry(uint32_t idx) const {
  if (idx < entries_.size()) {
    return &entries_[idx];
  }
  return nullptr;
}

// BinaryDictWriter implementation

BinaryDictWriter::BinaryDictWriter() = default;

void BinaryDictWriter::addEntry(const DictionaryEntry& entry,
                                 ConjugationType conj_type) {
  entries_.push_back({entry, conj_type});
}

core::Expected<std::vector<uint8_t>, core::Error> BinaryDictWriter::build() {
  if (entries_.empty()) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput,
                    "No entries to write"));
  }

  // Sort entries by surface for trie building
  std::sort(entries_.begin(), entries_.end(),
            [](const EntryData& lhs, const EntryData& rhs) {
              return lhs.entry.surface < rhs.entry.surface;
            });

  // Build string pool
  std::vector<char> string_pool;
  std::vector<BinaryDictEntry> binary_entries;
  binary_entries.reserve(entries_.size());

  for (const auto& ent : entries_) {
    BinaryDictEntry rec{};

    // Add surface to string pool
    rec.surface_offset = static_cast<uint32_t>(string_pool.size());
    rec.surface_length = static_cast<uint16_t>(ent.entry.surface.size());
    string_pool.insert(string_pool.end(),
                       ent.entry.surface.begin(), ent.entry.surface.end());

    // Add lemma if different from surface
    if (!ent.entry.lemma.empty() && ent.entry.lemma != ent.entry.surface) {
      rec.lemma_offset = static_cast<uint32_t>(string_pool.size());
      rec.lemma_length = static_cast<uint16_t>(ent.entry.lemma.size());
      string_pool.insert(string_pool.end(),
                         ent.entry.lemma.begin(), ent.entry.lemma.end());
    } else {
      rec.lemma_offset = 0;
      rec.lemma_length = 0;
    }

    rec.pos = posToUint8(ent.entry.pos);
    rec.conj_type = static_cast<uint8_t>(ent.conj_type);
    rec.cost = floatToCost(ent.entry.cost);

    uint8_t flags = 0;
    if (ent.entry.is_formal_noun) {
      flags |= kFlagFormalNoun;
    }
    if (ent.entry.is_low_info) {
      flags |= kFlagLowInfo;
    }
    if (ent.entry.is_prefix) {
      flags |= kFlagPrefix;
    }
    rec.flags = flags;

    binary_entries.push_back(rec);
  }

  // Build trie
  std::vector<std::string> keys;
  std::vector<int32_t> values;
  keys.reserve(entries_.size());
  values.reserve(entries_.size());

  for (size_t idx = 0; idx < entries_.size(); ++idx) {
    keys.push_back(entries_[idx].entry.surface);
    values.push_back(static_cast<int32_t>(idx));
  }

  DoubleArray trie;
  if (!trie.build(keys, values)) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError,
                    "Failed to build dictionary trie"));
  }

  auto trie_data = trie.serialize();

  // Calculate offsets
  size_t header_size = sizeof(BinaryDictHeader);
  size_t trie_offset = header_size;
  size_t trie_size = trie_data.size();
  size_t entry_offset = trie_offset + trie_size;
  size_t entry_size = binary_entries.size() * sizeof(BinaryDictEntry);
  size_t string_offset = entry_offset + entry_size;
  size_t total_size = string_offset + string_pool.size();

  // Build output
  std::vector<uint8_t> output(total_size);
  uint8_t* ptr = output.data();

  // Write header
  BinaryDictHeader header{};
  header.magic = BinaryDictHeader::kMagic;
  header.version_major = BinaryDictHeader::kVersionMajor;
  header.version_minor = BinaryDictHeader::kVersionMinor;
  header.entry_count = static_cast<uint32_t>(entries_.size());
  header.trie_offset = static_cast<uint32_t>(trie_offset);
  header.trie_size = static_cast<uint32_t>(trie_size);
  header.entry_offset = static_cast<uint32_t>(entry_offset);
  header.string_offset = static_cast<uint32_t>(string_offset);
  header.flags = 0;
  header.checksum = 0;

  std::memcpy(ptr, &header, sizeof(header));
  ptr += sizeof(header);

  // Write trie
  std::memcpy(ptr, trie_data.data(), trie_data.size());
  ptr += trie_data.size();

  // Write entries
  std::memcpy(ptr, binary_entries.data(), entry_size);
  ptr += entry_size;

  // Write string pool
  std::memcpy(ptr, string_pool.data(), string_pool.size());

  return output;
}

core::Expected<size_t, core::Error> BinaryDictWriter::writeToFile(
    const std::string& path) {
  auto result = build();
  if (!result) {
    return core::makeUnexpected(result.error());
  }

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError,
                    "Failed to create dictionary file: " + path));
  }

  const auto& data = result.value();
  file.write(reinterpret_cast<const char*>(data.data()), data.size());
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError,
                    "Failed to write dictionary file: " + path));
  }

  return data.size();
}

}  // namespace suzume::dictionary
