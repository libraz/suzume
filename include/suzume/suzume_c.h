/**
 * @file suzume_c.h
 * @brief C API for Suzume Japanese tokenizer
 *
 * This header provides a C-compatible API for use with WebAssembly
 * and other language bindings.
 */

#ifndef SUZUME_SUZUME_C_H_
#define SUZUME_SUZUME_C_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Symbol visibility control.
 *
 * The build system defines SUZUME_EXPORTS while compiling the suzume library
 * itself, and the exported CMake targets propagate SUZUME_STATIC to consumers
 * that link the static archive. Consumers of the shared library define neither,
 * so they resolve to the import side (dllimport on Windows).
 *
 *   - Building the library (shared) ....... SUZUME_EXPORTS  -> export
 *   - Consuming the static library ........ SUZUME_STATIC   -> no decoration
 *   - Consuming the shared library ........ (neither)       -> import
 */
#if defined(__EMSCRIPTEN__)
// CMake's EXPORTED_FUNCTIONS is the WASM API allowlist. Marking every C entry
// point as used would retain helpers that the JS binding never calls.
#define SUZUME_EXPORT
#elif defined(SUZUME_STATIC)
#define SUZUME_EXPORT
#elif defined(_WIN32)
#if defined(SUZUME_EXPORTS)
#define SUZUME_EXPORT __declspec(dllexport)
#else
#define SUZUME_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#if defined(SUZUME_EXPORTS)
#define SUZUME_EXPORT __attribute__((visibility("default")))
#else
#define SUZUME_EXPORT
#endif
#else
#define SUZUME_EXPORT
#endif

// --- ABI compatibility ---

/**
 * @brief Revision of the binary interface described by this header
 *
 * A consumer compares this compile-time value against suzume_abi_version(),
 * which reports the revision the loaded library was built from, and refuses to
 * run on a mismatch. Nothing else catches that mismatch: removing an exported
 * symbol fails loudly at link or load time, but growing a public struct does
 * not, and the consumer then reads the wrong bytes at the wrong offsets.
 *
 * Bump this when a public struct changes size or field order, or when an
 * exported entry point is removed or changes its signature. Adding a new entry
 * point that leaves the existing ones untouched does not bump it.
 */
#define SUZUME_ABI_VERSION 1

/**
 * @brief Get the ABI revision the library was compiled with
 * @return The library's SUZUME_ABI_VERSION at build time
 * @note Compare against the consumer's own SUZUME_ABI_VERSION before calling
 *       anything else; the struct layouts are only trustworthy when they match.
 */
SUZUME_EXPORT uint32_t suzume_abi_version(void);

/**
 * @brief Failure reporting contract
 *
 * Every entry point reports failure by return value (NULL, 0, or a false-like
 * code) and leaves a description in suzume_last_error(). Nothing propagates a
 * C++ exception across the ABI.
 *
 * The one failure this does NOT cover is allocation failure in a build made
 * without exceptions, which is how the WebAssembly package is compiled: there
 * the internal guards compile away and an out-of-memory condition terminates
 * the module instead of returning NULL. A caller cannot distinguish or recover
 * from it; size the runtime's memory for the largest input instead.
 */

/**
 * @brief Opaque handle to Suzume instance
 *
 * A handle is NOT thread-safe: the analyzer keeps per-handle mutable state,
 * so calling suzume_analyze / suzume_generate_tags concurrently on the same
 * handle is undefined behavior. Use one handle per thread, or serialize all
 * calls that share a handle. Distinct handles may be used concurrently.
 */
typedef struct SuzumeHandle* suzume_t;

/** Stable numeric part-of-speech codes used by result structures. */
typedef uint8_t suzume_pos_t;

enum {
  SUZUME_POS_UNKNOWN = 0,
  SUZUME_POS_NOUN = 1,
  SUZUME_POS_VERB = 2,
  SUZUME_POS_ADJECTIVE = 3,
  SUZUME_POS_ADVERB = 4,
  SUZUME_POS_PARTICLE = 5,
  SUZUME_POS_AUXILIARY = 6,
  SUZUME_POS_CONJUNCTION = 7,
  SUZUME_POS_DETERMINER = 8,
  SUZUME_POS_PRONOUN = 9,
  SUZUME_POS_PREFIX = 10,
  SUZUME_POS_SUFFIX = 11,
  SUZUME_POS_INTERJECTION = 12,
  SUZUME_POS_SYMBOL = 13,
  SUZUME_POS_OTHER = 14,
};

/** Stable numeric ExtendedPOS, conjugation-type, and conjugation-form codes. */
typedef uint8_t suzume_extended_pos_t;
typedef uint8_t suzume_conjugation_type_t;
typedef uint8_t suzume_conjugation_form_t;
typedef uint8_t suzume_error_code_t;

/** Stable error codes returned by suzume_last_error_code(). */
enum {
  SUZUME_ERROR_SUCCESS = 0,
  SUZUME_ERROR_INVALID_UTF8 = 1,
  SUZUME_ERROR_DICTIONARY_LOAD_FAILED = 2,
  SUZUME_ERROR_FILE_NOT_FOUND = 3,
  SUZUME_ERROR_PARSE = 4,
  SUZUME_ERROR_OUT_OF_MEMORY = 5,
  SUZUME_ERROR_INVALID_INPUT = 6,
  SUZUME_ERROR_INTERNAL = 7,
};

enum {
  SUZUME_MORPHEME_USER_DICT = 1U << 0U,
  SUZUME_MORPHEME_FORMAL_NOUN = 1U << 1U,
  SUZUME_MORPHEME_LOW_INFO = 1U << 2U,
  SUZUME_MORPHEME_UNKNOWN = 1U << 3U,
  SUZUME_MORPHEME_FROM_DICTIONARY = 1U << 4U,
  /** Verb, adjective, or auxiliary; conjugation fields are meaningful. */
  SUZUME_MORPHEME_CONJUGATABLE = 1U << 5U,
};

/** Stable POS-filter bits used by suzume_tag_options_t::pos_filter. */
enum {
  SUZUME_TAG_POS_NOUN = 1U << 0U,
  SUZUME_TAG_POS_VERB = 1U << 1U,
  SUZUME_TAG_POS_ADJECTIVE = 1U << 2U,
  SUZUME_TAG_POS_ADVERB = 1U << 3U,
  SUZUME_TAG_POS_PARTICLE = 1U << 4U,
  SUZUME_TAG_POS_AUXILIARY = 1U << 5U,
};

/** Stable analysis-mode codes used by extended options and mode accessors. */
enum {
  SUZUME_MODE_NORMAL = 0,
  SUZUME_MODE_SEARCH = 1,
  SUZUME_MODE_SPLIT = 2,
  SUZUME_MODE_INVALID = 255,
};

/**
 * @brief Morpheme data structure
 */
typedef struct {
  const char* surface;                        /**< Borrowed UTF-8 view owned by the containing result */
  const char* base_form;                      /**< Borrowed UTF-8 view owned by the containing result */
  uint32_t start;                             /**< Start character offset in normalized text */
  uint32_t end;                               /**< End character offset in normalized text */
  float score;                                /**< Candidate score/cost */
  suzume_pos_t pos;                           /**< Part-of-speech code */
  suzume_extended_pos_t extended_pos;         /**< ExtendedPOS code */
  suzume_conjugation_type_t conjugation_type; /**< Conjugation type code; may be None for a conjugatable morpheme */
  suzume_conjugation_form_t conjugation_form; /**< Conjugation form code; meaningful when CONJUGATABLE is set */
  uint8_t flags;                              /**< Bitwise SUZUME_MORPHEME_* flags */
  size_t surface_size;                        /**< Byte length; surface may contain U+0000 */
  size_t base_form_size;                      /**< Byte length; base form may contain U+0000 */
} suzume_morpheme_t;

/**
 * @brief Analysis result structure
 */
typedef struct {
  suzume_morpheme_t* morphemes; /**< Borrowed array; valid until suzume_result_free */
  size_t count;                 /**< Number of morphemes */
  const char* normalized_text;  /**< Borrowed UTF-8 text; valid until result free */
  size_t normalized_text_size;  /**< Byte length; normalized text may contain U+0000 */
} suzume_result_t;

/**
 * @brief Tag generation result structure
 */
typedef struct {
  const char* const* tags; /**< Borrowed strings and array; valid until suzume_tags_free */
  suzume_pos_t* pos;       /**< Borrowed array of numeric POS codes */
  size_t count;            /**< Number of tags */
} suzume_tags_t;

/**
 * @brief Extended analysis options structure.
 *
 * Use suzume_init_extended_options() before overriding individual fields so
 * default true values such as preserve_case and lemmatize are preserved.
 */
typedef struct {
  uint8_t preserve_vu;             /**< Preserve ヴ (don't normalize to ビ etc.) */
  uint8_t preserve_case;           /**< Preserve case (don't lowercase ASCII) */
  uint8_t preserve_symbols;        /**< Preserve punctuation-like SYMBOL tokens (emoji remain OTHER) */
  uint8_t mode;                    /**< 0=normal, 1=search, 2=split */
  uint8_t lemmatize;               /**< Replace source lemmas with corrected lemmas; annotations are always computed */
  uint8_t merge_compounds;         /**< Merge consecutive noun compounds */
  uint8_t skip_user_dictionary;    /**< Skip automatic loading of the bundled user dictionary */
  uint8_t skip_core_dictionary;    /**< Skip automatic loading of the bundled core dictionary */
  uint8_t report_scorer_config;    /**< Add scorer configuration diagnostics to dictionary warnings */
  uint8_t skip_env_config;         /**< Ignore native scorer configuration environment variables */
  const char* scorer_options_json; /**< UTF-8 JSON scorer overrides, or NULL for defaults */
  const char* data_directory;      /**< Exclusive dictionary directory, or NULL for search defaults */
} suzume_extended_options_t;

// --- Lifecycle functions ---

/**
 * @brief Create a new Suzume instance with default options
 * @return Handle to Suzume instance, or NULL on failure
 * @note The returned handle is not thread-safe for concurrent analysis
 *       calls; see suzume_t.
 */
SUZUME_EXPORT suzume_t suzume_create(void);

/**
 * @brief Initialize extended options with Suzume defaults
 * @param options Pointer to options structure to initialize
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_init_extended_options(suzume_extended_options_t* options);

/**
 * @brief Create a new Suzume instance with extended options
 * @param options Pointer to extended options structure
 * @return Handle to Suzume instance, or NULL on failure
 * @note Passing NULL selects all defaults. String pointers are borrowed only
 *       for the duration of this call and may be NULL. Program JSON overrides
 *       native environment configuration unless skip_env_config disables it.
 */
SUZUME_EXPORT suzume_t suzume_create_with_extended_options(const suzume_extended_options_t* options);

/**
 * @brief Destroy Suzume instance and free resources
 * @param handle Suzume handle
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_destroy(suzume_t handle);

/**
 * @brief Change an existing handle's analysis mode without reloading dictionaries.
 * @param handle Suzume handle
 * @param mode One of SUZUME_MODE_NORMAL, SUZUME_MODE_SEARCH, or SUZUME_MODE_SPLIT
 * @return 1 on success, 0 for an invalid handle or mode (see suzume_last_error).
 */
SUZUME_EXPORT int suzume_set_mode(suzume_t handle, uint8_t mode);

/**
 * @brief Get an existing handle's analysis mode.
 * @param handle Suzume handle
 * @return A SUZUME_MODE_* value, or SUZUME_MODE_INVALID for an invalid handle.
 */
SUZUME_EXPORT uint8_t suzume_mode(suzume_t handle);

// --- Analysis functions ---

/**
 * @brief Analyze Japanese text into morphemes
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @return Analysis result allocated by Suzume, or NULL on failure.
 *         Invalid UTF-8 input fails with NULL and a suzume_last_error()
 *         message; empty input succeeds with an empty result (count == 0).
 *         Non-NULL results must be freed exactly once with suzume_result_free.
 *         All pointers inside the result borrow from one result-owned arena and
 *         become invalid together when that function is called.
 * @note Not thread-safe with respect to other calls on the same handle;
 *       see suzume_t.
 */
SUZUME_EXPORT suzume_result_t* suzume_analyze(suzume_t handle, const char* text);

/**
 * @brief Analyze an explicit UTF-8 byte range
 * @param handle Suzume handle
 * @param text UTF-8 bytes; embedded U+0000 is preserved
 * @param size Number of bytes in text
 * @return Analysis result, or NULL on failure
 */
SUZUME_EXPORT suzume_result_t* suzume_analyze_n(suzume_t handle, const char* text, size_t size);

/**
 * @brief Free analysis result
 * @param result Result to free
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_result_free(suzume_result_t* result);

/**
 * @brief Generate tags from Japanese text
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @return Tags result allocated by Suzume, or NULL on failure.
 *         Non-NULL results must be freed exactly once with suzume_tags_free.
 *         The array and strings are borrowed from one result-owned arena.
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags(suzume_t handle, const char* text);

/**
 * @brief Generate tags from an explicit UTF-8 byte range
 * @param handle Suzume handle
 * @param text UTF-8 bytes; embedded U+0000 is preserved
 * @param size Number of bytes in text
 * @return Tags result, or NULL on failure
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_n(suzume_t handle, const char* text, size_t size);

/**
 * @brief Tag generation options.
 *
 * Call suzume_init_tag_options() to populate the documented defaults before
 * overriding individual fields.
 */
typedef struct {
  uint8_t pos_filter;           /**< Bitwise SUZUME_TAG_POS_* values; 0 includes all filterable POS */
  uint8_t exclude_basic;        /**< Exclude basic words (hiragana-only lemma) */
  uint8_t use_lemma;            /**< Use lemma instead of surface (default: 1) */
  size_t min_length;            /**< Minimum tag length in characters (default: 2) */
  size_t max_tags;              /**< Maximum number of tags (0=unlimited) */
  uint8_t exclude_particles;    /**< Exclude particles (default: 1) */
  uint8_t exclude_auxiliaries;  /**< Exclude auxiliaries (default: 1) */
  uint8_t exclude_formal_nouns; /**< Exclude formal nouns (default: 1) */
  uint8_t exclude_low_info;     /**< Exclude low information words (default: 1) */
  uint8_t remove_duplicates;    /**< Remove duplicate tags (default: 1) */
} suzume_tag_options_t;

/**
 * @brief Initialize tag options with Suzume defaults
 * @param options Pointer to options structure to initialize
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_init_tag_options(suzume_tag_options_t* options);

/**
 * @brief Generate tags from Japanese text with options
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @param options Tag generation options
 * @return Tags result allocated by Suzume, or NULL on failure.
 *         Non-NULL results must be freed exactly once with suzume_tags_free.
 * @note options must not be NULL.
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_with_options(suzume_t handle, const char* text,
                                                               const suzume_tag_options_t* options);

/**
 * @brief Generate tags from an explicit UTF-8 byte range with options
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_with_options_n(suzume_t handle, const char* text, size_t size,
                                                                 const suzume_tag_options_t* options);

/**
 * @brief Free tags result
 * @param tags Tags to free
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_tags_free(suzume_tags_t* tags);

// --- Dictionary functions ---

/**
 * @brief Add user-dictionary entries from UTF-8 source text
 * @param handle Suzume handle
 * @param data Current TSV or legacy 3-column CSV source bytes. TSV rows use
 *             surface<TAB>POS[<TAB>conj_type][<TAB>lemma]. The legacy CSV cost
 *             column is accepted but ignored.
 * @param size Data size in bytes
 * @return 1 on success, 0 on failure
 * @note Loads are additive until suzume_clear_user_dictionaries() is called.
 */
SUZUME_EXPORT int suzume_load_user_dict(suzume_t handle, const char* data, size_t size);

/**
 * @brief Add user-dictionary entries and return the installed entry count
 * @return Number of installed entries, or 0 on failure. Inspect
 *         suzume_last_error_code() to distinguish failure.
 */
SUZUME_EXPORT size_t suzume_load_user_dict_count(suzume_t handle, const char* data, size_t size);

/**
 * @brief Load binary dictionary from memory (as user dictionary)
 * @param handle Suzume handle
 * @param data Binary dictionary data (.dic format)
 * @param size Data size in bytes
 * @return 1 on success, 0 on failure
 * @note Loads are additive. A failed load preserves all existing dictionaries.
 */
SUZUME_EXPORT int suzume_load_binary_dict(suzume_t handle, const uint8_t* data, size_t size);

/**
 * @brief Remove user dictionaries explicitly loaded through this handle
 *
 * The automatically loaded bundled user dictionary remains installed.
 * @return 1 on success, 0 on invalid handle
 */
SUZUME_EXPORT int suzume_clear_user_dictionaries(suzume_t handle);

/**
 * @brief Check whether the handle has a loaded L2 core binary dictionary
 * @return 1 when loaded, 0 when absent or the handle is invalid
 */
SUZUME_EXPORT int suzume_has_core_dictionary(suzume_t handle);

// --- Utility functions ---

/**
 * @brief Get Suzume version string
 * @return Version string (static, do not free)
 */
SUZUME_EXPORT const char* suzume_version(void);

/**
 * @brief Get the last C API error message for the current thread/runtime
 * @return Borrowed thread-local string; do not free. It is invalidated by a
 *         later C API call that clears or replaces the diagnostic, and by
 *         thread termination.
 */
SUZUME_EXPORT const char* suzume_last_error(void);

/**
 * @brief Get the stable code for the last C API error on this thread/runtime
 */
SUZUME_EXPORT suzume_error_code_t suzume_last_error_code(void);

/**
 * @brief Get the Japanese label for a serialized conjugation type
 * @return Static string, or NULL when code is none/out of range
 */
SUZUME_EXPORT const char* suzume_conjugation_type_label(suzume_conjugation_type_t code);

/**
 * @brief Get the stable label for a serialized extended POS code
 * @return Static string, or NULL when code is out of range
 */
SUZUME_EXPORT const char* suzume_extended_pos_label(suzume_extended_pos_t code);

/**
 * @brief Get the Japanese label for a serialized conjugation form code
 * @return Static string, or NULL when code is out of range
 */
SUZUME_EXPORT const char* suzume_conjugation_form_label(suzume_conjugation_form_t code);

/**
 * @brief Get the stable English label for a serialized POS code
 * @return Static string, or NULL when code is out of range
 */
SUZUME_EXPORT const char* suzume_pos_label(suzume_pos_t code);

/**
 * @brief Get number of dictionary warnings from auto-loading dictionaries
 * @param handle Suzume handle
 * @return Warning count, or 0 for null handle
 */
SUZUME_EXPORT size_t suzume_dictionary_warning_count(suzume_t handle);

/**
 * @brief Get dictionary warning message by index
 * @param handle Suzume handle
 * @param index Warning index
 * @return Warning string owned by Suzume, or NULL if out of range.
 *         The pointer is valid only until the next suzume_dictionary_warning
 *         call from the same thread or until the handle is destroyed,
 *         whichever comes first; copy the string to retain it.
 */
SUZUME_EXPORT const char* suzume_dictionary_warning(suzume_t handle, size_t index);

/**
 * @brief Get sizeof(suzume_result_t)
 */
SUZUME_EXPORT size_t suzume_sizeof_result(void);

/**
 * @brief Get sizeof(suzume_morpheme_t)
 */
SUZUME_EXPORT size_t suzume_sizeof_morpheme(void);

/**
 * @brief Get sizeof(suzume_tags_t)
 */
SUZUME_EXPORT size_t suzume_sizeof_tags(void);

/**
 * @brief Get sizeof(suzume_tag_options_t)
 */
SUZUME_EXPORT size_t suzume_sizeof_tag_options(void);

/**
 * @brief Get sizeof(suzume_extended_options_t)
 */
SUZUME_EXPORT size_t suzume_sizeof_extended_options(void);

/**
 * @brief Get byte offset of field in suzume_result_t
 * @param field 0=morphemes, 1=count, 2=normalized_text,
 *              3=normalized_text_size
 */
SUZUME_EXPORT size_t suzume_offsetof_result(uint32_t field);

/**
 * @brief Get byte offset of field in suzume_morpheme_t
 * @param field 0=surface, 1=base_form, 2=start, 3=end, 4=score,
 *              5=pos, 6=extended_pos, 7=conjugation_type,
 *              8=conjugation_form, 9=flags, 10=surface_size,
 *              11=base_form_size
 */
SUZUME_EXPORT size_t suzume_offsetof_morpheme(uint32_t field);

/**
 * @brief Get byte offset of field in suzume_tags_t
 * @param field 0=tags, 1=pos, 2=count
 */
SUZUME_EXPORT size_t suzume_offsetof_tags(uint32_t field);

/**
 * @brief Get byte offset of field in suzume_tag_options_t
 * @param field 0=pos_filter, 1=exclude_basic, 2=use_lemma,
 *              3=min_length, 4=max_tags, 5=exclude_particles,
 *              6=exclude_auxiliaries, 7=exclude_formal_nouns,
 *              8=exclude_low_info, 9=remove_duplicates
 */
SUZUME_EXPORT size_t suzume_offsetof_tag_options(uint32_t field);

/**
 * @brief Get byte offset of field in suzume_extended_options_t
 * @param field 0=preserve_vu, 1=preserve_case, 2=preserve_symbols,
 *              3=mode, 4=lemmatize, 5=merge_compounds,
 *              6=skip_user_dictionary, 7=skip_core_dictionary,
 *              8=report_scorer_config, 9=skip_env_config,
 *              10=scorer_options_json, 11=data_directory
 */
SUZUME_EXPORT size_t suzume_offsetof_extended_options(uint32_t field);

#ifdef __cplusplus
}
#endif

#endif  // SUZUME_SUZUME_C_H_
