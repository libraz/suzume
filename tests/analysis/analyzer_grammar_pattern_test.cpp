// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Grammar tests for sentence patterns, conjunctions, counters, time nouns, etc.

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// ===== Onomatopoeia (擬音語・擬態語) Tests =====

TEST(AnalyzerTest, Onomatopoeia_WakuWaku) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("わくわくする");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Onomatopoeia_KiraKira) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("キラキラ光る");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Onomatopoeia_GataGata) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ガタガタ揺れる");
  ASSERT_FALSE(result.empty());
}

// ===== Counter Tests (助数詞) =====

TEST(AnalyzerTest, Counter_Nin) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("三人の学生");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Counter_Hon) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("二本のペン");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Counter_Ko) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("五個のリンゴ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Counter_Mai) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("十枚の紙");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Counter_Satsu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("三冊の本");
  ASSERT_FALSE(result.empty());
}

// ===== Conjunction Tests (接続詞) =====

TEST(AnalyzerTest, Conjunction_Shikashi) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("しかし問題がある");
  bool found_shikashi = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "しかし" &&
        morpheme.pos == core::PartOfSpeech::Conjunction) {
      found_shikashi = true;
      break;
    }
  }
  EXPECT_TRUE(found_shikashi);
}

TEST(AnalyzerTest, Conjunction_Sorede) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("それで帰った");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Conjunction_Demo) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("でも大丈夫");
  ASSERT_FALSE(result.empty());
}

// ===== Sentence Pattern Tests =====

TEST(AnalyzerTest, Pattern_NounNaAdjective) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("静かな部屋");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_IAdjective) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("高い山");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_TeForm) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べて寝る");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_ConditionalBa) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行けば分かる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_ConditionalTara) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行ったら教えて");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Pattern_ConditionalNara) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("君なら大丈夫");
  ASSERT_FALSE(result.empty());
}

// ===== NOUN + で Pattern Tests =====
// These patterns should split into NOUN + PARTICLE without dictionary

class NounDePatternTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};

  bool hasNoun(const std::vector<core::Morpheme>& result,
               const std::string& surface) {
    for (const auto& mor : result) {
      if (mor.surface == surface && mor.pos == core::PartOfSpeech::Noun) {
        return true;
      }
    }
    return false;
  }

  bool hasParticle(const std::vector<core::Morpheme>& result,
                   const std::string& surface) {
    for (const auto& mor : result) {
      if (mor.surface == surface && mor.pos == core::PartOfSpeech::Particle) {
        return true;
      }
    }
    return false;
  }
};

TEST_F(NounDePatternTest, Sokkoude) {
  // 速攻で (immediately)
  auto result = analyzer.analyze("速攻で");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "速攻")) << "速攻 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "で")) << "で should be PARTICLE";
}

TEST_F(NounDePatternTest, Byousokude) {
  // 秒速で (at lightning speed)
  auto result = analyzer.analyze("秒速で");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "秒速")) << "秒速 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "で")) << "で should be PARTICLE";
}

TEST_F(NounDePatternTest, Bakusokude) {
  // 爆速で (at explosive speed)
  auto result = analyzer.analyze("爆速で");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "爆速")) << "爆速 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "で")) << "で should be PARTICLE";
}

TEST_F(NounDePatternTest, Kousokude) {
  // 光速で (at the speed of light)
  auto result = analyzer.analyze("光速で");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "光速")) << "光速 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "で")) << "で should be PARTICLE";
}

TEST_F(NounDePatternTest, Kakuteide) {
  // 確定で (definitely)
  auto result = analyzer.analyze("確定で");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "確定")) << "確定 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "で")) << "で should be PARTICLE";
}

TEST_F(NounDePatternTest, Sokkoude_Katakana) {
  // ソッコーで (immediately - katakana)
  auto result = analyzer.analyze("ソッコーで");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "ソッコー")) << "ソッコー should be NOUN";
  EXPECT_TRUE(hasParticle(result, "で")) << "で should be PARTICLE";
}

// ===== Taru-Adjective + と Pattern Tests =====
// These taru-adjectives (タル形容動詞) split into NOUN + と without dictionary

TEST_F(NounDePatternTest, TaruAdj_Kizento) {
  // 毅然と (resolutely)
  auto result = analyzer.analyze("毅然と");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "毅然")) << "毅然 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "と")) << "と should be PARTICLE";
}

TEST_F(NounDePatternTest, TaruAdj_Heizento) {
  // 平然と (calmly)
  auto result = analyzer.analyze("平然と");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "平然")) << "平然 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "と")) << "と should be PARTICLE";
}

TEST_F(NounDePatternTest, TaruAdj_Taizento) {
  // 泰然と (composedly)
  auto result = analyzer.analyze("泰然と");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "泰然")) << "泰然 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "と")) << "と should be PARTICLE";
}

TEST_F(NounDePatternTest, TaruAdj_Sassouto) {
  // 颯爽と (gallantly)
  auto result = analyzer.analyze("颯爽と");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "颯爽")) << "颯爽 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "と")) << "と should be PARTICLE";
}

TEST_F(NounDePatternTest, TaruAdj_Hatsuratsuto) {
  // 溌剌と (vigorously)
  auto result = analyzer.analyze("溌剌と");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "溌剌")) << "溌剌 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "と")) << "と should be PARTICLE";
}

TEST_F(NounDePatternTest, TaruAdj_Yuuzento) {
  // 悠然と (leisurely)
  auto result = analyzer.analyze("悠然と");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "悠然")) << "悠然 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "と")) << "と should be PARTICLE";
}

// ===== NOUN + に Pattern Tests =====
// These patterns split into NOUN + に without dictionary

TEST_F(NounDePatternTest, NounNi_Saigoni) {
  // 最後に (finally)
  auto result = analyzer.analyze("最後に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "最後")) << "最後 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Saishoni) {
  // 最初に (first)
  auto result = analyzer.analyze("最初に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "最初")) << "最初 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Doujini) {
  // 同時に (simultaneously)
  auto result = analyzer.analyze("同時に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "同時")) << "同時 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Hantaini) {
  // 反対に (conversely)
  auto result = analyzer.analyze("反対に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "反対")) << "反対 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Ippanni) {
  // 一般に (generally)
  auto result = analyzer.analyze("一般に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "一般")) << "一般 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Shidaini) {
  // 次第に (gradually)
  auto result = analyzer.analyze("次第に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "次第")) << "次第 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Ikkini) {
  // 一気に (at once)
  auto result = analyzer.analyze("一気に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "一気")) << "一気 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Isseini) {
  // 一斉に (all at once)
  auto result = analyzer.analyze("一斉に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "一斉")) << "一斉 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Koini) {
  // 故意に (intentionally)
  auto result = analyzer.analyze("故意に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "故意")) << "故意 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NounDePatternTest, NounNi_Muishikini) {
  // 無意識に (unconsciously)
  auto result = analyzer.analyze("無意識に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasNoun(result, "無意識")) << "無意識 should be NOUN";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

// ===== Complex Sentence Tests =====

TEST(AnalyzerTest, ComplexSentence_RelativeClause) {
  // 昨日買った本を読んでいる (reading the book I bought yesterday)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("昨日買った本を読んでいる");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5);  // 昨日 + 買った + 本 + を + 読んでいる
  // Verify time noun segmentation
  bool found_kinou = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "昨日" && morpheme.pos == core::PartOfSpeech::Noun) {
      found_kinou = true;
      break;
    }
  }
  EXPECT_TRUE(found_kinou) << "昨日 should be recognized as separate noun";
}

TEST(AnalyzerTest, ComplexSentence_Embedded) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("彼が来ることを知っている");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, ComplexSentence_MultipleClauses) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("雨が降ったので、家にいた");
  ASSERT_FALSE(result.empty());
}

// ===== Time Noun Tests (時間名詞) =====

TEST(AnalyzerTest, TimeNoun_Kinou) {
  // 昨日 (yesterday)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("昨日");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].surface, "昨日");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
}

TEST(AnalyzerTest, TimeNoun_Ashita) {
  // 明日 (tomorrow)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("明日行く");
  bool found_ashita = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "明日" && morpheme.pos == core::PartOfSpeech::Noun) {
      found_ashita = true;
      break;
    }
  }
  EXPECT_TRUE(found_ashita) << "明日 should be recognized as noun";
}

TEST(AnalyzerTest, TimeNoun_Kyou) {
  // 今日 (today)
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("今日は暑い");
  ASSERT_GE(result.size(), 2);
  EXPECT_EQ(result[0].surface, "今日");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Noun);
}

// ===== Formal Noun Tests (形式名詞) =====

TEST(AnalyzerTest, FormalNoun_Koto) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("勉強すること");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, FormalNoun_Mono) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べるもの");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, FormalNoun_Tokoro) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べるところ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, FormalNoun_Wake) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("そういうわけ");
  ASSERT_FALSE(result.empty());
}

// ===== Loanword (外来語) Tests =====

TEST(AnalyzerTest, Loanword_Katakana) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("コンピューター");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Loanword_Mixed) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("インターネット接続");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Loanword_WithParticle) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("メールを送る");
  ASSERT_FALSE(result.empty());
  bool found_wo = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "を") {
      found_wo = true;
      break;
    }
  }
  EXPECT_TRUE(found_wo);
}

// ===== Abbreviation and Symbol Tests =====

TEST(AnalyzerTest, Abbreviation_JapaneseAbbrev) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("高校生");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Symbol_Parentheses) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("東京（とうきょう）");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Symbol_Brackets) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("「こんにちは」と言った");
  ASSERT_FALSE(result.empty());
}

// ===== Colloquial Expression Tests =====

TEST(AnalyzerTest, Colloquial_Tte) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行くって言った");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Colloquial_Jan) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("いいじゃん");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Colloquial_Cha) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行っちゃった");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Colloquial_Toku) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("やっとく");
  ASSERT_FALSE(result.empty());
}

// ===== Numeric Expression Tests =====

TEST(AnalyzerTest, Numeric_JapaneseNumbers) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("百二十三");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Numeric_MixedNumbers) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("3時間");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Numeric_OrdinalNumber) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("第一回");
  ASSERT_FALSE(result.empty());
}

// =============================================================================
// Auxiliary adjective patterns (~やすい, ~にくい)
// =============================================================================
// Moved from analyzer_regression_adjective_test.cpp
// Bug: 読みやすい in context was split as 読み (noun) + やすい (安い)
// Fix: Added connection cost penalty for やすい (安い) after verb renyokei-like
// nouns

TEST(AnalyzerTest, Regression_Yasui_YomiYasui_Context) {
  // この本は読みやすい - should be 読みやすい (easy to read), not 読み + 安い
  Suzume analyzer;
  auto result = analyzer.analyze("この本は読みやすい");
  ASSERT_GE(result.size(), 4);

  bool found_yomiyasui = false;
  for (const auto& mor : result) {
    if (mor.surface == "読みやすい" &&
        mor.pos == core::PartOfSpeech::Adjective) {
      found_yomiyasui = true;
    }
  }
  EXPECT_TRUE(found_yomiyasui)
      << "読みやすい should be single adjective (easy to read)";
}

TEST(AnalyzerTest, Regression_Yasui_Yasui_Standalone) {
  // この服は安い - should be 安い (cheap) as standalone adjective
  Suzume analyzer;
  auto result = analyzer.analyze("この服は安い");
  ASSERT_GE(result.size(), 4);

  bool found_yasui = false;
  for (const auto& mor : result) {
    if (mor.surface == "安い" && mor.lemma == "安い" &&
        mor.pos == core::PartOfSpeech::Adjective) {
      found_yasui = true;
    }
  }
  EXPECT_TRUE(found_yasui) << "安い should be recognized as cheap adjective";
}

// =============================================================================
// Conditional ~なければ patterns
// =============================================================================
// Moved from analyzer_regression_adjective_test.cpp
// Bug: 行かなければ was incorrectly analyzed as adjective (行かない + ければ)
// Fix: Added a-row hiragana to penalty check in inflection_scorer.cpp
//      and added penalty for short な-ending stems (しな, 来な)

TEST(AnalyzerTest, Regression_Nakereba_IkaNakereba_Verb) {
  // 行かなければ should be 行く (verb), not 行かない (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("行かなければ");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "行かなければ" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "行く") << "行かなければ lemma should be 行く";
    }
  }
  EXPECT_TRUE(found_verb) << "行かなければ should be recognized as verb";
}

TEST(AnalyzerTest, Regression_Nakereba_ShiNakereba_Verb) {
  // しなければ should be する (verb), not しない (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("しなければならない");
  ASSERT_FALSE(result.empty());

  bool found_suru = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb && mor.lemma == "する") {
      found_suru = true;
    }
  }
  EXPECT_TRUE(found_suru) << "しなければならない should contain する verb";
}

TEST(AnalyzerTest, Regression_Nakereba_KoNakereba_Verb) {
  // 来なければ should be 来る (verb), not 来ない (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("来なければ");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "来なければ" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "来る") << "来なければ lemma should be 来る";
    }
  }
  EXPECT_TRUE(found_verb) << "来なければ should be recognized as verb";
}

TEST(AnalyzerTest, Regression_Nakereba_KakaNakereba_Verb) {
  // 書かなければ should be 書く (verb), not 書かない (adjective)
  Suzume analyzer;
  auto result = analyzer.analyze("書かなければ");
  ASSERT_FALSE(result.empty());

  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface == "書かなければ" && mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      EXPECT_EQ(mor.lemma, "書く") << "書かなければ lemma should be 書く";
    }
  }
  EXPECT_TRUE(found_verb) << "書かなければ should be recognized as verb";
}

}  // namespace
}  // namespace suzume::analysis
