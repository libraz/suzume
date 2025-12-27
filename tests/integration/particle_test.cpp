#include "data_driven_test.h"

namespace suzume::test {
namespace {

class ParticleTest : public TokenizationTestBase {};

TEST_P(ParticleTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(ParticleTest, Particle,
                                        "tests/data/tokenization/particle.json");

}  // namespace
}  // namespace suzume::test
