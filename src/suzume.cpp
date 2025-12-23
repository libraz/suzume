#include "suzume.h"

#include <cstdlib>
#include <filesystem>
#include <vector>

#include "analysis/analyzer.h"
#include "dictionary/user_dict.h"
#include "postprocess/postprocessor.h"
#include "postprocess/tag_generator.h"

namespace suzume {

namespace {

namespace fs = std::filesystem;

/**
 * @brief Get dictionary search paths in priority order
 */
std::vector<fs::path> getDictSearchPaths() {
  std::vector<fs::path> paths;

  // 1. Environment variable $SUZUME_DATA_DIR
  if (const char* env_path = std::getenv("SUZUME_DATA_DIR")) {
    paths.emplace_back(env_path);
  }

  // 2. Current directory ./data/
  paths.emplace_back("./data");

  // 3. User directory ~/.suzume/
  if (const char* home = std::getenv("HOME")) {
    paths.emplace_back(fs::path(home) / ".suzume");
  }

  // 4. System directories
  paths.emplace_back("/usr/local/share/suzume");
  paths.emplace_back("/usr/share/suzume");

  return paths;
}

/**
 * @brief Find dictionary file in search paths
 * @param filename Dictionary filename (e.g., "core.dic")
 * @return Full path if found, empty string otherwise
 */
std::string findDictionary(const std::string& filename) {
  for (const auto& dir : getDictSearchPaths()) {
    fs::path dict_path = dir / filename;
    if (fs::exists(dict_path) && fs::is_regular_file(dict_path)) {
      return dict_path.string();
    }
  }
  return "";
}

}  // namespace

struct Suzume::Impl {
  SuzumeOptions options;
  analysis::Analyzer analyzer;
  postprocess::Postprocessor postprocessor;
  postprocess::TagGenerator tag_generator;
  std::shared_ptr<dictionary::UserDictionary> core_dict;
  std::shared_ptr<dictionary::UserDictionary> user_dict;

  Impl(const SuzumeOptions& opts)
      : options(opts),
        analyzer(analysis::AnalyzerOptions{opts.mode, {}, {}}),
        postprocessor(&analyzer.dictionaryManager()),
        tag_generator(opts.tag_options) {
    // Auto-load core.dic if found
    std::string core_path = findDictionary("core.dic");
    if (!core_path.empty()) {
      core_dict = std::make_shared<dictionary::UserDictionary>();
      if (core_dict->loadFromFile(core_path).hasValue()) {
        analyzer.addUserDictionary(core_dict);
      }
    }

    // Auto-load user.dic if found
    std::string user_path = findDictionary("user.dic");
    if (!user_path.empty()) {
      user_dict = std::make_shared<dictionary::UserDictionary>();
      if (user_dict->loadFromFile(user_path).hasValue()) {
        analyzer.addUserDictionary(user_dict);
      }
    }
  }
};

Suzume::Suzume() : Suzume(SuzumeOptions{}) {}

Suzume::Suzume(const SuzumeOptions& options)
    : impl_(std::make_unique<Impl>(options)) {}

Suzume::~Suzume() = default;

Suzume::Suzume(Suzume&&) noexcept = default;

Suzume& Suzume::operator=(Suzume&&) noexcept = default;

bool Suzume::loadUserDictionary(const std::string& path) {
  auto dict = std::make_shared<dictionary::UserDictionary>();
  auto result = dict->loadFromFile(path);
  if (result.hasValue()) {
    impl_->user_dict = dict;
    impl_->analyzer.addUserDictionary(dict);
    return true;
  }
  return false;
}

bool Suzume::loadUserDictionaryFromMemory(const char* data, size_t size) {
  auto dict = std::make_shared<dictionary::UserDictionary>();
  auto result = dict->loadFromMemory(data, size);
  if (result.hasValue()) {
    impl_->user_dict = dict;
    impl_->analyzer.addUserDictionary(dict);
    return true;
  }
  return false;
}

std::vector<core::Morpheme> Suzume::analyze(std::string_view text) const {
  auto morphemes = impl_->analyzer.analyze(text);
  return impl_->postprocessor.process(morphemes);
}

std::vector<std::string> Suzume::generateTags(std::string_view text) const {
  auto morphemes = impl_->analyzer.analyze(text);
  return impl_->tag_generator.generate(morphemes);
}

core::AnalysisMode Suzume::mode() const { return impl_->options.mode; }

void Suzume::setMode(core::AnalysisMode mode) {
  impl_->options.mode = mode;
  impl_->analyzer.setMode(mode);
}

std::string Suzume::version() { return "0.1.0"; }

}  // namespace suzume
