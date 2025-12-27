#include "data_driven_test.h"

namespace suzume::test {
namespace {

class SlangTest : public TokenizationTestBase {};

TEST_P(SlangTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(SlangTest, Slang,
                                        "tests/data/tokenization/slang.json");

}  // namespace
}  // namespace suzume::test
