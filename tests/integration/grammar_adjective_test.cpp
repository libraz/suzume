#include "data_driven_test.h"

namespace suzume::test {
namespace {

class GrammarAdjectiveTest : public TokenizationTestBase {};

TEST_P(GrammarAdjectiveTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(GrammarAdjectiveTest, GrammarAdjective,
                                        "tests/data/tokenization/grammar_adjective.json");

}  // namespace
}  // namespace suzume::test
