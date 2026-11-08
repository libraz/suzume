#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>

#include "analysis/adjective_candidates.h"
#include "analysis/tokenizer_unknown_internal.h"
#include "analysis/unknown.h"
#include "analysis/unknown_bracketed_noun_internal.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "normalize/utf8.h"
#include "suzume.h"

namespace suzume {
namespace analysis {
namespace {

std::vector<UnknownCandidate> generateNaCandidatesAt(std::string_view input, size_t start_pos,
                                                     const dictionary::DictionaryManager& dict_manager) {
  const auto codepoints = normalize::toCodepoints(input);
  const auto char_types = normalize::classifyCodepoints(codepoints);
  grammar::Inflection inflection;
  std::vector<UnknownCandidate> candidates;
  generateNaAdjectiveCandidates(codepoints, start_pos, char_types, inflection, &dict_manager, candidates);
  return candidates;
}

std::vector<UnknownCandidate> generateUnknownCandidatesAt(std::string_view input, size_t start_pos,
                                                          const dictionary::DictionaryManager& dict_manager) {
  const auto codepoints = normalize::toCodepoints(input);
  const auto char_types = normalize::classifyCodepoints(codepoints);
  const UnknownWordGenerator generator({}, &dict_manager);
  return generator.generate(input, codepoints, start_pos, char_types);
}

TEST(NoncontinuativeTeNominal, ShapeRejectsRealContinuativesAndMalformedText) {
  for (const auto* surface : {"にがて", "かたて", "あらて", "おもて"}) {
    EXPECT_TRUE(grammar::hasNoncontinuativeTeEnding(surface)) << surface;
  }
  for (const auto* surface :
       {"", "て", "食べて", "つなぎて", "よくて", "こうて", "ようて", "読んで", "すべて", "はじめて", "Ａて", "Aて",
        "ｶﾀﾃ", "カタテ", "かーて", "👨‍👩‍👧て", "か\u3099て", "か\xE3\x81"}) {
    EXPECT_FALSE(grammar::hasNoncontinuativeTeEnding(surface)) << surface;
  }
  // Invalid continuation bytes must not be decoded as valid kana by a fast
  // three-byte-only reader.
  EXPECT_FALSE(grammar::hasNoncontinuativeTeEnding(std::string("\xE3\x41\x8B") + "て"));
}

TEST(NoncontinuativeTeNominal, FiniteCopulaSelectsCompleteKanaNoun) {
  for (const bool lemmatize : {true, false}) {
    SuzumeOptions options;
    options.skip_user_dictionary = true;
    options.lemmatize = lemmatize;
    Suzume analyzer(options);
    for (const auto& [input, surface] :
         {std::pair{"ねこがにがてだ", "にがて"}, std::pair{"ねこがにがてだった", "にがて"},
          std::pair{"これはにがてです", "にがて"}, std::pair{"これはにがてでした", "にがて"},
          std::pair{"泳ぐのがにがてだ", "にがて"}, std::pair{"かたてだ", "かたて"}, std::pair{"あらてだ", "あらて"},
          std::pair{"あらてだった", "あらて"}}) {
      SCOPED_TRACE(input);
      SCOPED_TRACE(lemmatize);
      const auto result = analyzer.analyze(input);
      const std::string expected_surface = surface;
      const auto noun = std::find_if(result.begin(), result.end(),
                                     [&](const core::Morpheme& token) { return token.surface == expected_surface; });
      ASSERT_NE(noun, result.end());
      EXPECT_EQ(noun->pos, core::PartOfSpeech::Noun);
      EXPECT_EQ(noun->extended_pos, core::ExtendedPOS::Noun);
      EXPECT_EQ(noun->lemma, surface);
      EXPECT_EQ(noun->conj_type, dictionary::ConjugationType::None);
      EXPECT_EQ(noun->conj_form, grammar::ConjForm::Base);
      const size_t start = std::string(input).find(surface) / 3;
      EXPECT_EQ(noun->start, start);
      EXPECT_EQ(noun->end, start + 3);
      ASSERT_NE(noun + 1, result.end());
      EXPECT_EQ((noun + 1)->start, noun->end);
      EXPECT_EQ((noun + 1)->pos, core::PartOfSpeech::Auxiliary);
    }
  }
}

TEST(NoncontinuativeTeNominal, SubstantiveFormalNounSelectsAttributiveNaStem) {
  const dictionary::DictionaryManager dict_manager;
  for (const bool lemmatize : {true, false}) {
    SuzumeOptions options;
    options.skip_user_dictionary = true;
    options.lemmatize = lemmatize;
    Suzume analyzer(options);
    const auto attributive = analyzer.analyze("にがてな人");
    const auto baseline = std::find_if(attributive.begin(), attributive.end(),
                                       [](const core::Morpheme& token) { return token.surface == "にがて"; });
    ASSERT_NE(baseline, attributive.end());
    for (const auto& [input, formal_noun, expected_start] :
         {std::tuple{"にがてなもの", "もの", 0U}, std::tuple{"にがてなこと", "こと", 0U},
          std::tuple{"ねこがにがてなものを確認する", "もの", 3U}, std::tuple{"生徒がにがてなこと", "こと", 3U},
          std::tuple{"泳ぐのがにがてなもの", "もの", 4U}, std::tuple{"これはにがてなことを確認する", "こと", 3U}}) {
      SCOPED_TRACE(input);
      SCOPED_TRACE(lemmatize);
      const std::string_view expected_formal_noun = formal_noun;
      const auto generated = generateNaCandidatesAt(input, expected_start, dict_manager);
      const auto candidate = std::find_if(generated.begin(), generated.end(), [](const UnknownCandidate& value) {
        return value.surface == "にがて" && value.pos == core::PartOfSpeech::Adjective;
      });
      ASSERT_NE(candidate, generated.end());
      EXPECT_EQ(candidate->extended_pos, core::ExtendedPOS::AdjNaAdj);
      EXPECT_EQ(candidate->start, expected_start);
      EXPECT_EQ(candidate->end, expected_start + 3U);
      const auto result = analyzer.analyze(input);
      const auto adjective = std::find_if(result.begin(), result.end(),
                                          [](const core::Morpheme& token) { return token.surface == "にがて"; });
      ASSERT_NE(adjective, result.end());
      EXPECT_EQ(adjective->pos, core::PartOfSpeech::Adjective);
      EXPECT_EQ(adjective->extended_pos, core::ExtendedPOS::AdjNaAdj);
      EXPECT_EQ(adjective->lemma, "にがて");
      EXPECT_EQ(adjective->lemma, baseline->lemma);
      EXPECT_EQ(adjective->conj_type, baseline->conj_type);
      EXPECT_EQ(adjective->conj_form, baseline->conj_form);
      EXPECT_NE(adjective->conj_type, dictionary::ConjugationType::Ichidan);
      EXPECT_NE(adjective->extended_pos, core::ExtendedPOS::VerbTeForm);
      EXPECT_EQ(adjective->start, expected_start);
      EXPECT_EQ(adjective->end, expected_start + 3U);
      EXPECT_NE(std::find_if(adjective + 1, result.end(),
                             [&](const core::Morpheme& token) {
                               return token.surface == expected_formal_noun &&
                                      token.extended_pos == core::ExtendedPOS::NounFormal;
                             }),
                result.end());
    }
  }
}

TEST(NoncontinuativeTeNominal, RescueRequiresSubstantiveFormalNounEvidence) {
  const dictionary::DictionaryManager dict_manager;
  for (const auto* input :
       {"にがてなわけ", "にがてなはず", "にがてなつもり", "にがてなの", "にがてなら", "食べてなもの", "こうてなもの"}) {
    SCOPED_TRACE(input);
    const auto candidates = generateNaCandidatesAt(input, 0, dict_manager);
    EXPECT_EQ(std::find_if(candidates.begin(), candidates.end(),
                           [](const UnknownCandidate& candidate) {
                             return candidate.surface == "にがて" || candidate.surface == "食べて" ||
                                    candidate.surface == "こうて";
                           }),
              candidates.end());
  }
}

TEST(NoncontinuativeTeNominal, PreservesNominalParticleBeforeIndependentlyAdmittedStem) {
  const dictionary::DictionaryManager dict_manager;
  const auto wider = generateNaCandidatesAt("ねこがにがてなもの", 2, dict_manager);
  const auto narrower = generateNaCandidatesAt("ねこがにがてなもの", 3, dict_manager);
  const auto wide = std::find_if(wider.begin(), wider.end(), [](const UnknownCandidate& candidate) {
    return candidate.surface == "がにがて" && candidate.end == 6;
  });
  const auto narrow = std::find_if(narrower.begin(), narrower.end(), [](const UnknownCandidate& candidate) {
    return candidate.surface == "にがて" && candidate.end == 6;
  });
  ASSERT_NE(wide, wider.end());
  ASSERT_NE(narrow, narrower.end());
  EXPECT_TRUE(wide->rejects_preceding_nominal_head);
  EXPECT_FALSE(narrow->rejects_preceding_nominal_head);

  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto& [input, stem] : {std::pair{"にこやかなもの", "にこやか"}, std::pair{"はなやかなもの", "はなやか"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_EQ(result.size(), 3U);
    EXPECT_EQ(result[0].surface, stem);
    EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjNaAdj);
  }
}

TEST(NoncontinuativeTeNominal, KeepsDictionaryProvenHomographicVerb) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  const std::string source = "おもてだつ\tVERB\tGODAN_TA\n";
  ASSERT_TRUE(analyzer.loadUserDictionaryFromMemoryResult(source.data(), source.size()).hasValue());
  const auto result = analyzer.analyze("おもてだった");
  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].surface, "おもてだっ");
  EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(result[0].lemma, "おもてだつ");
  EXPECT_EQ(result[0].conj_type, dictionary::ConjugationType::GodanTa);
}

TEST(NoncontinuativeTeNominal, PreservesLexicalRolesAndProductiveInflections) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto& [input, surface] : {std::pair{"苦手だ", "苦手"}, std::pair{"にがてな人", "にがて"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0].surface, surface);
    EXPECT_EQ(result[0].extended_pos, core::ExtendedPOS::AdjNaAdj);
  }
  for (const auto& [input, surface] :
       {std::pair{"すべてだ", "すべて"}, std::pair{"はじめてだ", "はじめて"}, std::pair{"したてだ", "したて"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result[0].surface, surface);
  }
  for (const auto& [input, surface] : {std::pair{"読んだ", "読ん"}, std::pair{"泳いだ", "泳い"},
                                       std::pair{"食べてだ", "食べ"}, std::pair{"つなぎてだ", "つなぎ"}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0].surface, surface);
    EXPECT_EQ(result[0].pos, core::PartOfSpeech::Verb);
  }
  for (const auto* input : {"よくてだ", "こうてだ"}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    ASSERT_FALSE(result.empty());
    EXPECT_FALSE(result[0].pos == core::PartOfSpeech::Noun && result[0].surface == std::string(input).substr(0, 9));
  }
}

TEST(QuantityYoonNoun, EmitsSeparateCounterBoundCandidatesForYoonNouns) {
  const dictionary::DictionaryManager dict_manager;
  for (const auto& [input, surface, start] :
       {std::tuple{"二人いっしょ", "いっしょ", 2U}, std::tuple{"3人いっしょ", "いっしょ", 2U},
        std::tuple{"三件とっきょ", "とっきょ", 2U}}) {
    SCOPED_TRACE(input);
    const auto candidates = generateUnknownCandidatesAt(input, start, dict_manager);
    const std::string_view expected_surface = surface;
    const auto quantity_noun = std::find_if(candidates.begin(), candidates.end(), [&](const UnknownCandidate& value) {
      return value.surface == expected_surface && value.pos == core::PartOfSpeech::Noun &&
             value.origin == CandidateOrigin::BracketedNoun && value.requires_left_counter_edge;
    });
    ASSERT_NE(quantity_noun, candidates.end());
    EXPECT_EQ(quantity_noun->start, start);
    EXPECT_EQ(quantity_noun->end, start + normalize::utf8Length(surface));
    EXPECT_FALSE(quantity_noun->requires_left_attributive_edge);
  }
}

TEST(QuantityYoonNoun, AuxiliaryChainsAndNonYoonRunsDoNotGetCounterBoundCandidates) {
  const dictionary::DictionaryManager dict_manager;
  for (const auto& [input, surface, start] :
       {std::tuple{"二人いましょ", "いましょ", 2U}, std::tuple{"二人だっしょ", "だっしょ", 2U},
        std::tuple{"二人っしょ", "っしょ", 2U}}) {
    SCOPED_TRACE(input);
    const auto candidates = generateUnknownCandidatesAt(input, start, dict_manager);
    const std::string_view expected_surface = surface;
    EXPECT_EQ(std::find_if(candidates.begin(), candidates.end(),
                           [&](const UnknownCandidate& candidate) {
                             return candidate.surface == expected_surface &&
                                    candidate.pos == core::PartOfSpeech::Noun &&
                                    candidate.origin == CandidateOrigin::BracketedNoun &&
                                    candidate.requires_left_counter_edge;
                           }),
              candidates.end());
  }
}

TEST(QuantityYoonNoun, PreservesAnExplicitMimeticWithTheSameYoonEnding) {
  const dictionary::DictionaryManager dict_manager;
  const auto candidates = generateUnknownCandidatesAt("三人きゃきゃ", 2, dict_manager);
  const auto mimetic = std::find_if(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
    return candidate.surface == "きゃきゃ" && candidate.origin == CandidateOrigin::Onomatopoeia &&
           candidate.pos == core::PartOfSpeech::Adverb;
  });
  ASSERT_NE(mimetic, candidates.end());
  EXPECT_TRUE(std::none_of(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
    return candidate.surface == "きゃきゃ" && candidate.requires_left_counter_edge;
  }));
}

TEST(QuantityYoonNoun, SelectsBoundedNounWithoutChangingExistingPredicateAndWordReadings) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);
  for (const auto& [input, surface, start] :
       {std::tuple{"二人いっしょ", "いっしょ", 2U}, std::tuple{"数人いっしょ", "いっしょ", 2U},
        std::tuple{"3人いっしょ", "いっしょ", 2U}, std::tuple{"三人いっしょ", "いっしょ", 2U},
        std::tuple{"三人いっしょに行く", "いっしょ", 2U}, std::tuple{"二人いっしょだ", "いっしょ", 2U},
        std::tuple{"三件とっきょ", "とっきょ", 2U}}) {
    SCOPED_TRACE(input);
    const auto result = analyzer.analyze(input);
    const std::string_view expected_surface = surface;
    const auto noun = std::find_if(result.begin(), result.end(), [&](const core::Morpheme& token) {
      return token.surface == expected_surface && token.pos == core::PartOfSpeech::Noun;
    });
    ASSERT_NE(noun, result.end());
    EXPECT_EQ(noun->start, start);
    EXPECT_EQ(noun->end, start + normalize::utf8Length(expected_surface));
  }

  const auto predicate = analyzer.analyze("行けるっしょ");
  EXPECT_NE(std::find_if(predicate.begin(), predicate.end(),
                         [](const core::Morpheme& token) {
                           return token.surface == "っしょ" && token.pos == core::PartOfSpeech::Auxiliary;
                         }),
            predicate.end());

  const auto dictionary_word = analyzer.analyze("三人いつも元気");
  EXPECT_NE(std::find_if(dictionary_word.begin(), dictionary_word.end(),
                         [](const core::Morpheme& token) {
                           return token.surface == "いつも" && token.pos == core::PartOfSpeech::Adverb;
                         }),
            dictionary_word.end());

  const auto mimetic = analyzer.analyze("ふわふわだ");
  EXPECT_NE(std::find_if(mimetic.begin(), mimetic.end(),
                         [](const core::Morpheme& token) { return token.surface == "ふわふわ"; }),
            mimetic.end());
}

TEST(QuantityYoonNoun, CandidateRequiresCounterEdgeAtAdmission) {
  const std::string input = "二人いっしょ";
  const auto codepoints = normalize::toCodepoints(input);
  const auto char_types = normalize::classifyCodepoints(codepoints);
  const auto byte_offsets = buildByteOffsets(codepoints);
  const dictionary::DictionaryManager dict_manager;
  const UnknownWordGenerator generator({}, &dict_manager);
  const size_t candidate_start = 2;
  const auto candidates = generator.generate(input, codepoints, candidate_start, char_types);
  const auto quantity_noun = std::find_if(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
    return candidate.surface == "いっしょ" && candidate.pos == core::PartOfSpeech::Noun &&
           candidate.origin == CandidateOrigin::BracketedNoun && candidate.requires_left_counter_edge;
  });
  ASSERT_NE(quantity_noun, candidates.end());

  std::vector<dictionary::LookupResult> dict_results;
  const tokenizer_unknown_detail::UnknownCandidateContext ctx{
      dict_manager,    generator.inflection(), generator,  input,        codepoints,
      byte_offsets,    candidate_start,        char_types, dict_results, candidates,
      candidate_start, candidate_start,        0};
  core::Lattice without_counter(codepoints.size());
  EXPECT_TRUE(tokenizer_unknown_detail::isRejectedUnknownCandidate(ctx, without_counter, *quantity_noun));

  const auto counter_candidates = generator.generate(input, codepoints, 0, char_types);
  const auto counter = std::find_if(counter_candidates.begin(), counter_candidates.end(), [&](const auto& candidate) {
    return candidate.pos == core::PartOfSpeech::Noun && candidate.origin == CandidateOrigin::Counter &&
           candidate.start == 0 && candidate.end == candidate_start;
  });
  ASSERT_NE(counter, counter_candidates.end());
  core::Lattice with_counter(codepoints.size());
  with_counter.addEdge(counter->surface, static_cast<uint32_t>(counter->start), static_cast<uint32_t>(counter->end),
                       counter->pos, counter->cost, core::LatticeEdge::kIsUnknown, counter->lemma, counter->conj_type,
                       counter->origin, 0.0F, {}, counter->extended_pos);
  EXPECT_FALSE(tokenizer_unknown_detail::isRejectedUnknownCandidate(ctx, with_counter, *quantity_noun));
}

TEST(QuantityYoonNoun, RejectsANominalizedStemThatAbsorbsTheQuantity) {
  const std::string input = "二人いっしょ";
  const auto codepoints = normalize::toCodepoints(input);
  const auto char_types = normalize::classifyCodepoints(codepoints);
  const auto byte_offsets = buildByteOffsets(codepoints);
  const dictionary::DictionaryManager dict_manager;
  const UnknownWordGenerator generator({}, &dict_manager);
  const auto candidates = generator.generate(input, codepoints, 0, char_types);
  const auto nominalized = std::find_if(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
    return candidate.surface == "二人い" && candidate.origin == CandidateOrigin::NominalizedNoun;
  });
  ASSERT_NE(nominalized, candidates.end());
  ASSERT_TRUE(std::any_of(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
    return candidate.start == 0 && candidate.end == 2 && candidate.pos == core::PartOfSpeech::Noun &&
           candidate.origin == CandidateOrigin::Counter;
  }));
  const std::vector<dictionary::LookupResult> dict_results;
  const tokenizer_unknown_detail::UnknownCandidateContext ctx{dict_manager,
                                                              generator.inflection(),
                                                              generator,
                                                              input,
                                                              codepoints,
                                                              byte_offsets,
                                                              0,
                                                              char_types,
                                                              dict_results,
                                                              candidates,
                                                              2,
                                                              2,
                                                              0};
  const core::Lattice lattice(codepoints.size());
  EXPECT_TRUE(tokenizer_unknown_detail::isRejectedUnknownCandidate(ctx, lattice, *nominalized));
}

TEST(QuantityYoonNoun, QuantityEmitterDoesNotDeriveAnUnlicensedHostAfterHonorificPrefix) {
  const std::string input = "おもちゃ";
  const auto codepoints = normalize::toCodepoints(input);
  const auto char_types = normalize::classifyCodepoints(codepoints);
  const dictionary::DictionaryManager dict_manager;
  grammar::Inflection inflection;
  UnknownOptions options;
  const bracketed_noun_detail::BracketedNounContext ctx{codepoints,    char_types, 0,
                                                        &dict_manager, inflection, options.verb_candidate_options};
  bracketed_noun_detail::BracketedScan bracketed;
  bracketed.left_clause_bracket = true;
  bracketed_noun_detail::PromotedRun promoted;
  promoted.scan = codepoints.size();
  promoted.len = codepoints.size();
  promoted.right_clause = true;
  promoted.promoted_surface = input;
  promoted.promoted_dictionary_reading = nullptr;
  promoted.promoted_inflections = &inflection.analyze(input);
  promoted.min_len = 3;
  std::vector<UnknownCandidate> candidates;

  bracketed_noun_detail::appendPromotedRunCandidates(ctx, bracketed, promoted, 0.0F, 0, candidates,
                                                     /*after_quantity=*/true);

  const auto quantity_noun = std::find_if(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
    return candidate.start == 0 && candidate.end == 4 && candidate.pos == core::PartOfSpeech::Noun &&
           candidate.requires_left_counter_edge;
  });
  ASSERT_NE(quantity_noun, candidates.end());
  EXPECT_EQ(std::find_if(candidates.begin(), candidates.end(),
                         [](const UnknownCandidate& candidate) {
                           return candidate.start == 1 && candidate.end == 4 &&
                                  candidate.origin == CandidateOrigin::BracketedNoun;
                         }),
            candidates.end());
}

}  // namespace
}  // namespace analysis
}  // namespace suzume
