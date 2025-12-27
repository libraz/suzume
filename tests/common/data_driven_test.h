// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Data-driven test infrastructure using Google Test parameterized tests.

#pragma once

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "json_loader.h"
#include "suzume.h"
#include "test_case.h"

namespace suzume::test {

// Base class for data-driven tokenization tests
// Uses Suzume class (public API) which includes postprocessing for correct lemma
class TokenizationTestBase : public ::testing::TestWithParam<TestCase> {
 protected:
  void SetUp() override {
    // Suzume constructor automatically loads core dictionary
  }

  // Verify that the analysis result matches the expected morphemes
  void verifyMorphemes(const std::vector<core::Morpheme>& result,
                       const std::vector<ExpectedMorpheme>& expected) {
    SCOPED_TRACE("Input: " + GetParam().input);

    // Check count
    ASSERT_EQ(result.size(), expected.size())
        << "Morpheme count mismatch for: " << GetParam().input;

    // Check each morpheme
    for (size_t i = 0; i < expected.size(); ++i) {
      SCOPED_TRACE("Morpheme index: " + std::to_string(i));

      EXPECT_EQ(result[i].surface, expected[i].surface)
          << "Surface mismatch at index " << i;

      if (!expected[i].pos.empty()) {
        EXPECT_EQ(result[i].pos, expected[i].posEnum())
            << "POS mismatch at index " << i << " for surface '"
            << result[i].surface << "'";
      }

      if (!expected[i].lemma.empty()) {
        EXPECT_EQ(result[i].lemma, expected[i].lemma)
            << "Lemma mismatch at index " << i << " for surface '"
            << result[i].surface << "'";
      }
    }
  }

  // Verify only surface forms (when POS/lemma don't matter)
  void verifySurfaces(const std::vector<core::Morpheme>& result,
                      const std::vector<ExpectedMorpheme>& expected) {
    SCOPED_TRACE("Input: " + GetParam().input);

    std::vector<std::string> actual_surfaces;
    for (const auto& m : result) {
      actual_surfaces.push_back(m.surface);
    }

    std::vector<std::string> expected_surfaces;
    for (const auto& e : expected) {
      expected_surfaces.push_back(e.surface);
    }

    EXPECT_EQ(actual_surfaces, expected_surfaces)
        << "Surface mismatch for: " << GetParam().input;
  }

  Suzume analyzer_;
};

// Helper to generate test name from TestCase
struct TestCaseNameGenerator {
  std::string operator()(
      const ::testing::TestParamInfo<TestCase>& info) const {
    std::string name = info.param.id;
    // Replace invalid characters for test names
    for (char& c : name) {
      if (!std::isalnum(c) && c != '_') {
        c = '_';
      }
    }
    return name;
  }
};

// Macro to instantiate parameterized tests from JSON file
#define INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(TestClass, SuiteName, \
                                                 JsonPath)             \
  INSTANTIATE_TEST_SUITE_P(                                            \
      SuiteName, TestClass,                                            \
      ::testing::ValuesIn(                                             \
          suzume::test::JsonLoader::loadFromFile(JsonPath).cases),     \
      suzume::test::TestCaseNameGenerator())

// Macro to instantiate parameterized tests from JSON file with tag filter
#define INSTANTIATE_TOKENIZATION_TEST_FROM_JSON_WITH_TAG(               \
    TestClass, SuiteName, JsonPath, Tag)                                \
  INSTANTIATE_TEST_SUITE_P(                                             \
      SuiteName, TestClass,                                             \
      ::testing::ValuesIn(                                              \
          suzume::test::JsonLoader::loadFromFile(JsonPath).filterByTag( \
              Tag)),                                                    \
      suzume::test::TestCaseNameGenerator())

}  // namespace suzume::test
