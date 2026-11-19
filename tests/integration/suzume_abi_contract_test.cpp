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
       {{0, 0, "surface"},
        {1, 8, "base_form"},
        {2, 16, "start"},
        {3, 20, "end"},
        {4, 24, "score"},
        {5, 28, "pos"},
        {6, 29, "extended_pos"},
        {7, 30, "conjugation_type"},
        {8, 31, "conjugation_form"},
        {9, 32, "flags"},
        {10, 40, "surface_size"},
        {11, 48, "base_form_size"}}},
      {"suzume_result_t",
       32,
       suzume_sizeof_result,
       suzume_offsetof_result,
       {{0, 0, "morphemes"}, {1, 8, "count"}, {2, 16, "normalized_text"}, {3, 24, "normalized_text_size"}}},
      {"suzume_tags_t",
       24,
       suzume_sizeof_tags,
       suzume_offsetof_tags,
       {{0, 0, "tags"}, {1, 8, "pos"}, {2, 16, "count"}}},
      {"suzume_tag_options_t",
       32,
       suzume_sizeof_tag_options,
       suzume_offsetof_tag_options,
       {{0, 0, "pos_filter"},
        {1, 1, "exclude_basic"},
        {2, 2, "use_lemma"},
        {3, 8, "min_length"},
        {4, 16, "max_tags"},
        {5, 24, "exclude_particles"},
        {6, 25, "exclude_auxiliaries"},
        {7, 26, "exclude_formal_nouns"},
        {8, 27, "exclude_low_info"},
        {9, 28, "remove_duplicates"}}},
      {"suzume_extended_options_t",
       32,
       suzume_sizeof_extended_options,
       suzume_offsetof_extended_options,
       {{0, 0, "preserve_vu"},
        {1, 1, "preserve_case"},
        {2, 2, "preserve_symbols"},
        {3, 3, "mode"},
        {4, 4, "lemmatize"},
        {5, 5, "merge_compounds"},
        {6, 6, "skip_user_dictionary"},
        {7, 7, "skip_core_dictionary"},
        {8, 8, "report_scorer_config"},
        {9, 9, "skip_env_config"},
        {10, 16, "scorer_options_json"},
        {11, 24, "data_directory"}}},
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
