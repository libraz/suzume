/**
 * @file scorer_options_loader_test.cpp
 * @brief Tests for scorer options JSON loader
 */

#include "analysis/scorer_options_loader.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "analysis/scorer_bigram_overrides.h"

namespace suzume::analysis {
namespace {

// Helper to create temporary JSON file
class TempJsonFile {
 public:
  explicit TempJsonFile(const std::string& content) {
    path_ = "/tmp/scorer_test_" + std::to_string(reinterpret_cast<uintptr_t>(this)) + ".json";
    std::ofstream file(path_);
    file << content;
  }

  ~TempJsonFile() { std::remove(path_.c_str()); }

  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

// =============================================================================
// JSON Parser Tests
// =============================================================================

class JsonParserTest : public ::testing::Test {};

TEST_F(JsonParserTest, LoadEmptyObject) {
  TempJsonFile file("{}");
  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
}

TEST_F(JsonParserTest, LoadCandidatesJoin) {
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -0.7,
        "verified_v1_bonus": -0.3
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -0.7F);
  EXPECT_FLOAT_EQ(opts.candidates.join.verified_v1_bonus, -0.3F);
}

TEST_F(JsonParserTest, LoadCandidatesSplit) {
  TempJsonFile file(R"({
    "candidates": {
      "split": {
        "alpha_kanji_bonus": -0.4,
        "digit_kanji_1_bonus": -0.6,
        "split_base_cost": 1.5
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.candidates.split.alpha_kanji_bonus, -0.4F);
  EXPECT_FLOAT_EQ(opts.candidates.split.digit_kanji_1_bonus, -0.6F);
  EXPECT_FLOAT_EQ(opts.candidates.split.split_base_cost, 1.5F);
}

TEST_F(JsonParserTest, LoadFullConfig) {
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -0.9
      },
      "split": {
        "alpha_kanji_bonus": -0.35
      }
    },
    "unary": {
      "noun_prior": 0.1,
      "verb_prior": 0.3
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -0.9F);
  EXPECT_FLOAT_EQ(opts.candidates.split.alpha_kanji_bonus, -0.35F);
  EXPECT_FLOAT_EQ(opts.noun_prior, 0.1F);
  EXPECT_FLOAT_EQ(opts.verb_prior, 0.3F);
}

TEST_F(JsonParserTest, PartialOverridePreservesDefaults) {
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -1.5
      }
    }
  })");

  ScorerOptions opts;
  // Set some non-default values before loading
  opts.candidates.split.alpha_kanji_bonus = -0.123F;

  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));

  // Check that loaded value is applied
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -1.5F);

  // Check that non-loaded value is preserved
  EXPECT_FLOAT_EQ(opts.candidates.split.alpha_kanji_bonus, -0.123F);
}

TEST_F(JsonParserTest, LoadEveryBigramOverride) {
  std::string json = R"({"bigram":{)";
  for (size_t index = 0; index < kBigramOverrideSpecs.size(); ++index) {
    if (index != 0) {
      json += ',';
    }
    json += '"';
    json += kBigramOverrideSpecs[index].name;
    json += R"(":)";
    json += std::to_string(static_cast<float>(index) + 0.25F);
  }
  json += "}}";

  TempJsonFile file(json);
  ScorerOptions opts;
  ASSERT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));

  for (size_t index = 0; index < kBigramOverrideSpecs.size(); ++index) {
    const BigramOverrideSpec& spec = kBigramOverrideSpecs[index];
    EXPECT_FLOAT_EQ(opts.bigram.*(spec.value), static_cast<float>(index) + 0.25F) << spec.name;
  }
}

TEST_F(JsonParserTest, LoadJsonStringAndEveryVerbCandidateOption) {
  const std::string json = R"({
    "verb_candidates": {
      "confidence_low": 1,
      "confidence_standard": 2,
      "confidence_past_te": 3,
      "confidence_ichidan_dict": 4,
      "confidence_short_godan_base": 5,
      "confidence_dict_verb": 6,
      "confidence_katakana": 7,
      "confidence_high": 8,
      "confidence_very_high": 9,
      "base_cost_standard": 10,
      "base_cost_high": 11,
      "base_cost_low": 12,
      "base_cost_verified": 13,
      "base_cost_long_verified": 14,
      "bonus_ichidan": 15,
      "bonus_long_dict": 16,
      "bonus_long_verified": 17,
      "penalty_single_char": 18,
      "confidence_cost_scale": 19,
      "confidence_cost_scale_small": 20,
      "confidence_cost_scale_medium": 21,
      "base_cost_medium": 22
    }
  })";

  ScorerOptions opts;
  ASSERT_TRUE(ScorerOptionsLoader::loadFromJsonString(json, opts));
  EXPECT_FLOAT_EQ(opts.candidates.verb.confidence_low, 1.0F);
  EXPECT_FLOAT_EQ(opts.candidates.verb.confidence_cost_scale, 19.0F);
  EXPECT_FLOAT_EQ(opts.candidates.verb.confidence_cost_scale_small, 20.0F);
  EXPECT_FLOAT_EQ(opts.candidates.verb.confidence_cost_scale_medium, 21.0F);
  EXPECT_FLOAT_EQ(opts.candidates.verb.base_cost_medium, 22.0F);
}

TEST_F(JsonParserTest, LoadsPreviouslyUnreachableInflectionOptions) {
  ScorerOptions opts;
  ASSERT_TRUE(ScorerOptionsLoader::loadFromJsonString(
      R"({"inflection":{"penalty_i_adj_ru_stem_invalid":1.25,"penalty_godan_te_stem":2.5}})", opts));
  EXPECT_FLOAT_EQ(opts.inflection.penalty_i_adj_ru_stem_invalid, 1.25F);
  EXPECT_FLOAT_EQ(opts.inflection.penalty_godan_te_stem, 2.5F);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

class ErrorHandlingTest : public ::testing::Test {};

TEST_F(ErrorHandlingTest, NonexistentFile) {
  ScorerOptions opts;
  EXPECT_FALSE(ScorerOptionsLoader::loadFromFile("/nonexistent/path/file.json", opts));
}

TEST_F(ErrorHandlingTest, InvalidJsonSyntax) {
  TempJsonFile file("{invalid json}");
  ScorerOptions opts;
  EXPECT_FALSE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
}

TEST_F(ErrorHandlingTest, NonObjectRoot) {
  TempJsonFile file("[1, 2, 3]");
  ScorerOptions opts;
  EXPECT_FALSE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
}

TEST_F(ErrorHandlingTest, EmptyFile) {
  TempJsonFile file("");
  ScorerOptions opts;
  EXPECT_FALSE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
}

// =============================================================================
// JSON Value Types Tests
// =============================================================================

class JsonValueTypesTest : public ::testing::Test {};

TEST_F(JsonValueTypesTest, NegativeNumbers) {
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -1.5
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -1.5F);
}

TEST_F(JsonValueTypesTest, ScientificNotation) {
  TempJsonFile file(R"({
    "candidates": {
      "split": {
        "alpha_kanji_bonus": 1.5e-1
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.candidates.split.alpha_kanji_bonus, 0.15F);
}

TEST_F(JsonValueTypesTest, IntegerValues) {
  TempJsonFile file(R"({
    "unary": {
      "noun_prior": 3
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.noun_prior, 3.0F);
}

TEST_F(JsonValueTypesTest, RejectsUnknownKeysWithQualifiedDiagnostic) {
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -0.5,
        "unknown_key": 999.0
      }
    }
  })");

  ScorerOptions opts;
  std::string error;
  EXPECT_FALSE(ScorerOptionsLoader::loadFromFile(file.path(), opts, &error));
  EXPECT_EQ(error, "Unknown scorer option: candidates.join.unknown_key");
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, JoinOptions{}.compound_verb_bonus);
}

TEST_F(JsonValueTypesTest, RejectsUnknownTopLevelSection) {
  ScorerOptions opts;
  std::string error;
  EXPECT_FALSE(ScorerOptionsLoader::loadFromJsonString(R"({"typo_section":{}})", opts, &error));
  EXPECT_EQ(error, "Unknown scorer section: typo_section");
}

TEST_F(JsonValueTypesTest, RejectsNonnumericKnownOption) {
  ScorerOptions opts;
  std::string error;
  EXPECT_FALSE(ScorerOptionsLoader::loadFromJsonString(R"({"unary":{"noun_prior":"cheap"}})", opts, &error));
  EXPECT_EQ(error, "Scorer option must be numeric: unary.noun_prior");
}

// =============================================================================
// Default Values Tests
// =============================================================================

class DefaultValuesTest : public ::testing::Test {};

TEST_F(DefaultValuesTest, JoinOptionsDefaults) {
  JoinOptions opts;
  EXPECT_FLOAT_EQ(opts.compound_verb_bonus, -0.8F);
  EXPECT_FLOAT_EQ(opts.verified_v1_bonus, -0.3F);
  EXPECT_FLOAT_EQ(opts.verified_noun_bonus, -0.3F);
}

TEST_F(DefaultValuesTest, SplitOptionsDefaults) {
  SplitOptions opts;
  EXPECT_FLOAT_EQ(opts.alpha_kanji_bonus, -0.3F);
  EXPECT_FLOAT_EQ(opts.alpha_katakana_bonus, -0.3F);
  EXPECT_FLOAT_EQ(opts.split_base_cost, 1.0F);
}

// =============================================================================
// Environment Variable Override Tests
// =============================================================================

#ifndef __EMSCRIPTEN__

// Helper RAII class to set/unset environment variables
class ScopedEnv {
 public:
  explicit ScopedEnv(const std::string& name, const std::string& value) : name_(name) {
    setenv(name.c_str(), value.c_str(), 1);
  }

  ~ScopedEnv() { unsetenv(name_.c_str()); }

 private:
  std::string name_;
};

class EnvOverrideTest : public ::testing::Test {};

TEST_F(EnvOverrideTest, SingleJoinOverride) {
  ScopedEnv env("SUZUME_SCORER_JOIN_compound_verb_bonus", "-1.2");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 1);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -1.2F);
}

TEST_F(EnvOverrideTest, SingleSplitOverride) {
  ScopedEnv env("SUZUME_SCORER_SPLIT_alpha_kanji_bonus", "-0.45");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 1);
  EXPECT_FLOAT_EQ(opts.candidates.split.alpha_kanji_bonus, -0.45F);
}

TEST_F(EnvOverrideTest, MultipleOverrides) {
  ScopedEnv env1("SUZUME_SCORER_JOIN_compound_verb_bonus", "-1.0");
  ScopedEnv env2("SUZUME_SCORER_SPLIT_alpha_kanji_bonus", "-0.5");
  ScopedEnv env3("SUZUME_SCORER_UNARY_noun_prior", "0.2");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 3);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -1.0F);
  EXPECT_FLOAT_EQ(opts.candidates.split.alpha_kanji_bonus, -0.5F);
  EXPECT_FLOAT_EQ(opts.noun_prior, 0.2F);
}

TEST_F(EnvOverrideTest, InvalidValueKeepsDefault) {
  ScopedEnv env("SUZUME_SCORER_JOIN_compound_verb_bonus", "not_a_number");

  ScorerOptions opts;
  float original = opts.candidates.join.compound_verb_bonus;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 0);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, original);
}

TEST_F(EnvOverrideTest, InvalidValueWithSuffix) {
  ScopedEnv env("SUZUME_SCORER_JOIN_compound_verb_bonus", "1.5abc");

  ScorerOptions opts;
  float original = opts.candidates.join.compound_verb_bonus;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 0);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, original);
}

TEST_F(EnvOverrideTest, NonFiniteValuesAreRejected) {
  for (const char* value : {"nan", "inf", "-inf"}) {
    ScopedEnv env("SUZUME_SCORER_UNARY_noun_prior", value);
    ScorerOptions opts;
    const float original = opts.noun_prior;

    const int count = ScorerOptionsLoader::applyEnvOverrides(opts, false);

    EXPECT_EQ(count, 0) << value;
    EXPECT_FLOAT_EQ(opts.noun_prior, original) << value;
  }
}

TEST_F(EnvOverrideTest, NegativeValue) {
  ScopedEnv env("SUZUME_SCORER_JOIN_verified_v1_bonus", "-2.5");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 1);
  EXPECT_FLOAT_EQ(opts.candidates.join.verified_v1_bonus, -2.5F);
}

TEST_F(EnvOverrideTest, ZeroValue) {
  ScopedEnv env("SUZUME_SCORER_JOIN_compound_verb_bonus", "0");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 1);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, 0.0F);
}

TEST_F(EnvOverrideTest, PreviouslyUnreachableVerbScalesAreApplied) {
  ScopedEnv small("SUZUME_SCORER_VERB_confidence_cost_scale_small", "0.75");
  ScopedEnv medium("SUZUME_SCORER_VERB_confidence_cost_scale_medium", "0.85");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 2);
  EXPECT_FLOAT_EQ(opts.candidates.verb.confidence_cost_scale_small, 0.75F);
  EXPECT_FLOAT_EQ(opts.candidates.verb.confidence_cost_scale_medium, 0.85F);
}

TEST_F(EnvOverrideTest, ScientificNotation) {
  ScopedEnv env("SUZUME_SCORER_SPLIT_alpha_kanji_bonus", "1.5e-1");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 1);
  EXPECT_FLOAT_EQ(opts.candidates.split.alpha_kanji_bonus, 0.15F);
}

TEST_F(EnvOverrideTest, EnvOverridesJsonConfig) {
  // First load from JSON
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -0.5
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -0.5F);

  // Then apply env override (higher priority)
  ScopedEnv env("SUZUME_SCORER_JOIN_compound_verb_bonus", "-1.5");
  ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -1.5F);
}

TEST_F(EnvOverrideTest, NoOverridesReturnsZero) {
  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);
  EXPECT_EQ(count, 0);
}

// =============================================================================
// LoadFromEnv Tests
// =============================================================================

class LoadFromEnvTest : public ::testing::Test {};

TEST_F(LoadFromEnvTest, NoConfigReturnsEmptyResult) {
  ScorerOptions opts;
  auto result = ScorerOptionsLoader::loadFromEnv(opts);

  EXPECT_FALSE(result.hasConfig());
  EXPECT_TRUE(result.config_path.empty());
  EXPECT_EQ(result.env_override_count, 0);
}

TEST_F(LoadFromEnvTest, EnvOverrideOnly) {
  ScopedEnv env("SUZUME_SCORER_JOIN_compound_verb_bonus", "-1.0");

  ScorerOptions opts;
  auto result = ScorerOptionsLoader::loadFromEnv(opts);

  EXPECT_TRUE(result.hasConfig());
  EXPECT_TRUE(result.config_path.empty());
  EXPECT_EQ(result.env_override_count, 1);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -1.0F);
}

TEST_F(LoadFromEnvTest, ConfigFileOnly) {
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -0.6
      }
    }
  })");
  ScopedEnv env("SUZUME_SCORER_CONFIG", file.path());

  ScorerOptions opts;
  auto result = ScorerOptionsLoader::loadFromEnv(opts);

  EXPECT_TRUE(result.hasConfig());
  EXPECT_EQ(result.config_path, file.path());
  EXPECT_EQ(result.env_override_count, 0);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -0.6F);
}

TEST_F(LoadFromEnvTest, ConfigFileAndEnvOverride) {
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -0.6
      }
    }
  })");
  ScopedEnv env1("SUZUME_SCORER_CONFIG", file.path());
  ScopedEnv env2("SUZUME_SCORER_JOIN_compound_verb_bonus", "-1.2");

  ScorerOptions opts;
  auto result = ScorerOptionsLoader::loadFromEnv(opts);

  EXPECT_TRUE(result.hasConfig());
  EXPECT_EQ(result.config_path, file.path());
  EXPECT_EQ(result.env_override_count, 1);
  // Env override takes priority over JSON
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -1.2F);
}

TEST_F(LoadFromEnvTest, InvalidConfigFile) {
  ScopedEnv env("SUZUME_SCORER_CONFIG", "/nonexistent/path.json");

  ScorerOptions opts;
  auto result = ScorerOptionsLoader::loadFromEnv(opts);

  // Invalid file should not be recorded
  EXPECT_FALSE(result.hasConfig());
  EXPECT_TRUE(result.config_path.empty());
}

TEST_F(LoadFromEnvTest, ReportedWarningsAreReturnedToTheCaller) {
  ScopedEnv env("SUZUME_SCORER_UNARY_noun_prior", "nan");

  ScorerOptions opts;
  const auto result = ScorerOptionsLoader::loadFromEnv(opts, true);

  EXPECT_EQ(result.env_override_count, 0);
  ASSERT_EQ(result.warnings.size(), 1u);
  EXPECT_NE(result.warnings.front().find("Invalid value"), std::string::npos);
  EXPECT_NE(result.warnings.front().find("nan"), std::string::npos);
}

TEST_F(LoadFromEnvTest, UnknownScorerEnvironmentVariableIsReported) {
  ScopedEnv env("SUZUME_SCORER_JOIN_compund_verb_bonus", "-1.0");

  ScorerOptions opts;
  const auto result = ScorerOptionsLoader::loadFromEnv(opts, true);

  EXPECT_EQ(result.env_override_count, 0);
  ASSERT_EQ(result.warnings.size(), 1u);
  EXPECT_EQ(result.warnings.front(), "Unknown scorer environment variable: SUZUME_SCORER_JOIN_compund_verb_bonus");
}

// The removed unary priors are refused like any other unknown name, in JSON and in the environment.
TEST_F(LoadFromEnvTest, RemovedUnaryPriorsAreUnknown) {
  for (const std::string removed : {"particle_prior", "aux_prior", "pronoun_prior"}) {
    ScorerOptions opts;
    std::string error;
    EXPECT_FALSE(ScorerOptionsLoader::loadFromJsonString(R"({"unary":{")" + removed + R"(":0.1}})", opts, &error));
    EXPECT_EQ(error, "Unknown scorer option: unary." + removed);

    const std::string variable_name = "SUZUME_SCORER_UNARY_" + removed;
    ScopedEnv env(variable_name, "0.1");
    const auto result = ScorerOptionsLoader::loadFromEnv(opts, true);
    EXPECT_EQ(result.env_override_count, 0);
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_EQ(result.warnings.front(), "Unknown scorer environment variable: " + variable_name);
  }
}

#endif  // __EMSCRIPTEN__

// =============================================================================
// Number Conversion Parity
// =============================================================================
// The JSON number path converts decimals itself rather than calling std::strtof,
// because the libc scanner behind strtof computes in 128-bit long double and pulls
// several kilobytes of software floating-point helpers into the WASM binary. These
// tests hold the converter to what the scanner produced: the same accept/reject
// decision and, for accepted tokens, the identical float bit pattern.

uint32_t floatBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

// The strtof-based reference, including the trailing-character and ERANGE checks
// the JSON path applied around it.
bool referenceParse(const std::string& token, float& out) {
  const char* begin = token.c_str();
  char* end = nullptr;
  errno = 0;
  float value = std::strtof(begin, &end);
  if (end == begin || *end != '\0' || errno == ERANGE) {
    return false;
  }
  out = value;
  return true;
}

bool loadNumberToken(const std::string& token, float& out) {
  ScorerOptions opts;
  if (!ScorerOptionsLoader::loadFromJsonString(R"({"unary":{"noun_prior":)" + token + "}}", opts)) {
    return false;
  }
  out = opts.noun_prior;
  return true;
}

void expectStrtofParity(const std::string& token) {
  float reference = 0.0F;
  float parsed = 0.0F;
  const bool reference_accepted = referenceParse(token, reference);
  const bool parsed_accepted = loadNumberToken(token, parsed);
  ASSERT_EQ(parsed_accepted, reference_accepted) << "acceptance differs for token " << token;
  if (reference_accepted) {
    EXPECT_EQ(floatBits(parsed), floatBits(reference)) << "value differs for token " << token;
  }
}

TEST(ScorerOptionNumberTest, MatchesStrtofOnBoundaryTokens) {
  const std::vector<std::string> tokens = {
      "0",
      "-0",
      "0.0",
      "-0.0",
      "1",
      "-0.8",
      "0.1",
      "-0.25",
      "1.5e-3",
      "1E3",
      "1e+3",
      "0.000001",
      "123456789",
      "1234567890123456789",
      "12345678901234567890123456",
      "0.1234567890123456789",
      "1e22",
      "1e23",
      "3.4028234663852886e38",   // FLT_MAX
      "3.5e38",                  // past FLT_MAX, rejected as out of range
      "1.1754943508222875e-38",  // FLT_MIN
      "1e-38",                   // subnormal, rejected as out of range
      "1e-46",                   // underflows to zero, rejected as out of range
      "1e39",                    // overflows, rejected as out of range
      "-1e39",
  };
  for (const std::string& token : tokens) {
    expectStrtofParity(token);
  }
}

TEST(ScorerOptionNumberTest, MatchesStrtofOnGeneratedTokens) {
  std::mt19937 generator(20260727);
  std::uniform_int_distribution<int> digit_count(1, 20);
  std::uniform_int_distribution<int> digit(0, 9);
  std::uniform_int_distribution<int> exponent(-30, 30);
  std::uniform_int_distribution<int> shape(0, 3);

  for (int iteration = 0; iteration < 20000; ++iteration) {
    std::string token;
    if (shape(generator) == 0) {
      token += '-';
    }
    const int integer_digits = digit_count(generator);
    for (int index = 0; index < integer_digits; ++index) {
      token += static_cast<char>('0' + digit(generator));
    }
    if (shape(generator) != 0) {
      token += '.';
      const int fraction_digits = digit_count(generator);
      for (int index = 0; index < fraction_digits; ++index) {
        token += static_cast<char>('0' + digit(generator));
      }
    }
    if (shape(generator) != 0) {
      token += 'e';
      const int value = exponent(generator);
      if (value < 0) {
        token += '-';
      }
      token += std::to_string(value < 0 ? -value : value);
    }
    expectStrtofParity(token);
  }
}

TEST(ScorerOptionNumberTest, RejectsTheOverflowThresholdTie) {
  // Exactly halfway between FLT_MAX and 2^128. Round-to-nearest-even resolves the
  // tie upward, so the value overflows the float range and is rejected. Some
  // platform scanners resolve the same tie downward to FLT_MAX; the converter
  // follows IEEE rounding rather than the host libc, so the result no longer
  // depends on which platform parsed the configuration.
  float parsed = 0.0F;
  EXPECT_FALSE(loadNumberToken("3.4028235677973366e38", parsed));
}

TEST(ScorerOptionNumberTest, RejectsMalformedNumbers) {
  ScorerOptions opts;
  std::string error;
  EXPECT_FALSE(ScorerOptionsLoader::loadFromJsonString(R"({"unary":{"noun_prior":1e}})", opts, &error));
  EXPECT_FALSE(ScorerOptionsLoader::loadFromJsonString(R"({"unary":{"noun_prior":-}})", opts, &error));
  EXPECT_FALSE(ScorerOptionsLoader::loadFromJsonString(R"({"unary":{"noun_prior":1e+}})", opts, &error));
}

}  // namespace
}  // namespace suzume::analysis
