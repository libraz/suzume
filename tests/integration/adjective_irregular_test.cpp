// Tests for irregular adjectives (いい/よい)

#include "data_driven_test.h"

namespace suzume::test {
namespace {

class AdjectiveIrregularTest : public TokenizationTestBase {};

TEST_P(AdjectiveIrregularTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    AdjectiveIrregularTest, IrregularAdjective,
    "tests/data/tokenization/adjective_irregular.json");

}  // namespace
}  // namespace suzume::test
