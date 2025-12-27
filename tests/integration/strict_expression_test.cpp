#include "data_driven_test.h"

namespace suzume::test {
namespace {

class StrictExpressionTest : public TokenizationTestBase {};

TEST_P(StrictExpressionTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(StrictExpressionTest, StrictExpression,
                                        "tests/data/tokenization/strict_expression.json");

}  // namespace
}  // namespace suzume::test
