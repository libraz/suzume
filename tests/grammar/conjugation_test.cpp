#include "grammar/conjugation.h"

#include <gtest/gtest.h>

namespace suzume {
namespace grammar {
namespace {

class ConjugationTest : public ::testing::Test {
 protected:
  Conjugation conjugator_;
};

// ============================================================================
// getStem tests
// ============================================================================

TEST_F(ConjugationTest, GetStemIchidan) {
  EXPECT_EQ(conjugator_.getStem("食べる", VerbType::Ichidan), "食べ");
  EXPECT_EQ(conjugator_.getStem("見る", VerbType::Ichidan), "見");
  EXPECT_EQ(conjugator_.getStem("起きる", VerbType::Ichidan), "起き");
}

TEST_F(ConjugationTest, GetStemGodan) {
  EXPECT_EQ(conjugator_.getStem("書く", VerbType::GodanKa), "書");
  EXPECT_EQ(conjugator_.getStem("読む", VerbType::GodanMa), "読");
  EXPECT_EQ(conjugator_.getStem("話す", VerbType::GodanSa), "話");
  EXPECT_EQ(conjugator_.getStem("買う", VerbType::GodanWa), "買");
  EXPECT_EQ(conjugator_.getStem("走る", VerbType::GodanRa), "走");
  EXPECT_EQ(conjugator_.getStem("泳ぐ", VerbType::GodanGa), "泳");
  EXPECT_EQ(conjugator_.getStem("立つ", VerbType::GodanTa), "立");
  EXPECT_EQ(conjugator_.getStem("死ぬ", VerbType::GodanNa), "死");
  EXPECT_EQ(conjugator_.getStem("遊ぶ", VerbType::GodanBa), "遊");
}

TEST_F(ConjugationTest, GetStemSuru) {
  EXPECT_EQ(conjugator_.getStem("する", VerbType::Suru), "");
  EXPECT_EQ(conjugator_.getStem("勉強する", VerbType::Suru), "勉強");
  EXPECT_EQ(conjugator_.getStem("運動する", VerbType::Suru), "運動");
}

TEST_F(ConjugationTest, GetStemKuru) {
  EXPECT_EQ(conjugator_.getStem("来る", VerbType::Kuru), "来");
}

TEST_F(ConjugationTest, GetStemIAdjective) {
  EXPECT_EQ(conjugator_.getStem("高い", VerbType::IAdjective), "高");
  EXPECT_EQ(conjugator_.getStem("美しい", VerbType::IAdjective), "美し");
}

TEST_F(ConjugationTest, GetStemEmpty) {
  EXPECT_EQ(conjugator_.getStem("", VerbType::Ichidan), "");
}

TEST_F(ConjugationTest, GetStemTooShort) {
  EXPECT_EQ(conjugator_.getStem("a", VerbType::Ichidan), "a");
}

TEST_F(ConjugationTest, GetStemUnknown) {
  EXPECT_EQ(conjugator_.getStem("テスト", VerbType::Unknown), "テス");
}

// ============================================================================
// detectType tests
// ============================================================================

TEST_F(ConjugationTest, DetectTypeSuru) {
  EXPECT_EQ(conjugator_.detectType("する"), VerbType::Suru);
  EXPECT_EQ(conjugator_.detectType("勉強する"), VerbType::Suru);
}

TEST_F(ConjugationTest, DetectTypeKuru) {
  EXPECT_EQ(conjugator_.detectType("来る"), VerbType::Kuru);
  EXPECT_EQ(conjugator_.detectType("くる"), VerbType::Kuru);
}

TEST_F(ConjugationTest, DetectTypeIAdjective) {
  EXPECT_EQ(conjugator_.detectType("高い"), VerbType::IAdjective);
  EXPECT_EQ(conjugator_.detectType("美しい"), VerbType::IAdjective);
}

TEST_F(ConjugationTest, DetectTypeIchidan) {
  // え段、い段 (hiragana) + る → 一段
  EXPECT_EQ(conjugator_.detectType("食べる"), VerbType::Ichidan);
  // Note: 見る (kanji + る) is detected as GodanRa by heuristic
  // because the prev char is kanji "見" not hiragana "み"
  EXPECT_EQ(conjugator_.detectType("見る"), VerbType::GodanRa);
  EXPECT_EQ(conjugator_.detectType("起きる"), VerbType::Ichidan);
}

TEST_F(ConjugationTest, DetectTypeGodanRa) {
  // 五段ラ行（例外的に一段でない）
  EXPECT_EQ(conjugator_.detectType("走る"), VerbType::GodanRa);
}

TEST_F(ConjugationTest, DetectTypeGodanKa) {
  EXPECT_EQ(conjugator_.detectType("書く"), VerbType::GodanKa);
}

TEST_F(ConjugationTest, DetectTypeGodanGa) {
  EXPECT_EQ(conjugator_.detectType("泳ぐ"), VerbType::GodanGa);
}

TEST_F(ConjugationTest, DetectTypeGodanSa) {
  EXPECT_EQ(conjugator_.detectType("話す"), VerbType::GodanSa);
}

TEST_F(ConjugationTest, DetectTypeGodanTa) {
  EXPECT_EQ(conjugator_.detectType("立つ"), VerbType::GodanTa);
}

TEST_F(ConjugationTest, DetectTypeGodanNa) {
  EXPECT_EQ(conjugator_.detectType("死ぬ"), VerbType::GodanNa);
}

TEST_F(ConjugationTest, DetectTypeGodanBa) {
  EXPECT_EQ(conjugator_.detectType("遊ぶ"), VerbType::GodanBa);
}

TEST_F(ConjugationTest, DetectTypeGodanMa) {
  EXPECT_EQ(conjugator_.detectType("読む"), VerbType::GodanMa);
}

TEST_F(ConjugationTest, DetectTypeGodanWa) {
  EXPECT_EQ(conjugator_.detectType("買う"), VerbType::GodanWa);
}

TEST_F(ConjugationTest, DetectTypeEmpty) {
  EXPECT_EQ(conjugator_.detectType(""), VerbType::Unknown);
}

TEST_F(ConjugationTest, DetectTypeTooShort) {
  EXPECT_EQ(conjugator_.detectType("a"), VerbType::Unknown);
}

// ============================================================================
// generate tests - Ichidan
// ============================================================================

TEST_F(ConjugationTest, GenerateIchidan) {
  auto forms = conjugator_.generate("食べる", VerbType::Ichidan);
  EXPECT_GT(forms.size(), 0);

  // Check some expected forms
  bool found_base = false;
  bool found_nai = false;
  bool found_ta = false;
  bool found_te = false;

  for (const auto& form : forms) {
    if (form.surface == "食べる") found_base = true;
    if (form.surface == "食べない") found_nai = true;
    if (form.surface == "食べた") found_ta = true;
    if (form.surface == "食べて") found_te = true;
  }

  EXPECT_TRUE(found_base);
  EXPECT_TRUE(found_nai);
  EXPECT_TRUE(found_ta);
  EXPECT_TRUE(found_te);
}

// ============================================================================
// generate tests - Godan
// ============================================================================

TEST_F(ConjugationTest, GenerateGodanKa) {
  auto forms = conjugator_.generate("書く", VerbType::GodanKa);
  EXPECT_GT(forms.size(), 0);

  bool found_base = false;
  bool found_nai = false;
  bool found_ta = false;  // 書いた (音便)

  for (const auto& form : forms) {
    if (form.surface == "書く") found_base = true;
    if (form.surface == "書かない") found_nai = true;
    if (form.surface == "書いた") found_ta = true;
  }

  EXPECT_TRUE(found_base);
  EXPECT_TRUE(found_nai);
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateGodanGa) {
  auto forms = conjugator_.generate("泳ぐ", VerbType::GodanGa);

  bool found_ta = false;  // 泳いだ (濁音)
  for (const auto& form : forms) {
    if (form.surface == "泳いだ") found_ta = true;
  }
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateGodanSa) {
  auto forms = conjugator_.generate("話す", VerbType::GodanSa);

  bool found_ta = false;  // 話した (音便なし)
  for (const auto& form : forms) {
    if (form.surface == "話した") found_ta = true;
  }
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateGodanTa) {
  auto forms = conjugator_.generate("立つ", VerbType::GodanTa);

  bool found_ta = false;  // 立った (促音便)
  for (const auto& form : forms) {
    if (form.surface == "立った") found_ta = true;
  }
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateGodanNa) {
  auto forms = conjugator_.generate("死ぬ", VerbType::GodanNa);

  bool found_ta = false;  // 死んだ (撥音便 + 濁音)
  for (const auto& form : forms) {
    if (form.surface == "死んだ") found_ta = true;
  }
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateGodanBa) {
  auto forms = conjugator_.generate("遊ぶ", VerbType::GodanBa);

  bool found_ta = false;  // 遊んだ (撥音便 + 濁音)
  for (const auto& form : forms) {
    if (form.surface == "遊んだ") found_ta = true;
  }
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateGodanMa) {
  auto forms = conjugator_.generate("読む", VerbType::GodanMa);

  bool found_ta = false;  // 読んだ (撥音便 + 濁音)
  for (const auto& form : forms) {
    if (form.surface == "読んだ") found_ta = true;
  }
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateGodanRa) {
  auto forms = conjugator_.generate("走る", VerbType::GodanRa);

  bool found_ta = false;  // 走った (促音便)
  for (const auto& form : forms) {
    if (form.surface == "走った") found_ta = true;
  }
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateGodanWa) {
  auto forms = conjugator_.generate("買う", VerbType::GodanWa);

  bool found_ta = false;  // 買った (促音便)
  bool found_nai = false;  // 買わない
  for (const auto& form : forms) {
    if (form.surface == "買った") found_ta = true;
    if (form.surface == "買わない") found_nai = true;
  }
  EXPECT_TRUE(found_ta);
  EXPECT_TRUE(found_nai);
}

// ============================================================================
// generate tests - Suru/Kuru
// ============================================================================

TEST_F(ConjugationTest, GenerateSuru) {
  auto forms = conjugator_.generate("する", VerbType::Suru);

  bool found_base = false;
  bool found_nai = false;
  bool found_ta = false;

  for (const auto& form : forms) {
    if (form.surface == "する") found_base = true;
    if (form.surface == "しない") found_nai = true;
    if (form.surface == "した") found_ta = true;
  }

  EXPECT_TRUE(found_base);
  EXPECT_TRUE(found_nai);
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateSuruCompound) {
  auto forms = conjugator_.generate("勉強する", VerbType::Suru);

  bool found_base = false;
  bool found_nai = false;
  bool found_ta = false;

  for (const auto& form : forms) {
    if (form.surface == "勉強する") found_base = true;
    if (form.surface == "勉強しない") found_nai = true;
    if (form.surface == "勉強した") found_ta = true;
  }

  EXPECT_TRUE(found_base);
  EXPECT_TRUE(found_nai);
  EXPECT_TRUE(found_ta);
}

TEST_F(ConjugationTest, GenerateKuru) {
  auto forms = conjugator_.generate("来る", VerbType::Kuru);

  bool found_base = false;
  bool found_konai = false;  // こない
  bool found_kita = false;   // きた

  for (const auto& form : forms) {
    if (form.surface == "来る") found_base = true;
    if (form.surface == "来こない") found_konai = true;
    if (form.surface == "来きた") found_kita = true;
  }

  EXPECT_TRUE(found_base);
  EXPECT_TRUE(found_konai);
  EXPECT_TRUE(found_kita);
}

// ============================================================================
// generate tests - IAdjective
// ============================================================================

TEST_F(ConjugationTest, GenerateIAdjective) {
  auto forms = conjugator_.generate("高い", VerbType::IAdjective);

  bool found_base = false;
  bool found_kunai = false;   // くない
  bool found_katta = false;   // かった
  bool found_kute = false;    // くて

  for (const auto& form : forms) {
    if (form.surface == "高い") found_base = true;
    if (form.surface == "高くない") found_kunai = true;
    if (form.surface == "高かった") found_katta = true;
    if (form.surface == "高くて") found_kute = true;
  }

  EXPECT_TRUE(found_base);
  EXPECT_TRUE(found_kunai);
  EXPECT_TRUE(found_katta);
  EXPECT_TRUE(found_kute);
}

// ============================================================================
// generate tests - Unknown
// ============================================================================

TEST_F(ConjugationTest, GenerateUnknown) {
  auto forms = conjugator_.generate("テスト", VerbType::Unknown);
  EXPECT_TRUE(forms.empty());
}

// ============================================================================
// verbTypeToString tests
// ============================================================================

TEST(VerbTypeStringTest, AllTypes) {
  EXPECT_EQ(verbTypeToString(VerbType::Ichidan), "ichidan");
  EXPECT_EQ(verbTypeToString(VerbType::GodanKa), "godan-ka");
  EXPECT_EQ(verbTypeToString(VerbType::GodanGa), "godan-ga");
  EXPECT_EQ(verbTypeToString(VerbType::GodanSa), "godan-sa");
  EXPECT_EQ(verbTypeToString(VerbType::GodanTa), "godan-ta");
  EXPECT_EQ(verbTypeToString(VerbType::GodanNa), "godan-na");
  EXPECT_EQ(verbTypeToString(VerbType::GodanBa), "godan-ba");
  EXPECT_EQ(verbTypeToString(VerbType::GodanMa), "godan-ma");
  EXPECT_EQ(verbTypeToString(VerbType::GodanRa), "godan-ra");
  EXPECT_EQ(verbTypeToString(VerbType::GodanWa), "godan-wa");
  EXPECT_EQ(verbTypeToString(VerbType::Suru), "suru");
  EXPECT_EQ(verbTypeToString(VerbType::Kuru), "kuru");
  EXPECT_EQ(verbTypeToString(VerbType::IAdjective), "i-adj");
  EXPECT_EQ(verbTypeToString(VerbType::Unknown), "");
}

// ============================================================================
// verbTypeToJapanese tests
// ============================================================================

TEST(VerbTypeJapaneseTest, AllTypes) {
  EXPECT_EQ(verbTypeToJapanese(VerbType::Ichidan), "一段");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanKa), "五段・カ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanGa), "五段・ガ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanSa), "五段・サ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanTa), "五段・タ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanNa), "五段・ナ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanBa), "五段・バ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanMa), "五段・マ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanRa), "五段・ラ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::GodanWa), "五段・ワ行");
  EXPECT_EQ(verbTypeToJapanese(VerbType::Suru), "サ変");
  EXPECT_EQ(verbTypeToJapanese(VerbType::Kuru), "カ変");
  EXPECT_EQ(verbTypeToJapanese(VerbType::IAdjective), "形容詞");
  EXPECT_EQ(verbTypeToJapanese(VerbType::Unknown), "");
}

// ============================================================================
// conjFormToString tests
// ============================================================================

TEST(ConjFormStringTest, AllForms) {
  EXPECT_EQ(conjFormToString(ConjForm::Base), "base");
  EXPECT_EQ(conjFormToString(ConjForm::Mizenkei), "mizenkei");
  EXPECT_EQ(conjFormToString(ConjForm::Renyokei), "renyokei");
  EXPECT_EQ(conjFormToString(ConjForm::Onbinkei), "onbinkei");
  EXPECT_EQ(conjFormToString(ConjForm::Kateikei), "kateikei");
  EXPECT_EQ(conjFormToString(ConjForm::Meireikei), "meireikei");
  EXPECT_EQ(conjFormToString(ConjForm::Ishikei), "ishikei");
}

// ============================================================================
// conjFormToJapanese tests
// ============================================================================

TEST(ConjFormJapaneseTest, AllForms) {
  EXPECT_EQ(conjFormToJapanese(ConjForm::Base), "終止形");
  EXPECT_EQ(conjFormToJapanese(ConjForm::Mizenkei), "未然形");
  EXPECT_EQ(conjFormToJapanese(ConjForm::Renyokei), "連用形");
  EXPECT_EQ(conjFormToJapanese(ConjForm::Onbinkei), "連用形");
  EXPECT_EQ(conjFormToJapanese(ConjForm::Kateikei), "仮定形");
  EXPECT_EQ(conjFormToJapanese(ConjForm::Meireikei), "命令形");
  EXPECT_EQ(conjFormToJapanese(ConjForm::Ishikei), "意志形");
}

}  // namespace
}  // namespace grammar
}  // namespace suzume
