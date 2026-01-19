/**
 * @file scorer_options_loader_test.cpp
 * @brief Tests for scorer options JSON loader
 */

#include "analysis/scorer_options_loader.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

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

  ~TempJsonFile() {
    std::remove(path_.c_str());
  }

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
        "verified_v1_bonus": -0.3,
        "te_form_aux_bonus": -0.5
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -0.7F);
  EXPECT_FLOAT_EQ(opts.candidates.join.verified_v1_bonus, -0.3F);
  EXPECT_FLOAT_EQ(opts.candidates.join.te_form_aux_bonus, -0.5F);
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
      "single_kanji_penalty": 3
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.single_kanji_penalty, 3.0F);
}

TEST_F(JsonValueTypesTest, IgnoresUnknownKeys) {
  TempJsonFile file(R"({
    "candidates": {
      "join": {
        "compound_verb_bonus": -0.5,
        "unknown_key": 999.0
      }
    },
    "unknown_section": {
      "foo": "bar"
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -0.5F);
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
  EXPECT_FLOAT_EQ(opts.te_form_aux_bonus, -0.8F);
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
  explicit ScopedEnv(const std::string& name, const std::string& value)
      : name_(name) {
    setenv(name.c_str(), value.c_str(), 1);
  }

  ~ScopedEnv() {
    unsetenv(name_.c_str());
  }

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

TEST_F(EnvOverrideTest, NegativeValue) {
  ScopedEnv env("SUZUME_SCORER_JOIN_te_form_aux_bonus", "-2.5");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 1);
  EXPECT_FLOAT_EQ(opts.candidates.join.te_form_aux_bonus, -2.5F);
}

TEST_F(EnvOverrideTest, ZeroValue) {
  ScopedEnv env("SUZUME_SCORER_JOIN_compound_verb_bonus", "0");

  ScorerOptions opts;
  int count = ScorerOptionsLoader::applyEnvOverrides(opts);

  EXPECT_EQ(count, 1);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, 0.0F);
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

#endif  // __EMSCRIPTEN__

}  // namespace
}  // namespace suzume::analysis
