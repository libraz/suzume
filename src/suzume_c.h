/**
 * @file suzume_c.h
 * @brief C API for Suzume Japanese morphological analyzer
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

#ifdef __EMSCRIPTEN__
#define SUZUME_EXPORT __attribute__((used))
#else
#define SUZUME_EXPORT
#endif

/**
 * @brief Opaque handle to Suzume instance
 */
typedef struct SuzumeHandle* suzume_t;

/**
 * @brief Morpheme data structure
 */
typedef struct {
  const char* surface;      /**< Surface form (UTF-8) */
  const char* pos;          /**< Part of speech (English) */
  const char* base_form;    /**< Base/dictionary form */
  const char* pos_ja;       /**< Part of speech (Japanese) */
  const char* conj_type;    /**< Conjugation type (Japanese) */
  const char* conj_form;    /**< Conjugation form (Japanese) */
  const char* extended_pos; /**< Extended POS (English, e.g. "VerbRenyokei") */
} suzume_morpheme_t;

/**
 * @brief Analysis result structure
 */
typedef struct {
  suzume_morpheme_t* morphemes; /**< Array of morphemes */
  size_t count;                 /**< Number of morphemes */
} suzume_result_t;

/**
 * @brief Tag generation result structure
 */
typedef struct {
  char** tags;      /**< Array of tag strings */
  const char** pos; /**< Array of POS strings (English, e.g. "NOUN", "VERB") */
  size_t count;     /**< Number of tags */
} suzume_tags_t;

/**
 * @brief Analysis options structure
 */
typedef struct {
  int preserve_vu;      /**< Preserve ヴ (don't normalize to ビ etc.) */
  int preserve_case;    /**< Preserve case (don't lowercase ASCII) */
  int preserve_symbols; /**< Preserve symbols/emoji (don't remove from output) */
} suzume_options_t;

// --- Lifecycle functions ---

/**
 * @brief Create a new Suzume instance with default options
 * @return Handle to Suzume instance, or NULL on failure
 */
SUZUME_EXPORT suzume_t suzume_create(void);

/**
 * @brief Create a new Suzume instance with options
 * @param options Pointer to options structure
 * @return Handle to Suzume instance, or NULL on failure
 */
SUZUME_EXPORT suzume_t suzume_create_with_options(const suzume_options_t* options);

/**
 * @brief Destroy Suzume instance and free resources
 * @param handle Suzume handle
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_destroy(suzume_t handle);

// --- Analysis functions ---

/**
 * @brief Analyze Japanese text into morphemes
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @return Analysis result allocated by Suzume, or NULL on failure.
 *         Non-NULL results must be freed exactly once with suzume_result_free.
 */
SUZUME_EXPORT suzume_result_t* suzume_analyze(suzume_t handle, const char* text);

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
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags(suzume_t handle, const char* text);

/**
 * @brief Tag generation options
 */
typedef struct {
  uint8_t pos_filter; /**< POS bitmask: 1=noun, 2=verb, 4=adjective, 8=adverb (0=all) */
  int exclude_basic;  /**< Exclude basic words (hiragana-only lemma) */
  int use_lemma;      /**< Use lemma instead of surface (default: 1) */
  size_t min_length;  /**< Minimum tag length in characters (default: 2) */
  size_t max_tags;    /**< Maximum number of tags (0=unlimited) */
} suzume_tag_options_t;

/**
 * @brief Generate tags from Japanese text with options
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @param options Tag generation options
 * @return Tags result allocated by Suzume, or NULL on failure.
 *         Non-NULL results must be freed exactly once with suzume_tags_free.
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_with_options(suzume_t handle, const char* text,
                                                               const suzume_tag_options_t* options);

/**
 * @brief Free tags result
 * @param tags Tags to free
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_tags_free(suzume_tags_t* tags);

// --- Dictionary functions ---

/**
 * @brief Load user dictionary from memory
 * @param handle Suzume handle
 * @param data Dictionary data (CSV format)
 * @param size Data size in bytes
 * @return 1 on success, 0 on failure
 */
SUZUME_EXPORT int suzume_load_user_dict(suzume_t handle, const char* data, size_t size);

/**
 * @brief Load binary dictionary from memory (as user dictionary)
 * @param handle Suzume handle
 * @param data Binary dictionary data (.dic format)
 * @param size Data size in bytes
 * @return 1 on success, 0 on failure
 */
SUZUME_EXPORT int suzume_load_binary_dict(suzume_t handle, const uint8_t* data, size_t size);

// --- Utility functions ---

/**
 * @brief Get Suzume version string
 * @return Version string (static, do not free)
 */
SUZUME_EXPORT const char* suzume_version(void);

/**
 * @brief Get the last C API error message for the current thread/runtime
 * @return Last error string (static/thread-local, do not free)
 */
SUZUME_EXPORT const char* suzume_last_error(void);

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
 * @brief Get byte offset of field in suzume_result_t
 * @param field 0=morphemes, 1=count
 */
SUZUME_EXPORT size_t suzume_offsetof_result(uint32_t field);

/**
 * @brief Get byte offset of field in suzume_morpheme_t
 * @param field 0=surface, 1=pos, 2=base_form, 3=pos_ja,
 *              4=conj_type, 5=conj_form, 6=extended_pos
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
 *              3=min_length, 4=max_tags
 */
SUZUME_EXPORT size_t suzume_offsetof_tag_options(uint32_t field);

/**
 * @brief Allocate memory (for WASM interop)
 * @param size Size in bytes
 * @return Pointer to allocated memory
 */
SUZUME_EXPORT void* suzume_malloc(size_t size);

/**
 * @brief Free memory (for WASM interop)
 * @param ptr Pointer to free
 * @note Passing NULL is allowed and has no effect.
 */
SUZUME_EXPORT void suzume_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif  // SUZUME_SUZUME_C_H_
