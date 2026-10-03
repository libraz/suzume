#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "normalize/utf8.h"
#include "suzume.h"

namespace suzume {
namespace {

class SuzumeApiTest : public ::testing::Test {
 protected:
  SuzumeOptions makeTestOptions() {
    SuzumeOptions opts;
    opts.skip_user_dictionary = true;
    return opts;
  }
};

TEST_F(SuzumeApiTest, DefaultConstructorCreatesInstance) {
  Suzume instance;
  // Should not crash, instance is valid
  EXPECT_FALSE(Suzume::version().empty());
}

TEST_F(SuzumeApiTest, ConstructWithOptions) {
  SuzumeOptions opts = makeTestOptions();
  opts.lemmatize = true;
  opts.merge_compounds = false;
  Suzume instance(opts);
  // Instance created successfully with custom options
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Normal);
}

TEST_F(SuzumeApiTest, ConstructWithSkipUserDictionary) {
  SuzumeOptions opts;
  opts.skip_user_dictionary = true;
  Suzume instance(opts);
  auto results = instance.analyze("\xe6\x9d\xb1\xe4\xba\xac");  // Tokyo
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, VersionReturnsNonEmptyString) {
  std::string ver = Suzume::version();
  EXPECT_FALSE(ver.empty());
}

TEST_F(SuzumeApiTest, ModeDefaultsToNormal) {
  Suzume instance(makeTestOptions());
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Normal);
}

TEST_F(SuzumeApiTest, SetModeRoundtrip) {
  Suzume instance(makeTestOptions());

  instance.setMode(core::AnalysisMode::Search);
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Search);

  instance.setMode(core::AnalysisMode::Split);
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Split);

  instance.setMode(core::AnalysisMode::Normal);
  EXPECT_EQ(instance.mode(), core::AnalysisMode::Normal);
}

TEST_F(SuzumeApiTest, SplitModeDisablesMixedScriptJoinCandidates) {
  SuzumeOptions normal_opts = makeTestOptions();
  normal_opts.mode = core::AnalysisMode::Normal;
  Suzume normal(normal_opts);

  SuzumeOptions split_opts = makeTestOptions();
  split_opts.mode = core::AnalysisMode::Split;
  Suzume split(split_opts);

  auto normal_results = normal.analyze("API開発");
  auto split_results = split.analyze("API開発");

  ASSERT_FALSE(normal_results.empty());
  ASSERT_FALSE(split_results.empty());
  EXPECT_EQ(normal_results.front().surface, "API開発");
  EXPECT_GT(split_results.size(), normal_results.size());
}

TEST_F(SuzumeApiTest, VerbCandidateScorerJsonChangesAnalysis) {
  SuzumeOptions default_options = makeTestOptions();
  default_options.skip_core_dictionary = true;
  Suzume default_instance(default_options);

  SuzumeOptions tuned_options = default_options;
  tuned_options.scorer_options_json = R"({"verb_candidates":{"confidence_ichidan_dict":100}})";
  Suzume tuned_instance(tuned_options);

  const auto default_results = default_instance.analyze("食べました");
  const auto tuned_results = tuned_instance.analyze("食べました");

  ASSERT_EQ(default_results.size(), 3u);
  EXPECT_EQ(default_results[0].surface, "食べ");
  ASSERT_FALSE(tuned_results.empty());
  EXPECT_NE(tuned_results[0].surface, default_results[0].surface);
}

TEST_F(SuzumeApiTest, InflectionScorerJsonChangesAnalysis) {
  SuzumeOptions default_options = makeTestOptions();
  default_options.skip_core_dictionary = true;
  Suzume default_instance(default_options);

  SuzumeOptions tuned_options = default_options;
  tuned_options.scorer_options_json = R"({"inflection":{"confidence_ceiling":0}})";
  Suzume tuned_instance(tuned_options);

  const auto default_results = default_instance.analyze("歩いています");
  const auto tuned_results = tuned_instance.analyze("歩いています");

  ASSERT_EQ(default_results.size(), 4u);
  EXPECT_EQ(default_results[0].surface, "歩い");
  ASSERT_FALSE(tuned_results.empty());
  EXPECT_EQ(tuned_results[0].surface, "歩");
}

TEST_F(SuzumeApiTest, ProgramScorerJsonOverridesEnvironment) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_INFL_confidence_ceiling", "0", 1);
  SuzumeOptions options = makeTestOptions();
  options.skip_core_dictionary = true;
  options.scorer_options_json = R"({"inflection":{"confidence_ceiling":0.95}})";
  Suzume instance(options);
  unsetenv("SUZUME_SCORER_INFL_confidence_ceiling");

  const auto results = instance.analyze("歩いています");
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(results.front().surface, "歩い");
#endif
}

TEST_F(SuzumeApiTest, EnvironmentScorerConfigCanBeDisabled) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_INFL_confidence_ceiling", "0", 1);
  SuzumeOptions options = makeTestOptions();
  options.skip_core_dictionary = true;
  options.skip_env_config = true;
  Suzume instance(options);
  unsetenv("SUZUME_SCORER_INFL_confidence_ceiling");

  const auto results = instance.analyze("歩いています");
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(results.front().surface, "歩い");
#endif
}

TEST_F(SuzumeApiTest, InvalidDirectScorerJsonIsReportedThroughWarnings) {
  SuzumeOptions options = makeTestOptions();
  options.scorer_options_json = "{";
  Suzume instance(options);

  const auto& warnings = instance.dictionaryWarnings();
  ASSERT_FALSE(warnings.empty());
  EXPECT_NE(warnings.front().find("Failed to load direct scorer config"), std::string::npos);
}

TEST_F(SuzumeApiTest, ActiveScorerConfigStatusIsReportedThroughWarnings) {
  SuzumeOptions options = makeTestOptions();
  options.report_scorer_config = true;
  options.scorer_options_json = R"({"unary":{"noun_prior":0.25}})";
  Suzume instance(options);

  const auto& warnings = instance.dictionaryWarnings();
  ASSERT_FALSE(warnings.empty());
  EXPECT_NE(warnings.front().find("Scorer configuration active"), std::string::npos);
  EXPECT_NE(warnings.front().find("program_json=active"), std::string::npos);
}

TEST_F(SuzumeApiTest, SetModeUpdatesTokenizerAndPostprocessor) {
  Suzume instance(makeTestOptions());

  auto normal_results = instance.analyze("API開発");
  instance.setMode(core::AnalysisMode::Split);
  auto split_results = instance.analyze("API開発");

  ASSERT_FALSE(normal_results.empty());
  ASSERT_FALSE(split_results.empty());
  EXPECT_EQ(normal_results.front().surface, "API開発");
  EXPECT_GT(split_results.size(), normal_results.size());
}

TEST_F(SuzumeApiTest, SuruVerbSplitsWithoutCoreDictionary) {
  // Vanilla (core.dic disabled) must still split 管理する会社 grammatically as
  // 管理 / する / 会社. The base form する is a closed-class irregular verb held
  // in L1, so this does not depend on the L2 dictionary. Without a standalone
  // する token the analyzer mis-splits into 管 / 理する / 会社.
  SuzumeOptions opts;
  opts.skip_user_dictionary = true;
  opts.skip_core_dictionary = true;
  Suzume instance(opts);

  auto results = instance.analyze("管理する会社");
  ASSERT_EQ(results.size(), 3u);
  EXPECT_EQ(results[0].surface, "管理");
  EXPECT_EQ(results[1].surface, "する");
  EXPECT_EQ(results[1].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(results[2].surface, "会社");
}

TEST_F(SuzumeApiTest, DifficultySuffixPastFormKeepsItsAdjectiveBoundary) {
  Suzume instance(makeTestOptions());

  const auto results = instance.analyze("読みやすかった");
  ASSERT_EQ(results.size(), 3u);
  EXPECT_EQ(results[0].surface, "読み");
  EXPECT_EQ(results[1].surface, "やすかっ");
  EXPECT_EQ(results[1].pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(results[1].lemma, "やすい");
  EXPECT_EQ(results[1].extended_pos, core::ExtendedPOS::AdjKatt);
  EXPECT_EQ(results[2].surface, "た");
}

TEST_F(SuzumeApiTest, KyotoHonorificYasuRequiresItsHonorificHost) {
  Suzume instance(makeTestOptions());

  const auto prefixed_verb = instance.analyze("お見やす");
  ASSERT_EQ(prefixed_verb.size(), 3u);
  EXPECT_EQ(prefixed_verb[0].extended_pos, core::ExtendedPOS::Prefix);
  EXPECT_EQ(prefixed_verb[1].extended_pos, core::ExtendedPOS::VerbRenyokei);
  EXPECT_EQ(prefixed_verb[2].surface, "やす");
  EXPECT_EQ(prefixed_verb[2].extended_pos, core::ExtendedPOS::AuxHonorific);

  const auto benefactive_request = instance.analyze("読んでおくれやす");
  ASSERT_EQ(benefactive_request.size(), 4u);
  EXPECT_EQ(benefactive_request[2].extended_pos, core::ExtendedPOS::AuxBenefactive);
  EXPECT_EQ(benefactive_request[3].surface, "やす");
  EXPECT_EQ(benefactive_request[3].extended_pos, core::ExtendedPOS::AuxHonorific);
}

TEST_F(SuzumeApiTest, KuruKanaMizenkeiRequiresItsSelectingAuxiliary) {
  Suzume instance(makeTestOptions());

  const auto passive = instance.analyze("彼がこられる");
  ASSERT_EQ(passive.size(), 4u);
  EXPECT_EQ(passive[2].surface, "こ");
  EXPECT_EQ(passive[2].lemma, "くる");
  EXPECT_EQ(passive[2].extended_pos, core::ExtendedPOS::VerbMizenkei);
  EXPECT_EQ(passive[3].extended_pos, core::ExtendedPOS::AuxPassive);

  const auto causative = instance.analyze("彼をこさせる");
  ASSERT_EQ(causative.size(), 4u);
  EXPECT_EQ(causative[2].surface, "こ");
  EXPECT_EQ(causative[2].lemma, "くる");
  EXPECT_EQ(causative[2].extended_pos, core::ExtendedPOS::VerbMizenkei);
  EXPECT_EQ(causative[3].extended_pos, core::ExtendedPOS::AuxCausative);

  const auto ra_nuki_potential = instance.analyze("もうすぐこれる");
  ASSERT_EQ(ra_nuki_potential.size(), 3u);
  EXPECT_EQ(ra_nuki_potential[0].extended_pos, core::ExtendedPOS::Adverb);
  EXPECT_EQ(ra_nuki_potential[1].extended_pos, core::ExtendedPOS::Adverb);
  EXPECT_EQ(ra_nuki_potential[2].surface, "これる");
  EXPECT_EQ(ra_nuki_potential[2].pos, core::PartOfSpeech::Verb);
}

TEST_F(SuzumeApiTest, DictionaryHomographsPreserveProductiveNominalBoundaries) {
  Suzume instance(makeTestOptions());

  for (const std::string_view text : {"重要なお知らせ", "大切なお願い", "簡単なお仕事", "静かなお庭"}) {
    SCOPED_TRACE(text);
    const auto results = instance.analyze(text);
    ASSERT_EQ(results.size(), 4u);
    EXPECT_EQ(results[0].extended_pos, core::ExtendedPOS::AdjNaAdj);
    EXPECT_EQ(results[1].surface, "な");
    EXPECT_EQ(results[1].extended_pos, core::ExtendedPOS::AuxCopulaDa);
    EXPECT_EQ(results[2].surface, "お");
    EXPECT_EQ(results[2].extended_pos, core::ExtendedPOS::Prefix);
    EXPECT_EQ(results[3].pos, core::PartOfSpeech::Noun);
  }

  const auto coordinated = instance.analyze("行為やら経過やら確認する");
  ASSERT_EQ(coordinated.size(), 6u);
  EXPECT_EQ(coordinated[0].surface, "行為");
  EXPECT_EQ(coordinated[1].extended_pos, core::ExtendedPOS::ParticleAdverbial);
  EXPECT_EQ(coordinated[2].surface, "経過");

  const auto temporal = instance.analyze("当時やらの話");
  ASSERT_EQ(temporal.size(), 4u);
  EXPECT_EQ(temporal[0].surface, "当時");
  EXPECT_EQ(temporal[1].extended_pos, core::ExtendedPOS::ParticleAdverbial);
}

TEST_F(SuzumeApiTest, AnalyzeSimpleText) {
  Suzume instance(makeTestOptions());
  // "Tokyo is beautiful"
  auto results = instance.analyze("\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84");
  EXPECT_FALSE(results.empty());

  // Check that surfaces are non-empty
  for (const auto& morpheme : results) {
    EXPECT_FALSE(morpheme.surface.empty());
  }
}

TEST_F(SuzumeApiTest, AnalyzeReturnsNonEmptyForJapanese) {
  Suzume instance(makeTestOptions());
  // "eat" (taberu)
  auto results = instance.analyze("\xe9\xa3\x9f\xe3\x81\xb9\xe3\x82\x8b");
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, ProlongedSoundMergeKeepsSurfaceAndOffsetsConsistent) {
  Suzume instance(makeTestOptions());
  auto results = instance.analyze("すごーーい");

  ASSERT_FALSE(results.empty());
  bool found = false;
  for (const auto& morpheme : results) {
    if (morpheme.surface.find("ーー") != std::string::npos) {
      found = true;
      EXPECT_EQ(morpheme.end - morpheme.start, normalize::utf8Length(morpheme.surface));
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(SuzumeApiTest, AnalyzeEmptyTextReturnsEmpty) {
  Suzume instance(makeTestOptions());
  auto results = instance.analyze("");
  EXPECT_TRUE(results.empty());
}

TEST_F(SuzumeApiTest, AnalyzeInvalidUtf8ReturnsEmpty) {
  Suzume instance(makeTestOptions());
  auto results = instance.analyze(std::string_view("\xE3\x81", 2));
  EXPECT_TRUE(results.empty());
}

// analyze() cannot tell "nothing to segment" from "malformed input" — both are
// an empty vector, as the two tests above show. analyzeResult() is the form that
// separates them.
TEST_F(SuzumeApiTest, AnalyzeResultReportsInvalidUtf8) {
  Suzume instance(makeTestOptions());
  auto result = instance.analyzeResult(std::string_view("\xE3\x81", 2));
  ASSERT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, core::ErrorCode::InvalidUtf8);
}

TEST_F(SuzumeApiTest, AnalyzeResultTreatsEmptyInputAsSuccess) {
  Suzume instance(makeTestOptions());
  auto result = instance.analyzeResult("");
  ASSERT_TRUE(result.hasValue());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(SuzumeApiTest, AnalyzeResultTreatsSegmentableFreeInputAsSuccess) {
  Suzume instance(makeTestOptions());
  // Valid UTF-8 that carries no segmentable content: an empty result here is a
  // legitimate answer, not a failure.
  auto result = instance.analyzeResult("　 ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(SuzumeApiTest, AnalyzeResultMatchesAnalyzeOnValidText) {
  Suzume instance(makeTestOptions());
  auto result = instance.analyzeResult("本を読む");
  ASSERT_TRUE(result.hasValue());
  EXPECT_FALSE(result.value().empty());

  auto lenient = instance.analyze("本を読む");
  ASSERT_EQ(result.value().size(), lenient.size());
  for (size_t idx = 0; idx < lenient.size(); ++idx) {
    EXPECT_EQ(result.value()[idx].surface, lenient[idx].surface);
  }
}

TEST_F(SuzumeApiTest, DetailedAnalysisExposesNormalizedOffsetCoordinateText) {
  Suzume instance(makeTestOptions());
  auto result = instance.analyzeWithNormalizedTextResult("ｶﾞｸｾｲ");
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value().normalized_text, "ガクセイ");
  ASSERT_FALSE(result.value().morphemes.empty());

  const size_t normalized_length = normalize::utf8Length(result.value().normalized_text);
  EXPECT_EQ(result.value().morphemes.front().start, 0U);
  EXPECT_EQ(result.value().morphemes.back().end, normalized_length);
  for (const auto& morpheme : result.value().morphemes) {
    EXPECT_LE(morpheme.start, morpheme.end);
    EXPECT_LE(morpheme.end, normalized_length);
  }
}

TEST_F(SuzumeApiTest, LowInformationCategoriesDriveTagExclusion) {
  Suzume instance(makeTestOptions());
  auto morphemes = instance.analyze("それ");
  ASSERT_EQ(morphemes.size(), 1U);
  EXPECT_EQ(morphemes.front().extended_pos, core::ExtendedPOS::Pronoun);
  EXPECT_TRUE(morphemes.front().isLowInformation());
  EXPECT_TRUE(instance.generateTags("それ").empty());

  postprocess::TagGeneratorOptions options;
  options.exclude_low_info = false;
  options.min_tag_length = 1;
  auto included = instance.generateTags("それ", options);
  ASSERT_EQ(included.size(), 1U);
  EXPECT_EQ(included.front().tag, "それ");
}

TEST_F(SuzumeApiTest, NumericKatakanaMergesAsQuantity) {
  Suzume instance(makeTestOptions());

  // MeCab treats a numeral + any katakana noun as a single quantity token, so
  // this is a general rule rather than a curated unit list — both measurement
  // units and arbitrary katakana nouns merge.
  auto meter = instance.analyze("5メートル");
  ASSERT_FALSE(meter.empty());
  EXPECT_EQ(meter.front().surface, "5メートル");

  auto cut = instance.analyze("3カット");
  ASSERT_FALSE(cut.empty());
  EXPECT_EQ(cut.front().surface, "3カット");

  auto pattern = instance.analyze("10パターン");
  ASSERT_FALSE(pattern.empty());
  EXPECT_EQ(pattern.front().surface, "10パターン");
}

TEST_F(SuzumeApiTest, AnalyzeSingleCharacter) {
  Suzume instance(makeTestOptions());
  // Single kanji "mountain"
  auto results = instance.analyze("\xe5\xb1\xb1");
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, DefaultOptionsPreserveUnicodeLettersAndUnclassifiedText) {
  Suzume instance(makeTestOptions());

  for (const std::string_view text : {"café", "Москва", "서울", "ไทย", "𫠠", "ا"}) {
    const auto results = instance.analyze(text);
    std::string reconstructed;
    for (const auto& morpheme : results) {
      EXPECT_NE(morpheme.pos, core::PartOfSpeech::Symbol) << text;
      reconstructed += morpheme.surface;
    }
    EXPECT_EQ(reconstructed, text) << text;
  }
}

TEST_F(SuzumeApiTest, DefaultOptionsPreserveUnicodeSymbolsAndEmojiWithFullOffsetCoverage) {
  Suzume instance(makeTestOptions());

  for (const std::string_view text :
       {"温度25℃", "①を確認", "あ→い", "価格€", "番号№", "連絡℡", "紙★", "👨", "￥", "$", "§", "°", "±", "™", "©"}) {
    const auto results = instance.analyze(text);
    std::string reconstructed;
    size_t next_offset = 0;
    for (const auto& morpheme : results) {
      reconstructed += morpheme.surface;
      EXPECT_EQ(morpheme.start, next_offset) << text;
      next_offset = morpheme.end;
    }
    EXPECT_EQ(reconstructed, text) << text;
    EXPECT_EQ(next_offset, normalize::toCodepoints(text).size()) << text;
    EXPECT_NE(std::find_if(results.begin(), results.end(),
                           [](const auto& morpheme) { return morpheme.pos == core::PartOfSpeech::Other; }),
              results.end())
        << text;
  }
}

TEST_F(SuzumeApiTest, ControlCharactersAreRemovableSymbolBoundaries) {
  Suzume instance(makeTestOptions());
  const auto results = instance.analyze("東京\tみかん\r大阪\u00A0りんご");

  ASSERT_FALSE(results.empty());
  for (const auto& morpheme : results) {
    EXPECT_NE(morpheme.pos, core::PartOfSpeech::Symbol);
    EXPECT_EQ(morpheme.surface.find_first_of("\t\r"), std::string::npos);
  }
  const auto citrus =
      std::find_if(results.begin(), results.end(), [](const auto& morpheme) { return morpheme.surface == "みかん"; });
  ASSERT_NE(citrus, results.end());
  EXPECT_EQ(citrus->pos, core::PartOfSpeech::Noun);
}

TEST_F(SuzumeApiTest, PreservedNonWordCodepointsRetainSurfaceAcrossPunctuationBoundaries) {
  SuzumeOptions options = makeTestOptions();
  options.remove_symbols = false;
  Suzume instance(options);

  const auto results = instance.analyze("○×（￣▽￣）");
  std::string reconstructed;
  for (const auto& morpheme : results) {
    reconstructed += morpheme.surface;
  }
  EXPECT_EQ(reconstructed, "○×(￣▽￣)");
  EXPECT_NE(std::find_if(results.begin(), results.end(),
                         [](const auto& morpheme) { return morpheme.pos == core::PartOfSpeech::Other; }),
            results.end());
}

TEST_F(SuzumeApiTest, IdeographicVariationSelectorStaysWithItsWord) {
  Suzume instance(makeTestOptions());

  const auto results = instance.analyze("葛󠄀城市");
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results.front().surface, "葛󠄀城市");
  EXPECT_EQ(results.front().pos, core::PartOfSpeech::Noun);
}

TEST_F(SuzumeApiTest, ZeroWidthSpaceIsRemovedBeforeAnalysis) {
  Suzume instance(makeTestOptions());

  const auto results = instance.analyze("東京\u200B都");
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results.front().surface, "東京都");
  EXPECT_EQ(results.front().pos, core::PartOfSpeech::Noun);
}

TEST_F(SuzumeApiTest, ProlongedMarkWidthProducesIdenticalAnalysis) {
  Suzume instance(makeTestOptions());

  const auto fullwidth = instance.analyze("長いーー音");
  const auto halfwidth = instance.analyze("長いｰｰ音");
  ASSERT_EQ(fullwidth.size(), halfwidth.size());
  for (size_t idx = 0; idx < fullwidth.size(); ++idx) {
    EXPECT_EQ(fullwidth[idx].surface, halfwidth[idx].surface);
    EXPECT_EQ(fullwidth[idx].lemma, halfwidth[idx].lemma);
    EXPECT_EQ(fullwidth[idx].pos, halfwidth[idx].pos);
    EXPECT_EQ(fullwidth[idx].start, halfwidth[idx].start);
    EXPECT_EQ(fullwidth[idx].end, halfwidth[idx].end);
  }
}

TEST_F(SuzumeApiTest, NumericSnakeCaseIdentifierStaysWhole) {
  Suzume instance(makeTestOptions());

  const auto results = instance.analyze("2024_01");
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results.front().surface, "2024_01");
  EXPECT_EQ(results.front().pos, core::PartOfSpeech::Noun);
}

TEST_F(SuzumeApiTest, PretokenizedMorphemesHaveExtendedPos) {
  Suzume instance(makeTestOptions());
  auto results = instance.analyze("https://example.com");
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(results[0].extended_pos, core::ExtendedPOS::Noun);
}

TEST_F(SuzumeApiTest, SimilitudeYouRemainsAFormalNoun) {
  Suzume instance(makeTestOptions());

  for (const std::string_view text :
       {"夢のようだ", "読むようにする", "このような方法", "お待ちくださいますようお願いします"}) {
    auto results = instance.analyze(text);
    auto iter = std::find_if(results.begin(), results.end(),
                             [](const core::Morpheme& morpheme) { return morpheme.surface == "よう"; });
    ASSERT_NE(iter, results.end()) << text;
    EXPECT_EQ(iter->pos, core::PartOfSpeech::Noun) << text;
    EXPECT_EQ(iter->extended_pos, core::ExtendedPOS::NounFormal) << text;
    EXPECT_TRUE(iter->isFormalNoun()) << text;
  }

  auto volitional = instance.analyze("見よう");
  auto iter = std::find_if(volitional.begin(), volitional.end(),
                           [](const core::Morpheme& morpheme) { return morpheme.surface == "う"; });
  ASSERT_NE(iter, volitional.end());
  EXPECT_EQ(iter->pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(iter->extended_pos, core::ExtendedPOS::AuxVolitional);
  EXPECT_FALSE(iter->isFormalNoun());
}

TEST_F(SuzumeApiTest, FinalParticleQuotationUsesQuotativeExtendedPos) {
  Suzume instance(makeTestOptions());

  auto quotation = instance.analyze("行くかと尋ねた");
  auto quote = std::find_if(quotation.begin(), quotation.end(),
                            [](const core::Morpheme& morpheme) { return morpheme.surface == "と"; });
  ASSERT_NE(quote, quotation.end());
  EXPECT_EQ(quote->extended_pos, core::ExtendedPOS::ParticleQuote);

  auto companion = instance.analyze("誰かと話す");
  auto case_particle = std::find_if(companion.begin(), companion.end(),
                                    [](const core::Morpheme& morpheme) { return morpheme.surface == "と"; });
  ASSERT_NE(case_particle, companion.end());
  EXPECT_EQ(case_particle->extended_pos, core::ExtendedPOS::ParticleCase);
}

TEST_F(SuzumeApiTest, AttributivePredicateKeepsTemporalMaAsNoun) {
  Suzume instance(makeTestOptions());

  for (const std::string_view text : {"長い間続いた", "短い間休む", "待つ間休む", "休む間もなく働いた"}) {
    auto results = instance.analyze(text);
    auto interval = std::find_if(results.begin(), results.end(),
                                 [](const core::Morpheme& morpheme) { return morpheme.surface == "間"; });
    ASSERT_NE(interval, results.end()) << text;
    EXPECT_EQ(interval->pos, core::PartOfSpeech::Noun) << text;
  }

  auto lexical_compound = instance.analyze("期間を確認する");
  ASSERT_FALSE(lexical_compound.empty());
  EXPECT_EQ(lexical_compound.front().surface, "期間");
}

TEST_F(SuzumeApiTest, LexicalMamonakuRemainsAnAdverb) {
  Suzume instance(makeTestOptions());

  for (const std::string_view text : {"間もなく到着する", "終了間もなく報告する"}) {
    auto results = instance.analyze(text);
    auto temporal_adverb = std::find_if(results.begin(), results.end(),
                                        [](const core::Morpheme& morpheme) { return morpheme.surface == "間もなく"; });
    ASSERT_NE(temporal_adverb, results.end()) << text;
    EXPECT_EQ(temporal_adverb->pos, core::PartOfSpeech::Adverb) << text;
    EXPECT_EQ(temporal_adverb->lemma, "間もなく") << text;
  }

  auto duration = instance.analyze("時間もなく終わった");
  ASSERT_GE(duration.size(), 2U);
  EXPECT_EQ(duration[0].surface, "時間");
  EXPECT_EQ(duration[1].surface, "も");
}

TEST_F(SuzumeApiTest, AdverbHomographsRespectNominalFrames) {
  Suzume instance(makeTestOptions());

  // An adverb takes no case of its own, so a genitive makes it a nominal too.
  for (const std::string_view text : {"一切を任せる", "一切合切を確認する", "むしろを使う", "まったくの偶然"}) {
    auto results = instance.analyze(text);
    ASSERT_FALSE(results.empty()) << text;
    EXPECT_EQ(results.front().pos, core::PartOfSpeech::Noun) << text;
  }

  for (const std::string_view text : {"一切確認する", "一切合切確認する", "このほど確認した", "むしろ必要だ"}) {
    auto results = instance.analyze(text);
    ASSERT_FALSE(results.empty()) << text;
    EXPECT_EQ(results.front().pos, core::PartOfSpeech::Adverb) << text;
  }
}

TEST_F(SuzumeApiTest, BareNominalNegativeUsesIndependentAdjective) {
  Suzume instance(makeTestOptions());

  for (const std::string_view text : {"問題ない", "関係ない", "本にちがいない"}) {
    auto results = instance.analyze(text);
    ASSERT_GE(results.size(), 2U) << text;
    const auto& negative = results.back();
    EXPECT_EQ(negative.surface, "ない") << text;
    EXPECT_EQ(negative.pos, core::PartOfSpeech::Adjective) << text;
    EXPECT_EQ(negative.lemma, "ない") << text;
  }

  auto attributive = instance.analyze("頼りない返事");
  ASSERT_GE(attributive.size(), 3U);
  EXPECT_EQ(attributive[0].surface, "頼り");
  EXPECT_EQ(attributive[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(attributive[1].surface, "ない");
  EXPECT_EQ(attributive[1].pos, core::PartOfSpeech::Adjective);
}

TEST_F(SuzumeApiTest, TemporalNaoIsAdverbial) {
  Suzume instance(makeTestOptions());

  auto temporal = instance.analyze("いまなお確認できる");
  ASSERT_GE(temporal.size(), 2U);
  EXPECT_EQ(temporal[0].pos, core::PartOfSpeech::Adverb);
  EXPECT_EQ(temporal[1].surface, "なお");
  EXPECT_EQ(temporal[1].pos, core::PartOfSpeech::Adverb);

  auto connective = instance.analyze("なお確認する");
  ASSERT_FALSE(connective.empty());
  EXPECT_EQ(connective.front().surface, "なお");
  EXPECT_EQ(connective.front().pos, core::PartOfSpeech::Conjunction);
}

TEST_F(SuzumeApiTest, DemonstrativeIdentificationEndsInNoun) {
  Suzume instance(makeTestOptions());

  for (const std::string_view text : {"これが答え。", "それが答え。"}) {
    auto results = instance.analyze(text);
    ASSERT_FALSE(results.empty()) << text;
    const auto& answer = results.back();
    EXPECT_EQ(answer.surface, "答え") << text;
    EXPECT_EQ(answer.pos, core::PartOfSpeech::Noun) << text;
    EXPECT_EQ(answer.lemma, "答え") << text;
  }

  auto imperative = instance.analyze("君が急げ。");
  ASSERT_FALSE(imperative.empty());
  const auto& hurry = imperative.back();
  EXPECT_EQ(hurry.surface, "急げ");
  EXPECT_EQ(hurry.pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(hurry.lemma, "急ぐ");
}

TEST_F(SuzumeApiTest, PreservingSymbolsDoesNotChangeNeighboringRoles) {
  SuzumeOptions filtered_options = makeTestOptions();
  filtered_options.remove_symbols = true;
  Suzume filtered(filtered_options);

  SuzumeOptions preserved_options = makeTestOptions();
  preserved_options.remove_symbols = false;
  Suzume preserved(preserved_options);

  const auto filtered_results = filtered.analyze("「本ソフト」を使う");
  const auto preserved_results = preserved.analyze("「本ソフト」を使う");
  const auto find_hon = [](const std::vector<core::Morpheme>& morphemes) {
    return std::find_if(morphemes.begin(), morphemes.end(),
                        [](const core::Morpheme& morpheme) { return morpheme.surface == "本"; });
  };
  const auto filtered_hon = find_hon(filtered_results);
  const auto preserved_hon = find_hon(preserved_results);
  ASSERT_NE(filtered_hon, filtered_results.end());
  ASSERT_NE(preserved_hon, preserved_results.end());
  EXPECT_EQ(preserved_hon->pos, filtered_hon->pos);
  EXPECT_EQ(preserved_hon->extended_pos, filtered_hon->extended_pos);
}

TEST_F(SuzumeApiTest, DebugAnalysisUsesTheProductionPretokenizerPipeline) {
  Suzume instance(makeTestOptions());
  const std::string text = "2024年12月23日にhttps://example.com/abcを見た。価格は1,000円。";
  const auto production = instance.analyze(text);
  core::Lattice lattice(0);
  const auto debug = instance.analyzeDebug(text, &lattice);

  ASSERT_EQ(debug.size(), production.size());
  for (size_t index = 0; index < production.size(); ++index) {
    EXPECT_EQ(debug[index].surface, production[index].surface);
    EXPECT_EQ(debug[index].pos, production[index].pos);
    EXPECT_EQ(debug[index].lemma, production[index].lemma);
    EXPECT_EQ(debug[index].start, production[index].start);
    EXPECT_EQ(debug[index].end, production[index].end);
  }
  EXPECT_LT(lattice.textLength(), production.back().end);
}

TEST_F(SuzumeApiTest, GenerateTagsReturnsResults) {
  Suzume instance(makeTestOptions());
  // "Tokyo is beautiful"
  auto tags = instance.generateTags("\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84");
  EXPECT_FALSE(tags.empty());

  // Tags should have non-empty tag strings
  for (const auto& entry : tags) {
    EXPECT_FALSE(entry.tag.empty());
  }
}

TEST_F(SuzumeApiTest, GenerateTagsWithCustomOptions) {
  Suzume instance(makeTestOptions());
  postprocess::TagGeneratorOptions tag_opts;
  tag_opts.exclude_particles = true;
  tag_opts.min_tag_length = 1;

  auto tags =
      instance.generateTags("\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xaf\xe7\xbe\x8e\xe3\x81\x97\xe3\x81\x84", tag_opts);
  EXPECT_FALSE(tags.empty());
}

TEST_F(SuzumeApiTest, GenerateTagsEmptyText) {
  Suzume instance(makeTestOptions());
  auto tags = instance.generateTags("");
  EXPECT_TRUE(tags.empty());
}

TEST_F(SuzumeApiTest, GenerateTagsResultPreservesMalformedInputErrors) {
  Suzume instance(makeTestOptions());
  const std::string invalid_utf8("\xE3\x81", 2);

  const auto default_result = instance.generateTagsResult(invalid_utf8);
  ASSERT_FALSE(default_result.hasValue());
  EXPECT_EQ(default_result.error().code, core::ErrorCode::InvalidUtf8);
  EXPECT_TRUE(instance.generateTags(invalid_utf8).empty());

  postprocess::TagGeneratorOptions options;
  options.min_tag_length = 1;
  const auto custom_result = instance.generateTagsResult(invalid_utf8, options);
  ASSERT_FALSE(custom_result.hasValue());
  EXPECT_EQ(custom_result.error().code, core::ErrorCode::InvalidUtf8);
  EXPECT_TRUE(instance.generateTags(invalid_utf8, options).empty());
}

TEST_F(SuzumeApiTest, ClearUserDictionariesDiscardsOnlyRuntimeWarnings) {
  SuzumeOptions options = makeTestOptions();
  options.skip_core_dictionary = true;
  Suzume instance(options);
  const auto construction_warnings = instance.dictionaryWarnings();
  const std::string source = "ignored-record\n検査語\tNOUN\n";

  ASSERT_EQ(instance.loadUserDictionaryFromMemoryResult(source.data(), source.size()).value(), 1u);
  const auto after_load = instance.dictionaryWarnings();
  ASSERT_EQ(after_load.size(), construction_warnings.size() + 1u);
  EXPECT_NE(after_load.back().find("line 1"), std::string::npos);

  instance.clearUserDictionaries();
  EXPECT_EQ(instance.dictionaryWarnings(), construction_warnings);

  ASSERT_EQ(instance.loadUserDictionaryFromMemoryResult(source.data(), source.size()).value(), 1u);
  EXPECT_EQ(instance.dictionaryWarnings().size(), construction_warnings.size() + 1u);
  instance.clearUserDictionaries();
  EXPECT_EQ(instance.dictionaryWarnings(), construction_warnings);
}

TEST_F(SuzumeApiTest, MoveConstruct) {
  Suzume src(makeTestOptions());
  Suzume dst = std::move(src);

  // Moved-to instance should work
  auto results = dst.analyze("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");  // test
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, MoveAssign) {
  Suzume src(makeTestOptions());
  Suzume dst(makeTestOptions());
  dst = std::move(src);

  // Moved-to instance should work
  auto results = dst.analyze("\xe3\x82\x8a\xe3\x82\x93\xe3\x81\x94");  // ringo
  EXPECT_FALSE(results.empty());
}

TEST_F(SuzumeApiTest, AnalyzeWithLemmatizeOption) {
  SuzumeOptions lemmatized_options = makeTestOptions();
  lemmatized_options.lemmatize = true;
  Suzume lemmatized(lemmatized_options);
  SuzumeOptions source_lemma_options = makeTestOptions();
  source_lemma_options.lemmatize = false;
  Suzume source_lemma(source_lemma_options);

  const auto corrected = lemmatized.analyze("歩きます");
  const auto original = source_lemma.analyze("歩きます");
  ASSERT_EQ(corrected.size(), 2u);
  ASSERT_EQ(original.size(), corrected.size());
  EXPECT_EQ(corrected[0].surface, "歩き");
  EXPECT_EQ(corrected[0].lemma, "歩く");
  EXPECT_EQ(original[0].lemma, "歩き");
}

TEST_F(SuzumeApiTest, AnalyzeWithMergeCompoundsOption) {
  SuzumeOptions split_options = makeTestOptions();
  split_options.merge_compounds = false;
  Suzume split(split_options);
  SuzumeOptions merged_options = makeTestOptions();
  merged_options.merge_compounds = true;
  Suzume merged(merged_options);

  const auto separate = split.analyze("東京2024");
  const auto combined = merged.analyze("東京2024");
  ASSERT_EQ(separate.size(), 2u);
  EXPECT_EQ(separate[0].surface, "東京");
  EXPECT_EQ(separate[1].surface, "2024");
  ASSERT_EQ(combined.size(), 1u);
  EXPECT_EQ(combined[0].surface, "東京2024");
  EXPECT_EQ(combined[0].start, separate.front().start);
  EXPECT_EQ(combined[0].end, separate.back().end);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromInvalidPath) {
  Suzume instance(makeTestOptions());
  bool result = instance.loadUserDictionary("/nonexistent/path/dict.csv");
  EXPECT_FALSE(result);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryResultReportsInvalidPath) {
  Suzume instance(makeTestOptions());
  auto result = instance.loadUserDictionaryResult("/nonexistent/path/dict.csv");
  ASSERT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, core::ErrorCode::FileNotFound);
  EXPECT_NE(result.error().message.find("Failed to open dictionary file"), std::string::npos);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromNullMemory) {
  Suzume instance(makeTestOptions());
  bool result = instance.loadUserDictionaryFromMemory(nullptr, 0);
  EXPECT_FALSE(result);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromMemoryResultReportsParseError) {
  Suzume instance(makeTestOptions());
  const char* csv_data = "\"東京,NOUN,0.5\n";
  auto result = instance.loadUserDictionaryFromMemoryResult(csv_data, std::strlen(csv_data));
  ASSERT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, core::ErrorCode::ParseError);
  EXPECT_NE(result.error().message.find("unterminated quoted field"), std::string::npos);
}

TEST_F(SuzumeApiTest, LoadUserDictionaryFromMemoryResultReportsEntryCount) {
  Suzume instance(makeTestOptions());
  const char* csv_data =
      "東京,NOUN,0.5\n"
      "大阪,NOUN,0.5\n";
  auto result = instance.loadUserDictionaryFromMemoryResult(csv_data, std::strlen(csv_data));
  ASSERT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), 2u);
}

TEST_F(SuzumeApiTest, SourceDictionaryReportsExpandedInstallCountAndSkippedLines) {
  Suzume instance(makeTestOptions());
  const char* source =
      "missing-pos\n"
      "検査する\tVERB\tSURU\n";

  auto result = instance.loadUserDictionaryFromMemoryResult(source, std::strlen(source));

  ASSERT_TRUE(result.hasValue());
  EXPECT_GT(result.value(), 1u);
  ASSERT_FALSE(instance.dictionaryWarnings().empty());
  EXPECT_NE(instance.dictionaryWarnings().back().find("line 1"), std::string::npos);
}

TEST_F(SuzumeApiTest, LoadBinaryDictionaryFromInvalidMemory) {
  Suzume instance(makeTestOptions());
  const uint8_t bad_data[] = {0x00, 0x01, 0x02, 0x03};
  bool result = instance.loadBinaryDictionary(bad_data, sizeof(bad_data));
  EXPECT_FALSE(result);
}

TEST_F(SuzumeApiTest, LoadBinaryDictionaryResultReportsLoadError) {
  Suzume instance(makeTestOptions());
  const uint8_t bad_data[] = {0x00, 0x01, 0x02, 0x03};
  auto result = instance.loadBinaryDictionaryResult(bad_data, sizeof(bad_data));
  ASSERT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, core::ErrorCode::DictionaryLoadFailed);
  EXPECT_NE(result.error().message.find("Dictionary file too small"), std::string::npos);
}

TEST_F(SuzumeApiTest, ClearUserDictionariesRemovesRuntimeSourceDictionary) {
  Suzume instance(makeTestOptions());
  const char* source = "東京テスト\tNOUN\n";
  ASSERT_TRUE(instance.loadUserDictionaryFromMemoryResult(source, std::strlen(source)).hasValue());

  auto loaded = instance.analyze("東京テスト");
  ASSERT_TRUE(std::any_of(loaded.begin(), loaded.end(), [](const core::Morpheme& morpheme) {
    return morpheme.surface == "東京テスト" && morpheme.fromUserDict();
  }));

  instance.clearUserDictionaries();

  auto cleared = instance.analyze("東京テスト");
  EXPECT_FALSE(std::any_of(cleared.begin(), cleared.end(),
                           [](const core::Morpheme& morpheme) { return morpheme.fromUserDict(); }));
}

TEST_F(SuzumeApiTest, AutoDictionaryLoadWarningsAreRecorded) {
#ifndef __EMSCRIPTEN__
  namespace fs = std::filesystem;

  const char* old_data_dir = std::getenv("SUZUME_DATA_DIR");
  std::string old_value = old_data_dir != nullptr ? old_data_dir : "";

  fs::path dir = fs::temp_directory_path() / "suzume_bad_dict_test";
  fs::create_directories(dir);
  {
    std::ofstream file(dir / "core.dic", std::ios::binary);
    const char bad_data[] = {0x00, 0x01, 0x02, 0x03};
    file.write(bad_data, sizeof(bad_data));
  }

  setenv("SUZUME_DATA_DIR", dir.string().c_str(), 1);
  SuzumeOptions opts = makeTestOptions();
  Suzume instance(opts);
  if (old_data_dir != nullptr) {
    setenv("SUZUME_DATA_DIR", old_value.c_str(), 1);
  } else {
    unsetenv("SUZUME_DATA_DIR");
  }
  fs::remove_all(dir);

  auto warnings = instance.dictionaryWarnings();
  ASSERT_FALSE(warnings.empty());
  EXPECT_NE(warnings.front().find("Failed to auto-load dictionary"), std::string::npos);
  EXPECT_NE(warnings.front().find("Dictionary file too small"), std::string::npos);
#endif
}

TEST_F(SuzumeApiTest, MissingAutomaticCoreDictionaryIsReported) {
#ifndef __EMSCRIPTEN__
  namespace fs = std::filesystem;
  const fs::path old_working_directory = fs::current_path();
  const char* old_data_dir = std::getenv("SUZUME_DATA_DIR");
  const char* old_home = std::getenv("HOME");
  const std::string old_data_value = old_data_dir != nullptr ? old_data_dir : "";
  const std::string old_home_value = old_home != nullptr ? old_home : "";
  const fs::path empty_dir = fs::temp_directory_path() / "suzume_missing_dict_test";
  fs::remove_all(empty_dir);
  fs::create_directories(empty_dir);

  setenv("SUZUME_DATA_DIR", empty_dir.string().c_str(), 1);
  setenv("HOME", empty_dir.string().c_str(), 1);
  fs::current_path(empty_dir);
  SuzumeOptions opts = makeTestOptions();
  Suzume instance(opts);
  fs::current_path(old_working_directory);
  if (old_data_dir != nullptr) {
    setenv("SUZUME_DATA_DIR", old_data_value.c_str(), 1);
  } else {
    unsetenv("SUZUME_DATA_DIR");
  }
  if (old_home != nullptr) {
    setenv("HOME", old_home_value.c_str(), 1);
  } else {
    unsetenv("HOME");
  }
  fs::remove_all(empty_dir);

  EXPECT_FALSE(instance.hasCoreDictionary());
  const auto& warnings = instance.dictionaryWarnings();
  ASSERT_FALSE(warnings.empty());
  EXPECT_NE(warnings.front().find("Dictionary not found"), std::string::npos);
  EXPECT_NE(warnings.front().find("core.dic"), std::string::npos);
  EXPECT_TRUE(std::any_of(warnings.begin(), warnings.end(), [](const std::string& warning) {
    return warning.find("Using external dictionary directory") != std::string::npos;
  }));
#endif
}

TEST_F(SuzumeApiTest, ScorerEnvWarningsAreSilentByDefault) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_UNARY_noun_prior", "not-a-number", 1);
  testing::internal::CaptureStderr();
  { Suzume instance(makeTestOptions()); }
  std::string stderr_output = testing::internal::GetCapturedStderr();
  unsetenv("SUZUME_SCORER_UNARY_noun_prior");

  EXPECT_TRUE(stderr_output.empty());
#endif
}

TEST_F(SuzumeApiTest, ScorerEnvWarningsCanBeReported) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_UNARY_noun_prior", "not-a-number", 1);
  testing::internal::CaptureStderr();
  SuzumeOptions opts = makeTestOptions();
  opts.report_scorer_config = true;
  Suzume instance(opts);
  const std::string stderr_output = testing::internal::GetCapturedStderr();
  unsetenv("SUZUME_SCORER_UNARY_noun_prior");

  EXPECT_TRUE(stderr_output.empty());
  const auto& warnings = instance.dictionaryWarnings();
  ASSERT_FALSE(warnings.empty());
  EXPECT_NE(warnings.front().find("Invalid value"), std::string::npos);
#endif
}

}  // namespace
}  // namespace suzume
