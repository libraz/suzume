#include "data_driven_test.h"

namespace suzume::test {
namespace {

// Suru verb imperative form tokenization test
// Ensures that サ変動詞+しろ/せよ is recognized as a single token
class SuruImperativeTest : public TokenizationTestBase {};

TEST_P(SuruImperativeTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    SuruImperativeTest, SuruImperative,
    "tests/data/tokenization/suru_imperative.json");

}  // namespace
}  // namespace suzume::test
