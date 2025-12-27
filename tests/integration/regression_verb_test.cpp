// Regression tests for verb recognition (conjugation, honorifics, te-form, etc.)

#include "data_driven_test.h"

namespace suzume::test {
namespace {

class VerbRegressionTest : public TokenizationTestBase {};

TEST_P(VerbRegressionTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    VerbRegressionTest, RegressionVerb,
    "tests/data/tokenization/regression_verb.json");

}  // namespace
}  // namespace suzume::test
