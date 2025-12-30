#include "suzume.h"

#include <cstdlib>
#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif
#include <vector>

#include "analysis/analyzer.h"
#ifndef __EMSCRIPTEN__
#include "analysis/scorer_options_loader.h"
#endif
#include "dictionary/binary_dict.h"
#include "dictionary/user_dict.h"
#include "postprocess/postprocessor.h"
#include "postprocess/tag_generator.h"

namespace suzume {

namespace {

#ifdef __EMSCRIPTEN__
// WASM: Use embedded dictionary paths directly (Emscripten VFS)
std::string findDictionary(const std::string& filename) {
  return "/data/" + filename;
}
#else
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
#endif  // __EMSCRIPTEN__

}  // namespace

struct Suzume::Impl {
  SuzumeOptions options;
  analysis::Analyzer analyzer;
  postprocess::Postprocessor postprocessor;
  postprocess::TagGenerator tag_generator;
  std::shared_ptr<dictionary::UserDictionary> custom_dict;

  static analysis::ScorerOptions loadScorerConfig(const SuzumeOptions& opts) {
    analysis::ScorerOptions scorer_opts = opts.scorer_options;
#ifndef __EMSCRIPTEN__
    // File-based config loading is only available in native builds
    if (!opts.scorer_config_path.empty()) {
      std::string error_msg;
      if (!analysis::ScorerOptionsLoader::loadFromFile(opts.scorer_config_path, scorer_opts, &error_msg)) {
        std::cerr << "warning: Failed to load scorer config: " << error_msg << "\n";
      }
    }
#endif
    return scorer_opts;
  }

  Impl(const SuzumeOptions& opts)
      : options(opts),
        analyzer(analysis::AnalyzerOptions{opts.mode, loadScorerConfig(opts), {}, opts.normalize_options}),
        postprocessor(&analyzer.dictionaryManager(),
                      postprocess::PostprocessOptions{
                          .merge_noun_compounds = opts.merge_compounds,
                          .lemmatize = opts.lemmatize,
                          .remove_symbols = opts.remove_symbols}),
        tag_generator(opts.tag_options) {
    // Auto-load core.dic if found (binary format)
    std::string core_path = findDictionary("core.dic");
    if (!core_path.empty()) {
      analyzer.dictionaryManager().loadCoreDictionary(core_path);
    }

    // Auto-load user.dic if found (binary format)
    // Note: user.dic is also loaded as core binary dictionary for now
    std::string user_path = findDictionary("user.dic");
    if (!user_path.empty()) {
      analyzer.dictionaryManager().loadUserBinaryDictionary(user_path);
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
  // For CSV/TSV custom dictionaries
  auto dict = std::make_shared<dictionary::UserDictionary>();
  auto result = dict->loadFromFile(path);
  if (result.hasValue()) {
    impl_->custom_dict = dict;
    impl_->analyzer.addUserDictionary(dict);
    return true;
  }
  return false;
}

bool Suzume::loadUserDictionaryFromMemory(const char* data, size_t size) {
  // For CSV/TSV custom dictionaries
  auto dict = std::make_shared<dictionary::UserDictionary>();
  auto result = dict->loadFromMemory(data, size);
  if (result.hasValue()) {
    impl_->custom_dict = dict;
    impl_->analyzer.addUserDictionary(dict);
    return true;
  }
  return false;
}

std::vector<core::Morpheme> Suzume::analyze(std::string_view text) const {
  auto morphemes = impl_->analyzer.analyze(text);
  return impl_->postprocessor.process(morphemes);
}

std::vector<core::Morpheme> Suzume::analyzeDebug(std::string_view text,
                                                  core::Lattice* out_lattice) const {
  auto morphemes = impl_->analyzer.analyzeDebug(text, out_lattice);
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
