/**
 * @file verb_guard_family_test.cpp
 * @brief Characterization tests for the fabricated closed-class absorption guard
 *        family (see verb_candidates_helpers.h).
 *
 * These pin the decision boundaries of the guards that stop verb/adjective
 * candidate generators from fabricating a non-dictionary conjugation that
 * swallows an adjacent closed-class morpheme — a 係助詞 (しか/さえ/すら), or a
 * te-form + 補助動詞 みる. A failure here means a guard's boundary
 * moved; verify the tokenization of the named example before adjusting an
 * expectation rather than relaxing the test to the new output.
 */

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "analysis/join_compound_verb_internal.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/utf8.h"
#include "suzume.h"

namespace suzume::analysis {
namespace {

using core::ExtendedPOS;

std::vector<char32_t> cps(std::string_view text) {
  return normalize::toCodepoints(text);
}

TEST(VerbGuardFamilyBounds, KkoNominalizerRejectsInvalidEndPosition) {
  const auto codepoints = cps("打ったこと");

  EXPECT_FALSE(verb_helpers::crossesKkoNominalizer(codepoints, 0, codepoints.size() + 1));
  EXPECT_FALSE(verb_helpers::crossesKkoNominalizer(codepoints, 3, 2));
}

TEST(VerbGuardFamilyBounds, FormalNounDoesNotLoseItsHeadToHiraganaNounRescue) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  for (const std::string_view text : {"くること", "おくこと"}) {
    const auto result = analyzer.analyze(text);
    ASSERT_EQ(result.size(), 2U) << text;
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb) << text;
    EXPECT_EQ(result[1].surface, "こと") << text;
    EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::NounFormal) << text;
  }
}

TEST(VerbGuardFamilyBounds, ClassicalCopulaIrrealisWinsBeforeClassicalNegative) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  for (const std::string_view text : {"静かならず", "平和ならず", "元気ならず", "有名ならず"}) {
    const auto result = analyzer.analyze(text);
    ASSERT_EQ(result.size(), 3U) << text;
    EXPECT_EQ(result[1].surface, "なら") << text;
    EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxClassicalNari) << text;
    EXPECT_EQ(result[1].lemma, "なり") << text;
    EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxNegativeNu) << text;
  }
}

// =============================================================================
// Tail guards (T): the run ends in [word] + particle (+ negative)
// =============================================================================

TEST(VerbGuardFamilyTail, FocusParticleTailDetectsBindingShika) {
  dictionary::DictionaryManager dict;
  // 副助詞 しか after a host, with and without the trailing negative ない.
  auto bare = cps("水しか");
  EXPECT_TRUE(verb_helpers::endsWithFocusParticleTail(&dict, bare, 0, bare.size()));
  auto negative = cps("水しかない");
  EXPECT_TRUE(verb_helpers::endsWithFocusParticleTail(&dict, negative, 0, negative.size()));
}

TEST(VerbGuardFamilyTail, FocusParticleTailDetectsBindingSaeAndSura) {
  dictionary::DictionaryManager dict;
  auto sae = cps("お金さえ");
  EXPECT_TRUE(verb_helpers::endsWithFocusParticleTail(&dict, sae, 0, sae.size()));
  auto sura = cps("水すらない");
  EXPECT_TRUE(verb_helpers::endsWithFocusParticleTail(&dict, sura, 0, sura.size()));
}

TEST(VerbGuardFamilyTail, ParticleClassIsDiscriminated) {
  dictionary::DictionaryManager dict;
  // しか and すら are 係助詞 (Binding). Each must not be mistaken for a 副助詞.
  auto shika = cps("水しかない");
  EXPECT_TRUE(verb_helpers::endsWithParticleTailOfPos(&dict, shika, 0, shika.size(), ExtendedPOS::ParticleBinding));
  EXPECT_FALSE(verb_helpers::endsWithParticleTailOfPos(&dict, shika, 0, shika.size(), ExtendedPOS::ParticleAdverbial));
  auto sura = cps("水すらない");
  EXPECT_TRUE(verb_helpers::endsWithParticleTailOfPos(&dict, sura, 0, sura.size(), ExtendedPOS::ParticleBinding));
  EXPECT_FALSE(verb_helpers::endsWithParticleTailOfPos(&dict, sura, 0, sura.size(), ExtendedPOS::ParticleAdverbial));
}

TEST(VerbGuardFamilyTail, GenuineGodanMizenkeiIsNotSwept) {
  dictionary::DictionaryManager dict;
  // 行かない is a genuine godan-ka mizenkei; the single-mora か must never match a
  // 2+ codepoint particle tail.
  auto iku = cps("行かない");
  EXPECT_FALSE(verb_helpers::endsWithFocusParticleTail(&dict, iku, 0, iku.size()));
}

TEST(VerbGuardFamilyTail, VerbPrefixBeforeBindingParticleSplits) {
  dictionary::DictionaryManager dict;
  grammar::Inflection infl;
  // Single-mora prefix (る) is a bare verb-ending fragment, never a word, so the
  // whole run is garbage — the production catch for やるしかない, where や splits
  // off first and the residual るしか reaches this guard.
  auto ru = cps("るしか");
  EXPECT_TRUE(hiragana_verb_detail::endsWithParticleAfterVerb(&dict, infl, ru, 0, ru.size()));
  // て + verb + しか (てみるしか) is a subsidiary-verb sequence: after stripping
  // the leading te-form, the probe みる analyzes as a verb, so the run splits.
  auto temiru = cps("てみるしか");
  EXPECT_TRUE(hiragana_verb_detail::endsWithParticleAfterVerb(&dict, infl, temiru, 0, temiru.size()));
}

TEST(VerbGuardFamilyTail, TooShortRunHasNoVerbPlusParticleSplit) {
  dictionary::DictionaryManager dict;
  grammar::Inflection infl;
  // Too short to hold a 1+ char prefix and a 2+ char particle suffix.
  auto shika = cps("しか");
  EXPECT_FALSE(hiragana_verb_detail::endsWithParticleAfterVerb(&dict, infl, shika, 0, shika.size()));
}

// =============================================================================
// Embed guards (E): an internal て/で + 補助動詞 splits the run
// =============================================================================

TEST(VerbGuardFamilyEmbed, TeFormMiruIsDetected) {
  auto yatte = cps("やってみる");
  EXPECT_TRUE(verb_helpers::embedsTeFormMiruAuxiliary(yatte, 0, yatte.size()));
  auto tabete = cps("食べてみれば");
  EXPECT_TRUE(verb_helpers::embedsTeFormMiruAuxiliary(tabete, 0, tabete.size()));
  // The voiced onbin form でみ (読んでみる) is also a te-form + みる boundary.
  auto yonde = cps("読んでみる");
  EXPECT_TRUE(verb_helpers::embedsTeFormMiruAuxiliary(yonde, 0, yonde.size()));
}

TEST(VerbGuardFamilyEmbed, LeadingTeOrDeIsExempt) {
  // A candidate that merely BEGINS with て/で is a different shape, left untouched.
  auto temiru = cps("てみる");
  EXPECT_FALSE(verb_helpers::embedsTeFormMiruAuxiliary(temiru, 0, temiru.size()));
  auto demiru = cps("でみる");
  EXPECT_FALSE(verb_helpers::embedsTeFormMiruAuxiliary(demiru, 0, demiru.size()));
  // No embedded てみ/でみ at all.
  auto taberu = cps("食べる");
  EXPECT_FALSE(verb_helpers::embedsTeFormMiruAuxiliary(taberu, 0, taberu.size()));
}

TEST(VerbGuardFamilyEmbed, TeFormAuxiliaryPatterns) {
  EXPECT_TRUE(verb_helpers::embedsTeFormAuxiliary("食べていく"));
  EXPECT_TRUE(verb_helpers::embedsTeFormAuxiliary("助けてもらう"));
  EXPECT_TRUE(verb_helpers::embedsTeFormAuxiliary("見てくれ"));
  // 〜ている / 〜ておく are intentionally NOT matched: they would strand a
  // te-ending stem (慌て+ている), and the plain split already wins there.
  EXPECT_FALSE(verb_helpers::embedsTeFormAuxiliary("食べている"));
  EXPECT_FALSE(verb_helpers::embedsTeFormAuxiliary("置いておく"));
}

TEST(VerbGuardFamilyWiring, EveryGuardedOriginIsDeclared) {
  using verb_helpers::GuardMember;
  using verb_helpers::GuardOrigin;

  constexpr std::array<std::pair<GuardMember, GuardOrigin>, 10> expected = {{
      {GuardMember::EmbedTeAuxiliary, GuardOrigin::HiraganaInflection},
      {GuardMember::EmbedTeAuxiliary, GuardOrigin::KanjiFinalization},
      {GuardMember::EmbedTeAuxiliary, GuardOrigin::KanjiMizenkei},
      {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::HiraganaInflection},
      {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::HiraganaDerived},
      {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::KanjiFinalization},
      {GuardMember::FocusParticleHead, GuardOrigin::KanjiAdjective},
      {GuardMember::FocusParticleHead, GuardOrigin::KanjiCompoundAdjective},
      {GuardMember::ClassicalAuxiliaryTail, GuardOrigin::KanjiFinalization},
      {GuardMember::ClassicalAuxiliaryTail, GuardOrigin::KanjiRenyokei},
  }};

  EXPECT_EQ(verb_helpers::kGuardWiring.size(), expected.size());
  for (const auto& [member, origin] : expected) {
    EXPECT_TRUE(verb_helpers::guardIsWired(member, origin));
  }
}

TEST(VerbGuardFamilyWiring, HiraganaOriginsKeepClosedClassBoundaries) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  const auto surfaces = [&](std::string_view input) {
    std::vector<std::string> result;
    for (const auto& token : analyzer.analyze(std::string(input))) {
      result.push_back(token.surface);
    }
    return result;
  };

  // H-1: a Godan-ra form remains available without lexical dictionary help
  // when が/や would otherwise be read as a particle.
  EXPECT_EQ(surfaces("まがった"), (std::vector<std::string>{"まがっ", "た"}));
  EXPECT_EQ(surfaces("こわがっている"), (std::vector<std::string>{"こわがっ", "て", "いる"}));
  EXPECT_EQ(surfaces("ふさがる穴"), (std::vector<std::string>{"ふさがる", "穴"}));
  EXPECT_EQ(surfaces("はやった歌"), (std::vector<std::string>{"はやっ", "た", "歌"}));

  // H-3/M-1: both the condition fast path and ていく helper preserve the
  // independently searchable auxiliary boundary.
  EXPECT_EQ(surfaces("やってみればわかる"), (std::vector<std::string>{"やっ", "て", "みれ", "ば", "わかる"}));
  EXPECT_EQ(surfaces("もっていく"), (std::vector<std::string>{"もっ", "て", "いく"}));
  EXPECT_EQ(surfaces("もってくれば"), (std::vector<std::string>{"もっ", "て", "くれ", "ば"}));
}

TEST(VerbGuardFamilyEmbed, PassiveNegativeParadigmIsGuarded) {
  for (const std::string_view surface : {"書かれない", "書かれなく", "書かれなくて", "書かれなかった", "書かれなけれ",
                                         "書かれなければ", "書かれなきゃ"}) {
    EXPECT_TRUE(verb_helpers::shouldSkipPassiveAuxPattern(surface, grammar::VerbType::GodanKa)) << surface;
  }
  for (const std::string_view surface :
       {"検査されない", "検査されなくて", "検査されなかった", "検査されなければ", "検査されなきゃ"}) {
    EXPECT_TRUE(verb_helpers::shouldSkipPassiveAuxPattern(surface, grammar::VerbType::Suru)) << surface;
  }
  EXPECT_FALSE(verb_helpers::shouldSkipPassiveAuxPattern("書かない", grammar::VerbType::GodanKa));
  EXPECT_FALSE(verb_helpers::shouldSkipPassiveAuxPattern("検査しない", grammar::VerbType::Suru));
}

TEST(CompoundVerbForms, GodanRowsDriveConjugationAndTeForms) {
  using compound_verb_detail::TeFormType;
  using compound_verb_detail::V2VerbType;
  struct Case {
    std::string_view base;
    std::string_view ending;
    dictionary::ConjugationType conjugation_type;
    TeFormType te_form_type;
    std::string_view te_stem;
    bool uses_de;
  };
  constexpr Case cases[] = {
      {"たたく", "く", dictionary::ConjugationType::GodanKa, TeFormType::Ionbin, "たたい", false},
      {"つなぐ", "ぐ", dictionary::ConjugationType::GodanGa, TeFormType::Ionbin, "つない", true},
      {"はなす", "す", dictionary::ConjugationType::GodanSa, TeFormType::Renyokei, "はなし", false},
      {"まつ", "つ", dictionary::ConjugationType::GodanTa, TeFormType::Sokuonbin, "まっ", false},
      {"しぬ", "ぬ", dictionary::ConjugationType::GodanNa, TeFormType::Hatsuonbin, "しん", true},
      {"とぶ", "ぶ", dictionary::ConjugationType::GodanBa, TeFormType::Hatsuonbin, "とん", true},
      {"よむ", "む", dictionary::ConjugationType::GodanMa, TeFormType::Hatsuonbin, "よん", true},
      {"かえる", "る", dictionary::ConjugationType::GodanRa, TeFormType::Sokuonbin, "かえっ", false},
      {"かう", "う", dictionary::ConjugationType::GodanWa, TeFormType::Sokuonbin, "かっ", false},
  };

  for (const auto& test_case : cases) {
    EXPECT_EQ(compound_verb_detail::compoundConjugationType(V2VerbType::Godan, test_case.ending),
              test_case.conjugation_type);
    EXPECT_EQ(compound_verb_detail::getTeFormType(test_case.ending), test_case.te_form_type);
    const auto [stem, uses_de] =
        compound_verb_detail::generateTeFormStem(test_case.base, "", V2VerbType::Godan, test_case.ending);
    EXPECT_EQ(stem, test_case.te_stem);
    EXPECT_EQ(uses_de, test_case.uses_de);
  }

  EXPECT_EQ(compound_verb_detail::compoundConjugationType(V2VerbType::Godan, "い"), dictionary::ConjugationType::None);
  EXPECT_EQ(compound_verb_detail::getTeFormType("い"), TeFormType::Ichidan);
}

TEST(CompoundVerbForms, KanjiAndHiraganaHostsShareV2InflectionCells) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  const auto surfaces = [&](std::string_view input) {
    std::vector<std::string> result;
    for (const auto& token : analyzer.analyze(std::string(input))) {
      result.push_back(token.surface);
    }
    return result;
  };

  EXPECT_EQ(surfaces("取り戻せない"), (std::vector<std::string>{"取り戻せ", "ない"}));
  EXPECT_EQ(surfaces("とりもどせない"), (std::vector<std::string>{"とりもどせ", "ない"}));
  EXPECT_EQ(surfaces("取り出そう"), (std::vector<std::string>{"取り出そ", "う"}));
  EXPECT_EQ(surfaces("とりだそう"), (std::vector<std::string>{"とりだそ", "う"}));
}

TEST(CandidateGenerationRegression, IAdjectiveStemUsesCanonicalGaruParadigm) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  const auto result = analyzer.analyze("恥ずかしがらない");
  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "恥ずかし");
  EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjStem);
  EXPECT_EQ(result[0].lemma, "恥ずかしい");
  EXPECT_EQ(result[1].surface, "がら");
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxGaru);
  EXPECT_EQ(result[1].lemma, "がる");
  EXPECT_EQ(result[2].surface, "ない");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxNegativeNai);
}

TEST(CandidateGenerationRegression, KatakanaIAdjectiveNegativePastKeepsStemBoundary) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  const auto result = analyzer.analyze("エモくなかった");
  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].surface, "エモく");
  EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjRenyokei);
  EXPECT_EQ(result[0].lemma, "エモい");
  EXPECT_EQ(result[1].surface, "なかっ");
  // After an adjective continuative ない is the supplementary adjective.
  EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AdjKatt);
  EXPECT_EQ(result[2].surface, "た");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::AuxTenseTa);
}

// =============================================================================
// Prefix guards (P): a particle homograph may also be a lexical verb stem
// =============================================================================

TEST(VerbGuardFamilyPrefix, KeepsConfidentLexicalStemMatchingBindingParticle) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  const auto result = analyzer.analyze("さえない映画だった");

  ASSERT_EQ(result.size(), 5U);
  EXPECT_EQ(result[0].surface, "さえ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::VerbMizenkei);
  EXPECT_EQ(result[0].lemma, "さえる");
  EXPECT_EQ(result[1].surface, "ない");
  EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(result[2].surface, "映画");
  EXPECT_EQ(result[2].pos, core::PartOfSpeech::Noun);

  const auto subject_result = analyzer.analyze("映画がさえない");
  ASSERT_EQ(subject_result.size(), 4U);
  EXPECT_EQ(subject_result[2].surface, "さえ");
  EXPECT_EQ(subject_result[2].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(subject_result[2].lemma, "さえる");
  EXPECT_EQ(subject_result[3].surface, "ない");
}

TEST(VerbGuardFamilyPrefix, StillSplitsActualBindingParticleBeforeSubsidiaryVerb) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  const auto result = analyzer.analyze("読んでさえいればよい");

  ASSERT_EQ(result.size(), 6U);
  EXPECT_EQ(result[0].surface, "読ん");
  EXPECT_EQ(result[1].surface, "で");
  EXPECT_EQ(result[2].surface, "さえ");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::ParticleBinding);
  EXPECT_EQ(result[3].surface, "いれ");
  EXPECT_EQ(result[3].lemma, "いる");
  EXPECT_EQ(result[4].surface, "ば");
  EXPECT_EQ(result[5].surface, "よい");
}

TEST(VerbGuardFamilyPrefix, StillKeepsBindingParticleAfterNominalHost) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  const auto result = analyzer.analyze("読むことさえない");

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result[0].surface, "読む");
  EXPECT_EQ(result[1].surface, "こと");
  EXPECT_EQ(result[2].surface, "さえ");
  EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::ParticleBinding);
  EXPECT_EQ(result[2].lemma, "さえ");
  EXPECT_EQ(result[3].surface, "ない");
  EXPECT_EQ(result[3].pos, core::PartOfSpeech::Adjective);
}

}  // namespace
}  // namespace suzume::analysis
