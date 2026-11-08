/**
 * @file kanji_verb_acceptance_threshold_test.cpp
 * @brief Lexical evidence for a kanji-stem verb candidate only ever lowers its
 *        confidence bar: the lowest applicable threshold wins, whatever order
 *        the evidence is checked in.
 */

#include <gtest/gtest.h>

#include "analysis/candidate_options.h"
#include "analysis/verb_candidates_kanji_internal.h"
#include "grammar/inflection.h"

namespace suzume::analysis::kanji_verb_detail {
namespace {

grammar::InflectionCandidate godanKaCell(const char* suffix) {
  grammar::InflectionCandidate candidate;
  candidate.verb_type = grammar::VerbType::GodanKa;
  candidate.suffix = suffix;
  return candidate;
}

TEST(KanjiVerbAcceptanceThresholdTest, PlainCandidateUsesStandardBar) {
  const VerbCandidateOptions opts;
  EXPECT_FLOAT_EQ(kanjiVerbAcceptanceThreshold(godanKaCell("く"), false, false, opts), opts.confidence_standard);
}

TEST(KanjiVerbAcceptanceThresholdTest, LexicalEvidenceUsesDictionaryBar) {
  const VerbCandidateOptions opts;
  EXPECT_FLOAT_EQ(kanjiVerbAcceptanceThreshold(godanKaCell("く"), true, false, opts), opts.confidence_ichidan_dict);
}

TEST(KanjiVerbAcceptanceThresholdTest, MixedStemOnbinCellKeepsItsLowerBarWithEvidence) {
  const VerbCandidateOptions opts;
  ASSERT_LT(opts.confidence_past_te, opts.confidence_ichidan_dict);
  EXPECT_FLOAT_EQ(kanjiVerbAcceptanceThreshold(godanKaCell("いた"), false, true, opts), opts.confidence_past_te);
  EXPECT_FLOAT_EQ(kanjiVerbAcceptanceThreshold(godanKaCell("いた"), true, true, opts), opts.confidence_past_te);
}

TEST(KanjiVerbAcceptanceThresholdTest, EvidenceLowersMixedStemNonOnbinCell) {
  const VerbCandidateOptions opts;
  ASSERT_LT(opts.confidence_ichidan_dict, opts.confidence_low);
  EXPECT_FLOAT_EQ(kanjiVerbAcceptanceThreshold(godanKaCell("く"), false, true, opts), opts.confidence_low);
  EXPECT_FLOAT_EQ(kanjiVerbAcceptanceThreshold(godanKaCell("く"), true, true, opts), opts.confidence_ichidan_dict);
}

}  // namespace
}  // namespace suzume::analysis::kanji_verb_detail
