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

  EXPECT_EQ(suzume_offsetof_result(0), offsetof(suzume_result_t, morphemes));
  EXPECT_EQ(suzume_offsetof_result(1), offsetof(suzume_result_t, count));
  EXPECT_EQ(suzume_offsetof_morpheme(6), offsetof(suzume_morpheme_t, extended_pos));
  EXPECT_EQ(suzume_offsetof_tags(2), offsetof(suzume_tags_t, count));
  EXPECT_EQ(suzume_offsetof_tag_options(4), offsetof(suzume_tag_options_t, max_tags));
  EXPECT_EQ(suzume_offsetof_result(99), static_cast<size_t>(-1));
}

}  // namespace
