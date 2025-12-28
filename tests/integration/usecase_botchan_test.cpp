#include "data_driven_test.h"

namespace suzume::test {
namespace {

class UsecaseBotchanTest : public TokenizationTestBase {};

TEST_P(UsecaseBotchanTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(UsecaseBotchanTest, UsecaseBotchan,
                                        "tests/data/tokenization/usecase_botchan.json");

}  // namespace
}  // namespace suzume::test
