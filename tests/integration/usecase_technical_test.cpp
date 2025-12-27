#include "data_driven_test.h"

namespace suzume::test {
namespace {

class UsecaseTechnicalTest : public TokenizationTestBase {};

TEST_P(UsecaseTechnicalTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(UsecaseTechnicalTest, UsecaseTechnical,
                                        "tests/data/tokenization/usecase_technical.json");

}  // namespace
}  // namespace suzume::test
