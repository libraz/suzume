// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Sample data-driven test demonstrating the JSON-based test infrastructure.

#include "data_driven_test.h"

namespace suzume::test {
namespace {

// Simple tokenization test using data-driven approach
class SampleTokenizationTest : public TokenizationTestBase {};

TEST_P(SampleTokenizationTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

// Load test cases from JSON file
INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(SampleTokenizationTest, Sample,
                                        "tests/data/tokenization/sample.json");

}  // namespace
}  // namespace suzume::test
