#include "data_driven_test.h"

namespace suzume::test {
namespace {

class BasicTest : public TokenizationTestBase {};

TEST_P(BasicTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(BasicTest, Basic,
                                        "tests/data/tokenization/basic.json");

}  // namespace
}  // namespace suzume::test
