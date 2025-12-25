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

# Test source files
set(TEST_SOURCES
  core/types_test.cpp
  normalize/utf8_test.cpp
  normalize/normalizer_test.cpp
  dictionary/trie_test.cpp
  dictionary/double_array_test.cpp
  dictionary/binary_dict_test.cpp
  dictionary/core_dict_test.cpp
  grammar/inflection_test.cpp
  pretokenizer/pretokenizer_test.cpp
  analysis/analyzer_basic_test.cpp
  analysis/analyzer_particle_test.cpp
  analysis/analyzer_grammar_test.cpp
  analysis/analyzer_verb_test.cpp
  analysis/analyzer_usecase_business_test.cpp
  analysis/analyzer_usecase_conversation_test.cpp
  analysis/analyzer_usecase_technical_test.cpp
  analysis/analyzer_usecase_news_test.cpp
  analysis/analyzer_usecase_casual_test.cpp
  analysis/analyzer_usecase_mixed_test.cpp
  analysis/analyzer_usecase_realworld_test.cpp
  analysis/analyzer_regression_test.cpp
  analysis/analyzer_strict_test.cpp
  analysis/analyzer_prefix_test.cpp
)

# Create test executable
add_executable(suzume_test ${TEST_SOURCES})

# Include directories
target_include_directories(suzume_test PRIVATE
  ${CMAKE_SOURCE_DIR}/src
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
