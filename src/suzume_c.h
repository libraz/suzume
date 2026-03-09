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
  const char* surface;    /**< Surface form (UTF-8) */
  const char* pos;        /**< Part of speech (English) */
  const char* base_form;  /**< Base/dictionary form */
  const char* reading;    /**< Reading in katakana */
  const char* pos_ja;     /**< Part of speech (Japanese) */
  const char* conj_type;  /**< Conjugation type (Japanese) */
  const char* conj_form;  /**< Conjugation form (Japanese) */
} suzume_morpheme_t;

/**
 * @brief Analysis result structure
 */
typedef struct {
  suzume_morpheme_t* morphemes;  /**< Array of morphemes */
  size_t count;                  /**< Number of morphemes */
} suzume_result_t;

/**
 * @brief Tag generation result structure
 */
typedef struct {
  char** tags;   /**< Array of tag strings */
  size_t count;  /**< Number of tags */
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
 */
SUZUME_EXPORT void suzume_destroy(suzume_t handle);

// --- Analysis functions ---

/**
 * @brief Analyze Japanese text into morphemes
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @return Analysis result (must be freed with suzume_result_free)
 */
SUZUME_EXPORT suzume_result_t* suzume_analyze(suzume_t handle, const char* text);

/**
 * @brief Free analysis result
 * @param result Result to free
 */
SUZUME_EXPORT void suzume_result_free(suzume_result_t* result);

/**
 * @brief Generate tags from Japanese text
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @return Tags result (must be freed with suzume_tags_free)
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags(suzume_t handle,
                                                   const char* text);

/**
 * @brief Tag generation options
 */
typedef struct {
  uint8_t pos_filter;     /**< POS bitmask: 1=noun, 2=verb, 4=adjective, 8=adverb (0=all) */
  int exclude_basic;      /**< Exclude basic words (hiragana-only lemma) */
  int use_lemma;          /**< Use lemma instead of surface (default: 1) */
  size_t min_length;      /**< Minimum tag length in characters (default: 2) */
  size_t max_tags;        /**< Maximum number of tags (0=unlimited) */
} suzume_tag_options_t;

/**
 * @brief Generate tags from Japanese text with options
 * @param handle Suzume handle
 * @param text UTF-8 encoded Japanese text
 * @param options Tag generation options
 * @return Tags result (must be freed with suzume_tags_free)
 */
SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_with_options(
    suzume_t handle, const char* text, const suzume_tag_options_t* options);

/**
 * @brief Free tags result
 * @param tags Tags to free
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
SUZUME_EXPORT int suzume_load_user_dict(suzume_t handle, const char* data,
                                         size_t size);

/**
 * @brief Load binary dictionary from memory (as user dictionary)
 * @param handle Suzume handle
 * @param data Binary dictionary data (.dic format)
 * @param size Data size in bytes
 * @return 1 on success, 0 on failure
 */
SUZUME_EXPORT int suzume_load_binary_dict(suzume_t handle, const uint8_t* data,
                                           size_t size);

// --- Utility functions ---

/**
 * @brief Get Suzume version string
 * @return Version string (static, do not free)
 */
SUZUME_EXPORT const char* suzume_version(void);

/**
 * @brief Allocate memory (for WASM interop)
 * @param size Size in bytes
 * @return Pointer to allocated memory
 */
SUZUME_EXPORT void* suzume_malloc(size_t size);

/**
 * @brief Free memory (for WASM interop)
 * @param ptr Pointer to free
 */
SUZUME_EXPORT void suzume_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif  // SUZUME_SUZUME_C_H_
