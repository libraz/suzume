#include "data_driven_test.h"

namespace suzume::test {
namespace {

class VerbTest : public TokenizationTestBase {};

TEST_P(VerbTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(VerbTest, Verb,
                                        "tests/data/tokenization/verb.json");

}  // namespace
}  // namespace suzume::test
