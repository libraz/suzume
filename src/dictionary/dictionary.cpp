#include "dictionary/dictionary.h"

#include <array>
#include <iterator>

#include "dictionary/binary_dict.h"
#include "dictionary/core_dict.h"
#include "dictionary/user_dict.h"

namespace suzume::dictionary {

namespace {

// Single source of truth for conjugation-type spellings.
//   canonical  - short SCREAMING_SNAKE used in TSV/CLI output ("" for None)
//   pascal     - PascalCase enum name
//   screaming  - long SCREAMING_SNAKE alias (== canonical except None/INTJ/
//                FAMILY/GIVEN, which spell out NONE/INTERJECTION/PROPER_*)
struct ConjTypeAlias {
  ConjugationType type;
  std::string_view canonical;
  std::string_view pascal;
  std::string_view screaming;
};

constexpr std::array<ConjTypeAlias, 18> kConjTypeAliases = {{
    {ConjugationType::None, "", "None", "NONE"},
    {ConjugationType::Ichidan, "ICHIDAN", "Ichidan", "ICHIDAN"},
    {ConjugationType::GodanKa, "GODAN_KA", "GodanKa", "GODAN_KA"},
    {ConjugationType::GodanGa, "GODAN_GA", "GodanGa", "GODAN_GA"},
    {ConjugationType::GodanSa, "GODAN_SA", "GodanSa", "GODAN_SA"},
    {ConjugationType::GodanTa, "GODAN_TA", "GodanTa", "GODAN_TA"},
    {ConjugationType::GodanNa, "GODAN_NA", "GodanNa", "GODAN_NA"},
    {ConjugationType::GodanBa, "GODAN_BA", "GodanBa", "GODAN_BA"},
    {ConjugationType::GodanMa, "GODAN_MA", "GodanMa", "GODAN_MA"},
    {ConjugationType::GodanRa, "GODAN_RA", "GodanRa", "GODAN_RA"},
    {ConjugationType::GodanWa, "GODAN_WA", "GodanWa", "GODAN_WA"},
    {ConjugationType::Suru, "SURU", "Suru", "SURU"},
    {ConjugationType::Kuru, "KURU", "Kuru", "KURU"},
    {ConjugationType::IAdjective, "I_ADJ", "IAdjective", "I_ADJ"},
    {ConjugationType::NaAdjective, "NA_ADJ", "NaAdjective", "NA_ADJ"},
    {ConjugationType::Interjection, "INTJ", "Interjection", "INTERJECTION"},
    {ConjugationType::ProperFamily, "FAMILY", "ProperFamily", "PROPER_FAMILY"},
    {ConjugationType::ProperGiven, "GIVEN", "ProperGiven", "PROPER_GIVEN"},
}};

void appendLookupResults(std::vector<LookupResult>& destination, std::vector<LookupResult>&& source,
                         bool from_user_dict = false) {
  if (from_user_dict) {
    for (auto& result : source) {
      result.from_user_dict = true;
    }
  }
  destination.insert(destination.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
}

}  // namespace

std::string_view conjTypeToCanonicalString(ConjugationType type) {
  for (const auto& alias : kConjTypeAliases) {
    if (alias.type == type) {
      return alias.canonical;
    }
  }
  return "";
}

std::optional<ConjugationType> conjTypeFromCanonical(std::string_view str) {
  if (str.empty() || str == "NONE") {
    return ConjugationType::None;
  }
  for (const auto& alias : kConjTypeAliases) {
    if (!alias.canonical.empty() && alias.canonical == str) {
      return alias.type;
    }
  }
  return std::nullopt;
}

std::optional<ConjugationType> conjTypeFromAnyAlias(std::string_view str) {
  for (const auto& alias : kConjTypeAliases) {
    if (alias.pascal == str || alias.screaming == str) {
      return alias.type;
    }
  }
  return std::nullopt;
}

DictionaryManager::DictionaryManager() : core_dict_(std::make_unique<CoreDictionary>()) {}

DictionaryManager::~DictionaryManager() = default;

DictionaryManager::DictionaryManager(DictionaryManager&&) noexcept = default;
DictionaryManager& DictionaryManager::operator=(DictionaryManager&&) noexcept = default;

void DictionaryManager::addUserDictionary(std::shared_ptr<UserDictionary> dict) {
  if (dict) {
    user_dicts_.push_back(std::move(dict));
  }
}

void DictionaryManager::clearUserDictionaries() {
  user_binary_dicts_.clear();
  user_dicts_.clear();
}

std::vector<LookupResult> DictionaryManager::lookup(std::string_view text, size_t start_pos) const {
  std::vector<LookupResult> results;
  lookupInto(text, start_pos, results);
  return results;
}

void DictionaryManager::lookupInto(std::string_view text, size_t start_pos, std::vector<LookupResult>& out) const {
  out.clear();

  // Lookup in core dictionary (Layer 1: hardcoded)
  appendLookupResults(out, core_dict_->lookup(text, start_pos));

  // Lookup in core binary dictionary (Layer 2: core.dic)
  if (core_binary_dict_ && core_binary_dict_->isLoaded()) {
    appendLookupResults(out, core_binary_dict_->lookup(text, start_pos));
  }

  // Lookup in the automatically loaded bundled user dictionary (Layer 3)
  for (const auto& user_binary_dict : bundled_user_binary_dicts_) {
    appendLookupResults(out, user_binary_dict->lookup(text, start_pos), true);
  }

  // Lookup in caller-loaded binary user dictionaries (Layer 4)
  for (const auto& user_binary_dict : user_binary_dicts_) {
    appendLookupResults(out, user_binary_dict->lookup(text, start_pos), true);
  }

  // Lookup in caller-loaded source user dictionaries (Layer 5: CSV/TSV files)
  for (const auto& user_dict : user_dicts_) {
    appendLookupResults(out, user_dict->lookup(text, start_pos), true);
  }
}

const DictionaryEntry* DictionaryManager::lookupExact(std::string_view surface, core::PartOfSpeech pos) const {
  if (surface.empty()) {
    return nullptr;
  }

  if (const auto* entry = core_dict_->lookupExact(surface, pos)) {
    return entry;
  }
  if (core_binary_dict_ && core_binary_dict_->isLoaded()) {
    if (const auto* entry = core_binary_dict_->lookupExact(surface, pos)) {
      return entry;
    }
  }
  for (const auto& user_binary_dict : bundled_user_binary_dicts_) {
    if (const auto* entry = user_binary_dict->lookupExact(surface, pos)) {
      return entry;
    }
  }
  for (const auto& user_binary_dict : user_binary_dicts_) {
    if (const auto* entry = user_binary_dict->lookupExact(surface, pos)) {
      return entry;
    }
  }
  for (const auto& user_dict : user_dicts_) {
    if (const auto* entry = user_dict->lookupExact(surface, pos)) {
      return entry;
    }
  }
  return nullptr;
}

bool DictionaryManager::loadCoreDictionary(const std::string& path) {
  return loadCoreDictionaryResult(path).hasValue();
}

BinaryDictionary& DictionaryManager::ensureCoreBinaryDict() {
  if (!core_binary_dict_) {
    core_binary_dict_ = std::make_unique<BinaryDictionary>();
  }
  return *core_binary_dict_;
}

core::Expected<size_t, core::Error> DictionaryManager::loadCoreDictionaryResult(const std::string& path) {
  return ensureCoreBinaryDict().loadFromFile(path);
}

core::Expected<size_t, core::Error> DictionaryManager::loadCoreDictionaryFromMemoryResult(const uint8_t* data,
                                                                                          size_t size) {
  return ensureCoreBinaryDict().loadFromMemory(data, size);
}

bool DictionaryManager::hasCoreBinaryDictionary() const {
  return core_binary_dict_ && core_binary_dict_->isLoaded();
}

namespace {

// A dictionary only joins the list once its own load succeeded, so a rejected
// file or buffer leaves the manager exactly as it was.
template <typename Load>
core::Expected<size_t, core::Error> appendLoadedDictionary(std::vector<std::unique_ptr<BinaryDictionary>>& dictionaries,
                                                           Load load) {
  auto dictionary = std::make_unique<BinaryDictionary>();
  auto result = load(*dictionary);
  if (!result.hasValue()) {
    return core::makeUnexpected(result.error());
  }
  dictionaries.push_back(std::move(dictionary));
  return result.value();
}

}  // namespace

core::Expected<size_t, core::Error> DictionaryManager::loadUserBinaryDictionaryResultInto(
    const std::string& path, std::vector<std::unique_ptr<BinaryDictionary>>& dictionaries) {
  return appendLoadedDictionary(dictionaries,
                                [&path](BinaryDictionary& dictionary) { return dictionary.loadFromFile(path); });
}

bool DictionaryManager::loadUserBinaryDictionaryFromMemory(const uint8_t* data, size_t size) {
  return loadUserBinaryDictionaryFromMemoryResult(data, size).hasValue();
}

core::Expected<size_t, core::Error> DictionaryManager::loadUserBinaryDictionaryFromMemoryResult(const uint8_t* data,
                                                                                                size_t size) {
  return loadUserBinaryDictionaryFromMemoryResultInto(data, size, user_binary_dicts_);
}

core::Expected<size_t, core::Error> DictionaryManager::loadUserBinaryDictionaryFromMemoryResultInto(
    const uint8_t* data, size_t size, std::vector<std::unique_ptr<BinaryDictionary>>& dictionaries) {
  return appendLoadedDictionary(
      dictionaries, [data, size](BinaryDictionary& dictionary) { return dictionary.loadFromMemory(data, size); });
}

core::Expected<size_t, core::Error> DictionaryManager::loadBundledUserBinaryDictionaryResult(const std::string& path) {
  return loadUserBinaryDictionaryResultInto(path, bundled_user_binary_dicts_);
}

core::Expected<size_t, core::Error> DictionaryManager::loadBundledUserBinaryDictionaryFromMemoryResult(
    const uint8_t* data, size_t size) {
  return loadUserBinaryDictionaryFromMemoryResultInto(data, size, bundled_user_binary_dicts_);
}

bool DictionaryManager::hasUserBinaryDictionary() const {
  return !bundled_user_binary_dicts_.empty() || !user_binary_dicts_.empty();
}

}  // namespace suzume::dictionary
