
// Grammar tests for na-adjectives (〜的 pattern, L1 hardcoded na-adjectives)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"

namespace suzume::analysis {
namespace {

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

}  // namespace
}  // namespace suzume::analysis
