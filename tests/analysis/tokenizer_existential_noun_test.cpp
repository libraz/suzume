#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "analysis/tokenizer.h"
#include "analysis/unknown.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "dictionary/user_dict.h"
#include "normalize/utf8.h"
#include "suzume.h"

namespace suzume {
namespace analysis {
namespace {

struct ExpectedToken {
  std::string_view surface;
  core::PartOfSpeech pos;
  core::ExtendedPOS extended_pos;
  std::string_view lemma;
};

void expectTokenization(const std::vector<core::Morpheme>& tokens, const std::vector<ExpectedToken>& expected) {
  ASSERT_EQ(tokens.size(), expected.size());
  size_t next_start = 0;
  for (size_t token_index = 0; token_index < expected.size(); ++token_index) {
    const auto& token = tokens[token_index];
    const auto& expected_token = expected[token_index];
    SCOPED_TRACE(token_index);
    EXPECT_EQ(token.surface, expected_token.surface);
    EXPECT_EQ(token.pos, expected_token.pos);
    EXPECT_EQ(token.extended_pos, expected_token.extended_pos);
    EXPECT_EQ(token.lemma, expected_token.lemma);
    EXPECT_EQ(token.start, next_start);
    next_start += normalize::toCodepoints(expected_token.surface).size();
    EXPECT_EQ(token.end, next_start);
  }
}

const core::LatticeEdge* findNounEdge(const core::Lattice& lattice, std::string_view surface,
                                      core::CandidateOrigin origin) {
  for (const uint32_t edge_id : lattice.edgeIdsAt(0)) {
    const auto& edge = lattice.getEdge(edge_id);
    if (edge.surface == surface && edge.pos == core::PartOfSpeech::Noun && edge.origin == origin) {
      return &edge;
    }
  }
  return nullptr;
}

const core::LatticeEdge* findEdge(const core::Lattice& lattice, std::string_view surface, size_t start, size_t end,
                                  core::PartOfSpeech pos) {
  if (start >= lattice.textLength()) {
    return nullptr;
  }
  for (const uint32_t edge_id : lattice.edgeIdsAt(start)) {
    const auto& edge = lattice.getEdge(edge_id);
    if (edge.surface == surface && edge.start == start && edge.end == end && edge.pos == pos) {
      return &edge;
    }
  }
  return nullptr;
}

TEST(TokenizerExistentialNoun, FiniteDictionaryNounsAndCountersKeepTheirMorphemeBoundaries) {
  for (const bool lemmatize : {true, false}) {
    SuzumeOptions options;
    options.skip_user_dictionary = true;
    options.lemmatize = lemmatize;
    Suzume analyzer(options);
    const auto counted_baseline = analyzer.analyze("三人いた");
    ASSERT_EQ(counted_baseline.size(), 3U);
    EXPECT_EQ(counted_baseline[1].surface, "い");
    EXPECT_EQ(counted_baseline[1].pos, core::PartOfSpeech::Verb);
    EXPECT_EQ(counted_baseline[1].extended_pos, core::ExtendedPOS::VerbRenyokei);
    EXPECT_EQ(counted_baseline[1].lemma, "いる");
    EXPECT_EQ(counted_baseline[1].conj_form, grammar::ConjForm::Renyokei);

    const std::array<std::pair<std::string_view, std::vector<ExpectedToken>>, 9> cases = {{
        {"猫いた",
         {{"猫", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "猫"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"た", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseTa, "た"}}},
        {"猫いますか",
         {{"猫", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "猫"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"ます", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseMasu, "ます"},
          {"か", core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleFinal, "か"}}},
        {"猫いました",
         {{"猫", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "猫"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"まし", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseMasu, "ます"},
          {"た", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseTa, "た"}}},
        {"子どもいた",
         {{"子ども", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "子ども"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"た", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseTa, "た"}}},
        {"彼氏いますか",
         {{"彼氏", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "彼氏"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"ます", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseMasu, "ます"},
          {"か", core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleFinal, "か"}}},
        {"猫いません",
         {{"猫", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "猫"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"ませ", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseMasu, "ます"},
          {"ん", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxNegativeNu, "ん"}}},
        {"彼氏いませんか",
         {{"彼氏", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "彼氏"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"ませ", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseMasu, "ます"},
          {"ん", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxNegativeNu, "ん"},
          {"か", core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleFinal, "か"}}},
        {"二人いません",
         {{"二人", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "二人"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"ませ", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseMasu, "ます"},
          {"ん", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxNegativeNu, "ん"}}},
        {"数人いませんか",
         {{"数人", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "数人"},
          {"い", core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる"},
          {"ませ", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseMasu, "ます"},
          {"ん", core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxNegativeNu, "ん"},
          {"か", core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleFinal, "か"}}},
    }};

    for (const auto& [text, expected] : cases) {
      SCOPED_TRACE(text);
      SCOPED_TRACE(lemmatize);
      const auto tokens = analyzer.analyze(text);
      expectTokenization(tokens, expected);
      const auto verb =
          std::find_if(tokens.begin(), tokens.end(), [](const core::Morpheme& token) { return token.surface == "い"; });
      ASSERT_NE(verb, tokens.end());
      EXPECT_EQ(verb->conj_type, counted_baseline[1].conj_type);
      EXPECT_EQ(verb->conj_form, grammar::ConjForm::Renyokei);
    }
  }
}

TEST(TokenizerExistentialNoun, DictionaryEdgesCarryOnlyFiniteExistentialProof) {
  dictionary::DictionaryManager dict_manager;
  ASSERT_TRUE(dict_manager.loadCoreDictionaryResult("data/core.dic").hasValue());
  UnknownWordGenerator unknown_generator({}, &dict_manager);
  Scorer scorer;
  Tokenizer tokenizer(dict_manager, scorer, unknown_generator);

  struct ProofCase {
    std::string_view text;
    std::string_view noun;
    core::CandidateOrigin origin;
    bool finite;
  };
  for (const ProofCase& test_case : {ProofCase{"猫いた", "猫", core::CandidateOrigin::Dictionary, true},
                                     {"猫いますか", "猫", core::CandidateOrigin::Dictionary, true},
                                     {"子どもいた", "子ども", core::CandidateOrigin::Dictionary, true},
                                     {"彼氏いますか", "彼氏", core::CandidateOrigin::Dictionary, true},
                                     {"おじさんいますか", "おじさん", core::CandidateOrigin::Dictionary, true},
                                     {"二人いた", "二人", core::CandidateOrigin::Counter, true},
                                     {"二人いません", "二人", core::CandidateOrigin::Counter, true},
                                     {"数人いませんか", "数人", core::CandidateOrigin::Counter, true},
                                     {"猫いたずら", "猫", core::CandidateOrigin::Dictionary, false},
                                     {"猫いっしょ", "猫", core::CandidateOrigin::Dictionary, false},
                                     {"猫いましょう", "猫", core::CandidateOrigin::Dictionary, false},
                                     {"猫いますぐ", "猫", core::CandidateOrigin::Dictionary, false}}) {
    SCOPED_TRACE(test_case.text);
    const auto codepoints = normalize::toCodepoints(test_case.text);
    const auto lattice = tokenizer.buildLattice(test_case.text, codepoints, normalize::classifyCodepoints(codepoints));
    const core::LatticeEdge* noun_edge = findNounEdge(lattice, test_case.noun, test_case.origin);
    ASSERT_NE(noun_edge, nullptr);
    if (test_case.origin == core::CandidateOrigin::Dictionary) {
      EXPECT_TRUE(noun_edge->fromDictionary());
      EXPECT_EQ(noun_edge->extended_pos, core::ExtendedPOS::Noun);
    }
    EXPECT_EQ(noun_edge->has_finite_existential_continuation, test_case.finite);
  }
}

TEST(TokenizerExistentialNoun, FullSpanDictionaryWordsRemainAvailable) {
  dictionary::DictionaryManager dict_manager;
  ASSERT_TRUE(dict_manager.loadCoreDictionaryResult("data/core.dic").hasValue());
  auto user_dictionary = std::make_shared<dictionary::UserDictionary>();
  ASSERT_TRUE(user_dictionary->addEntry(
      dictionary::DictionaryEntry{"戦", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "戦"}));
  dict_manager.addUserDictionary(user_dictionary);
  UnknownWordGenerator unknown_generator({}, &dict_manager);
  Scorer scorer;
  Tokenizer tokenizer(dict_manager, scorer, unknown_generator);

  const std::string text = "戦います";
  const auto codepoints = normalize::toCodepoints(text);
  const auto lattice = tokenizer.buildLattice(text, codepoints, normalize::classifyCodepoints(codepoints));
  const core::LatticeEdge* noun_edge = findNounEdge(lattice, "戦", core::CandidateOrigin::Dictionary);
  ASSERT_NE(noun_edge, nullptr);
  EXPECT_FALSE(noun_edge->has_finite_existential_continuation);

  const core::LatticeEdge* attested_verb = findEdge(lattice, "戦い", 0, 2, core::PartOfSpeech::Verb);
  ASSERT_NE(attested_verb, nullptr);
  EXPECT_EQ(attested_verb->lemma, "戦う");
  EXPECT_TRUE(attested_verb->lemmaVerified());
}

TEST(TokenizerExistentialNoun, ExistingPredicateAndLexicalNounControlsStayAttested) {
  SuzumeOptions options;
  options.skip_user_dictionary = true;
  Suzume analyzer(options);

  for (const auto& [text, verb_surface, verb_lemma, auxiliary_surface] :
       {std::tuple<std::string_view, std::string_view, std::string_view, std::string_view>{"咲いた", "咲い", "咲く",
                                                                                           "た"},
        {"構います", "構い", "構う", "ます"},
        {"伺います", "伺い", "伺う", "ます"}}) {
    SCOPED_TRACE(text);
    const auto tokens = analyzer.analyze(text);
    ASSERT_GE(tokens.size(), 2U);
    EXPECT_EQ(tokens[0].surface, verb_surface);
    EXPECT_EQ(tokens[0].pos, core::PartOfSpeech::Verb);
    EXPECT_EQ(tokens[0].lemma, verb_lemma);
    EXPECT_EQ(tokens[1].surface, auxiliary_surface);
    EXPECT_EQ(tokens[1].pos, core::PartOfSpeech::Auxiliary);
  }

  dictionary::DictionaryManager dict_manager;
  ASSERT_TRUE(dict_manager.loadCoreDictionaryResult("data/core.dic").hasValue());
  UnknownWordGenerator unknown_generator({}, &dict_manager);
  Scorer scorer;
  Tokenizer tokenizer(dict_manager, scorer, unknown_generator);
  for (const auto& [text, surface, pos] : {std::tuple<std::string_view, std::string_view, core::PartOfSpeech>{
                                               "幅広い", "幅広い", core::PartOfSpeech::Adjective},
                                           {"間違い", "間違い", core::PartOfSpeech::Noun},
                                           {"勘違い", "勘違い", core::PartOfSpeech::Noun}}) {
    SCOPED_TRACE(text);
    const auto codepoints = normalize::toCodepoints(text);
    const auto lattice = tokenizer.buildLattice(text, codepoints, normalize::classifyCodepoints(codepoints));
    const core::LatticeEdge* attested = findEdge(lattice, surface, 0, codepoints.size(), pos);
    ASSERT_NE(attested, nullptr);
    EXPECT_TRUE(attested->fromDictionary());
  }

  const auto polite = analyzer.analyze("おじさんいますか");
  ASSERT_EQ(polite.size(), 4U);
  EXPECT_EQ(polite[0].surface, "おじさん");
  EXPECT_EQ(polite[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(polite[1].surface, "い");
  EXPECT_EQ(polite[1].lemma, "いる");
  EXPECT_EQ(polite[1].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(polite[2].surface, "ます");
  EXPECT_EQ(polite[2].pos, core::PartOfSpeech::Auxiliary);
  EXPECT_EQ(polite[3].surface, "か");

  const auto child = analyzer.analyze("子どもいますか");
  ASSERT_EQ(child.size(), 4U);
  EXPECT_EQ(child[0].surface, "子ども");
  EXPECT_EQ(child[0].pos, core::PartOfSpeech::Noun);
  EXPECT_EQ(child[1].surface, "い");
  EXPECT_EQ(child[1].pos, core::PartOfSpeech::Verb);
  EXPECT_EQ(child[2].surface, "ます");
  EXPECT_EQ(child[3].surface, "か");
}

}  // namespace
}  // namespace analysis
}  // namespace suzume
