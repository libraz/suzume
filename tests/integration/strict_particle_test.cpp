#include "data_driven_test.h"

namespace suzume::test {
namespace {

class StrictParticleTest : public TokenizationTestBase {};

TEST_P(StrictParticleTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(StrictParticleTest, StrictParticle,
                                        "tests/data/tokenization/strict_particle.json");

}  // namespace
}  // namespace suzume::test
