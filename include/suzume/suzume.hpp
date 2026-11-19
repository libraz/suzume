/**
 * @file suzume.hpp
 * @brief Header-only C++17 wrapper over the Suzume C ABI (suzume_c.h).
 *
 * Owns the handle via RAII, copies results into owning std::string/std::vector,
 * and reports every native failure through suzume::Expected<T> carrying a
 * suzume::Error. It never throws and builds with -fno-exceptions; owning C++
 * allocations may still fail as std::bad_alloc where exceptions are enabled.
 * It depends only on the C ABI, and Tokenizer::create() rejects a library whose
 * ABI revision differs from the header's. C code includes suzume_c.h directly.
 */

#ifndef SUZUME_SUZUME_HPP_
#define SUZUME_SUZUME_HPP_

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "suzume/suzume_c.h"

namespace suzume {

/** @brief Stable error codes; mirrors SUZUME_ERROR_*, plus Unknown for codes the header does not know. */
enum class ErrorCode : int {
  Success = SUZUME_ERROR_SUCCESS,
  InvalidUtf8 = SUZUME_ERROR_INVALID_UTF8,
  DictionaryLoadFailed = SUZUME_ERROR_DICTIONARY_LOAD_FAILED,
  FileNotFound = SUZUME_ERROR_FILE_NOT_FOUND,
  Parse = SUZUME_ERROR_PARSE,
  OutOfMemory = SUZUME_ERROR_OUT_OF_MEMORY,
  InvalidInput = SUZUME_ERROR_INVALID_INPUT,
  Internal = SUZUME_ERROR_INTERNAL,
  Unknown = 255,  ///< Reserved for bindings; the library never reports it.
};

/** @brief Map a native error code to ErrorCode; any value outside 0..7 becomes ErrorCode::Unknown. */
constexpr ErrorCode errorCodeFromNative(int code) noexcept {
  if (code < SUZUME_ERROR_SUCCESS || code > SUZUME_ERROR_INTERNAL) {
    return ErrorCode::Unknown;
  }
  return static_cast<ErrorCode>(code);
}

/**
 * @brief A failed call.
 *
 * `message` is the native text unchanged, or exactly "<operation> failed" when
 * the library supplied none. `operation` is one of: create, analyze,
 * generate_tags, load_user_dictionary, load_binary_dictionary,
 * clear_user_dictionaries, mode.
 */
struct Error {
  ErrorCode code = ErrorCode::Success;
  std::string message;
  std::string operation;
};

/**
 * @brief A value of type T or an Error.
 *
 * value(), operator* and operator-> require has_value(); calling them on an
 * error calls std::abort(). error() on a value returns an Error whose code is
 * ErrorCode::Success.
 */
template <class T>
class Expected {
 public:
  Expected(T value) : value_(std::move(value)) {}  // NOLINT(google-explicit-constructor): implicit like std::optional
  Expected(Error error) : error_(std::move(error)) {}  // NOLINT(google-explicit-constructor): implicit failure

  bool has_value() const noexcept { return value_.has_value(); }
  explicit operator bool() const noexcept { return has_value(); }

  T& value() & {
    requireValue();
    return *value_;
  }
  const T& value() const& {
    requireValue();
    return *value_;
  }
  T&& value() && {
    requireValue();
    return std::move(*value_);
  }

  T& operator*() & { return value(); }
  const T& operator*() const& { return value(); }
  T&& operator*() && { return std::move(*this).value(); }
  T* operator->() { return &value(); }
  const T* operator->() const { return &value(); }

  const Error& error() const noexcept { return error_; }

 private:
  void requireValue() const {
    if (!value_.has_value()) {
      std::abort();
    }
  }

  std::optional<T> value_;
  Error error_;
};

/** @brief Success with no value, or an Error. */
template <>
class Expected<void> {
 public:
  Expected() = default;
  Expected(Error error)
      : failed_(true), error_(std::move(error)) {}  // NOLINT(google-explicit-constructor): implicit failure

  bool has_value() const noexcept { return !failed_; }
  explicit operator bool() const noexcept { return has_value(); }
  const Error& error() const noexcept { return error_; }

 private:
  bool failed_ = false;
  Error error_;
};

/** @brief Analysis mode; mirrors the C ABI mode codes. */
enum class Mode : std::uint8_t {
  Normal = SUZUME_MODE_NORMAL,
  Search = SUZUME_MODE_SEARCH,
  Split = SUZUME_MODE_SPLIT,
};

/** @brief Tag POS filter entries; the names are the POS labels the output prints (NOUN VERB ADJ ADV PARTICLE AUX). */
enum class TagPos : std::uint8_t {
  Noun = SUZUME_TAG_POS_NOUN,  ///< Also admits pronouns (PRON).
  Verb = SUZUME_TAG_POS_VERB,
  Adj = SUZUME_TAG_POS_ADJECTIVE,
  Adv = SUZUME_TAG_POS_ADVERB,
  Particle = SUZUME_TAG_POS_PARTICLE,
  Aux = SUZUME_TAG_POS_AUXILIARY,
};

/** @brief Tokenizer construction options (see suzume_options_t). */
struct Options {
  bool preserve_vu = true;                    ///< Keep ヴ instead of normalizing it to ビ etc.
  bool preserve_case = true;                  ///< Keep ASCII case instead of lowercasing.
  bool preserve_symbols = false;              ///< Keep punctuation-like SYMBOL tokens (emoji remain OTHER).
  Mode mode = Mode::Normal;                   ///< Initial analysis mode; change it later with Tokenizer::setMode().
  bool lemmatize = true;                      ///< Replace source base forms with corrected base forms.
  bool merge_compounds = false;               ///< Merge consecutive noun compounds into one morpheme.
  bool skip_bundled_user_dictionary = false;  ///< Skip loading the bundled user dictionary only.
  bool skip_core_dictionary = false;          ///< Skip loading the bundled core dictionary.
  bool skip_env_config = false;               ///< Ignore native scorer configuration environment variables.
  bool report_scorer_config = false;          ///< Add scorer configuration diagnostics to dictionaryWarnings().
  std::string scorer_options;                 ///< Scorer overrides as JSON text; empty selects the defaults.
  std::string data_directory;                 ///< Exclusive dictionary directory; empty uses the search defaults.
};

/** @brief A single analyzed morpheme with owning strings. */
struct Morpheme {
  std::string surface;                   ///< Surface form (UTF-8).
  std::string pos;                       ///< Part of speech label (e.g. "NOUN"); "OTHER" when unknown.
  std::string base_form;                 ///< Base (dictionary) form.
  std::string pos_ja;                    ///< Part of speech label in Japanese (e.g. "名詞").
  std::optional<std::string> conj_type;  ///< Conjugation type (Japanese); std::nullopt when not applicable.
  std::optional<std::string> conj_form;  ///< Conjugation form (Japanese); std::nullopt when not applicable.
  std::string extended_pos;              ///< Stable extended POS code (e.g. "VERB_連用"); "UNKNOWN" when unknown.
  std::size_t start = 0;                 ///< Start offset in characters (code points) into normalized_text.
  std::size_t end = 0;                   ///< End offset (exclusive) in characters into normalized_text.
  bool is_user_dict = false;             ///< Came from a user dictionary.
  bool is_formal_noun = false;           ///< Formal noun such as こと or もの.
  bool is_low_info = false;              ///< Low-information word.
  bool is_unknown = false;               ///< Not found in any dictionary.
  bool is_from_dictionary = false;       ///< Came from a dictionary entry.
  float score = 0.0F;                    ///< Morpheme cost; lower is more likely, not comparable across inputs.
};

/** @brief Normalized input together with analyzed morphemes. */
struct AnalysisResult {
  std::string normalized_text;  ///< The text the morpheme offsets index into.
  std::vector<Morpheme> morphemes;
};

/** @brief Tag-generation options (see suzume_tag_options_t). */
struct TagOptions {
  std::vector<TagPos> pos_filter;    ///< POS to keep; empty keeps every filterable POS, duplicates are ignored.
  bool exclude_basic = false;        ///< Exclude basic words (base form written only in hiragana).
  bool use_base_form = true;         ///< Emit the base form instead of the surface.
  std::size_t min_length = 2;        ///< Minimum tag length in characters.
  std::size_t max_tags = 0;          ///< Maximum number of tags; 0 is unlimited.
  bool exclude_particles = true;     ///< Exclude particles unless pos_filter names TagPos::Particle.
  bool exclude_auxiliaries = true;   ///< Exclude auxiliaries unless pos_filter names TagPos::Aux.
  bool exclude_formal_nouns = true;  ///< Exclude formal nouns.
  bool exclude_low_info = true;      ///< Exclude low-information words.
  bool remove_duplicates = true;     ///< Keep only the first occurrence of each tag.
};

/** @brief A generated tag (base form or surface text plus its POS label). */
struct Tag {
  std::string tag;
  std::string pos;
};

/** @brief Library version string. */
inline std::string version() {
  const char* text = suzume_version();
  return text != nullptr ? std::string(text) : std::string();
}

namespace detail {

/** @brief A native label, or std::nullopt when it is NULL or empty. */
inline std::optional<std::string> optionalLabel(const char* native) {
  if (native == nullptr || *native == '\0') {
    return std::nullopt;
  }
  return std::string(native);
}

/** @brief A native label, or `fallback` when it is NULL or empty. */
inline std::string label(const char* native, const char* fallback) {
  auto text = optionalLabel(native);
  return text.has_value() ? std::move(*text) : std::string(fallback);
}

inline Error operationFailed(ErrorCode code, const char* operation) {
  return Error{code, std::string(operation) + " failed", operation};
}

/** @brief The error a failed native call left behind, attributed to `operation`. */
inline Error lastNativeError(const char* operation) {
  const ErrorCode code = errorCodeFromNative(suzume_last_error_code());
  if (code == ErrorCode::Success) {
    return operationFailed(ErrorCode::Internal, operation);
  }
  auto message = optionalLabel(suzume_last_error());
  if (!message.has_value()) {
    return operationFailed(code, operation);
  }
  return Error{code, std::move(*message), operation};
}

inline Error released(const char* operation) {
  return Error{ErrorCode::InvalidInput, "Suzume instance has been released", operation};
}

}  // namespace detail

/**
 * @brief RAII wrapper around a suzume_t handle, obtained from create().
 *
 * Move-only. A handle is not thread-safe for concurrent analysis; use one
 * Tokenizer per thread or serialize calls that share one. A moved-from
 * Tokenizer returns Error{InvalidInput, "Suzume instance has been released"}
 * from every Expected member, and its noexcept getters return Mode::Normal,
 * false and an empty list.
 */
class Tokenizer {
 public:
  /** @brief Create a tokenizer; fails on an ABI revision mismatch or a native construction error. */
  [[nodiscard]] static Expected<Tokenizer> create(const Options& options = {}) {
    const std::uint32_t library_abi = suzume_abi_version();
    if (library_abi != SUZUME_ABI_VERSION) {
      return Error{ErrorCode::Internal,
                   "suzume ABI revision mismatch: header " + std::to_string(SUZUME_ABI_VERSION) + ", library " +
                       std::to_string(library_abi),
                   "create"};
    }
    suzume_options_t copts;
    suzume_init_options(&copts);
    copts.preserve_vu = options.preserve_vu ? 1 : 0;
    copts.preserve_case = options.preserve_case ? 1 : 0;
    copts.preserve_symbols = options.preserve_symbols ? 1 : 0;
    copts.mode = static_cast<suzume_mode_t>(options.mode);
    copts.lemmatize = options.lemmatize ? 1 : 0;
    copts.merge_compounds = options.merge_compounds ? 1 : 0;
    copts.skip_bundled_user_dictionary = options.skip_bundled_user_dictionary ? 1 : 0;
    copts.skip_core_dictionary = options.skip_core_dictionary ? 1 : 0;
    copts.skip_env_config = options.skip_env_config ? 1 : 0;
    copts.report_scorer_config = options.report_scorer_config ? 1 : 0;
    copts.scorer_options_json = options.scorer_options.empty() ? nullptr : options.scorer_options.c_str();
    copts.data_directory = options.data_directory.empty() ? nullptr : options.data_directory.c_str();
    suzume_t handle = suzume_create_with_options(&copts);
    if (handle == nullptr) {
      return detail::lastNativeError("create");
    }
    return Tokenizer(handle);
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

  /** @brief Current analysis mode; Mode::Normal on a moved-from tokenizer. */
  Mode mode() const noexcept {
    if (handle_ == nullptr) {
      return Mode::Normal;
    }
    return static_cast<Mode>(suzume_mode(handle_));
  }

  /** @brief Change the analysis mode without reloading dictionaries (operation "mode"). */
  [[nodiscard]] Expected<void> setMode(Mode mode) {
    if (handle_ == nullptr) {
      return detail::released("mode");
    }
    if (suzume_set_mode(handle_, static_cast<suzume_mode_t>(mode)) == 0) {
      return detail::lastNativeError("mode");
    }
    return {};
  }

  /**
   * @brief Analyze text into morphemes (operation "analyze").
   * @note const does not make concurrent calls on one Tokenizer safe; the
   * underlying analyzer maintains mutable caches.
   */
  [[nodiscard]] Expected<std::vector<Morpheme>> analyze(std::string_view text) const {
    auto result = analyzeWithNormalizedText(text);
    if (!result) {
      return result.error();
    }
    return std::move(std::move(result).value().morphemes);
  }

  /** @brief Analyze text and expose the normalized text the offsets index into (operation "analyze"). */
  [[nodiscard]] Expected<AnalysisResult> analyzeWithNormalizedText(std::string_view text) const {
    if (handle_ == nullptr) {
      return detail::released("analyze");
    }
    suzume_result_t* result = suzume_analyze_n(handle_, text.empty() ? "" : text.data(), text.size());
    if (result == nullptr) {
      return detail::lastNativeError("analyze");
    }
    std::unique_ptr<suzume_result_t, decltype(&suzume_result_free)> owned_result(result, &suzume_result_free);
    AnalysisResult out;
    out.normalized_text.assign(owned_result->normalized_text, owned_result->normalized_text_size);
    out.morphemes.reserve(owned_result->count);
    for (std::size_t idx = 0; idx < owned_result->count; ++idx) {
      const suzume_morpheme_t& src = owned_result->morphemes[idx];
      Morpheme morph;
      morph.surface.assign(src.surface, src.surface_size);
      morph.pos = detail::label(suzume_pos_label(src.pos), "OTHER");
      morph.base_form.assign(src.base_form, src.base_form_size);
      morph.pos_ja = detail::label(suzume_pos_label_ja(src.pos), "その他");
      if ((src.flags & SUZUME_MORPHEME_CONJUGATABLE) != 0) {
        morph.conj_type = detail::optionalLabel(suzume_conjugation_type_label(src.conjugation_type));
        morph.conj_form = detail::optionalLabel(suzume_conjugation_form_label(src.conjugation_form));
      }
      morph.extended_pos = detail::label(suzume_extended_pos_label(src.extended_pos), "UNKNOWN");
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

  /** @brief Generate search tags (operation "generate_tags"). */
  [[nodiscard]] Expected<std::vector<Tag>> generateTags(std::string_view text, const TagOptions& options = {}) const {
    if (handle_ == nullptr) {
      return detail::released("generate_tags");
    }
    suzume_tag_options_t copts;
    suzume_init_tag_options(&copts);
    suzume_tag_pos_t pos_bits = 0;
    for (const TagPos pos : options.pos_filter) {
      pos_bits = static_cast<suzume_tag_pos_t>(pos_bits | static_cast<suzume_tag_pos_t>(pos));
    }
    copts.pos_filter = pos_bits;
    copts.exclude_basic = options.exclude_basic ? 1 : 0;
    copts.use_base_form = options.use_base_form ? 1 : 0;
    copts.min_length = options.min_length;
    copts.max_tags = options.max_tags;
    copts.exclude_particles = options.exclude_particles ? 1 : 0;
    copts.exclude_auxiliaries = options.exclude_auxiliaries ? 1 : 0;
    copts.exclude_formal_nouns = options.exclude_formal_nouns ? 1 : 0;
    copts.exclude_low_info = options.exclude_low_info ? 1 : 0;
    copts.remove_duplicates = options.remove_duplicates ? 1 : 0;
    suzume_tags_t* tags =
        suzume_generate_tags_with_options_n(handle_, text.empty() ? "" : text.data(), text.size(), &copts);
    if (tags == nullptr) {
      return detail::lastNativeError("generate_tags");
    }
    std::unique_ptr<suzume_tags_t, decltype(&suzume_tags_free)> owned_tags(tags, &suzume_tags_free);
    std::vector<Tag> out;
    out.reserve(owned_tags->count);
    for (std::size_t idx = 0; idx < owned_tags->count; ++idx) {
      Tag entry;
      entry.tag = detail::label(owned_tags->tags[idx], "");
      entry.pos = detail::label(suzume_pos_label(owned_tags->pos[idx]), "OTHER");
      out.push_back(std::move(entry));
    }
    return out;
  }

  /**
   * @brief Load user-dictionary entries from TSV text (operation "load_user_dictionary").
   * @return The number of installed entries (at least 1). An empty source fails
   * with InvalidInput, a source with no loadable entries with Parse.
   */
  [[nodiscard]] Expected<std::size_t> loadUserDictionary(std::string_view tsv) {
    if (handle_ == nullptr) {
      return detail::released("load_user_dictionary");
    }
    const std::size_t count = suzume_load_user_dictionary(handle_, tsv.data(), tsv.size());
    if (count == 0) {
      return detail::lastNativeError("load_user_dictionary");
    }
    return count;
  }

  /**
   * @brief Load a compiled binary dictionary from memory (operation "load_binary_dictionary").
   * @return The number of installed entries (at least 1).
   */
  [[nodiscard]] Expected<std::size_t> loadBinaryDictionary(std::string_view bytes) {
    if (handle_ == nullptr) {
      return detail::released("load_binary_dictionary");
    }
    const std::size_t count =
        suzume_load_binary_dictionary(handle_, reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    if (count == 0) {
      return detail::lastNativeError("load_binary_dictionary");
    }
    return count;
  }

  /** @brief Remove caller-loaded dictionaries, keeping the bundled user dictionary (operation
   * "clear_user_dictionaries"). */
  [[nodiscard]] Expected<void> clearUserDictionaries() {
    if (handle_ == nullptr) {
      return detail::released("clear_user_dictionaries");
    }
    if (suzume_clear_user_dictionaries(handle_) == 0) {
      return detail::lastNativeError("clear_user_dictionaries");
    }
    return {};
  }

  /** @brief Whether the core binary dictionary is loaded; false on a moved-from tokenizer. */
  bool hasCoreDictionary() const noexcept { return handle_ != nullptr && suzume_has_core_dictionary(handle_) != 0; }

  /** @brief Dictionary-loading, parsing and scorer-configuration diagnostics; empty on a moved-from tokenizer. */
  std::vector<std::string> dictionaryWarnings() const {
    std::vector<std::string> warnings;
    if (handle_ == nullptr) {
      return warnings;
    }
    const std::size_t count = suzume_dictionary_warning_count(handle_);
    warnings.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      auto warning = detail::optionalLabel(suzume_dictionary_warning(handle_, index));
      if (warning.has_value()) {
        warnings.push_back(std::move(*warning));
      }
    }
    return warnings;
  }

 private:
  explicit Tokenizer(suzume_t handle) noexcept : handle_(handle) {}

  suzume_t handle_;
};

}  // namespace suzume

#endif  // SUZUME_SUZUME_HPP_
