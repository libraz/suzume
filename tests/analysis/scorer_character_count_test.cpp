/**
 * @file scorer_character_count_test.cpp
 * @brief Connection rules that mean "one character" must count codepoints.
 *
 * A byte-length test (`surface.size() == 3`) is wrong in both directions: it
 * misses a supplementary-plane kanji, which is four bytes, and it fires on an
 * unrelated three-byte ASCII run.
 */

#include <gtest/gtest.h>

#include "analysis/bigram_table.h"
#include "analysis/scorer.h"
#include "analysis/scorer_connection_rules.h"
#include "analysis/scorer_connection_rules_internal.h"
#include "analysis/tokenizer_utils.h"

namespace suzume::analysis {
namespace {

// U+20B9F, a CJK Extension B kanji: one character, four bytes in UTF-8.
constexpr const char* kSupplementaryKanji = "𠮟";

core::LatticeEdge makeNoun(std::string_view surface) {
  core::LatticeEdge edge;
  edge.surface = surface;
  edge.pos = core::PartOfSpeech::Noun;
  edge.extended_pos = core::ExtendedPOS::Noun;
  return edge;
}

core::LatticeEdge makePassiveAuxiliary() {
  core::LatticeEdge edge;
  edge.surface = "れる";
  edge.pos = core::PartOfSpeech::Auxiliary;
  edge.extended_pos = core::ExtendedPOS::AuxPassive;
  return edge;
}

TEST(ScorerCharacterCountTest, SingleKanjiNounBeforePassiveIsPenalizedRegardlessOfEncodedLength) {
  const Scorer scorer;
  const core::LatticeEdge passive = makePassiveAuxiliary();

  const float bmp = scorer.connectionCost(makeNoun("揺"), passive);
  const float supplementary = scorer.connectionCost(makeNoun(kSupplementaryKanji), passive);
  const float multi_kanji = scorer.connectionCost(makeNoun("経済"), passive);

  // The supplementary-plane kanji is one character, so it gets the same
  // treatment as any other single-kanji noun.
  EXPECT_FLOAT_EQ(supplementary, bmp);
  EXPECT_GT(bmp, multi_kanji) << "the penalty applies only to a single-character noun";
}

TEST(ScorerCharacterCountTest, ThreeByteAsciiNounIsNotTreatedAsASingleKanji) {
  const Scorer scorer;
  const core::LatticeEdge passive = makePassiveAuxiliary();

  const float ascii_run = scorer.connectionCost(makeNoun("abc"), passive);
  const float single_kanji = scorer.connectionCost(makeNoun("揺"), passive);
  const float multi_kanji = scorer.connectionCost(makeNoun("経済"), passive);

  EXPECT_LT(ascii_run, single_kanji) << "a three-byte ASCII run is three characters";
  EXPECT_FLOAT_EQ(ascii_run, multi_kanji);
}

TEST(TokenizerUtf8RangeTest, ExtractsMixedWidthCodepointRanges) {
  constexpr std::string_view kText = "Aて𠮟";
  const ByteOffsets byte_offsets = {0, 1, 4, 8};

  EXPECT_EQ(textRange(kText, byte_offsets, 0, 1), "A");
  EXPECT_EQ(textRange(kText, byte_offsets, 1, 2), "て");
  EXPECT_EQ(textRange(kText, byte_offsets, 2, 3), kSupplementaryKanji);
  EXPECT_EQ(textRange(kText, byte_offsets, 0, 3), kText);
}

TEST(TokenizerUtf8RangeTest, InvalidRangesReturnEmptyViews) {
  constexpr std::string_view kText = "東京";
  const ByteOffsets byte_offsets = {0, 3, 6};

  EXPECT_TRUE(textRange(kText, byte_offsets, 1, 1).empty());
  EXPECT_TRUE(textRange(kText, byte_offsets, 2, 1).empty());
  EXPECT_TRUE(textRange(kText, byte_offsets, 0, 3).empty());
  EXPECT_TRUE(textRange(kText, ByteOffsets{0, 7}, 0, 1).empty());
}

TEST(ScorerConnectionDedupTest, AdverbSingleHiraganaRenyokeiKeepsEffectivePenalty) {
  const Scorer scorer;
  core::LatticeEdge prev;
  prev.surface = "そっと";
  prev.pos = core::PartOfSpeech::Adverb;
  prev.extended_pos = core::ExtendedPOS::Adverb;
  core::LatticeEdge generated;
  generated.surface = "し";
  generated.pos = core::PartOfSpeech::Verb;
  generated.extended_pos = core::ExtendedPOS::VerbRenyokei;
  core::LatticeEdge dictionary = generated;
  dictionary.flags = core::EdgeFlags::FromDictionary;

  EXPECT_FLOAT_EQ(scorer.connectionCost(prev, generated) - scorer.connectionCost(prev, dictionary),
                  bigram_cost::kVeryRare + bigram_cost::kVeryRare);
}

TEST(ScorerConnectionDedupTest, FinalParticleSingleHiraganaRenyokeiKeepsEffectivePenalty) {
  const Scorer scorer;
  core::LatticeEdge prev;
  prev.surface = "よ";
  prev.pos = core::PartOfSpeech::Particle;
  prev.extended_pos = core::ExtendedPOS::ParticleFinal;
  core::LatticeEdge hiragana;
  hiragana.surface = "ね";
  hiragana.pos = core::PartOfSpeech::Verb;
  hiragana.extended_pos = core::ExtendedPOS::VerbRenyokei;
  core::LatticeEdge kanji = hiragana;
  kanji.surface = "寝";

  EXPECT_FLOAT_EQ(scorer.connectionCost(prev, hiragana) - scorer.connectionCost(prev, kanji), bigram_cost::kSevere);
}

TEST(ScorerConnectionDedupTest, CopulaAruHypotheticalKeepsDoubleBonus) {
  const Scorer scorer;
  core::LatticeEdge prev;
  prev.surface = "で";
  prev.pos = core::PartOfSpeech::Auxiliary;
  prev.extended_pos = core::ExtendedPOS::AuxCopulaDa;
  core::LatticeEdge next;
  next.surface = "あれ";
  next.pos = core::PartOfSpeech::Verb;
  next.extended_pos = core::ExtendedPOS::VerbKateikei;
  next.lemma = "ある";

  EXPECT_FLOAT_EQ(scorer.connectionCost(prev, next), -2.7F);
}

TEST(ScorerConnectionDedupTest, CopulaHypotheticalLegacyDomainsKeepTheirEffectiveScores) {
  core::LatticeEdge prev;
  prev.surface = "で";
  prev.pos = core::PartOfSpeech::Auxiliary;
  prev.extended_pos = core::ExtendedPOS::AuxCopulaDa;
  core::LatticeEdge next;
  next.surface = "あれ";
  next.pos = core::PartOfSpeech::Verb;
  next.extended_pos = core::ExtendedPOS::VerbKateikei;
  next.lemma = "ある";

  EXPECT_FLOAT_EQ(connection_rules::computeCopulaConditionalBonus(prev, next), bigram_cost::kDoubleVeryStrongBonus);

  prev.surface = "だ";
  EXPECT_FLOAT_EQ(connection_rules::computeCopulaConditionalBonus(prev, next), bigram_cost::kVeryStrongBonus);

  prev.surface = "で";
  next.surface = "仮";
  EXPECT_FLOAT_EQ(connection_rules::computeCopulaConditionalBonus(prev, next),
                  bigram_cost::kStrong + bigram_cost::kVeryStrongBonus);

  prev.surface = "だ";
  next.lemma = "仮る";
  EXPECT_FLOAT_EQ(connection_rules::computeCopulaConditionalBonus(prev, next), bigram_cost::kStrong);
}

TEST(ScorerConnectionDedupTest, UnverifiedHiraganaRenyokeiReceivesPastBonusAtAnyStart) {
  core::LatticeEdge stem;
  stem.surface = "つめ";
  stem.pos = core::PartOfSpeech::Verb;
  stem.extended_pos = core::ExtendedPOS::VerbRenyokei;
  stem.conj_type = dictionary::ConjugationType::Ichidan;
  stem.origin = core::CandidateOrigin::VerbHiraganaInflectedRenyokei;
  core::LatticeEdge past;
  past.surface = "た";
  past.pos = core::PartOfSpeech::Auxiliary;
  past.extended_pos = core::ExtendedPOS::AuxTenseTa;

  EXPECT_FLOAT_EQ(connection_rules::computeTaFormVolitionalBonus(stem, past), bigram_cost::kVeryStrongBonus);

  stem.start = 1;
  EXPECT_FLOAT_EQ(connection_rules::computeTaFormVolitionalBonus(stem, past), bigram_cost::kVeryStrongBonus);

  stem.start = 0;
  stem.flags = core::EdgeFlags::LemmaVerified;
  EXPECT_FLOAT_EQ(connection_rules::computeTaFormVolitionalBonus(stem, past), bigram_cost::kVeryStrongBonus);
}

TEST(ScorerConnectionDedupTest, DictionaryRenyokeiNegativeBonusRemainsATieBreak) {
  core::LatticeEdge stem;
  stem.surface = "でき";
  stem.pos = core::PartOfSpeech::Verb;
  stem.extended_pos = core::ExtendedPOS::VerbRenyokei;
  stem.flags = core::EdgeFlags::FromDictionary;
  core::LatticeEdge negative;
  negative.surface = "ない";
  negative.pos = core::PartOfSpeech::Auxiliary;
  negative.extended_pos = core::ExtendedPOS::AuxNegativeNai;

  EXPECT_FLOAT_EQ(connection_rules::computeNegativeAndNounVerbBonus(stem, negative), bigram_cost::kMinorBonus);
}

}  // namespace
}  // namespace suzume::analysis
