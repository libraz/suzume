// Regression tests for adjective recognition (i-adjectives, na-adjectives)

#include "data_driven_test.h"

namespace suzume::test {
namespace {

class AdjectiveRegressionTest : public TokenizationTestBase {};

TEST_P(AdjectiveRegressionTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    AdjectiveRegressionTest, RegressionAdjective,
    "tests/data/tokenization/regression_adjective.json");

}  // namespace
}  // namespace suzume::test
