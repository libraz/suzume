#ifndef SUZUME_ANALYSIS_ANALYZER_H_
#define SUZUME_ANALYSIS_ANALYZER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "analysis/scorer.h"
#include "analysis/tokenizer.h"
#include "analysis/unknown.h"
#include "core/morpheme.h"
#include "core/types.h"
#include "core/viterbi.h"
#include "dictionary/dictionary.h"
#include "dictionary/user_dict.h"
#include "normalize/normalizer.h"
#include "pretokenizer/pretokenizer.h"

namespace suzume::analysis {

/**
 * @brief Analyzer options
 */
struct AnalyzerOptions {
  core::AnalysisMode mode = core::AnalysisMode::Normal;
  ScorerOptions scorer_options;
  UnknownOptions unknown_options;
  normalize::NormalizeOptions normalize_options;
};

/**
 * @brief Main morphological analyzer
 */
class Analyzer {
 public:
  explicit Analyzer(const AnalyzerOptions& options = {});
  ~Analyzer();

  // Non-copyable, non-movable (contains non-movable Scorer)
  Analyzer(const Analyzer&) = delete;
  Analyzer& operator=(const Analyzer&) = delete;
  Analyzer(Analyzer&&) = delete;
  Analyzer& operator=(Analyzer&&) = delete;

  /**
   * @brief Add user dictionary
   * @param dict User dictionary to add
   */
  void addUserDictionary(std::shared_ptr<dictionary::UserDictionary> dict);

  /**
   * @brief Try to auto-load core dictionary from standard paths
   * @return true if loaded successfully
   */
  bool tryAutoLoadCoreDictionary();

  /**
   * @brief Check if core binary dictionary is loaded
   */
  bool hasCoreBinaryDictionary() const;

  /**
   * @brief Analyze text
   * @param text UTF-8 text
   * @return Vector of morphemes
   */
  std::vector<core::Morpheme> analyze(std::string_view text) const;

  /**
   * @brief Debug analyze - returns lattice information for debugging
   * @param text UTF-8 text
   * @param out_lattice Output lattice (if not null)
   * @return Vector of morphemes
   */
  std::vector<core::Morpheme> analyzeDebug(std::string_view text,
                                           core::Lattice* out_lattice) const;

  /**
   * @brief Get analysis mode
   */
  core::AnalysisMode mode() const { return options_.mode; }

  /**
   * @brief Set analysis mode
   */
  void setMode(core::AnalysisMode mode) { options_.mode = mode; }

  /**
   * @brief Get dictionary manager reference
   *
   * Used for dictionary-aware lemmatization.
   */
  const dictionary::DictionaryManager& dictionaryManager() const {
    return dict_manager_;
  }

  dictionary::DictionaryManager& dictionaryManager() { return dict_manager_; }

 private:
  AnalyzerOptions options_;
  normalize::Normalizer normalizer_;
  pretokenizer::PreTokenizer pretokenizer_;
  dictionary::DictionaryManager dict_manager_;
  Scorer scorer_;
  UnknownWordGenerator unknown_gen_;
  std::unique_ptr<Tokenizer> tokenizer_;
  core::Viterbi viterbi_;

  /**
   * @brief Analyze a text span (without pretokenization)
   */
  std::vector<core::Morpheme> analyzeSpan(std::string_view text,
                                          size_t char_offset) const;

  /**
   * @brief Convert Viterbi result to morphemes
   */
  static std::vector<core::Morpheme> pathToMorphemes(const core::ViterbiResult& result, const core::Lattice& lattice,
                                                     std::string_view original_text);
};

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_ANALYZER_H_
