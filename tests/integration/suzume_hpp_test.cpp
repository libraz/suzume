#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dictionary/binary_dict.h"
#include "suzume/suzume.hpp"

namespace {

std::vector<std::string> tagTexts(const suzume::Expected<std::vector<suzume::Tag>>& tags) {
  std::vector<std::string> result;
  if (!tags) {
    return result;
  }
  result.reserve(tags->size());
  for (const auto& tag : *tags) {
    result.push_back(tag.tag);
  }
  return result;
}

bool contains(const std::vector<std::string>& tags, const std::string& tag) {
  return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

suzume::Tokenizer createTokenizer(const suzume::Options& options = {}) {
  auto created = suzume::Tokenizer::create(options);
  if (!created) {
    ADD_FAILURE() << created.error().message;
    std::abort();
  }
  return std::move(created).value();
}

void expectError(const suzume::Error& error, suzume::ErrorCode code, const std::string& message,
                 const std::string& operation) {
  EXPECT_EQ(error.code, code);
  EXPECT_EQ(error.message, message);
  EXPECT_EQ(error.operation, operation);
}

// The header-only wrapper is what a find_package/pkg-config consumer sees, so
// its surface has to stay aligned with the C ABI and the npm/PyPI bindings.

TEST(SuzumeHppTest, CreateChecksTheAbiRevisionAndSucceeds) {
  ASSERT_EQ(suzume_abi_version(), static_cast<std::uint32_t>(SUZUME_ABI_VERSION));
  auto created = suzume::Tokenizer::create();
  ASSERT_TRUE(created.has_value()) << created.error().message;
  EXPECT_TRUE(static_cast<bool>(created));
  EXPECT_EQ(created.error().code, suzume::ErrorCode::Success);
  EXPECT_TRUE(created->hasCoreDictionary());
}

TEST(SuzumeHppTest, CreateReportsInvalidScorerOptions) {
  suzume::Options options;
  options.scorer_options = "{";
  const auto created = suzume::Tokenizer::create(options);
  ASSERT_FALSE(created.has_value());
  EXPECT_EQ(created.error().code, suzume::ErrorCode::Parse);
  EXPECT_EQ(created.error().operation, "create");
  EXPECT_EQ(created.error().message.rfind("invalid scorer options: ", 0), 0u) << created.error().message;
}

TEST(SuzumeHppTest, VersionIsAFreeFunction) {
  EXPECT_EQ(suzume::version(), std::string(suzume_version()));
  EXPECT_FALSE(suzume::version().empty());
}

TEST(SuzumeHppTest, ErrorCodeFromNativeMapsUnknownCodes) {
  static_assert(suzume::errorCodeFromNative(SUZUME_ERROR_PARSE) == suzume::ErrorCode::Parse);
  static_assert(suzume::errorCodeFromNative(42) == suzume::ErrorCode::Unknown);
  EXPECT_EQ(suzume::errorCodeFromNative(0), suzume::ErrorCode::Success);
  EXPECT_EQ(suzume::errorCodeFromNative(7), suzume::ErrorCode::Internal);
  EXPECT_EQ(suzume::errorCodeFromNative(8), suzume::ErrorCode::Unknown);
  EXPECT_EQ(suzume::errorCodeFromNative(-1), suzume::ErrorCode::Unknown);
  EXPECT_EQ(suzume::errorCodeFromNative(42), suzume::ErrorCode::Unknown);
  EXPECT_EQ(static_cast<int>(suzume::ErrorCode::Unknown), 255);
}

TEST(SuzumeHppTest, AnalyzeReturnsMorphemesForJapaneseText) {
  const auto tokenizer = createTokenizer();

  const auto morphemes = tokenizer.analyze("本を読む");
  ASSERT_TRUE(morphemes) << morphemes.error().message;
  ASSERT_FALSE(morphemes->empty());
  const suzume::Morpheme& first = morphemes->front();
  EXPECT_EQ(first.surface, "本");
  EXPECT_EQ(first.pos, "NOUN");
  EXPECT_EQ(first.pos_ja, "名詞");
  EXPECT_EQ(first.base_form, "本");
  EXPECT_EQ(first.conj_type, std::nullopt);
  EXPECT_EQ(first.conj_form, std::nullopt);
  EXPECT_FALSE(first.extended_pos.empty());
}

TEST(SuzumeHppTest, RuntimeModeSwitchChangesAnalysisWithoutRecreatingTokenizer) {
  auto tokenizer = createTokenizer();
  EXPECT_EQ(tokenizer.mode(), suzume::Mode::Normal);

  const auto normal = tokenizer.analyze("API開発");
  ASSERT_TRUE(normal);
  const auto switched = tokenizer.setMode(suzume::Mode::Split);
  ASSERT_TRUE(switched) << switched.error().message;
  EXPECT_EQ(tokenizer.mode(), suzume::Mode::Split);
  const auto split = tokenizer.analyze("API開発");
  ASSERT_TRUE(split);
  EXPECT_GT(split->size(), normal->size());
}

TEST(SuzumeHppTest, SetModeReportsAnInvalidModeUnderTheModeOperation) {
  auto tokenizer = createTokenizer();
  const auto result = tokenizer.setMode(static_cast<suzume::Mode>(99));
  ASSERT_FALSE(result);
  expectError(result.error(), suzume::ErrorCode::InvalidInput, "invalid mode", "mode");
  EXPECT_EQ(tokenizer.mode(), suzume::Mode::Normal);
}

TEST(SuzumeHppTest, AnalyzeExposesNormalizedTextAndPreservesEmbeddedNull) {
  const auto tokenizer = createTokenizer();

  const std::string input("東京\0大阪", 13);
  const auto result = tokenizer.analyzeWithNormalizedText(input);
  ASSERT_TRUE(result) << result.error().message;
  EXPECT_NE(result->normalized_text.find("大阪"), std::string::npos);
  EXPECT_FALSE(result->morphemes.empty());
}

TEST(SuzumeHppTest, EmptyInputSucceedsWithNoMorphemes) {
  const auto tokenizer = createTokenizer();
  const auto morphemes = tokenizer.analyze("");
  ASSERT_TRUE(morphemes) << morphemes.error().message;
  EXPECT_TRUE(morphemes->empty());
  const auto tags = tokenizer.generateTags(std::string_view{});
  ASSERT_TRUE(tags) << tags.error().message;
  EXPECT_TRUE(tags->empty());
}

TEST(SuzumeHppTest, OptionalLabelTreatsNullAndEmptyAsMissing) {
  EXPECT_EQ(suzume::detail::optionalLabel(nullptr), std::nullopt);
  EXPECT_EQ(suzume::detail::optionalLabel(""), std::nullopt);
  EXPECT_EQ(suzume::detail::optionalLabel(suzume_conjugation_type_label(0)), std::nullopt);
  EXPECT_EQ(suzume::detail::optionalLabel(suzume_conjugation_type_label(14)), "ナ形容詞");
  EXPECT_EQ(suzume::detail::optionalLabel(suzume_conjugation_form_label(6)), "意志形");
  EXPECT_EQ(suzume::detail::optionalLabel(suzume_conjugation_form_label(7)), std::nullopt);

  EXPECT_EQ(suzume::detail::label(nullptr, "OTHER"), "OTHER");
  EXPECT_EQ(suzume::detail::label("", "UNKNOWN"), "UNKNOWN");
  EXPECT_EQ(suzume::detail::label("NOUN", "OTHER"), "NOUN");
}

TEST(SuzumeHppTest, AnalyzeExposesAuxiliaryConjugationMetadata) {
  const auto tokenizer = createTokenizer();

  const auto morphemes = tokenizer.analyze("書かなかった");
  ASSERT_TRUE(morphemes);
  const auto negative = std::find_if(morphemes->begin(), morphemes->end(),
                                     [](const suzume::Morpheme& morpheme) { return morpheme.surface == "なかっ"; });
  ASSERT_NE(negative, morphemes->end());
  EXPECT_EQ(negative->conj_type, std::nullopt);
  EXPECT_EQ(negative->conj_form, "連用形");
}

TEST(SuzumeHppTest, MalformedInputReturnsAnInvalidUtf8Error) {
  const auto tokenizer = createTokenizer();
  const std::string malformed("\xE3\x81", 2);

  const auto morphemes = tokenizer.analyze(malformed);
  ASSERT_FALSE(morphemes);
  expectError(morphemes.error(), suzume::ErrorCode::InvalidUtf8, "invalid UTF-8 input", "analyze");

  const auto normalized = tokenizer.analyzeWithNormalizedText(malformed);
  ASSERT_FALSE(normalized);
  expectError(normalized.error(), suzume::ErrorCode::InvalidUtf8, "invalid UTF-8 input", "analyze");

  const auto tags = tokenizer.generateTags(malformed);
  ASSERT_FALSE(tags);
  expectError(tags.error(), suzume::ErrorCode::InvalidUtf8, "invalid UTF-8 input", "generate_tags");
}

TEST(SuzumeHppTest, GeneratedTagsUseTheSameFieldNameAsTheOtherBindings) {
  const auto tokenizer = createTokenizer();

  const auto tags = tokenizer.generateTags("東京の観光地を調べる");
  ASSERT_TRUE(tags) << tags.error().message;
  ASSERT_FALSE(tags->empty());
  // The field is `tag`, matching TagEntry::tag in the core, Tag.tag in the
  // WASM binding and Tag.tag in the Python binding.
  EXPECT_FALSE(tags->front().tag.empty());
  EXPECT_FALSE(tags->front().pos.empty());
}

TEST(SuzumeHppTest, TagOptionsMapEveryGeneratorFilter) {
  const auto tokenizer = createTokenizer();
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
  surface.use_base_form = false;
  EXPECT_TRUE(contains(tagTexts(tokenizer.generateTags(text, surface)), "歩き"));

  auto minimum = inclusive;
  minimum.min_length = 2;
  EXPECT_FALSE(contains(tagTexts(tokenizer.generateTags(text, minimum)), "が"));

  auto maximum = inclusive;
  maximum.max_tags = 2;
  EXPECT_EQ(tagTexts(tokenizer.generateTags(text, maximum)).size(), 2u);

  auto duplicates = inclusive;
  duplicates.remove_duplicates = false;
  const auto duplicate_tags = tagTexts(tokenizer.generateTags(text, duplicates));
  EXPECT_EQ(std::count(duplicate_tags.begin(), duplicate_tags.end(), "りんご"), 2);

  auto nouns = inclusive;
  nouns.pos_filter = {suzume::TagPos::Noun};
  const auto noun_tags = tagTexts(tokenizer.generateTags(text, nouns));
  EXPECT_TRUE(contains(noun_tags, "りんご"));
  EXPECT_TRUE(contains(noun_tags, "それ"));
  EXPECT_FALSE(contains(noun_tags, "歩く"));
  EXPECT_FALSE(contains(noun_tags, "が"));

  auto repeated_nouns = inclusive;
  repeated_nouns.pos_filter = {suzume::TagPos::Noun, suzume::TagPos::Noun};
  EXPECT_EQ(tagTexts(tokenizer.generateTags(text, repeated_nouns)), noun_tags);

  auto verbs_and_nouns = inclusive;
  verbs_and_nouns.pos_filter = {suzume::TagPos::Verb, suzume::TagPos::Noun};
  const auto verb_noun_tags = tagTexts(tokenizer.generateTags(text, verbs_and_nouns));
  EXPECT_TRUE(contains(verb_noun_tags, "りんご"));
  EXPECT_TRUE(contains(verb_noun_tags, "歩く"));
  EXPECT_FALSE(contains(verb_noun_tags, "ます"));

  auto particles = inclusive;
  particles.pos_filter = {suzume::TagPos::Particle};
  EXPECT_TRUE(contains(tagTexts(tokenizer.generateTags(text, particles)), "が"));

  auto auxiliaries = inclusive;
  auxiliaries.pos_filter = {suzume::TagPos::Aux};
  EXPECT_TRUE(contains(tagTexts(tokenizer.generateTags(text, auxiliaries)), "ます"));

  // A POS named in pos_filter is admitted even though the defaults exclude particles.
  suzume::TagOptions named_particles;
  named_particles.pos_filter = {suzume::TagPos::Particle};
  named_particles.min_length = 1;
  const auto named_particle_tags = tagTexts(tokenizer.generateTags(text, named_particles));
  EXPECT_TRUE(contains(named_particle_tags, "が"));
  EXPECT_FALSE(contains(named_particle_tags, "りんご"));

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

TEST(SuzumeHppTest, UserDictionaryLoaderReturnsTheCountOrAnError) {
  auto tokenizer = createTokenizer();

  const auto loaded = tokenizer.loadUserDictionary("検査語\tNOUN\n検査句\tNOUN\n");
  ASSERT_TRUE(loaded) << loaded.error().message;
  EXPECT_EQ(*loaded, 2u);

  const auto empty = tokenizer.loadUserDictionary(std::string_view{});
  ASSERT_FALSE(empty);
  expectError(empty.error(), suzume::ErrorCode::InvalidInput, "Empty dictionary data", "load_user_dictionary");

  const auto comment_only = tokenizer.loadUserDictionary("# comment only\n");
  ASSERT_FALSE(comment_only);
  EXPECT_EQ(comment_only.error().code, suzume::ErrorCode::Parse);
  EXPECT_EQ(comment_only.error().operation, "load_user_dictionary");
  EXPECT_FALSE(comment_only.error().message.empty());
}

TEST(SuzumeHppTest, BinaryDictionaryLoaderReturnsTheCountOrAnError) {
  suzume::dictionary::BinaryDictWriter writer;
  suzume::dictionary::DictionaryEntry entry;
  entry.surface = "検査語";
  entry.lemma = "検査語";
  entry.pos = suzume::core::PartOfSpeech::Noun;
  writer.addEntry(entry);
  auto built = writer.build();
  ASSERT_TRUE(built.hasValue());
  const std::vector<std::uint8_t>& bytes = built.value();

  auto tokenizer = createTokenizer();
  const auto loaded =
      tokenizer.loadBinaryDictionary(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
  ASSERT_TRUE(loaded) << loaded.error().message;
  EXPECT_EQ(*loaded, 1u);

  const auto invalid = tokenizer.loadBinaryDictionary(std::string_view("\x00\x01\x02\x03", 4));
  ASSERT_FALSE(invalid);
  EXPECT_EQ(invalid.error().code, suzume::ErrorCode::DictionaryLoadFailed);
  EXPECT_EQ(invalid.error().operation, "load_binary_dictionary");
  EXPECT_NE(invalid.error().message.find("Dictionary file too small"), std::string::npos);

  const auto empty = tokenizer.loadBinaryDictionary(std::string_view{});
  ASSERT_FALSE(empty);
  expectError(empty.error(), suzume::ErrorCode::DictionaryLoadFailed, "Empty dictionary data",
              "load_binary_dictionary");
}

TEST(SuzumeHppTest, MovedFromTokenizerReportsTheReleasedError) {
  auto source = createTokenizer();
  auto destination(std::move(source));

  const std::string released = "Suzume instance has been released";
  // NOLINTBEGIN(bugprone-use-after-move): the moved-from contract is what is under test.
  expectError(source.analyze("東京").error(), suzume::ErrorCode::InvalidInput, released, "analyze");
  expectError(source.analyzeWithNormalizedText("東京").error(), suzume::ErrorCode::InvalidInput, released, "analyze");
  expectError(source.generateTags("東京").error(), suzume::ErrorCode::InvalidInput, released, "generate_tags");
  expectError(source.loadUserDictionary("検査語\tNOUN\n").error(), suzume::ErrorCode::InvalidInput, released,
              "load_user_dictionary");
  expectError(source.loadBinaryDictionary(std::string_view("\x00\x01", 2)).error(), suzume::ErrorCode::InvalidInput,
              released, "load_binary_dictionary");
  expectError(source.clearUserDictionaries().error(), suzume::ErrorCode::InvalidInput, released,
              "clear_user_dictionaries");
  expectError(source.setMode(suzume::Mode::Split).error(), suzume::ErrorCode::InvalidInput, released, "mode");
  EXPECT_EQ(source.mode(), suzume::Mode::Normal);
  EXPECT_FALSE(source.hasCoreDictionary());
  EXPECT_TRUE(source.dictionaryWarnings().empty());
  // NOLINTEND(bugprone-use-after-move)

  const auto analyzed = destination.analyze("東京");
  ASSERT_TRUE(analyzed);
  EXPECT_FALSE(analyzed->empty());
}

TEST(SuzumeHppTest, MoveAssignmentTransfersTheHandle) {
  auto first = createTokenizer();
  auto second = createTokenizer();
  ASSERT_TRUE(second.setMode(suzume::Mode::Split));
  first = std::move(second);
  EXPECT_EQ(first.mode(), suzume::Mode::Split);
  EXPECT_EQ(second.mode(), suzume::Mode::Normal);  // NOLINT(bugprone-use-after-move): moved-from default
}

TEST(SuzumeHppTest, DictionaryWarningsAndClearAreExposed) {
  auto tokenizer = createTokenizer();
  EXPECT_TRUE(tokenizer.dictionaryWarnings().empty());
  EXPECT_TRUE(tokenizer.hasCoreDictionary());
  const auto bundled_before = tokenizer.analyze("コーヒー豆");
  ASSERT_TRUE(bundled_before);
  ASSERT_EQ(bundled_before->size(), 1u);
  EXPECT_TRUE((*bundled_before)[0].is_user_dict);
  const auto loaded = tokenizer.loadUserDictionary("検査する\tVERB\tSURU\n");
  ASSERT_TRUE(loaded) << loaded.error().message;
  EXPECT_GT(*loaded, 1u);
  const auto cleared = tokenizer.clearUserDictionaries();
  EXPECT_TRUE(cleared) << cleared.error().message;
  const auto bundled_after = tokenizer.analyze("コーヒー豆");
  ASSERT_TRUE(bundled_after);
  ASSERT_EQ(bundled_after->size(), 1u);
  EXPECT_TRUE((*bundled_after)[0].is_user_dict);
}

TEST(SuzumeHppTest, EnvironmentScorerConfigCanBeDisabled) {
#ifndef __EMSCRIPTEN__
  setenv("SUZUME_SCORER_INFL_confidence_ceiling", "0", 1);
  suzume::Options options;
  options.skip_bundled_user_dictionary = true;
  options.skip_core_dictionary = true;
  options.skip_env_config = true;
  auto created = suzume::Tokenizer::create(options);
  unsetenv("SUZUME_SCORER_INFL_confidence_ceiling");
  ASSERT_TRUE(created) << created.error().message;

  const auto morphemes = created->analyze("歩いています");
  ASSERT_TRUE(morphemes);
  ASSERT_FALSE(morphemes->empty());
  EXPECT_EQ(morphemes->front().surface, "歩い");
#endif
}

}  // namespace
