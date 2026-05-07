#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>

#include "suzume_c.h"

namespace {

TEST(SuzumeCApiTest, LastErrorReportsInvalidArguments) {
  EXPECT_EQ(suzume_analyze(nullptr, "test"), nullptr);

  std::string error = suzume_last_error();
  EXPECT_NE(error.find("null handle"), std::string::npos);
}

TEST(SuzumeCApiTest, LastErrorClearsAfterSuccess) {
  EXPECT_EQ(suzume_analyze(nullptr, "test"), nullptr);
  ASSERT_STRNE(suzume_last_error(), "");

  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  suzume_result_t* result = suzume_analyze(handle, "東京");
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(suzume_last_error(), "");

  suzume_result_free(result);
  suzume_destroy(handle);
}

TEST(SuzumeCApiTest, LoadUserDictReportsParseDetails) {
  suzume_t handle = suzume_create();
  ASSERT_NE(handle, nullptr);

  const char* csv_data = "\"東京,NOUN,0.5\n";
  EXPECT_EQ(suzume_load_user_dict(handle, csv_data, std::strlen(csv_data)), 0);

  std::string error = suzume_last_error();
  EXPECT_NE(error.find("Invalid CSV quoting"), std::string::npos);
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

TEST(SuzumeCApiTest, InitExtendedOptionsPreservesDefaultTrueFields) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);

  EXPECT_EQ(options.size, sizeof(options));
  EXPECT_EQ(options.preserve_vu, 1);
  EXPECT_EQ(options.preserve_case, 1);
  EXPECT_EQ(options.preserve_symbols, 0);
  EXPECT_EQ(options.mode, 0);
  EXPECT_EQ(options.lemmatize, 1);
  EXPECT_EQ(options.merge_compounds, 0);
}

TEST(SuzumeCApiTest, CreateWithExtendedOptionsRejectsInvalidMode) {
  suzume_extended_options_t options{};
  suzume_init_extended_options(&options);
  options.mode = 99;

  suzume_t handle = suzume_create_with_extended_options(&options);
  EXPECT_EQ(handle, nullptr);

  std::string error = suzume_last_error();
  EXPECT_NE(error.find("invalid mode"), std::string::npos);
}

TEST(SuzumeCApiTest, FreeNullPointersAreNoOps) {
  suzume_destroy(nullptr);
  suzume_result_free(nullptr);
  suzume_tags_free(nullptr);
  suzume_free(nullptr);
}

TEST(SuzumeCApiTest, LayoutFunctionsMatchNativeStructs) {
  EXPECT_EQ(suzume_sizeof_result(), sizeof(suzume_result_t));
  EXPECT_EQ(suzume_sizeof_morpheme(), sizeof(suzume_morpheme_t));
  EXPECT_EQ(suzume_sizeof_tags(), sizeof(suzume_tags_t));
  EXPECT_EQ(suzume_sizeof_tag_options(), sizeof(suzume_tag_options_t));
  EXPECT_EQ(suzume_sizeof_extended_options(), sizeof(suzume_extended_options_t));

  EXPECT_EQ(suzume_offsetof_result(0), offsetof(suzume_result_t, morphemes));
  EXPECT_EQ(suzume_offsetof_result(1), offsetof(suzume_result_t, count));
  EXPECT_EQ(suzume_offsetof_morpheme(6), offsetof(suzume_morpheme_t, extended_pos));
  EXPECT_EQ(suzume_offsetof_tags(2), offsetof(suzume_tags_t, count));
  EXPECT_EQ(suzume_offsetof_tag_options(4), offsetof(suzume_tag_options_t, max_tags));
  EXPECT_EQ(suzume_offsetof_extended_options(0), offsetof(suzume_extended_options_t, size));
  EXPECT_EQ(suzume_offsetof_extended_options(4), offsetof(suzume_extended_options_t, mode));
  EXPECT_EQ(suzume_offsetof_extended_options(6), offsetof(suzume_extended_options_t, merge_compounds));
  EXPECT_EQ(suzume_offsetof_result(99), static_cast<size_t>(-1));
  EXPECT_EQ(suzume_offsetof_extended_options(99), static_cast<size_t>(-1));
}

}  // namespace
