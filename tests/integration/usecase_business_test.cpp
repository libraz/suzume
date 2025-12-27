#include "data_driven_test.h"

namespace suzume::test {
namespace {

class UsecaseBusinessTest : public TokenizationTestBase {};

TEST_P(UsecaseBusinessTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(UsecaseBusinessTest, UsecaseBusiness,
                                        "tests/data/tokenization/usecase_business.json");

}  // namespace
}  // namespace suzume::test
