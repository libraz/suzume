#include "dictionary/user_dict.h"

#ifndef __EMSCRIPTEN__
#include <fstream>
#include <iterator>
#endif

#include "core/types.h"
#include "dictionary/dictionary_lookup.h"
#include "dictionary/source_parser.h"
#include "normalize/utf8.h"

namespace suzume::dictionary {

namespace {

constexpr size_t kMaxUserDictionarySurfaceBytes = 255;

}  // namespace

UserDictionary::UserDictionary() = default;

core::Expected<size_t, core::Error> UserDictionary::loadFromFile(const std::string& path) {
#ifdef __EMSCRIPTEN__
  (void)path;
  return core::makeUnexpected(
      core::Error(core::ErrorCode::InvalidInput, "File dictionary loading is unavailable in WebAssembly"));
#else
  std::ifstream file(path);
  if (!file.is_open()) {
    return core::Error(core::ErrorCode::FileNotFound, "Failed to open dictionary file: " + path);
  }

  std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  return loadFromMemory(content.c_str(), content.size());
#endif
}

core::Expected<size_t, core::Error> UserDictionary::loadFromMemory(const char* data, size_t size) {
  if (data == nullptr || size == 0) {
    return core::Error(core::ErrorCode::InvalidInput, "Empty dictionary data");
  }

  return parseSource(std::string_view(data, size));
}

bool UserDictionary::addEntry(const DictionaryEntry& entry) {
  if (entry.surface.empty() || !core::isValidPartOfSpeech(entry.pos) || !core::isValidExtendedPos(entry.extended_pos) ||
      entry.surface.size() > kMaxUserDictionarySurfaceBytes || !normalize::isValidUtf8(entry.surface) ||
      (!entry.lemma.empty() && !normalize::isValidUtf8(entry.lemma))) {
    return false;
  }

  auto idx = static_cast<uint32_t>(entries_.size());
  entries_.push_back(entry);
  trie_.insert(entry.surface, idx);
  return true;
}

std::vector<LookupResult> UserDictionary::lookup(std::string_view text, size_t start_pos) const {
  std::vector<LookupResult> results;

  auto matches = trie_.prefixMatch(text, start_pos);
  for (const auto& [length, entry_ids] : matches) {
    for (uint32_t idx : *entry_ids) {
      if (idx < entries_.size()) {
        LookupResult result{};
        result.entry_id = idx;
        result.length = length;
        result.entry = &entries_[idx];
        results.push_back(result);
      }
    }
  }

  return results;
}

const DictionaryEntry* UserDictionary::lookupExact(std::string_view surface, core::PartOfSpeech pos) const {
  const auto* entry_ids = trie_.lookupView(surface);
  if (entry_ids == nullptr) {
    return nullptr;
  }
  for (uint32_t idx : *entry_ids) {
    if (idx < entries_.size() && (pos == core::PartOfSpeech::Unknown || entries_[idx].pos == pos)) {
      return &entries_[idx];
    }
  }
  return nullptr;
}

const DictionaryEntry* UserDictionary::getEntry(uint32_t idx) const {
  return entryAt(entries_, idx);
}

void UserDictionary::clear() {
  entries_.clear();
  trie_.clear();
}

core::Expected<size_t, core::Error> UserDictionary::parseSource(std::string_view source_data) {
  std::vector<DictionaryEntry> parsed_entries;
  SourceParseOptions options;
  options.skip_single_field_records = true;
  auto parsed = parseDictionarySource(source_data, options);
  if (!parsed.hasValue()) {
    return core::makeUnexpected(parsed.error());
  }
  parsed_entries.reserve(parsed.value().entries.size());
  for (const auto& source_entry : parsed.value().entries) {
    auto entry = sourceToDictionaryEntry(source_entry);
    if (entry.surface.size() > kMaxUserDictionarySurfaceBytes) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput,
                      "Dictionary surface exceeds " + core::decimalDigits(kMaxUserDictionarySurfaceBytes) + " bytes"));
    }
    parsed_entries.push_back(std::move(entry));
  }

  size_t installed_count = 0;
  entries_.reserve(entries_.size() + parsed_entries.size());
  for (auto& entry : parsed_entries) {
    installed_count += addEntry(entry) ? 1U : 0U;
  }

  if (installed_count == 0) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError, "Dictionary source contains no loadable entries"));
  }
  return installed_count;
}

}  // namespace suzume::dictionary
