// Regression tests for compound nouns and script boundaries

#include "data_driven_test.h"

namespace suzume::test {
namespace {

class CompoundRegressionTest : public TokenizationTestBase {};

TEST_P(CompoundRegressionTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    CompoundRegressionTest, RegressionCompound,
    "tests/data/tokenization/regression_compound.json");

}  // namespace
}  // namespace suzume::test
