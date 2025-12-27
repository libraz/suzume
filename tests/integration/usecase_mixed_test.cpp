#include "data_driven_test.h"

namespace suzume::test {
namespace {

class UsecaseMixedTest : public TokenizationTestBase {};

TEST_P(UsecaseMixedTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(UsecaseMixedTest, UsecaseMixed,
                                        "tests/data/tokenization/usecase_mixed.json");

}  // namespace
}  // namespace suzume::test
