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

TEST_F(JsonParserTest, LoadConnectionRulesEdge) {
  TempJsonFile file(R"({
    "connection_rules": {
      "edge": {
        "penalty_invalid_adj_sou": 3.5,
        "penalty_verb_aux_in_adj": 1.2
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.connection_rules.edge.penalty_invalid_adj_sou, 3.5F);
  EXPECT_FLOAT_EQ(opts.connection_rules.edge.penalty_verb_aux_in_adj, 1.2F);
}

TEST_F(JsonParserTest, LoadConnectionRulesConnection) {
  TempJsonFile file(R"({
    "connection_rules": {
      "connection": {
        "penalty_copula_after_verb": 2.0,
        "bonus_tai_after_renyokei": -0.8
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.connection_rules.connection.penalty_copula_after_verb, 2.0F);
  EXPECT_FLOAT_EQ(opts.connection_rules.connection.bonus_tai_after_renyokei, -0.8F);
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
    "connection_rules": {
      "edge": {
        "penalty_invalid_adj_sou": 2.5
      },
      "connection": {
        "bonus_tai_after_renyokei": -0.6
      }
    },
    "candidates": {
      "join": {
        "compound_verb_bonus": -0.9
      },
      "split": {
        "alpha_kanji_bonus": -0.35
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.connection_rules.edge.penalty_invalid_adj_sou, 2.5F);
  EXPECT_FLOAT_EQ(opts.connection_rules.connection.bonus_tai_after_renyokei, -0.6F);
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -0.9F);
  EXPECT_FLOAT_EQ(opts.candidates.split.alpha_kanji_bonus, -0.35F);
}

TEST_F(JsonParserTest, PartialOverridePreservesDefaults) {
  TempJsonFile file(R"({
    "connection_rules": {
      "edge": {
        "penalty_invalid_adj_sou": 5.0
      }
    }
  })");

  ScorerOptions opts;
  // Set some non-default values before loading
  opts.candidates.join.compound_verb_bonus = -0.123F;

  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));

  // Check that loaded value is applied
  EXPECT_FLOAT_EQ(opts.connection_rules.edge.penalty_invalid_adj_sou, 5.0F);

  // Check that non-loaded value is preserved
  EXPECT_FLOAT_EQ(opts.candidates.join.compound_verb_bonus, -0.123F);
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
    "connection_rules": {
      "edge": {
        "penalty_invalid_adj_sou": 3
      }
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.connection_rules.edge.penalty_invalid_adj_sou, 3.0F);
}

TEST_F(JsonValueTypesTest, IgnoresUnknownKeys) {
  TempJsonFile file(R"({
    "connection_rules": {
      "edge": {
        "penalty_invalid_adj_sou": 2.0,
        "unknown_key": 999.0
      }
    },
    "unknown_section": {
      "foo": "bar"
    }
  })");

  ScorerOptions opts;
  EXPECT_TRUE(ScorerOptionsLoader::loadFromFile(file.path(), opts));
  EXPECT_FLOAT_EQ(opts.connection_rules.edge.penalty_invalid_adj_sou, 2.0F);
}

// =============================================================================
// Default Values Tests
// =============================================================================

class DefaultValuesTest : public ::testing::Test {};

TEST_F(DefaultValuesTest, ConnectionRulesEdgeDefaults) {
  EdgeOptions opts;
  EXPECT_FLOAT_EQ(opts.penalty_invalid_adj_sou, 1.5F);
  EXPECT_FLOAT_EQ(opts.penalty_invalid_tai_pattern, 2.0F);
  EXPECT_FLOAT_EQ(opts.penalty_verb_aux_in_adj, 2.0F);
}

TEST_F(DefaultValuesTest, ConnectionRulesConnectionDefaults) {
  ConnectionOptions opts;
  EXPECT_FLOAT_EQ(opts.penalty_copula_after_verb, 3.0F);
  EXPECT_FLOAT_EQ(opts.bonus_tai_after_renyokei, 0.8F);
}

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

}  // namespace
}  // namespace suzume::analysis
