#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "suzume/suzume_c.h"

namespace {

TEST(SuzumeCApiTest, LastErrorReportsInvalidArguments) {
  EXPECT_EQ(suzume_analyze(nullptr, "test"), nullptr);

  std::string error = suzume_last_error();
  EXPECT_NE(error.find("null handle"), std::string::npos);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_INPUT);
}

TEST(SuzumeCApiTest, LastErrorClearsAfterSuccess) {
  EXPECT_EQ(suzume_analyze(nullptr, "test"), nullptr);
  ASSERT_STRNE(suzume_last_error(), "");

  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "東京");
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(suzume_last_error(), "");
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_SUCCESS);

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, LoadUserDictReportsParseDetails) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  const char* csv_data = "\"東京,NOUN,0.5\n";
  EXPECT_EQ(suzume_load_user_dict(handle, csv_data, std::strlen(csv_data)), 0);

  std::string error = suzume_last_error();
  EXPECT_NE(error.find("Invalid legacy CSV quoting"), std::string::npos);
  EXPECT_NE(error.find("unterminated quoted field"), std::string::npos);

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, LoadBinaryDictReportsParseDetails) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  const uint8_t bad_data[] = {0x00, 0x01, 0x02, 0x03};
  EXPECT_EQ(suzume_load_binary_dict(handle, bad_data, sizeof(bad_data)), 0);

  std::string error = suzume_last_error();
  EXPECT_NE(error.find("Dictionary file too small"), std::string::npos);

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, CreateWithExtendedOptionsAcceptsModeAndPostprocessOptions) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.mode = 2;  // split

  suzume_t handle = suzume_create_with_extended_options(&options);
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "API開発");
  ASSERT_NE(result, nullptr);
  EXPECT_GT(result->count, 1u);

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, RuntimeModeSwitchChangesAnalysisWithoutReplacingTheHandle) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);
  EXPECT_EQ(suzume_mode(handle), SUZUME_MODE_NORMAL);

  suzume_result_t* normal = suzume_analyze(handle, "API開発");
  ASSERT_NE(normal, nullptr);
  const size_t normal_count = normal->count;
  suzume_result_free(normal);

  ASSERT_EQ(suzume_set_mode(handle, SUZUME_MODE_SPLIT), 1);
  EXPECT_EQ(suzume_mode(handle), SUZUME_MODE_SPLIT);
  suzume_result_t* split = suzume_analyze(handle, "API開発");
  ASSERT_NE(split, nullptr);
  EXPECT_GT(split->count, normal_count);
  suzume_result_free(split);

  EXPECT_EQ(suzume_set_mode(handle, 99), 0);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_INPUT);
  EXPECT_EQ(suzume_mode(nullptr), SUZUME_MODE_INVALID);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_INPUT);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, InitExtendedOptionsPreservesDefaultTrueFields) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);

  EXPECT_EQ(options.preserve_vu, 1);
  EXPECT_EQ(options.preserve_case, 1);
  EXPECT_EQ(options.preserve_symbols, 0);
  EXPECT_EQ(options.mode, 0);
  EXPECT_EQ(options.lemmatize, 1);
  EXPECT_EQ(options.merge_compounds, 0);
  EXPECT_EQ(options.skip_user_dictionary, 0);
  EXPECT_EQ(options.skip_core_dictionary, 0);
  EXPECT_EQ(options.report_scorer_config, 0);
  EXPECT_EQ(options.skip_env_config, 0);
  EXPECT_EQ(options.scorer_options_json, nullptr);
  EXPECT_EQ(options.data_directory, nullptr);
}

TEST(SuzumeCApiTest, ExplicitDataDirectoryOverridesEnvironmentSearch) {
#ifndef __EMSCRIPTEN__
  namespace fs = std::filesystem;
  const char* old_data_dir = std::getenv("SUZUME_DATA_DIR");
  const std::string old_value = old_data_dir != nullptr ? old_data_dir : "";
  const fs::path empty_dir = fs::temp_directory_path() / "suzume_c_api_empty_data";
  fs::remove_all(empty_dir);
  fs::create_directories(empty_dir);
  const std::string bundled_dir = fs::absolute("data").string();

  setenv("SUZUME_DATA_DIR", empty_dir.string().c_str(), 1);
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.data_directory = bundled_dir.c_str();
  suzume_t handle = suzume_create_with_extended_options(&options);
  if (old_data_dir != nullptr) {
    setenv("SUZUME_DATA_DIR", old_value.c_str(), 1);
  } else {
    unsetenv("SUZUME_DATA_DIR");
  }
  fs::remove_all(empty_dir);

  ASSERT_NE(handle, nullptr);
  EXPECT_EQ(suzume_has_core_dictionary(handle), 1);
  for (size_t index = 0; index < suzume_dictionary_warning_count(handle); ++index) {
    EXPECT_EQ(std::string(suzume_dictionary_warning(handle, index)).find("external dictionary directory"),
              std::string::npos);
  }
  suzume_destroy(handle);
#endif
}

TEST(SuzumeCApiTest, CreateWithExtendedOptionsRejectsInvalidMode) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.mode = 99;

  suzume_t handle = suzume_create_with_extended_options(&options);
  EXPECT_EQ(handle, nullptr);

  std::string error = suzume_last_error();
  EXPECT_NE(error.find("invalid mode"), std::string::npos);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_INPUT);
}

TEST(SuzumeCApiTest, AnalyzeReturnsOffsetsAndDiagnosticFields) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "東京");
  ASSERT_NE(result, nullptr);
  ASSERT_GT(result->count, 0u);
  EXPECT_STREQ(result->normalized_text, "東京");

  const auto& morpheme = result->morphemes[0];
  EXPECT_STREQ(morpheme.surface, "東京");
  EXPECT_EQ(morpheme.start, 0u);
  EXPECT_GE(morpheme.end, morpheme.start + 1u);
  EXPECT_GE(morpheme.score, 0.0F);
  EXPECT_EQ(morpheme.flags & ~0x3FU, 0U);

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, ConjugationMetadataUsesCompactCodes) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "美しく");
  ASSERT_NE(result, nullptr);
  ASSERT_GT(result->count, 0u);
  EXPECT_EQ(result->morphemes[0].pos, SUZUME_POS_ADJECTIVE);
  EXPECT_EQ(result->morphemes[0].conjugation_type, 0U);
  EXPECT_EQ(result->morphemes[0].conjugation_form, 2U);
  EXPECT_NE(result->morphemes[0].flags & SUZUME_MORPHEME_CONJUGATABLE, 0U);
  EXPECT_STREQ(suzume_conjugation_type_label(14), "ナ形容詞");
  EXPECT_EQ(suzume_conjugation_type_label(18), nullptr);
  EXPECT_STREQ(suzume_pos_label(SUZUME_POS_VERB), "VERB");
  EXPECT_EQ(suzume_pos_label(15), nullptr);

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, AuxiliaryConjugationMetadataIsExposed) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "書かなかった");
  ASSERT_NE(result, nullptr);
  const auto negative =
      std::find_if(result->morphemes, result->morphemes + result->count,
                   [](const suzume_morpheme_t& morpheme) { return std::strcmp(morpheme.surface, "なかっ") == 0; });
  ASSERT_NE(negative, result->morphemes + result->count);
  EXPECT_EQ(negative->pos, SUZUME_POS_AUXILIARY);
  EXPECT_NE(negative->flags & SUZUME_MORPHEME_CONJUGATABLE, 0U);
  EXPECT_EQ(negative->conjugation_type, 0U);
  EXPECT_STREQ(suzume_conjugation_form_label(negative->conjugation_form), "終止形");

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, NominalizedMorphemesClearConjugationMetadata) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "お読みですか");
  ASSERT_NE(result, nullptr);
  const auto reading =
      std::find_if(result->morphemes, result->morphemes + result->count,
                   [](const suzume_morpheme_t& morpheme) { return std::strcmp(morpheme.surface, "読み") == 0; });
  ASSERT_NE(reading, result->morphemes + result->count);
  EXPECT_EQ(reading->pos, SUZUME_POS_NOUN);
  EXPECT_EQ(reading->conjugation_type, 0U);
  EXPECT_EQ(reading->conjugation_form, 0U);
  EXPECT_EQ(reading->flags & SUZUME_MORPHEME_CONJUGATABLE, 0U);

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, LengthAwareAnalyzePreservesTextAfterEmbeddedNull) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  const std::string text("東京\0大阪", 13);
  suzume_result_t* result = suzume_analyze_n(handle, text.data(), text.size());
  ASSERT_NE(result, nullptr) << suzume_last_error();
  bool found_after_null = false;
  for (size_t index = 0; index < result->count; ++index) {
    EXPECT_GE(result->morphemes[index].surface_size, std::strlen(result->morphemes[index].surface));
    EXPECT_GE(result->morphemes[index].base_form_size, std::strlen(result->morphemes[index].base_form));
    found_after_null = found_after_null || std::strcmp(result->morphemes[index].surface, "大阪") == 0;
  }
  EXPECT_TRUE(found_after_null);

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, MorphemeLengthsPreserveEmbeddedNull) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.preserve_symbols = 1;
  suzume_t handle = suzume_create_with_extended_options(&options);
  ASSERT_NE(handle, nullptr);

  const std::string text("検査\0語", 7);
  suzume_result_t* result = suzume_analyze_n(handle, text.data(), text.size());
  ASSERT_NE(result, nullptr) << suzume_last_error();
  bool found_null = false;
  for (size_t index = 0; index < result->count; ++index) {
    const suzume_morpheme_t& morpheme = result->morphemes[index];
    if (morpheme.surface_size == 1 && morpheme.surface[0] == '\0') {
      EXPECT_EQ(morpheme.base_form_size, 1U);
      EXPECT_EQ(morpheme.base_form[0], '\0');
      found_null = true;
    }
  }
  EXPECT_TRUE(found_null);

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, CanonicalLabelFunctionsCoverSerializedBoundaries) {
  EXPECT_EQ(suzume_conjugation_type_label(0), nullptr);
  EXPECT_STREQ(suzume_conjugation_type_label(17), "固有名詞・名");
  EXPECT_EQ(suzume_conjugation_type_label(18), nullptr);

  EXPECT_STREQ(suzume_extended_pos_label(0), "UNKNOWN");
  EXPECT_STREQ(suzume_extended_pos_label(83), "VERB_仮定縮約");
  EXPECT_STREQ(suzume_extended_pos_label(84), "PART_接続終止");
  EXPECT_EQ(suzume_extended_pos_label(85), nullptr);

  EXPECT_STREQ(suzume_conjugation_form_label(6), "意志形");
  EXPECT_EQ(suzume_conjugation_form_label(7), nullptr);
}

TEST(SuzumeCApiTest, TagEntrypointsRejectInvalidUtf8WithStableCode) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);
  const std::string invalid("\xE3\x81", 2);

  EXPECT_EQ(suzume_generate_tags_n(handle, invalid.data(), invalid.size()), nullptr);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_UTF8);

  suzume_tag_options_t options{};
  suzume_init_tag_options(&options);
  EXPECT_EQ(suzume_generate_tags_with_options_n(handle, invalid.data(), invalid.size(), &options), nullptr);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_UTF8);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, InvalidScorerJsonFailsConstruction) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.scorer_options_json = "{";

  EXPECT_EQ(suzume_create_with_extended_options(&options), nullptr);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_PARSE);
  EXPECT_NE(std::string(suzume_last_error()).find("scorer options"), std::string::npos);
}

TEST(SuzumeCApiTest, ExtendedOptionsCanDisableEnvironmentScorerConfig) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_INFL_confidence_ceiling", "0", 1);
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.skip_user_dictionary = 1;
  options.skip_core_dictionary = 1;
  options.skip_env_config = 1;
  suzume_t handle = suzume_create_with_extended_options(&options);
  unsetenv("SUZUME_SCORER_INFL_confidence_ceiling");
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "歩いています");
  ASSERT_NE(result, nullptr);
  ASSERT_GT(result->count, 0u);
  EXPECT_STREQ(result->morphemes[0].surface, "歩い");

  suzume_result_free(result);
  suzume_destroy(handle);
#endif
}

TEST(SuzumeCApiTest, ScorerConfigStatusUsesTheWarningChannel) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.report_scorer_config = 1;
  options.scorer_options_json = R"({"unary":{"noun_prior":0.25}})";
  suzume_t handle = suzume_create_with_extended_options(&options);
  ASSERT_NE(handle, nullptr);

  ASSERT_GT(suzume_dictionary_warning_count(handle), 0u);
  const char* warning = suzume_dictionary_warning(handle, 0);
  ASSERT_NE(warning, nullptr);
  EXPECT_NE(std::string(warning).find("Scorer configuration active"), std::string::npos);

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, ScorerJsonChangesAnalysis) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.skip_user_dictionary = 1;
  options.skip_core_dictionary = 1;
  options.skip_env_config = 1;
  options.scorer_options_json = R"({"inflection":{"confidence_ceiling":0}})";
  suzume_t handle = suzume_create_with_extended_options(&options);
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "歩いています");
  ASSERT_NE(result, nullptr);
  ASSERT_GT(result->count, 0u);
  EXPECT_STREQ(result->morphemes[0].surface, "歩");

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, UserDictionariesCanBeCleared) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.skip_user_dictionary = 1;
  options.skip_core_dictionary = 1;
  suzume_t handle = suzume_create_with_extended_options(&options);
  ASSERT_NE(handle, nullptr);
  const std::string dictionary = "検査語\tNOUN\n";
  ASSERT_EQ(suzume_load_user_dict(handle, dictionary.data(), dictionary.size()), 1);
  ASSERT_EQ(suzume_clear_user_dictionaries(handle), 1);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_SUCCESS);
  EXPECT_EQ(suzume_clear_user_dictionaries(nullptr), 0);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_INPUT);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, ClearUserDictionariesRetainsBundledDictionary) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  const std::string runtime_dictionary = "検査語\tNOUN\n";
  ASSERT_EQ(suzume_load_user_dict(handle, runtime_dictionary.data(), runtime_dictionary.size()), 1);
  ASSERT_EQ(suzume_clear_user_dictionaries(handle), 1);

  suzume_result_t* runtime_result = suzume_analyze(handle, "検査語");
  ASSERT_NE(runtime_result, nullptr);
  EXPECT_FALSE(runtime_result->count == 1 && std::strcmp(runtime_result->morphemes[0].surface, "検査語") == 0 &&
               (runtime_result->morphemes[0].flags & SUZUME_MORPHEME_USER_DICT) != 0);
  suzume_result_free(runtime_result);

  suzume_result_t* bundled_result = suzume_analyze(handle, "コーヒー豆");
  ASSERT_NE(bundled_result, nullptr);
  ASSERT_EQ(bundled_result->count, 1u);
  EXPECT_STREQ(bundled_result->morphemes[0].surface, "コーヒー豆");
  EXPECT_NE(bundled_result->morphemes[0].flags & SUZUME_MORPHEME_USER_DICT, 0U);
  suzume_result_free(bundled_result);

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, UserDictionaryCountAndCorePresenceAreExposed) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.skip_user_dictionary = 1;
  options.skip_core_dictionary = 1;
  suzume_t handle = suzume_create_with_extended_options(&options);
  ASSERT_NE(handle, nullptr);

  const std::string dictionary = "検査する\tVERB\tSURU\n";
  EXPECT_GT(suzume_load_user_dict_count(handle, dictionary.data(), dictionary.size()), 1u);
  EXPECT_EQ(suzume_has_core_dictionary(handle), 0);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_SUCCESS);
  EXPECT_EQ(suzume_has_core_dictionary(nullptr), 0);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_INPUT);

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, SkippedSourceRecordsReachTheDictionaryWarningChannel) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.skip_user_dictionary = 1;
  options.skip_core_dictionary = 1;
  suzume_t handle = suzume_create_with_extended_options(&options);
  ASSERT_NE(handle, nullptr);

  const std::string dictionary = "missing-pos\n検査語\tNOUN\n";
  EXPECT_EQ(suzume_load_user_dict_count(handle, dictionary.data(), dictionary.size()), 1u);
  ASSERT_EQ(suzume_dictionary_warning_count(handle), 1u);
  ASSERT_NE(suzume_dictionary_warning(handle, 0), nullptr);
  EXPECT_NE(std::string(suzume_dictionary_warning(handle, 0)).find("line 1"), std::string::npos);

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, TagOptionsExposeAllGeneratorFilters) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  const char* text = "りんごが歩きます。読むこと。それ。りんご";
  const auto generate = [handle, text](const suzume_tag_options_t& options) {
    std::vector<std::string> result;
    suzume_tags_t* tags = suzume_generate_tags_with_options(handle, text, &options);
    EXPECT_NE(tags, nullptr) << suzume_last_error();
    if (tags != nullptr) {
      result.reserve(tags->count);
      for (size_t index = 0; index < tags->count; ++index) {
        result.emplace_back(tags->tags[index]);
      }
      suzume_tags_free(tags);
    }
    return result;
  };
  const auto contains = [](const std::vector<std::string>& tags, const std::string& tag) {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
  };

  suzume_tag_options_t inclusive{};
  suzume_init_tag_options(&inclusive);
  inclusive.min_length = 1;
  inclusive.exclude_particles = 0;
  inclusive.exclude_auxiliaries = 0;
  inclusive.exclude_formal_nouns = 0;
  inclusive.exclude_low_info = 0;
  const auto all = generate(inclusive);
  EXPECT_TRUE(contains(all, "が"));
  EXPECT_TRUE(contains(all, "ます"));
  EXPECT_TRUE(contains(all, "こと"));
  EXPECT_TRUE(contains(all, "それ"));
  EXPECT_TRUE(contains(all, "歩く"));
  EXPECT_EQ(std::count(all.begin(), all.end(), "りんご"), 1);

  auto surface = inclusive;
  surface.use_lemma = 0;
  const auto surface_tags = generate(surface);
  EXPECT_TRUE(contains(surface_tags, "歩き"));
  EXPECT_FALSE(contains(surface_tags, "歩く"));

  auto minimum = inclusive;
  minimum.min_length = 2;
  EXPECT_FALSE(contains(generate(minimum), "が"));

  auto maximum = inclusive;
  maximum.max_tags = 2;
  EXPECT_EQ(generate(maximum).size(), 2u);

  auto duplicates = inclusive;
  duplicates.remove_duplicates = 0;
  const auto duplicate_tags = generate(duplicates);
  EXPECT_EQ(std::count(duplicate_tags.begin(), duplicate_tags.end(), "りんご"), 2);

  auto nouns = inclusive;
  nouns.pos_filter = SUZUME_TAG_POS_NOUN;
  const auto noun_tags = generate(nouns);
  EXPECT_TRUE(contains(noun_tags, "りんご"));
  EXPECT_FALSE(contains(noun_tags, "歩く"));
  EXPECT_FALSE(contains(noun_tags, "が"));

  auto particles = inclusive;
  particles.pos_filter = SUZUME_TAG_POS_PARTICLE;
  const auto particle_tags = generate(particles);
  EXPECT_TRUE(contains(particle_tags, "が"));
  EXPECT_FALSE(contains(particle_tags, "歩く"));

  auto auxiliaries = inclusive;
  auxiliaries.pos_filter = SUZUME_TAG_POS_AUXILIARY;
  const auto auxiliary_tags = generate(auxiliaries);
  EXPECT_TRUE(contains(auxiliary_tags, "ます"));
  EXPECT_FALSE(contains(auxiliary_tags, "りんご"));

  auto non_basic = inclusive;
  non_basic.exclude_basic = 1;
  const auto non_basic_tags = generate(non_basic);
  EXPECT_TRUE(contains(non_basic_tags, "歩く"));
  EXPECT_FALSE(contains(non_basic_tags, "りんご"));

  auto no_particles = inclusive;
  no_particles.exclude_particles = 1;
  EXPECT_FALSE(contains(generate(no_particles), "が"));
  auto no_auxiliaries = inclusive;
  no_auxiliaries.exclude_auxiliaries = 1;
  EXPECT_FALSE(contains(generate(no_auxiliaries), "ます"));
  auto no_formal_nouns = inclusive;
  no_formal_nouns.exclude_formal_nouns = 1;
  EXPECT_FALSE(contains(generate(no_formal_nouns), "こと"));
  auto no_low_info = inclusive;
  no_low_info.exclude_low_info = 1;
  EXPECT_FALSE(contains(generate(no_low_info), "それ"));

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, DictionaryWarningAccessorsHandleEmptyAndInvalidIndex) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  EXPECT_EQ(suzume_dictionary_warning_count(nullptr), 0u);
  EXPECT_EQ(suzume_dictionary_warning(nullptr, 0), nullptr);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_INPUT);
  EXPECT_EQ(suzume_dictionary_warning(handle, suzume_dictionary_warning_count(handle)), nullptr);

  std::string error = suzume_last_error();
  EXPECT_NE(error.find("index out of range"), std::string::npos);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_INPUT);

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, DictionaryReadAccessorsPreserveExistingErrorDiagnostics) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);
  const std::string dictionary = "missing-pos\n検査語\tNOUN\n";
  ASSERT_EQ(suzume_load_user_dict_count(handle, dictionary.data(), dictionary.size()), 1u);
  ASSERT_GT(suzume_dictionary_warning_count(handle), 0u);

  const char invalid_utf8[] = {static_cast<char>(0xE3), static_cast<char>(0x81)};
  EXPECT_EQ(suzume_analyze_n(handle, invalid_utf8, sizeof(invalid_utf8)), nullptr);
  ASSERT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_UTF8);
  const std::string expected_error = suzume_last_error();

  (void)suzume_has_core_dictionary(handle);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_UTF8);
  EXPECT_EQ(suzume_last_error(), expected_error);
  ASSERT_NE(suzume_dictionary_warning(handle, 0), nullptr);
  EXPECT_EQ(suzume_last_error_code(), SUZUME_ERROR_INVALID_UTF8);
  EXPECT_EQ(suzume_last_error(), expected_error);

  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, FreeNullPointersAreNoOps) {
  suzume_destroy(nullptr);
  suzume_result_free(nullptr);
  suzume_tags_free(nullptr);
}

TEST(SuzumeCApiTest, LayoutFunctionsMatchNativeStructs) {
  EXPECT_EQ(suzume_sizeof_result(), sizeof(suzume_result_t));
  EXPECT_EQ(suzume_sizeof_morpheme(), sizeof(suzume_morpheme_t));
  EXPECT_EQ(suzume_sizeof_tags(), sizeof(suzume_tags_t));
  EXPECT_EQ(suzume_sizeof_tag_options(), sizeof(suzume_tag_options_t));
  EXPECT_EQ(suzume_sizeof_extended_options(), sizeof(suzume_extended_options_t));

  EXPECT_EQ(suzume_offsetof_result(0), offsetof(suzume_result_t, morphemes));
  EXPECT_EQ(suzume_offsetof_result(1), offsetof(suzume_result_t, count));
  EXPECT_EQ(suzume_offsetof_result(2), offsetof(suzume_result_t, normalized_text));
  EXPECT_EQ(suzume_offsetof_result(3), offsetof(suzume_result_t, normalized_text_size));
  EXPECT_EQ(suzume_offsetof_morpheme(6), offsetof(suzume_morpheme_t, extended_pos));
  EXPECT_EQ(suzume_offsetof_morpheme(2), offsetof(suzume_morpheme_t, start));
  EXPECT_EQ(suzume_offsetof_morpheme(4), offsetof(suzume_morpheme_t, score));
  EXPECT_EQ(suzume_offsetof_morpheme(9), offsetof(suzume_morpheme_t, flags));
  EXPECT_EQ(suzume_offsetof_morpheme(10), offsetof(suzume_morpheme_t, surface_size));
  EXPECT_EQ(suzume_offsetof_morpheme(11), offsetof(suzume_morpheme_t, base_form_size));
  EXPECT_EQ(suzume_offsetof_tags(2), offsetof(suzume_tags_t, count));
  EXPECT_EQ(suzume_offsetof_tag_options(4), offsetof(suzume_tag_options_t, max_tags));
  EXPECT_EQ(suzume_offsetof_tag_options(5), offsetof(suzume_tag_options_t, exclude_particles));
  EXPECT_EQ(suzume_offsetof_tag_options(9), offsetof(suzume_tag_options_t, remove_duplicates));
  EXPECT_EQ(suzume_offsetof_extended_options(0), offsetof(suzume_extended_options_t, preserve_vu));
  EXPECT_EQ(suzume_offsetof_extended_options(3), offsetof(suzume_extended_options_t, mode));
  EXPECT_EQ(suzume_offsetof_extended_options(5), offsetof(suzume_extended_options_t, merge_compounds));
  EXPECT_EQ(suzume_offsetof_extended_options(6), offsetof(suzume_extended_options_t, skip_user_dictionary));
  EXPECT_EQ(suzume_offsetof_extended_options(7), offsetof(suzume_extended_options_t, skip_core_dictionary));
  EXPECT_EQ(suzume_offsetof_extended_options(8), offsetof(suzume_extended_options_t, report_scorer_config));
  EXPECT_EQ(suzume_offsetof_extended_options(9), offsetof(suzume_extended_options_t, skip_env_config));
  EXPECT_EQ(suzume_offsetof_extended_options(10), offsetof(suzume_extended_options_t, scorer_options_json));
  EXPECT_EQ(suzume_offsetof_result(99), static_cast<size_t>(-1));
  EXPECT_EQ(suzume_offsetof_extended_options(99), static_cast<size_t>(-1));
}

}  // namespace
