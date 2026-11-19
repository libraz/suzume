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
 * A consumer compares this compile-time value against the revision the loaded
 * library was built from (reported by suzume_abi_version()), and refuses to
 * run on a mismatch. Nothing else catches that mismatch: removing an exported
 * symbol fails loudly at link or load time, but growing a public struct does
 * not, and the consumer then reads the wrong bytes at the wrong offsets.
 *
 * Bump this when a public struct changes size or field order, or when an
 * exported entry point is removed or changes its signature. Adding a new entry
 * point that leaves the existing ones untouched does not bump it.
 */
#define SUZUME_ABI_VERSION 2

/**
 * @brief Get the ABI revision the library was compiled with
 * @return The library's SUZUME_ABI_VERSION at build time. Always succeeds and
 *         does not touch suzume_last_error().
 * @note Compare against the consumer's own SUZUME_ABI_VERSION before calling
 *       anything else; the struct layouts are only trustworthy when they match.
 */
SUZUME_EXPORT uint32_t suzume_abi_version(void);

/**
 * @brief Failure reporting contract
 *
 * Every entry point reports failure by return value (NULL, 0, or a false-like
 * code) and leaves a description in suzume_last_error(). Nothing propagates a
 * C++ exception across the ABI. A successful read-only query (suzume_mode,
 * suzume_has_core_dictionary, the dictionary-warning accessors) leaves an
 * earlier diagnostic in place, so it can be read after the failed call. A call
 * that clears the diagnostic sets the code back to SUZUME_ERROR_SUCCESS.
 *
 * Text entry points take UTF-8. The *_n variants take an explicit byte length
 * and preserve embedded U+0000, but text must not be NULL even when size is 0;
 * pass "" for empty input.
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

/**
 * Stable error codes returned by suzume_last_error_code().
 *
 * Values are frozen and may only be appended to. 255 is reserved for bindings,
 * which use it as UNKNOWN for a code they do not recognize; the library never
 * returns it.
 */
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

/** Bitwise OR of SUZUME_TAG_POS_* values; bit n selects SUZUME_POS_* code n+1. */
typedef uint8_t suzume_tag_pos_t;

/**
 * Stable POS-filter bits used by suzume_tag_options_t::pos_filter.
 *
 * Values are frozen and may only be appended to. SUZUME_TAG_POS_NOUN also
 * admits pronoun (SUZUME_POS_PRONOUN) tags. A bit outside this set selects no
 * POS, so a mask made only of such bits yields no tags.
 */
enum {
  SUZUME_TAG_POS_NOUN = 1U << 0U,
  SUZUME_TAG_POS_VERB = 1U << 1U,
  SUZUME_TAG_POS_ADJECTIVE = 1U << 2U,
  SUZUME_TAG_POS_ADVERB = 1U << 3U,
  SUZUME_TAG_POS_PARTICLE = 1U << 4U,
  SUZUME_TAG_POS_AUXILIARY = 1U << 5U,
};

/** One of the SUZUME_MODE_* values. */
typedef uint8_t suzume_mode_t;

/**
 * Stable analysis-mode codes used by suzume_options_t and the mode accessors.
 * Values are frozen and may only be appended to; SUZUME_MODE_INVALID is only
 * ever returned by suzume_mode() for a NULL handle.
 */
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
  uint32_t start;                             /**< Start offset in code points into normalized_text */
  uint32_t end;                               /**< End offset (exclusive) in code points into normalized_text */
  float score;                                /**< Morpheme cost; lower is more likely, not comparable across inputs */
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
 * @brief Analysis options structure.
 *
 * suzume_init_options() defines the defaults; call it before overriding
 * individual fields. Zero-initialization is valid but is not the default (it
 * turns off preserve_vu, preserve_case and lemmatize).
 */
typedef struct {
  uint8_t preserve_vu;      /**< Preserve ヴ (don't normalize to ビ etc.) */
  uint8_t preserve_case;    /**< Preserve case (don't lowercase ASCII) */
  uint8_t preserve_symbols; /**< Preserve punctuation-like SYMBOL tokens (emoji remain OTHER) */
  suzume_mode_t mode;       /**< 0=normal, 1=search, 2=split */
  uint8_t lemmatize;        /**< Replace source base forms with corrected base forms; annotations are always computed */
  uint8_t merge_compounds;  /**< Merge consecutive noun compounds */
  uint8_t skip_bundled_user_dictionary; /**< Skip automatic loading of the bundled user dictionary only */
  uint8_t skip_core_dictionary;         /**< Skip automatic loading of the bundled core dictionary */
  uint8_t report_scorer_config;         /**< Add scorer configuration diagnostics to dictionary warnings */
  uint8_t skip_env_config;              /**< Ignore native scorer configuration environment variables */
  const char* scorer_options_json;      /**< UTF-8 JSON scorer overrides, or NULL for defaults */
  const char* data_directory;           /**< Exclusive dictionary directory, or NULL for search defaults */
} suzume_options_t;

// --- Lifecycle functions ---

/**
 * @brief Create a new Suzume instance with default options
 * @return Handle to Suzume instance, or NULL on failure with
 *         suzume_last_error() set: SUZUME_ERROR_OUT_OF_MEMORY or
 *         SUZUME_ERROR_INTERNAL (see the failure reporting contract for builds
 *         without exceptions).
 * @note The returned handle is not thread-safe for concurrent analysis
 *       calls; see suzume_t.
 */
SUZUME_EXPORT suzume_t suzume_create(void);

/**
 * @brief Initialize analysis options with Suzume defaults
 * @param options Pointer to options structure to initialize
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_init_options(suzume_options_t* options);

/**
 * @brief Create a new Suzume instance with analysis options
 * @param options Pointer to options structure, or NULL
 * @return Handle to Suzume instance, or NULL on failure with
 *         suzume_last_error() set. SUZUME_ERROR_INVALID_INPUT: a mode outside
 *         SUZUME_MODE_* ("invalid mode"). SUZUME_ERROR_PARSE: malformed
 *         scorer_options_json ("invalid scorer options: ..."). Otherwise as
 *         suzume_create().
 * @note Passing NULL selects all defaults, exactly like suzume_create(). String pointers are borrowed only
 *       for the duration of this call and may be NULL. Program JSON overrides
 *       native environment configuration unless skip_env_config disables it.
 */
SUZUME_EXPORT suzume_t suzume_create_with_options(const suzume_options_t* options);

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
 * @return 1 on success. 0 on failure with SUZUME_ERROR_INVALID_INPUT: a NULL
 *         handle ("null handle") or a mode outside SUZUME_MODE_* ("invalid
 *         mode"); the mode is then unchanged.
 */
SUZUME_EXPORT int suzume_set_mode(suzume_t handle, suzume_mode_t mode);

/**
 * @brief Get an existing handle's analysis mode.
 * @param handle Suzume handle
 * @return A SUZUME_MODE_* value. SUZUME_MODE_INVALID for a NULL handle, with
 *         SUZUME_ERROR_INVALID_INPUT ("null handle"); a successful call leaves
 *         suzume_last_error() untouched.
 */
SUZUME_EXPORT suzume_mode_t suzume_mode(suzume_t handle);

// --- Analysis functions ---

/**
 * @brief Analyze Japanese text into morphemes
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @return Analysis result allocated by Suzume, or NULL on failure with
 *         suzume_last_error() set. SUZUME_ERROR_INVALID_INPUT: a NULL handle or
 *         text ("null handle or text"). SUZUME_ERROR_INVALID_UTF8: malformed
 *         input ("invalid UTF-8 input"). SUZUME_ERROR_OUT_OF_MEMORY or
 *         SUZUME_ERROR_INTERNAL: an internal failure. Empty input succeeds with
 *         an empty result (count == 0).
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
 * @return As suzume_analyze(); text must not be NULL even when size is 0.
 *         On failure NULL with the SUZUME_ERROR_* codes listed there.
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
 * @return Tags result allocated by Suzume, or NULL on failure with the
 *         SUZUME_ERROR_* codes suzume_analyze() lists. Uses the values
 *         suzume_init_tag_options() writes.
 *         Non-NULL results must be freed exactly once with suzume_tags_free.
 *         The array and strings are borrowed from one result-owned arena.
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags(suzume_t handle, const char* text);

/**
 * @brief Generate tags from an explicit UTF-8 byte range
 * @param handle Suzume handle
 * @param text UTF-8 bytes; embedded U+0000 is preserved
 * @param size Number of bytes in text
 * @return As suzume_generate_tags(); text must not be NULL even when size is 0.
 *         On failure NULL with the SUZUME_ERROR_* codes suzume_analyze() lists.
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_n(suzume_t handle, const char* text, size_t size);

/**
 * @brief Tag generation options.
 *
 * suzume_init_tag_options() defines the defaults; call it before overriding
 * individual fields. Zero-initialization is valid but is not the default.
 */
typedef struct {
  suzume_tag_pos_t pos_filter;  /**< Bitwise SUZUME_TAG_POS_* values; 0 includes all filterable POS */
  uint8_t exclude_basic;        /**< Exclude basic words (hiragana-only base form) */
  uint8_t use_base_form;        /**< Use the base form instead of the surface (default: 1) */
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
 * @param options Tag generation options, or NULL for the values
 *                suzume_init_tag_options() writes
 * @return Tags result allocated by Suzume, or NULL on failure with the
 *         SUZUME_ERROR_* codes suzume_analyze() lists.
 *         Non-NULL results must be freed exactly once with suzume_tags_free.
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_with_options(suzume_t handle, const char* text,
                                                               const suzume_tag_options_t* options);

/**
 * @brief Generate tags from an explicit UTF-8 byte range with options
 * @param handle Suzume handle
 * @param text UTF-8 bytes; embedded U+0000 is preserved
 * @param size Number of bytes in text
 * @param options Tag generation options, or NULL for the values
 *                suzume_init_tag_options() writes
 * @return As suzume_generate_tags_with_options(); text must not be NULL even
 *         when size is 0. On failure NULL with the SUZUME_ERROR_* codes
 *         suzume_analyze() lists.
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
 * @param data Dictionary source bytes: tab-separated rows
 *             surface<TAB>POS[<TAB>conj_type][<TAB>base_form]. May be NULL only
 *             when size is 0.
 * @param size Data size in bytes
 * @return Number of installed entries (at least 1), or 0 on failure with
 *         suzume_last_error() set. SUZUME_ERROR_INVALID_INPUT: a NULL handle,
 *         NULL data with a non-zero size, or a zero-length source ("Empty
 *         dictionary data"). SUZUME_ERROR_PARSE: a malformed source, including
 *         a row outside that layout, or one with no loadable entries
 *         ("Dictionary source contains no loadable entries").
 * @note Loads are additive until suzume_clear_user_dictionaries() is called.
 *       A failed load leaves earlier dictionaries installed.
 */
SUZUME_EXPORT size_t suzume_load_user_dictionary(suzume_t handle, const char* data, size_t size);

/**
 * @brief Add a binary user dictionary (.dic format) from memory
 * @param handle Suzume handle
 * @param data Binary dictionary bytes. May be NULL only when size is 0.
 * @param size Data size in bytes
 * @return Number of installed entries (at least 1), or 0 on failure with
 *         suzume_last_error() set. SUZUME_ERROR_INVALID_INPUT: a NULL handle,
 *         or NULL data with a non-zero size. SUZUME_ERROR_DICTIONARY_LOAD_FAILED:
 *         every other failure, including a zero-length source ("Empty
 *         dictionary data") and a malformed or zero-entry dictionary.
 * @note Loads are additive. A failed load leaves earlier dictionaries installed.
 */
SUZUME_EXPORT size_t suzume_load_binary_dictionary(suzume_t handle, const uint8_t* data, size_t size);

/**
 * @brief Remove user dictionaries explicitly loaded through this handle
 *
 * The automatically loaded bundled user dictionary remains installed.
 * @return 1 on success. 0 on failure: SUZUME_ERROR_INVALID_INPUT for a NULL
 *         handle ("null handle"), or SUZUME_ERROR_OUT_OF_MEMORY /
 *         SUZUME_ERROR_INTERNAL for an internal failure.
 */
SUZUME_EXPORT int suzume_clear_user_dictionaries(suzume_t handle);

/**
 * @brief Check whether the handle has a loaded L2 core binary dictionary
 * @return 1 when loaded, 0 when absent. 0 also for a NULL handle, which sets
 *         SUZUME_ERROR_INVALID_INPUT ("null handle"); a successful call leaves
 *         suzume_last_error() untouched.
 */
SUZUME_EXPORT int suzume_has_core_dictionary(suzume_t handle);

// --- Utility functions ---

/**
 * @brief Get Suzume version string
 * @return Version string (static, do not free). Always succeeds and does not
 *         touch suzume_last_error().
 */
SUZUME_EXPORT const char* suzume_version(void);

/**
 * @brief Get the last C API error message for the current thread/runtime
 * @return Borrowed thread-local string, always a valid C string; "" when the
 *         last call succeeded. Do not free. It stays valid until a later C API call
 *         clears or replaces the diagnostic, or the thread ends. Its code is
 *         suzume_last_error_code(); reading it never changes either.
 */
SUZUME_EXPORT const char* suzume_last_error(void);

/**
 * @brief Get the stable code for the last C API error on this thread/runtime
 * @return A SUZUME_ERROR_* value; SUZUME_ERROR_SUCCESS when the last call that
 *         touches the diagnostic succeeded. Never returns 255.
 */
SUZUME_EXPORT suzume_error_code_t suzume_last_error_code(void);

/**
 * @brief Get the Japanese label for a serialized conjugation type
 * @return Static string, never NULL: "" for 0 (none) and for any code outside
 *         the enum. Always succeeds and does not touch suzume_last_error() (no
 *         SUZUME_ERROR_* code).
 */
SUZUME_EXPORT const char* suzume_conjugation_type_label(suzume_conjugation_type_t code);

/**
 * @brief Get the stable label for a serialized extended POS code
 * @return Static string, never NULL: "UNKNOWN" for any code outside the enum
 *         (the same label as code 0). Always succeeds and does not touch
 *         suzume_last_error() (no SUZUME_ERROR_* code).
 */
SUZUME_EXPORT const char* suzume_extended_pos_label(suzume_extended_pos_t code);

/**
 * @brief Get the Japanese label for a serialized conjugation form code
 * @return Static string, never NULL: "" for any code outside the enum. Code 0
 *         is the terminal form; whether a morpheme conjugates at all is given
 *         by SUZUME_MORPHEME_CONJUGATABLE, not by the form code. Always succeeds and does not
 *         touch suzume_last_error() (no SUZUME_ERROR_* code).
 */
SUZUME_EXPORT const char* suzume_conjugation_form_label(suzume_conjugation_form_t code);

/**
 * @brief Get the stable English label for a serialized POS code (e.g. "NOUN");
 *        suzume_pos_label_ja() gives the Japanese label
 * @return Static string, never NULL: "OTHER" for any code outside the enum
 *         (the same label as SUZUME_POS_OTHER). Always succeeds and does not
 *         touch suzume_last_error() (no SUZUME_ERROR_* code).
 */
SUZUME_EXPORT const char* suzume_pos_label(suzume_pos_t code);

/**
 * @brief Get the Japanese label for a serialized POS code
 * @return Static string, never NULL: "その他" for SUZUME_POS_UNKNOWN,
 *         SUZUME_POS_OTHER and any code outside the enum. Always succeeds and
 *         does not touch suzume_last_error() (no SUZUME_ERROR_* code).
 */
SUZUME_EXPORT const char* suzume_pos_label_ja(suzume_pos_t code);

/**
 * @brief Get number of dictionary warnings from auto-loading dictionaries
 * @param handle Suzume handle
 * @return Warning count. 0 also for a NULL handle, which sets
 *         SUZUME_ERROR_INVALID_INPUT ("null handle"); a successful call leaves
 *         suzume_last_error() untouched.
 */
SUZUME_EXPORT size_t suzume_dictionary_warning_count(suzume_t handle);

/**
 * @brief Get dictionary warning message by index
 * @param handle Suzume handle
 * @param index Warning index
 * @return Warning string, never "". NULL on failure with
 *         SUZUME_ERROR_INVALID_INPUT: a NULL handle ("null handle") or an index
 *         at or past suzume_dictionary_warning_count() ("index out of range").
 *         The string is a thread-local copy owned by Suzume; do not free it. It
 *         is valid only until the next suzume_dictionary_warning call from the
 *         same thread or until the handle is destroyed, whichever comes first;
 *         copy it to retain it.
 */
SUZUME_EXPORT const char* suzume_dictionary_warning(suzume_t handle, size_t index);

/**
 * @brief Get sizeof(suzume_result_t)
 * @return The size in bytes the library was built with; always succeeds
 */
SUZUME_EXPORT size_t suzume_sizeof_result(void);

/**
 * @brief Get sizeof(suzume_morpheme_t)
 * @return The size in bytes the library was built with; always succeeds
 */
SUZUME_EXPORT size_t suzume_sizeof_morpheme(void);

/**
 * @brief Get sizeof(suzume_tags_t)
 * @return The size in bytes the library was built with; always succeeds
 */
SUZUME_EXPORT size_t suzume_sizeof_tags(void);

/**
 * @brief Get sizeof(suzume_tag_options_t)
 * @return The size in bytes the library was built with; always succeeds
 */
SUZUME_EXPORT size_t suzume_sizeof_tag_options(void);

/**
 * @brief Get sizeof(suzume_options_t)
 * @return The size in bytes the library was built with; always succeeds
 */
SUZUME_EXPORT size_t suzume_sizeof_options(void);

/** Field indexes for suzume_offsetof_result(). */
enum {
  SUZUME_RESULT_FIELD_MORPHEMES = 0,
  SUZUME_RESULT_FIELD_COUNT = 1,
  SUZUME_RESULT_FIELD_NORMALIZED_TEXT = 2,
  SUZUME_RESULT_FIELD_NORMALIZED_TEXT_SIZE = 3,
};

/**
 * @brief Get byte offset of field in suzume_result_t
 * @param field A SUZUME_RESULT_FIELD_* index
 * @return The byte offset, or (size_t)-1 for an unknown index
 */
SUZUME_EXPORT size_t suzume_offsetof_result(uint32_t field);

/** Field indexes for suzume_offsetof_morpheme(). */
enum {
  SUZUME_MORPHEME_FIELD_SURFACE = 0,
  SUZUME_MORPHEME_FIELD_BASE_FORM = 1,
  SUZUME_MORPHEME_FIELD_START = 2,
  SUZUME_MORPHEME_FIELD_END = 3,
  SUZUME_MORPHEME_FIELD_SCORE = 4,
  SUZUME_MORPHEME_FIELD_POS = 5,
  SUZUME_MORPHEME_FIELD_EXTENDED_POS = 6,
  SUZUME_MORPHEME_FIELD_CONJUGATION_TYPE = 7,
  SUZUME_MORPHEME_FIELD_CONJUGATION_FORM = 8,
  SUZUME_MORPHEME_FIELD_FLAGS = 9,
  SUZUME_MORPHEME_FIELD_SURFACE_SIZE = 10,
  SUZUME_MORPHEME_FIELD_BASE_FORM_SIZE = 11,
};

/**
 * @brief Get byte offset of field in suzume_morpheme_t
 * @param field A SUZUME_MORPHEME_FIELD_* index
 * @return The byte offset, or (size_t)-1 for an unknown index
 */
SUZUME_EXPORT size_t suzume_offsetof_morpheme(uint32_t field);

/** Field indexes for suzume_offsetof_tags(). */
enum {
  SUZUME_TAGS_FIELD_TAGS = 0,
  SUZUME_TAGS_FIELD_POS = 1,
  SUZUME_TAGS_FIELD_COUNT = 2,
};

/**
 * @brief Get byte offset of field in suzume_tags_t
 * @param field A SUZUME_TAGS_FIELD_* index
 * @return The byte offset, or (size_t)-1 for an unknown index
 */
SUZUME_EXPORT size_t suzume_offsetof_tags(uint32_t field);

/** Field indexes for suzume_offsetof_tag_options(). */
enum {
  SUZUME_TAG_OPTIONS_FIELD_POS_FILTER = 0,
  SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_BASIC = 1,
  SUZUME_TAG_OPTIONS_FIELD_USE_BASE_FORM = 2,
  SUZUME_TAG_OPTIONS_FIELD_MIN_LENGTH = 3,
  SUZUME_TAG_OPTIONS_FIELD_MAX_TAGS = 4,
  SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_PARTICLES = 5,
  SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_AUXILIARIES = 6,
  SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_FORMAL_NOUNS = 7,
  SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_LOW_INFO = 8,
  SUZUME_TAG_OPTIONS_FIELD_REMOVE_DUPLICATES = 9,
};

/**
 * @brief Get byte offset of field in suzume_tag_options_t
 * @param field A SUZUME_TAG_OPTIONS_FIELD_* index
 * @return The byte offset, or (size_t)-1 for an unknown index
 */
SUZUME_EXPORT size_t suzume_offsetof_tag_options(uint32_t field);

/** Field indexes for suzume_offsetof_options(). */
enum {
  SUZUME_OPTIONS_FIELD_PRESERVE_VU = 0,
  SUZUME_OPTIONS_FIELD_PRESERVE_CASE = 1,
  SUZUME_OPTIONS_FIELD_PRESERVE_SYMBOLS = 2,
  SUZUME_OPTIONS_FIELD_MODE = 3,
  SUZUME_OPTIONS_FIELD_LEMMATIZE = 4,
  SUZUME_OPTIONS_FIELD_MERGE_COMPOUNDS = 5,
  SUZUME_OPTIONS_FIELD_SKIP_BUNDLED_USER_DICTIONARY = 6,
  SUZUME_OPTIONS_FIELD_SKIP_CORE_DICTIONARY = 7,
  SUZUME_OPTIONS_FIELD_REPORT_SCORER_CONFIG = 8,
  SUZUME_OPTIONS_FIELD_SKIP_ENV_CONFIG = 9,
  SUZUME_OPTIONS_FIELD_SCORER_OPTIONS_JSON = 10,
  SUZUME_OPTIONS_FIELD_DATA_DIRECTORY = 11,
};

/**
 * @brief Get byte offset of field in suzume_options_t
 * @param field A SUZUME_OPTIONS_FIELD_* index
 * @return The byte offset, or (size_t)-1 for an unknown index
 */
SUZUME_EXPORT size_t suzume_offsetof_options(uint32_t field);

#ifdef __cplusplus
}
#endif

#endif  // SUZUME_SUZUME_C_H_
