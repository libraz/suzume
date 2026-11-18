#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "suzume/suzume.hpp"

namespace {

std::vector<std::string> tagTexts(const std::vector<suzume::Tag>& tags) {
  std::vector<std::string> result;
  result.reserve(tags.size());
  for (const auto& tag : tags) {
    result.push_back(tag.tag);
  }
  return result;
}

bool contains(const std::vector<std::string>& tags, const std::string& tag) {
  return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

// The header-only wrapper is what a find_package/pkg-config consumer sees, so
// its surface has to stay aligned with the C ABI and the npm/PyPI bindings.

TEST(SuzumeHppTest, AnalyzeReturnsMorphemesForJapaneseText) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();

  const std::vector<suzume::Morpheme> morphemes = tokenizer.analyze("本を読む");
  ASSERT_FALSE(morphemes.empty());
  EXPECT_EQ(morphemes.front().surface, "本");
  EXPECT_FALSE(morphemes.front().pos.empty());
  EXPECT_FALSE(morphemes.front().base_form.empty());
}

TEST(SuzumeHppTest, RuntimeModeSwitchChangesAnalysisWithoutRecreatingTokenizer) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();
  EXPECT_EQ(tokenizer.mode(), suzume::Mode::Normal);

  const auto normal = tokenizer.analyze("API開発");
  ASSERT_TRUE(tokenizer.setMode(suzume::Mode::Split)) << suzume::Tokenizer::lastError();
  EXPECT_EQ(tokenizer.mode(), suzume::Mode::Split);
  const auto split = tokenizer.analyze("API開発");
  EXPECT_GT(split.size(), normal.size());
}

TEST(SuzumeHppTest, AnalyzeExposesNormalizedTextAndPreservesEmbeddedNull) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();

  const std::string input("東京\0大阪", 13);
  const suzume::AnalysisResult result = tokenizer.analyzeWithNormalizedText(input);
  EXPECT_NE(result.normalized_text.find("大阪"), std::string::npos);
  EXPECT_FALSE(result.morphemes.empty());
}

TEST(SuzumeHppTest, ConjugationLabelsCoverTheSerializedBoundary) {
  EXPECT_TRUE(suzume::detail::conjugationTypeLabel(0).empty());
  EXPECT_EQ(suzume::detail::conjugationTypeLabel(14), "ナ形容詞");
  EXPECT_EQ(suzume::detail::conjugationTypeLabel(15), "感動詞");
  EXPECT_EQ(suzume::detail::conjugationTypeLabel(16), "固有名詞・姓");
  EXPECT_EQ(suzume::detail::conjugationTypeLabel(17), "固有名詞・名");
  EXPECT_TRUE(suzume::detail::conjugationTypeLabel(18).empty());

  EXPECT_EQ(suzume::detail::conjugationFormLabel(6), "意志形");
  EXPECT_TRUE(suzume::detail::conjugationFormLabel(7).empty());
}

TEST(SuzumeHppTest, AnalyzeExposesAuxiliaryConjugationMetadata) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();

  const auto morphemes = tokenizer.analyze("書かなかった");
  const auto negative = std::find_if(morphemes.begin(), morphemes.end(),
                                     [](const suzume::Morpheme& morpheme) { return morpheme.surface == "なかっ"; });
  ASSERT_NE(negative, morphemes.end());
  EXPECT_TRUE(negative->conj_type.empty());
  EXPECT_EQ(negative->conj_form, "連用形");
}

TEST(SuzumeHppTest, AnalyzeReportsMalformedInputThroughLastError) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();

  const std::vector<suzume::Morpheme> morphemes = tokenizer.analyze(std::string("\xE3\x81", 2));
  EXPECT_TRUE(morphemes.empty());
  EXPECT_NE(suzume::Tokenizer::lastError().find("UTF-8"), std::string::npos);
}

TEST(SuzumeHppTest, GeneratedTagsUseTheSameFieldNameAsTheOtherBindings) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();

  const std::vector<suzume::Tag> tags = tokenizer.generateTags("東京の観光地を調べる");
  ASSERT_FALSE(tags.empty());
  // The field is `tag`, matching TagEntry::tag in the core, Tag.tag in the
  // WASM binding and Tag.tag in the Python binding.
  EXPECT_FALSE(tags.front().tag.empty());
  EXPECT_FALSE(tags.front().pos.empty());
}

TEST(SuzumeHppTest, TagOptionsMapEveryGeneratorFilter) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();
  const std::string_view text = "りんごが歩きます。読むこと。それ。りんご";

  suzume::TagOptions inclusive;
  inclusive.min_length = 1;
  inclusive.exclude_particles = false;
  inclusive.exclude_auxiliaries = false;
  inclusive.exclude_formal_nouns = false;
  inclusive.exclude_low_info = false;
  const auto all = tagTexts(tokenizer.generateTags(text, inclusive));
  EXPECT_TRUE(contains(all, "が"));
  EXPECT_TRUE(contains(all, "ます"));
  EXPECT_TRUE(contains(all, "こと"));
  EXPECT_TRUE(contains(all, "それ"));
  EXPECT_TRUE(contains(all, "歩く"));
  EXPECT_EQ(std::count(all.begin(), all.end(), "りんご"), 1);

  auto surface = inclusive;
  surface.use_lemma = false;
  EXPECT_TRUE(contains(tagTexts(tokenizer.generateTags(text, surface)), "歩き"));

  auto minimum = inclusive;
  minimum.min_length = 2;
  EXPECT_FALSE(contains(tagTexts(tokenizer.generateTags(text, minimum)), "が"));

  auto maximum = inclusive;
  maximum.max_tags = 2;
  EXPECT_EQ(tokenizer.generateTags(text, maximum).size(), 2u);

  auto duplicates = inclusive;
  duplicates.remove_duplicates = false;
  const auto duplicate_tags = tagTexts(tokenizer.generateTags(text, duplicates));
  EXPECT_EQ(std::count(duplicate_tags.begin(), duplicate_tags.end(), "りんご"), 2);

  auto nouns = inclusive;
  nouns.pos_filter = SUZUME_TAG_POS_NOUN;
  const auto noun_tags = tagTexts(tokenizer.generateTags(text, nouns));
  EXPECT_TRUE(contains(noun_tags, "りんご"));
  EXPECT_FALSE(contains(noun_tags, "歩く"));
  EXPECT_FALSE(contains(noun_tags, "が"));

  auto particles = inclusive;
  particles.pos_filter = SUZUME_TAG_POS_PARTICLE;
  EXPECT_TRUE(contains(tagTexts(tokenizer.generateTags(text, particles)), "が"));

  auto auxiliaries = inclusive;
  auxiliaries.pos_filter = SUZUME_TAG_POS_AUXILIARY;
  EXPECT_TRUE(contains(tagTexts(tokenizer.generateTags(text, auxiliaries)), "ます"));

  auto non_basic = inclusive;
  non_basic.exclude_basic = true;
  const auto non_basic_tags = tagTexts(tokenizer.generateTags(text, non_basic));
  EXPECT_TRUE(contains(non_basic_tags, "歩く"));
  EXPECT_FALSE(contains(non_basic_tags, "りんご"));

  auto no_particles = inclusive;
  no_particles.exclude_particles = true;
  EXPECT_FALSE(contains(tagTexts(tokenizer.generateTags(text, no_particles)), "が"));
  auto no_auxiliaries = inclusive;
  no_auxiliaries.exclude_auxiliaries = true;
  EXPECT_FALSE(contains(tagTexts(tokenizer.generateTags(text, no_auxiliaries)), "ます"));
  auto no_formal_nouns = inclusive;
  no_formal_nouns.exclude_formal_nouns = true;
  EXPECT_FALSE(contains(tagTexts(tokenizer.generateTags(text, no_formal_nouns)), "こと"));
  auto no_low_info = inclusive;
  no_low_info.exclude_low_info = true;
  EXPECT_FALSE(contains(tagTexts(tokenizer.generateTags(text, no_low_info)), "それ"));
}

TEST(SuzumeHppTest, MoveInvalidatesSourceAndLeavesDestinationUsable) {
  suzume::Tokenizer source;
  ASSERT_TRUE(source.valid()) << suzume::Tokenizer::lastError();
  suzume::Tokenizer destination(std::move(source));

  EXPECT_FALSE(source.valid());
  EXPECT_TRUE(source.analyze("東京").empty());
  EXPECT_TRUE(source.analyzeWithNormalizedText("東京").morphemes.empty());
  EXPECT_TRUE(source.generateTags("東京").empty());
  EXPECT_TRUE(source.generateTags("東京", suzume::TagOptions{}).empty());
  EXPECT_EQ(source.loadUserDictionaryCount("検査語\tNOUN\n"), 0u);
  EXPECT_EQ(source.loadUserDictionaryCount(std::string_view{}), 0u);
  const std::uint8_t invalid_dictionary[] = {0, 1, 2, 3};
  EXPECT_FALSE(source.loadBinaryDictionary(invalid_dictionary, sizeof(invalid_dictionary)));
  EXPECT_FALSE(source.clearUserDictionaries());
  EXPECT_FALSE(source.hasCoreDictionary());
  EXPECT_TRUE(source.dictionaryWarnings().empty());
  EXPECT_EQ(source.mode(), suzume::Mode::Normal);
  EXPECT_FALSE(source.setMode(suzume::Mode::Split));

  EXPECT_FALSE(destination.analyze("東京").empty());
  EXPECT_FALSE(destination.generateTags("東京").empty());
}

TEST(SuzumeHppTest, LoadBinaryDictionaryReportsFailureThroughTheWrapper) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();
  const std::uint8_t invalid_dictionary[] = {0, 1, 2, 3};
  EXPECT_FALSE(tokenizer.loadBinaryDictionary(invalid_dictionary, sizeof(invalid_dictionary)));
  EXPECT_NE(suzume::Tokenizer::lastError().find("Dictionary file too small"), std::string::npos);

  EXPECT_EQ(tokenizer.loadUserDictionaryCount(std::string_view{}), 0u);
  EXPECT_NE(suzume::Tokenizer::lastError().find("Empty dictionary data"), std::string::npos);
}

TEST(SuzumeHppTest, DictionaryWarningsAndClearAreExposed) {
  suzume::Tokenizer tokenizer;
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();
  EXPECT_TRUE(tokenizer.dictionaryWarnings().empty());
  EXPECT_TRUE(tokenizer.hasCoreDictionary());
  const auto bundled_before = tokenizer.analyze("コーヒー豆");
  ASSERT_EQ(bundled_before.size(), 1u);
  EXPECT_TRUE(bundled_before[0].is_user_dict);
  EXPECT_GT(tokenizer.loadUserDictionaryCount("検査する\tVERB\tSURU\n"), 1u);
  EXPECT_TRUE(tokenizer.clearUserDictionaries());
  EXPECT_EQ(suzume::Tokenizer::lastErrorCode(), SUZUME_ERROR_SUCCESS);
  const auto bundled_after = tokenizer.analyze("コーヒー豆");
  ASSERT_EQ(bundled_after.size(), 1u);
  EXPECT_TRUE(bundled_after[0].is_user_dict);
}

TEST(SuzumeHppTest, EnvironmentScorerConfigCanBeDisabled) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_INFL_confidence_ceiling", "0", 1);
  suzume::Options options;
  options.skip_user_dictionary = true;
  options.skip_core_dictionary = true;
  options.skip_env_config = true;
  suzume::Tokenizer tokenizer(options);
  unsetenv("SUZUME_SCORER_INFL_confidence_ceiling");
  ASSERT_TRUE(tokenizer.valid()) << suzume::Tokenizer::lastError();

  const auto morphemes = tokenizer.analyze("歩いています");
  ASSERT_FALSE(morphemes.empty());
  EXPECT_EQ(morphemes.front().surface, "歩い");
#endif
}

}  // namespace
