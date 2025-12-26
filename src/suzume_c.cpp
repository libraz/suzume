/**
 * @file suzume_c.cpp
 * @brief C API implementation for Suzume
 */

#include "suzume_c.h"

#include <cstdlib>
#include <cstring>
#include <new>

#include "suzume.h"

// Internal handle structure
struct SuzumeHandle {
  suzume::Suzume instance;

  SuzumeHandle() : instance() {}
};

extern "C" {

SUZUME_EXPORT suzume_t suzume_create(void) {
  try {
    return new SuzumeHandle();
  } catch (...) {
    return nullptr;
  }
}

SUZUME_EXPORT void suzume_destroy(suzume_t handle) {
  delete handle;
}

SUZUME_EXPORT suzume_result_t* suzume_analyze(suzume_t handle,
                                               const char* text) {
  if (handle == nullptr || text == nullptr) {
    return nullptr;
  }

  try {
    auto morphemes = handle->instance.analyze(text);

    auto* result = new suzume_result_t();
    result->count = morphemes.size();

    if (result->count == 0) {
      result->morphemes = nullptr;
      return result;
    }

    result->morphemes = new suzume_morpheme_t[result->count];

    for (size_t idx = 0; idx < result->count; ++idx) {
      const auto& morph = morphemes[idx];

      // Allocate and copy strings
      auto* surface = new char[morph.surface.size() + 1];
      std::strcpy(surface, morph.surface.c_str());
      result->morphemes[idx].surface = surface;

      auto pos_str = suzume::core::posToString(morph.pos);
      auto* pos = new char[pos_str.size() + 1];
      std::memcpy(pos, pos_str.data(), pos_str.size());
      pos[pos_str.size()] = '\0';
      result->morphemes[idx].pos = pos;

      auto lemma = morph.getLemma();
      auto* base_form = new char[lemma.size() + 1];
      std::memcpy(base_form, lemma.data(), lemma.size());
      base_form[lemma.size()] = '\0';
      result->morphemes[idx].base_form = base_form;

      auto* reading = new char[morph.reading.size() + 1];
      std::strcpy(reading, morph.reading.c_str());
      result->morphemes[idx].reading = reading;
    }

    return result;
  } catch (...) {
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
      delete[] const_cast<char*>(result->morphemes[idx].reading);
    }
    delete[] result->morphemes;
  }

  delete result;
}

SUZUME_EXPORT suzume_tags_t* suzume_generate_tags(suzume_t handle,
                                                   const char* text) {
  if (handle == nullptr || text == nullptr) {
    return nullptr;
  }

  try {
    auto tags = handle->instance.generateTags(text);

    auto* result = new suzume_tags_t();
    result->count = tags.size();

    if (result->count == 0) {
      result->tags = nullptr;
      return result;
    }

    result->tags = new char*[result->count];

    for (size_t idx = 0; idx < result->count; ++idx) {
      result->tags[idx] = new char[tags[idx].size() + 1];
      std::strcpy(result->tags[idx], tags[idx].c_str());
    }

    return result;
  } catch (...) {
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

  delete tags;
}

SUZUME_EXPORT int suzume_load_user_dict(suzume_t handle, const char* data,
                                         size_t size) {
  if (handle == nullptr || data == nullptr) {
    return 0;
  }

  try {
    return handle->instance.loadUserDictionaryFromMemory(data, size) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

SUZUME_EXPORT const char* suzume_version(void) {
  static std::string version_str = suzume::Suzume::version();
  return version_str.c_str();
}

SUZUME_EXPORT void* suzume_malloc(size_t size) {
  return std::malloc(size);
}

SUZUME_EXPORT void suzume_free(void* ptr) {
  std::free(ptr);
}

}  // extern "C"
