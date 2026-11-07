#include "grammar/conjugation.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>

#include "grammar/verb_endings.h"

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
  EXPECT_EQ(conjugator_.getStem("くる", VerbType::Kuru), "く");
  EXPECT_TRUE(isKuruStem("来"));
  EXPECT_TRUE(isKuruStem(""));
  EXPECT_FALSE(isKuruStem("書"));
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
  // The old spelling is the same verb (isKuruKanjiBaseForm), not a godan ra-row verb.
  EXPECT_EQ(conjugator_.detectType("來る"), VerbType::Kuru);
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

TEST_F(ConjugationTest, DictionarySuffixesPreserveRegularAndIkuOnbin) {
  const auto suffixes_for = [this](std::string_view base_form) {
    return conjugator_.getDictionarySuffixes(VerbType::GodanKa, base_form);
  };
  const auto has_onbin = [](const std::vector<Conjugation::DictionarySuffix>& suffixes, std::string_view expected) {
    return std::any_of(suffixes.begin(), suffixes.end(), [expected](const auto& suffix) {
      return suffix.extended_pos == core::ExtendedPOS::VerbOnbinkei && suffix.suffix == expected;
    });
  };

  const auto regular = suffixes_for("書く");
  EXPECT_TRUE(has_onbin(regular, "い"));
  EXPECT_FALSE(has_onbin(regular, "っ"));

  for (const std::string_view base_form : {"行く", "いく"}) {
    const auto irregular = suffixes_for(base_form);
    EXPECT_TRUE(has_onbin(irregular, "っ"));
    EXPECT_FALSE(has_onbin(irregular, "い"));
  }
}

TEST_F(ConjugationTest, GodanWaUOnbinUsesTheClosedLexicalSubclass) {
  const auto has_onbin = [](const std::vector<Conjugation::DictionarySuffix>& suffixes, std::string_view expected) {
    return std::any_of(suffixes.begin(), suffixes.end(), [expected](const auto& suffix) {
      return suffix.extended_pos == core::ExtendedPOS::VerbOnbinkei && suffix.suffix == expected;
    });
  };

  for (const std::string_view base_form : {"問う", "乞う", "請う"}) {
    const auto suffixes = conjugator_.getDictionarySuffixes(VerbType::GodanWa, base_form);
    EXPECT_TRUE(has_onbin(suffixes, "う")) << base_form;
    EXPECT_FALSE(has_onbin(suffixes, "っ")) << base_form;
  }

  const auto regular = conjugator_.getDictionarySuffixes(VerbType::GodanWa, "買う");
  EXPECT_TRUE(has_onbin(regular, "っ"));
  EXPECT_FALSE(has_onbin(regular, "う"));
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
  EXPECT_EQ(conjFormToJapanese(ConjForm::Count_), "");
}

TEST(ConjFormTableTest, EveryConjFormHasOneCanonicalConnectionCell) {
  ASSERT_EQ(kAllVerbConjForms.size(), static_cast<size_t>(ConjForm::Count_));
  ASSERT_EQ(kVerbConjFormConnections.size(), static_cast<size_t>(ConjForm::Count_));

  std::array<bool, static_cast<size_t>(ConjForm::Count_)> seen{};
  for (ConjForm form : kAllVerbConjForms) {
    const size_t index = static_cast<size_t>(form);
    ASSERT_LT(index, seen.size());
    EXPECT_FALSE(seen[index]);
    seen[index] = true;
    EXPECT_FALSE(getVerbEndingsByForm(form).empty());
  }
  for (bool is_seen : seen) {
    EXPECT_TRUE(is_seen);
  }
}

}  // namespace
}  // namespace grammar
}  // namespace suzume
