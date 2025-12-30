// Regression tests for bugs fixed on 2025-12-30 (B23-B35)
// See backup/bug_20251230_sample_text_testing.md for details

#include "data_driven_test.h"

namespace suzume::test {
namespace {

class Bugs20251230Test : public TokenizationTestBase {};

TEST_P(Bugs20251230Test, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    Bugs20251230Test, Bugs20251230,
    "tests/data/tokenization/regression_bugs_20251230.json");

}  // namespace
}  // namespace suzume::test
