#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "analysis/analyzer.h"
#include "analysis/scorer.h"
#include "analysis/split_candidates.h"
#include "analysis/tokenizer_utils.h"
#include "analysis/unknown.h"
#include "core/lattice.h"
#include "dictionary/dictionary.h"
#include "dictionary/user_dict.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::analysis {
namespace {

TEST(CandidateGenerationEfficiencyTest, BoundsClosedClassProbeToFiveCodepoints) {
  const auto codepoints = normalize::toCodepoints("あいうえおかき");

  EXPECT_EQ(extractClosedClassProbe(codepoints, 0), "あいうえお");
  EXPECT_EQ(extractClosedClassProbe(codepoints, 4), "おかき");
}

TEST(CandidateGenerationEfficiencyTest, EmitsOneAdjectiveStemEdgeBeforeSaNominalizer) {
  dictionary::DictionaryManager dictionary_manager;
  ASSERT_TRUE(dictionary_manager.loadCoreDictionary("data/core.dic"));
  const std::string text = "美しさ";
  const auto codepoints = normalize::toCodepoints(text);
  const auto char_types = normalize::classifyCodepoints(codepoints);
  const UnknownWordGenerator generator({}, &dictionary_manager);

  const auto candidates = generator.generate(text, codepoints, 0, char_types);
  const size_t matching_edges =
      static_cast<size_t>(std::count_if(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
        return candidate.surface == "美し" && candidate.start == 0 && candidate.end == 2 &&
               candidate.pos == core::PartOfSpeech::Adjective && candidate.extended_pos == core::ExtendedPOS::AdjStem;
      }));

  EXPECT_EQ(matching_edges, 1u);
}

TEST(CandidateGenerationEfficiencyTest, EmitsOneDictionaryBackedCompoundSplitEdge) {
  auto user_dictionary = std::make_shared<dictionary::UserDictionary>();
  ASSERT_TRUE(user_dictionary->addEntry(
      dictionary::DictionaryEntry{"人工", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "人工"}));
  ASSERT_TRUE(user_dictionary->addEntry(
      dictionary::DictionaryEntry{"知能", core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "知能"}));
  dictionary::DictionaryManager dictionary_manager;
  dictionary_manager.addUserDictionary(user_dictionary);

  const std::string text = "人工知能";
  const auto codepoints = normalize::toCodepoints(text);
  const auto char_types = normalize::classifyCodepoints(codepoints);
  const auto byte_offsets = buildByteOffsets(codepoints);
  const Scorer scorer;
  core::Lattice lattice(codepoints.size());

  addCompoundSplitCandidates(lattice, text, byte_offsets, 0, char_types, dictionary_manager, scorer);

  const size_t matching_edges = static_cast<size_t>(
      std::count_if(lattice.edgeIdsAt(0).begin(), lattice.edgeIdsAt(0).end(), [&lattice](const uint32_t edge_id) {
        const auto& edge = lattice.getEdge(edge_id);
        return edge.surface == "人工" && edge.start == 0 && edge.end == 2 && edge.pos == core::PartOfSpeech::Noun;
      }));
  EXPECT_EQ(matching_edges, 1u);
}

TEST(CandidateGenerationEfficiencyTest, BoundsSameTypeCandidatesForLongRun) {
  UnknownOptions options;
  options.max_kanji_length = 4;
  dictionary::DictionaryManager dictionary_manager;
  const UnknownWordGenerator generator(options, &dictionary_manager);
  const std::string text = "研究研究研究研究";
  const auto codepoints = normalize::toCodepoints(text);
  const auto char_types = normalize::classifyCodepoints(codepoints);

  const auto candidates = generator.generate(text, codepoints, 0, char_types);
  auto has_same_type_end = [&candidates](size_t end) {
    return std::any_of(candidates.begin(), candidates.end(), [end](const UnknownCandidate& candidate) {
      return candidate.origin == core::CandidateOrigin::SameType && candidate.start == 0 && candidate.end == end &&
             candidate.pos == core::PartOfSpeech::Noun;
    });
  };

  EXPECT_TRUE(has_same_type_end(options.max_kanji_length));
  for (size_t end = options.max_kanji_length + 1; end <= codepoints.size(); ++end) {
    EXPECT_FALSE(has_same_type_end(end));
  }
}

TEST(CandidateGenerationEfficiencyTest, SameScriptRunsScaleLinearly) {
  using Clock = std::chrono::steady_clock;

  const auto measure = [](std::string_view unit, size_t count) {
    std::string text;
    text.reserve(unit.size() * count);
    for (size_t idx = 0; idx < count; ++idx) {
      text += unit;
    }

    Analyzer analyzer;
    const auto started = Clock::now();
    const auto result = analyzer.analyze(text);
    const auto elapsed = Clock::now() - started;
    EXPECT_TRUE(result.hasValue());
    return elapsed;
  };

  const auto hiragana_2k = measure("あ", 2000);
  const auto hiragana_4k = measure("あ", 4000);
  const auto hiragana_8k = measure("あ", 8000);
  const auto hiragana_16k = measure("あ", 16000);
  const auto katakana_8k = measure("ア", 8000);

  // A threefold allowance absorbs shared-CI variance while rejecting the
  // quadratic candidate/lattice growth that this guard protects against.
  constexpr auto kTimingSlack = std::chrono::milliseconds(100);
  EXPECT_LT(hiragana_4k, hiragana_2k * 3 + kTimingSlack);
  EXPECT_LT(hiragana_8k, hiragana_4k * 3 + kTimingSlack);
  EXPECT_LT(hiragana_16k, hiragana_8k * 3 + kTimingSlack);
  EXPECT_LT(katakana_8k, hiragana_8k * 3 + kTimingSlack);
}

TEST(CandidateGenerationEfficiencyTest, KeepsAsciiKeycapEmojiInOneSameTypeCandidate) {
  dictionary::DictionaryManager dictionary_manager;
  const UnknownWordGenerator generator({}, &dictionary_manager);
  const std::string text = "1️⃣です";
  const auto codepoints = normalize::toCodepoints(text);
  const auto char_types = normalize::classifyCodepoints(codepoints);

  const auto candidates = generator.generate(text, codepoints, 0, char_types);
  const bool has_keycap = std::any_of(candidates.begin(), candidates.end(), [](const UnknownCandidate& candidate) {
    return candidate.origin == core::CandidateOrigin::SameType && candidate.surface == "1️⃣" && candidate.start == 0 &&
           candidate.end == 3;
  });
  EXPECT_TRUE(has_keycap);
}

}  // namespace
}  // namespace suzume::analysis
