/**
 * @file suffix_candidates_counter_numeral.cpp
 * @brief Basic numeral and kanji-counter candidate generation
 */

#include <string>
#include <utility>

#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "dictionary/dictionary.h"
#include "normalize/char_type.h"
#include "suffix_candidates.h"
#include "suffix_candidates_counter_internal.h"
#include "tokenizer_utils.h"
#include "unknown.h"

namespace suzume::analysis::counter_detail {

void appendBasicNumeralCounterCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t numeral_end,
                                         const std::vector<normalize::CharType>& char_types,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates) {
  // A temporal counter closed by the span marker 間 is one duration unit
  // (三日間, 五年間).  Every boundary rule below that would cut before 間 has to
  // stand down for it, so the test is computed once here.
  const bool closes_duration_span = normalize::isTemporalCounterKanji(codepoints[numeral_end]) &&
                                    numeral_end + 1 < codepoints.size() && codepoints[numeral_end + 1] == U'間';

  // The extent marker 中 closes a quantity into one search unit: a ratio
  // (10件中3件, 百人中一人) or a span covering the whole quantity (一日中,
  // 一週間中).  After a plain noun the same kanji is the ordinary state suffix
  // (作業|中, 会議|中) and keeps its own boundary, so the numeral+counter left
  // context is what licenses the merge.  A following kanji is left out because
  // 中 then usually heads a lexical compound of its own (一時|中断, 三年|中学);
  // a numeral is exempt since it opens the second term of the ratio.  Like the
  // duration span above, the boundary rules that would cut before 中 stand down
  // for it, so the test is computed once here.
  const size_t extent_pos = numeral_end + (closes_duration_span ? 2 : 1);
  const bool closes_quantity_extent = [&] {
    if (!normalize::isCounterKanji(codepoints[numeral_end]) || extent_pos >= codepoints.size() ||
        codepoints[extent_pos] != U'中') {
      return false;
    }
    const size_t after_extent = extent_pos + 1;
    return after_extent >= codepoints.size() || !normalize::isKanjiCodepoint(codepoints[after_extent]) ||
           normalize::isNumeralCodepoint(codepoints[after_extent]);
  }();
  if (closes_quantity_extent) {
    std::string surface = extractSubstring(codepoints, start_pos, extent_pos + 1);
    auto cand = makeCandidate(surface, start_pos, extent_pos + 1, core::PartOfSpeech::Noun,
                              candidate::kQuantityExtentMergeBonus, false, CandidateOrigin::Counter,
                              core::ExtendedPOS::NounNumber);
    cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
    cand.pattern = "quantity_extent_naka";
#endif
    candidates.push_back(cand);
  }

  // A numeral+counter phrase can modify an i-adjective in adverbial form
  // (百件|近く確認する, 三日|早く終える). Preserve the quantity boundary so
  // the generic kanji sequence cannot absorb the adjective's stem. The same
  // structural boundary is also valid when the following ～く is a verb
  // (十人|歩く), so no lexical adjective list is needed here.
  if (normalize::isCounterKanji(codepoints[numeral_end]) && numeral_end + 2 < char_types.size() &&
      char_types[numeral_end + 1] == normalize::CharType::Kanji && codepoints[numeral_end + 2] == U'く') {
    std::string surface = extractSubstring(codepoints, start_pos, numeral_end + 1);
    if (!surface.empty()) {
      auto cand = makeCandidate(surface, start_pos, numeral_end + 1, core::PartOfSpeech::Noun,
                                candidate::kCounterNounSplitBonus, false, CandidateOrigin::Counter,
                                core::ExtendedPOS::NounNumber);
      cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
      cand.pattern = "counter_before_kanji_ku_split";
#endif
      candidates.push_back(cand);
    }
  }

  // A numeral+counter phrase before the independent comparison expression
  // 以上 is a compositional boundary (百倍|以上, 三名|以上).  The counter
  // candidate exists already, but discount this instance so a long unknown
  // kanji run cannot absorb the comparison term and a following predicate.
  if (numeral_end + 2 < codepoints.size() && normalize::isCounterKanji(codepoints[numeral_end]) &&
      codepoints[numeral_end + 1] == U'以' && codepoints[numeral_end + 2] == U'上') {
    std::string surface = extractSubstring(codepoints, start_pos, numeral_end + 1);
    if (!surface.empty()) {
      auto cand = makeCandidate(surface, start_pos, numeral_end + 1, core::PartOfSpeech::Noun,
                                candidate::kCounterComparisonSplitBonus, false, CandidateOrigin::Counter,
                                core::ExtendedPOS::NounNumber);
      cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
      cand.pattern = "counter_comparison_split";
#endif
      candidates.push_back(cand);
    }
  }

  // Approximate count: numeral + 数 + counter (十数件, 百数名).  数 binds
  // directly to the following counter, while the leading cardinal remains a
  // separate search unit.  Requiring a counter after 数 excludes ordinary
  // lexical compounds beginning with 数.
  if (numeral_end + 1 < codepoints.size() && codepoints[numeral_end] == U'数' &&
      normalize::isCounterKanji(codepoints[numeral_end + 1])) {
    std::string surface = extractSubstring(codepoints, start_pos, numeral_end);
    if (!surface.empty()) {
      auto cand = makeCandidate(surface, start_pos, numeral_end, core::PartOfSpeech::Noun,
                                candidate::kApproximateNumeralSplitBonus, false, CandidateOrigin::Counter,
                                core::ExtendedPOS::NounNumber);
      cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
      cand.pattern = "approximate_numeral_before_su_counter";
#endif
      candidates.push_back(cand);
    }
  }

  // Fraction: numeral + 分 + の + numeral (三分の一, 十分の三).  The
  // denominator marker requires both numeric sides, so duration phrases such
  // as 一分の休憩 never enter this branch.  Keep the complete fraction as one
  // quantity search unit, including when a following counter is present
  // (三分の一秒).
  if (numeral_end + 2 < codepoints.size() && codepoints[numeral_end] == U'分' && codepoints[numeral_end + 1] == U'の' &&
      normalize::isNumeralCodepoint(codepoints[numeral_end + 2])) {
    size_t denominator_end = numeral_end + 2;
    while (denominator_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[denominator_end])) {
      ++denominator_end;
    }
    std::string surface = extractSubstring(codepoints, start_pos, denominator_end);
    if (!surface.empty()) {
      auto cand =
          makeCandidate(surface, start_pos, denominator_end, core::PartOfSpeech::Noun, candidate::kFractionMergeCost,
                        false, CandidateOrigin::Counter, core::ExtendedPOS::NounNumber);
      cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
      cand.pattern = "fraction_numerator_bun_no_denominator";
#endif
      candidates.push_back(cand);
    }
  }

  // A numeral+counter preceding a registered suffix is compositional even when
  // the suffix starts with kanji (二階|建て, 二本|立て).  Consult the suffix
  // lexicon rather than enumerating suffix spellings here, so every closed-class
  // suffix can share the same quantity boundary rule.
  if (dict_manager != nullptr && normalize::isCounterKanji(codepoints[numeral_end])) {
    size_t counter_end = numeral_end + 1;
    std::string suffix_text = extractSubstring(codepoints, counter_end, codepoints.size());
    const bool suffix_follows = lookupResultsHavePartOfSpeech(dict_manager->lookup(suffix_text, 0),
                                                              partOfSpeechMask(core::PartOfSpeech::Suffix));
    if (closes_duration_span) {
      std::string surface = extractSubstring(codepoints, start_pos, counter_end + 1);
      auto cand = makeCandidate(surface, start_pos, counter_end + 1, core::PartOfSpeech::Noun,
                                candidate::kNumeralCounterMergeBonus, false, CandidateOrigin::Counter,
                                core::ExtendedPOS::NounNumber);
      cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
      cand.pattern = "temporal_counter_duration_span";
#endif
      candidates.push_back(cand);
    }
    if (suffix_follows && !closes_duration_span && !closes_quantity_extent) {
      std::string surface = extractSubstring(codepoints, start_pos, counter_end);
      if (!surface.empty()) {
        auto cand = makeCandidate(surface, start_pos, counter_end, core::PartOfSpeech::Noun,
                                  candidate::kCounterNounSplitBonus, false, CandidateOrigin::Counter);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "counter_registered_suffix_split";
#endif
        candidates.push_back(cand);
      }
    }
  }

  // A deverbal counter is written with its okurigana (一切れ, 三重ね), so the
  // quantity phrase spans past the numeral+kanji prefix that a dictionary entry
  // happens to cover — the adverb 一切, the noun 三重.  The registered
  // continuative behind the counter is what licenses the extra kana, and the
  // char-property table cannot hold the counter because its kanji alone spells
  // something else.  Only a single trailing kana qualifies: a longer okurigana
  // run is an inflected predicate, not a counter.
  if (dict_manager != nullptr && numeral_end + 1 < codepoints.size() &&
      normalize::isKanjiCodepoint(codepoints[numeral_end]) &&
      normalize::classifyChar(codepoints[numeral_end + 1]) == normalize::CharType::Hiragana) {
    const size_t counter_end = numeral_end + 2;
    const bool has_deverbal_counter =
        lookupResultsHavePartOfSpeech(dict_manager->lookup(extractSubstring(codepoints, numeral_end, counter_end), 0),
                                      partOfSpeechMask(core::PartOfSpeech::Verb));
    // A longer registered form of the same verb is an inflected predicate, and
    // it owns the span rather than the counter reading (二重ねる, 三切れる).
    bool inflected_predicate = false;
    for (size_t probe_end = counter_end + 1; probe_end <= std::min(codepoints.size(), counter_end + 2); ++probe_end) {
      if (hasExactPartOfSpeech(*dict_manager, extractSubstring(codepoints, numeral_end, probe_end),
                               partOfSpeechMask(core::PartOfSpeech::Verb))) {
        inflected_predicate = true;
        break;
      }
    }
    // A formal noun in the same slot is the same construction read off the
    // other side of the lexicon: 通り counts ways (一通り, 三通り) just as 切れ
    // counts slices, and its kanji alone spells the unrelated counter 通
    // (手紙三通). Only the formal-noun subclass qualifies — an ordinary noun
    // after a numeral is a modified nominal, not a quantity phrase.
    const auto* formal_noun =
        lookupEntryInRange(*dict_manager, codepoints, numeral_end, counter_end, core::PartOfSpeech::Noun);
    const bool has_formal_noun_counter =
        formal_noun != nullptr && formal_noun->extended_pos == core::ExtendedPOS::NounFormal;
    if ((has_deverbal_counter || has_formal_noun_counter) && !inflected_predicate) {
      std::string surface = extractSubstring(codepoints, start_pos, counter_end);
      auto cand =
          makeCandidate(surface, start_pos, counter_end, core::PartOfSpeech::Noun, candidate::kCounterNounSplitBonus,
                        false, CandidateOrigin::Counter, core::ExtendedPOS::NounNumber);
      cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
      cand.pattern = "numeral_okurigana_counter";
#endif
      candidates.push_back(cand);
    }
  }

  // Check for counter suffix (つ for native counters)
  char32_t next = codepoints[numeral_end];
  if (next == U'つ') {
    // Generate counter candidate: Nつ
    std::string surface = extractSubstring(codepoints, start_pos, numeral_end + 1);
    if (!surface.empty()) {
      auto cand = makeCandidate(surface, start_pos, numeral_end + 1, core::PartOfSpeech::Noun,
                                candidate::kNativeTsuCounterBonus, false, CandidateOrigin::Counter);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 0.95F;
      cand.pattern = "counter_tsu";
#endif
      candidates.push_back(cand);
    }
  }

  // Numeral(s) + a single kanji counter at a kanji→non-kanji boundary (三十度, 九十度,
  // 三十分, 十本) is a number+counter search unit. 度 also reads as a generic nominal
  // suffix (態度, 難易度), so its dictionary Suffix reading plus the suffix-stem split
  // pull a multi-digit numeral apart (三十|度); a discounted merged candidate keeps the
  // unit whole. Gated to a lone counter kanji followed by a non-kanji so a following
  // kanji noun/suffix (五度目, 五度見た, 三年間) keeps its own boundary.
  if (numeral_end < char_types.size() && normalize::isCounterKanji(codepoints[numeral_end]) &&
      (numeral_end + 1 >= char_types.size() || char_types[numeral_end + 1] != normalize::CharType::Kanji)) {
    std::string surface = extractSubstring(codepoints, start_pos, numeral_end + 1);
    if (!surface.empty()) {
      auto cand = makeCandidate(surface, start_pos, numeral_end + 1, core::PartOfSpeech::Noun,
                                candidate::kNumeralCounterMergeBonus, false, CandidateOrigin::Counter);
      cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
      cand.pattern = "numeral_kanji_counter";
#endif
      candidates.push_back(cand);
    }
  }

  // A quantity followed by a kanji サ変名詞 keeps its counter boundary
  // (一回|実施する, 三名|確認する).  The ordinary lone-counter branch above
  // deliberately avoids a following kanji because lexical compounds such as
  // 一回戦 must remain available; requiring the complete nominal+する
  // predicate distinguishes the productive quantity construction.
  if (numeral_end < char_types.size() && normalize::isCounterKanji(codepoints[numeral_end])) {
    const bool repeated_predicate_unit = isRepeatedNumeralNounPredicateUnitAt(codepoints, char_types, start_pos);
    if (!repeated_predicate_unit && !closes_duration_span && dict_manager != nullptr &&
        headsKanjiSuruPredicateAt(*dict_manager, codepoints, char_types, numeral_end + 1)) {
      std::string surface = extractSubstring(codepoints, start_pos, numeral_end + 1);
      if (!surface.empty()) {
        auto cand = makeCandidate(surface, start_pos, numeral_end + 1, core::PartOfSpeech::Noun,
                                  candidate::kCounterNounSplitBonus, false, CandidateOrigin::Counter,
                                  core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "counter_suru_predicate_split";
#endif
        candidates.push_back(cand);
      }
    }
  }
}

}  // namespace suzume::analysis::counter_detail
