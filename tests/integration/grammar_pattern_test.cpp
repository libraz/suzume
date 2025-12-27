#include "data_driven_test.h"

namespace suzume::test {
namespace {

class GrammarPatternTest : public TokenizationTestBase {};

TEST_P(GrammarPatternTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(GrammarPatternTest, GrammarPattern,
                                        "tests/data/tokenization/grammar_pattern.json");

}  // namespace
}  // namespace suzume::test
