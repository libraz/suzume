// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Basic analyzer functionality tests

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::hasSurface;

TEST(AnalyzerTest, AnalyzeEmptyStringReturnsEmpty) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("");
  EXPECT_TRUE(result.empty());
}

TEST(AnalyzerTest, AnalyzeSimpleKanji) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("世界");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "世界");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
}

TEST(AnalyzerTest, AnalyzeWithParticle) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("私は");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "私");
  EXPECT_EQ(result[1].surface, "は");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
}

TEST(AnalyzerTest, AnalyzeHiragana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("こんにちは");
  ASSERT_FALSE(result.empty());
  // Entire hiragana string should be parsed as one or more morphemes
}

TEST(AnalyzerTest, AnalyzeMixedText) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("私は猫が好き");
  // Should have multiple morphemes
  ASSERT_GE(result.size(), 3);

  // Check for particles
  bool has_wa = false;
  bool has_ga = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "は") has_wa = true;
    if (morpheme.surface == "が") has_ga = true;
  }
  EXPECT_TRUE(has_wa);
  EXPECT_TRUE(has_ga);
}

TEST(AnalyzerTest, AnalyzeMultipleSentences) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今日は天気です");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, MorphemeHasCorrectLemma) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("は");
  ASSERT_EQ(result.size(), 1);
  EXPECT_FALSE(result[0].lemma.empty());
}

// ===== Edge Cases =====

TEST(AnalyzerTest, EdgeCase_OnlyPunctuation) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("。。。");
  // Should handle gracefully
}

TEST(AnalyzerTest, EdgeCase_MixedPunctuation) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("えっ！？本当に？");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, EdgeCase_RepeatedCharacter) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("あああああ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, EdgeCase_VeryLongWord) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("独立行政法人情報処理推進機構");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, EdgeCase_SingleKanji) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("空");
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(result[0].surface, "空");
}

TEST(AnalyzerTest, EdgeCase_SingleHiragana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("あ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, EdgeCase_SingleKatakana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ア");
  ASSERT_FALSE(result.empty());
}

// ===== Special Character Tests =====

TEST(AnalyzerTest, SpecialChar_LongVowelMark) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("コーヒー");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SpecialChar_SmallTsu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ちょっと待って");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SpecialChar_Kurikaeshi) {
  // 々 iteration mark
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("人々が集まる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, SpecialChar_OldKana) {
  // Old kana like ゑ, ゐ
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ゐる");
  ASSERT_FALSE(result.empty());
}

// =============================================================================
// Bug regression tests (without external dictionaries)
// =============================================================================

TEST(AnalyzerBugTest, DesuNe_ShouldNotBeSune) {
  // Bug: "ですね" was split as "で" + "すね" instead of "です" + "ね"
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("いいですね");

  EXPECT_TRUE(hasSurface(result, "です")) << "Should have 'です' as a token";
  EXPECT_TRUE(hasSurface(result, "ね")) << "Should have 'ね' as a token";
  EXPECT_FALSE(hasSurface(result, "すね")) << "Should NOT have 'すね' as a token";
}

TEST(AnalyzerBugTest, Totemo_ShouldBeOneAdverb) {
  // Bug: "とても" was split as "と" + "て" + "も"
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("とても面白い");

  EXPECT_TRUE(hasSurface(result, "とても")) << "Should have 'とても' as one token";
  EXPECT_FALSE(hasSurface(result, "とて")) << "Should NOT have 'とて' as a token";
}

TEST(AnalyzerBugTest, NagakattaDesu_AdjConjugation) {
  // Bug: "長かったです" was split as "長" + "か" + "った" + "です"
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("長かったです");

  // Either "長かった" as one token, or at least not "長" + "か" split
  bool has_nagakatta = hasSurface(result, "長かった");
  bool has_naga_alone = hasSurface(result, "長");
  bool has_ka_alone = hasSurface(result, "か");

  // "長かった" together is ideal
  // But "長" + "か" + "った" is wrong
  if (!has_nagakatta) {
    EXPECT_FALSE(has_naga_alone && has_ka_alone)
        << "Should not split '長かった' into '長' + 'か' + 'った'";
  }
}

TEST(AnalyzerBugTest, Shiteimasu_LemmaShouldNotBeShiru) {
  // Bug: "しています" had lemma "しる" instead of "する"
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("勉強しています");

  for (const auto& m : result) {
    std::string surface(m.surface);
    std::string lemma(m.lemma);
    // Any token containing "して" should not have lemma "しる"
    if (surface.find("して") != std::string::npos) {
      EXPECT_NE(lemma, "しる") << "Lemma for 'して*' should not be 'しる'";
    }
  }
}

TEST(AnalyzerBugTest, ManandeImasu_ShouldNotSplitManaN) {
  // Bug: "学んでいます" was split as "学" + "ん" + "で" + "います"
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("学んでいます");

  bool has_mana_alone = hasSurface(result, "学");
  bool has_n_alone = hasSurface(result, "ん");

  // Should NOT have "学" and "ん" as separate single-char tokens
  EXPECT_FALSE(has_mana_alone && has_n_alone)
      << "Should not split '学んで' into '学' + 'ん' + 'で'";
}

TEST(AnalyzerBugTest, Kamoshirenai_LemmaShouldNotBeMoshiru) {
  // Bug: "もしれない" had lemma "もしる"
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("雨が降るかもしれない");

  for (const auto& m : result) {
    std::string lemma(m.lemma);
    EXPECT_NE(lemma, "もしる") << "Lemma 'もしる' is incorrect";
  }
}

TEST(AnalyzerBugTest, RyoushuushoWo_ParticleShouldNotBeAbsorbed) {
  // Bug: "領収書を" was parsed as "領収書をく" (verb) instead of "領収書" + "を"
  // The particle を was being absorbed into a verb candidate
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("領収書をください");

  EXPECT_TRUE(hasSurface(result, "を")) << "Should have 'を' as a separate particle";
  EXPECT_FALSE(hasSurface(result, "領収書を")) << "Should NOT merge 領収書 with を";
  EXPECT_FALSE(hasSurface(result, "領収書をく")) << "Should NOT parse as verb 領収書をく";
}

TEST(AnalyzerBugTest, Dekiru_ShouldNotSplitAsDeKiru) {
  // Bug: "できる" was split as "で" + "きる" instead of single verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("できます");

  EXPECT_TRUE(hasSurface(result, "できます")) << "Should have 'できます' as one token";
  EXPECT_FALSE(hasSurface(result, "で")) << "Should NOT have 'で' as separate particle";
}

TEST(AnalyzerBugTest, Morau_ShouldNotSplitAsMoRau) {
  // Bug: "もらう" was split as "も" + "らう" instead of single verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("もらって");

  EXPECT_TRUE(hasSurface(result, "もらって")) << "Should have 'もらって' as one token";
  EXPECT_FALSE(hasSurface(result, "も")) << "Should NOT have 'も' as separate particle";
}

TEST(AnalyzerBugTest, Wakaru_ShouldNotSplitAsWaKaRu) {
  // Bug: "わかる" was split as "わ" + "か" + "る" instead of single verb
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("わからない");

  EXPECT_TRUE(hasSurface(result, "わからない")) << "Should have 'わからない' as one token";
  EXPECT_FALSE(hasSurface(result, "わ")) << "Should NOT have 'わ' as separate particle";
}

TEST(AnalyzerBugTest, Suru_ShiteShouldHaveLemmaSuru) {
  // Suru-verbs like "勉強する" are recognized as compound verbs
  // "勉強して" is the te-form with lemma "勉強する"
  // Using Suzume class which includes postprocessing for proper lemmatization
  Suzume suzume;
  auto result = suzume.analyze("勉強して");

  // Check for compound suru-verb recognition
  bool found_compound = false;
  for (const auto& m : result) {
    if (m.surface == "勉強して") {
      found_compound = true;
      EXPECT_EQ(std::string(m.lemma), "勉強する")
          << "Lemma for '勉強して' should be '勉強する'";
    }
  }
  EXPECT_TRUE(found_compound)
      << "Should recognize '勉強して' as compound suru-verb";
}

TEST(AnalyzerBugTest, Suru_ShitaShouldHaveLemmaSuru) {
  // Suru-verbs like "勉強する" are recognized as compound verbs
  // "勉強した" is the past tense with lemma "勉強する"
  // Using Suzume class which includes postprocessing for proper lemmatization
  Suzume suzume;
  auto result = suzume.analyze("勉強した");

  // Check for compound suru-verb recognition
  bool found_compound = false;
  for (const auto& m : result) {
    if (m.surface == "勉強した") {
      found_compound = true;
      EXPECT_EQ(std::string(m.lemma), "勉強する")
          << "Lemma for '勉強した' should be '勉強する'";
    }
  }
  EXPECT_TRUE(found_compound)
      << "Should recognize '勉強した' as compound suru-verb";
}

TEST(AnalyzerBugTest, Suru_ShinaiShouldHaveLemmaSuru) {
  // Bug: "しない" had lemma "しる" instead of "する"
  // Using Suzume class which includes postprocessing for proper lemmatization
  Suzume suzume;
  auto result = suzume.analyze("しない");

  ASSERT_EQ(result.size(), 1) << "Should have 1 token";
  EXPECT_EQ(result[0].surface, "しない");
  EXPECT_EQ(std::string(result[0].lemma), "する") << "Lemma for 'しない' should be 'する'";
}

TEST(AnalyzerBugTest, Itasu_ItashiteorimasuShouldHaveLemmaItasu) {
  // Bug: "いたしております" had lemma "いたしる" instead of "いたす"
  // いたす is a GodanSa verb (致す = humble form of する)
  Suzume suzume;
  auto result = suzume.analyze("いたしております");

  ASSERT_EQ(result.size(), 1) << "Should have 1 token";
  EXPECT_EQ(result[0].surface, "いたしております");
  EXPECT_EQ(std::string(result[0].lemma), "いたす")
      << "Lemma for 'いたしております' should be 'いたす'";
}

TEST(AnalyzerBugTest, Kentou_ShouldNotBeParsedAsAdjective) {
  // Bug: "検討いたしております" was split as "検討い" (ADJ) + "たしております"
  // 検討 is a サ変名詞, not an i-adjective stem
  Suzume suzume;
  auto result = suzume.analyze("検討いたしております");

  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";
  EXPECT_EQ(result[0].surface, "検討") << "First token should be '検討'";
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun)
      << "'検討' should be parsed as Noun";
  EXPECT_FALSE(hasSurface(result, "検討い"))
      << "Should NOT have '検討い' as a token";
}

TEST(AnalyzerBugTest, Ichidan_TaberuShouldNotBeParsedAsGodanPotential) {
  // Bug: "食べる" was parsed as GodanBa potential (食ぶ+る) instead of Ichidan
  // 食べる is an Ichidan verb, not a potential form of a fictional verb 食ぶ
  Suzume suzume;
  auto result = suzume.analyze("食べる");

  ASSERT_EQ(result.size(), 1) << "Should have 1 token";
  EXPECT_EQ(result[0].surface, "食べる");
  EXPECT_EQ(std::string(result[0].lemma), "食べる")
      << "Lemma for '食べる' should be '食べる', not '食ぶ'";
}

TEST(AnalyzerBugTest, Ichidan_MieruShouldNotBeParsedAsGodanPotential) {
  // Bug: "見える" was parsed as GodanWa potential instead of Ichidan
  Suzume suzume;
  auto result = suzume.analyze("見える");

  ASSERT_EQ(result.size(), 1) << "Should have 1 token";
  EXPECT_EQ(result[0].surface, "見える");
  EXPECT_EQ(std::string(result[0].lemma), "見える")
      << "Lemma for '見える' should be '見える', not '見う'";
}

TEST(AnalyzerBugTest, Ichidan_TsutaeruShouldNotBeParsedAsGodanPotential) {
  // "伝える" should be parsed as Ichidan, not GodanWa potential "伝う"
  Suzume suzume;
  auto result = suzume.analyze("伝える");

  ASSERT_EQ(result.size(), 1) << "Should have 1 token";
  EXPECT_EQ(result[0].surface, "伝える");
  EXPECT_EQ(std::string(result[0].lemma), "伝える")
      << "Lemma for '伝える' should be '伝える', not '伝う'";
}

TEST(AnalyzerBugTest, Ichidan_OshieruShouldNotBeParsedAsGodanPotential) {
  // "教える" should be parsed as Ichidan, not GodanWa potential "教う"
  Suzume suzume;
  auto result = suzume.analyze("教える");

  ASSERT_EQ(result.size(), 1) << "Should have 1 token";
  EXPECT_EQ(result[0].surface, "教える");
  EXPECT_EQ(std::string(result[0].lemma), "教える")
      << "Lemma for '教える' should be '教える', not '教う'";
}

TEST(AnalyzerBugTest, Ichidan_ConjugatedFormsShouldHaveCorrectLemma) {
  // Conjugated forms of ichidan verbs should have correct lemma
  Suzume suzume;

  // て-form
  auto result = suzume.analyze("食べて");
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(std::string(result[0].lemma), "食べる")
      << "Lemma for '食べて' should be '食べる'";

  // た-form
  result = suzume.analyze("食べた");
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(std::string(result[0].lemma), "食べる")
      << "Lemma for '食べた' should be '食べる'";

  // ない-form
  result = suzume.analyze("食べない");
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(std::string(result[0].lemma), "食べる")
      << "Lemma for '食べない' should be '食べる'";
}

TEST(AnalyzerBugTest, GodanSa_HiraganaVerbStemsShouldWork) {
  // GodanSa verbs with hiragana stems (いたす, etc.) should work correctly
  Suzume suzume;

  // いたす conjugations
  auto result = suzume.analyze("いたします");
  ASSERT_FALSE(result.empty());
  bool found_itasu = false;
  for (const auto& m : result) {
    if (m.surface == "いたします" && std::string(m.lemma) == "いたす") {
      found_itasu = true;
      break;
    }
  }
  EXPECT_TRUE(found_itasu) << "Should find 'いたします' with lemma 'いたす'";
}

TEST(AnalyzerBugTest, SuruNoun_ShouldNotBeParsedAsIAdjective) {
  // サ変名詞 (like 勉強, 検討) should not be parsed as i-adjective stems
  Suzume suzume;

  // 勉強いたします should be 勉強 + いたします, not 勉強い (ADJ) + たします
  auto result = suzume.analyze("勉強いたします");
  ASSERT_GE(result.size(), 2) << "Should have at least 2 tokens";
  EXPECT_EQ(result[0].surface, "勉強") << "First token should be '勉強'";
  EXPECT_FALSE(hasSurface(result, "勉強い"))
      << "Should NOT have '勉強い' as a token";
}

TEST(AnalyzerBugTest, SuruNoun_ShouldNotBeParsedAsGodanVerb) {
  // サ変名詞 should not be parsed as Godan verb stems
  // 検討いた should NOT be parsed as GodanKa verb 検討く
  Suzume suzume;

  auto result = suzume.analyze("検討した");
  ASSERT_FALSE(result.empty());

  // Should be recognized as compound suru-verb
  bool found_suru_compound = false;
  for (const auto& m : result) {
    if (m.surface == "検討した" && std::string(m.lemma) == "検討する") {
      found_suru_compound = true;
      break;
    }
  }
  EXPECT_TRUE(found_suru_compound)
      << "Should recognize '検討した' as compound suru-verb with lemma '検討する'";
}

// ===== Pronoun Tests =====

TEST(AnalyzerPronounTest, HiraganaPronoun_Anata) {
  Suzume suzume;
  auto result = suzume.analyze("あなた");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "あなた");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Pronoun);
}

TEST(AnalyzerPronounTest, PluralPronoun_Watashitachi) {
  Suzume suzume;
  auto result = suzume.analyze("私たち");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "私たち");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Pronoun);
}

TEST(AnalyzerPronounTest, PluralPronoun_Bokutachi) {
  Suzume suzume;
  auto result = suzume.analyze("僕たち");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "僕たち");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Pronoun);
}

TEST(AnalyzerPronounTest, PluralPronoun_Oretachi) {
  Suzume suzume;
  auto result = suzume.analyze("俺たち");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "俺たち");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Pronoun);
}

TEST(AnalyzerPronounTest, CollectivePronoun_Minasan) {
  Suzume suzume;
  auto result = suzume.analyze("皆さん");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "皆さん");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Pronoun);
}

TEST(AnalyzerPronounTest, CollectivePronoun_Minna) {
  Suzume suzume;
  auto result = suzume.analyze("みんな");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "みんな");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Pronoun);
}

// ===== Na-Adjective Tests =====

TEST(AnalyzerNaAdjectiveTest, Arata) {
  Suzume suzume;
  auto result = suzume.analyze("新たな");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].surface, "新た");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(result[1].surface, "な");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Particle);
}

TEST(AnalyzerNaAdjectiveTest, Daisuki) {
  Suzume suzume;
  auto result = suzume.analyze("大好き");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "大好き");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
}

TEST(AnalyzerNaAdjectiveTest, Daikirai) {
  Suzume suzume;
  auto result = suzume.analyze("大嫌い");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "大嫌い");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Adjective);
}

TEST(AnalyzerNaAdjectiveTest, DaisukiInSentence) {
  Suzume suzume;
  auto result = suzume.analyze("あなたが大好きです");
  ASSERT_GE(result.size(), 4);
  EXPECT_TRUE(hasSurface(result, "あなた"));
  EXPECT_TRUE(hasSurface(result, "大好き"));
}

// ===== Hiragana Verb やる Tests =====

TEST(AnalyzerHiraganaVerbTest, Yaru_BasicForm) {
  Suzume suzume;
  auto result = suzume.analyze("やる");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "やる");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(std::string(result[0].lemma), "やる");
}

TEST(AnalyzerHiraganaVerbTest, Yaru_PastForm) {
  Suzume suzume;
  auto result = suzume.analyze("やった");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "やった");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(std::string(result[0].lemma), "やる");
}

TEST(AnalyzerHiraganaVerbTest, Yaru_TeForm) {
  Suzume suzume;
  auto result = suzume.analyze("やって");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "やって");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(std::string(result[0].lemma), "やる");
}

TEST(AnalyzerHiraganaVerbTest, Yaru_CausativePassive) {
  // やらされた = causative-passive past of やる
  Suzume suzume;
  auto result = suzume.analyze("やらされた");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "やらされた");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(std::string(result[0].lemma), "やる");
}

TEST(AnalyzerHiraganaVerbTest, Yaru_InSentence) {
  Suzume suzume;
  auto result = suzume.analyze("仕事をやらされた");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasSurface(result, "仕事"));
  EXPECT_TRUE(hasSurface(result, "を"));
  EXPECT_TRUE(hasSurface(result, "やらされた"));
}

}  // namespace
}  // namespace suzume::analysis
