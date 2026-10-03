#include "suzume.h"

#include <cstdlib>
#include <iterator>
#ifndef __EMSCRIPTEN__
#include <fstream>
#endif
#include <utility>
#include <vector>

// The embedded-dictionary path carries the compiled dictionaries in the binary
// and touches neither the filesystem nor environment variables. It is taken for
// WASM and for native builds configured with -DSUZUME_EMBED_DICT=ON.
#if defined(__EMSCRIPTEN__) || defined(SUZUME_EMBED_DICT)
#define SUZUME_USE_EMBEDDED_DICT 1
#endif

#ifndef SUZUME_USE_EMBEDDED_DICT
#include <filesystem>
#include <iostream>
#endif

#include "analysis/analyzer.h"
#include "analysis/scorer_options_loader.h"
#include "dictionary/binary_dict.h"
#include "dictionary/source_parser.h"
#include "dictionary/user_dict.h"
#include "grammar/dictionary_expansion.h"
#ifdef SUZUME_USE_EMBEDDED_DICT
#include "embedded_dictionaries.h"
#endif
#include "postprocess/postprocessor.h"
#include "postprocess/tag_generator.h"

namespace suzume {

namespace {

struct LoadedSourceDictionary {
  std::shared_ptr<dictionary::UserDictionary> dictionary;
  size_t installed_entry_count;
  std::vector<std::string> warnings;
};

core::Expected<LoadedSourceDictionary, core::Error> loadSourceDictionaryFromMemory(const char* data, size_t size) {
  if (data == nullptr || size == 0) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "Empty dictionary data"));
  }

  dictionary::SourceParseOptions parse_options;
  parse_options.skip_single_field_records = true;
  auto parsed = dictionary::parseDictionarySource(std::string_view(data, size), parse_options);
  if (!parsed.hasValue()) {
    return core::makeUnexpected(parsed.error());
  }
  if (parsed.value().entries.empty()) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError, "Dictionary source contains no loadable entries"));
  }

  auto expanded = grammar::expandDictionarySourceEntries(parsed.value().entries);
  auto user_dictionary = std::make_shared<dictionary::UserDictionary>();
  size_t installed_entry_count = 0;
  for (const auto& entry : expanded.entries) {
    installed_entry_count += user_dictionary->addEntry(entry) ? 1U : 0U;
  }
  if (installed_entry_count == 0) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::ParseError, "Dictionary source contains no valid UTF-8 entries"));
  }
  std::vector<std::string> warnings = std::move(parsed.value().warnings);
  if (installed_entry_count < expanded.entries.size()) {
    warnings.push_back("Skipped " + core::decimalDigits(expanded.entries.size() - installed_entry_count) +
                       " invalid dictionary entries");
  }
  if (expanded.duplicates_skipped > 0) {
    warnings.push_back("Skipped " + core::decimalDigits(expanded.duplicates_skipped) +
                       " duplicate expanded dictionary entries");
  }
  return LoadedSourceDictionary{std::move(user_dictionary), installed_entry_count, std::move(warnings)};
}

#ifndef SUZUME_USE_EMBEDDED_DICT
namespace fs = std::filesystem;

/**
 * @brief Get dictionary search paths in priority order
 */
std::vector<fs::path> getDictSearchPaths(const std::string& data_directory) {
  std::vector<fs::path> paths;

  // 1. Program-selected directory (bindings use this for bundled data).
  if (!data_directory.empty()) {
    paths.emplace_back(data_directory);
    return paths;
  }

  // 2. Environment variable $SUZUME_DATA_DIR
  if (const char* env_path = std::getenv("SUZUME_DATA_DIR")) {
    paths.emplace_back(env_path);
  }

  // 3. Per-user directory.
#ifdef _WIN32
  if (const char* profile = std::getenv("USERPROFILE")) {
    paths.emplace_back(fs::path(profile) / ".suzume");
  }
#else
  if (const char* home = std::getenv("HOME")) {
    paths.emplace_back(fs::path(home) / ".suzume");
  }
#endif

  // 4. System / install directories.
#ifdef SUZUME_DATA_INSTALL_DIR
  paths.emplace_back(SUZUME_DATA_INSTALL_DIR);
#endif
#ifdef _WIN32
  if (const char* program_data = std::getenv("ProgramData")) {
    paths.emplace_back(fs::path(program_data) / "suzume");
  }
#else
  paths.emplace_back("/usr/local/share/suzume");
  paths.emplace_back("/usr/share/suzume");
#endif

  // 5. Source-checkout fallback. Installed locations take precedence so a
  // writable service working directory cannot shadow shipped dictionaries.
  paths.emplace_back("./data");

  return paths;
}

/**
 * @brief Find dictionary file in search paths
 * @param filename Dictionary filename (e.g., "core.dic")
 * @return Full path if found, empty string otherwise
 */
std::string findDictionary(const std::string& filename, const std::string& data_directory) {
  for (const auto& dir : getDictSearchPaths(data_directory)) {
    fs::path dict_path = dir / filename;
    if (fs::exists(dict_path) && fs::is_regular_file(dict_path)) {
      return dict_path.string();
    }
  }
  return "";
}
#endif

}  // namespace

struct Suzume::Impl {
  SuzumeOptions options;
  analysis::Analyzer analyzer;
  postprocess::Postprocessor postprocessor;
  std::vector<std::string> dictionary_warnings;
  std::vector<std::string> runtime_dictionary_warnings;
  mutable std::vector<std::string> dictionary_warning_view;

  struct LoadedAnalyzerConfig {
    analysis::AnalyzerOptions analyzer_options;
    std::vector<std::string> diagnostics;
  };

  static LoadedAnalyzerConfig loadAnalyzerConfig(const SuzumeOptions& opts) {
    analysis::ScorerOptions scorer_opts = opts.scorer_options;
    std::vector<std::string> diagnostics;
    std::string config_status;

#ifndef __EMSCRIPTEN__
    if (!opts.skip_env_config) {
      auto result = analysis::ScorerOptionsLoader::loadFromEnv(scorer_opts, opts.report_scorer_config);
      diagnostics.insert(diagnostics.end(), std::make_move_iterator(result.warnings.begin()),
                         std::make_move_iterator(result.warnings.end()));
      if (!result.config_path.empty()) {
        config_status = "json=" + result.config_path;
      }
      if (result.env_override_count > 0) {
        if (!config_status.empty()) {
          config_status += ", ";
        }
        config_status += "env_overrides=" + core::decimalDigits(result.env_override_count);
      }
    }
#endif

    if (!opts.scorer_options_json.empty()) {
      std::string error_message;
      if (!analysis::ScorerOptionsLoader::loadFromJsonString(opts.scorer_options_json, scorer_opts, &error_message)) {
        diagnostics.push_back("Failed to load direct scorer config: " + error_message);
      } else {
        if (!config_status.empty()) {
          config_status += ", ";
        }
        config_status += "program_json=active";
      }
    }

    if (opts.report_scorer_config && !config_status.empty()) {
      diagnostics.push_back("Scorer configuration active: " + config_status);
    }

    analysis::UnknownOptions unknown_options;
    unknown_options.verb_candidate_options = scorer_opts.candidates.verb;
    unknown_options.inflection_scorer_options = scorer_opts.inflection;
    return LoadedAnalyzerConfig{analysis::AnalyzerOptions{opts.mode, std::move(scorer_opts), std::move(unknown_options),
                                                          opts.normalize_options},
                                std::move(diagnostics)};
  }

  static postprocess::PostprocessOptions postprocessOptionsFor(const SuzumeOptions& opts) {
    bool merge_noun_compounds = opts.merge_compounds || opts.mode == core::AnalysisMode::Search;
    if (opts.mode == core::AnalysisMode::Split) {
      merge_noun_compounds = false;
    }
    return postprocess::PostprocessOptions{merge_noun_compounds, opts.lemmatize, opts.remove_symbols};
  }

  void warnDictionary(std::string message) {
#ifndef SUZUME_USE_EMBEDDED_DICT
    if (options.report_scorer_config) {
      std::cerr << "[dictionary] " << message << "\n";
    }
#endif
    dictionary_warnings.push_back(std::move(message));
  }

  void warnDictionaryLoad(const std::string& path, const core::Error& error) {
    warnDictionary("Failed to auto-load dictionary " + path + ": " + error.message);
  }

  void warnDictionaryMissing(const std::string& filename) {
    warnDictionary("Dictionary not found in automatic search paths: " + filename);
  }

  void appendDictionaryWarnings(std::vector<std::string> warnings) {
    runtime_dictionary_warnings.insert(runtime_dictionary_warnings.end(), std::make_move_iterator(warnings.begin()),
                                       std::make_move_iterator(warnings.end()));
  }

  const std::vector<std::string>& dictionaryWarnings() const {
    dictionary_warning_view = dictionary_warnings;
    dictionary_warning_view.insert(dictionary_warning_view.end(), runtime_dictionary_warnings.begin(),
                                   runtime_dictionary_warnings.end());
    return dictionary_warning_view;
  }

  explicit Impl(const SuzumeOptions& opts) : Impl(opts, loadAnalyzerConfig(opts)) {}

  Impl(const SuzumeOptions& opts, LoadedAnalyzerConfig loaded_config)
      : options(opts),
        analyzer(loaded_config.analyzer_options),
        postprocessor(&analyzer.dictionaryManager(), postprocessOptionsFor(opts)),
        dictionary_warnings(std::move(loaded_config.diagnostics)) {
    // Auto-load core.dic if found (binary format)
    if (!opts.skip_core_dictionary) {
#ifdef SUZUME_USE_EMBEDDED_DICT
      auto result = analyzer.dictionaryManager().loadCoreDictionaryFromMemoryResult(embedded::kCoreDictionary,
                                                                                    embedded::kCoreDictionarySize);
      if (!result.hasValue()) {
        warnDictionaryLoad("embedded core.dic", result.error());
      }
#else
      std::string core_path = findDictionary("core.dic", opts.data_directory);
      if (!core_path.empty()) {
        auto result = analyzer.dictionaryManager().loadCoreDictionaryResult(core_path);
        if (!result.hasValue()) {
          warnDictionaryLoad(core_path, result.error());
        }
      } else {
        warnDictionaryMissing("core.dic");
      }
#endif
    }

    // Auto-load user.dic if found (binary format)
    // Note: user.dic is also loaded as core binary dictionary for now
    if (!opts.skip_user_dictionary) {
#ifdef SUZUME_USE_EMBEDDED_DICT
      auto result = analyzer.dictionaryManager().loadBundledUserBinaryDictionaryFromMemoryResult(
          embedded::kUserDictionary, embedded::kUserDictionarySize);
      if (!result.hasValue()) {
        warnDictionaryLoad("embedded user.dic", result.error());
      }
#else
      std::string user_path = findDictionary("user.dic", opts.data_directory);
      if (!user_path.empty()) {
        auto result = analyzer.dictionaryManager().loadBundledUserBinaryDictionaryResult(user_path);
        if (!result.hasValue()) {
          warnDictionaryLoad(user_path, result.error());
        }
      } else {
        warnDictionaryMissing("user.dic");
      }
#endif
    }
#ifndef SUZUME_USE_EMBEDDED_DICT
    if (opts.data_directory.empty()) {
      if (const char* env_path = std::getenv("SUZUME_DATA_DIR")) {
        dictionary_warnings.push_back("Using external dictionary directory from SUZUME_DATA_DIR: " +
                                      std::string(env_path));
      }
    }
#endif
  }

  void setMode(core::AnalysisMode mode) {
    options.mode = mode;
    analyzer.setMode(mode);
    postprocessor.setOptions(postprocessOptionsFor(options));
  }

  core::Expected<core::AnalysisOutput, core::Error> analyzeAndPostprocess(std::string_view text) const {
    auto result = analyzer.analyzeWithNormalizedText(text);
    if (!result.hasValue()) {
      return core::makeUnexpected(result.error());
    }
    core::AnalysisOutput output = std::move(result.value());
    output.morphemes = postprocessor.process(std::move(output.morphemes));
    return output;
  }

  core::Expected<std::vector<postprocess::TagEntry>, core::Error> generateTagsResult(
      std::string_view text, const postprocess::TagGeneratorOptions& tag_options) const {
    auto analyzed = analyzeAndPostprocess(text);
    if (!analyzed.hasValue()) {
      return core::makeUnexpected(analyzed.error());
    }
    postprocess::TagGenerator generator(tag_options);
    return generator.generate(analyzed.value().morphemes);
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
#ifdef __EMSCRIPTEN__
  (void)path;
  return core::makeUnexpected(
      core::Error(core::ErrorCode::InvalidInput, "File dictionary loading is unavailable in WebAssembly"));
#else
  std::ifstream file(path);
  if (!file.is_open()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::FileNotFound, "Failed to open dictionary file: " + path));
  }
  const std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  return loadUserDictionaryFromMemoryResult(content.data(), content.size());
#endif
}

bool Suzume::loadUserDictionaryFromMemory(const char* data, size_t size) {
  return loadUserDictionaryFromMemoryResult(data, size).hasValue();
}

core::Expected<size_t, core::Error> Suzume::loadUserDictionaryFromMemoryResult(const char* data, size_t size) {
  auto loaded = loadSourceDictionaryFromMemory(data, size);
  if (!loaded.hasValue()) {
    return core::makeUnexpected(loaded.error());
  }
  impl_->analyzer.addUserDictionary(loaded.value().dictionary);
  impl_->appendDictionaryWarnings(std::move(loaded.value().warnings));
  return loaded.value().installed_entry_count;
}

bool Suzume::loadBinaryDictionary(const uint8_t* data, size_t size) {
  return loadBinaryDictionaryResult(data, size).hasValue();
}

core::Expected<size_t, core::Error> Suzume::loadBinaryDictionaryResult(const uint8_t* data, size_t size) {
  auto result = impl_->analyzer.dictionaryManager().loadUserBinaryDictionaryFromMemoryResult(data, size);
  if (!result.hasValue()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::DictionaryLoadFailed, result.error().message));
  }
  return result.value();
}

void Suzume::clearUserDictionaries() {
  impl_->analyzer.dictionaryManager().clearUserDictionaries();
  impl_->runtime_dictionary_warnings.clear();
}

const std::vector<std::string>& Suzume::dictionaryWarnings() const {
  return impl_->dictionaryWarnings();
}

bool Suzume::hasCoreDictionary() const {
  return impl_->analyzer.hasCoreBinaryDictionary();
}

std::vector<core::Morpheme> Suzume::analyze(std::string_view text) const {
  auto result = analyzeResult(text);
  if (!result.hasValue()) {
    return {};
  }
  return std::move(result).value();
}

core::Expected<std::vector<core::Morpheme>, core::Error> Suzume::analyzeResult(std::string_view text) const {
  auto result = analyzeWithNormalizedTextResult(text);
  if (!result.hasValue()) {
    return core::makeUnexpected(result.error());
  }
  return std::move(result.value().morphemes);
}

core::Expected<core::AnalysisOutput, core::Error> Suzume::analyzeWithNormalizedTextResult(std::string_view text) const {
  return impl_->analyzeAndPostprocess(text);
}

std::vector<core::Morpheme> Suzume::analyzeDebug(std::string_view text, core::Lattice* out_lattice) const {
  auto morphemes = impl_->analyzer.analyzeDebug(text, out_lattice);
  morphemes = impl_->postprocessor.process(std::move(morphemes));
  return morphemes;
}

std::vector<postprocess::TagEntry> Suzume::generateTags(std::string_view text) const {
  return generateTags(text, impl_->options.tag_options);
}

std::vector<postprocess::TagEntry> Suzume::generateTags(std::string_view text,
                                                        const postprocess::TagGeneratorOptions& options) const {
  auto result = generateTagsResult(text, options);
  return result.hasValue() ? std::move(result).value() : std::vector<postprocess::TagEntry>{};
}

core::Expected<std::vector<postprocess::TagEntry>, core::Error> Suzume::generateTagsResult(
    std::string_view text) const {
  return generateTagsResult(text, impl_->options.tag_options);
}

core::Expected<std::vector<postprocess::TagEntry>, core::Error> Suzume::generateTagsResult(
    std::string_view text, const postprocess::TagGeneratorOptions& options) const {
  return impl_->generateTagsResult(text, options);
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
