// Regression tests for particle separation and recognition.

#include "data_driven_test.h"

namespace suzume::test {
namespace {

class ParticleRegressionTest : public TokenizationTestBase {};

TEST_P(ParticleRegressionTest, Tokenize) {
  const auto& tc = GetParam();
  auto result = analyzer_.analyze(tc.input);
  verifyMorphemes(result, tc.expected);
}

INSTANTIATE_TOKENIZATION_TEST_FROM_JSON(
    ParticleRegressionTest, RegressionParticle,
    "tests/data/tokenization/regression_particle.json");

}  // namespace
}  // namespace suzume::test
