/**
 * @file suzume_c.cpp
 * @brief C API implementation for Suzume
 */

#include "suzume_c.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>

#include "grammar/conjugation.h"
#include "postprocess/tag_generator.h"
#include "suzume.h"

// Internal handle structure
struct SuzumeHandle {
  suzume::Suzume instance;

  SuzumeHandle() : instance() {}
  explicit SuzumeHandle(const suzume::SuzumeOptions& opts) : instance(opts) {}
};

namespace {

thread_local std::string last_error;

void clearLastError() {
  last_error.clear();
}

void setLastError(std::string_view message) {
  last_error = message;
}

void setLastErrorFromException() {
  try {
    throw;
  } catch (const std::exception& err) {
    setLastError(err.what());
  } catch (...) {
    setLastError("Unknown C API error");
  }
}

char* copyString(std::string_view str) {
  auto* result = new char[str.size() + 1];
  std::memcpy(result, str.data(), str.size());
  result[str.size()] = '\0';
  return result;
}

bool hasExtendedOptionField(const suzume_extended_options_t* options, size_t field_end) {
  return options != nullptr && options->size >= field_end;
}

std::optional<suzume::core::AnalysisMode> parseAnalysisMode(int mode) {
  switch (mode) {
    case 1:
      return suzume::core::AnalysisMode::Search;
    case 2:
      return suzume::core::AnalysisMode::Split;
    case 0:
      return suzume::core::AnalysisMode::Normal;
    default:
      return std::nullopt;
  }
}

}  // namespace

extern "C" {

SUZUME_EXPORT suzume_t suzume_create(void) {
  clearLastError();
  try {
    return new SuzumeHandle();
  } catch (...) {
    setLastErrorFromException();
    return nullptr;
  }
}

SUZUME_EXPORT suzume_t suzume_create_with_options(const suzume_options_t* options) {
  clearLastError();
  try {
    suzume::SuzumeOptions opts;
    if (options != nullptr) {
      opts.normalize_options.preserve_vu = (options->preserve_vu != 0);
      opts.normalize_options.preserve_case = (options->preserve_case != 0);
      opts.remove_symbols = (options->preserve_symbols == 0);
    }
    return new SuzumeHandle(opts);
  } catch (...) {
    setLastErrorFromException();
    return nullptr;
  }
}

SUZUME_EXPORT void suzume_init_extended_options(suzume_extended_options_t* options) {
  if (options == nullptr) {
    return;
  }
  options->size = sizeof(suzume_extended_options_t);
  options->preserve_vu = 1;
  options->preserve_case = 1;
  options->preserve_symbols = 0;
  options->mode = 0;
  options->lemmatize = 1;
  options->merge_compounds = 0;
}

SUZUME_EXPORT suzume_t suzume_create_with_extended_options(const suzume_extended_options_t* options) {
  clearLastError();
  try {
    suzume::SuzumeOptions opts;
    if (options != nullptr) {
      if (hasExtendedOptionField(options,
                                 offsetof(suzume_extended_options_t, preserve_vu) + sizeof(options->preserve_vu))) {
        opts.normalize_options.preserve_vu = (options->preserve_vu != 0);
      }
      if (hasExtendedOptionField(options,
                                 offsetof(suzume_extended_options_t, preserve_case) + sizeof(options->preserve_case))) {
        opts.normalize_options.preserve_case = (options->preserve_case != 0);
      }
      if (hasExtendedOptionField(
              options, offsetof(suzume_extended_options_t, preserve_symbols) + sizeof(options->preserve_symbols))) {
        opts.remove_symbols = (options->preserve_symbols == 0);
      }
      if (hasExtendedOptionField(options, offsetof(suzume_extended_options_t, mode) + sizeof(options->mode))) {
        auto mode = parseAnalysisMode(options->mode);
        if (!mode.has_value()) {
          setLastError("suzume_create_with_extended_options: invalid mode");
          return nullptr;
        }
        opts.mode = *mode;
      }
      if (hasExtendedOptionField(options,
                                 offsetof(suzume_extended_options_t, lemmatize) + sizeof(options->lemmatize))) {
        opts.lemmatize = (options->lemmatize != 0);
      }
      if (hasExtendedOptionField(
              options, offsetof(suzume_extended_options_t, merge_compounds) + sizeof(options->merge_compounds))) {
        opts.merge_compounds = (options->merge_compounds != 0);
      }
    }
    return new SuzumeHandle(opts);
  } catch (...) {
    setLastErrorFromException();
    return nullptr;
  }
}

SUZUME_EXPORT void suzume_destroy(suzume_t handle) {
  delete handle;
}

SUZUME_EXPORT suzume_result_t* suzume_analyze(suzume_t handle, const char* text) {
  if (handle == nullptr || text == nullptr) {
    setLastError("suzume_analyze: null handle or text");
    return nullptr;
  }

  clearLastError();
  try {
    auto morphemes = handle->instance.analyze(text);

    std::unique_ptr<suzume_result_t, decltype(&suzume_result_free)> result(new suzume_result_t(), suzume_result_free);
    result->count = morphemes.size();

    if (result->count == 0) {
      result->morphemes = nullptr;
      return result.release();
    }

    result->morphemes = new suzume_morpheme_t[result->count]{};

    for (size_t idx = 0; idx < result->count; ++idx) {
      const auto& morph = morphemes[idx];

      // Allocate and copy strings
      result->morphemes[idx].surface = copyString(morph.surface);

      auto pos_str = suzume::core::posToString(morph.pos);
      result->morphemes[idx].pos = copyString(pos_str);

      auto lemma = morph.getLemma();
      result->morphemes[idx].base_form = copyString(lemma);

      // Japanese POS
      auto pos_ja_str = suzume::core::posToJapanese(morph.pos);
      result->morphemes[idx].pos_ja = copyString(pos_ja_str);

      // Conjugation type and form (for verbs and adjectives)
      if (morph.pos == suzume::core::PartOfSpeech::Verb || morph.pos == suzume::core::PartOfSpeech::Adjective) {
        auto verb_type = suzume::grammar::conjTypeToVerbType(morph.conj_type);
        auto conj_type_str = suzume::grammar::verbTypeToJapanese(verb_type);
        result->morphemes[idx].conj_type = copyString(conj_type_str);

        auto conj_form_str = suzume::grammar::conjFormToJapanese(morph.conj_form);
        result->morphemes[idx].conj_form = copyString(conj_form_str);
      } else {
        result->morphemes[idx].conj_type = nullptr;
        result->morphemes[idx].conj_form = nullptr;
      }

      // Extended POS
      auto epos_str = suzume::core::extendedPosToString(morph.extended_pos);
      result->morphemes[idx].extended_pos = copyString(epos_str);
    }

    return result.release();
  } catch (...) {
    setLastErrorFromException();
    return nullptr;
  }
}

SUZUME_EXPORT void suzume_result_free(suzume_result_t* result) {
  if (result == nullptr) {
    return;
  }

  if (result->morphemes != nullptr) {
    for (size_t idx = 0; idx < result->count; ++idx) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      delete[] const_cast<char*>(result->morphemes[idx].surface);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      delete[] const_cast<char*>(result->morphemes[idx].pos);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      delete[] const_cast<char*>(result->morphemes[idx].base_form);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      delete[] const_cast<char*>(result->morphemes[idx].pos_ja);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      delete[] const_cast<char*>(result->morphemes[idx].conj_type);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      delete[] const_cast<char*>(result->morphemes[idx].conj_form);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      delete[] const_cast<char*>(result->morphemes[idx].extended_pos);
    }
    delete[] result->morphemes;
  }

  delete result;
}

SUZUME_EXPORT suzume_tags_t* suzume_generate_tags(suzume_t handle, const char* text) {
  if (handle == nullptr || text == nullptr) {
    setLastError("suzume_generate_tags: null handle or text");
    return nullptr;
  }

  clearLastError();
  try {
    auto tags = handle->instance.generateTags(text);

    std::unique_ptr<suzume_tags_t, decltype(&suzume_tags_free)> result(new suzume_tags_t(), suzume_tags_free);
    result->count = tags.size();

    if (result->count == 0) {
      result->tags = nullptr;
      result->pos = nullptr;
      return result.release();
    }

    result->tags = new char* [result->count] {};
    result->pos = new const char* [result->count] {};

    for (size_t idx = 0; idx < result->count; ++idx) {
      result->tags[idx] = copyString(tags[idx].tag);

      auto pos_str = suzume::core::posToString(tags[idx].pos);
      result->pos[idx] = copyString(pos_str);
    }

    return result.release();
  } catch (...) {
    setLastErrorFromException();
    return nullptr;
  }
}

SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_with_options(suzume_t handle, const char* text,
                                                               const suzume_tag_options_t* options) {
  if (handle == nullptr || text == nullptr || options == nullptr) {
    setLastError("suzume_generate_tags_with_options: null handle, text, or options");
    return nullptr;
  }

  clearLastError();
  try {
    suzume::postprocess::TagGeneratorOptions tag_opts;
    tag_opts.pos_filter = options->pos_filter;
    tag_opts.exclude_basic = (options->exclude_basic != 0);
    tag_opts.use_lemma = (options->use_lemma != 0);
    tag_opts.min_tag_length = options->min_length;
    tag_opts.max_tags = options->max_tags;

    auto tags = handle->instance.generateTags(text, tag_opts);

    std::unique_ptr<suzume_tags_t, decltype(&suzume_tags_free)> result(new suzume_tags_t(), suzume_tags_free);
    result->count = tags.size();

    if (result->count == 0) {
      result->tags = nullptr;
      result->pos = nullptr;
      return result.release();
    }

    result->tags = new char* [result->count] {};
    result->pos = new const char* [result->count] {};

    for (size_t idx = 0; idx < result->count; ++idx) {
      result->tags[idx] = copyString(tags[idx].tag);

      auto pos_str = suzume::core::posToString(tags[idx].pos);
      result->pos[idx] = copyString(pos_str);
    }

    return result.release();
  } catch (...) {
    setLastErrorFromException();
    return nullptr;
  }
}

SUZUME_EXPORT void suzume_tags_free(suzume_tags_t* tags) {
  if (tags == nullptr) {
    return;
  }

  if (tags->tags != nullptr) {
    for (size_t idx = 0; idx < tags->count; ++idx) {
      delete[] tags->tags[idx];
    }
    delete[] tags->tags;
  }

  if (tags->pos != nullptr) {
    for (size_t idx = 0; idx < tags->count; ++idx) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      delete[] const_cast<char*>(tags->pos[idx]);
    }
    delete[] tags->pos;
  }

  delete tags;
}

SUZUME_EXPORT int suzume_load_user_dict(suzume_t handle, const char* data, size_t size) {
  if (handle == nullptr || data == nullptr) {
    setLastError("suzume_load_user_dict: null handle or data");
    return 0;
  }

  clearLastError();
  try {
    auto result = handle->instance.loadUserDictionaryFromMemoryResult(data, size);
    if (result.hasValue()) {
      return 1;
    }
    setLastError(result.error().message);
    return 0;
  } catch (...) {
    setLastErrorFromException();
    return 0;
  }
}

SUZUME_EXPORT int suzume_load_binary_dict(suzume_t handle, const uint8_t* data, size_t size) {
  if (handle == nullptr || data == nullptr) {
    setLastError("suzume_load_binary_dict: null handle or data");
    return 0;
  }

  clearLastError();
  try {
    auto result = handle->instance.loadBinaryDictionaryResult(data, size);
    if (result.hasValue()) {
      return 1;
    }
    setLastError(result.error().message);
    return 0;
  } catch (...) {
    setLastErrorFromException();
    return 0;
  }
}

SUZUME_EXPORT const char* suzume_version(void) {
  static std::string version_str = suzume::Suzume::version();
  return version_str.c_str();
}

SUZUME_EXPORT const char* suzume_last_error(void) {
  return last_error.c_str();
}

SUZUME_EXPORT size_t suzume_sizeof_result(void) {
  return sizeof(suzume_result_t);
}

SUZUME_EXPORT size_t suzume_sizeof_morpheme(void) {
  return sizeof(suzume_morpheme_t);
}

SUZUME_EXPORT size_t suzume_sizeof_tags(void) {
  return sizeof(suzume_tags_t);
}

SUZUME_EXPORT size_t suzume_sizeof_tag_options(void) {
  return sizeof(suzume_tag_options_t);
}

SUZUME_EXPORT size_t suzume_sizeof_extended_options(void) {
  return sizeof(suzume_extended_options_t);
}

SUZUME_EXPORT size_t suzume_offsetof_result(uint32_t field) {
  switch (field) {
    case 0:
      return offsetof(suzume_result_t, morphemes);
    case 1:
      return offsetof(suzume_result_t, count);
    default:
      return static_cast<size_t>(-1);
  }
}

SUZUME_EXPORT size_t suzume_offsetof_morpheme(uint32_t field) {
  switch (field) {
    case 0:
      return offsetof(suzume_morpheme_t, surface);
    case 1:
      return offsetof(suzume_morpheme_t, pos);
    case 2:
      return offsetof(suzume_morpheme_t, base_form);
    case 3:
      return offsetof(suzume_morpheme_t, pos_ja);
    case 4:
      return offsetof(suzume_morpheme_t, conj_type);
    case 5:
      return offsetof(suzume_morpheme_t, conj_form);
    case 6:
      return offsetof(suzume_morpheme_t, extended_pos);
    default:
      return static_cast<size_t>(-1);
  }
}

SUZUME_EXPORT size_t suzume_offsetof_tags(uint32_t field) {
  switch (field) {
    case 0:
      return offsetof(suzume_tags_t, tags);
    case 1:
      return offsetof(suzume_tags_t, pos);
    case 2:
      return offsetof(suzume_tags_t, count);
    default:
      return static_cast<size_t>(-1);
  }
}

SUZUME_EXPORT size_t suzume_offsetof_tag_options(uint32_t field) {
  switch (field) {
    case 0:
      return offsetof(suzume_tag_options_t, pos_filter);
    case 1:
      return offsetof(suzume_tag_options_t, exclude_basic);
    case 2:
      return offsetof(suzume_tag_options_t, use_lemma);
    case 3:
      return offsetof(suzume_tag_options_t, min_length);
    case 4:
      return offsetof(suzume_tag_options_t, max_tags);
    default:
      return static_cast<size_t>(-1);
  }
}

SUZUME_EXPORT size_t suzume_offsetof_extended_options(uint32_t field) {
  switch (field) {
    case 0:
      return offsetof(suzume_extended_options_t, size);
    case 1:
      return offsetof(suzume_extended_options_t, preserve_vu);
    case 2:
      return offsetof(suzume_extended_options_t, preserve_case);
    case 3:
      return offsetof(suzume_extended_options_t, preserve_symbols);
    case 4:
      return offsetof(suzume_extended_options_t, mode);
    case 5:
      return offsetof(suzume_extended_options_t, lemmatize);
    case 6:
      return offsetof(suzume_extended_options_t, merge_compounds);
    default:
      return static_cast<size_t>(-1);
  }
}

SUZUME_EXPORT void* suzume_malloc(size_t size) {
  return std::malloc(size);
}

SUZUME_EXPORT void suzume_free(void* ptr) {
  std::free(ptr);
}

}  // extern "C"
