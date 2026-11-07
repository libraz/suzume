#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "postprocess/postprocessor_internal.h"
#include "suzume.h"

namespace suzume {
namespace postprocess {
namespace {

core::Morpheme token(std::string surface, core::PartOfSpeech pos, core::ExtendedPOS epos, size_t start, size_t end) {
  core::Morpheme result;
  result.surface = surface;
  result.lemma = surface;
  result.pos = pos;
  result.extended_pos = epos;
  result.start = start;
  result.end = end;
  return result;
}

TEST(CopularQuote, SplitsAdjectivalPredicatesWithCompleteMetadata) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto* input : {"無理だって", "無理だって言った", "静かだって聞いた", "元気だって聞いた"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 3U);
    EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjNaAdj);
    EXPECT_EQ(result[1].surface, "だ");
    EXPECT_EQ(result[1].lemma, "だ");
    EXPECT_EQ(result[1].pos, core::PartOfSpeech::Auxiliary);
    EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::AuxCopulaDa);
    EXPECT_EQ(result[1].start, 2U);
    EXPECT_EQ(result[1].end, 3U);
    EXPECT_EQ(result[1].conj_type, dictionary::ConjugationType::None);
    EXPECT_EQ(result[1].conj_form, grammar::ConjForm::Base);
    EXPECT_EQ(result[2].surface, "って");
    EXPECT_EQ(result[2].lemma, "って");
    EXPECT_EQ(result[2].pos, core::PartOfSpeech::Particle);
    EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::ParticleQuote);
    EXPECT_EQ(result[2].start, 3U);
    EXPECT_EQ(result[2].end, 5U);
    EXPECT_EQ(result[2].conj_type, dictionary::ConjugationType::None);
    EXPECT_EQ(result[2].conj_form, grammar::ConjForm::Base);
  }
}

TEST(CopularQuote, PreservesOtherParticleReadings) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto& [input, particle] : {std::pair{"学生だって", "だって"}, std::pair{"学生だって言った", "だって"},
                                        std::pair{"無理でも", "でも"}, std::pair{"無理だの", "だの"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[1].extended_pos, core::ExtendedPOS::ParticleAdverbial);
    EXPECT_EQ(result[1].start, 2U);
    EXPECT_EQ(result[1].surface, particle);
    EXPECT_GE(result[1].end, 4U);
  }
  for (const auto* input : {"無理だってば", "静かだってば"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_EQ(result.size(), 3U);
    EXPECT_EQ(result[1].surface, "だ");
    EXPECT_EQ(result[2].surface, "ってば");
    EXPECT_EQ(result[2].extended_pos, core::ExtendedPOS::ParticleFinal);
  }
  const auto initial = analyzer.analyze("だって雨だ");
  ASSERT_FALSE(initial.empty());
  EXPECT_EQ(initial[0].surface, "だって");
}

TEST(CopularQuote, RequiresSelectedAdjectiveAndContiguousDictionaryEvidence) {
  dictionary::DictionaryManager manager;
  const auto adjective = token("静か", core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjNaAdj, 7, 9);
  const auto fused = token("だって", core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleAdverbial, 9, 12);
  for (const auto variant : {0, 1, 2, 3}) {
    SCOPED_TRACE(variant);
    std::vector<core::Morpheme> result{adjective, fused};
    if (variant == 0) {
      result[0].pos = core::PartOfSpeech::Noun;
      result[0].extended_pos = core::ExtendedPOS::Noun;
    } else if (variant == 1) {
      result[1].start = 10;
      result[1].end = 13;
    } else if (variant == 2) {
      result[1].surface = "だってば";
      result[1].end = 13;
    }
    resolvePrePrefixMorphemeRoles(result, variant == 3 ? nullptr : &manager);
    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[1].surface, variant == 2 ? "だってば" : "だって");
  }
  std::vector<core::Morpheme> result{adjective, fused};
  result[1].conj_type = dictionary::ConjugationType::GodanRa;
  result[1].conj_form = grammar::ConjForm::Renyokei;
  resolvePrePrefixMorphemeRoles(result, &manager);
  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[1].conj_type, dictionary::ConjugationType::None);
  EXPECT_EQ(result[1].conj_form, grammar::ConjForm::Base);
  EXPECT_EQ(result[2].conj_type, dictionary::ConjugationType::None);
  EXPECT_EQ(result[2].conj_form, grammar::ConjForm::Base);
  EXPECT_EQ(result[1].start, 9U);
  EXPECT_EQ(result[1].end, 10U);
  EXPECT_EQ(result[2].start, 10U);
  EXPECT_EQ(result[2].end, 12U);
}

}  // namespace
}  // namespace postprocess
}  // namespace suzume
