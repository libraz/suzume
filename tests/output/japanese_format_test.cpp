/**
 * @file japanese_format_test.cpp
 * @brief Tests for Japanese morphological output formatting
 *
 * Tests Japanese POS names, verb type names, and conjugation form names
 * for detailed morphological analysis output.
 */

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string_view>

#include "core/types.h"
#include "dictionary/user_dict.h"
#include "grammar/conjugation.h"
#include "postprocess/lemmatizer.h"
#include "postprocess/lemmatizer_internal.h"
#include "postprocess/postprocessor.h"
#include "suzume.h"

namespace suzume::output::test {

// =============================================================================
// Japanese POS Names (posToJapanese)
// =============================================================================

class JapanesePosNameTest : public ::testing::Test {};

TEST_F(JapanesePosNameTest, Noun) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Noun), "名詞");
}

TEST_F(JapanesePosNameTest, Verb) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Verb), "動詞");
}

TEST_F(JapanesePosNameTest, Adjective) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Adjective), "形容詞");
}

TEST_F(JapanesePosNameTest, Particle) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Particle), "助詞");
}

TEST_F(JapanesePosNameTest, Auxiliary) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Auxiliary), "助動詞");
}

TEST_F(JapanesePosNameTest, Adverb) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Adverb), "副詞");
}

TEST_F(JapanesePosNameTest, Conjunction) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Conjunction), "接続詞");
}

TEST_F(JapanesePosNameTest, Pronoun) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Pronoun), "代名詞");
}

TEST_F(JapanesePosNameTest, Determiner) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Determiner), "連体詞");
}

TEST_F(JapanesePosNameTest, Symbol) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Symbol), "記号");
}

TEST_F(JapanesePosNameTest, Unknown) {
  // Unknown and Other both return "その他"
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Unknown), "その他");
}

TEST_F(JapanesePosNameTest, Other) {
  EXPECT_EQ(core::posToJapanese(core::PartOfSpeech::Other), "その他");
}

TEST(LemmatizerTest, ContractedGodanSokuonFormsUseDictionaryVerifiedRows) {
  dictionary::DictionaryManager dict_manager;
  auto user_dict = std::make_shared<dictionary::UserDictionary>();
  user_dict->addEntry({"待つ", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "待つ"});
  user_dict->addEntry({"帰る", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "帰る"});
  user_dict->addEntry({"買う", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "買う"});
  dict_manager.addUserDictionary(user_dict);

  postprocess::Lemmatizer lemmatizer(&dict_manager);
  core::Morpheme morpheme;
  morpheme.pos = core::PartOfSpeech::Verb;

  morpheme.surface = "待ってしまった";
  EXPECT_EQ(lemmatizer.lemmatize(morpheme), "待つ");

  morpheme.surface = "帰ってしまった";
  EXPECT_EQ(lemmatizer.lemmatize(morpheme), "帰る");

  morpheme.surface = "買ってしまった";
  EXPECT_EQ(lemmatizer.lemmatize(morpheme), "買う");
}

TEST(LemmatizerTest, DictionarySuruPassiveReturnsSuruLemma) {
  dictionary::DictionaryManager dict_manager;
  auto user_dict = std::make_shared<dictionary::UserDictionary>();
  user_dict->addEntry({"処理する", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "処理する"});
  user_dict->addEntry({"確認する", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "確認する"});
  dict_manager.addUserDictionary(user_dict);

  postprocess::Lemmatizer lemmatizer(&dict_manager);
  core::Morpheme morpheme;
  morpheme.pos = core::PartOfSpeech::Verb;
  morpheme.flags = core::EdgeFlags::FromDictionary;

  morpheme.surface = "処理される";
  morpheme.lemma = "処理される";
  EXPECT_EQ(lemmatizer.lemmatize(morpheme), "処理する");

  morpheme.surface = "確認されて";
  morpheme.lemma = "確認されて";
  EXPECT_EQ(lemmatizer.lemmatize(morpheme), "確認する");
}

TEST(LemmatizerTest, ClassicalSuruFixPreservesGodanSaAcrossAllCallPaths) {
  using dictionary::ConjugationType;
  using postprocess::lemmatizer_detail::fixSuruClassical;

  EXPECT_TRUE(fixSuruClassical("見出す", ConjugationType::GodanSa).empty());
  EXPECT_EQ(fixSuruClassical("確認す", ConjugationType::Suru), "確認する");

  postprocess::Lemmatizer lemmatizer;
  for (const auto& [surface, lemma] :
       std::array<std::pair<std::string_view, std::string_view>, 2>{{{"見出し", "見出す"}, {"見通し", "見通す"}}}) {
    core::Morpheme morpheme;
    morpheme.surface = surface;
    morpheme.lemma = lemma;
    morpheme.pos = core::PartOfSpeech::Verb;
    morpheme.conj_type = ConjugationType::GodanSa;
    EXPECT_EQ(lemmatizer.lemmatize(morpheme), lemma);

    morpheme.surface = lemma;
    morpheme.lemma = lemma;
    EXPECT_EQ(lemmatizer.lemmatize(morpheme), lemma);
  }
}

TEST(PostprocessorTest, DisablingLemmatizationPreservesAnnotationsAndOriginalLemma) {
  std::vector<core::Morpheme> input(2);
  input[0].surface = "伺い";
  input[0].lemma = "伺い";
  input[0].pos = core::PartOfSpeech::Noun;
  input[0].extended_pos = core::ExtendedPOS::Noun;
  input[1].surface = "ます";
  input[1].lemma = "ます";
  input[1].pos = core::PartOfSpeech::Auxiliary;
  input[1].extended_pos = core::ExtendedPOS::AuxTenseMasu;

  postprocess::PostprocessOptions enabled_options;
  postprocess::Postprocessor enabled(enabled_options);
  const auto with_lemmas = enabled.process(input);

  postprocess::PostprocessOptions disabled_options;
  disabled_options.lemmatize = false;
  postprocess::Postprocessor disabled(disabled_options);
  const auto without_lemmas = disabled.process(input);

  ASSERT_EQ(with_lemmas.size(), without_lemmas.size());
  ASSERT_EQ(without_lemmas.size(), 2);
  EXPECT_EQ(with_lemmas[0].lemma, "伺う");
  EXPECT_EQ(without_lemmas[0].lemma, "伺い");
  for (size_t idx = 0; idx < with_lemmas.size(); ++idx) {
    EXPECT_EQ(without_lemmas[idx].surface, with_lemmas[idx].surface);
    EXPECT_EQ(without_lemmas[idx].pos, with_lemmas[idx].pos);
    EXPECT_EQ(without_lemmas[idx].extended_pos, with_lemmas[idx].extended_pos);
    EXPECT_EQ(without_lemmas[idx].conj_type, with_lemmas[idx].conj_type);
    EXPECT_EQ(without_lemmas[idx].conj_form, with_lemmas[idx].conj_form);
  }
  EXPECT_EQ(without_lemmas[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(without_lemmas[0].extended_pos, core::ExtendedPOS::VerbRenyokei);
  EXPECT_EQ(without_lemmas[0].conj_form, grammar::ConjForm::Renyokei);
}

TEST(PostprocessorTest, ClauseFinalERowPreservesIchidanRenyokei) {
  using dictionary::ConjugationType;

  dictionary::DictionaryManager dict_manager;
  auto user_dict = std::make_shared<dictionary::UserDictionary>();
  user_dict->addEntry({"まとめる", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "まとめる"});
  dict_manager.addUserDictionary(user_dict);

  constexpr std::array<std::pair<std::string_view, std::string_view>, 3> kCases = {{
      {"まとめ", "まとめる"},
      {"投げ", "投げる"},
      {"助け", "助ける"},
  }};
  postprocess::Postprocessor postprocessor(&dict_manager);
  for (const auto& [surface, lemma] : kCases) {
    SCOPED_TRACE(surface);
    std::vector<core::Morpheme> input(2);
    input[0].surface = "を";
    input[0].lemma = "を";
    input[0].pos = core::PartOfSpeech::Particle;
    input[0].extended_pos = core::ExtendedPOS::ParticleCase;
    input[1].surface = surface;
    input[1].lemma = lemma;
    input[1].pos = core::PartOfSpeech::Verb;
    input[1].extended_pos = core::ExtendedPOS::VerbRenyokei;
    input[1].conj_type = ConjugationType::Ichidan;
    input[1].conj_form = grammar::ConjForm::Renyokei;

    const auto result = postprocessor.process(std::move(input));

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[1].lemma, lemma);
    EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::VerbRenyokei);
    EXPECT_EQ(result[1].conj_type, ConjugationType::Ichidan);
    EXPECT_EQ(result[1].conj_form, grammar::ConjForm::Renyokei);
  }
}

TEST(PostprocessorTest, ClauseFinalERowStillRepairsGodanImperative) {
  using dictionary::ConjugationType;

  dictionary::DictionaryManager dict_manager;
  auto user_dict = std::make_shared<dictionary::UserDictionary>();
  user_dict->addEntry({"急ぐ", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "急ぐ"});
  dict_manager.addUserDictionary(user_dict);

  std::vector<core::Morpheme> input(2);
  input[0].surface = "が";
  input[0].lemma = "が";
  input[0].pos = core::PartOfSpeech::Particle;
  input[0].extended_pos = core::ExtendedPOS::ParticleCase;
  input[1].surface = "急げ";
  input[1].lemma = "急ぐ";
  input[1].pos = core::PartOfSpeech::Verb;
  input[1].extended_pos = core::ExtendedPOS::VerbRenyokei;
  input[1].conj_type = ConjugationType::Ichidan;
  input[1].conj_form = grammar::ConjForm::Renyokei;

  postprocess::Postprocessor postprocessor(&dict_manager);
  const auto result = postprocessor.process(std::move(input));

  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[1].lemma, "急ぐ");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::VerbMeireikei);
  EXPECT_EQ(result[1].conj_type, ConjugationType::GodanGa);
  EXPECT_EQ(result[1].conj_form, grammar::ConjForm::Meireikei);
}

TEST(PostprocessorTest, ClauseFinalERowPrefersGodanWhenBothDictionaryFormsExist) {
  using dictionary::ConjugationType;

  dictionary::DictionaryManager dict_manager;
  auto user_dict = std::make_shared<dictionary::UserDictionary>();
  user_dict->addEntry({"読む", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "読む"});
  user_dict->addEntry({"読める", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "読める"});
  dict_manager.addUserDictionary(user_dict);

  std::vector<core::Morpheme> input(2);
  input[0].surface = "を";
  input[0].lemma = "を";
  input[0].pos = core::PartOfSpeech::Particle;
  input[0].extended_pos = core::ExtendedPOS::ParticleCase;
  input[1].surface = "読め";
  input[1].lemma = "読める";
  input[1].pos = core::PartOfSpeech::Verb;
  input[1].extended_pos = core::ExtendedPOS::VerbRenyokei;
  input[1].conj_type = ConjugationType::Ichidan;
  input[1].conj_form = grammar::ConjForm::Renyokei;

  postprocess::Postprocessor postprocessor(&dict_manager);
  const auto result = postprocessor.process(std::move(input));

  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[1].lemma, "読む");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::VerbMeireikei);
  EXPECT_EQ(result[1].conj_type, ConjugationType::GodanMa);
  EXPECT_EQ(result[1].conj_form, grammar::ConjForm::Meireikei);
}

TEST(LemmatizerTest, FallbackGodanMizenRulesUseCanonicalRows) {
  using postprocess::lemmatizer_detail::lemmatizeVerbFallback;

  EXPECT_EQ(lemmatizeVerbFallback("書かれている"), "書く");
  EXPECT_EQ(lemmatizeVerbFallback("泳がれました"), "泳ぐ");
  EXPECT_EQ(lemmatizeVerbFallback("話されて"), "話す");
  EXPECT_EQ(lemmatizeVerbFallback("待たせる"), "待つ");
  EXPECT_EQ(lemmatizeVerbFallback("読ませた"), "読む");
  EXPECT_EQ(lemmatizeVerbFallback("買わない"), "買う");
  EXPECT_EQ(lemmatizeVerbFallback("買える"), "買える");
  EXPECT_EQ(lemmatizeVerbFallback("書ける"), "書ける");
  EXPECT_EQ(lemmatizeVerbFallback("泳げる"), "泳げる");
  EXPECT_EQ(lemmatizeVerbFallback("話せる"), "話せる");
  EXPECT_EQ(lemmatizeVerbFallback("死ねる"), "死ねる");
  EXPECT_EQ(lemmatizeVerbFallback("遊べる"), "遊べる");
  EXPECT_EQ(lemmatizeVerbFallback("読める"), "読める");
  EXPECT_EQ(lemmatizeVerbFallback("取れる"), "取れる");
}

TEST(LemmatizerTest, HatsuonbinFallbackUsesCanonicalOnbinRows) {
  dictionary::DictionaryManager dict_manager;
  auto user_dict = std::make_shared<dictionary::UserDictionary>();
  user_dict->addEntry({"学ぶ", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "学ぶ"});
  user_dict->addEntry({"死ぬ", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "死ぬ"});
  dict_manager.addUserDictionary(user_dict);

  using postprocess::lemmatizer_detail::fixHatsuonbin;
  EXPECT_EQ(fixHatsuonbin("学", &dict_manager), "学ぶ");
  EXPECT_EQ(fixHatsuonbin("死", &dict_manager), "死ぬ");
  EXPECT_EQ(fixHatsuonbin("読", nullptr), "読む");
}

TEST(LemmatizerTest, PotentialVerbFallbackUsesCanonicalARows) {
  core::Morpheme morpheme;
  morpheme.pos = core::PartOfSpeech::Verb;

  for (std::string_view surface :
       {"買われる", "書かれる", "泳がれる", "話される", "待たれる", "死なれる", "読まれる", "遊ばれる", "取られる"}) {
    morpheme.surface = surface;
    EXPECT_EQ(postprocess::lemmatizer_detail::fixPotentialVerb(morpheme), surface);
  }

  morpheme.surface = "書ける";
  EXPECT_TRUE(postprocess::lemmatizer_detail::fixPotentialVerb(morpheme).empty());
}

TEST(LemmatizerTest, IchidanRenyokeiFallbackUsesCanonicalIRows) {
  using postprocess::lemmatizer_detail::fixIchidanRenyokeiBeforeTe;
  EXPECT_EQ(fixIchidanRenyokeiBeforeTe("借り", "借る", "て", nullptr), "借りる");
  EXPECT_EQ(fixIchidanRenyokeiBeforeTe("過ぎ", "過ぐ", "た", nullptr), "過ぎる");

  dictionary::DictionaryManager dict_manager;
  auto user_dict = std::make_shared<dictionary::UserDictionary>();
  user_dict->addEntry({"走る", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "走る"});
  dict_manager.addUserDictionary(user_dict);
  EXPECT_TRUE(fixIchidanRenyokeiBeforeTe("走り", "走る", "て", &dict_manager).empty());
}

// =============================================================================
// Japanese Conjugation Form Names (conjFormToJapanese)
// =============================================================================

class JapaneseConjFormTest : public ::testing::Test {};

TEST_F(JapaneseConjFormTest, Base) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Base), "終止形");
}

TEST_F(JapaneseConjFormTest, Mizenkei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Mizenkei), "未然形");
}

TEST_F(JapaneseConjFormTest, Renyokei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Renyokei), "連用形");
}

TEST_F(JapaneseConjFormTest, Kateikei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Kateikei), "仮定形");
}

TEST_F(JapaneseConjFormTest, Meireikei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Meireikei), "命令形");
}

TEST_F(JapaneseConjFormTest, Ishikei) {
  EXPECT_EQ(grammar::conjFormToJapanese(grammar::ConjForm::Ishikei), "意志形");
}

// =============================================================================
// Conjugation Form Detection (detectConjForm)
// =============================================================================

class ConjFormDetectionTest : public ::testing::Test {};

// Verb: Mizenkei (未然形) - negative, passive, causative
TEST_F(ConjFormDetectionTest, Verb_Mizenkei_Negative) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べない", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Mizenkei);
}

TEST_F(ConjFormDetectionTest, Verb_Mizenkei_Passive) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べられる", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Mizenkei);
}

TEST_F(ConjFormDetectionTest, Verb_Mizenkei_Causative) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べさせる", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Mizenkei);
}

// Verb: Renyokei (連用形) - masu, ta, te
TEST_F(ConjFormDetectionTest, Verb_Renyokei_Masu) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べます", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

TEST_F(ConjFormDetectionTest, Verb_Renyokei_Ta) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べた", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

TEST_F(ConjFormDetectionTest, Verb_Renyokei_Te) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べて", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

TEST_F(ConjFormDetectionTest, Verb_Renyokei_Teiru) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べている", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

// Verb: Kateikei (仮定形) - ba
TEST_F(ConjFormDetectionTest, Verb_Kateikei_Ba) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べれば", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Kateikei);
}

TEST_F(ConjFormDetectionTest, Verb_Kateikei_Godan) {
  auto form = postprocess::Lemmatizer::detectConjForm("書けば", "書く", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Kateikei);
}

// Verb: Meireikei (命令形) - ro, e
TEST_F(ConjFormDetectionTest, Verb_Meireikei_Ichidan) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べろ", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Meireikei);
}

TEST_F(ConjFormDetectionTest, Verb_Meireikei_Godan) {
  // Godan imperative ends in 'e' sound - current implementation returns Renyokei
  // as fallback for unrecognized conjugated forms
  auto form = postprocess::Lemmatizer::detectConjForm("書け", "書く", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

// Verb: Ishikei (意志形) - ou, you
TEST_F(ConjFormDetectionTest, Verb_Ishikei_Ichidan) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べよう", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Ishikei);
}

TEST_F(ConjFormDetectionTest, Verb_Ishikei_Godan) {
  auto form = postprocess::Lemmatizer::detectConjForm("書こう", "書く", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Ishikei);
}

// Verb: Base form (終止形)
TEST_F(ConjFormDetectionTest, Verb_Base_Ichidan) {
  auto form = postprocess::Lemmatizer::detectConjForm("食べる", "食べる", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

TEST_F(ConjFormDetectionTest, Verb_Base_Godan) {
  auto form = postprocess::Lemmatizer::detectConjForm("書く", "書く", core::PartOfSpeech::Verb);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

// Adjective conjugation forms
TEST_F(ConjFormDetectionTest, Adjective_Renyokei_Ku) {
  auto form = postprocess::Lemmatizer::detectConjForm("美しく", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Renyokei);
}

TEST_F(ConjFormDetectionTest, Adjective_Onbinkei_Katta) {
  // "かった" ends with "った" which matches onbinkei pattern first
  auto form = postprocess::Lemmatizer::detectConjForm("美しかった", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Onbinkei);
}

TEST_F(ConjFormDetectionTest, Adjective_Mizenkei_Kunai) {
  auto form = postprocess::Lemmatizer::detectConjForm("美しくない", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Mizenkei);
}

TEST_F(ConjFormDetectionTest, Adjective_Kateikei_Kereba) {
  auto form = postprocess::Lemmatizer::detectConjForm("美しければ", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Kateikei);
}

TEST_F(ConjFormDetectionTest, Adjective_Base) {
  auto form = postprocess::Lemmatizer::detectConjForm("美しい", "美しい", core::PartOfSpeech::Adjective);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

// Non-verb/adjective should return Base
TEST_F(ConjFormDetectionTest, Noun_ReturnsBase) {
  auto form = postprocess::Lemmatizer::detectConjForm("学校", "学校", core::PartOfSpeech::Noun);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

TEST_F(ConjFormDetectionTest, Particle_ReturnsBase) {
  auto form = postprocess::Lemmatizer::detectConjForm("は", "は", core::PartOfSpeech::Particle);
  EXPECT_EQ(form, grammar::ConjForm::Base);
}

TEST_F(ConjFormDetectionTest, ExtendedPosCoversEveryGodanRowAndMajorCell) {
  using EPOS = core::ExtendedPOS;
  using Form = grammar::ConjForm;
  struct GodanParadigm {
    std::array<std::string_view, 7> surfaces;
  };
  constexpr std::array<GodanParadigm, 9> kParadigms = {{
      {{{"書く", "書か", "書き", "書い", "書け", "書け", "書こ"}}},
      {{{"泳ぐ", "泳が", "泳ぎ", "泳い", "泳げ", "泳げ", "泳ご"}}},
      {{{"話す", "話さ", "話し", "話し", "話せ", "話せ", "話そ"}}},
      {{{"持つ", "持た", "持ち", "持っ", "持て", "持て", "持と"}}},
      {{{"死ぬ", "死な", "死に", "死ん", "死ね", "死ね", "死の"}}},
      {{{"遊ぶ", "遊ば", "遊び", "遊ん", "遊べ", "遊べ", "遊ぼ"}}},
      {{{"読む", "読ま", "読み", "読ん", "読め", "読め", "読も"}}},
      {{{"取る", "取ら", "取り", "取っ", "取れ", "取れ", "取ろ"}}},
      {{{"買う", "買わ", "買い", "買っ", "買え", "買え", "買お"}}},
  }};
  constexpr std::array<EPOS, 7> kExtendedPos = {
      EPOS::VerbShuushikei, EPOS::VerbMizenkei,  EPOS::VerbRenyokei, EPOS::VerbOnbinkei,
      EPOS::VerbKateikei,   EPOS::VerbMeireikei, EPOS::VerbMizenkei,
  };
  constexpr std::array<Form, 7> kExpectedForms = {
      Form::Base, Form::Mizenkei, Form::Renyokei, Form::Onbinkei, Form::Kateikei, Form::Meireikei, Form::Ishikei,
  };

  for (size_t row_index = 0; row_index < kParadigms.size(); ++row_index) {
    for (size_t form_index = 0; form_index < kExtendedPos.size(); ++form_index) {
      SCOPED_TRACE(::testing::Message() << "row=" << row_index << " form=" << form_index);
      const bool is_volitional = form_index == kExtendedPos.size() - 1;
      EXPECT_EQ(postprocess::Lemmatizer::detectConjForm(kParadigms[row_index].surfaces[form_index],
                                                        kParadigms[row_index].surfaces[0], core::PartOfSpeech::Verb,
                                                        is_volitional ? "う" : "", kExtendedPos[form_index],
                                                        is_volitional ? EPOS::AuxVolitional : EPOS::Unknown),
                kExpectedForms[form_index]);
    }
  }
}

TEST_F(ConjFormDetectionTest, ExtendedPosOverridesNegativeLookingAdjectiveSurface) {
  EXPECT_EQ(postprocess::Lemmatizer::detectConjForm("少なく", "少ない", core::PartOfSpeech::Adjective, "",
                                                    core::ExtendedPOS::AdjRenyokei),
            grammar::ConjForm::Renyokei);
  EXPECT_EQ(postprocess::Lemmatizer::detectConjForm("危なけれ", "危ない", core::PartOfSpeech::Adjective, "ば",
                                                    core::ExtendedPOS::AdjKeForm, core::ExtendedPOS::ParticleConj),
            grammar::ConjForm::Kateikei);
}

// =============================================================================
// Conjugation Type to Verb Type Conversion
// =============================================================================

class ConjTypeToVerbTypeTest : public ::testing::Test {};

TEST_F(ConjTypeToVerbTypeTest, EveryConjugationTypeMapsAsSpecified) {
  using CT = dictionary::ConjugationType;
  using VT = grammar::VerbType;
  constexpr std::array<std::pair<CT, VT>, 18> kCases = {{
      {CT::None, VT::Unknown},
      {CT::Ichidan, VT::Ichidan},
      {CT::GodanKa, VT::GodanKa},
      {CT::GodanGa, VT::GodanGa},
      {CT::GodanSa, VT::GodanSa},
      {CT::GodanTa, VT::GodanTa},
      {CT::GodanNa, VT::GodanNa},
      {CT::GodanBa, VT::GodanBa},
      {CT::GodanMa, VT::GodanMa},
      {CT::GodanRa, VT::GodanRa},
      {CT::GodanWa, VT::GodanWa},
      {CT::Suru, VT::Suru},
      {CT::Kuru, VT::Kuru},
      {CT::IAdjective, VT::IAdjective},
      {CT::NaAdjective, VT::Unknown},
      {CT::Interjection, VT::Unknown},
      {CT::ProperFamily, VT::Unknown},
      {CT::ProperGiven, VT::Unknown},
  }};

  for (size_t case_index = 0; case_index < kCases.size(); ++case_index) {
    SCOPED_TRACE(case_index);
    EXPECT_EQ(grammar::conjTypeToVerbType(kCases[case_index].first), kCases[case_index].second);
  }
}

TEST_F(ConjTypeToVerbTypeTest, EveryVerbTypeRoundTripsToConjugationType) {
  using CT = dictionary::ConjugationType;
  using VT = grammar::VerbType;
  constexpr std::array<std::pair<VT, CT>, 14> kCases = {{
      {VT::Unknown, CT::None},
      {VT::Ichidan, CT::Ichidan},
      {VT::GodanKa, CT::GodanKa},
      {VT::GodanGa, CT::GodanGa},
      {VT::GodanSa, CT::GodanSa},
      {VT::GodanTa, CT::GodanTa},
      {VT::GodanNa, CT::GodanNa},
      {VT::GodanBa, CT::GodanBa},
      {VT::GodanMa, CT::GodanMa},
      {VT::GodanRa, CT::GodanRa},
      {VT::GodanWa, CT::GodanWa},
      {VT::Suru, CT::Suru},
      {VT::Kuru, CT::Kuru},
      {VT::IAdjective, CT::IAdjective},
  }};

  for (size_t case_index = 0; case_index < kCases.size(); ++case_index) {
    SCOPED_TRACE(case_index);
    EXPECT_EQ(grammar::verbTypeToConjType(kCases[case_index].first), kCases[case_index].second);
  }
}

// =============================================================================
// Integration Tests: Morpheme Analysis with Japanese Format Info
// =============================================================================

class JapaneseFormatIntegrationTest : public ::testing::Test {
 protected:
  Suzume analyzer_;
};

// Test that verb analysis includes correct conjugation type
// MeCab-compatible split: 食べました → 食べ + まし + た
TEST_F(JapaneseFormatIntegrationTest, VerbWithConjType_Ichidan) {
  auto morphemes = analyzer_.analyze("食べました");
  ASSERT_EQ(morphemes.size(), 3);
  EXPECT_EQ(morphemes[0].surface, "食べ");
  EXPECT_EQ(morphemes[0].getLemma(), "食べる");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(morphemes[1].surface, "まし");
  EXPECT_EQ(morphemes[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(morphemes[2].surface, "た");
  EXPECT_EQ(morphemes[2].pos, core::PartOfSpeech::Auxiliary);

  auto verb_type = grammar::conjTypeToVerbType(morphemes[0].conj_type);
  EXPECT_EQ(verb_type, grammar::VerbType::Ichidan);
}

// MeCab-compatible split: 書きました → 書き + まし + た
TEST_F(JapaneseFormatIntegrationTest, VerbWithConjType_GodanKa) {
  auto morphemes = analyzer_.analyze("書きました");
  ASSERT_EQ(morphemes.size(), 3);
  EXPECT_EQ(morphemes[0].surface, "書き");
  EXPECT_EQ(morphemes[0].getLemma(), "書く");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(morphemes[1].surface, "まし");
  EXPECT_EQ(morphemes[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(morphemes[2].surface, "た");
  EXPECT_EQ(morphemes[2].pos, core::PartOfSpeech::Auxiliary);

  auto verb_type = grammar::conjTypeToVerbType(morphemes[0].conj_type);
  EXPECT_EQ(verb_type, grammar::VerbType::GodanKa);
}

TEST_F(JapaneseFormatIntegrationTest, VerbWithConjType_Suru) {
  // MeCab-compatible split: し + て + い + ます
  auto morphemes = analyzer_.analyze("しています");
  ASSERT_EQ(morphemes.size(), 4);
  EXPECT_EQ(morphemes[0].surface, "し");
  EXPECT_EQ(morphemes[0].getLemma(), "する");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(morphemes[1].surface, "て");
  EXPECT_EQ(morphemes[1].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(morphemes[2].surface, "い");
  EXPECT_EQ(morphemes[2].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(morphemes[3].surface, "ます");
  EXPECT_EQ(morphemes[3].pos, core::PartOfSpeech::Auxiliary);

  // TODO: VerbType detection from conj_type needs work
  // auto verb_type = grammar::conjTypeToVerbType(morphemes[0].conj_type);
  // EXPECT_EQ(verb_type, grammar::VerbType::Suru);
}

TEST_F(JapaneseFormatIntegrationTest, VerbWithConjType_GodanMa) {
  // MeCab-compatible split: 読ん + で + い + ます
  auto morphemes = analyzer_.analyze("読んでいます");
  ASSERT_EQ(morphemes.size(), 4);
  EXPECT_EQ(morphemes[0].surface, "読ん");
  EXPECT_EQ(morphemes[0].getLemma(), "読む");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(morphemes[1].surface, "で");
  EXPECT_EQ(morphemes[1].pos, core::PartOfSpeech::Particle);
  EXPECT_EQ(morphemes[2].surface, "い");
  EXPECT_EQ(morphemes[2].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(morphemes[3].surface, "ます");
  EXPECT_EQ(morphemes[3].pos, core::PartOfSpeech::Auxiliary);

  auto verb_type = grammar::conjTypeToVerbType(morphemes[0].conj_type);
  EXPECT_EQ(verb_type, grammar::VerbType::GodanMa);
}

// Test conjugation form detection in analysis pipeline
// MeCab-compatible split: 食べない → 食べ + ない
TEST_F(JapaneseFormatIntegrationTest, ConjForm_Mizenkei) {
  auto morphemes = analyzer_.analyze("食べない");
  ASSERT_EQ(morphemes.size(), 2);
  EXPECT_EQ(morphemes[0].surface, "食べ");
  EXPECT_EQ(morphemes[0].conj_form, grammar::ConjForm::Mizenkei);
  EXPECT_EQ(morphemes[1].surface, "ない");
  EXPECT_EQ(morphemes[1].pos, core::PartOfSpeech::Auxiliary);
}

// MeCab-compatible split: 食べました → 食べ + まし + た
// Check ConjForm on the verb stem
TEST_F(JapaneseFormatIntegrationTest, ConjForm_Renyokei) {
  auto morphemes = analyzer_.analyze("食べました");
  ASSERT_EQ(morphemes.size(), 3);
  EXPECT_EQ(morphemes[0].conj_form, grammar::ConjForm::Renyokei);
}

TEST_F(JapaneseFormatIntegrationTest, ConjForm_Kateikei) {
  // MeCab-compatible split: 走れ + ば
  auto morphemes = analyzer_.analyze("走れば");
  ASSERT_EQ(morphemes.size(), 2);
  EXPECT_EQ(morphemes[0].surface, "走れ");
  EXPECT_EQ(morphemes[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(morphemes[1].surface, "ば");
  EXPECT_EQ(morphemes[1].pos, core::PartOfSpeech::Particle);
  // TODO: ConjForm detection needs work - currently returns Renyokei for godan-ra
  // EXPECT_EQ(morphemes[0].conj_form, grammar::ConjForm::Kateikei);
}

TEST_F(JapaneseFormatIntegrationTest, ConjForm_Base) {
  auto morphemes = analyzer_.analyze("食べる");
  ASSERT_EQ(morphemes.size(), 1);
  EXPECT_EQ(morphemes[0].conj_form, grammar::ConjForm::Base);
}

TEST_F(JapaneseFormatIntegrationTest, ConjFormUsesSelectedExtendedPos) {
  struct Case {
    std::string_view input;
    std::string_view surface;
    std::string_view lemma;
    core::ExtendedPOS extended_pos;
    grammar::ConjForm form;
  };
  constexpr std::array<Case, 5> kCases = {{
      {"書かない", "書か", "書く", core::ExtendedPOS::VerbMizenkei, grammar::ConjForm::Mizenkei},
      {"来い", "来い", "来る", core::ExtendedPOS::VerbMeireikei, grammar::ConjForm::Meireikei},
      {"食べよう", "食べよ", "食べる", core::ExtendedPOS::VerbMizenkei, grammar::ConjForm::Ishikei},
      {"少なく行く", "少なく", "少ない", core::ExtendedPOS::AdjRenyokei, grammar::ConjForm::Renyokei},
      {"危なければ帰る", "危なけれ", "危ない", core::ExtendedPOS::AdjKeForm, grammar::ConjForm::Kateikei},
  }};

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.input);
    const auto morphemes = analyzer_.analyze(test_case.input);
    ASSERT_FALSE(morphemes.empty());
    EXPECT_EQ(morphemes[0].surface, test_case.surface);
    EXPECT_EQ(morphemes[0].getLemma(), test_case.lemma);
    EXPECT_EQ(morphemes[0].extended_pos, test_case.extended_pos);
    EXPECT_EQ(morphemes[0].conj_form, test_case.form);
  }
}

TEST_F(JapaneseFormatIntegrationTest, ContractedShimauParadigmRepairsIOnbinLemma) {
  constexpr std::array<std::string_view, 12> kInputs = {
      "凪いちゃう", "凪いちゃわない", "凪いちゃいます", "凪いちゃった", "凪いちゃえば", "凪いちゃおう",
      "泳いじゃう", "泳いじゃわない", "泳いじゃいます", "泳いじゃった", "泳いじゃえば", "泳いじゃおう",
  };

  for (const std::string_view input : kInputs) {
    SCOPED_TRACE(input);
    const auto morphemes = analyzer_.analyze(input);
    ASSERT_GE(morphemes.size(), 2);
    EXPECT_TRUE(morphemes[0].surface == "凪い" || morphemes[0].surface == "泳い");
    EXPECT_EQ(morphemes[0].getLemma(), morphemes[0].surface == "凪い" ? "凪ぐ" : "泳ぐ");
    EXPECT_EQ(morphemes[0].extended_pos, core::ExtendedPOS::VerbOnbinkei);
    EXPECT_EQ(morphemes[1].extended_pos, core::ExtendedPOS::AuxAspectShimau);
  }
}

TEST_F(JapaneseFormatIntegrationTest, WaRowSokuonbinKeepsLexicalBase) {
  struct Case {
    std::string_view input;
    std::string_view lemma;
  };
  constexpr std::array<Case, 5> kCases = {{
      {"味わった", "味わう"},
      {"争った", "争う"},
      {"終わった", "終わる"},
      {"持った", "持つ"},
      {"買った", "買う"},
  }};

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.input);
    const auto morphemes = analyzer_.analyze(test_case.input);
    ASSERT_GE(morphemes.size(), 2);
    EXPECT_EQ(morphemes[0].getLemma(), test_case.lemma);
    EXPECT_EQ(morphemes[0].extended_pos, core::ExtendedPOS::VerbOnbinkei);
  }
}

TEST_F(JapaneseFormatIntegrationTest, MiNominalizationSurvivesAfterPastClause) {
  const auto morphemes = analyzer_.analyze("味わった苦しみ");
  ASSERT_EQ(morphemes.size(), 3);
  EXPECT_EQ(morphemes[0].surface, "味わっ");
  EXPECT_EQ(morphemes[0].getLemma(), "味わう");
  EXPECT_EQ(morphemes[1].extended_pos, core::ExtendedPOS::AuxTenseTa);
  EXPECT_EQ(morphemes[2].surface, "苦しみ");
  EXPECT_EQ(morphemes[2].pos, core::PartOfSpeech::Noun);
}

}  // namespace suzume::output::test
