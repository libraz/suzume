#include "dictionary/binary_dict.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_map>

#include "analysis/category_cost.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"

namespace suzume::dictionary {

namespace {

// Flag bits
constexpr uint8_t kFlagFormalNoun = 0x01;
constexpr uint8_t kFlagInterjection = 0x08;
constexpr uint8_t kFlagProperFamily = 0x10;
constexpr uint8_t kFlagProperGiven = 0x20;

/**
 * @brief Count UTF-8 characters in a byte range
 * @param text Full text
 * @param start_byte Start byte position
 * @param byte_length Length in bytes
 * @return Number of UTF-8 characters
 */
size_t countUtf8Chars(std::string_view text, size_t start_byte, size_t byte_length) {
  size_t char_count = 0;
  size_t end_byte = start_byte + byte_length;

  for (size_t pos = start_byte; pos < end_byte && pos < text.size();) {
    auto byte = static_cast<uint8_t>(text[pos]);
    // Count UTF-8 lead bytes (not continuation bytes 10xxxxxx)
    if ((byte & 0xC0) != 0x80) {
      ++char_count;
    }
    ++pos;
  }

  return char_count;
}

uint8_t posToUint8(core::PartOfSpeech pos) {
  return static_cast<uint8_t>(pos);
}

core::PartOfSpeech uint8ToPos(uint8_t val) {
  return static_cast<core::PartOfSpeech>(val);
}

bool isValidPos(uint8_t val) {
  return val < static_cast<uint8_t>(core::PartOfSpeech::Count_);
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
  return val < static_cast<uint8_t>(core::ExtendedPOS::Count_);
}

struct BinaryDictEntryV0 {
  uint32_t surface_offset;
  uint32_t lemma_offset;
  uint8_t surface_length;
  uint8_t lemma_length;
  uint8_t pos;
  uint8_t flags;
};

template <typename T>
T readPod(const std::vector<uint8_t>& data, size_t offset) {
  T value{};
  std::memcpy(&value, data.data() + offset, sizeof(T));
  return value;
}

}  // namespace

// BinaryDictionary implementation

BinaryDictionary::BinaryDictionary() = default;
BinaryDictionary::~BinaryDictionary() = default;

core::Expected<size_t, core::Error> BinaryDictionary::loadFromFile(const std::string& path) {
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

  DoubleArray loaded_trie;
  std::vector<DictionaryEntry> loaded_entries;
  auto result = parseData(loaded_data, loaded_trie, loaded_entries);
  if (!result.hasValue()) {
    return result;
  }

  data_ = std::move(loaded_data);
  trie_ = std::move(loaded_trie);
  entries_ = std::move(loaded_entries);
  return result;
}

core::Expected<size_t, core::Error> BinaryDictionary::loadFromMemory(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Empty dictionary data"));
  }

  std::vector<uint8_t> loaded_data(data, data + size);
  DoubleArray loaded_trie;
  std::vector<DictionaryEntry> loaded_entries;
  auto result = parseData(loaded_data, loaded_trie, loaded_entries);
  if (!result.hasValue()) {
    return result;
  }

  data_ = std::move(loaded_data);
  trie_ = std::move(loaded_trie);
  entries_ = std::move(loaded_entries);
  return result;
}

core::Expected<size_t, core::Error> BinaryDictionary::parseData(const std::vector<uint8_t>& data, DoubleArray& trie,
                                                                std::vector<DictionaryEntry>& entries) {
  if (data.size() < sizeof(BinaryDictHeader)) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary file too small"));
  }

  const auto header = readPod<BinaryDictHeader>(data, 0);

  // Validate magic
  if (header.magic != BinaryDictHeader::kMagic) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary magic number"));
  }

  // Validate version
  if (header.version_major != BinaryDictHeader::kVersionMajor) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Unsupported dictionary version"));
  }
  if (header.version_minor > BinaryDictHeader::kVersionMinor) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Unsupported dictionary minor version"));
  }

  // Validate offsets and section ranges before reading variable-length data.
  if (header.trie_offset > data.size() || header.trie_size > data.size() - header.trie_offset ||
      header.entry_offset > data.size() || header.string_offset > data.size()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary offsets"));
  }
  if (header.trie_offset < sizeof(BinaryDictHeader) || header.entry_offset < header.trie_offset + header.trie_size) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary section order"));
  }

  size_t entry_count = header.entry_count;
  if (entry_count > (std::numeric_limits<size_t>::max() / sizeof(BinaryDictEntry))) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary entry table too large"));
  }

  size_t entry_record_size = header.version_minor >= 1 ? sizeof(BinaryDictEntry) : sizeof(BinaryDictEntryV0);
  size_t entry_table_size = entry_count * entry_record_size;
  if (header.entry_offset > data.size() || entry_table_size > data.size() - header.entry_offset ||
      header.entry_offset + entry_table_size > header.string_offset) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary entry table"));
  }

  if (header.string_offset < header.entry_offset + entry_table_size) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary string pool offset"));
  }

  size_t string_pool_size = data.size() - header.string_offset;

  // Load trie
  if (!trie.deserialize(data.data() + header.trie_offset, header.trie_size)) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Failed to load dictionary trie"));
  }

  // Load entries
  const char* string_pool = reinterpret_cast<const char*>(data.data() + header.string_offset);

  entries.clear();
  entries.reserve(entry_count);

  for (uint32_t idx = 0; idx < header.entry_count; ++idx) {
    BinaryDictEntry rec{};
    size_t entry_pos = header.entry_offset + idx * entry_record_size;
    if (header.version_minor >= 1) {
      rec = readPod<BinaryDictEntry>(data, entry_pos);
    } else {
      const auto legacy = readPod<BinaryDictEntryV0>(data, entry_pos);
      rec.surface_offset = legacy.surface_offset;
      rec.lemma_offset = legacy.lemma_offset;
      rec.surface_length = legacy.surface_length;
      rec.lemma_length = legacy.lemma_length;
      rec.pos = legacy.pos;
      rec.flags = legacy.flags;
      rec.extended_pos = static_cast<uint8_t>(core::ExtendedPOS::Unknown);
    }

    if (rec.surface_offset > string_pool_size || rec.surface_length > string_pool_size - rec.surface_offset) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary surface string range"));
    }

    if (rec.surface_length == 0) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary surface must not be empty"));
    }

    if (!isValidPos(rec.pos)) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary POS value"));
    }

    if (header.version_minor >= 1 && !isValidExtendedPos(rec.extended_pos)) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary extended POS value"));
    }

    if (rec.lemma_length > 0 &&
        (rec.lemma_offset > string_pool_size || rec.lemma_length > string_pool_size - rec.lemma_offset)) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Invalid dictionary lemma string range"));
    }

    DictionaryEntry entry;
    entry.surface = std::string(string_pool + rec.surface_offset, rec.surface_length);
    entry.pos = uint8ToPos(rec.pos);

    if (rec.lemma_length > 0) {
      entry.lemma = std::string(string_pool + rec.lemma_offset, rec.lemma_length);
    } else {
      entry.lemma = entry.surface;
    }

    entry.extended_pos = uint8ToExtendedPos(rec.extended_pos);

    if (entry.extended_pos != core::ExtendedPOS::Unknown) {
      // Use the serialized fine-grained category when present.
    } else if ((rec.flags & kFlagFormalNoun) != 0) {
      entry.extended_pos = core::ExtendedPOS::NounFormal;
    } else if ((rec.flags & kFlagInterjection) != 0) {
      entry.extended_pos = core::ExtendedPOS::Interjection;
    } else if ((rec.flags & kFlagProperFamily) != 0) {
      entry.extended_pos = core::ExtendedPOS::NounProperFamily;
    } else if ((rec.flags & kFlagProperGiven) != 0) {
      entry.extended_pos = core::ExtendedPOS::NounProperGiven;
    } else {
      // Derive default extended_pos from POS for proper cost calculation
      switch (entry.pos) {
        case core::PartOfSpeech::Adjective: {
          // Distinguish I-adjective forms from NA-adjective based on ending
          // I-adjective forms: い, く, くて, かった, かっ, ければ, そう, etc.
          // NA-adjectives: don't end in these patterns
          // Exceptions: きれい, きらい are na-adjectives ending in い
          using namespace std::string_view_literals;
          if (utf8::endsWithAny(entry.surface, {"きれい"sv, "きらい"sv, "嫌い"sv, "綺麗"sv})) {
            entry.extended_pos = core::ExtendedPOS::AdjNaAdj;
          } else if (utf8::endsWith(entry.surface, "い")) {
            entry.extended_pos = core::ExtendedPOS::AdjBasic;
          } else if (utf8::endsWithAny(entry.surface, {"く"sv, "くて"sv})) {
            // く-form (adverbial/te-form): 美しく, 美しくて
            entry.extended_pos = core::ExtendedPOS::AdjRenyokei;
          } else if (utf8::endsWithAny(entry.surface, {"かっ"sv})) {
            // かっ-form (past stem): 美しかっ
            entry.extended_pos = core::ExtendedPOS::AdjKatt;
          } else if (utf8::endsWithAny(entry.surface, {"ければ"sv, "かったら"sv})) {
            // Conditional forms: 美しければ, 美しかったら
            entry.extended_pos = core::ExtendedPOS::AdjKeForm;
          } else if (utf8::endsWithAny(entry.surface, {"そう"sv})) {
            // Stem+そう: 美しそう
            entry.extended_pos = core::ExtendedPOS::AdjStem;
          } else {
            // Doesn't match I-adjective patterns → NA-adjective
            entry.extended_pos = core::ExtendedPOS::AdjNaAdj;
          }
          break;
        }
        case core::PartOfSpeech::Verb: {
          // Distinguish verb forms based on ending
          // 音便形: ends with っ/ん (onbin for ta/te form)
          using namespace std::string_view_literals;
          if (utf8::endsWithAny(entry.surface, {"っ"sv, "ん"sv})) {
            // Sokuonbin (っ) or hatsuonbin (ん): あっ, 飲ん, etc.
            entry.extended_pos = core::ExtendedPOS::VerbOnbinkei;
          } else if (utf8::endsWith(entry.surface, "い") && entry.surface.size() > core::kTwoJapaneseCharBytes) {
            // Godan-ka/ga i-onbin (い音便) for 3+ char compound verbs
            // e.g., たどり着い from たどり着く, 引っかい from 引っかく
            // Short forms (1-2 chars) are handled by the short-verb rules below
            entry.extended_pos = core::ExtendedPOS::VerbOnbinkei;
          } else if (utf8::endsWithAny(entry.surface, {"れば"sv, "けば"sv, "せば"sv, "てば"sv, "ねば"sv, "べば"sv,
                                                       "めば"sv, "えば"sv})) {
            // Conditional form
            entry.extended_pos = core::ExtendedPOS::VerbKateikei;
          } else if (entry.surface.size() == core::kJapaneseCharBytes) {
            // Single hiragana character verb forms are renyokei (連用形)
            // e.g., い from いる expansion, not shuushikei
            // This prevents incorrect VERB_終止→AUX_意志 connections like と→い→う
            entry.extended_pos = core::ExtendedPOS::VerbRenyokei;
          } else if (!utf8::endsWith(entry.surface, "る") && entry.surface.size() <= core::kTwoJapaneseCharBytes) {
            // Short verb forms (1-2 chars) not ending in る
            if (grammar::endsWithARow(entry.surface) && grammar::containsKanji(entry.surface)) {
              // Kanji + A-row ending = godan mizenkei (読ま, 書か, 行か)
              entry.extended_pos = core::ExtendedPOS::VerbMizenkei;
            } else {
              // Other short forms likely renyoukei (すぎ from すぎる)
              entry.extended_pos = core::ExtendedPOS::VerbRenyokei;
            }
          } else if (utf8::endsWithAny(entry.surface,
                                       {"き"sv, "ぎ"sv, "し"sv, "ち"sv, "に"sv, "び"sv, "み"sv, "り"sv})) {
            // Godan verb renyokei endings (I-row hiragana except い)
            // e.g., いただき from いただく → いただき + ます should work
            // Note: い excluded because godan-wa renyokei (思い) would need
            // disambiguation from noun/adj uses. Short forms are handled above.
            entry.extended_pos = core::ExtendedPOS::VerbRenyokei;
          } else if (utf8::endsWithAny(entry.surface,
                                       {"え"sv, "け"sv, "げ"sv, "せ"sv, "ぜ"sv, "ね"sv, "べ"sv, "め"sv, "れ"sv}) &&
                     entry.surface.size() > core::kTwoJapaneseCharBytes) {
            // Ichidan verb renyokei endings (E-row hiragana)
            // e.g., いただけ from いただける, 成し遂げ from 成し遂げる
            // Only for 3+ char forms to avoid te-form fragments (食べ+て, 捨て)
            // Short E-row forms are handled by the 1-2 char rule above
            // Note: て/で excluded — conflicts with te-form (捨て, 出で)
            entry.extended_pos = core::ExtendedPOS::VerbRenyokei;
          } else if (grammar::endsWithARow(entry.surface) && entry.surface.size() > core::kTwoJapaneseCharBytes) {
            // Godan verb mizenkei endings (A-row hiragana)
            // e.g., サボら from サボる → サボら + れる (passive) should work
            // Only for 3+ char forms to avoid conflicts with short words
            entry.extended_pos = core::ExtendedPOS::VerbMizenkei;
          } else {
            // Default: shuushikei (dictionary form or other forms)
            entry.extended_pos = core::ExtendedPOS::VerbShuushikei;
          }
          break;
        }
        case core::PartOfSpeech::Noun:
          entry.extended_pos = core::ExtendedPOS::Noun;
          break;
        case core::PartOfSpeech::Adverb:
          entry.extended_pos = core::ExtendedPOS::Adverb;
          break;
        case core::PartOfSpeech::Particle:
          entry.extended_pos = core::ExtendedPOS::ParticleCase;
          break;
        case core::PartOfSpeech::Auxiliary:
          entry.extended_pos = core::ExtendedPOS::AuxTenseTa;  // Default aux
          break;
        case core::PartOfSpeech::Suffix:
          entry.extended_pos = core::ExtendedPOS::Suffix;
          break;
        case core::PartOfSpeech::Prefix:
          entry.extended_pos = core::ExtendedPOS::Prefix;
          break;
        case core::PartOfSpeech::Conjunction:
          entry.extended_pos = core::ExtendedPOS::Conjunction;
          break;
        case core::PartOfSpeech::Determiner:
          entry.extended_pos = core::ExtendedPOS::Determiner;
          break;
        case core::PartOfSpeech::Pronoun:
          entry.extended_pos = core::ExtendedPOS::Pronoun;
          break;
        case core::PartOfSpeech::Symbol:
          entry.extended_pos = core::ExtendedPOS::Symbol;
          break;
        case core::PartOfSpeech::Other:
          entry.extended_pos = core::ExtendedPOS::Other;
          break;
        default:
          entry.extended_pos = core::ExtendedPOS::Unknown;
          break;
      }
    }
    // is_low_info, is_prefix, conj_type are no longer stored

    // Debug: log entries with Unknown extended_pos (indicates missing category mapping)
    // These entries get high cost (2.0) which may cause unexpected tokenization
    // At trace level (SUZUME_DEBUG=3) to avoid flooding output at lower levels
    if (entry.extended_pos == core::ExtendedPOS::Unknown) {
      SUZUME_DEBUG_LOG_TRACE("[DICT_LOAD] WARNING: \"" << entry.surface << "\" pos=" << core::posToString(entry.pos)
                                                       << " has epos=UNKNOWN (cost=2.0)\n");
    }

    entries.push_back(std::move(entry));
  }

  return entries.size();
}

std::vector<LookupResult> BinaryDictionary::lookup(std::string_view text, size_t start_pos) const {
  std::vector<LookupResult> results;

  if (!isLoaded() || start_pos >= text.size()) {
    return results;
  }

  auto trie_results = trie_.commonPrefixSearch(text, start_pos);

  for (const auto& tres : trie_results) {
    if (tres.value >= 0 && static_cast<size_t>(tres.value) < entries_.size()) {
      LookupResult result{};
      result.entry_id = static_cast<uint32_t>(tres.value);
      // Convert byte length from trie to character count
      result.length = countUtf8Chars(text, start_pos, tres.length);
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

void BinaryDictWriter::addEntry(const DictionaryEntry& entry) {
  entries_.push_back(entry);
}

void BinaryDictWriter::replaceEntry(const DictionaryEntry& entry) {
  for (auto& existing : entries_) {
    if (existing.surface == entry.surface) {
      existing = entry;
      return;
    }
  }
}

core::Expected<std::vector<uint8_t>, core::Error> BinaryDictWriter::build() {
  if (entries_.empty()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "No entries to write"));
  }

  // Sort entries by surface for trie building
  std::sort(entries_.begin(), entries_.end(),
            [](const DictionaryEntry& lhs, const DictionaryEntry& rhs) { return lhs.surface < rhs.surface; });

  // Build string pool with deduplication
  std::vector<char> string_pool;
  std::unordered_map<std::string, uint32_t> string_offsets;  // dedup map
  std::vector<BinaryDictEntry> binary_entries;
  binary_entries.reserve(entries_.size());

  // Helper: add string to pool, dedup identical strings
  auto addString = [&](const std::string& str) -> uint32_t {
    auto it = string_offsets.find(str);
    if (it != string_offsets.end()) {
      return it->second;
    }
    uint32_t offset = static_cast<uint32_t>(string_pool.size());
    string_pool.insert(string_pool.end(), str.begin(), str.end());
    string_offsets[str] = offset;
    return offset;
  };

  for (const auto& ent : entries_) {
    BinaryDictEntry rec{};

    if (ent.surface.empty()) {
      return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Dictionary surface must not be empty"));
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

    if (!isValidExtendedPos(extendedPosToUint8(ent.extended_pos))) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "Dictionary entry has invalid extended POS"));
    }

    rec.surface_offset = addString(ent.surface);
    rec.surface_length = static_cast<uint8_t>(ent.surface.size());

    if (!ent.lemma.empty() && ent.lemma != ent.surface) {
      rec.lemma_offset = addString(ent.lemma);
      rec.lemma_length = static_cast<uint8_t>(ent.lemma.size());
    } else {
      rec.lemma_offset = 0;
      rec.lemma_length = 0;
    }

    rec.pos = posToUint8(ent.pos);
    rec.extended_pos = extendedPosToUint8(ent.extended_pos);

    uint8_t flags = 0;
    if (ent.extended_pos == core::ExtendedPOS::NounFormal) {
      flags |= kFlagFormalNoun;
    }
    if (ent.extended_pos == core::ExtendedPOS::Interjection) {
      flags |= kFlagInterjection;
    }
    if (ent.extended_pos == core::ExtendedPOS::NounProperFamily) {
      flags |= kFlagProperFamily;
    }
    if (ent.extended_pos == core::ExtendedPOS::NounProperGiven) {
      flags |= kFlagProperGiven;
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
    keys.push_back(entries_[idx].surface);
    values.push_back(static_cast<int32_t>(idx));
  }

  DoubleArray trie;
  if (!trie.build(keys, values)) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InternalError, "Failed to build dictionary trie"));
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

core::Expected<size_t, core::Error> BinaryDictWriter::writeToFile(const std::string& path) {
  auto result = build();
  if (!result) {
    return core::makeUnexpected(result.error());
  }

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError, "Failed to create dictionary file: " + path));
  }

  const auto& data = result.value();
  file.write(reinterpret_cast<const char*>(data.data()), data.size());
  if (!file) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InternalError, "Failed to write dictionary file: " + path));
  }

  return data.size();
}

}  // namespace suzume::dictionary
