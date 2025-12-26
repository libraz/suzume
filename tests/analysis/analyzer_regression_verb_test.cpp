// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Regression tests for verb recognition (conjugation, honorifics, te-form, etc.)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

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

// =============================================================================
// Regression: Te-form + いる separation
// =============================================================================
// Bug: 来ていません was parsed as 来てい(ADJ) + ません
// Fix: Added て/で to particle filter in generateAdjectiveCandidates

TEST(AnalyzerTest, Regression_TeIru_Kiteimasen) {
  Suzume analyzer;
  auto result = analyzer.analyze("来ていません");
  ASSERT_GE(result.size(), 2);

  // Should not contain 来てい as adjective
  bool found_kitei_adj = false;
  bool found_kite_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "来てい" && mor.pos == core::PartOfSpeech::Adjective) {
      found_kitei_adj = true;
    }
    if (mor.surface == "来て" && mor.pos == core::PartOfSpeech::Verb) {
      found_kite_verb = true;
    }
  }
  EXPECT_FALSE(found_kitei_adj) << "来てい should NOT be parsed as adjective";
  EXPECT_TRUE(found_kite_verb) << "来て should be parsed as verb";
}

TEST(AnalyzerTest, Regression_TeIru_Kiteimasu) {
  Suzume analyzer;
  auto result = analyzer.analyze("来ています");
  ASSERT_GE(result.size(), 1);

  // Accept either:
  // 1. Single token: 来ています → 来る (progressive as single unit)
  // 2. Split tokens: 来て + います → 来る + いる
  // Both are valid morphological analyses

  bool found_kiteimasu = false;  // Single token
  bool found_kite = false;       // Split: first part
  bool found_imasu = false;      // Split: second part
  for (const auto& mor : result) {
    if (mor.surface == "来ています" && mor.pos == core::PartOfSpeech::Verb &&
        mor.lemma == "来る") {
      found_kiteimasu = true;
    }
    if (mor.surface == "来て" && mor.pos == core::PartOfSpeech::Verb) {
      found_kite = true;
    }
    if (mor.surface == "います" && mor.pos == core::PartOfSpeech::Verb) {
      found_imasu = true;
    }
  }

  bool is_valid = found_kiteimasu || (found_kite && found_imasu);
  EXPECT_TRUE(is_valid) << "来ています should be parsed as verb(s) with lemma 来る";
}

// =============================================================================
// Regression: Suru verb renyokei (サ変動詞連用形)
// =============================================================================
// Bug: 分割し was parsed as NOUN + OTHER instead of VERB
// Fix: Added renyokei matching to inflection module

TEST(AnalyzerTest, Regression_SuruRenyokei_Bunkatsu) {
  // 分割し、結合する - 分割し should be recognized as verb
  Suzume analyzer;
  auto result = analyzer.analyze("分割し、結合する");
  ASSERT_GE(result.size(), 2);

  bool found_bunkatsu = false;
  for (const auto& mor : result) {
    if (mor.surface == "分割し") {
      found_bunkatsu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "分割し should be Verb";
      EXPECT_EQ(mor.lemma, "分割する")
          << "分割し lemma should be 分割する";
    }
  }
  EXPECT_TRUE(found_bunkatsu) << "分割し should be found";
}

TEST(AnalyzerTest, Regression_SuruRenyokei_InSentence) {
  // Full sentence with suru verb renyokei
  Suzume analyzer;
  auto result = analyzer.analyze("文章を単語に分割し、それぞれの品詞を特定する");
  ASSERT_GE(result.size(), 8);

  bool found_bunkatsu = false;
  for (const auto& mor : result) {
    if (mor.surface == "分割し") {
      found_bunkatsu = true;
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb)
          << "分割し should be Verb";
    }
  }
  EXPECT_TRUE(found_bunkatsu) << "分割し should be found in sentence";
}

// =============================================================================
// Regression: Suru verb te-form should not split
// =============================================================================
// Bug: 勉強して was split into 勉強し + て
// Fix: Skip suru renyokei candidate when followed by て/た

TEST(AnalyzerTest, Regression_SuruTeForm_NotSplit) {
  Suzume analyzer;
  auto result = analyzer.analyze("勉強して");
  ASSERT_EQ(result.size(), 1) << "勉強して should be single token";

  EXPECT_EQ(result[0].surface, "勉強して");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "勉強して should be Verb";
  EXPECT_EQ(result[0].lemma, "勉強する")
      << "勉強して lemma should be 勉強する";
}

TEST(AnalyzerTest, Regression_SuruTaForm_NotSplit) {
  Suzume analyzer;
  auto result = analyzer.analyze("勉強した");
  ASSERT_EQ(result.size(), 1) << "勉強した should be single token";

  EXPECT_EQ(result[0].surface, "勉強した");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "勉強した should be Verb";
  EXPECT_EQ(result[0].lemma, "勉強する")
      << "勉強した lemma should be 勉強する";
}

// =============================================================================
// Regression: いただく verb should not be parsed as i-adjective
// =============================================================================
// Bug: いただく was parsed as ADJ with lemma いただい
// Fix: Added いただく to hiragana_verbs.h as GodanKa verb

TEST(AnalyzerTest, Regression_ItadakuVerb) {
  Suzume analyzer;
  auto result = analyzer.analyze("いただく");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "いただく");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb)
      << "いただく should be Verb, not Adjective";
  EXPECT_EQ(result[0].lemma, "いただく");
}

TEST(AnalyzerTest, Regression_ItadakimasuVerb) {
  Suzume analyzer;
  auto result = analyzer.analyze("いただきます");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "いただきます");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "いただく");
}

// =============================================================================
// Regression: なる verb should not split as な + りました
// =============================================================================
// Bug: なりました was split as な (PARTICLE) + りました (VERB)
// Fix: Added なる to hiragana_verbs.h as GodanRa verb

TEST(AnalyzerTest, Regression_NaruVerb) {
  Suzume analyzer;
  auto result = analyzer.analyze("なりました");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "なりました");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "なる");
}

TEST(AnalyzerTest, Regression_YoiNiNaru) {
  Suzume analyzer;
  auto result = analyzer.analyze("容易になりました");
  ASSERT_GE(result.size(), 3);

  // Find なりました token
  bool found = false;
  for (const auto& mor : result) {
    if (mor.surface == "なりました") {
      EXPECT_EQ(mor.pos, core::PartOfSpeech::Verb);
      EXPECT_EQ(mor.lemma, "なる");
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "なりました should be single verb token";
}

// =============================================================================
// Regression: VERB + だ (copula) should be penalized
// =============================================================================
// Bug: 食べさせていただきます was split as 食べさせていた + だ + きます
// Fix: Added connection cost penalty for VERB → だ (copula)

TEST(AnalyzerTest, Regression_TeItadakimasu) {
  Suzume analyzer;
  auto result = analyzer.analyze("食べさせていただきます");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "食べさせていただきます");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "食べる");
}

TEST(AnalyzerTest, Regression_TaioSaseTeItadakimasu) {
  Suzume analyzer;
  auto result = analyzer.analyze("対応させていただきます");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "対応させていただきます");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "対応する");
}

// =============================================================================
// Regression: Suru verb passive polite form (されました)
// =============================================================================
// Bug: 開催されました was split as 開催さ (ADJ) + れました (VERB)
// Fix: Added されました pattern and empty suffix for suru mizenkei

TEST(AnalyzerTest, Regression_SuruPassivePolite) {
  Suzume analyzer;
  auto result = analyzer.analyze("開催されました");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "開催されました");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "開催する");
}

TEST(AnalyzerTest, Regression_SuruPassivePolite2) {
  Suzume analyzer;
  auto result = analyzer.analyze("勉強されました");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "勉強されました");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "勉強する");
}

// Ensure non-suru passives are not affected
TEST(AnalyzerTest, Regression_GodanPassiveNotAffected) {
  Suzume analyzer;
  auto result = analyzer.analyze("奪われた");
  ASSERT_EQ(result.size(), 1);

  EXPECT_EQ(result[0].surface, "奪われた");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "奪う");
}

// =============================================================================
// Regression: Te-form verb penalty skip
// =============================================================================
// Bug: 来て was penalized as NOUN+particle because て is a particle
// Fix: Skip penalty for te-form endings (て/で) in tokenizer

TEST(AnalyzerTest, Regression_TeFormNoPenalty_Kite) {
  // 来て should be a verb, not 来(NOUN) + て(PARTICLE)
  Suzume analyzer;
  auto result = analyzer.analyze("来て");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "来て");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
}

TEST(AnalyzerTest, Regression_TeFormNoPenalty_Tabete) {
  // 食べて should be a verb
  Suzume analyzer;
  auto result = analyzer.analyze("食べて");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "食べて");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "食べる");
}

TEST(AnalyzerTest, Regression_TeFormNoPenalty_Yonde) {
  // 読んで should be a verb (de-form)
  Suzume analyzer;
  auto result = analyzer.analyze("読んで");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "読んで");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "読む");
}

// =============================================================================
// Regression: Hiragana verb vs noun overlap
// =============================================================================
// Bug: います was penalized because いま(今) is in dictionary
// Fix: Skip penalty for pure hiragana verbs overlapping short dict entries

TEST(AnalyzerTest, Regression_HiraganaVerb_Imasu) {
  // います should be a verb, not いま(NOUN)+す
  Suzume analyzer;
  auto result = analyzer.analyze("います");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "います");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "いる");
}

TEST(AnalyzerTest, Regression_HiraganaVerb_Imasen) {
  // いません should be a verb
  Suzume analyzer;
  auto result = analyzer.analyze("いません");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "いません");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "いる");
}

// =============================================================================
// Regression: Suru passive negative past (されなかった)
// =============================================================================
// Bug: されなかった pattern was missing
// Fix: Added されなかった to aux patterns in inflection.cpp

TEST(AnalyzerTest, Regression_SuruPassiveNegativePast) {
  Suzume analyzer;
  auto result = analyzer.analyze("開催されなかった");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "開催されなかった");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "開催する");
}

TEST(AnalyzerTest, Regression_SuruPassiveNegativePast2) {
  Suzume analyzer;
  auto result = analyzer.analyze("勉強されなかった");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "勉強されなかった");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "勉強する");
}

// =============================================================================
// Regression: Compound verb + desiderative negative past (走り出したくなかった)
// =============================================================================
// Bug: 走り出したくなかった was split incorrectly as 走り出した + くなかった
//      and くなかった was being analyzed as verb form of くる (Ichidan)
// Fix: 1) Rejected Ichidan candidates with stems く/す/こ in inflection.cpp
//      2) Added subsidiary verb renyokei forms (出し, 込み, etc.) in join_candidates.cpp
//      3) Build compound verb base form (走り出す) for lemma

TEST(AnalyzerTest, Regression_CompoundVerbDesiderativeNegativePast) {
  Suzume analyzer;
  auto result = analyzer.analyze("走り出したくなかった");

  // Should be split as: 走り出し (compound verb) + たくなかった (desiderative)
  ASSERT_GE(result.size(), 2);

  // First token should be the compound verb with correct lemma
  EXPECT_EQ(result[0].surface, "走り出し");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "走り出す");

  // Second token should be desiderative auxiliary
  EXPECT_EQ(result[1].surface, "たくなかった");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[1].lemma, "たい");
}

// Ensure くなかった is NOT analyzed as くる verb form
TEST(AnalyzerTest, Regression_KuNakatta_NotKuruVerb) {
  Suzume analyzer;
  auto result = analyzer.analyze("くなかった");

  // くなかった should NOT have lemma くる
  for (const auto& mor : result) {
    EXPECT_NE(mor.lemma, "くる")
        << "くなかった should NOT be analyzed as くる verb form";
  }
}

}  // namespace
}  // namespace suzume::analysis
