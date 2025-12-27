#include "data_driven_test.h"

namespace suzume::test {
namespace {

class StrictWordTest : public TokenizationTestBase {};

TEST_P(StrictWordTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(StrictWordTest, StrictWord,
                                        "tests/data/tokenization/strict_word.json");

}  // namespace
}  // namespace suzume::test
