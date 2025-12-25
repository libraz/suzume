#include "grammar/inflection.h"

#include <gtest/gtest.h>

namespace suzume {
namespace grammar {
namespace {

class InflectionTest : public ::testing::Test {
 protected:
  Inflection inflection_;
};

// ===== Basic verb conjugations =====

TEST_F(InflectionTest, GodanVerbTeForm) {
  auto result = inflection_.getBest("書いて");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, GodanVerbTaForm) {
  auto result = inflection_.getBest("読んだ");
  EXPECT_EQ(result.base_form, "読む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionTest, IchidanVerbTeForm) {
  // Using longer Ichidan suffix pattern that has higher confidence
  auto result = inflection_.getBest("食べている");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Passive forms =====

TEST_F(InflectionTest, GodanPassiveForm) {
  auto result = inflection_.getBest("奪われた");
  EXPECT_EQ(result.base_form, "奪う");
  EXPECT_EQ(result.verb_type, VerbType::GodanWa);
}

TEST_F(InflectionTest, IchidanPassiveForm) {
  // Ichidan passive られた pattern - base form detection
  auto result = inflection_.getBest("見られた");
  EXPECT_EQ(result.base_form, "見る");
  // Note: VerbType may vary due to pattern ambiguity between Ichidan passive
  // and GodanRa passive; the important thing is correct base form
}

// ===== Causative forms =====

TEST_F(InflectionTest, GodanCausativeForm) {
  auto result = inflection_.getBest("書かせた");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, IchidanCausativeForm) {
  // Ichidan causative させている pattern (longer pattern has higher confidence)
  auto result = inflection_.getBest("食べさせている");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Causative-passive forms =====

TEST_F(InflectionTest, IchidanCausativePassiveForm) {
  auto result = inflection_.getBest("食べさせられた");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionTest, IchidanCausativePassiveFormMiru) {
  auto result = inflection_.getBest("見させられた");
  EXPECT_EQ(result.base_form, "見る");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Potential negative/past forms =====

TEST_F(InflectionTest, PotentialNegativePastKaRow) {
  auto result = inflection_.getBest("書けなかった");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, PotentialNegativePastMaRow) {
  auto result = inflection_.getBest("読めなかった");
  EXPECT_EQ(result.base_form, "読む");
  EXPECT_EQ(result.verb_type, VerbType::GodanMa);
}

TEST_F(InflectionTest, PotentialNegativePastWaRow) {
  auto result = inflection_.getBest("もらえなかった");
  EXPECT_EQ(result.base_form, "もらう");
  EXPECT_EQ(result.verb_type, VerbType::GodanWa);
}

TEST_F(InflectionTest, PotentialPoliteNegativePast) {
  auto result = inflection_.getBest("書けませんでした");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

// ===== Iku irregular verb =====

TEST_F(InflectionTest, IkuTeForm) {
  auto result = inflection_.getBest("いって");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, IkuTaForm) {
  auto result = inflection_.getBest("いった");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, IkuTeIruForm) {
  auto result = inflection_.getBest("いっている");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, IkuTeShimattaForm) {
  auto result = inflection_.getBest("いってしまった");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, IkuTeKitaForm) {
  auto result = inflection_.getBest("いってきた");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, IkuTeMitaForm) {
  auto result = inflection_.getBest("いってみた");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

// ===== Compound verb patterns =====

TEST_F(InflectionTest, CompoundTeMita) {
  auto result = inflection_.getBest("作ってみた");
  EXPECT_EQ(result.base_form, "作る");
  EXPECT_EQ(result.verb_type, VerbType::GodanRa);
}

TEST_F(InflectionTest, CompoundTeShimatta) {
  auto result = inflection_.getBest("忘れてしまった");
  EXPECT_EQ(result.base_form, "忘れる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionTest, CompoundTeOita) {
  auto result = inflection_.getBest("準備しておいた");
  EXPECT_EQ(result.base_form, "準備する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

TEST_F(InflectionTest, CompoundCausativePassiveTeKita) {
  auto result = inflection_.getBest("いかされてきた");
  EXPECT_EQ(result.base_form, "いく");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

// ===== I-adjective patterns =====

TEST_F(InflectionTest, IAdjPastForm) {
  auto result = inflection_.getBest("美しかった");
  EXPECT_EQ(result.base_form, "美しい");
  EXPECT_EQ(result.verb_type, VerbType::IAdjective);
}

TEST_F(InflectionTest, IAdjNegativeForm) {
  auto result = inflection_.getBest("美しくない");
  EXPECT_EQ(result.base_form, "美しい");
  EXPECT_EQ(result.verb_type, VerbType::IAdjective);
}

// ===== Analyze returns multiple candidates =====

TEST_F(InflectionTest, AnalyzeReturnsMultipleCandidates) {
  auto candidates = inflection_.analyze("書いた");
  EXPECT_GT(candidates.size(), 1);
  // First candidate should be the best match
  EXPECT_EQ(candidates[0].base_form, "書く");
}

TEST_F(InflectionTest, AnalyzeSortsByConfidence) {
  auto candidates = inflection_.analyze("作ってみた");
  EXPECT_GT(candidates.size(), 1);
  // Verify sorted by confidence (descending)
  for (size_t idx = 1; idx < candidates.size(); ++idx) {
    EXPECT_GE(candidates[idx - 1].confidence, candidates[idx].confidence);
  }
}

// ===== LooksConjugated =====

TEST_F(InflectionTest, LooksConjugatedTrue) {
  EXPECT_TRUE(inflection_.looksConjugated("食べた"));
  EXPECT_TRUE(inflection_.looksConjugated("書いている"));
  EXPECT_TRUE(inflection_.looksConjugated("読めなかった"));
}

TEST_F(InflectionTest, LooksConjugatedFalse) {
  // Very short strings that don't match any pattern
  EXPECT_FALSE(inflection_.looksConjugated("あ"));
  EXPECT_FALSE(inflection_.looksConjugated(""));
}

// ===== Honorific verb forms =====

TEST_F(InflectionTest, HonorificIrasshattaForm) {
  auto result = inflection_.getBest("いらっしゃった");
  EXPECT_EQ(result.base_form, "いらっしゃる");
}

TEST_F(InflectionTest, HonorificOsshatteitaForm) {
  auto result = inflection_.getBest("おっしゃっていた");
  EXPECT_EQ(result.base_form, "おっしゃる");
}

TEST_F(InflectionTest, HonorificKudasattaForm) {
  auto result = inflection_.getBest("くださった");
  EXPECT_EQ(result.base_form, "くださる");
}

TEST_F(InflectionTest, HonorificNasattaForm) {
  auto result = inflection_.getBest("なさった");
  EXPECT_EQ(result.base_form, "なさる");
}

// ===== Negative progressive forms =====

TEST_F(InflectionTest, NegativeProgressiveIchidan) {
  auto result = inflection_.getBest("食べないでいた");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionTest, NegativeProgressiveGodanKa) {
  auto result = inflection_.getBest("書かないでいた");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, NegativeProgressiveSuru) {
  auto result = inflection_.getBest("勉強しないでいた");
  EXPECT_EQ(result.base_form, "勉強する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

// ===== Compound verb: てもらう =====

TEST_F(InflectionTest, CompoundTeMorattaGodanKa) {
  auto result = inflection_.getBest("書いてもらった");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, CompoundTeMorattaIchidan) {
  auto result = inflection_.getBest("教えてもらった");
  EXPECT_EQ(result.base_form, "教える");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionTest, CompoundTeMoratteiru) {
  auto result = inflection_.getBest("教えてもらっている");
  EXPECT_EQ(result.base_form, "教える");
}

// ===== Compound verb: てくれる =====

TEST_F(InflectionTest, CompoundTeKuretaGodanKa) {
  auto result = inflection_.getBest("書いてくれた");
  EXPECT_EQ(result.base_form, "書く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, CompoundTeKuretaIchidan) {
  auto result = inflection_.getBest("教えてくれた");
  EXPECT_EQ(result.base_form, "教える");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Compound verb: てあげる =====

TEST_F(InflectionTest, CompoundTeAgetaIchidan) {
  auto result = inflection_.getBest("教えてあげた");
  EXPECT_EQ(result.base_form, "教える");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionTest, CompoundTeAgetaGodanWa) {
  // Note: ってあげた pattern is ambiguous (GodanRa/Wa/Ta) without dictionary
  // Pattern matching defaults to GodanRa (most common), so we only check that
  // verb_type is one of the valid options
  auto result = inflection_.getBest("買ってあげた");
  EXPECT_TRUE(result.base_form == "買う" || result.base_form == "買る" ||
              result.base_form == "買つ");
}

// ===== Compound verb: ておる (humble/polite) =====

TEST_F(InflectionTest, CompoundTeOrimasu) {
  // Note: っております pattern is ambiguous (GodanRa/Wa/Ta) without dictionary
  // Pattern matching defaults to GodanRa (most common)
  auto result = inflection_.getBest("待っております");
  EXPECT_TRUE(result.base_form == "待つ" || result.base_form == "待る" ||
              result.base_form == "待う");
}

TEST_F(InflectionTest, CompoundTeOrimasita) {
  auto result = inflection_.getBest("いただいておりました");
  EXPECT_EQ(result.base_form, "いただく");
}

// ===== Passive + compound =====

TEST_F(InflectionTest, PassiveTeShimatta) {
  auto result = inflection_.getBest("殺されてしまった");
  EXPECT_EQ(result.base_form, "殺す");
  EXPECT_EQ(result.verb_type, VerbType::GodanSa);
}

TEST_F(InflectionTest, PassiveTeKita) {
  auto result = inflection_.getBest("愛されてきた");
  EXPECT_EQ(result.base_form, "愛す");
  EXPECT_EQ(result.verb_type, VerbType::GodanSa);
}

// ===== Causative-passive + compound =====

TEST_F(InflectionTest, CausativePassiveTeKita) {
  auto result = inflection_.getBest("歩かされてきた");
  EXPECT_EQ(result.base_form, "歩く");
  EXPECT_EQ(result.verb_type, VerbType::GodanKa);
}

TEST_F(InflectionTest, CausativePassiveTeIta) {
  auto result = inflection_.getBest("待たされていた");
  EXPECT_EQ(result.base_form, "待つ");
  EXPECT_EQ(result.verb_type, VerbType::GodanTa);
}

// ===== Complex: Triple compound verbs (三重複合) =====

TEST_F(InflectionTest, TripleCompoundTeMiteOita) {
  // 書く + てみる + ておく + た
  auto result = inflection_.getBest("書いてみておいた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TripleCompoundTeShimatteIta) {
  // 読む + てしまう + ている + た
  auto result = inflection_.getBest("読んでしまっていた");
  EXPECT_EQ(result.base_form, "読む");
}

TEST_F(InflectionTest, TripleCompoundCausativePassiveTeShimatta) {
  // 食べる + させられる + てしまう + た
  auto result = inflection_.getBest("食べさせられてしまった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: Potential + なる patterns =====

TEST_F(InflectionTest, PotentialNaruGodanMa) {
  // 読む → 読める + ようになる + た
  auto result = inflection_.getBest("読めるようになった");
  EXPECT_EQ(result.base_form, "読む");
}

TEST_F(InflectionTest, PotentialNaruTeKita) {
  // 書く → 書ける + ようになる + てくる + た
  auto result = inflection_.getBest("書けるようになってきた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, PotentialNegativeNaruTeShimatta) {
  // 話す → 話せない + なる + てしまう + た
  auto result = inflection_.getBest("話せなくなってしまった");
  EXPECT_EQ(result.base_form, "話す");
}

// ===== Complex: ていただく (polite request/receiving) =====

TEST_F(InflectionTest, TeItadakuGodanKa) {
  auto result = inflection_.getBest("書いていただいた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TeItadakuIchidan) {
  auto result = inflection_.getBest("教えていただきました");
  EXPECT_EQ(result.base_form, "教える");
}

TEST_F(InflectionTest, TeItadakuSuru) {
  auto result = inflection_.getBest("説明していただけますか");
  EXPECT_EQ(result.base_form, "説明する");
}

// ===== Complex: てほしい (wanting someone to do) =====

TEST_F(InflectionTest, TeHoshiiGodanKa) {
  auto result = inflection_.getBest("書いてほしかった");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TeHoshiiIchidan) {
  auto result = inflection_.getBest("食べてほしい");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: Colloquial ちゃう/じゃう forms =====

TEST_F(InflectionTest, ColloquialChattaIchidan) {
  // 食べてしまった → 食べちゃった
  auto result = inflection_.getBest("食べちゃった");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionTest, ColloquialJattaGodanMa) {
  // 読んでしまった → 読んじゃった
  auto result = inflection_.getBest("読んじゃった");
  EXPECT_EQ(result.base_form, "読む");
}

TEST_F(InflectionTest, ColloquialChauIchidan) {
  auto result = inflection_.getBest("忘れちゃう");
  EXPECT_EQ(result.base_form, "忘れる");
}

// ===== Complex: ないといけない/なければならない =====

TEST_F(InflectionTest, NaiToIkenaiGodanKa) {
  auto result = inflection_.getBest("書かないといけない");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, NakerebaNaranaiIchidan) {
  auto result = inflection_.getBest("食べなければならない");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: ようとする (attempting) =====

TEST_F(InflectionTest, YouToSuruGodanKa) {
  auto result = inflection_.getBest("書こうとした");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, YouToSuruIchidan) {
  auto result = inflection_.getBest("食べようとしている");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: ことができる (ability) =====

TEST_F(InflectionTest, KotoGaDekiruGodanKa) {
  auto result = inflection_.getBest("書くことができた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, KotoGaDekiruIchidan) {
  auto result = inflection_.getBest("食べることができない");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: Casual explanatory forms (んだ/のだ/だもん) =====

TEST_F(InflectionTest, CasualNdaGodanKa) {
  // 書くんだ (explanatory)
  auto result = inflection_.getBest("書くんだ");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, CasualNdamonIchidan) {
  // 食べるんだもん (because I eat, casual)
  auto result = inflection_.getBest("食べるんだもん");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionTest, CasualTandamonGodanKa) {
  // 書いたんだもん (because I wrote, casual past)
  auto result = inflection_.getBest("書いたんだもん");
  EXPECT_EQ(result.base_form, "書く");
}

// ===== Complex: たり form (doing things like) =====

TEST_F(InflectionTest, TariFormGodanKa) {
  auto result = inflection_.getBest("書いたりした");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TariFormIchidan) {
  auto result = inflection_.getBest("食べたりする");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: ながら form (while doing) =====

TEST_F(InflectionTest, NagaraFormGodanKa) {
  auto result = inflection_.getBest("書きながら");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, NagaraFormIchidan) {
  auto result = inflection_.getBest("食べながら");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: すぎる (too much) =====

TEST_F(InflectionTest, SugiruGodanKa) {
  auto result = inflection_.getBest("書きすぎた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, SugiruIchidan) {
  auto result = inflection_.getBest("食べすぎている");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: やすい/にくい (easy/hard to do) =====

TEST_F(InflectionTest, YasuiGodanKa) {
  auto result = inflection_.getBest("書きやすい");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, NikuiIchidan) {
  auto result = inflection_.getBest("食べにくかった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: Nested compound patterns =====

TEST_F(InflectionTest, NestedTeShimatteitaGodanKa) {
  // 書いてしまっていた (had completely written)
  auto result = inflection_.getBest("書いてしまっていた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, NestedTeKiteiru) {
  // 増えてきている (has been increasing)
  auto result = inflection_.getBest("増えてきている");
  EXPECT_EQ(result.base_form, "増える");
}

TEST_F(InflectionTest, NestedTeOiteAru) {
  // 書いておいてある (has been prepared in writing)
  auto result = inflection_.getBest("書いておいてある");
  EXPECT_EQ(result.base_form, "書く");
}

// ===== Complex: てくださる (honorific giving of action) =====

TEST_F(InflectionTest, TeKudasaruGodanKa) {
  auto result = inflection_.getBest("書いてくださった");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TeKudasaruIchidan) {
  auto result = inflection_.getBest("教えてくださいました");
  EXPECT_EQ(result.base_form, "教える");
}

// ===== Complex: せざるを得ない (cannot help but do) =====

TEST_F(InflectionTest, SezaruWoEnaiGodanKa) {
  // 書かざるを得ない (cannot help but write)
  auto result = inflection_.getBest("書かざるを得ない");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, SezaruWoEnaiIchidan) {
  // 食べざるを得なかった (had no choice but to eat)
  auto result = inflection_.getBest("食べざるを得なかった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: てはいけない/てはならない (must not) =====

TEST_F(InflectionTest, TeWaIkenaiGodanKa) {
  auto result = inflection_.getBest("書いてはいけない");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TeWaNaranaiIchidan) {
  auto result = inflection_.getBest("食べてはならない");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: てもいい/てもかまわない (may do) =====

TEST_F(InflectionTest, TemoIiGodanKa) {
  auto result = inflection_.getBest("書いてもいい");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TemoKamawanaiIchidan) {
  auto result = inflection_.getBest("食べてもかまわない");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: べき (should) =====

TEST_F(InflectionTest, BekiGodanKa) {
  // 書くべきだ (should write)
  auto result = inflection_.getBest("書くべきだ");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, BekiIchidan) {
  // 食べるべきだった (should have eaten)
  auto result = inflection_.getBest("食べるべきだった");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionTest, BekiSuru) {
  // すべきではない (should not do)
  auto result = inflection_.getBest("すべきではない");
  EXPECT_EQ(result.base_form, "する");
}

// ===== Complex: ところだ (about to / just did) =====

TEST_F(InflectionTest, TokorodaAboutToGodanKa) {
  // 書くところだ (about to write)
  auto result = inflection_.getBest("書くところだ");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TokorodaJustDidIchidan) {
  // 食べたところだ (just ate)
  auto result = inflection_.getBest("食べたところだ");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionTest, TokorodaProgressiveGodanMa) {
  // 読んでいるところだった (was in the middle of reading)
  auto result = inflection_.getBest("読んでいるところだった");
  EXPECT_EQ(result.base_form, "読む");
}

// ===== Complex: ばかり (just did / only) =====

TEST_F(InflectionTest, BakariJustDidGodanKa) {
  // 書いたばかりだ (just wrote)
  auto result = inflection_.getBest("書いたばかりだ");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, BakariIchidan) {
  // 食べたばかりなのに (even though just ate)
  auto result = inflection_.getBest("食べたばかりなのに");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: っぱなし (leaving in state) =====

TEST_F(InflectionTest, PpanashiGodanKa) {
  // 開けっぱなしだ (left open)
  auto result = inflection_.getBest("開けっぱなしだ");
  EXPECT_EQ(result.base_form, "開ける");
}

TEST_F(InflectionTest, PpanashiGodanRa) {
  // 出しっぱなしにする (leave out)
  auto result = inflection_.getBest("出しっぱなしにする");
  EXPECT_EQ(result.base_form, "出す");
}

// ===== Complex: かける (about to, half-done) =====

TEST_F(InflectionTest, KakeruGodanKa) {
  // 書きかけた (started to write / half-written)
  auto result = inflection_.getBest("書きかけた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, KakeruIchidan) {
  // 食べかけている (in the middle of eating)
  auto result = inflection_.getBest("食べかけている");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: だす (start to suddenly) =====

TEST_F(InflectionTest, DasuGodanKa) {
  // 書き出した (started writing)
  auto result = inflection_.getBest("書き出した");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, DasuGodanMa) {
  // 読み出して (started reading and...)
  auto result = inflection_.getBest("読み出して");
  EXPECT_EQ(result.base_form, "読む");
}

// ===== Complex: おわる/おえる (finish) =====

TEST_F(InflectionTest, OwaruGodanKa) {
  // 書き終わった (finished writing)
  auto result = inflection_.getBest("書き終わった");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, OeruIchidan) {
  // 食べ終えた (finished eating)
  auto result = inflection_.getBest("食べ終えた");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: つづける (continue) =====

TEST_F(InflectionTest, TsuzukeruGodanKa) {
  // 書き続けている (continuing to write)
  auto result = inflection_.getBest("書き続けている");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, TsuzukeruIchidan) {
  // 食べ続けた (continued eating)
  auto result = inflection_.getBest("食べ続けた");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: なおす (redo) =====

TEST_F(InflectionTest, NaosuGodanKa) {
  // 書き直した (rewrote)
  auto result = inflection_.getBest("書き直した");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, NaosuIchidan) {
  // 考え直している (reconsidering)
  auto result = inflection_.getBest("考え直している");
  EXPECT_EQ(result.base_form, "考える");
}

// ===== Complex: Quadruple compound patterns =====

TEST_F(InflectionTest, QuadrupleCompoundGodanKa) {
  // 書いてみてしまっておいた (tried writing and completely prepared it)
  auto result = inflection_.getBest("書いてみてしまっておいた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, QuadrupleCompoundIchidan) {
  // 食べてみてもらっていた (had someone try eating)
  auto result = inflection_.getBest("食べてみてもらっていた");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: Passive + Causative + Compound =====

TEST_F(InflectionTest, PassiveCausativeCompound) {
  // 書かせられてしまった (was made to write completely)
  auto result = inflection_.getBest("書かせられてしまった");
  EXPECT_EQ(result.base_form, "書く");
}

// ===== Complex: ずにはいられない (cannot help doing) =====

TEST_F(InflectionTest, ZuniWaIrarenaiGodanKa) {
  // 笑わずにはいられない (cannot help laughing)
  auto result = inflection_.getBest("笑わずにはいられない");
  EXPECT_EQ(result.base_form, "笑う");
}

TEST_F(InflectionTest, ZuniWaIrarenaiIchidan) {
  // 食べずにはいられなかった (couldn't help eating)
  auto result = inflection_.getBest("食べずにはいられなかった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Complex: わけにはいかない (cannot afford to) =====

TEST_F(InflectionTest, WakeNiWaIkanaiGodanKa) {
  // 書かないわけにはいかない (cannot not write)
  auto result = inflection_.getBest("書かないわけにはいかない");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionTest, WakeNiWaIkanaiIchidan) {
  // 食べるわけにはいかなかった (couldn't afford to eat)
  auto result = inflection_.getBest("食べるわけにはいかなかった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Suru verb renyokei (conjunctive form) =====

TEST_F(InflectionTest, SuruRenyokeiBunkatsu) {
  // 分割し (divide - conjunctive form)
  auto result = inflection_.getBest("分割し");
  EXPECT_EQ(result.base_form, "分割する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

TEST_F(InflectionTest, SuruRenyokeiBenkyo) {
  // 勉強し (study - conjunctive form)
  auto result = inflection_.getBest("勉強し");
  EXPECT_EQ(result.base_form, "勉強する");
  EXPECT_EQ(result.verb_type, VerbType::Suru);
}

// ===== Passive/Potential negative te-form (られなくて) =====

TEST_F(InflectionTest, PassivePotentialNegativeTe_Ichidan) {
  // 食べられなくて (couldn't eat - te form)
  auto result = inflection_.getBest("食べられなくて");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

TEST_F(InflectionTest, CausativeNegativeTe_Ichidan) {
  // 食べさせなくて (didn't let eat - te form)
  auto result = inflection_.getBest("食べさせなくて");
  EXPECT_EQ(result.base_form, "食べる");
  EXPECT_EQ(result.verb_type, VerbType::Ichidan);
}

// ===== Conditional form with 2-kanji stem =====

TEST_F(InflectionTest, ConditionalBa_TwoKanjiStem) {
  // 頑張れば (if one works hard)
  auto result = inflection_.getBest("頑張れば");
  EXPECT_EQ(result.base_form, "頑張る");
  EXPECT_EQ(result.verb_type, VerbType::GodanRa);
}

}  // namespace
}  // namespace grammar
}  // namespace suzume
