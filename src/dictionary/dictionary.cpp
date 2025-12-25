#include "dictionary/dictionary.h"

#include <cstdlib>
#include <filesystem>

#include "dictionary/binary_dict.h"
#include "dictionary/core_dict.h"
#include "dictionary/user_dict.h"

namespace suzume::dictionary {

namespace {

/**
 * @brief Get home directory path
 */
std::string getHomeDir() {
  if (const char* home = std::getenv("HOME")) {
    return home;
  }
  return "";
}

}  // namespace

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

  // Lookup in core dictionary (Layer 1: hardcoded)
  auto core_results = core_dict_->lookup(text, start_pos);
  results.insert(results.end(), core_results.begin(), core_results.end());

  // Lookup in core binary dictionary (Layer 2: core.dic)
  if (core_binary_dict_ && core_binary_dict_->isLoaded()) {
    auto binary_results = core_binary_dict_->lookup(text, start_pos);
    results.insert(results.end(), binary_results.begin(), binary_results.end());
  }

  // Lookup in user binary dictionary (Layer 3: user.dic)
  if (user_binary_dict_ && user_binary_dict_->isLoaded()) {
    auto user_binary_results = user_binary_dict_->lookup(text, start_pos);
    results.insert(results.end(), user_binary_results.begin(),
                   user_binary_results.end());
  }

  // Lookup in custom user dictionaries (Layer 4: CSV/TSV files)
  for (const auto& user_dict : user_dicts_) {
    auto user_results = user_dict->lookup(text, start_pos);
    results.insert(results.end(), user_results.begin(), user_results.end());
  }

  return results;
}

const CoreDictionary& DictionaryManager::coreDictionary() const {
  return *core_dict_;
}

bool DictionaryManager::loadCoreDictionary(const std::string& path) {
  if (!core_binary_dict_) {
    core_binary_dict_ = std::make_unique<BinaryDictionary>();
  }

  auto result = core_binary_dict_->loadFromFile(path);
  return result.hasValue();
}

bool DictionaryManager::hasCoreBinaryDictionary() const {
  return core_binary_dict_ && core_binary_dict_->isLoaded();
}

bool DictionaryManager::loadUserBinaryDictionary(const std::string& path) {
  if (!user_binary_dict_) {
    user_binary_dict_ = std::make_unique<BinaryDictionary>();
  }

  auto result = user_binary_dict_->loadFromFile(path);
  return result.hasValue();
}

bool DictionaryManager::hasUserBinaryDictionary() const {
  return user_binary_dict_ && user_binary_dict_->isLoaded();
}

bool DictionaryManager::tryAutoLoadCoreDictionary() {
  namespace fs = std::filesystem;

  // Already loaded
  if (hasCoreBinaryDictionary()) {
    return true;
  }

  std::vector<std::string> search_paths;

  // 1. $SUZUME_DATA_DIR/core.dic
  if (const char* data_dir = std::getenv("SUZUME_DATA_DIR")) {
    search_paths.push_back(std::string(data_dir) + "/core.dic");
  }

  // 2. ./data/core.dic
  search_paths.push_back("./data/core.dic");

  // 3. ~/.suzume/core.dic
  std::string home = getHomeDir();
  if (!home.empty()) {
    search_paths.push_back(home + "/.suzume/core.dic");
  }

  // 4. /usr/local/share/suzume/core.dic
  search_paths.push_back("/usr/local/share/suzume/core.dic");

  // 5. /usr/share/suzume/core.dic
  search_paths.push_back("/usr/share/suzume/core.dic");

  // Try each path
  for (const auto& path : search_paths) {
    if (fs::exists(path) && loadCoreDictionary(path)) {
      return true;
    }
  }

  return false;
}

}  // namespace suzume::dictionary
