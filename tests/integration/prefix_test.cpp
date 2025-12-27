#include "data_driven_test.h"

namespace suzume::test {
namespace {

class PrefixTest : public TokenizationTestBase {};

TEST_P(PrefixTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(PrefixTest, Prefix,
                                        "tests/data/tokenization/prefix.json");

}  // namespace
}  // namespace suzume::test
