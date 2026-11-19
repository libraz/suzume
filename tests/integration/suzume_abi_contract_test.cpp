// Freezes the public C ABI: the revision number and the byte layout it names.
//
// An out-of-tree consumer (the Go binding lives in its own repository) compiles
// against include/suzume/suzume_c.h and links or dlopens whatever libsuzume it
// finds. A removed symbol fails loudly, but a struct that changes size or field
// order does not: the consumer keeps reading the old offsets out of the new
// layout. SUZUME_ABI_VERSION is the only signal it has, so the layout table and
// the revision must move together, which is why both live in this one file.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "suzume/suzume_c.h"

namespace {

// The revision the table below describes. Bumping SUZUME_ABI_VERSION without
// updating this constant, or the reverse, fails AbiVersionMatchesFrozenLayout.
constexpr uint32_t kFrozenAbiVersion = 2;

constexpr const char* kBumpInstruction =
    "If this layout change was deliberate, bump SUZUME_ABI_VERSION in "
    "include/suzume/suzume_c.h and update the frozen table in this file, both in "
    "the same commit. Out-of-tree consumers compare that number to detect the "
    "change; nothing else does.";

struct FieldLayout {
  uint32_t index;
  size_t offset;
  const char* name;
};

struct StructLayout {
  const char* name;
  size_t size;
  size_t (*size_oracle)(void);
  size_t (*offset_oracle)(uint32_t);
  std::vector<FieldLayout> fields;
};

// Frozen for the LP64 data model (8-byte pointer and size_t), which is what
// every native consumer of this library uses. A 32-bit target lays these structs
// out differently, so the byte-layout test skips there; the WebAssembly build
// carries no out-of-tree consumer to protect, since the npm package ships the
// module and its JS binding together.
std::vector<StructLayout> frozenLayouts() {
  return {
      {"suzume_morpheme_t",
       56,
       suzume_sizeof_morpheme,
       suzume_offsetof_morpheme,
       {{SUZUME_MORPHEME_FIELD_SURFACE, 0, "surface"},
        {SUZUME_MORPHEME_FIELD_BASE_FORM, 8, "base_form"},
        {SUZUME_MORPHEME_FIELD_START, 16, "start"},
        {SUZUME_MORPHEME_FIELD_END, 20, "end"},
        {SUZUME_MORPHEME_FIELD_SCORE, 24, "score"},
        {SUZUME_MORPHEME_FIELD_POS, 28, "pos"},
        {SUZUME_MORPHEME_FIELD_EXTENDED_POS, 29, "extended_pos"},
        {SUZUME_MORPHEME_FIELD_CONJUGATION_TYPE, 30, "conjugation_type"},
        {SUZUME_MORPHEME_FIELD_CONJUGATION_FORM, 31, "conjugation_form"},
        {SUZUME_MORPHEME_FIELD_FLAGS, 32, "flags"},
        {SUZUME_MORPHEME_FIELD_SURFACE_SIZE, 40, "surface_size"},
        {SUZUME_MORPHEME_FIELD_BASE_FORM_SIZE, 48, "base_form_size"}}},
      {"suzume_result_t",
       32,
       suzume_sizeof_result,
       suzume_offsetof_result,
       {{SUZUME_RESULT_FIELD_MORPHEMES, 0, "morphemes"},
        {SUZUME_RESULT_FIELD_COUNT, 8, "count"},
        {SUZUME_RESULT_FIELD_NORMALIZED_TEXT, 16, "normalized_text"},
        {SUZUME_RESULT_FIELD_NORMALIZED_TEXT_SIZE, 24, "normalized_text_size"}}},
      {"suzume_tags_t",
       24,
       suzume_sizeof_tags,
       suzume_offsetof_tags,
       {{SUZUME_TAGS_FIELD_TAGS, 0, "tags"},
        {SUZUME_TAGS_FIELD_POS, 8, "pos"},
        {SUZUME_TAGS_FIELD_COUNT, 16, "count"}}},
      {"suzume_tag_options_t",
       32,
       suzume_sizeof_tag_options,
       suzume_offsetof_tag_options,
       {{SUZUME_TAG_OPTIONS_FIELD_POS_FILTER, 0, "pos_filter"},
        {SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_BASIC, 1, "exclude_basic"},
        {SUZUME_TAG_OPTIONS_FIELD_USE_BASE_FORM, 2, "use_base_form"},
        {SUZUME_TAG_OPTIONS_FIELD_MIN_LENGTH, 8, "min_length"},
        {SUZUME_TAG_OPTIONS_FIELD_MAX_TAGS, 16, "max_tags"},
        {SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_PARTICLES, 24, "exclude_particles"},
        {SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_AUXILIARIES, 25, "exclude_auxiliaries"},
        {SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_FORMAL_NOUNS, 26, "exclude_formal_nouns"},
        {SUZUME_TAG_OPTIONS_FIELD_EXCLUDE_LOW_INFO, 27, "exclude_low_info"},
        {SUZUME_TAG_OPTIONS_FIELD_REMOVE_DUPLICATES, 28, "remove_duplicates"}}},
      {"suzume_options_t",
       32,
       suzume_sizeof_options,
       suzume_offsetof_options,
       {{SUZUME_OPTIONS_FIELD_PRESERVE_VU, 0, "preserve_vu"},
        {SUZUME_OPTIONS_FIELD_PRESERVE_CASE, 1, "preserve_case"},
        {SUZUME_OPTIONS_FIELD_PRESERVE_SYMBOLS, 2, "preserve_symbols"},
        {SUZUME_OPTIONS_FIELD_MODE, 3, "mode"},
        {SUZUME_OPTIONS_FIELD_LEMMATIZE, 4, "lemmatize"},
        {SUZUME_OPTIONS_FIELD_MERGE_COMPOUNDS, 5, "merge_compounds"},
        {SUZUME_OPTIONS_FIELD_SKIP_BUNDLED_USER_DICTIONARY, 6, "skip_bundled_user_dictionary"},
        {SUZUME_OPTIONS_FIELD_SKIP_CORE_DICTIONARY, 7, "skip_core_dictionary"},
        {SUZUME_OPTIONS_FIELD_REPORT_SCORER_CONFIG, 8, "report_scorer_config"},
        {SUZUME_OPTIONS_FIELD_SKIP_ENV_CONFIG, 9, "skip_env_config"},
        {SUZUME_OPTIONS_FIELD_SCORER_OPTIONS_JSON, 16, "scorer_options_json"},
        {SUZUME_OPTIONS_FIELD_DATA_DIRECTORY, 24, "data_directory"}}},
  };
}

TEST(SuzumeAbiContractTest, AbiVersionMatchesFrozenLayout) {
  EXPECT_EQ(static_cast<uint32_t>(SUZUME_ABI_VERSION), kFrozenAbiVersion)
      << "The header's ABI revision moved but the frozen layout table in this file did not. "
      << "Update the table to describe revision " << SUZUME_ABI_VERSION << ", then set kFrozenAbiVersion to match.";
  EXPECT_EQ(suzume_abi_version(), static_cast<uint32_t>(SUZUME_ABI_VERSION))
      << "suzume_abi_version() must return the header's SUZUME_ABI_VERSION; a consumer compares the two "
      << "to decide whether the library it loaded is safe to call.";
}

static_assert(std::is_same_v<suzume_mode_t, uint8_t>, "suzume_mode_t must stay one byte");
static_assert(std::is_same_v<suzume_tag_pos_t, uint8_t>, "suzume_tag_pos_t must stay one byte");
static_assert(std::is_same_v<decltype(suzume_options_t::mode), suzume_mode_t>);
static_assert(std::is_same_v<decltype(suzume_tag_options_t::pos_filter), suzume_tag_pos_t>);

TEST(SuzumeAbiContractTest, PublicStructsKeepTheirFrozenByteLayout) {
  if (sizeof(void*) != 8 || sizeof(size_t) != 8) {
    GTEST_SKIP() << "The frozen table describes the LP64 layout; this target is not LP64.";
  }

  for (const StructLayout& layout : frozenLayouts()) {
    const std::string prefix = std::string(layout.name) + ": ";
    EXPECT_EQ(layout.size_oracle(), layout.size) << prefix << "size changed. " << kBumpInstruction;
    for (const FieldLayout& field : layout.fields) {
      EXPECT_EQ(layout.offset_oracle(field.index), field.offset)
          << prefix << "field " << field.index << " (" << field.name << ") moved. " << kBumpInstruction;
    }
    EXPECT_EQ(layout.offset_oracle(static_cast<uint32_t>(layout.fields.size())), static_cast<size_t>(-1))
        << prefix << "a field was appended without extending the frozen table. " << kBumpInstruction;
  }
}

}  // namespace
