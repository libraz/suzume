
// Strict analyzer tests: Compound verbs, Compound nouns

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "dictionary/user_dict.h"
#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::getSurfaces;

// Base class for tests that need core dictionary
class AnalyzerTestBase : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};

  void SetUp() override {
    analyzer.tryAutoLoadCoreDictionary();
  }
};

// ===== Compound Verb Tests =====

class CompoundVerbStrictTest : public AnalyzerTestBase {
 protected:
  void SetUp() override {
    AnalyzerTestBase::SetUp();
    auto dict = std::make_shared<dictionary::UserDictionary>();
    auto result = dict->loadFromFile("data/core/verbs.tsv");
    if (result.hasValue()) {
      analyzer.addUserDictionary(dict);
    }
  }
};

TEST_F(CompoundVerbStrictTest, Yobidasu) {
  auto result = analyzer.analyze("呼び出す");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  bool has_standalone_bi = false;
  for (const auto& sur : surfaces) {
    if (sur == "び") {
      has_standalone_bi = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_bi)
      << "呼び出す: び should not be standalone. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, Yomikomu) {
  auto result = analyzer.analyze("読み込む");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  bool has_standalone_mi = false;
  for (const auto& sur : surfaces) {
    if (sur == "み") {
      has_standalone_mi = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_mi)
      << "読み込む: み should not be standalone. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, Kakidasu) {
  auto result = analyzer.analyze("書き出す");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  bool has_standalone_ki = false;
  for (const auto& sur : surfaces) {
    if (sur == "き") {
      has_standalone_ki = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_ki)
      << "書き出す: き should not be standalone. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, Tobikomu) {
  auto result = analyzer.analyze("飛び込む");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  bool has_standalone_bi = false;
  for (const auto& sur : surfaces) {
    if (sur == "び") {
      has_standalone_bi = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_bi)
      << "飛び込む: び should not be standalone. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, Torikesu) {
  auto result = analyzer.analyze("取り消す");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  bool has_standalone_ri = false;
  for (const auto& sur : surfaces) {
    if (sur == "り") {
      has_standalone_ri = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_ri)
      << "取り消す: り should not be standalone. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, Hikitsuzuki) {
  auto result = analyzer.analyze("引き続き");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  EXPECT_LT(surfaces.size(), 4)
      << "引き続き should not be fragmented into 4+ pieces. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, Kaimono) {
  auto result = analyzer.analyze("買い物");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  EXPECT_LE(surfaces.size(), 2)
      << "買い物 should be at most 2 tokens. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, CompoundVerbInSentence) {
  auto result = analyzer.analyze("データを読み込む");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を particle. " << debug_msg;

  bool has_standalone_mi = false;
  for (const auto& sur : surfaces) {
    if (sur == "み") {
      has_standalone_mi = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_mi)
      << "読み込む: み should not be standalone in sentence. " << debug_msg;
}

// ===== Compound Noun Tests =====

class CompoundNounStrictTest : public AnalyzerTestBase {};

TEST_F(CompoundNounStrictTest, Shizengengo) {
  auto result = analyzer.analyze("自然言語処理");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  EXPECT_LE(surfaces.size(), 3)
      << "自然言語処理 should be at most 3 tokens. " << debug_msg;
}

TEST_F(CompoundNounStrictTest, JinkouChinou) {
  auto result = analyzer.analyze("人工知能");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  EXPECT_LE(surfaces.size(), 2)
      << "人工知能 should be at most 2 tokens. " << debug_msg;
}

TEST_F(CompoundNounStrictTest, KikaiGakushuu) {
  auto result = analyzer.analyze("機械学習");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  EXPECT_LE(surfaces.size(), 2)
      << "機械学習 should be at most 2 tokens. " << debug_msg;
}

}  // namespace
}  // namespace suzume::analysis
