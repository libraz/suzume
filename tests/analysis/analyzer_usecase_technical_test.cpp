
// Technical use case analyzer tests (documentation, programming, complex)

#include "analysis/analyzer.h"

#include <gtest/gtest.h>

#include "suzume.h"
#include "test_helpers.h"

namespace suzume::analysis {
namespace {

using suzume::test::hasParticle;
using suzume::test::hasSurface;

// ===== Technical Documentation Tests (技術文書) =====

class TechnicalDocTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(TechnicalDocTest, ErrorMessage) {
  // Error message in technical context
  auto result = analyzer.analyze("ファイルが見つかりませんでした");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(TechnicalDocTest, ProgrammingTerm) {
  // Programming terminology with Japanese
  auto result = analyzer.analyze("変数に値を代入する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(TechnicalDocTest, CodeReview) {
  // Code review comment
  auto result =
      analyzer.analyze("この関数は複雑すぎるので分割してください");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5) << "Should produce multiple tokens";
}

TEST_F(TechnicalDocTest, DocumentationSpec) {
  // Documentation specification style
  auto result = analyzer.analyze("戻り値は成功時に0を返す");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(TechnicalDocTest, APIDescription) {
  // API description
  auto result = analyzer.analyze("このAPIは認証が必要です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(TechnicalDocTest, InstallGuide) {
  // Installation guide
  auto result = analyzer.analyze("以下のコマンドを実行してください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(TechnicalDocTest, Troubleshooting) {
  // Troubleshooting
  auto result = analyzer.analyze("問題が解決しない場合は再起動してください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

TEST_F(TechnicalDocTest, Configuration) {
  // Configuration instruction
  auto result = analyzer.analyze("設定ファイルを編集する必要があります");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

// ===== Complex Real Sentences (複雑な実文) =====

class ComplexSentenceTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(ComplexSentenceTest, LostItem) {
  // Lost item description
  auto result = analyzer.analyze("昨日買ったばかりの本をなくしてしまった");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5);
  EXPECT_TRUE(hasSurface(result, "昨日")) << "Should recognize 昨日";
}

// Regression tests for time noun + verb split
// Issue: When hiragana sequence extends beyond verb ending (e.g., ばかり),
// the inflection analyzer wouldn't recognize the verb part, causing
// time nouns like 昨日 to be incorrectly merged with following verbs.
TEST_F(ComplexSentenceTest, TimeNounVerbSplit_Yesterday) {
  // 昨日 should be split from 買った even with trailing ばかり
  auto result = analyzer.analyze("昨日買ったばかり");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "昨日")) << "昨日 should be separate token";
  EXPECT_TRUE(hasSurface(result, "買った")) << "買った should be separate token";
}

TEST_F(ComplexSentenceTest, TimeNounVerbSplit_Today) {
  // 今日 should be split from 届いた
  auto result = analyzer.analyze("今日届いたばかりの荷物");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "今日")) << "今日 should be separate token";
}

TEST_F(ComplexSentenceTest, TimeNounVerbSplit_Tomorrow) {
  // 明日 should be split from 届く
  auto result = analyzer.analyze("明日届くはずです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "明日")) << "明日 should be separate token";
}

TEST_F(ComplexSentenceTest, LateForWork) {
  // Excuse for being late
  auto result = analyzer.analyze("電車が遅れているので遅刻しそうです");
  ASSERT_FALSE(result.empty());
  bool found_node = hasParticle(result, "ので") || hasSurface(result, "ので");
  EXPECT_TRUE(found_node) << "Should recognize ので";
}

TEST_F(ComplexSentenceTest, Cooking) {
  // Comment about cooking
  auto result = analyzer.analyze("彼女が作った料理はとても美味しかった");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "彼女")) << "Should recognize 彼女";
}

TEST_F(ComplexSentenceTest, StudyAbroad) {
  // Study abroad plan
  auto result = analyzer.analyze("来年から留学するつもりです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasSurface(result, "来年")) << "Should recognize 来年";
  EXPECT_TRUE(hasParticle(result, "から")) << "Should recognize から";
}

TEST_F(ComplexSentenceTest, Experience) {
  // Past experience
  auto result = analyzer.analyze("子供の頃によく遊んだ場所を訪れた");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(ComplexSentenceTest, Hearsay) {
  // Hearsay expression
  auto result = analyzer.analyze("彼は来月結婚するそうです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

// ===== Multi-clause Sentence Tests (複文) =====

class MultiClauseTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(MultiClauseTest, Conditional_Tara) {
  // Conditional sentence with たら
  auto result = analyzer.analyze("雨が降ったら、試合は中止になります");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(MultiClauseTest, Conditional_Ba) {
  // Conditional sentence with ば
  auto result = analyzer.analyze("時間があれば手伝います");
  ASSERT_FALSE(result.empty());
}

TEST_F(MultiClauseTest, Reason_Kara) {
  // Reason clause with から
  auto result = analyzer.analyze("疲れたから早く寝ます");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "から")) << "Should recognize から";
}

TEST_F(MultiClauseTest, Reason_Node) {
  // Reason clause with ので
  auto result = analyzer.analyze("忙しいので後で連絡します");
  ASSERT_FALSE(result.empty());
  bool found = hasParticle(result, "ので") || hasSurface(result, "ので");
  EXPECT_TRUE(found) << "Should recognize ので";
}

TEST_F(MultiClauseTest, Contrast_Kedo) {
  // Contrastive clause with けど
  auto result = analyzer.analyze("勉強したけど、試験に落ちた");
  ASSERT_FALSE(result.empty());
  bool found = hasParticle(result, "けど") || hasSurface(result, "けど");
  EXPECT_TRUE(found) << "Should recognize けど";
}

TEST_F(MultiClauseTest, Contrast_Ga) {
  // Contrastive clause with が
  auto result = analyzer.analyze("高いですが、品質は良いです");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(MultiClauseTest, While_Nagara) {
  // Simultaneous action with ながら
  auto result = analyzer.analyze("音楽を聴きながら勉強する");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(MultiClauseTest, Purpose_Tame) {
  // Purpose clause with ため
  auto result = analyzer.analyze("健康のために運動している");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
}

TEST_F(MultiClauseTest, Before_Mae) {
  // Before clause
  auto result = analyzer.analyze("寝る前に歯を磨く");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(MultiClauseTest, After_Ato) {
  // After clause
  auto result = analyzer.analyze("食事の後でコーヒーを飲む");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "の")) << "Should recognize の particle";
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

// ===== Quotation Tests (引用) =====

class QuotationTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(QuotationTest, DirectSpeech) {
  // Direct speech quotation
  auto result = analyzer.analyze("彼は「明日行く」と言った");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "と")) << "Should recognize と particle";
}

TEST_F(QuotationTest, IndirectSpeech) {
  // Indirect speech
  auto result = analyzer.analyze("彼女が来ないと思う");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
  EXPECT_TRUE(hasParticle(result, "と")) << "Should recognize と particle";
}

TEST_F(QuotationTest, Question_Kadouka) {
  // Embedded question
  auto result = analyzer.analyze("彼が来るかどうか分からない");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "が")) << "Should recognize が particle";
}

TEST_F(QuotationTest, Naming) {
  // Naming expression
  auto result = analyzer.analyze("これを「成功」と呼ぶ");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
  EXPECT_TRUE(hasParticle(result, "と")) << "Should recognize と particle";
}

// ===== Recipe/Cooking Tests (レシピ・料理) =====

class RecipeTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(RecipeTest, CookingInstruction) {
  // Cooking instruction
  auto result = analyzer.analyze("玉ねぎをみじん切りにする");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
}

TEST_F(RecipeTest, CookingTime) {
  // Cooking time instruction
  auto result = analyzer.analyze("中火で5分間炒める");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(RecipeTest, Seasoning) {
  // Seasoning instruction
  auto result = analyzer.analyze("塩と胡椒で味を調える");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "と")) << "Should recognize と particle";
  EXPECT_TRUE(hasParticle(result, "で")) << "Should recognize で particle";
}

TEST_F(RecipeTest, Ingredient) {
  // Ingredient list
  auto result = analyzer.analyze("材料は卵と牛乳と砂糖です");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
  EXPECT_TRUE(hasParticle(result, "と")) << "Should recognize と particle";
}

// ===== Medical/Health Tests (医療・健康) =====

class MedicalTest : public ::testing::Test {
 protected:
  AnalyzerOptions options;
  Analyzer analyzer{options};
};

TEST_F(MedicalTest, Symptom) {
  // Symptom description
  auto result = analyzer.analyze("頭が痛くて熱がある");
  ASSERT_FALSE(result.empty());
  int ga_count = 0;
  for (const auto& mor : result) {
    if (mor.surface == "が" && mor.pos == core::PartOfSpeech::Particle) {
      ga_count++;
    }
  }
  EXPECT_GE(ga_count, 1) << "Should recognize が particles";
}

TEST_F(MedicalTest, Prescription) {
  // Prescription instruction
  auto result = analyzer.analyze("食後に一錠を服用してください");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "に")) << "Should recognize に particle";
  EXPECT_TRUE(hasParticle(result, "を")) << "Should recognize を particle";
}

TEST_F(MedicalTest, Consultation) {
  // Medical consultation
  auto result =
      analyzer.analyze("症状が続くようでしたら医師に相談してください");
  ASSERT_FALSE(result.empty());
  EXPECT_GE(result.size(), 5) << "Should produce multiple tokens";
}

TEST_F(MedicalTest, Allergy) {
  // Allergy question
  auto result = analyzer.analyze("アレルギーはありますか");
  ASSERT_FALSE(result.empty());
  EXPECT_TRUE(hasParticle(result, "は")) << "Should recognize は particle";
}

}  // namespace
}  // namespace suzume::analysis
