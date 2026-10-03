
// Inflection tests: Basic conjugations, Passive, Causative, Iku irregular,
// I-adjective, Analyze, LooksConjugated, Honorific

#include <gtest/gtest.h>

#include <algorithm>

#include "grammar/inflection.h"
#include "grammar/inflection_scorer.h"
#include "grammar/inflection_scorer_constants.h"

namespace suzume::grammar {
namespace {

class InflectionBasicTest : public ::testing::Test {
 protected:
  Inflection inflection_;
};

// ===== Basic verb conjugations =====

TEST_F(InflectionBasicTest, GodanVerbTeForm) {
  auto result = inflection_.getBest("書いて");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, GodanVerbTaForm) {
  auto result = inflection_.getBest("読んだ");
  EXPECT_EQ(result.base_form, "読む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionBasicTest, IchidanVerbTeForm) {
  auto result = inflection_.getBest("食べている");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionBasicTest, SuruClassicalNegativeStem) {
  // Classical 未然形 せ is only licensed by a following negative auxiliary.
  // Test the complete grammatical form so a bare noun ending in せ does not
  // acquire a spurious サ変 candidate.
  const auto candidates = inflection_.analyze("屈せず");
  const bool has_suru_mizenkei = std::any_of(candidates.begin(), candidates.end(), [](const auto& candidate) {
    return candidate.base_form == "屈する" && candidate.verb_type == VerbType::Suru;
  });
  EXPECT_TRUE(has_suru_mizenkei);
}

// ===== Passive forms =====

TEST_F(InflectionBasicTest, GodanPassiveForm) {
  auto result = inflection_.getBest("奪われた");
  EXPECT_EQ(result.base_form, "奪う");
  EXPECT_EQ(result.verb_type, VerbType::GodanWa);
}

TEST_F(InflectionBasicTest, IchidanPassiveForm) {
  auto result = inflection_.getBest("見られた");
  EXPECT_EQ(result.base_form, "見る");
}

// ===== Causative forms =====

TEST_F(InflectionBasicTest, GodanCausativeForm) {
  auto result = inflection_.getBest("書かせた");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IchidanCausativeForm) {
  auto result = inflection_.getBest("食べさせている");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Causative-passive forms =====

TEST_F(InflectionBasicTest, IchidanCausativePassiveForm) {
  auto result = inflection_.getBest("食べさせられた");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionBasicTest, IchidanCausativePassiveFormMiru) {
  auto result = inflection_.getBest("見させられた");
  EXPECT_EQ(result.base_form, "見る");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Iku irregular verb =====

TEST_F(InflectionBasicTest, IkuTeForm) {
  auto result = inflection_.getBest("いって");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTaForm) {
  auto result = inflection_.getBest("いった");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTeIruForm) {
  auto result = inflection_.getBest("いっている");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTeShimattaForm) {
  auto result = inflection_.getBest("いってしまった");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTeKitaForm) {
  auto result = inflection_.getBest("いってきた");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, IkuTeMitaForm) {
  auto result = inflection_.getBest("いってみた");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, GodanTaRenyokeiAuxiliariesKeepGodanLemma) {
  struct Case {
    const char* surface;
    const char* base_form;
  };
  constexpr Case kCases[] = {
      {"もちたい", "もつ"}, {"持ちます", "持つ"}, {"待ちたい", "待つ"}, {"立ちます", "立つ"},
      {"打ちたい", "打つ"}, {"勝ちます", "勝つ"}, {"育ちたい", "育つ"}, {"保ちます", "保つ"},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.surface);
    const auto result = inflection_.getBest(test_case.surface);
    EXPECT_EQ(result.base_form, test_case.base_form);
    EXPECT_EQ(result.verb_type, VerbType::GodanTa);
  }
}

TEST(InflectionScorerTest, GodanTaPenaltyOnlyAppliesToTeDeStartingAuxiliary) {
  constexpr size_t kOneKanaBytes = 3;
  InflectionScorerOptions opts;
  opts.confidence_floor = 0.0F;
  opts.confidence_ceiling = 1.0F;
  const float regular_aux = calculateConfidence(VerbType::GodanTa, "持", kOneKanaBytes, 1, conn::kVerbRenyokei,
                                                kOneKanaBytes * 2, "た", &opts);
  const float te_de_aux = calculateConfidence(VerbType::GodanTa, "持", kOneKanaBytes, 1, conn::kVerbRenyokei,
                                              kOneKanaBytes * 2, "て", &opts);

  EXPECT_FLOAT_EQ(regular_aux - te_de_aux, inflection::kPenaltyGodanTaRenyokeiTeDeAuxInvalid);
}

// ===== I-adjective patterns =====

TEST_F(InflectionBasicTest, IAdjPastForm) {
  auto result = inflection_.getBest("美しかった");
  EXPECT_EQ(result.base_form, "美しい");
  EXPECT_EQ(result.verb_type, VerbType::IAdjective);
}

TEST_F(InflectionBasicTest, IAdjNegativeForm) {
  auto result = inflection_.getBest("美しくない");
  EXPECT_EQ(result.base_form, "美しい");
  EXPECT_EQ(result.verb_type, VerbType::IAdjective);
}

// ===== Analyze returns multiple candidates =====

TEST_F(InflectionBasicTest, AnalyzeReturnsMultipleCandidates) {
  auto candidates = inflection_.analyze("書いた");
  EXPECT_GT(candidates.size(), 1);
  EXPECT_EQ(candidates[0].base_form, "書く");
}

TEST_F(InflectionBasicTest, AnalyzeSortsByConfidence) {
  auto candidates = inflection_.analyze("作ってみた");
  EXPECT_GT(candidates.size(), 1);
  for (size_t idx = 1; idx < candidates.size(); ++idx) {
    EXPECT_GE(candidates[idx - 1].confidence, candidates[idx].confidence);
  }
}

// ===== LooksConjugated =====

TEST_F(InflectionBasicTest, LooksConjugatedTrue) {
  EXPECT_TRUE(inflection_.looksConjugated("食べた"));
  EXPECT_TRUE(inflection_.looksConjugated("書いている"));
  EXPECT_TRUE(inflection_.looksConjugated("読めなかった"));
}

TEST_F(InflectionBasicTest, LooksConjugatedFalse) {
  EXPECT_FALSE(inflection_.looksConjugated("あ"));
  EXPECT_FALSE(inflection_.looksConjugated(""));
}

// ===== Honorific verb forms =====

TEST_F(InflectionBasicTest, HonorificIrasshattaForm) {
  auto result = inflection_.getBest("いらっしゃった");
  EXPECT_EQ(result.base_form, "いらっしゃる");
}

TEST_F(InflectionBasicTest, HonorificOsshatteitaForm) {
  auto result = inflection_.getBest("おっしゃっていた");
  EXPECT_EQ(result.base_form, "おっしゃる");
}

TEST_F(InflectionBasicTest, HonorificKudasattaForm) {
  auto result = inflection_.getBest("くださった");
  EXPECT_EQ(result.base_form, "くださる");
}

TEST_F(InflectionBasicTest, HonorificNasattaForm) {
  auto result = inflection_.getBest("なさった");
  EXPECT_EQ(result.base_form, "なさる");
}

// ===== Negative progressive forms =====

TEST_F(InflectionBasicTest, NegativeProgressiveIchidan) {
  auto result = inflection_.getBest("食べないでいた");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionBasicTest, NegativeProgressiveGodanKa) {
  auto result = inflection_.getBest("書かないでいた");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, NegativeProgressiveSuru) {
  auto result = inflection_.getBest("勉強しないでいた");
  EXPECT_EQ(result.base_form, "勉強する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

// ===== Suru verb renyokei =====

TEST_F(InflectionBasicTest, SuruRenyokeiBunkatsu) {
  auto result = inflection_.getBest("分割し");
  EXPECT_EQ(result.base_form, "分割する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

TEST_F(InflectionBasicTest, SuruRenyokeiBenkyo) {
  auto result = inflection_.getBest("勉強し");
  EXPECT_EQ(result.base_form, "勉強する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

// ===== Conditional form =====

TEST_F(InflectionBasicTest, ConditionalBa_TwoKanjiStem) {
  auto result = inflection_.getBest("頑張れば");
  EXPECT_EQ(result.base_form, "頑張る");
  EXPECT_EQ(result.verb_type, VerbType::GodanRa);
}

// ===== Suru passive negative past =====

TEST_F(InflectionBasicTest, SuruPassiveNegativePast) {
  auto result = inflection_.getBest("されなかった");
  EXPECT_EQ(result.base_form, "される");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionBasicTest, SuruPassiveNegativePast_Compound) {
  auto result = inflection_.getBest("開催されなかった");
  EXPECT_EQ(result.base_form, "開催する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

// ===== Volitional forms (意志形) =====

TEST_F(InflectionBasicTest, GodanVolitional_Iku) {
  auto result = inflection_.getBest("行こう");
  EXPECT_EQ(result.base_form, "行く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionBasicTest, GodanVolitional_Hashiru) {
  auto result = inflection_.getBest("走ろう");
  EXPECT_EQ(result.base_form, "走る");
  EXPECT_EQ(result.verb_type, VerbType::GodanRa);
}

TEST_F(InflectionBasicTest, IchidanVolitional_Taberu) {
  auto result = inflection_.getBest("食べよう");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionBasicTest, HiraganaVolitional_Ikou) {
  auto result = inflection_.getBest("いこう");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

// ===== Verb + そう (appearance/likelihood) =====

TEST_F(InflectionBasicTest, VerbSou_Furi) {
  auto result = inflection_.getBest("降りそうだ");
  EXPECT_EQ(result.base_form, "降る");
  EXPECT_EQ(result.verb_type, VerbType::GodanRa);
}

TEST_F(InflectionBasicTest, VerbSou_Nomu) {
  auto result = inflection_.getBest("飲みそう");
  EXPECT_EQ(result.base_form, "飲む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionBasicTest, VerbSou_Taberu) {
  auto result = inflection_.getBest("食べそうだ");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Irregular verb validation =====
// くなかった should NOT be analyzed as Ichidan くる (来る is Kuru, not Ichidan)
// This regression test ensures we don't incorrectly treat くなかった as a conjugation of くる
TEST_F(InflectionBasicTest, KuNakatta_NotIchidanKuru) {
  auto result = inflection_.getBest("くなかった");
  // The Ichidan interpretation with stem く → base form くる should be rejected
  EXPECT_NE(result.base_form, "くる");
}

TEST_F(InflectionBasicTest, ParticleNaGodanWaPenaltyCoversCanonicalParticleSet) {
  auto godan_wa_confidence = [this](std::string_view surface) {
    const auto& candidates = inflection_.analyze(surface);
    const auto candidate = std::find_if(candidates.begin(), candidates.end(),
                                        [](const auto& value) { return value.verb_type == VerbType::GodanWa; });
    EXPECT_NE(candidate, candidates.end()) << surface;
    return candidate == candidates.end() ? -1.0F : candidate->confidence;
  };

  const float established_particle_confidence = godan_wa_confidence("もない");
  EXPECT_FLOAT_EQ(godan_wa_confidence("わない"), established_particle_confidence);
  EXPECT_FLOAT_EQ(godan_wa_confidence("ぞない"), established_particle_confidence);
}

// すなかった should NOT be analyzed as Ichidan する (する is Suru, not Ichidan)
TEST_F(InflectionBasicTest, SuNakatta_NotIchidanSuru) {
  auto result = inflection_.getBest("すなかった");
  // The Ichidan interpretation with stem す → base form する should be rejected
  EXPECT_NE(result.base_form, "する");
}

// Verify valid Kuru conjugations still work
TEST_F(InflectionBasicTest, Kuru_ValidConjugations) {
  // 来なかった is the correct negative past of 来る
  auto result = inflection_.getBest("来なかった");
  EXPECT_EQ(result.base_form, "来る");
  EXPECT_EQ(result.verb_type, VerbType::Kuru);
}

TEST_F(InflectionBasicTest, KuruKanaFormsHaveNoPrefixedFalseCandidates) {
  for (const std::string_view surface : {"くる", "きます", "こない", "くれば", "こよう"}) {
    SCOPED_TRACE(surface);
    const auto& candidates = inflection_.analyze(surface);
    const auto valid_kuru = std::find_if(candidates.begin(), candidates.end(), [](const auto& candidate) {
      return candidate.verb_type == VerbType::Kuru && candidate.base_form == "くる";
    });
    EXPECT_NE(valid_kuru, candidates.end());
  }

  for (const std::string_view surface : {"かきます", "さきます", "ききます"}) {
    SCOPED_TRACE(surface);
    const auto& candidates = inflection_.analyze(surface);
    const auto false_kuru = std::find_if(candidates.begin(), candidates.end(),
                                         [](const auto& candidate) { return candidate.verb_type == VerbType::Kuru; });
    EXPECT_EQ(false_kuru, candidates.end());
  }
}

// Verify valid Suru conjugations still work
TEST_F(InflectionBasicTest, Suru_ValidConjugations) {
  // しなかった is the correct negative past of する
  auto result = inflection_.getBest("勉強しなかった");
  EXPECT_EQ(result.base_form, "勉強する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

// Verify たくなかった (desiderative negative past) works correctly
TEST_F(InflectionBasicTest, Desiderative_TakuNakatta) {
  auto result = inflection_.getBest("食べたくなかった");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

}  // namespace
}  // namespace suzume::grammar
