#include "data_driven_test.h"

namespace suzume::test {
namespace {

class UsecaseConversationTest : public TokenizationTestBase {};

TEST_P(UsecaseConversationTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(UsecaseConversationTest, UsecaseConversation,
                                        "tests/data/tokenization/usecase_conversation.json");

}  // namespace
}  // namespace suzume::test
