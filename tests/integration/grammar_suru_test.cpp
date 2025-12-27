#include "data_driven_test.h"

namespace suzume::test {
namespace {

class GrammarSuruTest : public TokenizationTestBase {};

TEST_P(GrammarSuruTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(GrammarSuruTest, GrammarSuru,
                                        "tests/data/tokenization/grammar_suru.json");

}  // namespace
}  // namespace suzume::test
