#include "data_driven_test.h"

namespace suzume::test {
namespace {

class CopulaRegressionTest : public TokenizationTestBase {};

TEST_P(CopulaRegressionTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    CopulaRegressionTest, RegressionCopula,
    "tests/data/tokenization/regression_copula.json");

}  // namespace
}  // namespace suzume::test
