/**
 * @file suzume_c.cpp
 * @brief C API implementation for Suzume
 */

#include "suzume/suzume_c.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <optional>
#include <string>
#include <string_view>

#include "analysis/scorer_options_loader.h"
#include "normalize/utf8.h"
#include "postprocess/tag_generator.h"
#include "suzume.h"

// Internal handle structure
struct SuzumeHandle {
  suzume::Suzume instance;

  SuzumeHandle() : instance() {}
  explicit SuzumeHandle(const suzume::SuzumeOptions& opts) : instance(opts) {}
};

namespace {

constexpr std::array<suzume::core::ExtendedPOS, 85> kSerializedExtendedPos = {
    suzume::core::ExtendedPOS::Unknown,
    suzume::core::ExtendedPOS::VerbShuushikei,
    suzume::core::ExtendedPOS::VerbRenyokei,
    suzume::core::ExtendedPOS::VerbMizenkei,
    suzume::core::ExtendedPOS::VerbOnbinkei,
    suzume::core::ExtendedPOS::VerbTeForm,
    suzume::core::ExtendedPOS::VerbKateikei,
    suzume::core::ExtendedPOS::VerbMeireikei,
    suzume::core::ExtendedPOS::VerbRentaikei,
    suzume::core::ExtendedPOS::VerbTaForm,
    suzume::core::ExtendedPOS::VerbTaraForm,
    suzume::core::ExtendedPOS::AdjBasic,
    suzume::core::ExtendedPOS::AdjRenyokei,
    suzume::core::ExtendedPOS::AdjStem,
    suzume::core::ExtendedPOS::AdjKatt,
    suzume::core::ExtendedPOS::AdjKeForm,
    suzume::core::ExtendedPOS::AdjNaAdj,
    suzume::core::ExtendedPOS::AuxTenseTa,
    suzume::core::ExtendedPOS::AuxTenseMasu,
    suzume::core::ExtendedPOS::AuxNegativeNai,
    suzume::core::ExtendedPOS::AuxNegativeNu,
    suzume::core::ExtendedPOS::AuxDesireTai,
    suzume::core::ExtendedPOS::AuxVolitional,
    suzume::core::ExtendedPOS::AuxPassive,
    suzume::core::ExtendedPOS::AuxCausative,
    suzume::core::ExtendedPOS::AuxPotential,
    suzume::core::ExtendedPOS::AuxAspectIru,
    suzume::core::ExtendedPOS::AuxAspectShimau,
    suzume::core::ExtendedPOS::AuxAspectOku,
    suzume::core::ExtendedPOS::AuxAspectMiru,
    suzume::core::ExtendedPOS::AuxAspectIku,
    suzume::core::ExtendedPOS::AuxAspectKuru,
    suzume::core::ExtendedPOS::AuxAspectHajimeru,
    suzume::core::ExtendedPOS::AuxAppearanceSou,
    suzume::core::ExtendedPOS::AuxConjectureRashii,
    suzume::core::ExtendedPOS::AuxConjectureMitai,
    suzume::core::ExtendedPOS::AuxCopulaDa,
    suzume::core::ExtendedPOS::AuxCopulaDesu,
    suzume::core::ExtendedPOS::AuxHonorific,
    suzume::core::ExtendedPOS::AuxGozaru,
    suzume::core::ExtendedPOS::AuxExcessive,
    suzume::core::ExtendedPOS::AuxGaru,
    suzume::core::ExtendedPOS::ParticleCase,
    suzume::core::ExtendedPOS::ParticleTopic,
    suzume::core::ExtendedPOS::ParticleFinal,
    suzume::core::ExtendedPOS::ParticleConj,
    suzume::core::ExtendedPOS::ParticleQuote,
    suzume::core::ExtendedPOS::ParticleAdverbial,
    suzume::core::ExtendedPOS::ParticleNo,
    suzume::core::ExtendedPOS::ParticleBinding,
    suzume::core::ExtendedPOS::Noun,
    suzume::core::ExtendedPOS::NounFormal,
    suzume::core::ExtendedPOS::NounVerbal,
    suzume::core::ExtendedPOS::NounProper,
    suzume::core::ExtendedPOS::NounProperFamily,
    suzume::core::ExtendedPOS::NounProperGiven,
    suzume::core::ExtendedPOS::NounNumber,
    suzume::core::ExtendedPOS::Pronoun,
    suzume::core::ExtendedPOS::PronounInterrogative,
    suzume::core::ExtendedPOS::Adverb,
    suzume::core::ExtendedPOS::AdverbQuotative,
    suzume::core::ExtendedPOS::Conjunction,
    suzume::core::ExtendedPOS::Determiner,
    suzume::core::ExtendedPOS::Prefix,
    suzume::core::ExtendedPOS::Suffix,
    suzume::core::ExtendedPOS::Symbol,
    suzume::core::ExtendedPOS::Interjection,
    suzume::core::ExtendedPOS::Other,
    suzume::core::ExtendedPOS::AdjMizenkei,
    suzume::core::ExtendedPOS::AuxNegativeMai,
    suzume::core::ExtendedPOS::AuxClassicalNari,
    suzume::core::ExtendedPOS::AuxClassicalKeri,
    suzume::core::ExtendedPOS::AuxClassicalTari,
    suzume::core::ExtendedPOS::AuxClassicalPerfect,
    suzume::core::ExtendedPOS::AuxClassicalBeshi,
    suzume::core::ExtendedPOS::AuxInability,
    suzume::core::ExtendedPOS::AuxBenefactive,
    suzume::core::ExtendedPOS::SuffixRecentCompletion,
    suzume::core::ExtendedPOS::SuffixTendency,
    suzume::core::ExtendedPOS::DeterminerQuotative,
    suzume::core::ExtendedPOS::AuxSimilitudeYou,
    suzume::core::ExtendedPOS::AuxKuruwaPolite,
    suzume::core::ExtendedPOS::AuxClassicalKi,
    suzume::core::ExtendedPOS::VerbContractedKateikei,
    suzume::core::ExtendedPOS::ParticleConjFinite,
};

constexpr bool serializedExtendedPosValuesAreStable() {
  for (size_t index = 0; index < kSerializedExtendedPos.size(); ++index) {
    if (static_cast<uint8_t>(kSerializedExtendedPos[index]) != index) {
      return false;
    }
  }
  return true;
}

static_assert(static_cast<uint8_t>(suzume::core::PartOfSpeech::Count_) == 15);
static_assert(static_cast<uint8_t>(suzume::core::ExtendedPOS::Count_) == 85);
static_assert(serializedExtendedPosValuesAreStable());
static_assert(static_cast<uint8_t>(suzume::dictionary::ConjugationType::ProperGiven) == 17);
static_assert(sizeof(suzume_pos_t) == 1);
static_assert(sizeof(suzume_extended_pos_t) == 1);
static_assert(sizeof(suzume_conjugation_type_t) == 1);
static_assert(sizeof(suzume_conjugation_form_t) == 1);
static_assert(sizeof(suzume_error_code_t) == 1);
static_assert(SUZUME_POS_UNKNOWN == static_cast<uint8_t>(suzume::core::PartOfSpeech::Unknown));
static_assert(SUZUME_POS_NOUN == static_cast<uint8_t>(suzume::core::PartOfSpeech::Noun));
static_assert(SUZUME_POS_VERB == static_cast<uint8_t>(suzume::core::PartOfSpeech::Verb));
static_assert(SUZUME_POS_ADJECTIVE == static_cast<uint8_t>(suzume::core::PartOfSpeech::Adjective));
static_assert(SUZUME_POS_ADVERB == static_cast<uint8_t>(suzume::core::PartOfSpeech::Adverb));
static_assert(SUZUME_POS_PARTICLE == static_cast<uint8_t>(suzume::core::PartOfSpeech::Particle));
static_assert(SUZUME_POS_AUXILIARY == static_cast<uint8_t>(suzume::core::PartOfSpeech::Auxiliary));
static_assert(SUZUME_POS_CONJUNCTION == static_cast<uint8_t>(suzume::core::PartOfSpeech::Conjunction));
static_assert(SUZUME_POS_DETERMINER == static_cast<uint8_t>(suzume::core::PartOfSpeech::Determiner));
static_assert(SUZUME_POS_PRONOUN == static_cast<uint8_t>(suzume::core::PartOfSpeech::Pronoun));
static_assert(SUZUME_POS_PREFIX == static_cast<uint8_t>(suzume::core::PartOfSpeech::Prefix));
static_assert(SUZUME_POS_SUFFIX == static_cast<uint8_t>(suzume::core::PartOfSpeech::Suffix));
static_assert(SUZUME_POS_INTERJECTION == static_cast<uint8_t>(suzume::core::PartOfSpeech::Interjection));
static_assert(SUZUME_POS_SYMBOL == static_cast<uint8_t>(suzume::core::PartOfSpeech::Symbol));
static_assert(SUZUME_POS_OTHER == static_cast<uint8_t>(suzume::core::PartOfSpeech::Other));
static_assert(static_cast<uint8_t>(suzume::core::AnalysisMode::Normal) == 0);
static_assert(static_cast<uint8_t>(suzume::core::AnalysisMode::Search) == 1);
static_assert(static_cast<uint8_t>(suzume::core::AnalysisMode::Split) == 2);
static_assert(SUZUME_MODE_NORMAL == static_cast<uint8_t>(suzume::core::AnalysisMode::Normal));
static_assert(SUZUME_MODE_SEARCH == static_cast<uint8_t>(suzume::core::AnalysisMode::Search));
static_assert(SUZUME_MODE_SPLIT == static_cast<uint8_t>(suzume::core::AnalysisMode::Split));
static_assert(SUZUME_MORPHEME_USER_DICT == (1U << 0U));
static_assert(SUZUME_MORPHEME_FORMAL_NOUN == (1U << 1U));
static_assert(SUZUME_MORPHEME_LOW_INFO == (1U << 2U));
static_assert(SUZUME_MORPHEME_UNKNOWN == (1U << 3U));
static_assert(SUZUME_MORPHEME_FROM_DICTIONARY == (1U << 4U));
static_assert(SUZUME_MORPHEME_CONJUGATABLE == (1U << 5U));
static_assert(SUZUME_TAG_POS_NOUN == suzume::postprocess::kTagPosNoun);
static_assert(SUZUME_TAG_POS_VERB == suzume::postprocess::kTagPosVerb);
static_assert(SUZUME_TAG_POS_ADJECTIVE == suzume::postprocess::kTagPosAdjective);
static_assert(SUZUME_TAG_POS_ADVERB == suzume::postprocess::kTagPosAdverb);
static_assert(SUZUME_TAG_POS_PARTICLE == suzume::postprocess::kTagPosParticle);
static_assert(SUZUME_TAG_POS_AUXILIARY == suzume::postprocess::kTagPosAuxiliary);
static_assert(SUZUME_ERROR_SUCCESS == static_cast<uint8_t>(suzume::core::ErrorCode::Success));
static_assert(SUZUME_ERROR_INVALID_UTF8 == static_cast<uint8_t>(suzume::core::ErrorCode::InvalidUtf8));
static_assert(SUZUME_ERROR_DICTIONARY_LOAD_FAILED ==
              static_cast<uint8_t>(suzume::core::ErrorCode::DictionaryLoadFailed));
static_assert(SUZUME_ERROR_FILE_NOT_FOUND == static_cast<uint8_t>(suzume::core::ErrorCode::FileNotFound));
static_assert(SUZUME_ERROR_PARSE == static_cast<uint8_t>(suzume::core::ErrorCode::ParseError));
static_assert(SUZUME_ERROR_OUT_OF_MEMORY == static_cast<uint8_t>(suzume::core::ErrorCode::OutOfMemory));
static_assert(SUZUME_ERROR_INVALID_INPUT == static_cast<uint8_t>(suzume::core::ErrorCode::InvalidInput));
static_assert(SUZUME_ERROR_INTERNAL == static_cast<uint8_t>(suzume::core::ErrorCode::InternalError));

thread_local std::string last_error;
thread_local suzume_error_code_t last_error_code = SUZUME_ERROR_SUCCESS;

void clearLastError() {
  last_error.clear();
  last_error_code = SUZUME_ERROR_SUCCESS;
}

void setLastError(std::string_view message, suzume_error_code_t code = SUZUME_ERROR_INTERNAL) {
  last_error = message;
  last_error_code = code;
}

void setLastError(const suzume::core::Error& error) {
  setLastError(error.message, static_cast<suzume_error_code_t>(error.code));
}

#if defined(__cpp_exceptions) && __cpp_exceptions
void setLastErrorFromException() {
  try {
    throw;
  } catch (const std::bad_alloc&) {
    setLastError("Out of memory", SUZUME_ERROR_OUT_OF_MEMORY);
  } catch (const std::exception& err) {
    setLastError(err.what());
  } catch (...) {
    setLastError("Unknown C API error");
  }
}
#endif

// Exception firewall for the C ABI. With exceptions enabled every entry point
// turns an unexpected C++ exception into a last-error string plus an error
// return; with -fno-exceptions the guards compile away — nothing in the core
// throws, and an allocation failure terminates as is standard for such builds.
#if defined(__cpp_exceptions) && __cpp_exceptions
#define SUZUME_C_TRY try
#define SUZUME_C_CATCH(fallback) \
  catch (...) {                  \
    setLastErrorFromException(); \
    fallback;                    \
  }
#else
#define SUZUME_C_TRY
#define SUZUME_C_CATCH(fallback)
#endif

constexpr size_t alignUp(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

char* copyStringToArena(std::string_view str, char*& cursor) {
  char* result = cursor;
  std::memcpy(result, str.data(), str.size());
  result[str.size()] = '\0';
  cursor += str.size() + 1;
  return result;
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

suzume_tags_t* makeTagsResult(const std::vector<suzume::postprocess::TagEntry>& tags) {
  size_t strings_size = 0;
  for (const auto& tag : tags) {
    strings_size += tag.tag.size() + 1;
  }

  const size_t tags_offset = alignUp(sizeof(suzume_tags_t), alignof(char*));
  const size_t pos_offset = tags_offset + tags.size() * sizeof(char*);
  const size_t strings_offset = pos_offset + tags.size() * sizeof(suzume_pos_t);
  auto* memory = static_cast<std::byte*>(::operator new(strings_offset + strings_size));
  auto* result = reinterpret_cast<suzume_tags_t*>(memory);
  result->count = tags.size();
  auto** tag_array = tags.empty() ? nullptr : reinterpret_cast<const char**>(memory + tags_offset);
  result->tags = tag_array;
  result->pos = tags.empty() ? nullptr : reinterpret_cast<suzume_pos_t*>(memory + pos_offset);

  char* cursor = reinterpret_cast<char*>(memory + strings_offset);
  for (size_t idx = 0; idx < tags.size(); ++idx) {
    tag_array[idx] = copyStringToArena(tags[idx].tag, cursor);
    result->pos[idx] = static_cast<suzume_pos_t>(tags[idx].pos);
  }
  return result;
}

suzume_result_t* analyzeBytes(SuzumeHandle* handle, std::string_view text) {
  if (!suzume::normalize::isValidUtf8(text)) {
    setLastError("suzume_analyze: invalid UTF-8 input", SUZUME_ERROR_INVALID_UTF8);
    return nullptr;
  }

  auto analyzed = handle->instance.analyzeWithNormalizedTextResult(text);
  if (!analyzed.hasValue()) {
    setLastError(analyzed.error());
    return nullptr;
  }
  const auto& output = analyzed.value();
  const auto& morphemes = output.morphemes;

  size_t strings_size = output.normalized_text.size() + 1;
  for (const auto& morph : morphemes) {
    strings_size += morph.surface.size() + 1;
    strings_size += morph.getLemma().size() + 1;
  }

  const size_t morphemes_offset = alignUp(sizeof(suzume_result_t), alignof(suzume_morpheme_t));
  const size_t strings_offset = morphemes_offset + morphemes.size() * sizeof(suzume_morpheme_t);
  auto* memory = static_cast<std::byte*>(::operator new(strings_offset + strings_size));
  auto* result = reinterpret_cast<suzume_result_t*>(memory);
  result->count = morphemes.size();
  result->morphemes = morphemes.empty() ? nullptr : reinterpret_cast<suzume_morpheme_t*>(memory + morphemes_offset);
  char* cursor = reinterpret_cast<char*>(memory + strings_offset);
  result->normalized_text = copyStringToArena(output.normalized_text, cursor);
  result->normalized_text_size = output.normalized_text.size();
  for (size_t idx = 0; idx < morphemes.size(); ++idx) {
    const auto& morph = morphemes[idx];
    result->morphemes[idx].surface = copyStringToArena(morph.surface, cursor);
    const std::string_view lemma = morph.getLemma();
    result->morphemes[idx].base_form = copyStringToArena(lemma, cursor);
    result->morphemes[idx].start = static_cast<uint32_t>(morph.start);
    result->morphemes[idx].end = static_cast<uint32_t>(morph.end);
    result->morphemes[idx].score = morph.score;
    result->morphemes[idx].pos = static_cast<suzume_pos_t>(morph.pos);
    result->morphemes[idx].extended_pos = static_cast<suzume_extended_pos_t>(morph.extended_pos);
    result->morphemes[idx].conjugation_type = static_cast<suzume_conjugation_type_t>(morph.conj_type);
    result->morphemes[idx].conjugation_form = static_cast<suzume_conjugation_form_t>(morph.conj_form);
    const bool conjugatable = morph.pos == suzume::core::PartOfSpeech::Verb ||
                              morph.pos == suzume::core::PartOfSpeech::Adjective ||
                              morph.pos == suzume::core::PartOfSpeech::Auxiliary;
    result->morphemes[idx].flags =
        static_cast<uint8_t>((morph.fromUserDict() ? SUZUME_MORPHEME_USER_DICT : 0U) |
                             (morph.isFormalNoun() ? SUZUME_MORPHEME_FORMAL_NOUN : 0U) |
                             (morph.isLowInformation() ? SUZUME_MORPHEME_LOW_INFO : 0U) |
                             (morph.isUnknown() ? SUZUME_MORPHEME_UNKNOWN : 0U) |
                             (morph.fromDictionary() ? SUZUME_MORPHEME_FROM_DICTIONARY : 0U) |
                             (conjugatable ? SUZUME_MORPHEME_CONJUGATABLE : 0U));
    result->morphemes[idx].surface_size = morph.surface.size();
    result->morphemes[idx].base_form_size = lemma.size();
  }
  return result;
}

suzume_tags_t* generateTagsBytes(SuzumeHandle* handle, std::string_view text, const suzume_tag_options_t* options) {
  if (options == nullptr) {
    auto result = handle->instance.generateTagsResult(text);
    if (!result.hasValue()) {
      setLastError(result.error());
      return nullptr;
    }
    return makeTagsResult(result.value());
  }

  suzume::postprocess::TagGeneratorOptions tag_opts;
  tag_opts.pos_filter = options->pos_filter;
  tag_opts.exclude_basic = (options->exclude_basic != 0);
  tag_opts.use_lemma = (options->use_lemma != 0);
  tag_opts.min_tag_length = options->min_length;
  tag_opts.max_tags = options->max_tags;
  tag_opts.exclude_particles = (options->exclude_particles != 0);
  tag_opts.exclude_auxiliaries = (options->exclude_auxiliaries != 0);
  tag_opts.exclude_formal_nouns = (options->exclude_formal_nouns != 0);
  tag_opts.exclude_low_info = (options->exclude_low_info != 0);
  tag_opts.remove_duplicates = (options->remove_duplicates != 0);
  auto result = handle->instance.generateTagsResult(text, tag_opts);
  if (!result.hasValue()) {
    setLastError(result.error());
    return nullptr;
  }
  return makeTagsResult(result.value());
}

}  // namespace

extern "C" {

SUZUME_EXPORT suzume_t suzume_create(void) {
  clearLastError();
  SUZUME_C_TRY {
    return new SuzumeHandle();
  }
  SUZUME_C_CATCH(return nullptr)
}

SUZUME_EXPORT void suzume_init_extended_options(suzume_extended_options_t* options) {
  if (options == nullptr) {
    return;
  }
  options->preserve_vu = 1;
  options->preserve_case = 1;
  options->preserve_symbols = 0;
  options->mode = 0;
  options->lemmatize = 1;
  options->merge_compounds = 0;
  options->skip_user_dictionary = 0;
  options->skip_core_dictionary = 0;
  options->report_scorer_config = 0;
  options->skip_env_config = 0;
  options->scorer_options_json = nullptr;
  options->data_directory = nullptr;
}

SUZUME_EXPORT void suzume_init_tag_options(suzume_tag_options_t* options) {
  if (options == nullptr) {
    return;
  }
  options->pos_filter = 0;
  options->exclude_basic = 0;
  options->use_lemma = 1;
  options->min_length = 2;
  options->max_tags = 0;
  options->exclude_particles = 1;
  options->exclude_auxiliaries = 1;
  options->exclude_formal_nouns = 1;
  options->exclude_low_info = 1;
  options->remove_duplicates = 1;
}

SUZUME_EXPORT suzume_t suzume_create_with_extended_options(const suzume_extended_options_t* options) {
  clearLastError();
  SUZUME_C_TRY {
    suzume::SuzumeOptions opts;
    if (options != nullptr) {
      opts.normalize_options.preserve_vu = (options->preserve_vu != 0);
      opts.normalize_options.preserve_case = (options->preserve_case != 0);
      opts.remove_symbols = (options->preserve_symbols == 0);
      auto mode = parseAnalysisMode(options->mode);
      if (!mode.has_value()) {
        setLastError("suzume_create_with_extended_options: invalid mode", SUZUME_ERROR_INVALID_INPUT);
        return nullptr;
      }
      opts.mode = *mode;
      opts.lemmatize = (options->lemmatize != 0);
      opts.merge_compounds = (options->merge_compounds != 0);
      opts.skip_user_dictionary = (options->skip_user_dictionary != 0);
      opts.skip_core_dictionary = (options->skip_core_dictionary != 0);
      opts.skip_env_config = (options->skip_env_config != 0);
      opts.report_scorer_config = (options->report_scorer_config != 0);
      if (options->scorer_options_json != nullptr) {
        std::string scorer_error;
        if (!suzume::analysis::ScorerOptionsLoader::loadFromJsonString(options->scorer_options_json,
                                                                       opts.scorer_options, &scorer_error)) {
          setLastError("suzume_create_with_extended_options: invalid scorer options: " + scorer_error,
                       SUZUME_ERROR_PARSE);
          return nullptr;
        }
        opts.scorer_options_json = options->scorer_options_json;
      }
      if (options->data_directory != nullptr) {
        opts.data_directory = options->data_directory;
      }
    }
    return new SuzumeHandle(opts);
  }
  SUZUME_C_CATCH(return nullptr)
}

SUZUME_EXPORT void suzume_destroy(suzume_t handle) {
  delete handle;
}

SUZUME_EXPORT int suzume_set_mode(suzume_t handle, uint8_t mode) {
  if (handle == nullptr) {
    setLastError("suzume_set_mode: null handle", SUZUME_ERROR_INVALID_INPUT);
    return 0;
  }
  const auto parsed_mode = parseAnalysisMode(mode);
  if (!parsed_mode.has_value()) {
    setLastError("suzume_set_mode: invalid mode", SUZUME_ERROR_INVALID_INPUT);
    return 0;
  }

  clearLastError();
  SUZUME_C_TRY {
    handle->instance.setMode(*parsed_mode);
    return 1;
  }
  SUZUME_C_CATCH(return 0)
}

SUZUME_EXPORT uint8_t suzume_mode(suzume_t handle) {
  if (handle == nullptr) {
    setLastError("suzume_mode: null handle", SUZUME_ERROR_INVALID_INPUT);
    return SUZUME_MODE_INVALID;
  }

  clearLastError();
  return static_cast<uint8_t>(handle->instance.mode());
}

SUZUME_EXPORT suzume_result_t* suzume_analyze(suzume_t handle, const char* text) {
  return suzume_analyze_n(handle, text, text == nullptr ? 0 : std::strlen(text));
}

SUZUME_EXPORT suzume_result_t* suzume_analyze_n(suzume_t handle, const char* text, size_t size) {
  if (handle == nullptr || text == nullptr) {
    setLastError("suzume_analyze: null handle or text", SUZUME_ERROR_INVALID_INPUT);
    return nullptr;
  }

  clearLastError();
  SUZUME_C_TRY {
    return analyzeBytes(handle, std::string_view(text, size));
  }
  SUZUME_C_CATCH(return nullptr)
}

SUZUME_EXPORT void suzume_result_free(suzume_result_t* result) {
  if (result == nullptr) {
    return;
  }

  ::operator delete(result);
}

SUZUME_EXPORT suzume_tags_t* suzume_generate_tags(suzume_t handle, const char* text) {
  return suzume_generate_tags_n(handle, text, text == nullptr ? 0 : std::strlen(text));
}

SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_n(suzume_t handle, const char* text, size_t size) {
  if (handle == nullptr || text == nullptr) {
    setLastError("suzume_generate_tags: null handle or text", SUZUME_ERROR_INVALID_INPUT);
    return nullptr;
  }

  clearLastError();
  SUZUME_C_TRY {
    return generateTagsBytes(handle, std::string_view(text, size), nullptr);
  }
  SUZUME_C_CATCH(return nullptr)
}

SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_with_options(suzume_t handle, const char* text,
                                                               const suzume_tag_options_t* options) {
  return suzume_generate_tags_with_options_n(handle, text, text == nullptr ? 0 : std::strlen(text), options);
}

SUZUME_EXPORT suzume_tags_t* suzume_generate_tags_with_options_n(suzume_t handle, const char* text, size_t size,
                                                                 const suzume_tag_options_t* options) {
  if (handle == nullptr || text == nullptr || options == nullptr) {
    setLastError("suzume_generate_tags_with_options: null handle, text, or options", SUZUME_ERROR_INVALID_INPUT);
    return nullptr;
  }

  clearLastError();
  SUZUME_C_TRY {
    return generateTagsBytes(handle, std::string_view(text, size), options);
  }
  SUZUME_C_CATCH(return nullptr)
}

SUZUME_EXPORT void suzume_tags_free(suzume_tags_t* tags) {
  if (tags == nullptr) {
    return;
  }

  ::operator delete(tags);
}

SUZUME_EXPORT int suzume_load_user_dict(suzume_t handle, const char* data, size_t size) {
  return suzume_load_user_dict_count(handle, data, size) > 0 ? 1 : 0;
}

SUZUME_EXPORT size_t suzume_load_user_dict_count(suzume_t handle, const char* data, size_t size) {
  if (handle == nullptr || data == nullptr) {
    setLastError("suzume_load_user_dict_count: null handle or data", SUZUME_ERROR_INVALID_INPUT);
    return 0;
  }

  clearLastError();
  SUZUME_C_TRY {
    auto result = handle->instance.loadUserDictionaryFromMemoryResult(data, size);
    if (result.hasValue()) {
      return result.value();
    }
    setLastError(result.error());
    return 0;
  }
  SUZUME_C_CATCH(return 0)
}

SUZUME_EXPORT int suzume_load_binary_dict(suzume_t handle, const uint8_t* data, size_t size) {
  if (handle == nullptr || data == nullptr) {
    setLastError("suzume_load_binary_dict: null handle or data", SUZUME_ERROR_INVALID_INPUT);
    return 0;
  }

  clearLastError();
  SUZUME_C_TRY {
    auto result = handle->instance.loadBinaryDictionaryResult(data, size);
    if (result.hasValue()) {
      return 1;
    }
    setLastError(result.error());
    return 0;
  }
  SUZUME_C_CATCH(return 0)
}

SUZUME_EXPORT int suzume_clear_user_dictionaries(suzume_t handle) {
  if (handle == nullptr) {
    setLastError("suzume_clear_user_dictionaries: null handle", SUZUME_ERROR_INVALID_INPUT);
    return 0;
  }
  clearLastError();
  handle->instance.clearUserDictionaries();
  return 1;
}

SUZUME_EXPORT int suzume_has_core_dictionary(suzume_t handle) {
  if (handle == nullptr) {
    setLastError("suzume_has_core_dictionary: null handle", SUZUME_ERROR_INVALID_INPUT);
    return 0;
  }
  return handle->instance.hasCoreDictionary() ? 1 : 0;
}

SUZUME_EXPORT const char* suzume_version(void) {
  return SUZUME_VERSION;
}

SUZUME_EXPORT uint32_t suzume_abi_version(void) {
  return SUZUME_ABI_VERSION;
}

SUZUME_EXPORT const char* suzume_last_error(void) {
  return last_error.c_str();
}

SUZUME_EXPORT suzume_error_code_t suzume_last_error_code(void) {
  return last_error_code;
}

SUZUME_EXPORT const char* suzume_conjugation_type_label(suzume_conjugation_type_t code) {
  static constexpr std::array<const char*, 18> labels = {
      "",           "一段",       "五段・カ行", "五段・ガ行", "五段・サ行",   "五段・タ行",
      "五段・ナ行", "五段・バ行", "五段・マ行", "五段・ラ行", "五段・ワ行",   "サ変",
      "カ変",       "形容詞",     "ナ形容詞",   "感動詞",     "固有名詞・姓", "固有名詞・名"};
  return code > 0 && code < labels.size() ? labels[code] : nullptr;
}

SUZUME_EXPORT const char* suzume_extended_pos_label(suzume_extended_pos_t code) {
  if (code >= static_cast<suzume_extended_pos_t>(suzume::core::ExtendedPOS::Count_)) {
    return nullptr;
  }
  return suzume::core::extendedPosToString(static_cast<suzume::core::ExtendedPOS>(code)).data();
}

SUZUME_EXPORT const char* suzume_conjugation_form_label(suzume_conjugation_form_t code) {
  if (code >= static_cast<suzume_conjugation_form_t>(suzume::grammar::ConjForm::Count_)) {
    return nullptr;
  }
  return suzume::grammar::conjFormToJapanese(static_cast<suzume::grammar::ConjForm>(code)).data();
}

SUZUME_EXPORT const char* suzume_pos_label(suzume_pos_t code) {
  if (code >= static_cast<suzume_pos_t>(suzume::core::PartOfSpeech::Count_)) {
    return nullptr;
  }
  return suzume::core::posToString(static_cast<suzume::core::PartOfSpeech>(code)).data();
}

SUZUME_EXPORT size_t suzume_dictionary_warning_count(suzume_t handle) {
  if (handle == nullptr) {
    return 0;
  }
  return handle->instance.dictionaryWarnings().size();
}

SUZUME_EXPORT const char* suzume_dictionary_warning(suzume_t handle, size_t index) {
  if (handle == nullptr) {
    setLastError("suzume_dictionary_warning: null handle", SUZUME_ERROR_INVALID_INPUT);
    return nullptr;
  }
  const auto& warnings = handle->instance.dictionaryWarnings();
  if (index >= warnings.size()) {
    setLastError("suzume_dictionary_warning: index out of range", SUZUME_ERROR_INVALID_INPUT);
    return nullptr;
  }
  thread_local std::string warning;
  warning = warnings[index];
  return warning.c_str();
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
    case 2:
      return offsetof(suzume_result_t, normalized_text);
    case 3:
      return offsetof(suzume_result_t, normalized_text_size);
    default:
      return static_cast<size_t>(-1);
  }
}

SUZUME_EXPORT size_t suzume_offsetof_morpheme(uint32_t field) {
  switch (field) {
    case 0:
      return offsetof(suzume_morpheme_t, surface);
    case 1:
      return offsetof(suzume_morpheme_t, base_form);
    case 2:
      return offsetof(suzume_morpheme_t, start);
    case 3:
      return offsetof(suzume_morpheme_t, end);
    case 4:
      return offsetof(suzume_morpheme_t, score);
    case 5:
      return offsetof(suzume_morpheme_t, pos);
    case 6:
      return offsetof(suzume_morpheme_t, extended_pos);
    case 7:
      return offsetof(suzume_morpheme_t, conjugation_type);
    case 8:
      return offsetof(suzume_morpheme_t, conjugation_form);
    case 9:
      return offsetof(suzume_morpheme_t, flags);
    case 10:
      return offsetof(suzume_morpheme_t, surface_size);
    case 11:
      return offsetof(suzume_morpheme_t, base_form_size);
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
    case 5:
      return offsetof(suzume_tag_options_t, exclude_particles);
    case 6:
      return offsetof(suzume_tag_options_t, exclude_auxiliaries);
    case 7:
      return offsetof(suzume_tag_options_t, exclude_formal_nouns);
    case 8:
      return offsetof(suzume_tag_options_t, exclude_low_info);
    case 9:
      return offsetof(suzume_tag_options_t, remove_duplicates);
    default:
      return static_cast<size_t>(-1);
  }
}

SUZUME_EXPORT size_t suzume_offsetof_extended_options(uint32_t field) {
  switch (field) {
    case 0:
      return offsetof(suzume_extended_options_t, preserve_vu);
    case 1:
      return offsetof(suzume_extended_options_t, preserve_case);
    case 2:
      return offsetof(suzume_extended_options_t, preserve_symbols);
    case 3:
      return offsetof(suzume_extended_options_t, mode);
    case 4:
      return offsetof(suzume_extended_options_t, lemmatize);
    case 5:
      return offsetof(suzume_extended_options_t, merge_compounds);
    case 6:
      return offsetof(suzume_extended_options_t, skip_user_dictionary);
    case 7:
      return offsetof(suzume_extended_options_t, skip_core_dictionary);
    case 8:
      return offsetof(suzume_extended_options_t, report_scorer_config);
    case 9:
      return offsetof(suzume_extended_options_t, skip_env_config);
    case 10:
      return offsetof(suzume_extended_options_t, scorer_options_json);
    case 11:
      return offsetof(suzume_extended_options_t, data_directory);
    default:
      return static_cast<size_t>(-1);
  }
}

#ifdef __EMSCRIPTEN__
static_assert(sizeof(suzume_result_t) == 16);
static_assert(offsetof(suzume_result_t, morphemes) == 0);
static_assert(offsetof(suzume_result_t, count) == 4);
static_assert(offsetof(suzume_result_t, normalized_text) == 8);
static_assert(offsetof(suzume_result_t, normalized_text_size) == 12);
static_assert(sizeof(suzume_morpheme_t) == 36);
static_assert(offsetof(suzume_morpheme_t, surface) == 0);
static_assert(offsetof(suzume_morpheme_t, base_form) == 4);
static_assert(offsetof(suzume_morpheme_t, start) == 8);
static_assert(offsetof(suzume_morpheme_t, end) == 12);
static_assert(offsetof(suzume_morpheme_t, score) == 16);
static_assert(offsetof(suzume_morpheme_t, pos) == 20);
static_assert(offsetof(suzume_morpheme_t, extended_pos) == 21);
static_assert(offsetof(suzume_morpheme_t, conjugation_type) == 22);
static_assert(offsetof(suzume_morpheme_t, conjugation_form) == 23);
static_assert(offsetof(suzume_morpheme_t, flags) == 24);
static_assert(offsetof(suzume_morpheme_t, surface_size) == 28);
static_assert(offsetof(suzume_morpheme_t, base_form_size) == 32);
static_assert(sizeof(suzume_tags_t) == 12);
static_assert(offsetof(suzume_tags_t, tags) == 0);
static_assert(offsetof(suzume_tags_t, pos) == 4);
static_assert(offsetof(suzume_tags_t, count) == 8);
static_assert(sizeof(suzume_tag_options_t) == 20);
static_assert(offsetof(suzume_tag_options_t, pos_filter) == 0);
static_assert(offsetof(suzume_tag_options_t, exclude_basic) == 1);
static_assert(offsetof(suzume_tag_options_t, use_lemma) == 2);
static_assert(offsetof(suzume_tag_options_t, min_length) == 4);
static_assert(offsetof(suzume_tag_options_t, max_tags) == 8);
static_assert(offsetof(suzume_tag_options_t, exclude_particles) == 12);
static_assert(offsetof(suzume_tag_options_t, exclude_auxiliaries) == 13);
static_assert(offsetof(suzume_tag_options_t, exclude_formal_nouns) == 14);
static_assert(offsetof(suzume_tag_options_t, exclude_low_info) == 15);
static_assert(offsetof(suzume_tag_options_t, remove_duplicates) == 16);
static_assert(sizeof(suzume_extended_options_t) == 20);
static_assert(offsetof(suzume_extended_options_t, preserve_vu) == 0);
static_assert(offsetof(suzume_extended_options_t, preserve_case) == 1);
static_assert(offsetof(suzume_extended_options_t, preserve_symbols) == 2);
static_assert(offsetof(suzume_extended_options_t, mode) == 3);
static_assert(offsetof(suzume_extended_options_t, lemmatize) == 4);
static_assert(offsetof(suzume_extended_options_t, merge_compounds) == 5);
static_assert(offsetof(suzume_extended_options_t, skip_user_dictionary) == 6);
static_assert(offsetof(suzume_extended_options_t, skip_core_dictionary) == 7);
static_assert(offsetof(suzume_extended_options_t, report_scorer_config) == 8);
static_assert(offsetof(suzume_extended_options_t, skip_env_config) == 9);
static_assert(offsetof(suzume_extended_options_t, scorer_options_json) == 12);
static_assert(offsetof(suzume_extended_options_t, data_directory) == 16);
#endif

}  // extern "C"
