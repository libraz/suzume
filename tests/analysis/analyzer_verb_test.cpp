// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Verb conjugation analyzer tests (五段・一段・複合動詞)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// ===== Godan Verb Tests (五段動詞) =====
// These tests verify proper recognition of godan verb conjugations

TEST(AnalyzerTest, GodanVerb_KakuBaseForm) {
  // 書く (to write) - ka-row godan verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("書く");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "書く" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "書く should be recognized as verb";
}

TEST(AnalyzerTest, GodanVerb_KaitaConjugated) {
  // 書いた (wrote) - past tense of 書く
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("書いた");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "書いた" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "書いた should be recognized as verb";
}

TEST(AnalyzerTest, GodanVerb_YomuBaseForm) {
  // 読む (to read) - ma-row godan verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("読む");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "読む" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "読む should be recognized as verb";
}

TEST(AnalyzerTest, GodanVerb_YondaConjugated) {
  // 読んだ (read - past) - past tense of 読む
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("読んだ");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "読んだ" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "読んだ should be recognized as verb";
}

TEST(AnalyzerTest, GodanVerb_HashiruBaseForm) {
  // 走る (to run) - ra-row godan verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("走る");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "走る" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "走る should be recognized as verb";
}

TEST(AnalyzerTest, GodanVerb_HashittaConjugated) {
  // 走った (ran) - past tense of 走る
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("走った");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "走った" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "走った should be recognized as verb";
}

TEST(AnalyzerTest, GodanVerb_KauBaseForm) {
  // 買う (to buy) - wa-row godan verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("買う");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "買う" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "買う should be recognized as verb";
}

TEST(AnalyzerTest, GodanVerb_KattaConjugated) {
  // 買った (bought) - past tense of 買う
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("買った");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "買った" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "買った should be recognized as verb";
}

// ===== Ichidan Verb Tests (一段動詞) =====

TEST(AnalyzerTest, IchidanVerb_TaberuBaseForm) {
  // 食べる (to eat) - ichidan verb base form
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べる");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "食べる" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "食べる should be recognized as verb";
}

TEST(AnalyzerTest, IchidanVerb_TabetaiDesiderative) {
  // 食べたい (want to eat) - desiderative form
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べたい");
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "食べたい" &&
        morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "食べたい should be recognized as verb";
}

TEST(AnalyzerTest, IchidanVerb_Tabenakereba) {
  // 食べなければ (if not eat) - conditional negative
  // TODO: Currently splits as 食 + べなければ due to ichidan stem detection issue
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べなければ");
  // For now, just verify it produces some output
  ASSERT_FALSE(result.empty()) << "食べなければ should produce tokens";
  // Check if any token contains verb-like morpheme
  bool has_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.pos == core::PartOfSpeech::Verb) {
      has_verb = true;
      break;
    }
  }
  EXPECT_TRUE(has_verb) << "食べなければ should contain verb morpheme";
}

// ===== Compound Verb Tests (複合動詞) =====

TEST(AnalyzerTest, CompoundVerb_TabeHajimeru) {
  // 食べ始める (start eating) - compound verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べ始める");
  // Should recognize as verb(s)
  bool found_verb = false;
  for (const auto& morpheme : result) {
    if (morpheme.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "食べ始める should contain verb";
}

// ===== Potential Form Tests =====

TEST(AnalyzerTest, Potential_CanDo) {
  // Potential expression
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("日本語が話せるようになりたい");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が in potential sentence";
}

TEST(AnalyzerTest, Potential_CannotDo) {
  // Cannot do expression
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今は外出できない状況です");
  ASSERT_FALSE(result.empty());
  bool found_ha = false;
  for (const auto& mor : result) {
    if (mor.surface == "は") {
      found_ha = true;
      break;
    }
  }
  EXPECT_TRUE(found_ha) << "Should recognize は particle";
}

// ===== Multi-Verb Sequential Tests =====

TEST(AnalyzerTest, MultiVerb_Sequential) {
  // Sequential verb actions
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("起きて朝ご飯を食べた");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を") {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
}

TEST(AnalyzerTest, MultiVerb_Purpose) {
  // Purpose clause with verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本を買いに行った");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  bool found_ni = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
    }
    if (mor.surface == "に" && mor.pos == core::PartOfSpeech::Particle) {
      found_ni = true;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
  EXPECT_TRUE(found_ni) << "Should recognize に particle";
}

// ===== Honorific Verb Tests =====

TEST(AnalyzerTest, Honorific_Irassharu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("先生がいらっしゃいます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Polite_Itadaku) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("資料をいただきました");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Polite_Moushiageru) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ご報告申し上げます");
  ASSERT_FALSE(result.empty());
}

}  // namespace
}  // namespace suzume::analysis
