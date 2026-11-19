/**
 * @file suzume.hpp
 * @brief Header-only C++ wrapper over the Suzume C ABI (suzume_c.h).
 *
 * This is a thin, exception-free convenience layer: it owns the handle via RAII,
 * copies results into owning std::string/std::vector, and reports native errors
 * through empty results plus lastError(). Owning C++ allocations may still
 * throw std::bad_alloc. It depends only on the stable C ABI, so
 * it stays ABI-compatible across releases. Include this from C++; C code includes
 * suzume_c.h directly.
 */

#ifndef SUZUME_SUZUME_HPP_
#define SUZUME_SUZUME_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "suzume/suzume_c.h"

namespace suzume {

/** @brief Analysis mode; mirrors the C ABI mode codes. */
enum class Mode : std::uint8_t {
  Normal = SUZUME_MODE_NORMAL,
  Search = SUZUME_MODE_SEARCH,
  Split = SUZUME_MODE_SPLIT,
};

/** @brief Tokenizer construction options (see suzume_options_t). */
struct Options {
  bool preserve_vu = true;
  bool preserve_case = true;
  bool preserve_symbols = false;
  Mode mode = Mode::Normal;
  bool lemmatize = true;
  bool merge_compounds = false;
  bool skip_user_dictionary = false;
  bool skip_core_dictionary = false;
  bool skip_env_config = false;
  bool report_scorer_config = false;
  std::string scorer_options_json;
  std::string data_directory;
};

/** @brief A single analyzed morpheme with owning strings. */
struct Morpheme {
  std::string surface;       ///< Surface form (UTF-8).
  std::string pos;           ///< Part of speech (English).
  std::string base_form;     ///< Base/dictionary form.
  std::string lemma;         ///< @deprecated Use base_form; retained for source compatibility through the next release.
  std::string pos_ja;        ///< Part of speech (Japanese).
  std::string conj_type;     ///< Conjugation type (Japanese); empty when N/A.
  std::string conj_form;     ///< Conjugation form (Japanese); empty when N/A.
  std::string extended_pos;  ///< Stable extended POS code (e.g. "VERB_連用").
  std::size_t start = 0;     ///< Start character offset in normalized text.
  std::size_t end = 0;       ///< End character offset in normalized text.
  bool is_user_dict = false;
  bool is_formal_noun = false;
  bool is_low_info = false;
  bool is_unknown = false;
  bool is_from_dictionary = false;
  float score = 0.0F;
};

/** @brief Normalized input together with analyzed morphemes. */
struct AnalysisResult {
  std::string normalized_text;
  std::vector<Morpheme> morphemes;
};

/** @brief Tag-generation options (see suzume_tag_options_t). */
struct TagOptions {
  std::uint8_t pos_filter = 0;  ///< POS bitmask; 0 includes all filterable POS.
  bool exclude_basic = false;
  bool use_lemma = true;
  std::size_t min_length = 2;
  std::size_t max_tags = 0;
  bool exclude_particles = true;
  bool exclude_auxiliaries = true;
  bool exclude_formal_nouns = true;
  bool exclude_low_info = true;
  bool remove_duplicates = true;
};

/** @brief A generated tag (surface/lemma text plus its POS). */
struct Tag {
  std::string tag;
  std::string pos;
};

namespace detail {

inline std::string conjugationTypeLabel(std::uint8_t code) {
  const char* label = suzume_conjugation_type_label(code);
  return label != nullptr ? std::string(label) : std::string();
}

inline std::string conjugationFormLabel(std::uint8_t code) {
  const char* label = suzume_conjugation_form_label(code);
  return label != nullptr ? std::string(label) : std::string();
}

}  // namespace detail

/**
 * @brief RAII wrapper around a suzume_t handle.
 *
 * Move-only. A handle is not thread-safe for concurrent analysis; use one
 * Tokenizer per thread or serialize calls that share one.
 */
class Tokenizer {
 public:
  Tokenizer() : handle_(suzume_create()) {}

  explicit Tokenizer(const Options& options) : handle_(nullptr) {
    suzume_options_t copts;
    suzume_init_options(&copts);
    copts.preserve_vu = options.preserve_vu ? 1 : 0;
    copts.preserve_case = options.preserve_case ? 1 : 0;
    copts.preserve_symbols = options.preserve_symbols ? 1 : 0;
    copts.mode = static_cast<std::uint8_t>(options.mode);
    copts.lemmatize = options.lemmatize ? 1 : 0;
    copts.merge_compounds = options.merge_compounds ? 1 : 0;
    copts.skip_bundled_user_dictionary = options.skip_user_dictionary ? 1 : 0;
    copts.skip_core_dictionary = options.skip_core_dictionary ? 1 : 0;
    copts.skip_env_config = options.skip_env_config ? 1 : 0;
    copts.report_scorer_config = options.report_scorer_config ? 1 : 0;
    copts.scorer_options_json = options.scorer_options_json.empty() ? nullptr : options.scorer_options_json.c_str();
    copts.data_directory = options.data_directory.empty() ? nullptr : options.data_directory.c_str();
    handle_ = suzume_create_with_options(&copts);
  }

  Tokenizer(const Tokenizer&) = delete;
  Tokenizer& operator=(const Tokenizer&) = delete;

  Tokenizer(Tokenizer&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  Tokenizer& operator=(Tokenizer&& other) noexcept {
    if (this != &other) {
      suzume_destroy(handle_);
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ~Tokenizer() { suzume_destroy(handle_); }

  /** @brief True when the handle was created successfully. */
  bool valid() const noexcept { return handle_ != nullptr; }
  explicit operator bool() const noexcept { return valid(); }

  /** @brief Get the current analysis mode. Returns Mode::Normal for an invalid tokenizer. */
  Mode mode() const {
    if (handle_ == nullptr) {
      return Mode::Normal;
    }
    return static_cast<Mode>(suzume_mode(handle_));
  }

  /** @brief Change analysis mode without reloading dictionaries. */
  bool setMode(Mode mode) { return handle_ != nullptr && suzume_set_mode(handle_, static_cast<uint8_t>(mode)) != 0; }

  /**
   * @brief Analyze text into morphemes.
   * @note const does not make concurrent calls on one Tokenizer safe; the
   * underlying analyzer maintains mutable caches.
   * @return Morphemes, or an empty vector on failure (see lastError()).
   */
  std::vector<Morpheme> analyze(std::string_view text) const { return analyzeWithNormalizedText(text).morphemes; }

  /** @brief Analyze text and expose the normalized text used for offsets. */
  AnalysisResult analyzeWithNormalizedText(std::string_view text) const {
    AnalysisResult out;
    if (handle_ == nullptr) {
      return out;
    }
    suzume_result_t* result = suzume_analyze_n(handle_, text.empty() ? "" : text.data(), text.size());
    if (result == nullptr) {
      return out;
    }
    std::unique_ptr<suzume_result_t, decltype(&suzume_result_free)> owned_result(result, &suzume_result_free);
    out.normalized_text.assign(owned_result->normalized_text, owned_result->normalized_text_size);
    out.morphemes.reserve(owned_result->count);
    for (std::size_t idx = 0; idx < owned_result->count; ++idx) {
      const suzume_morpheme_t& src = owned_result->morphemes[idx];
      Morpheme morph;
      morph.surface.assign(src.surface, src.surface_size);
      morph.pos = posLabel(src.pos, false);
      morph.base_form.assign(src.base_form, src.base_form_size);
      morph.lemma = morph.base_form;
      morph.pos_ja = posLabel(src.pos, true);
      const bool conjugates = (src.flags & SUZUME_MORPHEME_CONJUGATABLE) != 0;
      morph.conj_type = conjugates ? detail::conjugationTypeLabel(src.conjugation_type) : std::string();
      morph.conj_form = conjugates ? detail::conjugationFormLabel(src.conjugation_form) : std::string();
      morph.extended_pos = extendedPosLabel(src.extended_pos);
      morph.start = src.start;
      morph.end = src.end;
      morph.is_user_dict = (src.flags & SUZUME_MORPHEME_USER_DICT) != 0;
      morph.is_formal_noun = (src.flags & SUZUME_MORPHEME_FORMAL_NOUN) != 0;
      morph.is_low_info = (src.flags & SUZUME_MORPHEME_LOW_INFO) != 0;
      morph.is_unknown = (src.flags & SUZUME_MORPHEME_UNKNOWN) != 0;
      morph.is_from_dictionary = (src.flags & SUZUME_MORPHEME_FROM_DICTIONARY) != 0;
      morph.score = src.score;
      out.morphemes.push_back(std::move(morph));
    }
    return out;
  }

  /** @brief Generate search tags with default options. Returns empty on failure; see lastError(). */
  std::vector<Tag> generateTags(std::string_view text) const {
    if (handle_ == nullptr) {
      return {};
    }
    return collectTags(suzume_generate_tags_n(handle_, text.empty() ? "" : text.data(), text.size()));
  }

  /** @brief Generate search tags with explicit options. Returns empty on failure; see lastError(). */
  std::vector<Tag> generateTags(std::string_view text, const TagOptions& options) const {
    if (handle_ == nullptr) {
      return {};
    }
    suzume_tag_options_t copts;
    suzume_init_tag_options(&copts);
    copts.pos_filter = options.pos_filter;
    copts.exclude_basic = options.exclude_basic ? 1 : 0;
    copts.use_base_form = options.use_lemma ? 1 : 0;
    copts.min_length = options.min_length;
    copts.max_tags = options.max_tags;
    copts.exclude_particles = options.exclude_particles ? 1 : 0;
    copts.exclude_auxiliaries = options.exclude_auxiliaries ? 1 : 0;
    copts.exclude_formal_nouns = options.exclude_formal_nouns ? 1 : 0;
    copts.exclude_low_info = options.exclude_low_info ? 1 : 0;
    copts.remove_duplicates = options.remove_duplicates ? 1 : 0;
    return collectTags(
        suzume_generate_tags_with_options_n(handle_, text.empty() ? "" : text.data(), text.size(), &copts));
  }

  /**
   * @brief Load additional user-dictionary entries from TSV bytes.
   * @return True on success; see lastError() on failure.
   */
  bool loadUserDictionary(std::string_view tsv) { return loadUserDictionaryCount(tsv) > 0; }

  /**
   * @brief Load additional user-dictionary entries and return the installed count.
   */
  std::size_t loadUserDictionaryCount(std::string_view tsv) {
    if (handle_ == nullptr) {
      return 0;
    }
    return suzume_load_user_dictionary(handle_, tsv.empty() ? "" : tsv.data(), tsv.size());
  }

  /**
   * @brief Load a compiled binary dictionary from memory.
   * @return True on success; see lastError() on failure.
   */
  bool loadBinaryDictionary(const std::uint8_t* data, std::size_t size) {
    if (handle_ == nullptr) {
      return false;
    }
    return suzume_load_binary_dictionary(handle_, data, size) != 0;
  }

  /** @brief Remove caller-loaded dictionaries while retaining the bundled user dictionary. */
  bool clearUserDictionaries() { return handle_ != nullptr && suzume_clear_user_dictionaries(handle_) != 0; }

  /** @brief Whether the L2 core binary dictionary is loaded. */
  bool hasCoreDictionary() const { return handle_ != nullptr && suzume_has_core_dictionary(handle_) != 0; }

  /** @brief Dictionary-loading, parsing, and scorer-configuration diagnostics. */
  std::vector<std::string> dictionaryWarnings() const {
    if (handle_ == nullptr) {
      return {};
    }
    std::vector<std::string> warnings;
    const std::size_t count = suzume_dictionary_warning_count(handle_);
    warnings.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      const char* warning = suzume_dictionary_warning(handle_, index);
      if (warning != nullptr) {
        warnings.emplace_back(warning);
      }
    }
    return warnings;
  }

  /** @brief Last error message for this thread (empty when none). */
  static std::string lastError() { return cstr(suzume_last_error()); }

  /** @brief Stable code for the last C ABI error. */
  static suzume_error_code_t lastErrorCode() { return suzume_last_error_code(); }

  /** @brief Library version string. */
  static std::string version() { return cstr(suzume_version()); }

 private:
  static std::string cstr(const char* str) { return str != nullptr ? std::string(str) : std::string(); }

  static std::string posLabel(std::uint8_t code, bool japanese) {
    static constexpr std::array<const char*, 15> japanese_labels = {"その他", "名詞",   "動詞",   "形容詞", "副詞",
                                                                    "助詞",   "助動詞", "接続詞", "連体詞", "代名詞",
                                                                    "接頭辞", "接尾辞", "感動詞", "記号",   "その他"};
    if (!japanese) {
      return cstr(suzume_pos_label(code));
    }
    if (code >= japanese_labels.size()) {
      code = 0;
    }
    return japanese_labels[code];
  }

  static std::string extendedPosLabel(std::uint8_t code) {
    const char* label = suzume_extended_pos_label(code);
    return label != nullptr ? std::string(label) : "UNKNOWN";
  }

  static std::vector<Tag> collectTags(suzume_tags_t* tags) {
    std::vector<Tag> out;
    if (tags == nullptr) {
      return out;
    }
    std::unique_ptr<suzume_tags_t, decltype(&suzume_tags_free)> owned_tags(tags, &suzume_tags_free);
    out.reserve(owned_tags->count);
    for (std::size_t idx = 0; idx < owned_tags->count; ++idx) {
      Tag entry;
      entry.tag = cstr(owned_tags->tags != nullptr ? owned_tags->tags[idx] : nullptr);
      entry.pos = owned_tags->pos != nullptr ? posLabel(owned_tags->pos[idx], false) : std::string();
      out.push_back(std::move(entry));
    }
    return out;
  }

  suzume_t handle_;
};

}  // namespace suzume

#endif  // SUZUME_SUZUME_HPP_
