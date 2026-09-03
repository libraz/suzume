/**
 * @file suffix_candidates_counter_temporal.cpp
 * @brief Temporal quantity candidates for numeral-counter expressions
 */

#include <algorithm>

#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "dictionary/dictionary.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "suffix_candidates_counter_internal.h"
#include "tokenizer_utils.h"
#include "unknown.h"

namespace suzume::analysis::counter_detail {

namespace {

bool isChainCounterKanji(char32_t codepoint) {
  // 泊 is a duration unit even though it is not part of the narrower temporal
  // property used by 後/前 rules. 割 is the ratio head that licenses 五分.
  return normalize::isTemporalCounterKanji(codepoint) || codepoint == U'泊' || codepoint == U'割';
}

void appendCounterChainCandidate(const std::vector<char32_t>& codepoints, size_t start_pos,
                                 std::vector<UnknownCandidate>& candidates) {
  // A time/date or ratio chain consists of two or more numeral+counter units:
  // 十時三十分, 二泊三日, 二〇二五年三月, 三割五分. Its internal counters cannot
  // become search boundaries merely because the next character is hiragana.
  if (start_pos > 0 && isChainCounterKanji(codepoints[start_pos - 1])) {
    return;
  }

  size_t scan = start_pos;
  size_t unit_count = 0;
  while (scan < codepoints.size()) {
    const size_t numeral_start = scan;
    while (scan < codepoints.size() && normalize::isNumeralCodepoint(codepoints[scan])) {
      ++scan;
    }
    if (scan == numeral_start || scan >= codepoints.size() || !isChainCounterKanji(codepoints[scan])) {
      break;
    }
    ++scan;
    ++unit_count;
  }
  if (unit_count < 2) {
    return;
  }

  std::string surface = extractSubstring(codepoints, start_pos, scan);
  if (surface.empty()) {
    return;
  }
  auto chain_candidate =
      makeCandidate(surface, start_pos, scan, core::PartOfSpeech::Noun, candidate::kCounterChainMergeBonus, false,
                    CandidateOrigin::Counter, core::ExtendedPOS::NounNumber);
  chain_candidate.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
  chain_candidate.pattern = "counter_chain_merge";
#endif
  candidates.push_back(std::move(chain_candidate));
}

// A lexicalized duration unit is emitted as one search token, and an optional
// following kanji completes it into a longer closed unit (三ヶ月+間, 一時間+目).
// The two duration spellings below differ only in the kana that heads the unit
// and the kanji that completes it.
void appendDurationCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t unit_end,
                              char32_t completion, std::vector<UnknownCandidate>& candidates, const char* unit_pattern,
                              const char* completion_pattern) {
#ifndef SUZUME_DEBUG_INFO
  static_cast<void>(unit_pattern);
  static_cast<void>(completion_pattern);
#endif
  std::string surface = extractSubstring(codepoints, start_pos, unit_end);
  if (surface.empty()) {
    return;
  }
  const bool completion_follows = unit_end < codepoints.size() && codepoints[unit_end] == completion;
  const float unit_cost =
      completion_follows ? candidate::kNumeralKanaMonthMergeBonus : candidate::kDurationCounterMergeBonus;
  auto cand = makeCandidate(surface, start_pos, unit_end, core::PartOfSpeech::Noun, unit_cost, false,
                            CandidateOrigin::Counter, core::ExtendedPOS::NounNumber);
  cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
  cand.pattern = unit_pattern;
#endif
  candidates.push_back(cand);
  if (unit_end >= codepoints.size() || codepoints[unit_end] != completion) {
    return;
  }
  std::string completed_surface = extractSubstring(codepoints, start_pos, unit_end + 1);
  auto completed = makeCandidate(completed_surface, start_pos, unit_end + 1, core::PartOfSpeech::Noun,
                                 candidate::kClosedTemporalCounterMergeBonus, false, CandidateOrigin::Counter,
                                 core::ExtendedPOS::NounNumber);
  completed.lemma = completed_surface;
#ifdef SUZUME_DEBUG_INFO
  completed.pattern = completion_pattern;
#endif
  candidates.push_back(completed);
}

}  // namespace

void appendTemporalCounterCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const std::vector<normalize::CharType>& char_types,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  appendCounterChainCandidate(codepoints, start_pos, candidates);

  // Month counters admit all three common kana spellings between a numeral and
  // 月 (一か月, 一ヶ月, 一ケ月). Keep the complete duration together before
  // any following comparison expression.
  if (normalize::isNumeralCodepoint(codepoints[start_pos])) {
    size_t numeral_end = start_pos;
    while (numeral_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[numeral_end])) {
      ++numeral_end;
    }
    // 時間 is a lexicalized duration unit. The regular counter scan recognizes
    // 時 first, so retain a competing numeral+時+間 candidate for kanji
    // numerals as well as digit-based pretokenized durations.
    if (numeral_end + 1 < codepoints.size() && codepoints[numeral_end] == U'時' &&
        codepoints[numeral_end + 1] == U'間') {
      appendDurationCandidates(codepoints, start_pos, numeral_end + 2, U'目', candidates,
                               "numeral_jikan_duration_merge", "temporal_counter_ordinal_merge");
    }
    if (numeral_end + 1 < codepoints.size() &&
        (codepoints[numeral_end] == U'か' || codepoints[numeral_end] == U'ヶ' || codepoints[numeral_end] == U'ケ') &&
        codepoints[numeral_end + 1] == U'月') {
      appendDurationCandidates(codepoints, start_pos, numeral_end + 2, U'間', candidates, "numeral_kana_month_merge",
                               "temporal_counter_span_merge");
    }
  }

  // Quantified time + relational suffix: split 後/前 off a numeral/quantity run that
  // ends in a temporal counter (三日|後, 十年|前, 数日|後, 半年|前). The whole run is
  // otherwise emitted as one kanji_seq token; the left counter token already exists
  // as a kanji_seq candidate, so a discounted duplicate lets the split path win. The
  // counter must be temporal, keeping lexical wholes on non-temporal counters intact
  // (一人前, not 一人|前).
  {
    size_t scan = start_pos;
    bool has_quantity = false;
    if (normalize::isQuantityPrefixKanji(codepoints[scan])) {
      ++scan;
      has_quantity = true;
    }
    while (scan < codepoints.size() && normalize::isNumeralCodepoint(codepoints[scan])) {
      ++scan;
      has_quantity = true;
    }
    size_t counter_start = scan;
    while (scan < codepoints.size()) {
      if (normalize::isTemporalCounterKanji(codepoints[scan])) {
        ++scan;
        continue;
      }
      // ヶ/ケ heads a counter only with a following kanji (ヶ月); take it as part of
      // the temporal run when that kanji is itself a temporal counter (三ヶ月|後).
      if ((codepoints[scan] == U'ヶ' || codepoints[scan] == U'ケ') && scan + 1 < codepoints.size() &&
          normalize::isTemporalCounterKanji(codepoints[scan + 1])) {
        scan += 2;
        continue;
      }
      break;
    }
    // A temporal counter run followed by a suffix that is always compositional:
    //   - 後/前 relation suffix (三日|後, 十年|前)
    //   - 半 "and a half" (三時間|半, 二年|半, 五分|半, 六ヶ月|半)
    // The 半 case excludes a run ending in bare 時, which keeps the clock reading
    // (三時半 = half past three), not a duration-plus-half.
    bool suffix_is_compositional = false;
    bool suffix_is_half = false;
    if (scan < codepoints.size()) {
      if (normalize::isTemporalRelationSuffixKanji(codepoints[scan])) {
        suffix_is_compositional = true;
      } else if (scan > 0 && codepoints[scan] == U'半' && codepoints[scan - 1] != U'時') {
        suffix_is_compositional = true;
        suffix_is_half = true;
      }
    }
    if (has_quantity && scan > counter_start && suffix_is_compositional) {
      std::string surface = extractSubstring(codepoints, start_pos, scan);
      if (!surface.empty()) {
        auto cand =
            makeCandidate(surface, start_pos, scan, core::PartOfSpeech::Noun, candidate::kCounterRelationSplitBonus,
                          false, CandidateOrigin::Counter, core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "counter_relation_split";
#endif
        candidates.push_back(cand);
      }
      // Unlike 後/前 (single-kanji dict relation nouns), the split-off 半 only
      // exists as a generic kanji_seq NOUN, which the single-kanji-noun →
      // hiragana-verb compound protection penalizes before かかっ/すぎ etc.
      // In a continuing predicate it is a quantity noun (三時間|半|かかった),
      // while at a clause boundary it is the compositional suffix of the
      // duration expression (一時間|半。).
      if (suffix_is_half) {
        std::string half_surface = extractSubstring(codepoints, scan, scan + 1);
        if (!half_surface.empty()) {
          const size_t after_half = scan + 1;
          const bool closes_clause = after_half == codepoints.size() ||
                                     normalize::classifyChar(codepoints[after_half]) == normalize::CharType::Symbol;
          auto half_cand = makeCandidate(half_surface, scan, after_half,
                                         closes_clause ? core::PartOfSpeech::Suffix : core::PartOfSpeech::Noun,
                                         candidate::kCounterHalfSuffixCost, false, CandidateOrigin::Counter,
                                         closes_clause ? core::ExtendedPOS::Suffix : core::ExtendedPOS::NounNumber);
          half_cand.lemma = half_surface;
#ifdef SUZUME_DEBUG_INFO
          half_cand.pattern = "counter_half_suffix";
#endif
          candidates.push_back(half_cand);
        }
      }
    }
  }

  // A numeral followed by one or more temporal-unit kanji is a complete
  // quantity before a hiragana word or degree particle (一昼夜+かけて,
  // 二時間+待つ, 三時間+ほど). Emit the quantity boundary so an unknown
  // word candidate cannot absorb the final temporal kanji as its stem.
  {
    size_t scan = start_pos;
    bool has_quantity = false;
    // An approximation prefix reads as such only at the head of its own word.
    // Inside a kanji run it is the tail of the preceding noun (人数+分, not
    // 人+数分).
    const bool prefix_inside_kanji_run =
        start_pos > 0 && start_pos - 1 < char_types.size() && char_types[start_pos - 1] == normalize::CharType::Kanji;
    if (!prefix_inside_kanji_run && normalize::isQuantityPrefixKanji(codepoints[scan])) {
      ++scan;
      has_quantity = true;
    }
    while (scan < codepoints.size() && normalize::isNumeralCodepoint(codepoints[scan])) {
      ++scan;
      has_quantity = true;
    }
    size_t unit_start = scan;
    while (scan < codepoints.size()) {
      if (normalize::isTemporalCounterKanji(codepoints[scan])) {
        ++scan;
      } else if (codepoints[scan] == U'昼' && scan + 1 < codepoints.size() && codepoints[scan + 1] == U'夜') {
        // 昼夜 is one cyclic temporal unit only as a pair (一昼夜).
        scan += 2;
      } else {
        break;
      }
    }
    bool followed_by_hiragana = scan < char_types.size() && char_types[scan] == normalize::CharType::Hiragana;
    const bool followed_by_extent_suffix =
        scan + 1 < codepoints.size() && codepoints[scan] == U'が' && codepoints[scan + 1] == U'け';
    bool followed_by_quantity_particle = false;
    constexpr size_t kMaxQuantityParticleLength = 4;
    const size_t max_particle_end = std::min(codepoints.size(), scan + kMaxQuantityParticleLength);
    for (size_t particle_end = scan + 1; particle_end <= max_particle_end; ++particle_end) {
      const auto* particle =
          lookupEntryInRange(*dict_manager, codepoints, scan, particle_end, core::PartOfSpeech::Particle);
      if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleAdverbial) {
        followed_by_quantity_particle = true;
        break;
      }
    }
    // 分 followed by の and a numeral is the denominator marker of a fraction
    // (5分の3), not a duration before a particle. The numeral generator emits
    // the whole fraction as one quantity unit, so the duration split must not
    // undercut it — with kanji numerals it never competes, which is why only
    // the digit spelling was breaking.
    const bool opens_fraction_denominator = scan > unit_start && codepoints[scan - 1] == U'分' &&
                                            scan + 1 < codepoints.size() && codepoints[scan] == U'の' &&
                                            normalize::isNumeralCodepoint(codepoints[scan + 1]);
    if (has_quantity && scan > unit_start && !opens_fraction_denominator &&
        (followed_by_hiragana || followed_by_quantity_particle)) {
      std::string surface = extractSubstring(codepoints, start_pos, scan);
      if (!surface.empty()) {
        auto cand = makeCandidate(surface, start_pos, scan, core::PartOfSpeech::Noun, candidate::kCounterNounSplitBonus,
                                  false, CandidateOrigin::Counter, core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "temporal_quantity_hiragana_split";
#endif
        candidates.push_back(cand);
      }
      if (followed_by_extent_suffix) {
        const std::string suffix_surface = extractSubstring(codepoints, scan, scan + 2);
        auto suffix = makeCandidate(suffix_surface, scan, scan + 2, core::PartOfSpeech::Suffix,
                                    candidate::kCounterExtentSuffixCost, true, CandidateOrigin::SuffixPattern,
                                    core::ExtendedPOS::Suffix);
        suffix.lemma = suffix_surface;
#ifdef SUZUME_DEBUG_INFO
        suffix.pattern = "temporal_quantity_extent_suffix";
#endif
        candidates.push_back(std::move(suffix));
      }
    }
  }

  // Duration span + independent kanji noun: a numeral-led temporal-counter run closed
  // by the span marker 間 (三年間, 三ヶ月間, 二時間) is a complete duration, and a kanji
  // noun immediately after 間 is a separate word (三年間|勉強, 三ヶ月間|入院, 二時間|睡眠).
  // The whole run is otherwise one kanji_seq token that beats the split on total cost, so
  // a discounted duplicate of the duration phrase lets the split path win. The trailing
  // kanji must be an ordinary noun char: a temporal counter (三日月 = one word), a
  // relation/span suffix (後/前/中/末, handled elsewhere), or a lone ordinal 目 (二時間目)
  // keeps its own reading. The interval member 隔 heads 間隔 (三年間隔 = 三年|間隔), so the
  // split falls BEFORE 間 instead. The run heads with a numeral or a quantity prefix
  // (数, 半, 何): a 間-closed duration does not merge with a following independent kanji
  // noun (数年間|海外) regardless of how its interior tokenizes — the split-after-間 here
  // only carves the following noun off; the 半年 vs 半|年 interior is decided elsewhere.
  {
    size_t scan = start_pos;
    bool has_quantity = false;
    if (scan < codepoints.size() && normalize::isQuantityPrefixKanji(codepoints[scan])) {
      ++scan;
      has_quantity = true;
    }
    while (scan < codepoints.size() && normalize::isNumeralCodepoint(codepoints[scan])) {
      ++scan;
      has_quantity = true;
    }
    size_t counter_start = scan;
    while (scan < codepoints.size()) {
      if (normalize::isTemporalCounterKanji(codepoints[scan])) {
        ++scan;
        continue;
      }
      if ((codepoints[scan] == U'ヶ' || codepoints[scan] == U'ケ') && scan + 1 < codepoints.size() &&
          normalize::isTemporalCounterKanji(codepoints[scan + 1])) {
        scan += 2;
        continue;
      }
      break;
    }
    // The run must end in 間, and that 間 must be preceded by another counter char in the
    // run (a bare numeral+間 is not a duration).
    bool run_ends_in_span =
        has_quantity && scan > counter_start && scan - 1 > counter_start && codepoints[scan - 1] == U'間';
    if (run_ends_in_span && scan < char_types.size() && char_types[scan] == normalize::CharType::Kanji) {
      // 間 heading the interval word 間隔 splits the numeral+counter off before 間
      // (三年|間隔); otherwise an ordinary kanji noun after 間 splits after it (三年間|勉強).
      // A lone ordinal 目 binds to the duration (二時間目 = one word); 目 heading a noun
      // still splits (五年間|目標, gate: 目 followed by a non-kanji).
      bool trailing_ordinal_me = codepoints[scan] == U'目' &&
                                 (scan + 1 >= char_types.size() || char_types[scan + 1] != normalize::CharType::Kanji);
      if (normalize::isIntervalCompoundSecondKanji(codepoints[scan])) {
        size_t split_end = scan - 1;  // before 間
        std::string surface = extractSubstring(codepoints, start_pos, split_end);
        if (!surface.empty()) {
          auto cand = makeCandidate(surface, start_pos, split_end, core::PartOfSpeech::Noun,
                                    candidate::kDurationSpanSplitBonus, false, CandidateOrigin::Counter);
          cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
          cand.pattern = "duration_interval_split";
#endif
          candidates.push_back(cand);
        }
      } else if (!trailing_ordinal_me && !normalize::isTemporalCounterKanji(codepoints[scan]) &&
                 !normalize::isTemporalRelationSuffixKanji(codepoints[scan]) &&
                 !normalize::isTemporalSpanSuffixKanji(codepoints[scan])) {
        std::string surface = extractSubstring(codepoints, start_pos, scan);
        if (!surface.empty()) {
          auto cand = makeCandidate(surface, start_pos, scan, core::PartOfSpeech::Noun,
                                    candidate::kDurationSpanSplitBonus, false, CandidateOrigin::Counter);
          cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
          cand.pattern = "duration_span_split";
#endif
          candidates.push_back(cand);
        }
      }
    } else if (run_ends_in_span) {
      // Nothing continues the kanji run, so the 間-closed duration is a complete
      // quantity of its own (数年間, 三日間).
      std::string surface = extractSubstring(codepoints, start_pos, scan);
      if (!surface.empty()) {
        auto cand =
            makeCandidate(surface, start_pos, scan, core::PartOfSpeech::Noun, candidate::kNumeralCounterMergeBonus,
                          false, CandidateOrigin::Counter, core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "duration_span_whole";
#endif
        candidates.push_back(cand);
      }
    }
  }
}

}  // namespace suzume::analysis::counter_detail
