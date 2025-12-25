#include "dictionary/core_dict.h"

#include <string>
#include <vector>

#include "dictionary/entries/adverbs.h"
#include "dictionary/entries/auxiliaries.h"
#include "dictionary/entries/common_vocabulary.h"
#include "dictionary/entries/compound_particles.h"
#include "dictionary/entries/conjunctions.h"
#include "dictionary/entries/determiners.h"
#include "dictionary/entries/essential_verbs.h"
#include "dictionary/entries/formal_nouns.h"
#include "dictionary/entries/greetings.h"
#include "dictionary/entries/hiragana_verbs.h"
#include "dictionary/entries/i_adjectives.h"
#include "dictionary/entries/low_info.h"
#include "dictionary/entries/na_adjectives.h"
#include "dictionary/entries/particles.h"
#include "dictionary/entries/pronouns.h"
#include "dictionary/entries/time_nouns.h"

namespace suzume::dictionary {

namespace {

// =============================================================================
// Verb Conjugation Expansion
// =============================================================================
// Generates common conjugated forms from a base verb entry.
// Only applies to verbs with ConjugationType != None.

struct ConjugatedForm {
  std::string suffix;       // Suffix to add to stem
  std::string base_suffix;  // Base form suffix to remove
};

// Get conjugated forms for Ichidan verbs (食べる → 食べ + suffix)
std::vector<ConjugatedForm> getIchidanForms() {
  return {
      {"る", ""},        // Base: 食べる
      {"ます", ""},      // Polite: 食べます
      {"た", ""},        // Past: 食べた
      {"ました", ""},    // Polite past: 食べました
      {"て", ""},        // Te-form: 食べて
      {"ない", ""},      // Negative: 食べない
      {"ません", ""},    // Polite negative: 食べません
      {"なかった", ""},  // Past negative: 食べなかった
      {"れば", ""},      // Conditional: 食べれば
      {"たら", ""},      // Conditional: 食べたら
      {"よう", ""},      // Volitional: 食べよう
      {"ろ", ""},        // Imperative: 食べろ
  };
}

// Get conjugated forms for Godan-Ra verbs (わかる → わか + suffix)
std::vector<ConjugatedForm> getGodanRaForms() {
  return {
      {"る", ""},        // Base: わかる
      {"り", ""},        // Renyokei: わかり (used in compounds or as noun)
      {"ります", ""},    // Polite: わかります
      {"った", ""},      // Past: わかった
      {"りました", ""},  // Polite past: わかりました
      {"って", ""},      // Te-form: わかって
      {"らない", ""},    // Negative: わからない
      {"りません", ""},  // Polite negative: わかりません
      {"らなかった", ""},// Past negative: わからなかった
      {"れば", ""},      // Conditional: わかれば
      {"ったら", ""},    // Conditional: わかったら
      {"ろう", ""},      // Volitional: わかろう
      {"れ", ""},        // Imperative: わかれ
  };
}

// Get conjugated forms for Godan-Wa verbs (もらう → もら + suffix)
std::vector<ConjugatedForm> getGodanWaForms() {
  return {
      {"う", ""},        // Base: もらう
      {"い", ""},        // Renyokei: もらい (used in compounds or as noun)
      {"います", ""},    // Polite: もらいます
      {"った", ""},      // Past: もらった
      {"いました", ""},  // Polite past: もらいました
      {"って", ""},      // Te-form: もらって
      {"わない", ""},    // Negative: もらわない
      {"いません", ""},  // Polite negative: もらいません
      {"わなかった", ""},// Past negative: もらわなかった
      {"えば", ""},      // Conditional: もらえば
      {"ったら", ""},    // Conditional: もらったら
      {"おう", ""},      // Volitional: もらおう
      {"え", ""},        // Imperative: もらえ
      {"える", ""},      // Potential: もらえる
      {"えます", ""},    // Potential polite: もらえます
      {"えない", ""},    // Potential negative: もらえない
  };
}

// Get conjugated forms for Godan-Sa verbs (いたす → いた + suffix)
std::vector<ConjugatedForm> getGodanSaForms() {
  return {
      {"す", ""},        // Base: いたす
      {"し", ""},        // Renyokei: いたし (used in compounds)
      {"します", ""},    // Polite: いたします
      {"した", ""},      // Past: いたした
      {"しました", ""},  // Polite past: いたしました
      {"して", ""},      // Te-form: いたして
      {"さない", ""},    // Negative: いたさない
      {"しません", ""},  // Polite negative: いたしません
      {"しまして", ""},  // Te-polite: いたしまして
      {"しております", ""},  // Progressive polite: いたしております
      {"しておりました", ""}, // Past progressive polite: いたしておりました
  };
}

// Generate Suru forms (irregular)
std::vector<std::pair<std::string, std::string>> getSuruConjugations(
    const std::string& stem) {
  // stem is empty for する itself
  return {
      {stem + "する", stem + "する"},      // Base
      {stem + "します", stem + "する"},    // Polite
      {stem + "した", stem + "する"},      // Past
      {stem + "しました", stem + "する"},  // Polite past
      {stem + "して", stem + "する"},      // Te-form
      {stem + "しない", stem + "する"},    // Negative
      {stem + "しません", stem + "する"},  // Polite negative
      {stem + "しなかった", stem + "する"},// Past negative
      {stem + "したら", stem + "する"},    // Conditional
      {stem + "すれば", stem + "する"},    // Conditional
      {stem + "しろ", stem + "する"},      // Imperative
      {stem + "しよう", stem + "する"},    // Volitional
      {stem + "している", stem + "する"},  // Progressive
      {stem + "しています", stem + "する"},// Progressive polite
      {stem + "していた", stem + "する"},  // Past progressive
      {stem + "していました", stem + "する"},// Past progressive polite
  };
}

// Get verb stem by removing the base suffix
std::string getVerbStem(const std::string& base_form, ConjugationType type) {
  if (base_form.empty()) return "";

  switch (type) {
    case ConjugationType::Ichidan:
      // Remove る (3 bytes in UTF-8)
      if (base_form.size() >= 3) {
        return base_form.substr(0, base_form.size() - 3);
      }
      break;
    case ConjugationType::GodanRa:
    case ConjugationType::GodanWa:
    case ConjugationType::GodanSa:
    case ConjugationType::GodanKa:
    case ConjugationType::GodanGa:
    case ConjugationType::GodanTa:
    case ConjugationType::GodanMa:
    case ConjugationType::GodanBa:
    case ConjugationType::GodanNa:
      // Remove last hiragana (3 bytes in UTF-8)
      if (base_form.size() >= 3) {
        return base_form.substr(0, base_form.size() - 3);
      }
      break;
    case ConjugationType::Suru:
      // Remove する (6 bytes) or just return as-is for standalone する
      if (base_form == "する") {
        return "";
      }
      if (base_form.size() >= 6 &&
          base_form.substr(base_form.size() - 6) == "する") {
        return base_form.substr(0, base_form.size() - 6);
      }
      break;
    default:
      break;
  }
  return base_form;
}

// Expand a verb entry into conjugated forms
std::vector<DictionaryEntry> expandVerbEntry(const DictionaryEntry& entry) {
  std::vector<DictionaryEntry> result;

  if (entry.conj_type == ConjugationType::None) {
    // No expansion needed
    result.push_back(entry);
    return result;
  }

  std::string stem = getVerbStem(entry.surface, entry.conj_type);
  std::string lemma = entry.lemma.empty() ? entry.surface : entry.lemma;

  std::vector<ConjugatedForm> forms;
  switch (entry.conj_type) {
    case ConjugationType::Ichidan:
      forms = getIchidanForms();
      break;
    case ConjugationType::GodanRa:
      forms = getGodanRaForms();
      break;
    case ConjugationType::GodanWa:
      forms = getGodanWaForms();
      break;
    case ConjugationType::GodanSa:
      forms = getGodanSaForms();
      break;
    case ConjugationType::Suru: {
      // Special handling for suru
      auto suru_forms = getSuruConjugations(stem);
      for (const auto& [surface, base] : suru_forms) {
        DictionaryEntry new_entry = entry;
        new_entry.surface = surface;
        new_entry.lemma = lemma;
        new_entry.conj_type = ConjugationType::None;  // Already expanded
        result.push_back(new_entry);
      }
      return result;
    }
    default:
      // For unsupported types, just add the base entry
      result.push_back(entry);
      return result;
  }

  // Generate conjugated forms
  for (const auto& form : forms) {
    DictionaryEntry new_entry = entry;
    new_entry.surface = stem + form.suffix;
    new_entry.lemma = lemma;
    new_entry.conj_type = ConjugationType::None;  // Already expanded
    result.push_back(new_entry);
  }

  return result;
}

}  // namespace

CoreDictionary::CoreDictionary() { initializeEntries(); }

void CoreDictionary::initializeEntries() {
  // ==========================================================================
  // Layer 1: Hardcoded entries (closed class, required for WASM minimal)
  // ==========================================================================
  // These are grammatically fixed sets with known upper bounds.
  // They rarely change and are tied to language structure.
  auto particles = entries::getParticleEntries();
  auto compound_particles = entries::getCompoundParticleEntries();
  auto auxiliaries = entries::getAuxiliaryEntries();
  auto conjunctions = entries::getConjunctionEntries();
  auto determiners = entries::getDeterminerEntries();
  auto pronouns = entries::getPronounEntries();
  auto formal_nouns = entries::getFormalNounEntries();

  // ==========================================================================
  // WASM Fallback: Layer 2 entries included for WASM minimal builds
  // ==========================================================================
  // These are OPEN CLASS vocabulary that should be in core.dic (Layer 2).
  // They are included here as fallback for WASM builds where core.dic
  // may not be available. Native builds should load core.dic.
  //
  // Canonical data sources:
  //   - data/core/basic.tsv  (time nouns, greetings, suffixes)
  //   - data/core/verbs.tsv  (low info verbs, common verbs)
  //
  // See backup/dictionary_layers.md for the dictionary layer design.
  // TODO(v2): Add compile-time flag SUZUME_WASM_MINIMAL to exclude these
  //           when building for native with core.dic support.
  auto time_nouns = entries::getTimeNounEntries();
  auto low_info = entries::getLowInfoEntries();
  auto greetings = entries::getGreetingEntries();
  auto adverbs = entries::getAdverbEntries();
  auto na_adjectives = entries::getNaAdjectiveEntries();
  auto i_adjectives = entries::getIAdjectiveEntries();
  auto hiragana_verbs = entries::getHiraganaVerbEntries();
  auto essential_verbs = entries::getEssentialVerbEntries();
  auto common_vocabulary = entries::getCommonVocabularyEntries();

  // Reserve space
  entries_.reserve(particles.size() + compound_particles.size() +
                   auxiliaries.size() + conjunctions.size() +
                   determiners.size() + pronouns.size() + formal_nouns.size() +
                   time_nouns.size() + low_info.size() + greetings.size() +
                   adverbs.size() + na_adjectives.size() + i_adjectives.size() +
                   hiragana_verbs.size() + essential_verbs.size() +
                   common_vocabulary.size());

  // Add entries and build trie
  // Also auto-generates hiragana entries from reading field
  auto addEntries = [this](const std::vector<DictionaryEntry>& source) {
    for (const auto& entry : source) {
      auto idx = static_cast<uint32_t>(entries_.size());
      entries_.push_back(entry);
      trie_.insert(entry.surface, idx);

      // Auto-generate hiragana entry from reading if available
      // Only for safe POS types (closed class / function words)
      // Regular nouns are excluded due to homophone ambiguity
      if (!entry.reading.empty() && entry.reading != entry.surface) {
        bool should_expand = false;

        // Safe POS types for hiragana auto-generation
        switch (entry.pos) {
          case core::PartOfSpeech::Adjective:   // 形容詞
          case core::PartOfSpeech::Adverb:      // 副詞 (L1 adverbs are closed class)
          case core::PartOfSpeech::Conjunction: // 接続詞 (closed class)
          case core::PartOfSpeech::Pronoun:     // 代名詞 (closed class)
            should_expand = true;
            break;
          case core::PartOfSpeech::Noun:
            // Only formal nouns (形式名詞) / time nouns are safe
            should_expand = entry.is_formal_noun;
            break;
          default:
            break;
        }

        if (should_expand) {
          DictionaryEntry reading_entry = entry;
          reading_entry.surface = entry.reading;
          reading_entry.lemma = entry.surface;  // Lemma points to kanji form

          auto reading_idx = static_cast<uint32_t>(entries_.size());
          entries_.push_back(reading_entry);
          trie_.insert(reading_entry.surface, reading_idx);
        }
      }
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
  addEntries(greetings);
  addEntries(adverbs);
  addEntries(na_adjectives);
  addEntries(i_adjectives);

  // Add verb entries with conjugation expansion
  // Verbs with ConjugationType set will have their conjugated forms auto-generated
  auto addVerbEntries = [this](const std::vector<DictionaryEntry>& source) {
    for (const auto& entry : source) {
      auto expanded = expandVerbEntry(entry);
      for (const auto& exp_entry : expanded) {
        auto idx = static_cast<uint32_t>(entries_.size());
        entries_.push_back(exp_entry);
        trie_.insert(exp_entry.surface, idx);
      }
    }
  };

  addVerbEntries(hiragana_verbs);
  addVerbEntries(essential_verbs);
  addEntries(common_vocabulary);
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
