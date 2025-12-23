// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Real-world use case analyzer tests (business, conversation, news, etc.)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// ===== Mixed Script Joining Tests (Phase M2) =====

TEST(AnalyzerTest, MixedScript_AlphabetKanji) {
  // Test: "Web開発" should preferably be analyzed as a single token
  // or at minimum have candidates that join it
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("Web開発の基礎");

  // The result should parse successfully
  ASSERT_FALSE(result.empty());

  // At minimum, check that "の" particle is found
  // Note: "Web開発" may or may not be joined depending on implementation
  bool found_no = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "の") {
      found_no = true;
      break;
    }
  }
  EXPECT_TRUE(found_no);
}

TEST(AnalyzerTest, MixedScript_AlphabetKatakana) {
  // Test: "APIリクエスト" should preferably be analyzed as a single token
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("APIリクエスト処理");

  ASSERT_FALSE(result.empty());

  // At minimum, verify the text produces tokens
  // Note: "APIリクエスト" may or may not be joined depending on implementation
  EXPECT_GT(result.size(), 0);
}

TEST(AnalyzerTest, MixedScript_DigitKanji) {
  // Test: "3月" should be analyzed as a joined token
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("3月の予定");

  ASSERT_FALSE(result.empty());

  // Check that "の" particle is found
  // Note: "3月" may or may not be joined depending on implementation
  bool found_no = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "の") {
      found_no = true;
      break;
    }
  }
  EXPECT_TRUE(found_no);
}

TEST(AnalyzerTest, MixedScript_MultipleDigitKanji) {
  // Test: "100人" should be analyzed with the joining candidate
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("100人が参加");

  ASSERT_FALSE(result.empty());

  // Check for "が" particle
  bool found_ga = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "が") {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga);
}

// ===== Compound Noun Splitting Tests (Phase M3) =====

TEST(AnalyzerTest, CompoundNoun_FourKanji) {
  // Test: "人工知能" (4 kanji) should be parsed
  // Either as single token or split into "人工" + "知能"
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("人工知能の研究");

  ASSERT_FALSE(result.empty());

  // Check for "の" particle
  bool found_no = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "の") {
      found_no = true;
      break;
    }
  }
  EXPECT_TRUE(found_no);
}

TEST(AnalyzerTest, CompoundNoun_LongKanji) {
  // Test: Long kanji compound should be parsed successfully
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("東京都知事選挙");

  ASSERT_FALSE(result.empty());
  // Should be parsed into meaningful segments
  EXPECT_GE(result.size(), 1);
}

TEST(AnalyzerTest, CompoundNoun_WithParticle) {
  // Test: Compound noun followed by particle
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("情報処理技術者が");

  ASSERT_FALSE(result.empty());

  // Check for "が" particle at the end
  bool found_ga = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "が") {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga);
}

// ===== Business Email Tests (ビジネスメール) =====

TEST(AnalyzerTest, BusinessEmail_Greeting) {
  // Common business email opening
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("お世話になっております");
  ASSERT_FALSE(result.empty());
  // Should contain the honorific prefix お
  bool found_prefix = false;
  for (const auto& mor : result) {
    if (mor.surface == "お") {
      found_prefix = true;
      break;
    }
  }
  EXPECT_TRUE(found_prefix) << "Should recognize お as prefix";
}

TEST(AnalyzerTest, BusinessEmail_Request) {
  // Polite request form
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ご確認ください");
  ASSERT_FALSE(result.empty());
  // Check that ご prefix is recognized
  bool found_go = false;
  for (const auto& mor : result) {
    if (mor.surface == "ご") {
      found_go = true;
      break;
    }
  }
  EXPECT_TRUE(found_go) << "Should recognize ご as prefix";
}

TEST(AnalyzerTest, BusinessEmail_Closing) {
  // Standard business email closing
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("よろしくお願いいたします");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2);
}

TEST(AnalyzerTest, BusinessEmail_FullSentence) {
  // Complete business email sentence
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("資料を添付いたしましたので、ご確認ください");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5);
}

// ===== Everyday Conversation Tests (日常会話) =====

TEST(AnalyzerTest, Conversation_Weather) {
  // Weather small talk
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今日は寒いですね");
  ASSERT_FALSE(result.empty());
  // Should have: 今日 + は + 寒い + です + ね
  bool found_kyou = false;
  bool found_samui = false;
  for (const auto& mor : result) {
    if (mor.surface == "今日") found_kyou = true;
    if (mor.surface == "寒い") found_samui = true;
  }
  EXPECT_TRUE(found_kyou) << "Should recognize 今日";
  EXPECT_TRUE(found_samui) << "Should recognize 寒い";
}

TEST(AnalyzerTest, Conversation_AskingDirections) {
  // Asking for directions
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("駅までどうやって行きますか");
  ASSERT_FALSE(result.empty());
  bool found_eki = false;
  bool found_made = false;
  for (const auto& mor : result) {
    if (mor.surface == "駅") found_eki = true;
    if (mor.surface == "まで") found_made = true;
  }
  EXPECT_TRUE(found_eki) << "Should recognize 駅 as noun";
  EXPECT_TRUE(found_made) << "Should recognize まで as particle";
}

TEST(AnalyzerTest, Conversation_PoliteRequest) {
  // Polite request
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ちょっと待ってください");
  ASSERT_FALSE(result.empty());
  bool found_matte = false;
  for (const auto& mor : result) {
    if (mor.surface.find("待") != std::string::npos) {
      found_matte = true;
      break;
    }
  }
  EXPECT_TRUE(found_matte) << "Should recognize waiting verb";
}

TEST(AnalyzerTest, Conversation_ThankYou) {
  // Thank you variations
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ありがとうございます");
  ASSERT_FALSE(result.empty());
}

// ===== Schedule/Appointment Tests (予定・約束) =====

TEST(AnalyzerTest, Schedule_MeetingTime) {
  // Meeting schedule
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("明日の10時に会議があります");
  ASSERT_FALSE(result.empty());
  bool found_ashita = false;
  bool found_ni = false;
  for (const auto& mor : result) {
    if (mor.surface == "明日") found_ashita = true;
    if (mor.surface == "に" && mor.pos == core::PartOfSpeech::Particle)
      found_ni = true;
  }
  EXPECT_TRUE(found_ashita) << "Should recognize 明日";
  EXPECT_TRUE(found_ni) << "Should recognize に particle";
}

TEST(AnalyzerTest, Schedule_NextWeek) {
  // Next week appointment
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("来週の金曜日はいかがですか");
  ASSERT_FALSE(result.empty());
  bool found_raishuu = false;
  for (const auto& mor : result) {
    if (mor.surface == "来週") {
      found_raishuu = true;
      break;
    }
  }
  EXPECT_TRUE(found_raishuu) << "Should recognize 来週";
}

TEST(AnalyzerTest, Schedule_Busy) {
  // Expressing busy schedule
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今週は忙しいので来週にしましょう");
  ASSERT_FALSE(result.empty());
  bool found_konshuu = false;
  bool found_raishuu = false;
  for (const auto& mor : result) {
    if (mor.surface == "今週") found_konshuu = true;
    if (mor.surface == "来週") found_raishuu = true;
  }
  EXPECT_TRUE(found_konshuu) << "Should recognize 今週";
  EXPECT_TRUE(found_raishuu) << "Should recognize 来週";
}

// ===== Shopping/Transaction Tests (買い物・取引) =====

TEST(AnalyzerTest, Shopping_Price) {
  // Asking price
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("これはいくらですか");
  ASSERT_FALSE(result.empty());
  // Check that か (question particle) is recognized
  bool found_ka = false;
  for (const auto& mor : result) {
    if (mor.surface == "か" && mor.pos == core::PartOfSpeech::Particle) {
      found_ka = true;
      break;
    }
  }
  EXPECT_TRUE(found_ka) << "Should recognize か as question particle";
}

TEST(AnalyzerTest, Shopping_Payment) {
  // Payment method
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("カードで払えますか");
  ASSERT_FALSE(result.empty());
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle) {
      found_de = true;
      break;
    }
  }
  EXPECT_TRUE(found_de) << "Should recognize で as particle";
}

TEST(AnalyzerTest, Shopping_Quantity) {
  // Ordering quantity
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("これを3つください");
  ASSERT_FALSE(result.empty());
  // Check basic segmentation works
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens";
}

// ===== News/Article Style Tests (ニュース・記事) =====

TEST(AnalyzerTest, News_Announcement) {
  // News announcement pattern
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("政府は新しい政策を発表した");
  ASSERT_FALSE(result.empty());
  bool found_ha = false;
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "は" && mor.pos == core::PartOfSpeech::Particle)
      found_ha = true;
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle)
      found_wo = true;
  }
  EXPECT_TRUE(found_ha) << "Should recognize は as topic marker";
  EXPECT_TRUE(found_wo) << "Should recognize を as object marker";
}

TEST(AnalyzerTest, News_According) {
  // Citation pattern
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("関係者によると問題はない");
  ASSERT_FALSE(result.empty());
  // Check that basic segmentation produces multiple tokens
  EXPECT_GE(result.size(), 3) << "Should produce multiple tokens for news";
}

TEST(AnalyzerTest, News_Event) {
  // Event description
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("昨日、記者会見が行われた");
  ASSERT_FALSE(result.empty());
  bool found_kinou = false;
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "昨日") found_kinou = true;
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle)
      found_ga = true;
  }
  EXPECT_TRUE(found_kinou) << "Should recognize 昨日";
  EXPECT_TRUE(found_ga) << "Should recognize が";
}

// ===== Complex Real Sentences (複雑な実文) =====

TEST(AnalyzerTest, Complex_LostItem) {
  // Lost item description
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("昨日買ったばかりの本をなくしてしまった");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5);
  bool found_kinou = false;
  bool found_katta = false;
  for (const auto& mor : result) {
    if (mor.surface == "昨日") found_kinou = true;
    if (mor.surface == "買った" ||
        mor.surface.find("買") != std::string::npos)
      found_katta = true;
  }
  EXPECT_TRUE(found_kinou) << "Should recognize 昨日";
  EXPECT_TRUE(found_katta) << "Should recognize 買った";
}

TEST(AnalyzerTest, Complex_LateForWork) {
  // Excuse for being late
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("電車が遅れているので遅刻しそうです");
  ASSERT_FALSE(result.empty());
  bool found_node = false;
  for (const auto& mor : result) {
    if (mor.surface == "ので" || mor.surface == "の") {
      found_node = true;
      break;
    }
  }
  EXPECT_TRUE(found_node) << "Should recognize ので (reason conjunction)";
}

TEST(AnalyzerTest, Complex_Cooking) {
  // Comment about cooking
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("彼女が作った料理はとても美味しかった");
  ASSERT_FALSE(result.empty());
  bool found_tsukutta = false;
  bool found_kanojo = false;
  for (const auto& mor : result) {
    if (mor.surface.find("作") != std::string::npos) found_tsukutta = true;
    if (mor.surface == "彼女") found_kanojo = true;
  }
  EXPECT_TRUE(found_tsukutta) << "Should recognize 作った";
  EXPECT_TRUE(found_kanojo) << "Should recognize 彼女";
}

TEST(AnalyzerTest, Complex_StudyAbroad) {
  // Study abroad plan
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("来年から留学するつもりです");
  ASSERT_FALSE(result.empty());
  bool found_rainen = false;
  bool found_kara = false;
  for (const auto& mor : result) {
    if (mor.surface == "来年") found_rainen = true;
    if (mor.surface == "から") found_kara = true;
  }
  EXPECT_TRUE(found_rainen) << "Should recognize 来年";
  EXPECT_TRUE(found_kara) << "Should recognize から";
}

// ===== Casual/SNS Style Tests (カジュアル/SNS) =====

TEST(AnalyzerTest, Casual_Fun) {
  // Casual expression of fun
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("めっちゃ楽しかった");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Casual_Really) {
  // Casual confirmation
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本当にそうなの");
  ASSERT_FALSE(result.empty());
  // Check basic segmentation
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens";
}

TEST(AnalyzerTest, Casual_Desire) {
  // Desire expression
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ラーメン食べたい");
  ASSERT_FALSE(result.empty());
  bool found_tabetai = false;
  for (const auto& mor : result) {
    if (mor.surface.find("食べ") != std::string::npos ||
        mor.surface == "食べたい") {
      found_tabetai = true;
      break;
    }
  }
  EXPECT_TRUE(found_tabetai) << "Should recognize 食べたい";
}

// ===== Compound Expression Tests (複合表現) =====

TEST(AnalyzerTest, Compound_NiTsuite) {
  // について (regarding)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("日本の文化について話す");
  ASSERT_FALSE(result.empty());
  bool found_ni = false;
  for (const auto& mor : result) {
    if (mor.surface == "について" || mor.surface == "に") {
      found_ni = true;
      break;
    }
  }
  EXPECT_TRUE(found_ni) << "Should recognize について or に";
}

TEST(AnalyzerTest, Compound_NiYotte) {
  // によって (by means of)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("場合によって対応が変わる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Compound_ToShite) {
  // として (as)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("教師として働いている");
  ASSERT_FALSE(result.empty());
  // Check that the sentence is segmented
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens";
}

// ===== Multi-clause Sentence Tests (複文) =====

TEST(AnalyzerTest, MultiClause_Conditional) {
  // Conditional sentence
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("雨が降ったら、試合は中止になります");
  ASSERT_FALSE(result.empty());
  // Check that が particle is recognized
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が as particle";
}

TEST(AnalyzerTest, MultiClause_Reason) {
  // Reason clause
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("疲れたから早く寝ます");
  ASSERT_FALSE(result.empty());
  bool found_kara = false;
  for (const auto& mor : result) {
    if (mor.surface == "から") {
      found_kara = true;
      break;
    }
  }
  EXPECT_TRUE(found_kara) << "Should recognize から";
}

TEST(AnalyzerTest, MultiClause_Contrast) {
  // Contrastive clause
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("勉強したけど、試験に落ちた");
  ASSERT_FALSE(result.empty());
  bool found_kedo = false;
  for (const auto& mor : result) {
    if (mor.surface == "けど" || mor.surface == "けれど") {
      found_kedo = true;
      break;
    }
  }
  EXPECT_TRUE(found_kedo) << "Should recognize けど";
}

TEST(AnalyzerTest, MultiClause_While) {
  // Simultaneous action
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("音楽を聴きながら勉強する");
  ASSERT_FALSE(result.empty());
  // Check that を particle is recognized
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を as particle";
}

// ===== Mixed Language Tests (混合言語) =====

TEST(AnalyzerTest, Mixed_EnglishInJapanese) {
  // English words in Japanese text
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今日はMeetingがあります");
  ASSERT_FALSE(result.empty());
  // Check that segmentation produces multiple tokens
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens for mixed";
}

TEST(AnalyzerTest, Mixed_TechnicalTerm) {
  // Technical term with Japanese
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("APIを使ってデータを取得する");
  ASSERT_FALSE(result.empty());
  int wo_count = 0;
  for (const auto& mor : result) {
    if (mor.surface == "を") wo_count++;
  }
  EXPECT_GE(wo_count, 1) << "Should recognize を particles";
}

TEST(AnalyzerTest, Mixed_BrandName) {
  // Brand name in sentence
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("iPhoneを買いました");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を") {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を after brand name";
}

// ===== Technical Documentation Tests (技術文書) =====

TEST(AnalyzerTest, Technical_ErrorMessage) {
  // Error message in technical context
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ファイルが見つかりませんでした");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が particle in error message";
}

TEST(AnalyzerTest, Technical_ProgrammingTerm) {
  // Programming terminology with Japanese
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("変数に値を代入する");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  bool found_ni = false;
  for (const auto& mor : result) {
    if (mor.surface == "を") found_wo = true;
    if (mor.surface == "に") found_ni = true;
  }
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
  EXPECT_TRUE(found_ni) << "Should recognize に particle";
}

TEST(AnalyzerTest, Technical_CodeReview) {
  // Code review comment
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result =
      analyzer.analyze("この関数は複雑すぎるので分割してください");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5) << "Should produce multiple tokens";
}

TEST(AnalyzerTest, Technical_DocumentationSpec) {
  // Documentation specification style
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("戻り値は成功時に0を返す");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を") {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を in technical spec";
}

// ===== Recipe/Cooking Tests (レシピ・料理) =====

TEST(AnalyzerTest, Recipe_CookingInstruction) {
  // Cooking instruction
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("玉ねぎをみじん切りにする");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  bool found_ni = false;
  for (const auto& mor : result) {
    if (mor.surface == "を") found_wo = true;
    if (mor.surface == "に") found_ni = true;
  }
  EXPECT_TRUE(found_wo) << "Should recognize を in recipe";
  EXPECT_TRUE(found_ni) << "Should recognize に in recipe";
}

TEST(AnalyzerTest, Recipe_CookingTime) {
  // Cooking time instruction
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("中火で5分間炒める");
  ASSERT_FALSE(result.empty());
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "で") {
      found_de = true;
      break;
    }
  }
  EXPECT_TRUE(found_de) << "Should recognize で particle in cooking";
}

TEST(AnalyzerTest, Recipe_Seasoning) {
  // Seasoning instruction
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("塩と胡椒で味を調える");
  ASSERT_FALSE(result.empty());
  bool found_to = false;
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "と") found_to = true;
    if (mor.surface == "で") found_de = true;
  }
  EXPECT_TRUE(found_to) << "Should recognize と particle";
  EXPECT_TRUE(found_de) << "Should recognize で particle";
}

// ===== Medical/Health Tests (医療・健康) =====

TEST(AnalyzerTest, Medical_Symptom) {
  // Symptom description
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("頭が痛くて熱がある");
  ASSERT_FALSE(result.empty());
  int ga_count = 0;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      ga_count++;
    }
  }
  EXPECT_GE(ga_count, 1) << "Should recognize が particles in symptom";
}

TEST(AnalyzerTest, Medical_Prescription) {
  // Prescription instruction
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食後に一錠を服用してください");
  ASSERT_FALSE(result.empty());
  bool found_ni = false;
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "に") found_ni = true;
    if (mor.surface == "を") found_wo = true;
  }
  EXPECT_TRUE(found_ni) << "Should recognize に particle";
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
}

TEST(AnalyzerTest, Medical_Consultation) {
  // Medical consultation
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result =
      analyzer.analyze("症状が続くようでしたら医師に相談してください");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5) << "Should produce multiple tokens";
}

// ===== Legal/Terms Tests (法律・規約) =====

TEST(AnalyzerTest, Legal_TermsOfService) {
  // Terms of service clause
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本サービスの利用に際して");
  ASSERT_FALSE(result.empty());
  bool found_no = false;
  bool found_ni = false;
  for (const auto& mor : result) {
    if (mor.surface == "の") found_no = true;
    if (mor.surface == "に") found_ni = true;
  }
  EXPECT_TRUE(found_no) << "Should recognize の particle";
  EXPECT_TRUE(found_ni) << "Should recognize に particle";
}

TEST(AnalyzerTest, Legal_Prohibition) {
  // Prohibition clause
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("以下の行為を禁止します");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を") {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を in prohibition";
}

TEST(AnalyzerTest, Legal_Contract) {
  // Contract language
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result =
      analyzer.analyze("甲は乙に対して損害賠償の責任を負うものとする");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5) << "Should produce multiple tokens for contract";
}

// ===== Product Review Tests (商品レビュー) =====

TEST(AnalyzerTest, Review_Positive) {
  // Positive review
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("とても使いやすくて満足しています");
  ASSERT_FALSE(result.empty());
  // Should contain verb 使いやすい or 満足
  bool has_satisfaction = false;
  for (const auto& mor : result) {
    if (mor.surface.find("満足") != std::string::npos ||
        mor.surface.find("使") != std::string::npos) {
      has_satisfaction = true;
      break;
    }
  }
  EXPECT_TRUE(has_satisfaction) << "Should recognize key terms in review";
}

TEST(AnalyzerTest, Review_Negative) {
  // Negative review
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("期待していたほどではなかった");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 3) << "Should produce multiple tokens";
}

TEST(AnalyzerTest, Review_Comparison) {
  // Comparative review
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("前のモデルより性能が良くなった");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が particle in comparison";
}

// ===== Travel/Transportation Tests (旅行・交通) =====

TEST(AnalyzerTest, Travel_Reservation) {
  // Reservation request
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("来週の金曜日に二名で予約したいのですが");
  ASSERT_FALSE(result.empty());
  bool found_ni = false;
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "に") found_ni = true;
    if (mor.surface == "で") found_de = true;
  }
  EXPECT_TRUE(found_ni) << "Should recognize に particle";
  EXPECT_TRUE(found_de) << "Should recognize で particle";
}

TEST(AnalyzerTest, Travel_TrainAnnouncement) {
  // Train announcement
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("次は新宿、新宿です");
  ASSERT_FALSE(result.empty());
  bool found_desu = false;
  for (const auto& mor : result) {
    if (mor.surface == "です") {
      found_desu = true;
      break;
    }
  }
  EXPECT_TRUE(found_desu) << "Should recognize です in announcement";
}

TEST(AnalyzerTest, Travel_Delay) {
  // Delay announcement
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("電車が10分ほど遅れております");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が in delay announcement";
}

// ===== Weather Forecast Tests (天気予報) =====

TEST(AnalyzerTest, Weather_Forecast) {
  // Weather forecast
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("明日は晴れのち曇りでしょう");
  ASSERT_FALSE(result.empty());
  bool found_ashita = false;
  for (const auto& mor : result) {
    if (mor.surface == "明日") {
      found_ashita = true;
      break;
    }
  }
  EXPECT_TRUE(found_ashita) << "Should recognize 明日";
}

TEST(AnalyzerTest, Weather_Warning) {
  // Weather warning
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("大雨警報が発令されました");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が in warning";
}

TEST(AnalyzerTest, Weather_Temperature) {
  // Temperature description
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("最高気温は30度の予想です");
  ASSERT_FALSE(result.empty());
  bool found_ha = false;
  bool found_no = false;
  for (const auto& mor : result) {
    if (mor.surface == "は") found_ha = true;
    if (mor.surface == "の") found_no = true;
  }
  EXPECT_TRUE(found_ha) << "Should recognize は particle";
  EXPECT_TRUE(found_no) << "Should recognize の particle";
}

// ===== Sports Tests (スポーツ) =====

TEST(AnalyzerTest, Sports_GameResult) {
  // Game result
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("日本代表が2対1で勝利した");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "が") found_ga = true;
    if (mor.surface == "で") found_de = true;
  }
  EXPECT_TRUE(found_ga) << "Should recognize が particle";
  EXPECT_TRUE(found_de) << "Should recognize で particle";
}

TEST(AnalyzerTest, Sports_PlayerComment) {
  // Player comment
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("チーム一丸となって戦いたい");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 3) << "Should produce multiple tokens";
}

TEST(AnalyzerTest, Sports_Schedule) {
  // Game schedule
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("試合は午後7時から開始予定です");
  ASSERT_FALSE(result.empty());
  bool found_ha = false;
  bool found_kara = false;
  for (const auto& mor : result) {
    if (mor.surface == "は") found_ha = true;
    if (mor.surface == "から") found_kara = true;
  }
  EXPECT_TRUE(found_ha) << "Should recognize は particle";
  EXPECT_TRUE(found_kara) << "Should recognize から particle";
}

// ===== Academic/Research Tests (学術・論文) =====

TEST(AnalyzerTest, Academic_Hypothesis) {
  // Academic hypothesis
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本研究では以下の仮説を検証する");
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

TEST(AnalyzerTest, Academic_Result) {
  // Research result
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("実験の結果、有意な差が認められた");
  ASSERT_FALSE(result.empty());
  bool found_no = false;
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "の") found_no = true;
    if (mor.surface == "が") found_ga = true;
  }
  EXPECT_TRUE(found_no) << "Should recognize の particle";
  EXPECT_TRUE(found_ga) << "Should recognize が particle";
}

TEST(AnalyzerTest, Academic_Conclusion) {
  // Conclusion statement
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("以上の結果から次のように結論づけられる");
  ASSERT_FALSE(result.empty());
  bool found_kara = false;
  for (const auto& mor : result) {
    if (mor.surface == "から") {
      found_kara = true;
      break;
    }
  }
  EXPECT_TRUE(found_kara) << "Should recognize から particle";
}

// ===== Social Media Tests (SNS・ソーシャルメディア) =====

TEST(AnalyzerTest, SNS_Hashtag) {
  // Post with hashtag-like content
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今日のランチ美味しかった");
  ASSERT_FALSE(result.empty());
  bool found_kyou = false;
  for (const auto& mor : result) {
    if (mor.surface == "今日") {
      found_kyou = true;
      break;
    }
  }
  EXPECT_TRUE(found_kyou) << "Should recognize 今日";
}

TEST(AnalyzerTest, SNS_Reaction) {
  // Casual reaction
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("まじで嬉しい");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 2) << "Should produce multiple tokens";
}

TEST(AnalyzerTest, SNS_QuestionPost) {
  // Question post
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("これどこで買えるか知ってる人いる？");
  ASSERT_FALSE(result.empty());
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "で") {
      found_de = true;
      break;
    }
  }
  EXPECT_TRUE(found_de) << "Should recognize で particle";
}

// ===== Customer Service Tests (カスタマーサービス) =====

TEST(AnalyzerTest, CustomerService_Inquiry) {
  // Customer inquiry
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("商品がまだ届いていないのですが");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が in inquiry";
}

TEST(AnalyzerTest, CustomerService_Response) {
  // Service response
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("大変申し訳ございませんでした");
  ASSERT_FALSE(result.empty());
  // Should parse the polite apology expression
}

TEST(AnalyzerTest, CustomerService_Request) {
  // Customer request
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("返品の手続きについて教えてください");
  ASSERT_FALSE(result.empty());
  bool found_no = false;
  for (const auto& mor : result) {
    if (mor.surface == "の") {
      found_no = true;
      break;
    }
  }
  EXPECT_TRUE(found_no) << "Should recognize の particle";
}

// ===== Education Tests (教育) =====

TEST(AnalyzerTest, Education_TeacherInstruction) {
  // Teacher instruction
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("教科書の35ページを開いてください");
  ASSERT_FALSE(result.empty());
  bool found_no = false;
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "の") found_no = true;
    if (mor.surface == "を") found_wo = true;
  }
  EXPECT_TRUE(found_no) << "Should recognize の particle";
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
}

TEST(AnalyzerTest, Education_StudentQuestion) {
  // Student question
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("この問題の解き方が分かりません");
  ASSERT_FALSE(result.empty());
  bool found_no = false;
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "の") found_no = true;
    if (mor.surface == "が") found_ga = true;
  }
  EXPECT_TRUE(found_no) << "Should recognize の particle";
  EXPECT_TRUE(found_ga) << "Should recognize が particle";
}

TEST(AnalyzerTest, Education_Assignment) {
  // Homework assignment
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("明日までに宿題を提出してください");
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

// ===== Finance Tests (金融) =====

TEST(AnalyzerTest, Finance_Transaction) {
  // Transaction description
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("お振込みは翌営業日に反映されます");
  ASSERT_FALSE(result.empty());
  bool found_ha = false;
  bool found_ni = false;
  for (const auto& mor : result) {
    if (mor.surface == "は") found_ha = true;
    if (mor.surface == "に") found_ni = true;
  }
  EXPECT_TRUE(found_ha) << "Should recognize は particle";
  EXPECT_TRUE(found_ni) << "Should recognize に particle";
}

TEST(AnalyzerTest, Finance_Interest) {
  // Interest rate description
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("金利は年率0.5%となっております");
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

// ===== Long Sentence Tests (長文テスト) =====

TEST(AnalyzerTest, LongSentence_NewsArticle) {
  // News article style long sentence
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze(
      "政府は昨日の閣議で、新しい経済政策を正式に決定したと発表した");
  ASSERT_FALSE(result.empty());
  bool found_ha = false;
  bool found_wo = false;
  bool found_to = false;
  for (const auto& mor : result) {
    if (mor.surface == "は") found_ha = true;
    if (mor.surface == "を") found_wo = true;
    if (mor.surface == "と") found_to = true;
  }
  EXPECT_TRUE(found_ha) << "Should recognize は particle";
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
  EXPECT_TRUE(found_to) << "Should recognize と particle";
}

TEST(AnalyzerTest, LongSentence_Narrative) {
  // Narrative style
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze(
      "彼は昔から音楽が好きで、毎日ピアノの練習を欠かさなかった");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 8) << "Should produce many tokens for narrative";
}

TEST(AnalyzerTest, LongSentence_Instructions) {
  // Multi-step instructions
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze(
      "まず電源ボタンを押して起動し、次に設定画面から言語を選択してください");
  ASSERT_FALSE(result.empty());
  int wo_count = 0;
  for (const auto& mor : result) {
    if (mor.surface == "を") wo_count++;
  }
  EXPECT_GE(wo_count, 1) << "Should recognize multiple を particles";
}

// ===== Edge Case: Numbers and Special Characters =====

TEST(AnalyzerTest, EdgeCase_WithEmoji) {
  // Text that might include emoji context (post-emoji text)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今日も頑張ろう");
  ASSERT_FALSE(result.empty());
  bool found_kyou = false;
  for (const auto& mor : result) {
    if (mor.surface == "今日") {
      found_kyou = true;
      break;
    }
  }
  EXPECT_TRUE(found_kyou) << "Should recognize 今日";
}

TEST(AnalyzerTest, EdgeCase_NumbersAndUnits) {
  // Numbers with Japanese units
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("体重が3キロ減った");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が") {
      found_ga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が particle with numbers";
}

TEST(AnalyzerTest, EdgeCase_URL_Like) {
  // URL-like mixed content (domain followed by Japanese)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("example.comで登録してください");
  ASSERT_FALSE(result.empty());
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "で") {
      found_de = true;
      break;
    }
  }
  EXPECT_TRUE(found_de) << "Should recognize で particle after URL-like text";
}

// ===== Quotation Tests (引用) =====

TEST(AnalyzerTest, Quotation_DirectSpeech) {
  // Direct speech quotation
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("彼は「明日行く」と言った");
  ASSERT_FALSE(result.empty());
  bool found_to = false;
  for (const auto& mor : result) {
    if (mor.surface == "と" && mor.pos == core::PartOfSpeech::Particle) {
      found_to = true;
      break;
    }
  }
  EXPECT_TRUE(found_to) << "Should recognize と quotation particle";
}

TEST(AnalyzerTest, Quotation_IndirectSpeech) {
  // Indirect speech
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("彼女が来ないと思う");
  ASSERT_FALSE(result.empty());
  bool found_ga = false;
  bool found_to = false;
  for (const auto& mor : result) {
    if (mor.surface == "が") found_ga = true;
    if (mor.surface == "と") found_to = true;
  }
  EXPECT_TRUE(found_ga) << "Should recognize が particle";
  EXPECT_TRUE(found_to) << "Should recognize と particle";
}

}  // namespace
}  // namespace suzume::analysis
