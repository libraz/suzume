// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Strict analyzer tests that verify exact tokenization results
// These tests expose issues found during TDD analysis

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "dictionary/user_dict.h"
#include "suzume.h"

namespace suzume::analysis {
namespace {

// Helper: Get surface forms as vector
std::vector<std::string> getSurfaces(const std::vector<core::Morpheme>& result) {
  std::vector<std::string> surfaces;
  surfaces.reserve(result.size());
  for (const auto& mor : result) {
    surfaces.push_back(mor.surface);
  }
  return surfaces;
}

// Helper: Check if result contains a surface with specific POS
[[maybe_unused]] bool hasSurfaceWithPOS(
    const std::vector<core::Morpheme>& result, const std::string& surface,
    core::PartOfSpeech pos) {
  for (const auto& mor : result) {
    if (mor.surface == surface && mor.pos == pos) {
      return true;
    }
  }
  return false;
}

// Helper: Check token count
[[maybe_unused]] size_t countTokens(
    const std::vector<core::Morpheme>& result) {
  return result.size();
}

// Base class for tests that need core dictionary
class AnalyzerTestBase : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};

  void SetUp() override {
    // Load core binary dictionary for L2 entries
    analyzer.tryAutoLoadCoreDictionary();
  }
};

// ===== Greeting Tests (挨拶) =====
// These common greetings should be recognized as single units

class GreetingStrictTest : public AnalyzerTestBase {};

TEST_F(GreetingStrictTest, Konnichiwa) {
  // こんにちは should be a single token, not こん/に/ち/は
  auto result = analyzer.analyze("こんにちは");
  auto surfaces = getSurfaces(result);

  // Current implementation splits it - this test should fail until fixed
  EXPECT_EQ(surfaces.size(), 1)
      << "こんにちは should be single token, got: "
      << surfaces.size() << " tokens";
  if (surfaces.size() == 1) {
    EXPECT_EQ(surfaces[0], "こんにちは");
  }
}

TEST_F(GreetingStrictTest, Ohayougozaimasu) {
  // おはようございます should not be split into お/は/よ/う...
  auto result = analyzer.analyze("おはようございます");
  auto surfaces = getSurfaces(result);

  // Should be 1-2 tokens: おはよう + ございます or おはようございます
  EXPECT_LE(surfaces.size(), 2)
      << "おはようございます should be at most 2 tokens, got: "
      << surfaces.size();
}

TEST_F(GreetingStrictTest, Arigatougozaimasu) {
  // ありがとうございます should not be split into あり/が/とう...
  auto result = analyzer.analyze("ありがとうございます");
  auto surfaces = getSurfaces(result);

  // Should be 1-2 tokens
  EXPECT_LE(surfaces.size(), 2)
      << "ありがとうございます should be at most 2 tokens, got: "
      << surfaces.size();
}

TEST_F(GreetingStrictTest, Sumimasen) {
  // すみません should be a single token with correct lemma
  auto result = analyzer.analyze("すみません");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "すみません should be single token";
  if (!result.empty()) {
    // Lemma should be すみません or すむ (済む) - not random
    EXPECT_TRUE(result[0].lemma == "すみません" || result[0].lemma == "すむ" ||
                result[0].lemma == "済む")
        << "すみません lemma incorrect: " << result[0].lemma;
  }
}

// ===== Honorific Prefix Tests (敬語接頭辞) =====
// お/ご + noun should form meaningful units

class HonorificPrefixTest : public AnalyzerTestBase {};

TEST_F(HonorificPrefixTest, Ocha) {
  // お茶 should be a single noun, not お + 茶
  auto result = analyzer.analyze("お茶");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "お茶 should be single token";
  if (surfaces.size() == 1) {
    EXPECT_EQ(surfaces[0], "お茶");
  }
}

TEST_F(HonorificPrefixTest, Gohan) {
  // ご飯 should be a single noun
  auto result = analyzer.analyze("ご飯");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "ご飯 should be single token";
  if (surfaces.size() == 1) {
    EXPECT_EQ(surfaces[0], "ご飯");
  }
}

TEST_F(HonorificPrefixTest, Onegai) {
  // お願い should be a single noun
  auto result = analyzer.analyze("お願い");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "お願い should be single token";
}

TEST_F(HonorificPrefixTest, Otsukareama) {
  // お疲れ様 should be a single unit or お疲れ + 様
  auto result = analyzer.analyze("お疲れ様");
  auto surfaces = getSurfaces(result);

  // At most 2 tokens
  EXPECT_LE(surfaces.size(), 2)
      << "お疲れ様 should be at most 2 tokens, got: " << surfaces.size();
}

TEST_F(HonorificPrefixTest, OsewaNiNatteOrimasu) {
  // お世話になっております - common business phrase
  auto result = analyzer.analyze("お世話になっております");
  auto surfaces = getSurfaces(result);

  // Should recognize お世話 as unit (not お + 世話)
  bool found_osewa = false;
  for (const auto& sur : surfaces) {
    if (sur == "お世話" || sur == "世話") {
      found_osewa = true;
      break;
    }
  }
  EXPECT_TRUE(found_osewa) << "Should contain お世話 or 世話";
}

// ===== Business Phrase Tests (ビジネス表現) =====

class BusinessPhraseStrictTest : public AnalyzerTestBase {};

TEST_F(BusinessPhraseStrictTest, YoroshikuOnegaiitashimasu) {
  // よろしくお願いいたします - should not be completely fragmented
  auto result = analyzer.analyze("よろしくお願いいたします");
  auto surfaces = getSurfaces(result);

  // Should contain recognizable units
  bool found_yoroshiku = false;
  bool found_onegai = false;
  for (const auto& sur : surfaces) {
    if (sur.find("よろしく") != std::string::npos) found_yoroshiku = true;
    if (sur.find("願") != std::string::npos) found_onegai = true;
  }
  EXPECT_TRUE(found_yoroshiku) << "Should contain よろしく";
  EXPECT_TRUE(found_onegai) << "Should contain お願い/願い";
}

TEST_F(BusinessPhraseStrictTest, Ikagadeshouka) {
  // いかがでしょうか - should not split い/か/が
  auto result = analyzer.analyze("いかがでしょうか");
  auto surfaces = getSurfaces(result);

  // いかが should be a single token
  bool found_ikaga = false;
  for (const auto& sur : surfaces) {
    if (sur == "いかが") {
      found_ikaga = true;
      break;
    }
  }
  EXPECT_TRUE(found_ikaga) << "Should contain いかが as single token";
}

// ===== Verb Conjugation Tests (動詞活用) =====

class VerbConjugationStrictTest : public AnalyzerTestBase {};

TEST_F(VerbConjugationStrictTest, TaberuLemma) {
  // 食べる should have lemma 食べる (not 食ぶ)
  auto result = analyzer.analyze("食べる");
  ASSERT_FALSE(result.empty());

  // Find the verb
  for (const auto& mor : result) {
    if (mor.surface.find("食べ") != std::string::npos ||
        mor.surface == "食べる") {
      EXPECT_EQ(mor.lemma, "食べる")
          << "食べる lemma should be 食べる, got: " << mor.lemma;
      break;
    }
  }
}

TEST_F(VerbConjugationStrictTest, GohanWoTaberu) {
  // ご飯を食べる - check both noun and verb
  auto result = analyzer.analyze("ご飯を食べる");
  auto surfaces = getSurfaces(result);

  // Should have reasonable tokenization
  EXPECT_GE(surfaces.size(), 2);
  EXPECT_LE(surfaces.size(), 4);

  // Check verb lemma
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb &&
        mor.surface.find("食べ") != std::string::npos) {
      EXPECT_NE(mor.lemma, "食ぶ")
          << "食べる lemma should not be 食ぶ";
    }
  }
}

// ===== Suffix Attachment Tests (接尾語) =====

class SuffixStrictTest : public AnalyzerTestBase {};

TEST_F(SuffixStrictTest, Tsuke_NotSplit) {
  // 付け should not be split into 付 + け
  auto result = analyzer.analyze("付けで");
  auto surfaces = getSurfaces(result);

  // け should not appear as standalone token
  bool has_standalone_ke = false;
  for (const auto& sur : surfaces) {
    if (sur == "け") {
      has_standalone_ke = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_ke)
      << "付け should not split into 付 + け";
}

TEST_F(SuffixStrictTest, HizukeDe) {
  // 日付けで - 付け should stay together
  auto result = analyzer.analyze("日付けで");
  auto surfaces = getSurfaces(result);

  bool has_standalone_ke = false;
  for (const auto& sur : surfaces) {
    if (sur == "け") {
      has_standalone_ke = true;
      break;
    }
  }
  EXPECT_FALSE(has_standalone_ke)
      << "日付けで: 付け should not split";
}

// ===== Interrogative Tests (疑問詞) =====

class InterrogativeStrictTest : public AnalyzerTestBase {};

TEST_F(InterrogativeStrictTest, Ikaga) {
  // いかが should be a single token
  auto result = analyzer.analyze("いかが");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "いかが should be single token";
  if (surfaces.size() == 1) {
    EXPECT_EQ(surfaces[0], "いかが");
  }
}

TEST_F(InterrogativeStrictTest, Doushite) {
  // どうして should be a single token
  auto result = analyzer.analyze("どうして");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "どうして should be single token";
}

// ===== Common Noun Tests =====

class CommonNounStrictTest : public AnalyzerTestBase {};

TEST_F(CommonNounStrictTest, Yoroshiku) {
  // よろしく should be a single token
  auto result = analyzer.analyze("よろしく");
  auto surfaces = getSurfaces(result);

  EXPECT_EQ(surfaces.size(), 1) << "よろしく should be single token";
}

// ===== Mixed Script Tests (英日混合) =====
// From edge_cases.md Section 1

class MixedScriptStrictTest : public AnalyzerTestBase {};

TEST_F(MixedScriptStrictTest, EnglishWithParticle_Wo) {
  // API + を should separate correctly
  auto result = analyzer.analyze("APIを呼ぶ");
  auto surfaces = getSurfaces(result);

  // Should have: api/API, を, 呼ぶ
  EXPECT_GE(surfaces.size(), 2) << "Should have at least 2 tokens";

  bool found_wo = false;
  for (const auto& sur : surfaces) {
    if (sur == "を") found_wo = true;
  }
  EXPECT_TRUE(found_wo) << "Should contain を particle";
}

TEST_F(MixedScriptStrictTest, CamelCasePreserved) {
  // CamelCase should not be split
  auto result = analyzer.analyze("getUserDataを呼ぶ");
  auto surfaces = getSurfaces(result);

  // Print debug info
  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  // getUserData or getuserdata should be kept together (not split at capitals)
  // Check that we have reasonable number of tokens
  ASSERT_GE(surfaces.size(), 3) << "Should have at least 3 tokens. " << debug_msg;

  // Check that を particle is present
  bool found_wo = false;
  for (const auto& sur : surfaces) {
    if (sur == "を") found_wo = true;
  }
  EXPECT_TRUE(found_wo) << "Should contain を particle. " << debug_msg;

  // Check that English function name is present as single token (case-insensitive)
  bool found_function = false;
  for (const auto& sur : surfaces) {
    // Check if it contains "userdata" (case insensitive check)
    std::string lower_sur = sur;
    for (auto& chr : lower_sur) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    if (lower_sur.find("userdata") != std::string::npos) {
      found_function = true;
      break;
    }
  }
  EXPECT_TRUE(found_function)
      << "Should contain userdata in some token. " << debug_msg;
}

TEST_F(MixedScriptStrictTest, DigitWithUnit) {
  // 3人 should be handled (may be single or two tokens)
  auto result = analyzer.analyze("3人で行く");
  auto surfaces = getSurfaces(result);

  // Should not split weirdly
  EXPECT_GE(surfaces.size(), 2);
  EXPECT_LE(surfaces.size(), 4);
}

// ===== Lemma Correctness Tests =====

class LemmaCorrectnessTest : public AnalyzerTestBase {};

TEST_F(LemmaCorrectnessTest, GohanLemma) {
  // ご飯 lemma should be ご飯
  auto result = analyzer.analyze("ご飯");
  ASSERT_FALSE(result.empty());

  for (const auto& mor : result) {
    if (mor.surface == "ご飯") {
      EXPECT_EQ(mor.lemma, "ご飯") << "ご飯 lemma should be ご飯";
      break;
    }
  }
}

TEST_F(LemmaCorrectnessTest, OchaLemma) {
  // お茶 lemma should be お茶
  auto result = analyzer.analyze("お茶");
  ASSERT_FALSE(result.empty());

  for (const auto& mor : result) {
    if (mor.surface == "お茶") {
      EXPECT_EQ(mor.lemma, "お茶") << "お茶 lemma should be お茶";
      break;
    }
  }
}

TEST_F(LemmaCorrectnessTest, KonnichiwaLemma) {
  // こんにちは lemma should be こんにちは
  auto result = analyzer.analyze("こんにちは");
  ASSERT_FALSE(result.empty());

  for (const auto& mor : result) {
    if (mor.surface == "こんにちは") {
      EXPECT_EQ(mor.lemma, "こんにちは") << "こんにちは lemma incorrect";
      break;
    }
  }
}

// ===== Real World Sentence Tests =====

class RealWorldSentenceTest : public AnalyzerTestBase {};

TEST_F(RealWorldSentenceTest, BusinessEmail) {
  // お世話になっております - common business phrase
  auto result = analyzer.analyze("お世話になっております");
  auto surfaces = getSurfaces(result);

  // Should be well-formed with recognizable tokens
  ASSERT_FALSE(result.empty());
  EXPECT_LE(surfaces.size(), 5) << "Should not over-fragment";
}

TEST_F(RealWorldSentenceTest, ShoppingConversation) {
  // これはいくらですか
  auto result = analyzer.analyze("これはいくらですか");
  auto surfaces = getSurfaces(result);

  bool found_ha = false;
  bool found_ka = false;
  for (const auto& mor : result) {
    if (mor.surface == "は" && mor.pos == core::PartOfSpeech::Particle) {
      found_ha = true;
    }
    if (mor.surface == "か" && mor.pos == core::PartOfSpeech::Particle) {
      found_ka = true;
    }
  }
  EXPECT_TRUE(found_ha) << "Should contain は particle";
  EXPECT_TRUE(found_ka) << "Should contain か particle";
}

TEST_F(RealWorldSentenceTest, WeatherTalk) {
  // 今日は暑いですね
  auto result = analyzer.analyze("今日は暑いですね");
  auto surfaces = getSurfaces(result);

  bool found_today = false;
  for (const auto& sur : surfaces) {
    if (sur == "今日") found_today = true;
  }
  EXPECT_TRUE(found_today) << "Should recognize 今日";
}

TEST_F(RealWorldSentenceTest, TechnicalDoc) {
  // ファイルが見つかりませんでした
  auto result = analyzer.analyze("ファイルが見つかりませんでした");
  auto surfaces = getSurfaces(result);

  bool found_ga = false;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      found_ga = true;
    }
  }
  EXPECT_TRUE(found_ga) << "Should recognize が particle";
}

// ===== Compound Particle Tests (複合助詞) =====
// From edge_cases.md Section 2.2

class CompoundParticleStrictTest : public AnalyzerTestBase {};

TEST_F(CompoundParticleStrictTest, Nitsuite) {
  // "日本について" → ["日本", "について"]
  auto result = analyzer.analyze("日本について");
  auto surfaces = getSurfaces(result);

  bool found_nitsuite = false;
  for (const auto& sur : surfaces) {
    if (sur == "について") {
      found_nitsuite = true;
      break;
    }
  }
  EXPECT_TRUE(found_nitsuite) << "Should recognize について as compound particle";
}

TEST_F(CompoundParticleStrictTest, Niyotte) {
  // "風によって" → ["風", "によって"]
  auto result = analyzer.analyze("風によって");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "によって") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize によって as compound particle";
}

TEST_F(CompoundParticleStrictTest, Toshite) {
  // "代表として" → contains "として"
  auto result = analyzer.analyze("代表として");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "として") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize として as compound particle";
}

TEST_F(CompoundParticleStrictTest, Nitaishite) {
  // "彼に対して" → contains "に対して"
  auto result = analyzer.analyze("彼に対して");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "に対して") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize に対して as compound particle";
}

// ===== Pronoun Tests (代名詞) =====
// From edge_cases.md Section 10

class PronounStrictTest : public AnalyzerTestBase {};

TEST_F(PronounStrictTest, Demonstrative_Kore) {
  // "これを見て" → ["これ", "を", "見て"]
  auto result = analyzer.analyze("これを見て");
  auto surfaces = getSurfaces(result);

  bool found_kore = false;
  bool found_wo = false;
  for (const auto& sur : surfaces) {
    if (sur == "これ") found_kore = true;
    if (sur == "を") found_wo = true;
  }
  EXPECT_TRUE(found_kore) << "Should recognize これ as pronoun";
  EXPECT_TRUE(found_wo) << "Should recognize を as particle";
}

TEST_F(PronounStrictTest, Demonstrative_Sore) {
  // "それは何ですか" → contains "それ"
  auto result = analyzer.analyze("それは何ですか");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "それ") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize それ as pronoun";
}

TEST_F(PronounStrictTest, Demonstrative_Are) {
  // "あれが欲しい" → contains "あれ"
  auto result = analyzer.analyze("あれが欲しい");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "あれ") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize あれ as pronoun";
}

TEST_F(PronounStrictTest, Interrogative_Doko) {
  // "どこに行く" → contains "どこ"
  auto result = analyzer.analyze("どこに行く");
  auto surfaces = getSurfaces(result);

  bool found = false;
  for (const auto& sur : surfaces) {
    if (sur == "どこ") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Should recognize どこ as interrogative pronoun";
}

// ===== Number + Counter Tests (数詞+助数詞) =====
// From edge_cases.md Section 3

class NumberCounterStrictTest : public AnalyzerTestBase {};

TEST_F(NumberCounterStrictTest, ThreePeople) {
  // "3人で行く" - should handle 3人 reasonably
  auto result = analyzer.analyze("3人で行く");
  auto surfaces = getSurfaces(result);

  // Should have reasonable tokenization
  EXPECT_GE(surfaces.size(), 2);
  EXPECT_LE(surfaces.size(), 5);

  // Should recognize で as particle
  bool found_de = false;
  for (const auto& mor : result) {
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle) {
      found_de = true;
    }
  }
  EXPECT_TRUE(found_de) << "Should recognize で particle";
}

TEST_F(NumberCounterStrictTest, HundredYen) {
  // "100円の商品" - reasonable tokenization
  auto result = analyzer.analyze("100円の商品");
  auto surfaces = getSurfaces(result);

  // Should recognize の as particle
  bool found_no = false;
  for (const auto& mor : result) {
    if (mor.surface == "の" && mor.pos == core::PartOfSpeech::Particle) {
      found_no = true;
    }
  }
  EXPECT_TRUE(found_no) << "Should recognize の particle";
}

// ===== Keigo Expression Tests (敬語表現) =====
// From edge_cases.md Section 2.1

class KeigoStrictTest : public AnalyzerTestBase {};

TEST_F(KeigoStrictTest, Gozaimasu) {
  // "ございます" should be recognized
  auto result = analyzer.analyze("ございます");
  auto surfaces = getSurfaces(result);

  EXPECT_LE(surfaces.size(), 2) << "ございます should not be over-fragmented";
}

TEST_F(KeigoStrictTest, OtsukareSama) {
  // "お疲れ様です" - business greeting
  auto result = analyzer.analyze("お疲れ様です");
  auto surfaces = getSurfaces(result);

  EXPECT_LE(surfaces.size(), 4) << "Should not over-fragment business greeting";
}

// ===== Contraction Tests (縮約形) =====
// From edge_cases.md Section 2.4

class ContractionStrictTest : public AnalyzerTestBase {};

TEST_F(ContractionStrictTest, Shiteru) {
  // "してる" (= している)
  auto result = analyzer.analyze("してる");
  auto surfaces = getSurfaces(result);

  // Should recognize as verb form
  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.pos == core::PartOfSpeech::Verb ||
        mor.pos == core::PartOfSpeech::Auxiliary) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "してる should contain verb component";
}

TEST_F(ContractionStrictTest, Miteta) {
  // "見てた" (= 見ていた)
  auto result = analyzer.analyze("見てた");
  auto surfaces = getSurfaces(result);

  // Should have verb
  bool found_verb = false;
  for (const auto& mor : result) {
    if (mor.surface.find("見") != std::string::npos ||
        mor.pos == core::PartOfSpeech::Verb) {
      found_verb = true;
      break;
    }
  }
  EXPECT_TRUE(found_verb) << "見てた should contain verb";
}

// ===== Sentence Ending Particle Tests (終助詞) =====
// From edge_cases.md Section 2.3

class SentenceEndingStrictTest : public AnalyzerTestBase {};

TEST_F(SentenceEndingStrictTest, Kana) {
  // "行くかな" - should recognize か and な
  auto result = analyzer.analyze("行くかな");
  auto surfaces = getSurfaces(result);

  // Should have final particles
  bool found_particle = false;
  for (const auto& mor : result) {
    if ((mor.surface == "か" || mor.surface == "な" || mor.surface == "かな") &&
        mor.pos == core::PartOfSpeech::Particle) {
      found_particle = true;
      break;
    }
  }
  EXPECT_TRUE(found_particle) << "Should recognize sentence-ending particle(s)";
}

TEST_F(SentenceEndingStrictTest, Yone) {
  // "いいよね" - should recognize よ and ね
  auto result = analyzer.analyze("いいよね");
  auto surfaces = getSurfaces(result);

  // よね or よ and ね should be recognized
  bool found_particle = false;
  for (const auto& mor : result) {
    if ((mor.surface == "よ" || mor.surface == "ね" || mor.surface == "よね") &&
        mor.pos == core::PartOfSpeech::Particle) {
      found_particle = true;
      break;
    }
  }
  EXPECT_TRUE(found_particle) << "Should recognize sentence-ending particle(s)";
}

// ===== Special Symbol Tests (特殊記号) =====
// From edge_cases.md Section 4

class SymbolStrictTest : public AnalyzerTestBase {};

TEST_F(SymbolStrictTest, Brackets) {
  // "AI（人工知能）" - brackets should be separate
  auto result = analyzer.analyze("AI（人工知能）");
  auto surfaces = getSurfaces(result);

  // Should contain AI and 人工知能
  bool found_ai = false;
  bool found_jinkou = false;
  for (const auto& sur : surfaces) {
    std::string lower_sur = sur;
    for (auto& chr : lower_sur) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    if (lower_sur == "ai") found_ai = true;
    if (sur.find("人工") != std::string::npos ||
        sur.find("知能") != std::string::npos) {
      found_jinkou = true;
    }
  }
  EXPECT_TRUE(found_ai) << "Should recognize AI";
  EXPECT_TRUE(found_jinkou) << "Should recognize 人工知能";
}

TEST_F(SymbolStrictTest, QuotationMarks) {
  // 「こんにちは」- Japanese quotes
  auto result = analyzer.analyze("「こんにちは」");
  auto surfaces = getSurfaces(result);

  bool found_greeting = false;
  for (const auto& sur : surfaces) {
    if (sur == "こんにちは") {
      found_greeting = true;
      break;
    }
  }
  EXPECT_TRUE(found_greeting) << "Should recognize こんにちは inside quotes";
}

// ===== Complex Mixed Expression Tests =====

class ComplexExpressionStrictTest : public AnalyzerTestBase {};

TEST_F(ComplexExpressionStrictTest, TechnicalWithEnglish) {
  // "Pythonで機械学習を実装する"
  auto result = analyzer.analyze("Pythonで機械学習を実装する");
  auto surfaces = getSurfaces(result);

  // Should recognize Python, で, を
  bool found_python = false;
  bool found_de = false;
  bool found_wo = false;
  for (const auto& mor : result) {
    std::string lower_sur = mor.surface;
    for (auto& chr : lower_sur) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    if (lower_sur == "python") found_python = true;
    if (mor.surface == "で" && mor.pos == core::PartOfSpeech::Particle)
      found_de = true;
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle)
      found_wo = true;
  }
  EXPECT_TRUE(found_python) << "Should recognize Python";
  EXPECT_TRUE(found_de) << "Should recognize で particle";
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
}

TEST_F(ComplexExpressionStrictTest, BusinessRequest) {
  // "ご確認をお願いいたします"
  auto result = analyzer.analyze("ご確認をお願いいたします");
  auto surfaces = getSurfaces(result);

  // Should not over-fragment
  EXPECT_LE(surfaces.size(), 6) << "Should not over-fragment business request";

  // Should recognize key components
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を particle";
}

// ===== Compound Verb Tests (複合動詞) =====
// These tests require compound verb dictionary (data/core/verbs.tsv)
// Expected: "呼び出す" → single verb token, not "呼" + "び" + "出す"

class CompoundVerbStrictTest : public AnalyzerTestBase {
 protected:
  void SetUp() override {
    // Call base SetUp to load core dictionary
    AnalyzerTestBase::SetUp();
    // Load compound verb dictionary
    auto dict = std::make_shared<dictionary::UserDictionary>();
    auto result = dict->loadFromFile("data/core/verbs.tsv");
    if (result.hasValue()) {
      analyzer.addUserDictionary(dict);
    }
  }
};

TEST_F(CompoundVerbStrictTest, Yobidasu) {
  // "呼び出す" should be recognized as a compound verb
  // Currently: 呼 + び + 出す (WRONG)
  // Expected: 呼び出す (single verb)
  auto result = analyzer.analyze("呼び出す");
  auto surfaces = getSurfaces(result);

  // Print debug info
  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  // The verb should be recognized as a single unit
  // び should NOT be a standalone token
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
  // "読み込む" should be recognized as compound verb
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
  // "書き出す" should be recognized as compound verb
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
  // "飛び込む" should be recognized as compound verb
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
  // "取り消す" should be recognized as compound verb
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
  // "引き続き" should be recognized as compound word
  auto result = analyzer.analyze("引き続き");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  // Should not have 4 separate tokens
  EXPECT_LT(surfaces.size(), 4)
      << "引き続き should not be fragmented into 4+ pieces. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, Kaimono) {
  // "買い物" should be recognized as compound noun
  auto result = analyzer.analyze("買い物");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  // Ideally single token, at most 2 (買い + 物)
  EXPECT_LE(surfaces.size(), 2)
      << "買い物 should be at most 2 tokens. " << debug_msg;
}

TEST_F(CompoundVerbStrictTest, CompoundVerbInSentence) {
  // "データを読み込む" - compound verb in context
  auto result = analyzer.analyze("データを読み込む");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  // Should recognize を particle
  bool found_wo = false;
  for (const auto& mor : result) {
    if (mor.surface == "を" && mor.pos == core::PartOfSpeech::Particle) {
      found_wo = true;
    }
  }
  EXPECT_TRUE(found_wo) << "Should recognize を particle. " << debug_msg;

  // み should not be standalone
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

// ===== Compound Noun Tests (複合名詞) =====

class CompoundNounStrictTest : public AnalyzerTestBase {};

TEST_F(CompoundNounStrictTest, Shizengengo) {
  // "自然言語処理" - technical term
  auto result = analyzer.analyze("自然言語処理");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  // Should be 1-3 tokens, not character-by-character
  EXPECT_LE(surfaces.size(), 3)
      << "自然言語処理 should be at most 3 tokens. " << debug_msg;
}

TEST_F(CompoundNounStrictTest, JinkouChinou) {
  // "人工知能" - AI term
  auto result = analyzer.analyze("人工知能");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  // Should be 1-2 tokens
  EXPECT_LE(surfaces.size(), 2)
      << "人工知能 should be at most 2 tokens. " << debug_msg;
}

TEST_F(CompoundNounStrictTest, KikaiGakushuu) {
  // "機械学習" - ML term
  auto result = analyzer.analyze("機械学習");
  auto surfaces = getSurfaces(result);

  std::string debug_msg = "Surfaces: ";
  for (const auto& sur : surfaces) {
    debug_msg += "[" + sur + "] ";
  }

  // Should be 1-2 tokens
  EXPECT_LE(surfaces.size(), 2)
      << "機械学習 should be at most 2 tokens. " << debug_msg;
}

}  // namespace
}  // namespace suzume::analysis
