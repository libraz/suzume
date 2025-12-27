
// Inflection tests: Complex patterns (colloquial, obligation, attempt,
// ability, casual, tari, nagara, sugiru, etc.)

#include "grammar/inflection.h"

#include <gtest/gtest.h>

namespace suzume::grammar {
namespace {

class InflectionComplexTest : public ::testing::Test {
 protected:
  Inflection inflection_;
};

// ===== Colloquial chau/jau forms =====

TEST_F(InflectionComplexTest, ColloquialChattaIchidan) {
  auto result = inflection_.getBest("食べちゃった");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionComplexTest, ColloquialJattaGodanMa) {
  auto result = inflection_.getBest("読んじゃった");
  EXPECT_EQ(result.base_form, "読む");
}

TEST_F(InflectionComplexTest, ColloquialChauIchidan) {
  auto result = inflection_.getBest("忘れちゃう");
  EXPECT_EQ(result.base_form, "忘れる");
}

// ===== naitoikenai/nakereba naranai =====

TEST_F(InflectionComplexTest, NaiToIkenaiGodanKa) {
  auto result = inflection_.getBest("書かないといけない");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, NakerebaNaranaiIchidan) {
  auto result = inflection_.getBest("食べなければならない");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== youtosuru =====

TEST_F(InflectionComplexTest, YouToSuruGodanKa) {
  auto result = inflection_.getBest("書こうとした");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, YouToSuruIchidan) {
  auto result = inflection_.getBest("食べようとしている");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== kotogadekiru =====

TEST_F(InflectionComplexTest, KotoGaDekiruGodanKa) {
  auto result = inflection_.getBest("書くことができた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, KotoGaDekiruIchidan) {
  auto result = inflection_.getBest("食べることができない");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== Casual explanatory forms =====

TEST_F(InflectionComplexTest, CasualNdaGodanKa) {
  auto result = inflection_.getBest("書くんだ");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, CasualNdamonIchidan) {
  auto result = inflection_.getBest("食べるんだもん");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionComplexTest, CasualTandamonGodanKa) {
  auto result = inflection_.getBest("書いたんだもん");
  EXPECT_EQ(result.base_form, "書く");
}

// ===== tari form =====

TEST_F(InflectionComplexTest, TariFormGodanKa) {
  auto result = inflection_.getBest("書いたりした");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, TariFormIchidan) {
  auto result = inflection_.getBest("食べたりする");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== nagara form =====

TEST_F(InflectionComplexTest, NagaraFormGodanKa) {
  auto result = inflection_.getBest("書きながら");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, NagaraFormIchidan) {
  auto result = inflection_.getBest("食べながら");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== sugiru =====

TEST_F(InflectionComplexTest, SugiruGodanKa) {
  auto result = inflection_.getBest("書きすぎた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, SugiruIchidan) {
  auto result = inflection_.getBest("食べすぎている");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionComplexTest, SugiruIAdjective) {
  auto result = inflection_.getBest("難しすぎる");
  EXPECT_EQ(result.base_form, "難しい");
}

TEST_F(InflectionComplexTest, SugiruIAdjectiveTe) {
  auto result = inflection_.getBest("難しすぎて");
  EXPECT_EQ(result.base_form, "難しい");
}

TEST_F(InflectionComplexTest, SugiruIAdjectiveTa) {
  auto result = inflection_.getBest("高すぎた");
  EXPECT_EQ(result.base_form, "高い");
}

// ===== yasui/nikui =====

TEST_F(InflectionComplexTest, YasuiGodanKa) {
  auto result = inflection_.getBest("書きやすい");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, NikuiIchidan) {
  auto result = inflection_.getBest("食べにくかった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== sezaruwoenai =====

TEST_F(InflectionComplexTest, SezaruWoEnaiGodanKa) {
  auto result = inflection_.getBest("書かざるを得ない");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, SezaruWoEnaiIchidan) {
  auto result = inflection_.getBest("食べざるを得なかった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== tewaikenai/tewanaranaai =====

TEST_F(InflectionComplexTest, TeWaIkenaiGodanKa) {
  auto result = inflection_.getBest("書いてはいけない");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, TeWaNaranaiIchidan) {
  auto result = inflection_.getBest("食べてはならない");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== temoii/temokamawanai =====

TEST_F(InflectionComplexTest, TemoIiGodanKa) {
  auto result = inflection_.getBest("書いてもいい");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, TemoKamawanaiIchidan) {
  auto result = inflection_.getBest("食べてもかまわない");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== beki =====

TEST_F(InflectionComplexTest, BekiGodanKa) {
  auto result = inflection_.getBest("書くべきだ");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, BekiIchidan) {
  auto result = inflection_.getBest("食べるべきだった");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionComplexTest, BekiSuru) {
  auto result = inflection_.getBest("すべきではない");
  EXPECT_EQ(result.base_form, "する");
}

// ===== tokoroda =====

TEST_F(InflectionComplexTest, TokorodaAboutToGodanKa) {
  auto result = inflection_.getBest("書くところだ");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, TokorodaJustDidIchidan) {
  auto result = inflection_.getBest("食べたところだ");
  EXPECT_EQ(result.base_form, "食べる");
}

TEST_F(InflectionComplexTest, TokorodaProgressiveGodanMa) {
  auto result = inflection_.getBest("読んでいるところだった");
  EXPECT_EQ(result.base_form, "読む");
}

// ===== bakari =====

TEST_F(InflectionComplexTest, BakariJustDidGodanKa) {
  auto result = inflection_.getBest("書いたばかりだ");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, BakariIchidan) {
  auto result = inflection_.getBest("食べたばかりなのに");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== ppanashi =====

TEST_F(InflectionComplexTest, PpanashiGodanKa) {
  auto result = inflection_.getBest("開けっぱなしだ");
  EXPECT_EQ(result.base_form, "開ける");
}

TEST_F(InflectionComplexTest, PpanashiGodanRa) {
  auto result = inflection_.getBest("出しっぱなしにする");
  EXPECT_EQ(result.base_form, "出す");
}

// ===== kakeru =====

TEST_F(InflectionComplexTest, KakeruGodanKa) {
  auto result = inflection_.getBest("書きかけた");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, KakeruIchidan) {
  auto result = inflection_.getBest("食べかけている");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== dasu =====

TEST_F(InflectionComplexTest, DasuGodanKa) {
  auto result = inflection_.getBest("書き出した");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, DasuGodanMa) {
  auto result = inflection_.getBest("読み出して");
  EXPECT_EQ(result.base_form, "読む");
}

// ===== owaru/oeru =====

TEST_F(InflectionComplexTest, OwaruGodanKa) {
  auto result = inflection_.getBest("書き終わった");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, OeruIchidan) {
  auto result = inflection_.getBest("食べ終えた");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== tsuzukeru =====

TEST_F(InflectionComplexTest, TsuzukeruGodanKa) {
  auto result = inflection_.getBest("書き続けている");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, TsuzukeruIchidan) {
  auto result = inflection_.getBest("食べ続けた");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== naosu =====

TEST_F(InflectionComplexTest, NaosuGodanKa) {
  auto result = inflection_.getBest("書き直した");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, NaosuIchidan) {
  auto result = inflection_.getBest("考え直している");
  EXPECT_EQ(result.base_form, "考える");
}

// ===== zuniwairareanai =====

TEST_F(InflectionComplexTest, ZuniWaIrarenaiGodanKa) {
  auto result = inflection_.getBest("笑わずにはいられない");
  EXPECT_EQ(result.base_form, "笑う");
}

TEST_F(InflectionComplexTest, ZuniWaIrarenaiIchidan) {
  auto result = inflection_.getBest("食べずにはいられなかった");
  EXPECT_EQ(result.base_form, "食べる");
}

// ===== wakeniwaikanaai =====

TEST_F(InflectionComplexTest, WakeNiWaIkanaiGodanKa) {
  auto result = inflection_.getBest("書かないわけにはいかない");
  EXPECT_EQ(result.base_form, "書く");
}

TEST_F(InflectionComplexTest, WakeNiWaIkanaiIchidan) {
  auto result = inflection_.getBest("食べるわけにはいかなかった");
  EXPECT_EQ(result.base_form, "食べる");
}

}  // namespace
}  // namespace suzume::grammar
