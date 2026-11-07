/**
 * @file conjugator_test.cpp
 * @brief Tests for Conjugator stem generation with connection IDs
 */

#include "conjugator.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace suzume {
namespace grammar {
namespace {

class ConjugatorTest : public ::testing::Test {
 protected:
  Conjugator conjugator_;

  // Helper to find a stem form by its right_id
  const StemForm* findByRightId(const std::vector<StemForm>& forms, uint16_t right_id) const {
    for (const auto& form : forms) {
      if (form.right_id == right_id) {
        return &form;
      }
    }
    return nullptr;
  }
};

// ============================================================================
// Ichidan verb tests
// ============================================================================

TEST_F(ConjugatorTest, IchidanStems) {
  auto forms = conjugator_.generateStems("食べる", VerbType::Ichidan);
  ASSERT_EQ(forms.size(), 7u);

  // Base form
  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "食べる");
  EXPECT_EQ(base->base_suffix, "る");
  EXPECT_EQ(base->verb_type, VerbType::Ichidan);

  // Mizenkei (stem only)
  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "食べ");

  // Renyokei (stem only)
  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "食べ");

  // Onbinkei (stem only for ichidan)
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "食べ");

  EXPECT_EQ(findByRightId(forms, conn::kVerbKatei)->surface, "食べれ");
  EXPECT_EQ(findByRightId(forms, conn::kVerbMeireikei)->surface, "食べろ");
  EXPECT_EQ(findByRightId(forms, conn::kVerbVolitional)->surface, "食べよ");
}

TEST_F(ConjugatorTest, IchidanShortVerb) {
  auto forms = conjugator_.generateStems("見る", VerbType::Ichidan);
  ASSERT_EQ(forms.size(), 7u);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "見る");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "見");
}

// ============================================================================
// Godan verb tests
// ============================================================================

TEST_F(ConjugatorTest, GodanKaStems) {
  auto forms = conjugator_.generateStems("書く", VerbType::GodanKa);
  ASSERT_EQ(forms.size(), 8u);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "書く");
  EXPECT_EQ(base->base_suffix, "く");

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "書か");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "書き");

  // Onbinkei: i-onbin for ka-row
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "書い");
}

TEST_F(ConjugatorTest, IkuUsesIrregularSokuonbinInGeneratedStems) {
  for (const auto& base_form : {"行く", "いく"}) {
    auto forms = conjugator_.generateStems(base_form, VerbType::GodanKa);
    auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
    ASSERT_NE(onbinkei, nullptr);
    EXPECT_EQ(onbinkei->surface, base_form == std::string_view("行く") ? "行っ" : "いっ");
  }
}

TEST_F(ConjugatorTest, GodanGaStems) {
  auto forms = conjugator_.generateStems("泳ぐ", VerbType::GodanGa);
  ASSERT_EQ(forms.size(), 8u);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "泳ぐ");
  EXPECT_EQ(base->base_suffix, "ぐ");

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "泳が");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "泳ぎ");

  // Onbinkei: i-onbin for ga-row
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "泳い");
}

TEST_F(ConjugatorTest, GodanSaStems) {
  auto forms = conjugator_.generateStems("話す", VerbType::GodanSa);
  ASSERT_EQ(forms.size(), 8u);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "話す");

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "話さ");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "話し");

  // Sa-row: renyokei doubles as onbinkei (no separate onbin form)
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "話し");
}

TEST_F(ConjugatorTest, GodanTaStems) {
  auto forms = conjugator_.generateStems("持つ", VerbType::GodanTa);

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "持た");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "持ち");

  // Sokuonbin for ta-row
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "持っ");
}

TEST_F(ConjugatorTest, GodanNaStems) {
  auto forms = conjugator_.generateStems("死ぬ", VerbType::GodanNa);

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "死な");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "死に");

  // N-onbin for na-row
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "死ん");
}

TEST_F(ConjugatorTest, GodanBaStems) {
  auto forms = conjugator_.generateStems("遊ぶ", VerbType::GodanBa);

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "遊ば");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "遊び");

  // N-onbin for ba-row
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "遊ん");
}

TEST_F(ConjugatorTest, GodanMaStems) {
  auto forms = conjugator_.generateStems("読む", VerbType::GodanMa);

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "読ま");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "読み");

  // N-onbin for ma-row
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "読ん");
}

TEST_F(ConjugatorTest, GodanRaStems) {
  auto forms = conjugator_.generateStems("走る", VerbType::GodanRa);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "走る");
  EXPECT_EQ(base->base_suffix, "る");

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "走ら");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "走り");

  // Sokuonbin for ra-row
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "走っ");
}

TEST_F(ConjugatorTest, GodanWaStems) {
  auto forms = conjugator_.generateStems("買う", VerbType::GodanWa);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "買う");
  EXPECT_EQ(base->base_suffix, "う");

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "買わ");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "買い");

  // Sokuonbin for wa-row
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "買っ");
}

TEST_F(ConjugatorTest, GodanWaUOnbinUsesTheClosedLexicalSubclass) {
  for (const auto& [base_form, expected] : std::array<std::pair<const char*, const char*>, 3>{{
           {"問う", "問う"},
           {"乞う", "乞う"},
           {"請う", "請う"},
       }}) {
    const auto forms = conjugator_.generateStems(base_form, VerbType::GodanWa);
    const auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
    ASSERT_NE(onbinkei, nullptr);
    EXPECT_EQ(onbinkei->surface, expected);
  }
}

// ============================================================================
// Suru verb tests
// ============================================================================

TEST_F(ConjugatorTest, SuruStems) {
  auto forms = conjugator_.generateStems("する", VerbType::Suru);
  ASSERT_EQ(forms.size(), 13u);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "する");
  EXPECT_EQ(base->base_suffix, "する");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "し");

  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "し");

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_TRUE(std::any_of(forms.begin(), forms.end(), [](const StemForm& form) {
    return form.right_id == conn::kVerbMizenkei && form.surface == "さ";
  }));

  EXPECT_EQ(findByRightId(forms, conn::kVerbKatei)->surface, "すれ");
  EXPECT_EQ(findByRightId(forms, conn::kVerbMeireikei)->surface, "しろ");
  EXPECT_EQ(findByRightId(forms, conn::kVerbVolitional)->surface, "しよ");
}

TEST_F(ConjugatorTest, SuruWithPrefix) {
  auto forms = conjugator_.generateStems("勉強する", VerbType::Suru);
  ASSERT_EQ(forms.size(), 13u);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "勉強する");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "勉強し");

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_TRUE(std::any_of(forms.begin(), forms.end(), [](const StemForm& form) {
    return form.right_id == conn::kVerbMizenkei && form.surface == "勉強さ";
  }));
}

// ============================================================================
// Kuru verb tests
// ============================================================================

TEST_F(ConjugatorTest, KuruStems) {
  auto forms = conjugator_.generateStems("くる", VerbType::Kuru);
  ASSERT_EQ(forms.size(), 7u);

  auto* base = findByRightId(forms, conn::kVerbBase);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->surface, "くる");
  EXPECT_EQ(base->base_suffix, "くる");

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  ASSERT_NE(renyokei, nullptr);
  EXPECT_EQ(renyokei->surface, "き");

  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  ASSERT_NE(onbinkei, nullptr);
  EXPECT_EQ(onbinkei->surface, "き");

  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  ASSERT_NE(mizenkei, nullptr);
  EXPECT_EQ(mizenkei->surface, "こ");

  auto* meireikei = findByRightId(forms, conn::kVerbMeireikei);
  ASSERT_NE(meireikei, nullptr);
  EXPECT_EQ(meireikei->surface, "こい");
  EXPECT_EQ(findByRightId(forms, conn::kVerbKatei)->surface, "くれ");
  EXPECT_EQ(findByRightId(forms, conn::kVerbVolitional)->surface, "こよ");
}

TEST_F(ConjugatorTest, KuruKanjiBaseSuffixUsesDisplayBase) {
  auto forms = conjugator_.generateStems("来る", VerbType::Kuru);
  ASSERT_EQ(forms.size(), 7u);

  for (const auto& form : forms) {
    EXPECT_EQ(form.base_suffix, "来る");
  }

  auto* renyokei = findByRightId(forms, conn::kVerbRenyokei);
  auto* onbinkei = findByRightId(forms, conn::kVerbOnbinkei);
  auto* mizenkei = findByRightId(forms, conn::kVerbMizenkei);
  auto* meireikei = findByRightId(forms, conn::kVerbMeireikei);
  ASSERT_NE(renyokei, nullptr);
  ASSERT_NE(onbinkei, nullptr);
  ASSERT_NE(mizenkei, nullptr);
  ASSERT_NE(meireikei, nullptr);
  EXPECT_EQ(renyokei->surface, "来");
  EXPECT_EQ(onbinkei->surface, "来");
  EXPECT_EQ(mizenkei->surface, "来");
  EXPECT_EQ(meireikei->surface, "来い");
  EXPECT_EQ(findByRightId(forms, conn::kVerbKatei)->surface, "来れ");
  EXPECT_EQ(findByRightId(forms, conn::kVerbVolitional)->surface, "来よ");
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_F(ConjugatorTest, UnknownTypeReturnsEmpty) {
  auto forms = conjugator_.generateStems("テスト", VerbType::Unknown);
  EXPECT_TRUE(forms.empty());
}

TEST_F(ConjugatorTest, IAdjectiveGeneratesEveryBareStemCell) {
  auto forms = conjugator_.generateStems("高い", VerbType::IAdjective);
  ASSERT_EQ(forms.size(), 6u);
  EXPECT_EQ(findByRightId(forms, conn::kVerbBase)->surface, "高い");
  EXPECT_EQ(findByRightId(forms, conn::kIAdjStem)->surface, "高");
  EXPECT_EQ(findByRightId(forms, conn::kVerbRenyokei)->surface, "高く");
  EXPECT_EQ(findByRightId(forms, conn::kVerbOnbinkei)->surface, "高かっ");
  EXPECT_EQ(findByRightId(forms, conn::kVerbKatei)->surface, "高けれ");
  EXPECT_EQ(findByRightId(forms, conn::kVerbVolitional)->surface, "高かろ");
}

TEST_F(ConjugatorTest, AllGodanFormsCoverTheReverseEndingCells) {
  struct TestCase {
    const char* base;
    VerbType type;
  };
  TestCase cases[] = {
      {"書く", VerbType::GodanKa}, {"泳ぐ", VerbType::GodanGa}, {"話す", VerbType::GodanSa},
      {"持つ", VerbType::GodanTa}, {"死ぬ", VerbType::GodanNa}, {"遊ぶ", VerbType::GodanBa},
      {"読む", VerbType::GodanMa}, {"走る", VerbType::GodanRa}, {"買う", VerbType::GodanWa},
  };
  for (const auto& tc : cases) {
    auto forms = conjugator_.generateStems(tc.base, tc.type);
    const size_t expected_size = tc.type == VerbType::GodanRa ? 7U : 8U;
    EXPECT_EQ(forms.size(), expected_size) << "Failed for: " << tc.base;
    EXPECT_NE(findByRightId(forms, conn::kVerbKatei), nullptr) << tc.base;
    EXPECT_NE(findByRightId(forms, conn::kVerbMeireikei), nullptr) << tc.base;
    EXPECT_NE(findByRightId(forms, conn::kVerbVolitional), nullptr) << tc.base;
    EXPECT_EQ(findByRightId(forms, conn::kVerbPotential) != nullptr, tc.type != VerbType::GodanRa) << tc.base;
  }
}

TEST_F(ConjugatorTest, AllFormsHaveCorrectVerbType) {
  auto forms = conjugator_.generateStems("書く", VerbType::GodanKa);
  for (const auto& form : forms) {
    EXPECT_EQ(form.verb_type, VerbType::GodanKa);
  }

  forms = conjugator_.generateStems("食べる", VerbType::Ichidan);
  for (const auto& form : forms) {
    EXPECT_EQ(form.verb_type, VerbType::Ichidan);
  }

  forms = conjugator_.generateStems("する", VerbType::Suru);
  for (const auto& form : forms) {
    EXPECT_EQ(form.verb_type, VerbType::Suru);
  }

  forms = conjugator_.generateStems("くる", VerbType::Kuru);
  for (const auto& form : forms) {
    EXPECT_EQ(form.verb_type, VerbType::Kuru);
  }
}

}  // namespace
}  // namespace grammar
}  // namespace suzume
