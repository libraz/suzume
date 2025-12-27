#include "data_driven_test.h"

namespace suzume::test {
namespace {

class UsecaseRealworldTest : public TokenizationTestBase {};

TEST_P(UsecaseRealworldTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(UsecaseRealworldTest, UsecaseRealworld,
                                        "tests/data/tokenization/usecase_realworld.json");

}  // namespace
}  // namespace suzume::test
