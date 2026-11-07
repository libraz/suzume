#include "dictionary/user_dict.h"

#include "core/types.h"
#include "normalize/utf8.h"

namespace suzume::dictionary {

namespace {

constexpr size_t kMaxUserDictionarySurfaceBytes = 255;

}  // namespace

UserDictionary::UserDictionary() = default;

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

}  // namespace suzume::dictionary
