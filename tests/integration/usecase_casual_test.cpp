#include "data_driven_test.h"

namespace suzume::test {
namespace {

class UsecaseCasualTest : public TokenizationTestBase {};

TEST_P(UsecaseCasualTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(UsecaseCasualTest, UsecaseCasual,
                                        "tests/data/tokenization/usecase_casual.json");

}  // namespace
}  // namespace suzume::test
