#include "dictionary/dictionary.h"

#include "dictionary/core_dict.h"
#include "dictionary/user_dict.h"

namespace suzume::dictionary {

DictionaryManager::DictionaryManager()
    : core_dict_(std::make_unique<CoreDictionary>()) {}

DictionaryManager::~DictionaryManager() = default;

DictionaryManager::DictionaryManager(DictionaryManager&&) noexcept = default;
DictionaryManager& DictionaryManager::operator=(DictionaryManager&&) noexcept = default;

void DictionaryManager::addUserDictionary(
    std::shared_ptr<UserDictionary> dict) {
  if (dict) {
    user_dicts_.push_back(std::move(dict));
  }
}

std::vector<LookupResult> DictionaryManager::lookup(std::string_view text,
                                                    size_t start_pos) const {
  std::vector<LookupResult> results;

  // Lookup in core dictionary
  auto core_results = core_dict_->lookup(text, start_pos);
  results.insert(results.end(), core_results.begin(), core_results.end());

  // Lookup in user dictionaries
  for (const auto& user_dict : user_dicts_) {
    auto user_results = user_dict->lookup(text, start_pos);
    results.insert(results.end(), user_results.begin(), user_results.end());
  }

  return results;
}

const CoreDictionary& DictionaryManager::coreDictionary() const {
  return *core_dict_;
}

}  // namespace dictionary
