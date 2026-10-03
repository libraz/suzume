#include "dictionary/binary_dict.h"

#include <algorithm>
#include <cstring>

#include "dictionary/dictionary_lookup.h"
#ifndef __EMSCRIPTEN__
#include <filesystem>
#include <fstream>
#endif
#include <limits>
#include <tuple>
#include <unordered_map>

#include "core/debug.h"
#include "normalize/utf8.h"

namespace suzume::dictionary {

namespace {

constexpr size_t kInlineFrontLength = 15;
constexpr size_t kMaxSerializedSurfaceLength = std::numeric_limits<uint8_t>::max();
constexpr uint16_t kPackedLemmaMask = 0x07FFU;
constexpr int64_t kMinPackedLemmaDelta = -1024;
constexpr int64_t kMaxPackedLemmaDelta = 1023;

uint8_t posToUint8(core::PartOfSpeech pos) {
  return static_cast<uint8_t>(pos);
}

core::PartOfSpeech uint8ToPos(uint8_t val) {
  return static_cast<core::PartOfSpeech>(val);
}

bool isValidPos(uint8_t val) {
  return core::isValidPartOfSpeech(static_cast<core::PartOfSpeech>(val));
}

uint8_t extendedPosToUint8(core::ExtendedPOS epos) {
  return static_cast<uint8_t>(epos);
}

core::ExtendedPOS uint8ToExtendedPos(uint8_t val) {
  if (val >= static_cast<uint8_t>(core::ExtendedPOS::Count_)) {
    return core::ExtendedPOS::Unknown;
  }
  return static_cast<core::ExtendedPOS>(val);
}

bool isValidExtendedPos(uint8_t val) {
  return core::isValidExtendedPos(static_cast<core::ExtendedPOS>(val));
}

bool encodeRelativeLemmaReference(size_t entry_index, size_t lemma_index, uint16_t& reference) {
  const int64_t delta = static_cast<int64_t>(lemma_index) - static_cast<int64_t>(entry_index);
  if (delta < kMinPackedLemmaDelta || delta > kMaxPackedLemmaDelta) {
    return false;
  }
  reference = delta < 0 ? static_cast<uint16_t>((-delta * 2) - 1) : static_cast<uint16_t>(delta * 2);
  return true;
}

int32_t decodeRelativeLemmaReference(uint16_t reference) {
  return static_cast<int32_t>(reference >> 1U) ^ -static_cast<int32_t>(reference & 1U);
}

struct GrammarPair {
  uint8_t pos;
  uint8_t extended_pos;
};

struct CompactEntry {
  uint16_t lemma_reference;
  uint8_t grammar_index;
};

uint16_t packEntry(const CompactEntry& entry) {
  return entry.lemma_reference | static_cast<uint16_t>(entry.grammar_index << 11U);
}

static_assert(sizeof(GrammarPair) == 2, "Compact grammar palette entries must remain two bytes");

template <typename T>
T readPod(const uint8_t* data, size_t offset) {
  T value{};
  std::memcpy(&value, data + offset, sizeof(T));
  return value;
}

size_t commonPrefixLength(std::string_view lhs, std::string_view rhs) {
  const size_t limit = std::min(lhs.size(), rhs.size());
  size_t length = 0;
  while (length < limit && lhs[length] == rhs[length]) {
    ++length;
  }
  return length;
}

void appendFrontLength(std::vector<uint8_t>& output, size_t length) {
  if (length < kInlineFrontLength) {
    return;
  }
  size_t remaining = length - kInlineFrontLength;
  do {
    uint8_t byte = static_cast<uint8_t>(remaining & 0x7FU);
    remaining >>= 7U;
    if (remaining != 0) {
      byte |= 0x80U;
    }
    output.push_back(byte);
  } while (remaining != 0);
}

bool readFrontLength(const uint8_t* data, size_t size, size_t& offset, uint8_t inline_length, size_t& length) {
  length = inline_length;
  if (inline_length < kInlineFrontLength) {
    return true;
  }

  size_t extra = 0;
  unsigned shift = 0;
  while (offset < size && shift < 16) {
    const uint8_t byte = data[offset++];
    extra |= static_cast<size_t>(byte & 0x7FU) << shift;
    if ((byte & 0x80U) == 0) {
      if (extra > kMaxSerializedSurfaceLength - kInlineFrontLength) {
        return false;
      }
      length += extra;
      return true;
    }
    shift += 7;
  }
  return false;
}

core::Expected<std::vector<uint8_t>, core::Error> encodeFrontCodedSurfaces(
    const std::vector<DictionaryEntry>& entries) {
  std::vector<uint8_t> output;
  std::string_view previous;
  for (const DictionaryEntry& entry : entries) {
    const size_t prefix_length = commonPrefixLength(previous, entry.surface);
    const size_t suffix_length = entry.surface.size() - prefix_length;

    const uint8_t control = static_cast<uint8_t>((std::min(prefix_length, kInlineFrontLength) << 4U) |
                                                 std::min(suffix_length, kInlineFrontLength));
    output.push_back(control);
    appendFrontLength(output, prefix_length);
    appendFrontLength(output, suffix_length);
    output.insert(output.end(), entry.surface.begin() + static_cast<std::ptrdiff_t>(prefix_length),
                  entry.surface.end());
    previous = entry.surface;
  }
  return output;
}

bool decodeFrontCodedSurfaces(const uint8_t* data, size_t size, size_t entry_count,
                              std::vector<std::string>& surfaces) {
  surfaces.clear();
  surfaces.reserve(entry_count);
  size_t offset = 0;
  std::string previous;
  for (size_t entry_idx = 0; entry_idx < entry_count; ++entry_idx) {
    if (offset >= size) {
      return false;
    }
    const uint8_t control = data[offset++];
    size_t prefix_length = 0;
    size_t suffix_length = 0;
    if (!readFrontLength(data, size, offset, control >> 4U, prefix_length) ||
        !readFrontLength(data, size, offset, control & 0x0FU, suffix_length) || prefix_length > previous.size() ||
        suffix_length > size - offset || prefix_length + suffix_length > kMaxSerializedSurfaceLength) {
      return false;
    }
    const bool repeats_previous = suffix_length == 0;
    if (repeats_previous && (previous.empty() || prefix_length != previous.size())) {
      return false;
    }

    std::string surface(previous.data(), prefix_length);
    surface.append(reinterpret_cast<const char*>(data + offset), suffix_length);
    offset += suffix_length;
    if (surface.empty() || !normalize::isValidUtf8(surface) ||
        (!previous.empty() && (repeats_previous ? surface != previous : surface <= previous))) {
      return false;
    }
    surfaces.push_back(surface);
    previous = std::move(surface);
  }
  return offset == size;
}

}  // namespace

// BinaryDictionary implementation

BinaryDictionary::BinaryDictionary() = default;
BinaryDictionary::~BinaryDictionary() = default;

core::Expected<size_t, core::Error> BinaryDictionary::loadFromFile(const std::string& path) {
#ifdef __EMSCRIPTEN__
  (void)path;
  return core::makeUnexpected(
      core::Error(core::ErrorCode::InvalidInput, "File dictionary loading is unavailable in WASM"));
#else
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return core::makeUnexpected(core::Error(core::ErrorCode::FileNotFound, "Failed to open dictionary file: " + path));
  }

  size_t file_size = static_cast<size_t>(file.tellg());
  file.seekg(0);

  std::vector<uint8_t> loaded_data(file_size);
  if (!file.read(reinterpret_cast<char*>(loaded_data.data()), file_size)) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InternalError, "Failed to read dictionary file: " + path));
  }

  // Reuse the memory loader so file and embedded dictionaries share the same
  // validation and atomic publish path. The decoded trie and entries own their
  // data, so loaded_data can be released immediately after this call.
  return loadFromMemory(loaded_data.data(), loaded_data.size());
#endif
}

core::Expected<size_t, core::Error> BinaryDictionary::loadFromMemory(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Empty dictionary data"));
  }

  DoubleArray loaded_trie;
  std::vector<DictionaryEntry> loaded_entries;
  auto result = parseData(data, size, loaded_trie, loaded_entries);
  if (!result.hasValue()) {
    return result;
  }

  // loaded_data is not retained (see loadFromFile): decoded structures own copies.
  trie_ = std::move(loaded_trie);
  entries_ = std::move(loaded_entries);
  return result;
}

core::Expected<size_t, core::Error> BinaryDictionary::parseData(const uint8_t* data, size_t size, DoubleArray& trie,
                                                                std::vector<DictionaryEntry>& entries) {
  if (size < sizeof(BinaryDictHeader)) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary file too small"));
  }

  const auto header = readPod<BinaryDictHeader>(data, 0);

  // Validate magic
  if (header.magic != BinaryDictHeader::kMagic) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary magic number"));
  }

  if (header.version != BinaryDictHeader::kVersion) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Unsupported dictionary version"));
  }

  const size_t entry_count = header.entry_count;
  if (entry_count > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary has too many entries"));
  }
  const size_t surface_offset = sizeof(BinaryDictHeader);
  const size_t surface_size = header.surface_size;
  const uint8_t entry_encoding = header.flags & BinaryDictHeader::kEntryEncodingMask;
  const bool uses_relative_lemmas = (header.flags & BinaryDictHeader::kRelativeLemmaRefs) != 0;
  if (entry_count == 0 || surface_size == 0 || surface_size > size - surface_offset ||
      (header.flags & ~BinaryDictHeader::kKnownFlags) != 0 ||
      (uses_relative_lemmas && entry_encoding != BinaryDictHeader::kPackedEntries &&
       entry_encoding != BinaryDictHeader::kRecordPaletteEntries) ||
      header.reserved != 0) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary header"));
  }
  size_t entry_table_offset = surface_offset + surface_size;
  size_t entry_record_size = 0;
  std::vector<GrammarPair> grammar_palette;
  if (entry_table_offset >= size) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Missing dictionary grammar palette"));
  }
  const size_t palette_count = data[entry_table_offset++];
  const size_t palette_bytes = palette_count * sizeof(GrammarPair);
  if (palette_count == 0 || palette_bytes > size - entry_table_offset) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary grammar palette"));
  }
  grammar_palette.reserve(palette_count);
  for (size_t idx = 0; idx < palette_count; ++idx) {
    const size_t pair_offset = entry_table_offset + idx * sizeof(GrammarPair);
    GrammarPair pair{data[pair_offset], data[pair_offset + 1]};
    if (!isValidPos(pair.pos) || !isValidExtendedPos(pair.extended_pos)) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary grammar palette value"));
    }
    grammar_palette.push_back(pair);
  }
  entry_table_offset += palette_bytes;
  const uint8_t* record_palette = nullptr;
  size_t record_palette_size = 0;
  switch (entry_encoding) {
    case BinaryDictHeader::kRecordPaletteEntries: {
      if (entry_table_offset >= size) {
        return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Missing dictionary record palette"));
      }
      const size_t record_count = data[entry_table_offset++];
      const size_t record_bytes = record_count * sizeof(uint16_t);
      if (record_count == 0 || record_bytes > size - entry_table_offset) {
        return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary record palette"));
      }
      record_palette = data + entry_table_offset;
      record_palette_size = record_count;
      entry_table_offset += record_bytes;
      entry_record_size = 1;
      break;
    }
    case BinaryDictHeader::kGrammarOnlyEntries:
      entry_record_size = 1;
      break;
    case BinaryDictHeader::kPackedEntries:
      entry_record_size = 2;
      break;
    case BinaryDictHeader::kWideEntries:
      entry_record_size = kWideCompactEntrySize;
      break;
    default:
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Invalid compact dictionary entry encoding"));
  }
  if (entry_count > (std::numeric_limits<size_t>::max() / entry_record_size)) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary entry table too large"));
  }
  const size_t entry_table_size = entry_count * entry_record_size;
  if (entry_table_size > size - entry_table_offset) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary entry table"));
  }
  const size_t string_offset = entry_table_offset + entry_table_size;

  std::vector<std::string> trie_surfaces;
  if (!decodeFrontCodedSurfaces(data + surface_offset, surface_size, entry_count, trie_surfaces)) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary surface table"));
  }
  std::vector<std::string> trie_keys;
  std::vector<uint32_t> trie_values;
  trie_keys.reserve(trie_surfaces.size());
  trie_values.reserve(trie_surfaces.size());
  for (size_t idx = 0; idx < trie_surfaces.size(); ++idx) {
    if (idx == 0 || trie_surfaces[idx] != trie_surfaces[idx - 1]) {
      trie_keys.push_back(trie_surfaces[idx]);
      trie_values.push_back(static_cast<uint32_t>(idx));
    }
  }
  if (!trie.build(trie_keys, trie_values)) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Failed to build dictionary trie"));
  }

  // Relative references consume the entire tail; other encodings use it as a
  // length-prefixed lemma pool.
  std::vector<std::string_view> compact_lemmas;
  if (uses_relative_lemmas) {
    if (string_offset != size) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Relative lemma dictionary has trailing data"));
    }
  } else {
    size_t lemma_offset = string_offset;
    while (lemma_offset < size) {
      const size_t lemma_length = data[lemma_offset++];
      if (lemma_length == 0 || lemma_length > size - lemma_offset) {
        return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid compact lemma table"));
      }
      const std::string_view lemma(reinterpret_cast<const char*>(data + lemma_offset), lemma_length);
      if (!normalize::isValidUtf8(lemma)) {
        return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary lemma is not valid UTF-8"));
      }
      compact_lemmas.push_back(lemma);
      lemma_offset += lemma_length;
      if (compact_lemmas.size() > std::numeric_limits<uint16_t>::max()) {
        return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Compact lemma table is too large"));
      }
    }
  }

  entries.clear();
  entries.reserve(entry_count);

  for (uint32_t idx = 0; idx < header.entry_count; ++idx) {
    const size_t entry_pos = entry_table_offset + idx * entry_record_size;
    uint16_t lemma_reference = 0;
    uint8_t grammar_idx = 0;
    switch (entry_encoding) {
      case BinaryDictHeader::kRecordPaletteEntries: {
        const uint8_t record_idx = data[entry_pos];
        if (record_idx >= record_palette_size) {
          return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid record palette index"));
        }
        const uint16_t packed = readPod<uint16_t>(record_palette, record_idx * sizeof(uint16_t));
        lemma_reference = packed & kPackedLemmaMask;
        grammar_idx = static_cast<uint8_t>(packed >> 11U);
        break;
      }
      case BinaryDictHeader::kGrammarOnlyEntries:
        grammar_idx = data[entry_pos];
        break;
      case BinaryDictHeader::kPackedEntries: {
        const uint16_t packed = static_cast<uint16_t>(data[entry_pos]) |
                                static_cast<uint16_t>(static_cast<uint16_t>(data[entry_pos + 1]) << 8U);
        lemma_reference = packed & kPackedLemmaMask;
        grammar_idx = static_cast<uint8_t>(packed >> 11U);
        break;
      }
      case BinaryDictHeader::kWideEntries:
        lemma_reference = static_cast<uint16_t>(data[entry_pos]) |
                          static_cast<uint16_t>(static_cast<uint16_t>(data[entry_pos + 1]) << 8U);
        grammar_idx = data[entry_pos + 2];
        break;
      default:
        break;
    }
    if (grammar_idx >= grammar_palette.size()) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary grammar palette index"));
    }
    const uint8_t pos = grammar_palette[grammar_idx].pos;
    const uint8_t extended_pos = grammar_palette[grammar_idx].extended_pos;

    DictionaryEntry entry;
    entry.pos = uint8ToPos(pos);

    if (uses_relative_lemmas) {
      const int64_t lemma_target = static_cast<int64_t>(idx) + decodeRelativeLemmaReference(lemma_reference);
      if (lemma_target < 0 || lemma_target >= static_cast<int64_t>(entry_count)) {
        return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid relative lemma reference"));
      }
      entry.lemma = trie_surfaces[static_cast<size_t>(lemma_target)];
    } else if (lemma_reference > 0) {
      if (lemma_reference > compact_lemmas.size()) {
        return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid compact lemma index"));
      }
      entry.lemma = compact_lemmas[lemma_reference - 1];
    } else {
      entry.lemma = trie_surfaces[idx];
    }

    entry.extended_pos = uint8ToExtendedPos(extended_pos);

    entries.push_back(std::move(entry));
  }

  for (size_t idx = 0; idx < entry_count; ++idx) {
    entries[idx].surface = std::move(trie_surfaces[idx]);
    // Debug: log entries with Unknown extended_pos (indicates missing category mapping)
    // These entries get high cost (2.0) which may cause unexpected tokenization.
    if (entries[idx].extended_pos == core::ExtendedPOS::Unknown) {
      SUZUME_DEBUG_LOG_TRACE("[DICT_LOAD] WARNING: \"" << entries[idx].surface
                                                       << "\" pos=" << core::posToString(entries[idx].pos)
                                                       << " has epos=UNKNOWN (cost=2.0)\n");
    }
  }

  return entries.size();
}

std::vector<LookupResult> BinaryDictionary::lookup(std::string_view text, size_t start_pos) const {
  return lookupByTrie(trie_, entries_, text, start_pos);
}

const DictionaryEntry* BinaryDictionary::lookupExact(std::string_view surface, core::PartOfSpeech pos) const {
  return lookupExactByTrie(trie_, entries_, surface, pos);
}

const DictionaryEntry* BinaryDictionary::getEntry(uint32_t idx) const {
  return entryAt(entries_, idx);
}

// BinaryDictWriter implementation

BinaryDictWriter::BinaryDictWriter() = default;

void BinaryDictWriter::addEntry(const DictionaryEntry& entry) {
  entries_.push_back(entry);
}

core::Expected<std::vector<uint8_t>, core::Error> BinaryDictWriter::build() {
  if (entries_.empty()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "No entries to write"));
  }
  if (entries_.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary has too many entries"));
  }

  // Keep grammatical homographs consecutive and deterministic. The trie points
  // at the first entry for each surface and lookup returns the whole group.
  std::sort(entries_.begin(), entries_.end(), [](const DictionaryEntry& lhs, const DictionaryEntry& rhs) {
    return std::tie(lhs.surface, lhs.pos, lhs.extended_pos, lhs.lemma) <
           std::tie(rhs.surface, rhs.pos, rhs.extended_pos, rhs.lemma);
  });

  // Entries use a lemma reference and an index into a per-file
  // POS/ExtendedPOS palette. The final representation is selected after
  // collecting both palettes.
  std::vector<char> compact_lemma_table;
  std::unordered_map<std::string, uint16_t> lemma_indices;
  std::vector<GrammarPair> grammar_palette;
  std::unordered_map<uint16_t, uint8_t> grammar_indices;
  std::vector<CompactEntry> compact_entries;
  compact_entries.reserve(entries_.size());

  for (const auto& ent : entries_) {
    if (ent.surface.empty()) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary surface must not be empty"));
    }
    if (ent.surface.find('\0') != std::string::npos) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Dictionary surface contains an embedded NUL byte"));
    }
    if (!normalize::isValidUtf8(ent.surface) || (!ent.lemma.empty() && !normalize::isValidUtf8(ent.lemma))) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary entry is not valid UTF-8"));
    }

    if (ent.surface.size() > std::numeric_limits<uint8_t>::max()) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Dictionary surface exceeds 255 bytes: " + ent.surface));
    }

    if (!ent.lemma.empty() && ent.lemma.size() > std::numeric_limits<uint8_t>::max()) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Dictionary lemma exceeds 255 bytes: " + ent.lemma));
    }

    if (!isValidPos(posToUint8(ent.pos))) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary entry has invalid POS"));
    }

    core::ExtendedPOS extended_pos_value = ent.extended_pos;
    if (extended_pos_value == core::ExtendedPOS::Unknown && ent.pos != core::PartOfSpeech::Unknown) {
      extended_pos_value = core::posToExtendedPos(ent.pos);
    }
    if (!isValidExtendedPos(extendedPosToUint8(extended_pos_value))) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Dictionary entry has invalid extended POS"));
    }

    uint16_t lemma_reference = 0;
    if (!ent.lemma.empty() && ent.lemma != ent.surface) {
      auto lemma_iter = lemma_indices.find(ent.lemma);
      if (lemma_iter != lemma_indices.end()) {
        lemma_reference = lemma_iter->second;
      } else if (lemma_indices.size() >= std::numeric_limits<uint16_t>::max()) {
        return core::makeUnexpected(
            core::Error(core::ErrorCode::InvalidInput, "Dictionary has too many distinct lemma strings"));
      } else {
        lemma_reference = static_cast<uint16_t>(lemma_indices.size() + 1);
        lemma_indices.emplace(ent.lemma, lemma_reference);
        compact_lemma_table.push_back(static_cast<char>(ent.lemma.size()));
        compact_lemma_table.insert(compact_lemma_table.end(), ent.lemma.begin(), ent.lemma.end());
      }
    }

    const uint8_t pos = posToUint8(ent.pos);
    const uint8_t extended_pos = extendedPosToUint8(extended_pos_value);
    const uint16_t grammar_key = static_cast<uint16_t>(pos) << 8U | extended_pos;
    uint8_t grammar_index = 0;
    auto grammar_iter = grammar_indices.find(grammar_key);
    if (grammar_iter != grammar_indices.end()) {
      grammar_index = grammar_iter->second;
    } else if (grammar_palette.size() >= std::numeric_limits<uint8_t>::max()) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Dictionary has too many grammar categories"));
    } else {
      grammar_index = static_cast<uint8_t>(grammar_palette.size());
      grammar_indices.emplace(grammar_key, grammar_index);
      grammar_palette.push_back({pos, extended_pos});
    }

    compact_entries.push_back({lemma_reference, grammar_index});
  }

  for (size_t idx = 1; idx < entries_.size(); ++idx) {
    const auto& previous = entries_[idx - 1];
    const auto& entry = entries_[idx];
    if (std::tie(previous.surface, previous.pos, previous.extended_pos, previous.lemma) ==
        std::tie(entry.surface, entry.pos, entry.extended_pos, entry.lemma)) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Duplicate dictionary entry: " + entry.surface));
    }
  }
  std::vector<std::string> trie_surfaces;
  trie_surfaces.reserve(entries_.size());
  for (const auto& entry : entries_) {
    if (trie_surfaces.empty() || trie_surfaces.back() != entry.surface) {
      trie_surfaces.push_back(entry.surface);
    }
  }
  DoubleArray trie_validation;
  if (!trie_validation.build(trie_surfaces)) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput,
                    "Dictionary surfaces cannot be represented by the runtime trie (duplicate or capacity limit)"));
  }

  // If every differing lemma is also a nearby surface, encode its signed
  // surface-index delta directly in the packed entry. This removes the lemma
  // string pool and makes the small deltas substantially more compressible.
  bool uses_relative_lemmas = !lemma_indices.empty() && grammar_palette.size() <= 32;
  std::vector<uint16_t> relative_lemma_references;
  if (uses_relative_lemmas) {
    relative_lemma_references.resize(entries_.size());
    for (size_t idx = 0; idx < entries_.size(); ++idx) {
      const auto& entry = entries_[idx];
      if (entry.lemma.empty() || entry.lemma == entry.surface) {
        continue;
      }
      const std::string_view lemma = entry.lemma;
      const auto lemma_iter = std::lower_bound(entries_.begin(), entries_.end(), lemma,
                                               [](const DictionaryEntry& candidate, std::string_view value) {
                                                 return std::string_view(candidate.surface) < value;
                                               });
      uint16_t reference = 0;
      if (lemma_iter == entries_.end() || lemma_iter->surface != lemma ||
          !encodeRelativeLemmaReference(idx, static_cast<size_t>(lemma_iter - entries_.begin()), reference)) {
        uses_relative_lemmas = false;
        break;
      }
      relative_lemma_references[idx] = reference;
    }
  }
  if (uses_relative_lemmas) {
    for (size_t idx = 0; idx < compact_entries.size(); ++idx) {
      compact_entries[idx].lemma_reference = relative_lemma_references[idx];
    }
    compact_lemma_table.clear();
  }

  std::vector<uint8_t> entry_data;
  size_t compact_entry_size = kWideCompactEntrySize;
  uint8_t output_flags = BinaryDictHeader::kWideEntries;
  // Keep grammar-only entries as their dense byte array even when one grammar
  // index dominates. A default-plus-sparse-exceptions experiment reduced
  // user.dic by 2,682 raw bytes, but most original bytes are zero and Binaryen
  // already represents those runs efficiently. The extra decoder and non-zero
  // exception indexes made the final WASM gzip larger, so file size alone is a
  // misleading selection criterion here.
  if (lemma_indices.empty()) {
    output_flags = BinaryDictHeader::kGrammarOnlyEntries;
    compact_entry_size = 1;
  } else if (uses_relative_lemmas) {
    output_flags = BinaryDictHeader::kPackedEntries | BinaryDictHeader::kRelativeLemmaRefs;
    compact_entry_size = 2;
  } else if (lemma_indices.size() <= 0x07FFU && grammar_palette.size() <= 32) {
    output_flags = BinaryDictHeader::kPackedEntries;
    compact_entry_size = 2;
  }

  std::vector<uint16_t> record_palette;
  std::unordered_map<uint16_t, uint8_t> record_indices;
  // Packed core records are mostly non-zero and repeat heavily, unlike the
  // grammar-only zero runs above. A byte-indexed palette reduced core.dic by
  // 5,148 bytes and still reduced the complete WASM after decoder cost.
  if ((output_flags & BinaryDictHeader::kEntryEncodingMask) == BinaryDictHeader::kPackedEntries) {
    for (const auto& entry : compact_entries) {
      const uint16_t packed = packEntry(entry);
      if (record_indices.find(packed) == record_indices.end()) {
        if (record_palette.size() >= std::numeric_limits<uint8_t>::max()) {
          record_palette.clear();
          break;
        }
        const uint8_t record_idx = static_cast<uint8_t>(record_palette.size());
        record_indices.emplace(packed, record_idx);
        record_palette.push_back(packed);
      }
    }
    if (!record_palette.empty() && 1 + record_palette.size() * sizeof(uint16_t) + compact_entries.size() <
                                       compact_entries.size() * sizeof(uint16_t)) {
      output_flags = static_cast<uint8_t>((output_flags & ~BinaryDictHeader::kEntryEncodingMask) |
                                          BinaryDictHeader::kRecordPaletteEntries);
      compact_entry_size = 1;
    } else {
      record_palette.clear();
    }
  }

  entry_data.reserve(1 + grammar_palette.size() * sizeof(GrammarPair) + compact_entries.size() * compact_entry_size);
  entry_data.push_back(static_cast<uint8_t>(grammar_palette.size()));
  for (const auto& pair : grammar_palette) {
    entry_data.push_back(pair.pos);
    entry_data.push_back(pair.extended_pos);
  }
  if (!record_palette.empty()) {
    entry_data.push_back(static_cast<uint8_t>(record_palette.size()));
    for (uint16_t packed : record_palette) {
      entry_data.push_back(static_cast<uint8_t>(packed & 0xFFU));
      entry_data.push_back(static_cast<uint8_t>(packed >> 8U));
    }
    for (const auto& entry : compact_entries) {
      entry_data.push_back(record_indices.at(packEntry(entry)));
    }
  } else {
    const uint8_t entry_encoding = output_flags & BinaryDictHeader::kEntryEncodingMask;
    for (const auto& entry : compact_entries) {
      if (entry_encoding == BinaryDictHeader::kGrammarOnlyEntries) {
        entry_data.push_back(entry.grammar_index);
      } else if (entry_encoding == BinaryDictHeader::kPackedEntries) {
        const uint16_t packed = packEntry(entry);
        entry_data.push_back(static_cast<uint8_t>(packed & 0xFFU));
        entry_data.push_back(static_cast<uint8_t>(packed >> 8U));
      } else {
        entry_data.push_back(static_cast<uint8_t>(entry.lemma_reference & 0xFFU));
        entry_data.push_back(static_cast<uint8_t>(entry.lemma_reference >> 8U));
        entry_data.push_back(entry.grammar_index);
      }
    }
  }

  auto surface_result = encodeFrontCodedSurfaces(entries_);
  if (!surface_result) {
    return core::makeUnexpected(surface_result.error());
  }
  std::vector<uint8_t> surface_data = std::move(surface_result.value());
  std::vector<char> string_pool = std::move(compact_lemma_table);

  // Calculate offsets
  const size_t surface_offset = sizeof(BinaryDictHeader);
  const size_t surface_size = surface_data.size();
  const size_t entry_offset = surface_offset + surface_size;
  const size_t entry_size = entry_data.size();
  const size_t string_offset = entry_offset + entry_size;
  const size_t total_size = string_offset + string_pool.size();

  if (total_size > std::numeric_limits<uint32_t>::max()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary binary is too large"));
  }

  // Build output
  std::vector<uint8_t> output(total_size);
  uint8_t* ptr = output.data();

  BinaryDictHeader header{};
  header.magic = BinaryDictHeader::kMagic;
  header.entry_count = static_cast<uint32_t>(entries_.size());
  header.surface_size = static_cast<uint32_t>(surface_size);
  header.version = BinaryDictHeader::kVersion;
  header.flags = output_flags;

  std::memcpy(ptr, &header, sizeof(header));
  ptr += sizeof(header);
  std::memcpy(ptr, surface_data.data(), surface_data.size());
  ptr += surface_data.size();
  std::memcpy(ptr, entry_data.data(), entry_size);
  ptr += entry_size;
  if (!string_pool.empty()) {
    std::memcpy(ptr, string_pool.data(), string_pool.size());
  }
  return output;
}

core::Expected<size_t, core::Error> BinaryDictWriter::writeToFile(const std::string& path) {
#ifdef __EMSCRIPTEN__
  (void)path;
  return core::makeUnexpected(
      core::Error(core::ErrorCode::InvalidInput, "File dictionary writing is unavailable in WASM"));
#else
  auto result = build();
  if (!result) {
    return core::makeUnexpected(result.error());
  }

  const std::filesystem::path output_path(path);
  const std::filesystem::path temporary_path = output_path.string() + ".tmp";
  std::ofstream file(temporary_path, std::ios::binary);
  if (!file) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InternalError,
                                            "Failed to create temporary dictionary file: " + temporary_path.string()));
  }

  const auto& data = result.value();
  file.write(reinterpret_cast<const char*>(data.data()), data.size());
  file.close();
  if (!file) {
    std::error_code remove_error;
    std::filesystem::remove(temporary_path, remove_error);
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError, "Failed to write dictionary file: " + path));
  }

  std::error_code rename_error;
  std::filesystem::rename(temporary_path, output_path, rename_error);
  if (rename_error) {
    std::error_code remove_error;
    std::filesystem::remove(temporary_path, remove_error);
    return core::makeUnexpected(core::Error(
        core::ErrorCode::InternalError, "Failed to replace dictionary file: " + path + ": " + rename_error.message()));
  }

  return data.size();
#endif
}

}  // namespace suzume::dictionary
