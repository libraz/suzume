// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Particle (助詞) analyzer tests

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// ===== Case Particles (格助詞) =====

TEST(AnalyzerTest, Particle_TopicMarkerWa) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("彼女は学生です");
  bool found_wa = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "は" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_wa = true;
      break;
    }
  }
  EXPECT_TRUE(found_wa);
}

TEST(AnalyzerTest, Particle_SubjectMarkerGa) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("雨が降っている");
  bool found_ga = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "が" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga);
}

TEST(AnalyzerTest, Particle_ObjectMarkerWo) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本を読む");
  bool found_wo = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "を" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo);
}

TEST(AnalyzerTest, Particle_DirectionMarkerNi) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("東京に行く");
  bool found_ni = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "に" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_ni = true;
      break;
    }
  }
  EXPECT_TRUE(found_ni);
}

TEST(AnalyzerTest, Particle_PossessiveNo) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("彼の車");
  bool found_no = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "の" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_no = true;
      break;
    }
  }
  EXPECT_TRUE(found_no);
}

TEST(AnalyzerTest, Particle_FromKara) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("東京から大阪まで");
  bool found_kara = false;
  bool found_made = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "から") found_kara = true;
    if (morpheme.surface == "まで") found_made = true;
  }
  EXPECT_TRUE(found_kara);
  EXPECT_TRUE(found_made);
}

TEST(AnalyzerTest, Particle_LocationDe) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("公園で遊ぶ");
  bool found_de = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "で" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_de = true;
      break;
    }
  }
  EXPECT_TRUE(found_de);
}

TEST(AnalyzerTest, Particle_ConjunctiveMo) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("私も行きたい");
  bool found_mo = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "も" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_mo = true;
      break;
    }
  }
  EXPECT_TRUE(found_mo);
}

// ===== Quotation and Conditional Particle と Tests =====

TEST(AnalyzerTest, QuotationTo_IkutoItta) {
  // 行くと言った (said to go) - quotation with godan verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行くと言った");
  // Check that と is recognized as particle
  bool found_to = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "と" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_to = true;
      break;
    }
  }
  EXPECT_TRUE(found_to) << "と should be recognized as particle in 行くと言った";
}

TEST(AnalyzerTest, QuotationTo_TaberuToOmou) {
  // 食べると思う (think will eat) - quotation with ichidan verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べると思う");
  bool found_to = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "と" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_to = true;
      break;
    }
  }
  EXPECT_TRUE(found_to) << "と should be recognized as particle in 食べると思う";
}

TEST(AnalyzerTest, ConditionalTo_HaruNinaruto) {
  // 春になると (when spring comes) - conditional
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("春になると咲く");
  bool found_to = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "と" &&
        morpheme.pos == core::PartOfSpeech::Particle) {
      found_to = true;
      break;
    }
  }
  EXPECT_TRUE(found_to) << "と should be recognized as particle in 春になると咲く";
}

// ===== Sentence Ending Particle Tests (終助詞) =====

TEST(AnalyzerTest, Question_Ka) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行きますか");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Question_Ne) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("いいですね");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Question_Yo) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行くよ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SentenceEnding_WaNe) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("そうだわね");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SentenceEnding_Kana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行けるかな");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SentenceEnding_Kashira) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("大丈夫かしら");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SentenceEnding_Zo) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行くぞ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SentenceEnding_Ze) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("やるぜ");
  ASSERT_FALSE(result.empty());
}

}  // namespace
}  // namespace suzume::analysis
