#include "data_driven_test.h"

namespace suzume::test {
namespace {

class StrictCompoundTest : public TokenizationTestBase {};

TEST_P(StrictCompoundTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(StrictCompoundTest, StrictCompound,
                                        "tests/data/tokenization/strict_compound.json");

}  // namespace
}  // namespace suzume::test
