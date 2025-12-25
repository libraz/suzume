// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Regression tests to ensure previously fixed bugs don't reoccur

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// =============================================================================
// Regression: Particle を separation
// =============================================================================
// Bug: をなくしてしまった was being merged as one token
// Fix: を should always be recognized as separate particle

TEST(AnalyzerTest, Regression_WoParticleSeparation) {
  // 本をなくした - を should be separate particle
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本をなくした");
  ASSERT_FALSE(result.empty());

  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "を should be recognized as separate particle";
}

TEST(AnalyzerTest, Regression_WoNotMergedWithVerb) {
  // をなくして should not be merged as single unknown word
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("をなくして");
  ASSERT_FALSE(result.empty());

  // First token should be を as particle
  EXPECT_EQ(result[0].surface, "を");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Particle);
}

TEST(AnalyzerTest, Regression_WoInComplex) {
  // Full sentence: 昨日買ったばかりの本をなくしてしまった
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("昨日買ったばかりの本をなくしてしまった");
  ASSERT_FALSE(result.empty());

  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo) << "を should be separate particle in complex sentence";
}

// =============================================================================
// Regression: ので lemma
// =============================================================================
// Bug: ので lemma was のる (incorrectly treated as verb)
// Fix: ので lemma should be ので (particle/conjunction doesn't conjugate)

TEST(AnalyzerTest, Regression_NodeLemma) {
  // ので should have lemma ので (not のる)
  // Use full Suzume pipeline which includes lemmatization
  Suzume analyzer;
  auto result = analyzer.analyze("ので");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "ので");
  EXPECT_EQ(result[0].lemma, "ので") << "ので lemma should be ので, not のる";
}

TEST(AnalyzerTest, Regression_NodeInSentence) {
  // 電車が遅れているので遅刻しそうです
  Suzume analyzer;
  auto result = analyzer.analyze("電車が遅れているので遅刻しそうです");
  ASSERT_FALSE(result.empty());

  bool found_node = false;
  for (const auto& mor : result) {
    if (mor.surface == "ので") {
      found_node = true;
      EXPECT_EQ(mor.lemma, "ので") << "ので lemma should be ので";
      break;
    }
  }
  EXPECT_TRUE(found_node) << "ので should be recognized";
}

// =============================================================================
// Regression: しそう auxiliary lemma
// =============================================================================
// Bug: 遅刻しそう lemma was 遅刻しい (incorrect)
// Fix: しそう pattern should produce correct lemma 遅刻する

TEST(AnalyzerTest, Regression_ShisouLemma) {
  // 遅刻しそう should have lemma 遅刻する
  // Use full Suzume pipeline which includes lemmatization
  Suzume analyzer;
  auto result = analyzer.analyze("遅刻しそう");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "遅刻しそう" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "遅刻する")
          << "遅刻しそう lemma should be 遅刻する";
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "遅刻しそう should be recognized as verb";
}

TEST(AnalyzerTest, Regression_SouAuxiliaryPattern) {
  // 食べそう should have lemma 食べる
  Suzume analyzer;
  auto result = analyzer.analyze("食べそう");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "食べそう" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "食べる") << "食べそう lemma should be 食べる";
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "食べそう should be recognized as verb";
}

TEST(AnalyzerTest, Regression_SouWithDesu) {
  // 遅刻しそうです should have lemma 遅刻する for the verb part
  Suzume analyzer;
  auto result = analyzer.analyze("遅刻しそうです");
  ASSERT_FALSE(result.empty());

  bool found_chikoku = false;
  for (const auto& mor : result) {
    if (mor.surface.find("遅刻") != std::string::npos &&
        mor.pos == core::PartOfSpeech::Verb) {
      found_chikoku = true;
      EXPECT_EQ(mor.lemma, "遅刻する")
          << "遅刻しそうです verb part lemma should be 遅刻する";
      break;
    }
  }
  EXPECT_TRUE(found_chikoku) << "遅刻しそうです should contain verb with 遅刻";
}

// =============================================================================
// Script Boundary Tests (文字種境界テスト)
// =============================================================================
// Tests for proper segmentation at script boundaries (ASCII/Japanese)

TEST(AnalyzerTest, ScriptBoundary_AsciiToJapaneseVerb) {
  // "iphone買った" should split at ASCII→Japanese boundary
  // Expected: "iphone" (NOUN) + "買った" (VERB)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("iphone買った");
  ASSERT_FALSE(result.empty());

  // Should have at least 2 tokens (iphone + verb)
  EXPECT_GE(result.size(), 2)
      << "iphone買った should split into at least 2 tokens";

  // Check that we have a verb token that looks conjugated
  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      // The verb should contain 買 (not merged with iphone)
      EXPECT_TRUE(mor.surface.find("買") != std::string::npos ||
                  mor.surface.find("った") != std::string::npos)
          << "Verb token should contain 買 or った, got: " << mor.surface;
    }
  }
  EXPECT_TRUE(found_verb) << "Should find a verb token in iphone買った";
}

TEST(AnalyzerTest, ScriptBoundary_ParticlelessNounVerb) {
  // "本買った" without particle should still split noun and verb
  // This is a colloquial pattern (本を買った with を omitted)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("本買った");
  ASSERT_FALSE(result.empty());

  // Should have at least 2 tokens (noun + verb)
  EXPECT_GE(result.size(), 2)
      << "本買った should split into at least 2 tokens";

  // Check that 買った is recognized as a separate verb
  bool found_verb_with_katta = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb &&
        mor.surface.find("買") != std::string::npos) {
      found_verb_with_katta = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb_with_katta)
      << "買った should be recognized as a verb in 本買った";
}

TEST(AnalyzerTest, ScriptBoundary_MixedWithTeForm) {
  // "買ってきた" - compound verb with て-form
  // Should be recognized as a single verb unit with correct lemma
  // Note: Using Suzume (not Analyzer) because lemmatization requires
  // the full pipeline including Postprocessor with dictionary verification
  Suzume suzume;
  auto result = suzume.analyze("買ってきた");
  ASSERT_FALSE(result.empty());

  // Check that it's recognized as verb with correct lemma
  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      // Lemma should be 買う (base form of the main verb)
      // This requires dictionary-aware lemmatization to disambiguate
      // between GodanWa (買う), GodanRa (買る), and GodanTa (買つ)
      EXPECT_EQ(mor.lemma, "買う")
          << "買ってきた should have lemma 買う, got: " << mor.lemma;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "買ってきた should be recognized as verb";
}

// =============================================================================
// Regression: Copula だった (断定の助動詞)
// =============================================================================
// Bug: だった was recognized as VERB with lemma だる
// Fix: だった should be AUX with lemma だった (copula doesn't conjugate to だる)

TEST(AnalyzerTest, Regression_DattaCopulaPos) {
  // だった should be recognized as Auxiliary, not Verb
  Suzume analyzer;
  auto result = analyzer.analyze("神だった");
  ASSERT_FALSE(result.empty());

  bool found_datta = false;
  for (const auto& mor : result) {
    if (mor.surface == "だった") {
      found_datta = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "だった should be Auxiliary, not Verb";
      break;
    }
  }
  EXPECT_TRUE(found_datta) << "だった should be found in 神だった";
}

TEST(AnalyzerTest, Regression_DattaCopulaLemma) {
  // だった lemma should be だった, not だる
  Suzume analyzer;
  auto result = analyzer.analyze("本だった");
  ASSERT_FALSE(result.empty());

  bool found_datta = false;
  for (const auto& mor : result) {
    if (mor.surface == "だった") {
      found_datta = true;
      EXPECT_EQ(mor.lemma, "だった")
          << "だった lemma should be だった, not だる";
      break;
    }
  }
  EXPECT_TRUE(found_datta) << "だった should be found in 本だった";
}

TEST(AnalyzerTest, Regression_DattaInSentence) {
  // Full sentence with だった
  Suzume analyzer;
  auto result = analyzer.analyze("ワンマンライブのセットリストが神だった");
  ASSERT_FALSE(result.empty());

  bool found_datta = false;
  for (const auto& mor : result) {
    if (mor.surface == "だった") {
      found_datta = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "だった should be Auxiliary in sentence";
      EXPECT_EQ(mor.lemma, "だった")
          << "だった lemma should be だった in sentence";
      break;
    }
  }
  EXPECT_TRUE(found_datta) << "だった should be found in sentence";
}

TEST(AnalyzerTest, Regression_DeshitaCopula) {
  // でした (polite past copula) should also be Auxiliary
  Suzume analyzer;
  auto result = analyzer.analyze("本でした");
  ASSERT_FALSE(result.empty());

  bool found_deshita = false;
  for (const auto& mor : result) {
    if (mor.surface == "でした") {
      found_deshita = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "でした should be Auxiliary";
      EXPECT_EQ(mor.lemma, "でした")
          << "でした lemma should be でした";
      break;
    }
  }
  EXPECT_TRUE(found_deshita) << "でした should be found in 本でした";
}

TEST(AnalyzerTest, Regression_DeattaCopula) {
  // であった (formal past copula) should also be Auxiliary
  Suzume analyzer;
  auto result = analyzer.analyze("重要であった");
  ASSERT_FALSE(result.empty());

  bool found_deatta = false;
  for (const auto& mor : result) {
    if (mor.surface == "であった") {
      found_deatta = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "であった should be Auxiliary";
      EXPECT_EQ(mor.lemma, "であった")
          << "であった lemma should be であった";
      break;
    }
  }
  EXPECT_TRUE(found_deatta) << "であった should be found in 重要であった";
}

// =============================================================================
// Regression: Honorific verb pattern (お + renyokei + いたす)
// =============================================================================
// Bug: お伝えいたします was split incorrectly as 伝えい + たします
// Fix: Should be お + 伝え + いたします

TEST(AnalyzerTest, Regression_HonorificVerbOtsutae) {
  // お伝えいたします should split as: お + 伝え + いたします
  Suzume analyzer;
  auto result = analyzer.analyze("お伝えいたします");
  ASSERT_FALSE(result.empty());

  // Should find 伝え as verb with lemma 伝える
  bool found_tsutae = false;
  for (const auto& mor : result) {
    if (mor.surface == "伝え") {
      found_tsutae = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "伝え should be Verb";
      EXPECT_EQ(mor.lemma, "伝える")
          << "伝え lemma should be 伝える";
    }
  }
  EXPECT_TRUE(found_tsutae)
      << "伝え should be found as separate token in お伝えいたします";

  // Should find いたします
  bool found_itashimasu = false;
  for (const auto& mor : result) {
    if (mor.surface == "いたします") {
      found_itashimasu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "いたします should be Verb";
    }
  }
  EXPECT_TRUE(found_itashimasu)
      << "いたします should be found in お伝えいたします";
}

// =============================================================================
// Regression: Suru-noun + いたす pattern
// =============================================================================
// Bug: 検討いたします was incorrectly analyzed with 検討い as adjective
// Fix: Should be 検討 + いたします

TEST(AnalyzerTest, Regression_SuruNounItasu) {
  // 検討いたします should split as: 検討 + いたします
  Suzume analyzer;
  auto result = analyzer.analyze("検討いたします");
  ASSERT_FALSE(result.empty());

  // Should find 検討 as noun
  bool found_kentou = false;
  for (const auto& mor : result) {
    if (mor.surface == "検討") {
      found_kentou = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "検討 should be Noun";
    }
  }
  EXPECT_TRUE(found_kentou)
      << "検討 should be found as separate token in 検討いたします";

  // Should find いたします
  bool found_itashimasu = false;
  for (const auto& mor : result) {
    if (mor.surface == "いたします") {
      found_itashimasu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "いたします should be Verb";
    }
  }
  EXPECT_TRUE(found_itashimasu)
      << "いたします should be found in 検討いたします";
}

// =============================================================================
// Regression: Prefix + compound noun (お + 買い物)
// =============================================================================
// Bug: お買い物 was split as お + 買い物 instead of joined
// Fix: Should be recognized as single NOUN token

TEST(AnalyzerTest, Regression_PrefixCompoundNoun) {
  // お買い物 should be a single NOUN token
  Suzume analyzer;
  auto result = analyzer.analyze("お買い物");
  ASSERT_FALSE(result.empty());

  // Should be single token
  EXPECT_EQ(result.size(), 1)
      << "お買い物 should be single token, got " << result.size();

  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "お買い物");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "お買い物 should be Noun";
  }
}

// =============================================================================
// Regression: I-adjective recognition
// =============================================================================
// Bug: 悲しい was incorrectly recognized as Verb
// Fix: Should be recognized as Adjective

TEST(AnalyzerTest, Regression_IAdjectiveKanashii) {
  // 悲しい should be recognized as Adjective
  Suzume analyzer;
  auto result = analyzer.analyze("悲しい");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "悲しい should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "悲しい");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "悲しい should be Adjective, not Verb";
    EXPECT_EQ(result[0].lemma, "悲しい")
        << "悲しい lemma should be 悲しい";
  }
}

TEST(AnalyzerTest, Regression_IAdjectiveUtsukushikatta) {
  // 美しかった should be recognized as Adjective with lemma 美しい
  Suzume analyzer;
  auto result = analyzer.analyze("美しかった");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "美しかった should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "美しかった");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "美しかった should be Adjective";
    EXPECT_EQ(result[0].lemma, "美しい")
        << "美しかった lemma should be 美しい";
  }
}

// =============================================================================
// Regression: Adjective + particle pattern
// =============================================================================
// Bug: 面白いな was not properly splitting adjective and particle
// Fix: Should be 面白い (ADJ) + な (PARTICLE)

TEST(AnalyzerTest, Regression_AdjectiveParticleOmoshiroina) {
  // 面白いな should split as: 面白い + な
  Suzume analyzer;
  auto result = analyzer.analyze("面白いな");
  ASSERT_GE(result.size(), 2) << "面白いな should have at least 2 tokens";

  bool found_omoshiroi = false;
  bool found_na = false;
  for (const auto& mor : result) {
    if (mor.surface == "面白い") {
      found_omoshiroi = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "面白い should be Adjective";
    }
    if (mor.surface == "な" && mor.pos == core::PartOfSpeech::Particle) {
      found_na = true;
    }
  }
  EXPECT_TRUE(found_omoshiroi) << "面白い should be found";
  EXPECT_TRUE(found_na) << "な particle should be found";
}

// =============================================================================
// Regression: Irregular adjective いい
// =============================================================================
// Bug: いいよね was not properly tokenized (いい not recognized)
// Fix: いい should be recognized as Adjective

TEST(AnalyzerTest, Regression_IrregularAdjectiveIi) {
  // いいよね should split as: いい + よ + ね (or いい + よね)
  Suzume analyzer;
  auto result = analyzer.analyze("いいよね");
  ASSERT_FALSE(result.empty());

  // Should find いい as adjective
  bool found_ii = false;
  for (const auto& mor : result) {
    if (mor.surface == "いい") {
      found_ii = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "いい should be Adjective";
    }
  }
  EXPECT_TRUE(found_ii) << "いい should be found in いいよね";

  // Should have sentence-ending particles
  bool found_particle = false;
  for (const auto& mor : result) {
    if ((mor.surface == "よ" || mor.surface == "ね" ||
         mor.surface == "よね") &&
        mor.pos == core::PartOfSpeech::Particle) {
      found_particle = true;
    }
  }
  EXPECT_TRUE(found_particle)
      << "Sentence-ending particle should be found in いいよね";
}

// =============================================================================
// Regression: Ichidan verb 用いる recognition
// =============================================================================
// Bug: 用いて was parsed as ADJ 用い + PARTICLE て, lemma was wrong
// Fix: Should be VERB 用いて with lemma 用いる

TEST(AnalyzerTest, Regression_IchidanVerbMochiite) {
  Suzume analyzer;
  auto result = analyzer.analyze("用いて");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "用いて should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "用いて");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
        << "用いて should be Verb, not Adjective";
    EXPECT_EQ(result[0].lemma, "用いる")
        << "用いて lemma should be 用いる (Ichidan)";
  }
}

// =============================================================================
// Regression: GodanWa verb 行う lemmatization
// =============================================================================
// Bug: 行います lemma was incorrectly 行いる (as Ichidan)
// Fix: Should be 行う (GodanWa)

TEST(AnalyzerTest, Regression_GodanWaVerbOkonaimasu) {
  Suzume analyzer;
  auto result = analyzer.analyze("行います");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "行います should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "行います");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
        << "行います should be Verb";
    EXPECT_EQ(result[0].lemma, "行う")
        << "行います lemma should be 行う (GodanWa), not 行いる";
  }
}

// =============================================================================
// Regression: Compound noun 飲み会
// =============================================================================
// Bug: 飲み会 was split as 飲 + み + 会
// Fix: Should be single NOUN token

TEST(AnalyzerTest, Regression_CompoundNounNomikai) {
  Suzume analyzer;
  auto result = analyzer.analyze("飲み会");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "飲み会 should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "飲み会");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "飲み会 should be Noun";
  }
}

// =============================================================================
// Regression: Humble verb 恐れ入る
// =============================================================================
// Bug: 恐れ入ります was split as 恐 + れ + 入ります
// Fix: Should be single VERB token with lemma 恐れ入る

TEST(AnalyzerTest, Regression_HumbleVerbOsoreirimasu) {
  Suzume analyzer;
  auto result = analyzer.analyze("恐れ入ります");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "恐れ入ります should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "恐れ入ります");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
        << "恐れ入ります should be Verb";
    EXPECT_EQ(result[0].lemma, "恐れ入る")
        << "恐れ入ります lemma should be 恐れ入る";
  }
}

// =============================================================================
// Regression: Noun 楽しみ
// =============================================================================
// Bug: 楽しみ was incorrectly tokenized (楽 + しみ)
// Fix: Should be single NOUN token

TEST(AnalyzerTest, Regression_NounTanoshimi) {
  Suzume analyzer;
  auto result = analyzer.analyze("楽しみ");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "楽しみ should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "楽しみ");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "楽しみ should be Noun";
  }
}

// =============================================================================
// Regression: Colloquial adverb めっちゃ
// =============================================================================
// Bug: めっちゃ was classified as OTHER
// Fix: Should be ADVERB

TEST(AnalyzerTest, Regression_ColloquialAdverbMeccha) {
  Suzume analyzer;
  auto result = analyzer.analyze("めっちゃ面白い");
  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";

  bool found_meccha = false;
  for (const auto& mor : result) {
    if (mor.surface == "めっちゃ") {
      found_meccha = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adverb)
          << "めっちゃ should be Adverb, not Other";
    }
  }
  EXPECT_TRUE(found_meccha) << "めっちゃ should be found";
}

// =============================================================================
// Regression: GodanWa verb renyokei 伴い
// =============================================================================
// Bug: 伴い was split as 伴 + い or classified as ADJ
// Fix: Should be VERB with lemma 伴う

TEST(AnalyzerTest, Regression_GodanWaVerbTomonai) {
  Suzume analyzer;
  auto result = analyzer.analyze("景気回復に伴い");
  ASSERT_GE(result.size(), 3) << "Should have at least 3 tokens";

  bool found_tomonai = false;
  for (const auto& mor : result) {
    if (mor.surface == "伴い") {
      found_tomonai = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "伴い should be Verb, not Noun+Other or Adjective";
      EXPECT_EQ(mor.lemma, "伴う")
          << "伴い lemma should be 伴う (GodanWa)";
    }
  }
  EXPECT_TRUE(found_tomonai) << "伴い should be found as single token";
}

// =============================================================================
// Regression: Single-kanji i-adjective 寒い
// =============================================================================
// Bug: 寒い was split as 寒 + い due to ADJ candidate skip heuristic
// Fix: Should be single ADJ token via dictionary

TEST(AnalyzerTest, Regression_IAdjective_Samui) {
  Suzume analyzer;
  auto result = analyzer.analyze("今日は寒いですね");
  ASSERT_GE(result.size(), 4) << "Should have at least 4 tokens";

  bool found_samui = false;
  for (const auto& mor : result) {
    if (mor.surface == "寒い") {
      found_samui = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "寒い should be Adjective";
      EXPECT_EQ(mor.lemma, "寒い")
          << "寒い lemma should be 寒い";
    }
  }
  EXPECT_TRUE(found_samui) << "寒い should be found as single token";
}

// =============================================================================
// Regression: Verb with ましょう auxiliary
// =============================================================================
// Bug: 行きましょう was split as 行 + きましょう
// Fix: Added ましょう to inflection auxiliaries

TEST(AnalyzerTest, Regression_MashouAuxiliary_Ikimashou) {
  Suzume analyzer;
  auto result = analyzer.analyze("行きましょう");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "行きましょう should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "行きましょう");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
        << "行きましょう should be Verb";
    EXPECT_EQ(result[0].lemma, "行く")
        << "行きましょう lemma should be 行く";
  }
}

TEST(AnalyzerTest, Regression_MashouAuxiliary_Tabemashou) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べましょう");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "食べましょう should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "食べましょう");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
        << "食べましょう should be Verb";
    EXPECT_EQ(result[0].lemma, "食べる")
        << "食べましょう lemma should be 食べる";
  }
}

// =============================================================================
// Regression: Na-adjective 好き
// =============================================================================
// Bug: 好き was split as 好 + き
// Fix: Added 好き to na_adjectives.h

TEST(AnalyzerTest, Regression_NaAdjective_Suki) {
  Suzume analyzer;
  auto result = analyzer.analyze("好き");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "好き should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "好き");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "好き should be Adjective";
  }
}

TEST(AnalyzerTest, Regression_NaAdjective_SukiNa) {
  Suzume analyzer;
  auto result = analyzer.analyze("好きな食べ物");
  ASSERT_GE(result.size(), 3) << "Should have at least 3 tokens";

  bool found_suki = false;
  bool found_na = false;
  bool found_tabemono = false;
  for (const auto& mor : result) {
    if (mor.surface == "好き") {
      found_suki = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "好き should be Adjective";
    }
    if (mor.surface == "な" && mor.pos == core::PartOfSpeech::Particle) {
      found_na = true;
    }
    if (mor.surface == "食べ物") {
      found_tabemono = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "食べ物 should be Noun";
    }
  }
  EXPECT_TRUE(found_suki) << "好き should be found";
  EXPECT_TRUE(found_na) << "な particle should be found";
  EXPECT_TRUE(found_tabemono) << "食べ物 should be found";
}

TEST(AnalyzerTest, Regression_NaAdjective_Kirai) {
  Suzume analyzer;
  auto result = analyzer.analyze("嫌い");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "嫌い should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "嫌い");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective)
        << "嫌い should be Adjective";
  }
}

// =============================================================================
// Regression: Compound noun splitting (毎日電車)
// =============================================================================
// Bug: 毎日電車 was analyzed as single unknown token
// Fix: Fixed cost and is_formal_noun flag propagation in split candidates

TEST(AnalyzerTest, Regression_CompoundSplit_MainichiDensha) {
  Suzume analyzer;
  auto result = analyzer.analyze("毎日電車");
  ASSERT_GE(result.size(), 2) << "毎日電車 should split into at least 2 tokens";

  bool found_mainichi = false;
  bool found_densha = false;
  for (const auto& mor : result) {
    if (mor.surface == "毎日") {
      found_mainichi = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "毎日 should be Noun";
    }
    if (mor.surface == "電車") {
      found_densha = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "電車 should be Noun";
    }
  }
  EXPECT_TRUE(found_mainichi) << "毎日 should be found as separate token";
  EXPECT_TRUE(found_densha) << "電車 should be found as separate token";
}

TEST(AnalyzerTest, Regression_CompoundSplit_MainichiDenshaDeKommute) {
  Suzume analyzer;
  auto result = analyzer.analyze("毎日電車で通勤");
  ASSERT_GE(result.size(), 4) << "Should have at least 4 tokens";

  bool found_mainichi = false;
  bool found_densha = false;
  bool found_de = false;
  bool found_tsuukin = false;
  for (const auto& mor : result) {
    if (mor.surface == "毎日") found_mainichi = true;
    if (mor.surface == "電車") found_densha = true;
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle)
      found_de = true;
    if (mor.surface == "通勤") found_tsuukin = true;
  }
  EXPECT_TRUE(found_mainichi) << "毎日 should be found";
  EXPECT_TRUE(found_densha) << "電車 should be found";
  EXPECT_TRUE(found_de) << "で particle should be found";
  EXPECT_TRUE(found_tsuukin) << "通勤 should be found";
}

// =============================================================================
// Regression: Compound nouns (食べ物, 飲み物, 買い物)
// =============================================================================
// Bug: 食べ物 was split as 食 + べ + 物
// Fix: Added these compound nouns to common_vocabulary.h

TEST(AnalyzerTest, Regression_CompoundNoun_Tabemono) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べ物");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "食べ物 should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "食べ物");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "食べ物 should be Noun";
  }
}

TEST(AnalyzerTest, Regression_CompoundNoun_Nomimono) {
  Suzume analyzer;
  auto result = analyzer.analyze("飲み物");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "飲み物 should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "飲み物");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "飲み物 should be Noun";
  }
}

TEST(AnalyzerTest, Regression_CompoundNoun_Kaimono) {
  Suzume analyzer;
  auto result = analyzer.analyze("買い物");
  ASSERT_FALSE(result.empty());

  EXPECT_EQ(result.size(), 1) << "買い物 should be single token";
  if (!result.empty()) {
    EXPECT_EQ(result[0].surface, "買い物");
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
        << "買い物 should be Noun";
  }
}

TEST(AnalyzerTest, Regression_CompoundNoun_KaimonoNiIku) {
  Suzume analyzer;
  auto result = analyzer.analyze("買い物に行く");
  ASSERT_GE(result.size(), 3) << "Should have at least 3 tokens";

  bool found_kaimono = false;
  bool found_ni = false;
  bool found_iku = false;
  for (const auto& mor : result) {
    if (mor.surface == "買い物") {
      found_kaimono = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "買い物 should be Noun";
    }
    if (mor.surface == "に" && mor.pos == core::PartOfSpeech::Particle) {
      found_ni = true;
    }
    if (mor.surface == "行く") {
      found_iku = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "行く should be Verb";
    }
  }
  EXPECT_TRUE(found_kaimono) << "買い物 should be found";
  EXPECT_TRUE(found_ni) << "に particle should be found";
  EXPECT_TRUE(found_iku) << "行く should be found";
}

// =============================================================================
// Regression: Te-form contraction not adjective
// =============================================================================
// Bug: 待ってく was analyzed as adjective, not 待って + く
// Fix: Skip っ + hiragana patterns in generateAdjectiveCandidates

TEST(AnalyzerTest, Regression_TeKuNotAdjective) {
  Suzume analyzer;
  auto result = analyzer.analyze("待ってくれない");
  ASSERT_GE(result.size(), 2);

  // Should be 待って + くれない, not 待ってく + れない
  bool found_matte = false;
  bool found_kurenai = false;
  for (const auto& mor : result) {
    if (mor.surface == "待って" && mor.pos == core::PartOfSpeech::Verb) {
      found_matte = true;
    }
    if (mor.surface == "くれない" && mor.pos == core::PartOfSpeech::Verb) {
      found_kurenai = true;
    }
  }
  EXPECT_TRUE(found_matte) << "待って should be recognized as verb";
  EXPECT_TRUE(found_kurenai) << "くれない should be recognized as verb";
}

// =============================================================================
// Regression: Hiragana adjective conjugation
// =============================================================================
// Bug: まずかった was split as まず + か + った
// Fix: Added generateHiraganaAdjectiveCandidates

TEST(AnalyzerTest, Regression_HiraganaAdjective) {
  Suzume analyzer;
  auto result = analyzer.analyze("まずかった");
  ASSERT_EQ(result.size(), 1) << "まずかった should be single token";
  EXPECT_EQ(result[0].surface, "まずかった");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  // Hiragana input normalizes to kanji lemma when kanji form exists
  EXPECT_EQ(result[0].lemma, "不味い");
}

TEST(AnalyzerTest, Regression_HiraganaAdjective_Oishii) {
  Suzume analyzer;
  auto result = analyzer.analyze("おいしくない");
  ASSERT_EQ(result.size(), 1) << "おいしくない should be single token";
  EXPECT_EQ(result[0].surface, "おいしくない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[0].lemma, "おいしい");
}

// =============================================================================
// Regression: Verb starting with が/か
// =============================================================================
// Bug: 上がらない was split as 上 + が + らない
// Fix: Allow が/か in first hiragana position for verbs

TEST(AnalyzerTest, Regression_VerbStartingWithGa) {
  Suzume analyzer;
  auto result = analyzer.analyze("上がらない");
  ASSERT_EQ(result.size(), 1) << "上がらない should be single token";
  EXPECT_EQ(result[0].surface, "上がらない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "上がる");
}

TEST(AnalyzerTest, Regression_VerbStartingWithKa) {
  Suzume analyzer;
  auto result = analyzer.analyze("書かない");
  ASSERT_EQ(result.size(), 1) << "書かない should be single token";
  EXPECT_EQ(result[0].surface, "書かない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "書く");
}

// =============================================================================
// Regression: Dictionary entries
// =============================================================================

TEST(AnalyzerTest, Regression_Conjunction_Nimokakawarazu) {
  Suzume analyzer;
  auto result = analyzer.analyze("にもかかわらず");
  ASSERT_EQ(result.size(), 1) << "にもかかわらず should be single token";
  EXPECT_EQ(result[0].surface, "にもかかわらず");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Conjunction);
}

TEST(AnalyzerTest, Regression_Determiner_Souiu) {
  Suzume analyzer;
  auto result = analyzer.analyze("そういうこと");
  ASSERT_GE(result.size(), 2);

  bool found_souiu = false;
  for (const auto& mor : result) {
    if (mor.surface == "そういう" && mor.pos == core::PartOfSpeech::Determiner) {
      found_souiu = true;
    }
  }
  EXPECT_TRUE(found_souiu) << "そういう should be recognized as determiner";
}

TEST(AnalyzerTest, Regression_Adverb_Imasugu) {
  Suzume analyzer;
  auto result = analyzer.analyze("今すぐ行く");
  ASSERT_GE(result.size(), 2);

  bool found_imasugu = false;
  for (const auto& mor : result) {
    if (mor.surface == "今すぐ" && mor.pos == core::PartOfSpeech::Adverb) {
      found_imasugu = true;
    }
  }
  EXPECT_TRUE(found_imasugu) << "今すぐ should be recognized as adverb";
}

// =============================================================================
// Regression: Negative auxiliary ない + んだ
// =============================================================================
// Bug: ないんだ was analyzed as verb with lemma ないむ
// Fix: Skip ない in generateHiraganaVerbCandidates (should be AUX)

TEST(AnalyzerTest, Regression_NaiNda_Split) {
  Suzume analyzer;
  auto result = analyzer.analyze("ないんだ");
  ASSERT_EQ(result.size(), 2) << "ないんだ should split into 2 tokens";

  EXPECT_EQ(result[0].surface, "ない");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Auxiliary)
      << "ない should be Auxiliary";

  EXPECT_EQ(result[1].surface, "んだ");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary)
      << "んだ should be Auxiliary";
  EXPECT_EQ(result[1].lemma, "のだ") << "んだ lemma should be のだ";
}

TEST(AnalyzerTest, Regression_NaiNda_InSentence) {
  Suzume analyzer;
  auto result = analyzer.analyze("知らないんだ");
  ASSERT_GE(result.size(), 2);

  bool found_nda = false;
  for (const auto& mor : result) {
    if (mor.surface == "んだ") {
      found_nda = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "んだ should be Auxiliary";
      EXPECT_EQ(mor.lemma, "のだ") << "んだ lemma should be のだ";
    }
  }
  EXPECT_TRUE(found_nda) << "んだ should be found as separate token";
}

// =============================================================================
// Regression: Nagara pattern (ながら形)
// =============================================================================
// Bug: 飲みながら was split as 飲 + み + ながら
// Fix: Removed early termination at particle-like characters in unknown.cpp

TEST(AnalyzerTest, Regression_NagaraPattern_Godan) {
  Suzume analyzer;
  auto result = analyzer.analyze("飲みながら");
  ASSERT_EQ(result.size(), 1) << "飲みながら should be single token";
  EXPECT_EQ(result[0].surface, "飲みながら");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "飲みながら should be Verb";
  EXPECT_EQ(result[0].lemma, "飲む")
      << "飲みながら lemma should be 飲む";
}

TEST(AnalyzerTest, Regression_NagaraPattern_Ichidan) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べながら");
  ASSERT_EQ(result.size(), 1) << "食べながら should be single token";
  EXPECT_EQ(result[0].surface, "食べながら");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "食べながら should be Verb";
  EXPECT_EQ(result[0].lemma, "食べる")
      << "食べながら lemma should be 食べる";
}

TEST(AnalyzerTest, Regression_NagaraPattern_GodanKa) {
  Suzume analyzer;
  auto result = analyzer.analyze("書きながら");
  ASSERT_EQ(result.size(), 1) << "書きながら should be single token";
  EXPECT_EQ(result[0].surface, "書きながら");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "書きながら should be Verb";
  EXPECT_EQ(result[0].lemma, "書く")
      << "書きながら lemma should be 書く";
}

TEST(AnalyzerTest, Regression_NagaraPattern_InSentence) {
  Suzume analyzer;
  auto result = analyzer.analyze("コーヒーを飲みながら読む");
  ASSERT_GE(result.size(), 4);

  bool found_nominagara = false;
  for (const auto& mor : result) {
    if (mor.surface == "飲みながら") {
      found_nominagara = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "飲みながら should be Verb";
      EXPECT_EQ(mor.lemma, "飲む")
          << "飲みながら lemma should be 飲む";
    }
  }
  EXPECT_TRUE(found_nominagara) << "飲みながら should be found as single token";
}

// =============================================================================
// Regression: Conditional form (仮定形 + ば)
// =============================================================================
// Bug: 食べれば, 書けば were not recognized as verb conjugations
// Fix: Added kVerbKatei connection and hypothetical stem entries

TEST(AnalyzerTest, Regression_ConditionalForm_Ichidan) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べれば");
  ASSERT_EQ(result.size(), 1) << "食べれば should be single token";
  EXPECT_EQ(result[0].surface, "食べれば");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "食べれば should be Verb";
  EXPECT_EQ(result[0].lemma, "食べる")
      << "食べれば lemma should be 食べる";
}

TEST(AnalyzerTest, Regression_ConditionalForm_GodanKa) {
  Suzume analyzer;
  auto result = analyzer.analyze("書けば");
  ASSERT_EQ(result.size(), 1) << "書けば should be single token";
  EXPECT_EQ(result[0].surface, "書けば");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "書けば should be Verb";
  EXPECT_EQ(result[0].lemma, "書く")
      << "書けば lemma should be 書く";
}

TEST(AnalyzerTest, Regression_ConditionalForm_GodanKa_Iku) {
  Suzume analyzer;
  auto result = analyzer.analyze("行けば");
  ASSERT_EQ(result.size(), 1) << "行けば should be single token";
  EXPECT_EQ(result[0].surface, "行けば");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "行けば should be Verb";
  EXPECT_EQ(result[0].lemma, "行く")
      << "行けば lemma should be 行く";
}

TEST(AnalyzerTest, Regression_ConditionalForm_IchidanOkiru) {
  Suzume analyzer;
  auto result = analyzer.analyze("起きれば");
  ASSERT_EQ(result.size(), 1) << "起きれば should be single token";
  EXPECT_EQ(result[0].surface, "起きれば");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "起きれば should be Verb";
  EXPECT_EQ(result[0].lemma, "起きる")
      << "起きれば lemma should be 起きる";
}

TEST(AnalyzerTest, Regression_ConditionalForm_Complex) {
  Suzume analyzer;
  auto result = analyzer.analyze("起きればよかった");
  ASSERT_GE(result.size(), 2);

  bool found_okireba = false;
  for (const auto& mor : result) {
    if (mor.surface == "起きれば") {
      found_okireba = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "起きれば should be Verb";
      EXPECT_EQ(mor.lemma, "起きる")
          << "起きれば lemma should be 起きる";
    }
  }
  EXPECT_TRUE(found_okireba) << "起きれば should be found as single token";
}

// =============================================================================
// Regression: Time noun separation (毎朝コーヒー)
// =============================================================================
// Bug: 毎朝コーヒー was merged as single noun
// Fix: Added 毎朝 to time_nouns.h with is_formal_noun=true

TEST(AnalyzerTest, Regression_TimeNoun_MainchoSplit) {
  Suzume analyzer;
  auto result = analyzer.analyze("毎朝コーヒー");
  ASSERT_GE(result.size(), 2) << "毎朝コーヒー should split into at least 2 tokens";

  bool found_maiasa = false;
  bool found_coffee = false;
  for (const auto& mor : result) {
    if (mor.surface == "毎朝") {
      found_maiasa = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "毎朝 should be Noun";
    }
    if (mor.surface == "コーヒー") {
      found_coffee = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "コーヒー should be Noun";
    }
  }
  EXPECT_TRUE(found_maiasa) << "毎朝 should be found as separate token";
  EXPECT_TRUE(found_coffee) << "コーヒー should be found as separate token";
}

TEST(AnalyzerTest, Regression_TimeNoun_FullSentence) {
  Suzume analyzer;
  auto result = analyzer.analyze("毎朝コーヒーを飲みながら新聞を読む");
  ASSERT_GE(result.size(), 6);

  bool found_maiasa = false;
  bool found_coffee = false;
  bool found_nominagara = false;
  for (const auto& mor : result) {
    if (mor.surface == "毎朝") found_maiasa = true;
    if (mor.surface == "コーヒー") found_coffee = true;
    if (mor.surface == "飲みながら") {
      found_nominagara = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb);
      EXPECT_EQ(mor.lemma, "飲む");
    }
  }
  EXPECT_TRUE(found_maiasa) << "毎朝 should be found";
  EXPECT_TRUE(found_coffee) << "コーヒー should be found";
  EXPECT_TRUE(found_nominagara) << "飲みながら should be found";
}

// =============================================================================
// Regression: Na-adjective + copula (幸いです)
// =============================================================================
// Bug: 幸いです was being parsed as 幸いで (VERB) + す (OTHER)
// Fix: Added 幸い to na_adjectives.h, added penalty for い-ending stems

TEST(AnalyzerTest, Regression_NaAdjective_SaiwaiDesu) {
  Suzume analyzer;
  auto result = analyzer.analyze("幸いです");
  ASSERT_GE(result.size(), 2) << "幸いです should split into 幸い + です";

  bool found_saiwai = false;
  bool found_desu = false;
  for (const auto& mor : result) {
    if (mor.surface == "幸い") {
      found_saiwai = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective)
          << "幸い should be Adjective";
    }
    if (mor.surface == "です") {
      found_desu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary)
          << "です should be Auxiliary";
    }
  }
  EXPECT_TRUE(found_saiwai) << "幸い should be found as separate token";
  EXPECT_TRUE(found_desu) << "です should be found as separate token";
}

TEST(AnalyzerTest, Regression_NaAdjective_BusinessEmail) {
  // Full business email pattern: ご返信いただけますと幸いです
  Suzume analyzer;
  auto result = analyzer.analyze("ご返信いただけますと幸いです");
  ASSERT_GE(result.size(), 4);

  bool found_saiwai = false;
  bool found_desu = false;
  for (const auto& mor : result) {
    if (mor.surface == "幸い") {
      found_saiwai = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Adjective);
    }
    if (mor.surface == "です") {
      found_desu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Auxiliary);
    }
  }
  EXPECT_TRUE(found_saiwai) << "幸い should be found";
  EXPECT_TRUE(found_desu) << "です should be found";
}

// =============================================================================
// Regression: Formal noun 付け separation
// =============================================================================
// Bug: 2024年12月23日付けで was being parsed with 付けで as VERB
// Fix: Added 付け to formal_nouns.h with is_formal_noun=true

TEST(AnalyzerTest, Regression_FormalNoun_TsukeSplit) {
  Suzume analyzer;
  auto result = analyzer.analyze("日付けで");
  ASSERT_GE(result.size(), 2) << "日付けで should split 付け from で";

  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle) {
      found_de = true;
    }
  }
  EXPECT_TRUE(found_de) << "で should be recognized as particle";
}

TEST(AnalyzerTest, Regression_FormalNoun_DateWithTsuke) {
  // Full date format: 2024年12月23日付けで
  Suzume analyzer;
  auto result = analyzer.analyze("2024年12月23日付けで");
  ASSERT_GE(result.size(), 2);

  bool found_tsuke = false;
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "付け") {
      found_tsuke = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Noun)
          << "付け should be Noun";
    }
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle) {
      found_de = true;
    }
  }
  EXPECT_TRUE(found_tsuke) << "付け should be found as separate token";
  EXPECT_TRUE(found_de) << "で should be found as particle";
}

// =============================================================================
// Regression: Ichidan te-form lemma (食べて)
// =============================================================================
// Bug: 食べて was being parsed as GodanBa (lemma 食ぶ)
// Fix: Removed overly broad e-row stem penalty in inflection

TEST(AnalyzerTest, Regression_Ichidan_TabeteCorrectLemma) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べて");
  ASSERT_EQ(result.size(), 1) << "食べて should be single token";

  EXPECT_EQ(result[0].surface, "食べて");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "食べて should be Verb";
  EXPECT_EQ(result[0].lemma, "食べる")
      << "食べて lemma should be 食べる (not 食ぶ)";
}

TEST(AnalyzerTest, Regression_Ichidan_TabetaCorrectLemma) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べた");
  ASSERT_EQ(result.size(), 1) << "食べた should be single token";

  EXPECT_EQ(result[0].surface, "食べた");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "食べた should be Verb";
  EXPECT_EQ(result[0].lemma, "食べる")
      << "食べた lemma should be 食べる";
}

TEST(AnalyzerTest, Regression_Ichidan_OshietemoraruCorrectLemma) {
  // Long compound Ichidan pattern - should not be parsed as Godan
  Suzume analyzer;
  auto result = analyzer.analyze("教えてもらった");
  ASSERT_GE(result.size(), 1);

  // The first token should be parsed correctly with Ichidan base
  bool found_correct = false;
  for (const auto& mor : result) {
    if (mor.surface.find("教え") == 0 && mor.pos == core::PartOfSpeech::Verb) {
      found_correct = true;
      // Should NOT be parsed as Godan (教う, 教ぶ, etc.)
      EXPECT_TRUE(mor.lemma.find("教え") == 0 || mor.lemma == "教える")
          << "教え... lemma should start with 教え, not " << mor.lemma;
      break;
    }
  }
  EXPECT_TRUE(found_correct) << "教え... verb should be found";
}

}  // namespace
}  // namespace suzume::analysis
