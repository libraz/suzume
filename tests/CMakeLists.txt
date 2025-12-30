# Tests CMakeLists.txt

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Include Google Test module
include(GoogleTest)

# Common test utilities
set(TEST_COMMON_SOURCES
  common/test_helpers.cpp
  common/test_case.cpp
)

# Test source files
set(TEST_SOURCES
  ${TEST_COMMON_SOURCES}
  core/types_test.cpp
  normalize/utf8_test.cpp
  normalize/normalizer_test.cpp
  dictionary/trie_test.cpp
  dictionary/double_array_test.cpp
  dictionary/binary_dict_test.cpp
  dictionary/core_dict_test.cpp
  dictionary/user_dict_test.cpp
  grammar/conjugation_test.cpp
  grammar/inflection_basic_test.cpp
  grammar/inflection_potential_test.cpp
  grammar/inflection_compound_test.cpp
  grammar/inflection_complex_test.cpp
  grammar/inflection_imperative_test.cpp
  grammar/auxiliary_generator_test.cpp
  grammar/patterns_test.cpp
  pretokenizer/pretokenizer_url_test.cpp
  pretokenizer/pretokenizer_number_test.cpp
  pretokenizer/pretokenizer_text_test.cpp
  analysis/connection_rules_test.cpp
  analysis/scorer_options_loader_test.cpp
  output/japanese_format_test.cpp
  postprocess/tag_generator_test.cpp
  # Universal test: auto-discovers all JSON files in tests/data/tokenization/
  # New JSON files are automatically picked up without creating C++ files
  integration/universal_tokenization_test.cpp
)

# Create test executable
add_executable(suzume_test ${TEST_SOURCES})

# Include directories
target_include_directories(suzume_test PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/common
)

# Link libraries
target_link_libraries(suzume_test PRIVATE
  suzume
  suzume_core
  suzume_normalize
  suzume_dictionary
  suzume_grammar
  suzume_pretokenizer
  suzume_analysis
  suzume_postprocess
  GTest::gtest_main
)

# Register tests with working directory set to project root
# This allows tests to access data files via relative paths (e.g., "data/core/verbs.tsv")
gtest_discover_tests(suzume_test
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
