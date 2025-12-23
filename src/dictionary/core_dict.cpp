#include "dictionary/core_dict.h"

#include "dictionary/entries/auxiliaries.h"
#include "dictionary/entries/compound_particles.h"
#include "dictionary/entries/conjunctions.h"
#include "dictionary/entries/determiners.h"
#include "dictionary/entries/formal_nouns.h"
#include "dictionary/entries/low_info.h"
#include "dictionary/entries/particles.h"
#include "dictionary/entries/pronouns.h"
#include "dictionary/entries/time_nouns.h"

namespace suzume::dictionary {

CoreDictionary::CoreDictionary() { initializeEntries(); }

void CoreDictionary::initializeEntries() {
  // Add all entry types
  auto particles = entries::getParticleEntries();
  auto compound_particles = entries::getCompoundParticleEntries();
  auto auxiliaries = entries::getAuxiliaryEntries();
  auto conjunctions = entries::getConjunctionEntries();
  auto determiners = entries::getDeterminerEntries();
  auto pronouns = entries::getPronounEntries();
  auto formal_nouns = entries::getFormalNounEntries();
  // TODO(v2): Time nouns should be loaded from core.dic (Layer 2), not hardcoded.
  // This is a WASM fallback. Canonical data is in data/core/basic.tsv.
  // See backup/dictionary_layers.md for the dictionary layer design.
  auto time_nouns = entries::getTimeNounEntries();
  auto low_info = entries::getLowInfoEntries();

  // Reserve space
  entries_.reserve(particles.size() + compound_particles.size() +
                   auxiliaries.size() + conjunctions.size() +
                   determiners.size() + pronouns.size() + formal_nouns.size() +
                   time_nouns.size() + low_info.size());

  // Add entries and build trie
  auto addEntries = [this](const std::vector<DictionaryEntry>& source) {
    for (const auto& entry : source) {
      auto idx = static_cast<uint32_t>(entries_.size());
      entries_.push_back(entry);
      trie_.insert(entry.surface, idx);
    }
  };

  addEntries(particles);
  addEntries(compound_particles);
  addEntries(auxiliaries);
  addEntries(conjunctions);
  addEntries(determiners);
  addEntries(pronouns);
  addEntries(formal_nouns);
  addEntries(time_nouns);
  addEntries(low_info);
}

std::vector<LookupResult> CoreDictionary::lookup(std::string_view text,
                                                 size_t start_pos) const {
  std::vector<LookupResult> results;

  auto matches = trie_.prefixMatch(text, start_pos);
  for (const auto& [length, entry_ids] : matches) {
    for (uint32_t idx : entry_ids) {
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

const DictionaryEntry* CoreDictionary::getEntry(uint32_t idx) const {
  if (idx < entries_.size()) {
    return &entries_[idx];
  }
  return nullptr;
}

}  // namespace dictionary
