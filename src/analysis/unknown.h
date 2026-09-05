#ifndef SUZUME_ANALYSIS_UNKNOWN_H_
#define SUZUME_ANALYSIS_UNKNOWN_H_

#include <string>
#include <string_view>
#include <vector>

#include "analysis/candidate_options.h"
#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"

namespace suzume::analysis {

// Use CandidateOrigin from core::
using core::CandidateOrigin;

/**
 * @brief Unknown word generation options
 */
struct UnknownOptions {
  size_t max_kanji_length = 16;  // Long compounds like 独立行政法人情報処理推進機構
  size_t max_katakana_length = 16;
  size_t max_alphabet_length = 20;
  size_t max_alphanumeric_length = 24;
  size_t max_hiragana_length = 6;
  size_t max_unknown_length = 12;

  // Suffix separation
  bool separate_suffix = true;
  float suffix_separation_bonus = -0.3F;

  // Character speech patterns (キャラ語尾)
  // When enabled, short hiragana/katakana at sentence-end positions
  // are treated as potential character speech suffixes with lower cost.
  bool enable_character_speech = true;
  float character_speech_cost = 0.6F;      // Higher than dict suffix (0.5) to prefer dict
  size_t max_character_speech_length = 4;  // Max codepoints for pattern match

  // Verb candidate generation options
  VerbCandidateOptions verb_candidate_options;
  grammar::InflectionScorerOptions inflection_scorer_options;
};

/**
 * @brief Unknown word candidate
 */
struct UnknownCandidate {
  std::string surface;  // Store as string, not string_view
  size_t start{0};
  size_t end{0};
  core::PartOfSpeech pos{core::PartOfSpeech::Noun};
  core::ExtendedPOS extended_pos{core::ExtendedPOS::Unknown};  // Fine-grained POS for bigram
  float cost{0.0F};
  bool has_suffix{false};
  bool lemma_verified{false};  // Lemma (base form) attested as a dictionary verb at generation time
  bool bracketed_noun_rescue{false};
  bool requires_left_content_edge{false};
  bool requires_left_attributive_edge{false};
  bool rejects_preceding_content_edge{false};
  std::string lemma;  // Base form (for verbs/adjectives)
  dictionary::ConjugationType conj_type{dictionary::ConjugationType::None};
  // Retained outside debug builds because connection scoring uses selected
  // generated-candidate origins.
  CandidateOrigin origin{CandidateOrigin::Unknown};

#ifdef SUZUME_DEBUG_INFO
  float confidence{0.0F};   // Inflection analysis confidence (for verbs/adj)
  std::string pattern;      // Pattern detail (e.g., "ichidan_te_form")
  std::string epos_source;  // Where ExtendedPOS was set (e.g., "verb_cand_kanji")
#endif
};

// =============================================================================
// Candidate Factory Functions
// =============================================================================

/**
 * @brief Create a verb candidate
 * @param surface Surface form
 * @param start Start position (character index)
 * @param end End position (character index)
 * @param cost Candidate cost
 * @param lemma Base form (dictionary form)
 * @param conj_type Conjugation type
 * @param has_suffix Whether candidate expects suffix
 * @param origin Candidate origin (used by connection scoring)
 * @param confidence Inflection confidence (debug only)
 * @param pattern Pattern name (debug only)
 * @param extended_pos Extended POS for bigram (optional)
 */
UnknownCandidate makeVerbCandidate(const std::string& surface, size_t start, size_t end, float cost,
                                   const std::string& lemma, dictionary::ConjugationType conj_type,
                                   bool has_suffix = false, CandidateOrigin origin = CandidateOrigin::Unknown,
                                   [[maybe_unused]] float confidence = 0.0F,
                                   [[maybe_unused]] const char* pattern = nullptr,
                                   core::ExtendedPOS extended_pos = core::ExtendedPOS::Unknown,
                                   [[maybe_unused]] const char* epos_source = nullptr);

/**
 * @brief Create a verb candidate spelled by codepoints[start, end)
 *
 * The span form owns the conversion from the codepoint range to the surface,
 * which otherwise inlines the UTF-8 encode loop and the temporary's teardown
 * into every caller. Callers that already hold the surface keep using the
 * string form.
 *
 * Every span caller states its own suffix expectation, origin and confidence,
 * so those carry no defaults here.
 */
UnknownCandidate makeVerbCandidate(const std::vector<char32_t>& codepoints, size_t start, size_t end, float cost,
                                   const std::string& lemma, dictionary::ConjugationType conj_type, bool has_suffix,
                                   CandidateOrigin origin, [[maybe_unused]] float confidence,
                                   [[maybe_unused]] const char* pattern = nullptr,
                                   core::ExtendedPOS extended_pos = core::ExtendedPOS::Unknown,
                                   [[maybe_unused]] const char* epos_source = nullptr);

/**
 * @brief Create a noun candidate
 * @param surface Surface form
 * @param start Start position (character index)
 * @param end End position (character index)
 * @param cost Candidate cost
 * @param has_suffix Whether candidate expects suffix
 * @param origin Candidate origin (used by connection scoring)
 * @param extended_pos Extended POS for bigram (optional)
 */
UnknownCandidate makeNounCandidate(const std::string& surface, size_t start, size_t end, float cost,
                                   bool has_suffix = false, CandidateOrigin origin = CandidateOrigin::Unknown,
                                   core::ExtendedPOS extended_pos = core::ExtendedPOS::Unknown,
                                   [[maybe_unused]] const char* epos_source = nullptr);

/**
 * @brief Create a candidate with specified POS
 * @param surface Surface form
 * @param start Start position (character index)
 * @param end End position (character index)
 * @param pos Part of speech
 * @param cost Candidate cost
 * @param has_suffix Whether candidate expects suffix
 * @param origin Candidate origin (used by connection scoring)
 * @param extended_pos Extended POS for bigram (optional)
 */
UnknownCandidate makeCandidate(const std::string& surface, size_t start, size_t end, core::PartOfSpeech pos, float cost,
                               bool has_suffix = false, CandidateOrigin origin = CandidateOrigin::Unknown,
                               core::ExtendedPOS extended_pos = core::ExtendedPOS::Unknown,
                               [[maybe_unused]] const char* epos_source = nullptr);

/**
 * @brief Create a candidate with specified POS spelled by codepoints[start, end)
 *
 * The span counterpart of the string form, and the single owner of that
 * conversion for the callers that build the surface only to hand it over.
 */
UnknownCandidate makeCandidate(const std::vector<char32_t>& codepoints, size_t start, size_t end,
                               core::PartOfSpeech pos, float cost, bool has_suffix = false,
                               CandidateOrigin origin = CandidateOrigin::Unknown,
                               core::ExtendedPOS extended_pos = core::ExtendedPOS::Unknown,
                               [[maybe_unused]] const char* epos_source = nullptr);

/**
 * @brief Unknown word generator
 *
 * Generates candidates for words not in dictionary
 * based on character type sequences.
 */
class UnknownWordGenerator {
 public:
  explicit UnknownWordGenerator(const UnknownOptions& options = {},
                                const dictionary::DictionaryManager* dict_manager = nullptr);
  ~UnknownWordGenerator() = default;

  // Non-copyable, non-movable
  UnknownWordGenerator(const UnknownWordGenerator&) = delete;
  UnknownWordGenerator& operator=(const UnknownWordGenerator&) = delete;
  UnknownWordGenerator(UnknownWordGenerator&&) = delete;
  UnknownWordGenerator& operator=(UnknownWordGenerator&&) = delete;

  /**
   * @brief Generate unknown word candidates
   * @param text Text
   * @param codepoints Codepoints of text
   * @param start_pos Start position (character index)
   * @param char_types Character types
   * @return Vector of candidates
   */
  std::vector<UnknownCandidate> generate(std::string_view text, const std::vector<char32_t>& codepoints,
                                         size_t start_pos, const std::vector<normalize::CharType>& char_types) const;

  /**
   * @brief Access the shared inflection analyzer
   */
  const grammar::Inflection& inflection() const { return inflection_; }

 private:
  UnknownOptions options_;
  const dictionary::DictionaryManager* dict_manager_;
  grammar::Inflection inflection_;

  /**
   * @brief Generate candidates for same-type sequences
   */
  void generateBySameType(const std::vector<char32_t>& codepoints, size_t start_pos,
                          const std::vector<normalize::CharType>& char_types,
                          std::vector<UnknownCandidate>& candidates) const;

  /**
   * @brief Generate alphanumeric sequence candidates
   */
  void generateAlphanumeric(std::string_view text, const std::vector<char32_t>& codepoints, size_t start_pos,
                            const std::vector<normalize::CharType>& char_types,
                            std::vector<UnknownCandidate>& candidates) const;

  /**
   * @brief Generate character speech pattern candidates (キャラ語尾)
   *
   * For unknown hiragana/katakana sequences at potential sentence-end positions,
   * generates low-cost auxiliary candidates. This allows recognition of novel
   * character speech patterns not in the hardcoded dictionary.
   *
   * Examples: ナリ, ござる, だわ, etc.
   */
  void generateCharacterSpeechCandidates(std::string_view text, const std::vector<char32_t>& codepoints,
                                         size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                         std::vector<UnknownCandidate>& candidates) const;

  /**
   * @brief Generate ABAB-type onomatopoeia candidates
   *
   * Detects 4-character hiragana/katakana patterns where characters 1-2
   * match characters 3-4, like わくわく, きらきら, どきどき.
   * These are recognized as adverbs.
   */
  void generateOnomatopoeiaCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                      const std::vector<normalize::CharType>& char_types,
                                      std::vector<UnknownCandidate>& candidates) const;

  /**
   * @brief Get max length for character type
   */
  size_t getMaxLength(normalize::CharType ctype) const;

  /**
   * @brief Get POS for character type
   */
  static core::PartOfSpeech getPosForType(normalize::CharType ctype);

  /**
   * @brief Get cost for character type
   */
  static float getCostForType(normalize::CharType ctype, size_t length);
};

}  // namespace suzume::analysis
#endif  // SUZUME_ANALYSIS_UNKNOWN_H_
