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
  std::shared_ptr<dictionary::UserDictionary> custom_dict;
  std::vector<std::string> dictionary_warnings;

  static analysis::ScorerOptions loadScorerConfig(const SuzumeOptions& opts) {
    analysis::ScorerOptions scorer_opts = opts.scorer_options;
#ifndef __EMSCRIPTEN__
    // Load from environment variables (SUZUME_SCORER_CONFIG and SUZUME_SCORER_*)
    auto result = analysis::ScorerOptionsLoader::loadFromEnv(scorer_opts, opts.report_scorer_config);

    // Output status message when config is active (debug feature)
    if (opts.report_scorer_config && result.hasConfig()) {
      std::cerr << "[scorer-config] ";
      if (!result.config_path.empty()) {
        std::cerr << "json=" << result.config_path;
        if (result.env_override_count > 0) {
          std::cerr << ", ";
        }
      }
      if (result.env_override_count > 0) {
        std::cerr << "env_overrides=" << result.env_override_count;
      }
      std::cerr << "\n";
    }
#endif
    return scorer_opts;
  }

  static postprocess::PostprocessOptions postprocessOptionsFor(const SuzumeOptions& opts) {
    bool merge_noun_compounds = opts.merge_compounds || opts.mode == core::AnalysisMode::Search;
    if (opts.mode == core::AnalysisMode::Split) {
      merge_noun_compounds = false;
    }
    return postprocess::PostprocessOptions{merge_noun_compounds, opts.lemmatize, opts.remove_symbols};
  }

  void warnDictionaryLoad(const std::string& path, const core::Error& error) {
    std::string message = "Failed to auto-load dictionary " + path + ": " + error.message;
    dictionary_warnings.push_back(message);
    if (options.report_scorer_config) {
      std::cerr << "[dictionary] " << message << "\n";
    }
  }

  Impl(const SuzumeOptions& opts)
      : options(opts),
        analyzer(analysis::AnalyzerOptions{opts.mode, loadScorerConfig(opts), {}, opts.normalize_options}),
        postprocessor(&analyzer.dictionaryManager(), postprocessOptionsFor(opts)) {
    // Auto-load core.dic if found (binary format)
    std::string core_path = findDictionary("core.dic");
    if (!core_path.empty()) {
      auto result = analyzer.dictionaryManager().loadCoreDictionaryResult(core_path);
      if (!result.hasValue()) {
        warnDictionaryLoad(core_path, result.error());
      }
    }

    // Auto-load user.dic if found (binary format)
    // Note: user.dic is also loaded as core binary dictionary for now
    if (!opts.skip_user_dictionary) {
      std::string user_path = findDictionary("user.dic");
      if (!user_path.empty()) {
        auto result = analyzer.dictionaryManager().loadUserBinaryDictionaryResult(user_path);
        if (!result.hasValue()) {
          warnDictionaryLoad(user_path, result.error());
        }
      }
    }
  }

  void setMode(core::AnalysisMode mode) {
    options.mode = mode;
    analyzer.setMode(mode);
    postprocessor.setOptions(postprocessOptionsFor(options));
  }
};

Suzume::Suzume() : Suzume(SuzumeOptions{}) {}

Suzume::Suzume(const SuzumeOptions& options) : impl_(std::make_unique<Impl>(options)) {}

Suzume::~Suzume() = default;

Suzume::Suzume(Suzume&&) noexcept = default;

Suzume& Suzume::operator=(Suzume&&) noexcept = default;

bool Suzume::loadUserDictionary(const std::string& path) {
  return loadUserDictionaryResult(path).hasValue();
}

core::Expected<size_t, core::Error> Suzume::loadUserDictionaryResult(const std::string& path) {
  // For CSV/TSV custom dictionaries
  auto dict = std::make_shared<dictionary::UserDictionary>();
  auto result = dict->loadFromFile(path);
  if (result.hasValue()) {
    impl_->custom_dict = dict;
    impl_->analyzer.addUserDictionary(dict);
    return result.value();
  }
  return result.error();
}

bool Suzume::loadUserDictionaryFromMemory(const char* data, size_t size) {
  return loadUserDictionaryFromMemoryResult(data, size).hasValue();
}

core::Expected<size_t, core::Error> Suzume::loadUserDictionaryFromMemoryResult(const char* data, size_t size) {
  // For CSV/TSV custom dictionaries
  auto dict = std::make_shared<dictionary::UserDictionary>();
  auto result = dict->loadFromMemory(data, size);
  if (result.hasValue()) {
    impl_->custom_dict = dict;
    impl_->analyzer.addUserDictionary(dict);
    return result.value();
  }
  return result.error();
}

bool Suzume::loadBinaryDictionary(const uint8_t* data, size_t size) {
  return loadBinaryDictionaryResult(data, size).hasValue();
}

core::Expected<size_t, core::Error> Suzume::loadBinaryDictionaryResult(const uint8_t* data, size_t size) {
  return impl_->analyzer.dictionaryManager().loadUserBinaryDictionaryFromMemoryResult(data, size);
}

std::vector<std::string> Suzume::dictionaryWarnings() const {
  return impl_->dictionary_warnings;
}

std::vector<core::Morpheme> Suzume::analyze(std::string_view text) const {
  auto morphemes = impl_->analyzer.analyze(text);
  return impl_->postprocessor.process(morphemes);
}

std::vector<core::Morpheme> Suzume::analyzeDebug(std::string_view text, core::Lattice* out_lattice) const {
  auto morphemes = impl_->analyzer.analyzeDebug(text, out_lattice);
  return impl_->postprocessor.process(morphemes);
}

std::vector<postprocess::TagEntry> Suzume::generateTags(std::string_view text) const {
  auto morphemes = impl_->analyzer.analyze(text);
  auto processed = impl_->postprocessor.process(morphemes);
  postprocess::TagGenerator generator(impl_->options.tag_options);
  return generator.generate(processed);
}

std::vector<postprocess::TagEntry> Suzume::generateTags(std::string_view text,
                                                        const postprocess::TagGeneratorOptions& options) const {
  auto morphemes = impl_->analyzer.analyze(text);
  auto processed = impl_->postprocessor.process(morphemes);
  postprocess::TagGenerator generator(options);
  return generator.generate(processed);
}

core::AnalysisMode Suzume::mode() const {
  return impl_->options.mode;
}

void Suzume::setMode(core::AnalysisMode mode) {
  impl_->setMode(mode);
}

std::string Suzume::version() {
  return SUZUME_VERSION;
}

}  // namespace suzume
