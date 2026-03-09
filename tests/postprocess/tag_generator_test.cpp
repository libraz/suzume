#include "postprocess/tag_generator.h"

#include <gtest/gtest.h>

namespace suzume {
namespace postprocess {
namespace {

// Helper to create a morpheme
core::Morpheme makeMorpheme(const std::string& surface, core::PartOfSpeech pos,
                            const std::string& lemma = "") {
  core::Morpheme m;
  m.surface = surface;
  m.pos = pos;
  m.lemma = lemma.empty() ? surface : lemma;
  return m;
}

TEST(TagGeneratorTest, DefaultConstruction) {
  TagGenerator generator;
  std::vector<core::Morpheme> morphemes;
  auto tags = generator.generate(morphemes);
  EXPECT_TRUE(tags.empty());
}

TEST(TagGeneratorTest, GenerateFromNouns) {
  TagGenerator generator;

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("駅", core::PartOfSpeech::Noun));

  auto tags = generator.generate(morphemes);
  // Postprocessor may merge nouns, so just verify we get at least 1 tag
  EXPECT_GE(tags.size(), 1);
}

TEST(TagGeneratorTest, ExcludeParticles) {
  TagGeneratorOptions options;
  options.exclude_particles = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("に", core::PartOfSpeech::Particle));
  morphemes.push_back(makeMorpheme("行く", core::PartOfSpeech::Verb));

  auto tags = generator.generate(morphemes);
  // Particles should be excluded
  for (const auto& tag : tags) {
    EXPECT_NE(tag, "に");
  }
}

TEST(TagGeneratorTest, IncludeParticles) {
  TagGeneratorOptions options;
  options.exclude_particles = false;
  options.min_tag_length = 1;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("に", core::PartOfSpeech::Particle));

  auto tags = generator.generate(morphemes);
  EXPECT_EQ(tags.size(), 2);
}

TEST(TagGeneratorTest, ExcludeAuxiliaries) {
  TagGeneratorOptions options;
  options.exclude_auxiliaries = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("食べ", core::PartOfSpeech::Verb, "食べる"));
  morphemes.push_back(makeMorpheme("た", core::PartOfSpeech::Auxiliary));

  auto tags = generator.generate(morphemes);
  // Auxiliaries should be excluded
  for (const auto& tag : tags) {
    EXPECT_NE(tag, "た");
  }
}

TEST(TagGeneratorTest, IncludeAuxiliaries) {
  TagGeneratorOptions options;
  options.exclude_auxiliaries = false;
  options.min_tag_length = 1;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("食べ", core::PartOfSpeech::Verb, "食べる"));
  morphemes.push_back(makeMorpheme("た", core::PartOfSpeech::Auxiliary));

  auto tags = generator.generate(morphemes);
  bool found_auxiliary = false;
  for (const auto& tag : tags) {
    if (tag == "た") {
      found_auxiliary = true;
    }
  }
  EXPECT_TRUE(found_auxiliary);
}

TEST(TagGeneratorTest, ExcludeConjunction) {
  TagGenerator generator;

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("そして", core::PartOfSpeech::Conjunction));
  morphemes.push_back(makeMorpheme("大阪", core::PartOfSpeech::Noun));

  auto tags = generator.generate(morphemes);
  for (const auto& tag : tags) {
    EXPECT_NE(tag, "そして");
  }
}

TEST(TagGeneratorTest, ExcludeSymbol) {
  TagGenerator generator;

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("。", core::PartOfSpeech::Symbol));

  auto tags = generator.generate(morphemes);
  for (const auto& tag : tags) {
    EXPECT_NE(tag, "。");
  }
}

TEST(TagGeneratorTest, UseLemma) {
  TagGeneratorOptions options;
  options.use_lemma = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("食べた", core::PartOfSpeech::Verb, "食べる"));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "食べる");  // Lemma, not surface
}

TEST(TagGeneratorTest, UseSurface) {
  TagGeneratorOptions options;
  options.use_lemma = false;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("食べた", core::PartOfSpeech::Verb, "食べる"));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "食べた");  // Surface, not lemma
}

TEST(TagGeneratorTest, MinTagLength) {
  TagGeneratorOptions options;
  options.min_tag_length = 2;
  options.exclude_particles = false;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("に", core::PartOfSpeech::Particle));

  auto tags = generator.generate(morphemes);
  // "に" should be excluded (length 1 < min_tag_length 2)
  EXPECT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "東京");
}

TEST(TagGeneratorTest, RemoveDuplicates) {
  TagGeneratorOptions options;
  options.remove_duplicates = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));

  auto tags = generator.generate(morphemes);
  // With deduplication and possible merging, expect 1 tag
  EXPECT_EQ(tags.size(), 1);
}

TEST(TagGeneratorTest, AllowDuplicates) {
  TagGeneratorOptions options;
  options.remove_duplicates = false;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));

  auto tags = generator.generate(morphemes);
  EXPECT_EQ(tags.size(), 2);
}

TEST(TagGeneratorTest, MaxTags) {
  TagGeneratorOptions options;
  options.max_tags = 2;
  TagGenerator generator(options);

  // Use verbs to avoid noun merging
  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb));
  morphemes.push_back(makeMorpheme("飲む", core::PartOfSpeech::Verb));
  morphemes.push_back(makeMorpheme("走る", core::PartOfSpeech::Verb));

  auto tags = generator.generate(morphemes);
  EXPECT_EQ(tags.size(), 2);
}

TEST(TagGeneratorTest, UnlimitedTags) {
  TagGeneratorOptions options;
  options.max_tags = 0;  // Unlimited
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  for (int i = 0; i < 100; ++i) {
    morphemes.push_back(
        makeMorpheme("タグ" + std::to_string(i), core::PartOfSpeech::Noun));
  }

  auto tags = generator.generate(morphemes);
  EXPECT_EQ(tags.size(), 100);
}

TEST(TagGeneratorTest, ExcludeFormalNouns) {
  TagGeneratorOptions options;
  options.exclude_formal_nouns = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  core::Morpheme formal_noun = makeMorpheme("こと", core::PartOfSpeech::Noun);
  formal_noun.features.is_formal_noun = true;
  morphemes.push_back(formal_noun);
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));

  auto tags = generator.generate(morphemes);
  for (const auto& tag : tags) {
    EXPECT_NE(tag, "こと");
  }
}

TEST(TagGeneratorTest, ExcludeLowInfoWords) {
  TagGeneratorOptions options;
  options.exclude_low_info = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  core::Morpheme low_info = makeMorpheme("ある", core::PartOfSpeech::Verb);
  low_info.features.is_low_info = true;
  morphemes.push_back(low_info);
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));

  auto tags = generator.generate(morphemes);
  for (const auto& tag : tags) {
    EXPECT_NE(tag, "ある");
  }
}

TEST(TagGeneratorTest, GenerateFromTextReturnsEmpty) {
  // generateFromText is not implemented (returns empty)
  auto tags = TagGenerator::generateFromText("東京駅");
  EXPECT_TRUE(tags.empty());
}

TEST(TagGeneratorTest, EmptyLemmaFallsBackToSurface) {
  TagGeneratorOptions options;
  options.use_lemma = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  core::Morpheme m;
  m.surface = "東京";
  m.pos = core::PartOfSpeech::Noun;
  m.lemma = "";  // Empty lemma
  morphemes.push_back(m);

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "東京");  // Falls back to surface
}

TEST(TagGeneratorTest, CountCharsJapanese) {
  TagGeneratorOptions options;
  options.min_tag_length = 3;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));   // 2 chars
  morphemes.push_back(makeMorpheme("新宿駅", core::PartOfSpeech::Noun)); // 3 chars

  auto tags = generator.generate(morphemes);
  EXPECT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "新宿駅");
}

TEST(TagGeneratorTest, PosFilterNounOnly) {
  TagGeneratorOptions options;
  options.pos_filter = kTagPosNoun;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb));
  morphemes.push_back(makeMorpheme("美味しい", core::PartOfSpeech::Adjective));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "東京");
}

TEST(TagGeneratorTest, PosFilterVerbOnly) {
  TagGeneratorOptions options;
  options.pos_filter = kTagPosVerb;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb));
  morphemes.push_back(makeMorpheme("美味しい", core::PartOfSpeech::Adjective));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "食べる");
}

TEST(TagGeneratorTest, PosFilterAdjectiveOnly) {
  TagGeneratorOptions options;
  options.pos_filter = kTagPosAdjective;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb));
  morphemes.push_back(makeMorpheme("美味しい", core::PartOfSpeech::Adjective));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "美味しい");
}

TEST(TagGeneratorTest, PosFilterAdverbOnly) {
  TagGeneratorOptions options;
  options.pos_filter = kTagPosAdverb;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("とても", core::PartOfSpeech::Adverb));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "とても");
}

TEST(TagGeneratorTest, PosFilterMultiple) {
  TagGeneratorOptions options;
  options.pos_filter = kTagPosNoun | kTagPosVerb;  // NOLINT(hicpp-signed-bitwise): bit flag
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb));
  morphemes.push_back(makeMorpheme("美味しい", core::PartOfSpeech::Adjective));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 2);
  EXPECT_EQ(tags[0], "東京");
  EXPECT_EQ(tags[1], "食べる");
}

TEST(TagGeneratorTest, PosFilterZeroIncludesAll) {
  TagGeneratorOptions options;
  options.pos_filter = 0;  // Include all content words
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb));
  morphemes.push_back(makeMorpheme("美味しい", core::PartOfSpeech::Adjective));

  auto tags = generator.generate(morphemes);
  EXPECT_EQ(tags.size(), 3);
}

TEST(TagGeneratorTest, PosFilterExcludesParticlesAndAuxiliaries) {
  TagGeneratorOptions options;
  options.pos_filter = kTagPosNoun;
  // POS filter should exclude non-filterable categories even if
  // exclude_particles/auxiliaries are false
  options.exclude_particles = false;
  options.exclude_auxiliaries = false;
  options.min_tag_length = 1;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("に", core::PartOfSpeech::Particle));
  morphemes.push_back(makeMorpheme("た", core::PartOfSpeech::Auxiliary));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "東京");
}

TEST(TagGeneratorTest, PosFilterIncludesPronounAsNoun) {
  TagGeneratorOptions options;
  options.pos_filter = kTagPosNoun;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("それ", core::PartOfSpeech::Pronoun));
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "それ");
}

TEST(TagGeneratorTest, ExcludeBasicHiraganaLemma) {
  TagGeneratorOptions options;
  options.exclude_basic = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("たべる", core::PartOfSpeech::Verb, "たべる"));
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb, "食べる"));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 2);
  EXPECT_EQ(tags[0], "東京");
  EXPECT_EQ(tags[1], "食べる");
}

TEST(TagGeneratorTest, ExcludeBasicUsesLemmaNotSurface) {
  TagGeneratorOptions options;
  options.exclude_basic = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  // Surface is kanji but lemma is hiragana - should be excluded
  morphemes.push_back(makeMorpheme("食べ", core::PartOfSpeech::Verb, "たべる"));
  // Surface is hiragana but lemma is kanji - should be included
  morphemes.push_back(makeMorpheme("たべ", core::PartOfSpeech::Verb, "食べる"));

  auto tags = generator.generate(morphemes);
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "食べる");  // Lemma used
}

TEST(TagGeneratorTest, ExcludeBasicEmptyLemmaFallsBackToSurface) {
  TagGeneratorOptions options;
  options.exclude_basic = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  core::Morpheme morph;
  morph.surface = "りんご";
  morph.pos = core::PartOfSpeech::Noun;
  morph.lemma = "";  // Empty lemma, falls back to surface
  morphemes.push_back(morph);

  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));

  auto tags = generator.generate(morphemes);
  // "りんご" has hiragana-only surface (used as fallback), should be excluded
  ASSERT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0], "東京");
}

TEST(TagGeneratorTest, PosFilterAndExcludeBasicCombined) {
  TagGeneratorOptions options;
  options.pos_filter = kTagPosNoun | kTagPosVerb;  // NOLINT(hicpp-signed-bitwise): bit flag
  options.exclude_basic = true;
  TagGenerator generator(options);

  std::vector<core::Morpheme> morphemes;
  morphemes.push_back(makeMorpheme("東京", core::PartOfSpeech::Noun));
  morphemes.push_back(makeMorpheme("ある", core::PartOfSpeech::Verb, "ある"));
  morphemes.push_back(makeMorpheme("美味しい", core::PartOfSpeech::Adjective));
  morphemes.push_back(makeMorpheme("食べる", core::PartOfSpeech::Verb, "食べる"));

  auto tags = generator.generate(morphemes);
  // Noun+Verb filter: excludes Adjective
  // exclude_basic: excludes "ある" (hiragana-only lemma)
  ASSERT_EQ(tags.size(), 2);
  EXPECT_EQ(tags[0], "東京");
  EXPECT_EQ(tags[1], "食べる");
}

}  // namespace
}  // namespace postprocess
}  // namespace suzume
