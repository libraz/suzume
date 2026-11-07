#include "grammar/dictionary_expansion.h"

#include <gtest/gtest.h>

#include <algorithm>

#include "grammar/conjugation.h"

namespace suzume {
namespace grammar {
namespace {

const dictionary::DictionaryEntry* findSurface(const std::vector<dictionary::DictionaryEntry>& entries,
                                               std::string_view surface) {
  auto found =
      std::find_if(entries.begin(), entries.end(), [surface](const auto& entry) { return entry.surface == surface; });
  return found == entries.end() ? nullptr : &*found;
}

TEST(DictionaryExpansionTest, ExpandsExplicitVerbWithLemmaAndExtendedPos) {
  const dictionary::SourceEntry source{"テストする", core::PartOfSpeech::Verb, dictionary::ConjugationType::Suru, "",
                                       1};

  auto expanded = expandDictionarySourceEntry(source);

  const auto* conditional = findSurface(expanded, "テストすれば");
  ASSERT_NE(conditional, nullptr);
  EXPECT_EQ(conditional->lemma, "テストする");
  EXPECT_EQ(conditional->pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(conditional->extended_pos, core::ExtendedPOS::VerbKateikei);
}

TEST(DictionaryExpansionTest, KeepsLegacyInflectedCsvSurfaceLiteral) {
  const dictionary::SourceEntry source{"テストした", core::PartOfSpeech::Verb, dictionary::ConjugationType::None,
                                       "テストする", 1};

  auto expanded = expandDictionarySourceEntry(source);

  ASSERT_EQ(expanded.size(), 1);
  EXPECT_EQ(expanded[0].surface, "テストした");
  EXPECT_EQ(expanded[0].lemma, "テストする");
}

TEST(DictionaryExpansionTest, PreservesPlainProperNounExtendedPos) {
  dictionary::SourceEntry source{"東京", core::PartOfSpeech::Noun, dictionary::ConjugationType::None, "", 1};
  source.is_proper_noun = true;

  const auto expanded = expandDictionarySourceEntry(source);

  ASSERT_EQ(expanded.size(), 1);
  EXPECT_EQ(expanded[0].extended_pos, core::ExtendedPOS::NounProper);
}

TEST(DictionaryExpansionTest, IAdjectiveExpansionKeepsBareMorphemeBoundaries) {
  const dictionary::SourceEntry source{"高い", core::PartOfSpeech::Adjective, dictionary::ConjugationType::IAdjective,
                                       "", 1};

  const auto expanded = expandDictionarySourceEntry(source);

  const auto* conditional = findSurface(expanded, "高けれ");
  const auto* conjectural = findSurface(expanded, "高かろ");
  ASSERT_NE(conditional, nullptr);
  ASSERT_NE(conjectural, nullptr);
  EXPECT_EQ(conditional->extended_pos, core::ExtendedPOS::AdjKeForm);
  EXPECT_EQ(conjectural->extended_pos, core::ExtendedPOS::AdjMizenkei);
  EXPECT_EQ(findSurface(expanded, "高ければ"), nullptr);
  EXPECT_EQ(findSurface(expanded, "高かったら"), nullptr);
  EXPECT_EQ(findSurface(expanded, "高そう"), nullptr);
}

TEST(DictionaryExpansionTest, ExpandsRegularIAdjectiveEndingInIi) {
  const dictionary::SourceEntry source{"かわいい", core::PartOfSpeech::Adjective,
                                       dictionary::ConjugationType::IAdjective, "", 1};

  const auto expanded = expandDictionarySourceEntry(source);

  ASSERT_NE(findSurface(expanded, "かわいけれ"), nullptr);
  ASSERT_NE(findSurface(expanded, "かわいかっ"), nullptr);
}

TEST(DictionaryExpansionTest, UsesRegisteredYoiSiblingForSuppletiveIiAdjective) {
  const dictionary::SourceEntry colloquial{"かっこいい", core::PartOfSpeech::Adjective,
                                           dictionary::ConjugationType::IAdjective, "", 1};
  const dictionary::SourceEntry regular{"かっこよい", core::PartOfSpeech::Adjective,
                                        dictionary::ConjugationType::IAdjective, "", 2};

  const auto expanded = expandDictionarySourceEntries({colloquial, regular});

  ASSERT_NE(findSurface(expanded.entries, "かっこよけれ"), nullptr);
  EXPECT_EQ(findSurface(expanded.entries, "かっこいけれ"), nullptr);
  ASSERT_NE(findSurface(expanded.entries, "かっこいい"), nullptr);
}

TEST(DictionaryExpansionTest, InvalidConjugationTypeKeepsLiteralSurface) {
  const dictionary::SourceEntry source{"テスト", core::PartOfSpeech::Verb, dictionary::ConjugationType::Suru, "", 1};

  EXPECT_NE(dictionaryConjugationTypeIssue(source).find("ending in する"), std::string::npos);
  const auto expanded = expandDictionarySourceEntry(source);

  ASSERT_EQ(expanded.size(), 1U);
  EXPECT_EQ(expanded[0].surface, "テスト");
  EXPECT_EQ(findSurface(expanded, "テする"), nullptr);
}

TEST(DictionaryExpansionTest, ReusesKuruKanjiAndKanaForms) {
  const dictionary::SourceEntry source{"来る", core::PartOfSpeech::Verb, dictionary::ConjugationType::Kuru, "", 1};

  auto expanded = expandDictionarySourceEntries({source});

  const auto* kanji = findSurface(expanded.entries, "来れ");
  const auto* kana = findSurface(expanded.entries, "くれ");
  ASSERT_NE(kanji, nullptr);
  ASSERT_NE(kana, nullptr);
  EXPECT_EQ(kanji->lemma, "来る");
  EXPECT_EQ(kana->lemma, "くる");
  EXPECT_EQ(kanji->extended_pos, core::ExtendedPOS::VerbKateikei);
  EXPECT_EQ(kana->extended_pos, core::ExtendedPOS::VerbKateikei);
  EXPECT_EQ(findSurface(expanded.entries, "き"), nullptr);
}

TEST(DictionaryExpansionTest, KuruExpansionPreservesCompoundPrefix) {
  const dictionary::SourceEntry source{"持って来る", core::PartOfSpeech::Verb, dictionary::ConjugationType::Kuru, "",
                                       1};

  EXPECT_TRUE(dictionaryConjugationTypeIssue(source).empty());
  const auto expanded = expandDictionarySourceEntry(source);

  const auto* base = findSurface(expanded, "持って来る");
  const auto* conditional = findSurface(expanded, "持って来れ");
  const auto* kana_base = findSurface(expanded, "持ってくる");
  ASSERT_NE(base, nullptr);
  ASSERT_NE(conditional, nullptr);
  ASSERT_NE(kana_base, nullptr);
  EXPECT_EQ(base->lemma, "持って来る");
  EXPECT_EQ(conditional->lemma, "持って来る");
  EXPECT_EQ(kana_base->lemma, "持ってくる");
  EXPECT_EQ(findSurface(expanded, "来る"), nullptr);
}

TEST(DictionaryExpansionTest, ConsumesCanonicalKuruStemForms) {
  const dictionary::SourceEntry source{"来る", core::PartOfSpeech::Verb, dictionary::ConjugationType::Kuru, "", 1};
  const KuruStemForms kanji = getKuruStemForms("来る");
  const KuruStemForms kana = getKuruStemForms("くる");

  const auto expanded = expandDictionarySourceEntry(source);

  for (const auto* surface : {&kanji.base, &kanji.mizenkei, &kanji.renyokei, &kanji.kateikei, &kanji.ishikei,
                              &kanji.meireikei, &kana.base, &kana.kateikei, &kana.ishikei, &kana.meireikei}) {
    EXPECT_NE(findSurface(expanded, *surface), nullptr) << *surface;
  }
  // Ambiguous one-mora kana stems remain reverse-inflection candidates instead
  // of receiving the dictionary short-verb bonus.
  EXPECT_EQ(findSurface(expanded, kana.mizenkei), nullptr);
  EXPECT_EQ(findSurface(expanded, kana.renyokei), nullptr);
  EXPECT_EQ(findSurface(expanded, kana.mizenkei + "られる"), nullptr);
  EXPECT_EQ(findSurface(expanded, kana.mizenkei + "れる"), nullptr);
  EXPECT_EQ(findSurface(expanded, kana.mizenkei + "させる"), nullptr);
}

TEST(DictionaryExpansionTest, KeepsKuruCausativeInTheCanonicalParadigm) {
  const auto forms = getKuruDictionaryForms();
  const auto causative = std::find_if(forms.begin(), forms.end(), [](const auto& form) {
    return form.kanji_surface == "来させる" && form.kana_surface == "こさせる";
  });

  ASSERT_NE(causative, forms.end());
  EXPECT_EQ(causative->extended_pos, core::ExtendedPOS::VerbShuushikei);
  EXPECT_FALSE(causative->emit_kanji);
  EXPECT_FALSE(causative->emit_kana);
}

TEST(DictionaryExpansionTest, PreservesDifferentPosAtTheSameExpandedSurface) {
  const dictionary::SourceEntry noun{"テストすれば", core::PartOfSpeech::Noun, dictionary::ConjugationType::None, "",
                                     1};
  const dictionary::SourceEntry verb{"テストする", core::PartOfSpeech::Verb, dictionary::ConjugationType::Suru, "", 2};

  auto expanded = expandDictionarySourceEntries({verb, noun});

  const auto collisions = std::count_if(expanded.entries.begin(), expanded.entries.end(),
                                        [](const auto& entry) { return entry.surface == "テストすれば"; });
  EXPECT_EQ(collisions, 2);
  EXPECT_EQ(expanded.duplicates_skipped, 0);

  DictionaryExpansionOptions distinct_pos_options;
  distinct_pos_options.preserve_same_pos_homographs = false;
  auto distinct_pos_expanded = expandDictionarySourceEntries({verb, noun}, distinct_pos_options);
  const auto distinct_pos_collisions =
      std::count_if(distinct_pos_expanded.entries.begin(), distinct_pos_expanded.entries.end(),
                    [](const auto& entry) { return entry.surface == "テストすれば"; });
  EXPECT_EQ(distinct_pos_collisions, 2);
}

}  // namespace
}  // namespace grammar
}  // namespace suzume
