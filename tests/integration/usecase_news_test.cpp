#include "data_driven_test.h"

namespace suzume::test {
namespace {

class UsecaseNewsTest : public TokenizationTestBase {};

TEST_P(UsecaseNewsTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(UsecaseNewsTest, UsecaseNews,
                                        "tests/data/tokenization/usecase_news.json");

}  // namespace
}  // namespace suzume::test
