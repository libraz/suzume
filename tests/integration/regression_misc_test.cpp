// Regression tests for misc items (auxiliaries, adverbs, time nouns, etc.)

#include "data_driven_test.h"

namespace suzume::test {
namespace {

class MiscRegressionTest : public TokenizationTestBase {};

TEST_P(MiscRegressionTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    MiscRegressionTest, RegressionMisc,
    "tests/data/tokenization/regression_misc.json");

}  // namespace
}  // namespace suzume::test
