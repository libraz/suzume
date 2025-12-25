// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Grammar-related analyzer tests (auxiliary verbs, keigo, counters, etc.)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

// ===== Auxiliary Verb Tests (助動詞) =====

TEST(AnalyzerTest, AuxiliaryVerb_Desu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("これは本です");
  bool found_desu = false;
  for (const auto& morpheme : result) {
    if (morpheme.surface == "です") {
      found_desu = true;
      break;
    }
  }
  EXPECT_TRUE(found_desu);
}

TEST(AnalyzerTest, AuxiliaryVerb_Masu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("食べます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, AuxiliaryVerb_Tai) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行きたい");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, AuxiliaryVerb_Nai) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("行かない");
  ASSERT_FALSE(result.empty());
}

// ===== Keigo (敬語) Tests =====

TEST(AnalyzerTest, Keigo_Irassharu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("先生がいらっしゃる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Gozaimasu) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ございます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Itadaku) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("いただきます");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_Kudasaru) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("教えてくださる");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_OPrefix) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("お忙しいところ");
  ASSERT_FALSE(result.empty());
}

TEST(AnalyzerTest, Keigo_GoPrefix) {
  AnalyzerOptions options;
  Analyzer analyzer(options);
  auto result = analyzer.analyze("ご確認ください");
  ASSERT_FALSE(result.empty());
}

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

// ===== Na-Adjective 〜的 Pattern Tests =====
// These test the dictionary-independent recognition of 〜的 as na-adjective

class NaAdjectiveTekiTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};

  bool hasAdjective(const std::vector<core::Morpheme>& result,
                    const std::string& surface) {
    for (const auto& mor : result) {
      if (mor.surface == surface && mor.pos == core::PartOfSpeech::Adjective) {
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

TEST_F(NaAdjectiveTekiTest, Basic_Riseitekini) {
  // 理性的に (rationally)
  auto result = analyzer.analyze("理性的に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "理性的")) << "理性的 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTekiTest, Basic_Ronritekini) {
  // 論理的に (logically)
  auto result = analyzer.analyze("論理的に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "論理的")) << "論理的 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTekiTest, Basic_Kouritsutekini) {
  // 効率的に (efficiently)
  auto result = analyzer.analyze("効率的に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "効率的")) << "効率的 should be ADJ";
}

TEST_F(NaAdjectiveTekiTest, Attributive_Ronritekina) {
  // 論理的な説明 (logical explanation)
  auto result = analyzer.analyze("論理的な説明");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "論理的")) << "論理的 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "な")) << "な should be PARTICLE";
}

TEST_F(NaAdjectiveTekiTest, Attributive_Kouritsutekina) {
  // 効率的な方法 (efficient method)
  auto result = analyzer.analyze("効率的な方法");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "効率的")) << "効率的 should be ADJ";
}

TEST_F(NaAdjectiveTekiTest, InSentence_Kangaeru) {
  // 理性的に考える (think rationally)
  auto result = analyzer.analyze("理性的に考える");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "理性的")) << "理性的 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTekiTest, InSentence_Naru) {
  // 感情的になる (become emotional)
  auto result = analyzer.analyze("感情的になる");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "感情的")) << "感情的 should be ADJ";
}

TEST_F(NaAdjectiveTekiTest, InSentence_Sagyousuru) {
  // 効率的に作業する (work efficiently)
  // 作業する is treated as a single suru-verb, so 3 tokens total
  auto result = analyzer.analyze("効率的に作業する");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "効率的")) << "効率的 should be ADJ";
}

TEST_F(NaAdjectiveTekiTest, Various_Sekkyokuteki) {
  // 積極的 (proactive)
  auto result = analyzer.analyze("積極的に参加する");
  EXPECT_TRUE(hasAdjective(result, "積極的")) << "積極的 should be ADJ";
}

TEST_F(NaAdjectiveTekiTest, Various_Gutaitekini) {
  // 具体的に (concretely)
  auto result = analyzer.analyze("具体的に説明する");
  EXPECT_TRUE(hasAdjective(result, "具体的")) << "具体的 should be ADJ";
}

TEST_F(NaAdjectiveTekiTest, Various_Kagakuteki) {
  // 科学的 (scientific)
  auto result = analyzer.analyze("科学的な根拠");
  EXPECT_TRUE(hasAdjective(result, "科学的")) << "科学的 should be ADJ";
}

// ===== Na-Adjective (L1 Hardcoded) Tests =====
// These test the na-adjectives from na_adjectives.h (L1 dictionary)
// that form adverbs with 〜に suffix

class NaAdjectiveTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};

  bool hasAdjective(const std::vector<core::Morpheme>& result,
                    const std::string& surface) {
    for (const auto& mor : result) {
      if (mor.surface == surface && mor.pos == core::PartOfSpeech::Adjective) {
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

  bool hasLemma(const std::vector<core::Morpheme>& result,
                const std::string& surface, const std::string& lemma) {
    for (const auto& mor : result) {
      if (mor.surface == surface && mor.lemma == lemma) {
        return true;
      }
    }
    return false;
  }
};

// Basic na-adjective + に (adverb form)
TEST_F(NaAdjectiveTest, Adverb_Teineini) {
  // 丁寧に (politely)
  auto result = analyzer.analyze("丁寧に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "丁寧")) << "丁寧 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Adverb_Shinchouni) {
  // 慎重に (carefully)
  auto result = analyzer.analyze("慎重に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "慎重")) << "慎重 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Adverb_Jouzuni) {
  // 上手に (skillfully)
  auto result = analyzer.analyze("上手に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "上手")) << "上手 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Adverb_Shizukani) {
  // 静かに (quietly)
  auto result = analyzer.analyze("静かに");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "静か")) << "静か should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Adverb_Kireini) {
  // 綺麗に (beautifully)
  auto result = analyzer.analyze("綺麗に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "綺麗")) << "綺麗 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Adverb_Kantanni) {
  // 簡単に (simply)
  auto result = analyzer.analyze("簡単に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "簡単")) << "簡単 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

// Na-adjective + な (attributive form)
TEST_F(NaAdjectiveTest, Attributive_Shizukana) {
  // 静かな部屋 (quiet room)
  auto result = analyzer.analyze("静かな部屋");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "静か")) << "静か should be ADJ";
  EXPECT_TRUE(hasParticle(result, "な")) << "な should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Attributive_Taisetsuna) {
  // 大切な人 (important person)
  auto result = analyzer.analyze("大切な人");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "大切")) << "大切 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "な")) << "な should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Attributive_Benrina) {
  // 便利な道具 (convenient tool)
  auto result = analyzer.analyze("便利な道具");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "便利")) << "便利 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "な")) << "な should be PARTICLE";
}

// Hiragana form recognition (auto-generated from reading)
TEST_F(NaAdjectiveTest, Hiragana_Shinchouni) {
  // しんちょうに (carefully - hiragana)
  auto result = analyzer.analyze("しんちょうに");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "しんちょう")) << "しんちょう should be ADJ";
  EXPECT_TRUE(hasLemma(result, "しんちょう", "慎重"))
      << "lemma should be 慎重";
}

TEST_F(NaAdjectiveTest, Hiragana_Teineini) {
  // ていねいに (politely - hiragana)
  auto result = analyzer.analyze("ていねいに");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "ていねい")) << "ていねい should be ADJ";
  EXPECT_TRUE(hasLemma(result, "ていねい", "丁寧")) << "lemma should be 丁寧";
}

TEST_F(NaAdjectiveTest, Hiragana_Shizukani) {
  // しずかに (quietly - hiragana)
  auto result = analyzer.analyze("しずかに");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "しずか")) << "しずか should be ADJ";
  EXPECT_TRUE(hasLemma(result, "しずか", "静か")) << "lemma should be 静か";
}

// In-sentence usage
TEST_F(NaAdjectiveTest, InSentence_ShizukaniHanasu) {
  // 静かに話す (speak quietly)
  auto result = analyzer.analyze("静かに話す");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "静か")) << "静か should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, InSentence_TeineiniSetsumei) {
  // 丁寧に説明する (explain politely)
  auto result = analyzer.analyze("丁寧に説明する");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "丁寧")) << "丁寧 should be ADJ";
}

TEST_F(NaAdjectiveTest, InSentence_JouzuniUtau) {
  // 上手に歌う (sing skillfully)
  auto result = analyzer.analyze("上手に歌う");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "上手")) << "上手 should be ADJ";
}

TEST_F(NaAdjectiveTest, InSentence_ShinchouniKangaeru) {
  // 慎重に考える (think carefully)
  auto result = analyzer.analyze("慎重に考える");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "慎重")) << "慎重 should be ADJ";
}

// Various na-adjectives from L1 dictionary
TEST_F(NaAdjectiveTest, Various_Hitsuyou) {
  // 必要な情報 (necessary information)
  auto result = analyzer.analyze("必要な情報");
  EXPECT_TRUE(hasAdjective(result, "必要")) << "必要 should be ADJ";
}

TEST_F(NaAdjectiveTest, Various_Tokubetsu) {
  // 特別に (specially)
  auto result = analyzer.analyze("特別に");
  EXPECT_TRUE(hasAdjective(result, "特別")) << "特別 should be ADJ";
}

TEST_F(NaAdjectiveTest, Various_Yuumei) {
  // 有名な人 (famous person)
  auto result = analyzer.analyze("有名な人");
  EXPECT_TRUE(hasAdjective(result, "有名")) << "有名 should be ADJ";
}

TEST_F(NaAdjectiveTest, Various_Jiyuu) {
  // 自由に (freely) - Note: 自由 not in L1, but 自然 is
  auto result = analyzer.analyze("自然に");
  EXPECT_TRUE(hasAdjective(result, "自然")) << "自然 should be ADJ";
}

TEST_F(NaAdjectiveTest, Various_Fukuzatsu) {
  // 複雑な問題 (complex problem)
  auto result = analyzer.analyze("複雑な問題");
  EXPECT_TRUE(hasAdjective(result, "複雑")) << "複雑 should be ADJ";
}

TEST_F(NaAdjectiveTest, Various_Juuyou) {
  // 重要な決定 (important decision)
  auto result = analyzer.analyze("重要な決定");
  EXPECT_TRUE(hasAdjective(result, "重要")) << "重要 should be ADJ";
}

// Additional NA_ADJ + に patterns (from extended L1 entries)
TEST_F(NaAdjectiveTest, Extended_Hijouni) {
  // 非常に (extremely)
  auto result = analyzer.analyze("非常に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "非常")) << "非常 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Hontouni) {
  // 本当に (really)
  auto result = analyzer.analyze("本当に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "本当")) << "本当 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Isshoni) {
  // 一緒に (together)
  auto result = analyzer.analyze("一緒に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "一緒")) << "一緒 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Bimyouni) {
  // 微妙に (subtly)
  auto result = analyzer.analyze("微妙に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "微妙")) << "微妙 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Hinpanni) {
  // 頻繁に (frequently)
  auto result = analyzer.analyze("頻繁に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "頻繁")) << "頻繁 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Kakujitsuni) {
  // 確実に (certainly)
  auto result = analyzer.analyze("確実に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "確実")) << "確実 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Murini) {
  // 無理に (forcibly)
  auto result = analyzer.analyze("無理に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "無理")) << "無理 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Eienni) {
  // 永遠に (eternally)
  auto result = analyzer.analyze("永遠に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "永遠")) << "永遠 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Mugenni) {
  // 無限に (infinitely)
  auto result = analyzer.analyze("無限に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "無限")) << "無限 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Extended_Mettani) {
  // 滅多に (rarely)
  auto result = analyzer.analyze("滅多に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "滅多")) << "滅多 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Sasuga_Ni) {
  // さすがに (as expected)
  auto result = analyzer.analyze("さすがに");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "さすが")) << "さすが should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Sasuga_Base) {
  // さすが (as expected - base form)
  auto result = analyzer.analyze("さすが");
  ASSERT_EQ(result.size(), 1);
  EXPECT_TRUE(hasAdjective(result, "さすが")) << "さすが should be ADJ";
}

TEST_F(NaAdjectiveTest, Sasuga_Na) {
  // さすがな (attributive form)
  auto result = analyzer.analyze("さすがな人");
  ASSERT_GE(result.size(), 3);
  EXPECT_TRUE(hasAdjective(result, "さすが")) << "さすが should be ADJ";
  EXPECT_TRUE(hasParticle(result, "な")) << "な should be PARTICLE";
}

TEST_F(NaAdjectiveTest, Sasuga_Kanji) {
  // 流石に (kanji form)
  auto result = analyzer.analyze("流石に");
  ASSERT_GE(result.size(), 2);
  EXPECT_TRUE(hasAdjective(result, "流石")) << "流石 should be ADJ";
  EXPECT_TRUE(hasParticle(result, "に")) << "に should be PARTICLE";
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

// ===== Suru Verb Tests (サ変動詞) =====
// These tests verify that Noun+する patterns are handled by the inflection
// analyzer without needing dictionary entries.

class SuruVerbTest : public ::testing::Test {
 protected:
  SuruVerbTest() : analyzer(options) {}

  bool hasVerb(const std::vector<core::Morpheme>& morphemes,
               std::string_view surface) {
    for (const auto& mor : morphemes) {
      if (mor.surface == surface && mor.pos == core::PartOfSpeech::Verb) {
        return true;
      }
    }
    return false;
  }

  AnalyzerOptions options;
  Analyzer analyzer;
};

TEST_F(SuruVerbTest, Basic_BenkyouSuru) {
  // 勉強する (to study)
  auto result = analyzer.analyze("勉強する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "勉強する")) << "勉強する should be VERB";
}

TEST_F(SuruVerbTest, Basic_BunsekiSuru) {
  // 分析する (to analyze)
  auto result = analyzer.analyze("分析する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "分析する")) << "分析する should be VERB";
}

TEST_F(SuruVerbTest, Basic_KakuninSuru) {
  // 確認する (to confirm)
  auto result = analyzer.analyze("確認する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "確認する")) << "確認する should be VERB";
}

TEST_F(SuruVerbTest, Basic_SetsumeiSuru) {
  // 説明する (to explain)
  auto result = analyzer.analyze("説明する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "説明する")) << "説明する should be VERB";
}

TEST_F(SuruVerbTest, Basic_ShoriSuru) {
  // 処理する (to process)
  auto result = analyzer.analyze("処理する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "処理する")) << "処理する should be VERB";
}

TEST_F(SuruVerbTest, Past_BenkyouShita) {
  // 勉強した (studied)
  auto result = analyzer.analyze("勉強した");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "勉強した")) << "勉強した should be VERB";
}

TEST_F(SuruVerbTest, Teiru_BenkyouShiteiru) {
  // 勉強している (is studying)
  auto result = analyzer.analyze("勉強している");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "勉強している")) << "勉強している should be VERB";
}

TEST_F(SuruVerbTest, Masu_KakuninShimasu) {
  // 確認します (will confirm - polite)
  auto result = analyzer.analyze("確認します");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "確認します")) << "確認します should be VERB";
}

TEST_F(SuruVerbTest, Nai_SetsumeiShinai) {
  // 説明しない (does not explain)
  auto result = analyzer.analyze("説明しない");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "説明しない")) << "説明しない should be VERB";
}

TEST_F(SuruVerbTest, Tai_RyokouShitai) {
  // 旅行したい (want to travel)
  auto result = analyzer.analyze("旅行したい");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "旅行したい")) << "旅行したい should be VERB";
}

TEST_F(SuruVerbTest, Passive_ShoriSareru) {
  // 処理される (is processed)
  auto result = analyzer.analyze("処理される");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "処理される")) << "処理される should be VERB";
}

TEST_F(SuruVerbTest, Causative_BenkyouSaseru) {
  // 勉強させる (make someone study)
  auto result = analyzer.analyze("勉強させる");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasVerb(result, "勉強させる")) << "勉強させる should be VERB";
}

}  // namespace
}  // namespace suzume::analysis
