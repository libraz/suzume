#include "data_driven_test.h"

namespace suzume::test {
namespace {

class StrictGreetingTest : public TokenizationTestBase {};

TEST_P(StrictGreetingTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(StrictGreetingTest, StrictGreeting,
                                        "tests/data/tokenization/strict_greeting.json");

}  // namespace
}  // namespace suzume::test
