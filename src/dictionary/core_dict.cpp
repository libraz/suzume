#include "dictionary/core_dict.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "core/utf8_constants.h"
#include "grammar/conjugation.h"
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

// Conjugation engine for generating dictionary entries (singleton)
const grammar::Conjugation& getConjugation() {
  static grammar::Conjugation conj;
  return conj;
}

// Expand a verb entry into conjugated forms using Conjugation engine
std::vector<DictionaryEntry> expandVerbEntry(const DictionaryEntry& entry) {
  std::vector<DictionaryEntry> result;

  if (entry.conj_type == ConjugationType::None) {
    result.push_back(entry);
    return result;
  }

  grammar::VerbType verb_type = grammar::conjTypeToVerbType(entry.conj_type);
  if (verb_type == grammar::VerbType::Unknown) {
    result.push_back(entry);
    return result;
  }

  std::string stem = grammar::Conjugation::getStem(entry.surface, verb_type);
  std::string lemma = entry.lemma.empty() ? entry.surface : entry.lemma;

  // Special handling for Kuru (kanji vs hiragana)
  if (verb_type == grammar::VerbType::Kuru) {
    bool is_kanji = (entry.surface.find("来") != std::string::npos);
    auto suffixes = getConjugation().getDictionarySuffixes(verb_type);
    for (const auto& suf : suffixes) {
      DictionaryEntry new_entry = entry;
      if (is_kanji) {
        // Replace hiragana stem changes with kanji 来
        std::string surface = suf.suffix;
        // く→来, き→来, こ→来
        if (surface.substr(0, 3) == "く" || surface.substr(0, 3) == "き" ||
            surface.substr(0, 3) == "こ") {
          new_entry.surface = "来" + surface.substr(3);
        } else {
          new_entry.surface = stem + surface;
        }
      } else {
        new_entry.surface = suf.suffix;  // Hiragana: suffix is complete form
      }
      new_entry.lemma = lemma;
      result.push_back(new_entry);
    }
    return result;
  }

  // Special handling for Suru (stem + suffix)
  if (verb_type == grammar::VerbType::Suru) {
    auto suffixes = getConjugation().getDictionarySuffixes(verb_type);
    for (const auto& suf : suffixes) {
      DictionaryEntry new_entry = entry;
      new_entry.surface = stem + suf.suffix;
      new_entry.lemma = lemma;
      result.push_back(new_entry);
    }
    return result;
  }

  // Standard handling for Ichidan and Godan verbs
  auto suffixes = getConjugation().getDictionarySuffixes(verb_type);
  for (const auto& suf : suffixes) {
    DictionaryEntry new_entry = entry;
    new_entry.surface = stem + suf.suffix;
    new_entry.lemma = lemma;
    result.push_back(new_entry);
  }

  return result;
}

// Expand an i-adjective entry into conjugated forms
std::vector<DictionaryEntry> expandAdjectiveEntry(const DictionaryEntry& entry) {
  std::vector<DictionaryEntry> result;

  if (entry.conj_type != ConjugationType::IAdjective) {
    // Not an i-adjective, no expansion needed
    result.push_back(entry);
    return result;
  }

  // Get stem by removing final い
  std::string surface = entry.surface;
  if (surface.size() < core::kJapaneseCharBytes || surface.substr(surface.size() - core::kJapaneseCharBytes) != "い") {
    // Doesn't end with い, return as-is
    result.push_back(entry);
    return result;
  }
  std::string stem = surface.substr(0, surface.size() - core::kJapaneseCharBytes);
  std::string lemma = entry.lemma.empty() ? entry.surface : entry.lemma;

  // I-adjective conjugation suffixes
  // Format: {suffix, pos_override} - pos_override: '\0' = use ADJ, 'N' = use NOUN
  static const std::vector<std::pair<std::string, char>> kIAdjSuffixes = {
      {"い", '\0'},           // Base form
      {"かった", '\0'},       // Past
      {"くない", '\0'},       // Negative
      {"くなかった", '\0'},   // Negative past
      {"くて", '\0'},         // Te-form
      {"ければ", '\0'},       // Conditional
      {"く", '\0'},           // Adverbial
      {"さ", 'N'},            // Nominalization (NOUN: 高さ, 美しさ, etc.)
      {"かったら", '\0'},     // Conditional past
      {"くなる", '\0'},       // Become ~
      {"くなった", '\0'},     // Became ~
      {"そう", '\0'},         // Looks like ~
      {"さそう", '\0'},       // Looks like ~ (for ない/いい: なさそう, よさそう)
  };

  for (const auto& [suffix, pos_override] : kIAdjSuffixes) {
    // Skip さそう for normal adjectives (only valid for ない/いい/よい)
    // Normal adjectives use stem+そう (たかそう), not stem+さそう
    if (suffix == "さそう") {
      // Only generate さそう for single-character stems (な from ない, よ from よい)
      if (stem.size() != core::kJapaneseCharBytes) {
        continue;
      }
    }
    DictionaryEntry new_entry = entry;
    new_entry.surface = stem + suffix;
    new_entry.lemma = lemma;
    // Use NOUN for nominalization forms (高さ, 美しさ, etc.)
    if (pos_override == 'N') {
      new_entry.pos = core::PartOfSpeech::Noun;
    }
    // Lower cost for さそう to beat split (よさ+そう) analysis
    if (suffix == "さそう") {
      new_entry.cost = -0.5F;
    }
    // Preserve conj_type for ChaSen output
    result.push_back(new_entry);
  }

  // Generate contracted forms for adjectives ending in ない (stem ends with な)
  // E.g., くだらない → くだらん, くだらなかった → くだらんかった
  // This handles colloquial contractions: ない→ん, なかった→んかった, etc.
  if (stem.size() >= core::kJapaneseCharBytes &&
      stem.substr(stem.size() - core::kJapaneseCharBytes) == "な") {
    std::string contracted_stem = stem.substr(0, stem.size() - core::kJapaneseCharBytes) + "ん";

    // Contracted form suffixes (parallel to standard forms with な→ん)
    static const std::vector<std::string> kContractedSuffixes = {
        "",           // くだらん (base, from くだらない)
        "かった",     // くだらんかった (past, from くだらなかった)
        "ければ",     // くだらんければ (conditional, from くだらなければ)
        "かったら",   // くだらんかったら (conditional past)
    };

    for (const auto& suffix : kContractedSuffixes) {
      DictionaryEntry new_entry = entry;
      new_entry.surface = contracted_stem + suffix;
      new_entry.lemma = lemma;
      // Slightly higher cost than standard forms (colloquial)
      new_entry.cost = entry.cost + 0.1F;
      result.push_back(new_entry);
    }
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

  // Collect entries (trie built after sorting)
  // Also auto-generates hiragana entries from reading field
  auto addEntries = [this](const std::vector<DictionaryEntry>& source) {
    for (const auto& entry : source) {
      entries_.push_back(entry);

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
          reading_entry.lemma = entry.reading;  // Lemma is the reading itself (MeCab-compatible)
          entries_.push_back(reading_entry);
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

  // Add i-adjective entries with conjugation expansion
  // I-adjectives with ConjugationType::IAdjective will have their conjugated forms auto-generated
  auto addAdjectiveEntries = [this](const std::vector<DictionaryEntry>& source) {
    for (const auto& entry : source) {
      auto expanded = expandAdjectiveEntry(entry);
      for (const auto& exp_entry : expanded) {
        entries_.push_back(exp_entry);

        // Auto-generate hiragana entries from reading for base form only
        // (avoid duplicating conjugated forms for each reading variant)
        if (!entry.reading.empty() && entry.reading != entry.surface &&
            exp_entry.surface == entry.surface) {  // Only for base form
          DictionaryEntry reading_entry = entry;  // Use original entry with conj_type
          reading_entry.surface = entry.reading;
          reading_entry.lemma = entry.reading;  // Lemma is the reading itself (MeCab-compatible)

          // Also expand reading entry (conj_type is preserved from original)
          auto reading_expanded = expandAdjectiveEntry(reading_entry);
          for (const auto& rexp_entry : reading_expanded) {
            entries_.push_back(rexp_entry);
          }
        }
      }
    }
  };

  addAdjectiveEntries(i_adjectives);

  // Add verb entries with conjugation expansion
  // Verbs with ConjugationType set will have their conjugated forms auto-generated
  auto addVerbEntries = [this](const std::vector<DictionaryEntry>& source) {
    for (const auto& entry : source) {
      auto expanded = expandVerbEntry(entry);
      for (const auto& exp_entry : expanded) {
        entries_.push_back(exp_entry);
      }
    }
  };

  addVerbEntries(hiragana_verbs);
  addVerbEntries(essential_verbs);
  addEntries(common_vocabulary);

  // Sort entries by surface for Double-Array compatibility
  // Use stable_sort to preserve relative order of entries with same surface
  std::stable_sort(entries_.begin(), entries_.end(),
                   [](const DictionaryEntry& lhs, const DictionaryEntry& rhs) {
                     return lhs.surface < rhs.surface;
                   });

  // Build Double-Array trie
  buildTrie();
}

void CoreDictionary::buildTrie() {
  if (entries_.empty()) {
    return;
  }

  // Build unique keys with first occurrence index
  std::vector<std::string> keys;
  std::vector<int32_t> values;

  // Entries are sorted by surface, so extract unique keys
  // Store the first index for each surface - lookup() will collect all
  // consecutive entries with the same surface
  std::string prev_surface;
  size_t first_idx = 0;

  for (size_t idx = 0; idx < entries_.size(); ++idx) {
    const auto& entry = entries_[idx];
    if (entry.surface != prev_surface) {
      // New surface - save previous first if exists
      if (!prev_surface.empty()) {
        keys.push_back(prev_surface);
        values.push_back(static_cast<int32_t>(first_idx));
      }
      // Start tracking new surface
      prev_surface = entry.surface;
      first_idx = idx;
    }
    // For same surface, first_idx stays unchanged (first occurrence)
  }
  // Don't forget the last surface
  if (!prev_surface.empty()) {
    keys.push_back(prev_surface);
    values.push_back(static_cast<int32_t>(first_idx));
  }

  // Build the Double-Array trie
  trie_.build(keys, values);
}

namespace {

/**
 * @brief Count UTF-8 characters in a byte range
 */
size_t countUtf8Chars(std::string_view text, size_t start_byte,
                      size_t byte_length) {
  size_t char_count = 0;
  size_t end_byte = start_byte + byte_length;

  for (size_t pos = start_byte; pos < end_byte && pos < text.size();) {
    auto byte = static_cast<uint8_t>(text[pos]);
    if ((byte & 0xC0) != 0x80) {
      ++char_count;
    }
    ++pos;
  }

  return char_count;
}

}  // namespace

std::vector<LookupResult> CoreDictionary::lookup(std::string_view text,
                                                 size_t start_pos) const {
  std::vector<LookupResult> results;

  if (entries_.empty() || start_pos >= text.size()) {
    return results;
  }

  // Use Double-Array common prefix search
  auto trie_results = trie_.commonPrefixSearch(text, start_pos);

  for (const auto& tres : trie_results) {
    if (tres.value < 0 || static_cast<size_t>(tres.value) >= entries_.size()) {
      continue;
    }

    size_t first_idx = static_cast<size_t>(tres.value);
    const std::string& matched_surface = entries_[first_idx].surface;

    // Collect all entries with the same surface (consecutive in sorted array)
    for (size_t idx = first_idx; idx < entries_.size(); ++idx) {
      if (entries_[idx].surface != matched_surface) {
        break;
      }

      LookupResult result{};
      result.entry_id = static_cast<uint32_t>(idx);
      // Convert byte length to character count
      result.length = countUtf8Chars(text, start_pos, tres.length);
      result.entry = &entries_[idx];
      results.push_back(result);
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
