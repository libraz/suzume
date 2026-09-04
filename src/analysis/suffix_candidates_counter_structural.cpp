/**
 * @file suffix_candidates_counter_structural.cpp
 * @brief Structural counter boundaries outside the basic numeral scan
 */

#include <algorithm>

#include "candidate_constants.h"
#include "dictionary_probe.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "suffix_candidates.h"
#include "suffix_candidates_counter_internal.h"
#include "tokenizer_utils.h"
#include "unknown.h"

namespace suzume::analysis::counter_detail {
namespace {

// Discrete-object counter kanji whose numeral+counter phrase is a pure quantity
// (三名, 二台, 五冊, 三箱) and never heads a lexical compound. Deliberately
// narrower than normalize::isCounterKanji: measure/rank/event counters (段, 本,
// 枚, 件, 頭, 級, …) head four-character lexical nouns (五段活用, 一本調子,
// 一枚看板, 一件落着, 三頭政治) and must keep merging with what follows.
bool isObjectCounterKanji(char32_t code_point) {
  switch (code_point) {
    case U'人':
    case U'名':
    case U'台':
    case U'冊':
    case U'箱':
    case U'袋':
    case U'匹':
    case U'個':
      return true;
    default:
      return false;
  }
}

// NounNumber is a closed L1 class. Its longest kana surface is ここのつ
// (four codepoints); the Suffix entries that may complete a kana quantity are
// likewise closed, with a six-codepoint compatibility window. Keeping these
// probes fixed prevents ordinary long hiragana from turning a counter check
// into a scan of the entire remaining input.
constexpr size_t kMaxKanaNounNumberLength = 4;
constexpr size_t kMaxKanaCounterSuffixLength = 6;
}  // namespace

}  // namespace suzume::analysis::counter_detail

namespace suzume::analysis {

size_t repeatedNumeralNounUnitEndAt(const std::vector<char32_t>& codepoints,
                                    const std::vector<normalize::CharType>& char_types, size_t start_pos) {
  if (start_pos >= codepoints.size() || start_pos >= char_types.size() ||
      !normalize::isNumeralCodepoint(codepoints[start_pos])) {
    return 0;
  }

  size_t numeral_end = start_pos;
  while (numeral_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[numeral_end])) {
    ++numeral_end;
  }
  if (numeral_end >= codepoints.size() || numeral_end >= char_types.size() ||
      char_types[numeral_end] != normalize::CharType::Kanji) {
    return 0;
  }

  const size_t unit_end = numeral_end + 1;
  const size_t unit_length = unit_end - start_pos;
  const size_t repeated_end = unit_end + unit_length;
  if (repeated_end > codepoints.size()) {
    return 0;
  }
  for (size_t offset = 0; offset < unit_length; ++offset) {
    if (codepoints[start_pos + offset] != codepoints[unit_end + offset]) {
      return 0;
    }
  }
  return repeated_end;
}

bool isRepeatedNumeralNounPredicateUnitAt(const std::vector<char32_t>& codepoints,
                                          const std::vector<normalize::CharType>& char_types, size_t start_pos) {
  size_t repeated_end = repeatedNumeralNounUnitEndAt(codepoints, char_types, start_pos);
  if (repeated_end == 0 && start_pos < codepoints.size() && normalize::isNumeralCodepoint(codepoints[start_pos])) {
    size_t numeral_end = start_pos;
    while (numeral_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[numeral_end])) {
      ++numeral_end;
    }
    if (numeral_end < char_types.size() && char_types[numeral_end] == normalize::CharType::Kanji) {
      const size_t unit_end = numeral_end + 1;
      const size_t unit_length = unit_end - start_pos;
      if (start_pos >= unit_length) {
        const size_t repeated_start = start_pos - unit_length;
        const size_t preceding_repeated_end = repeatedNumeralNounUnitEndAt(codepoints, char_types, repeated_start);
        if (preceding_repeated_end == unit_end) {
          repeated_end = preceding_repeated_end;
        }
      }
    }
  }
  return repeated_end != 0 && hasKanjiSuruPredicateAt(codepoints, char_types, repeated_end, 2);
}

}  // namespace suzume::analysis

namespace suzume::analysis::counter_detail {

void appendStructuralCounterCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                       const std::vector<normalize::CharType>& char_types,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= codepoints.size() || start_pos >= char_types.size()) {
    return;
  }

  // Kana quantity readings are a finite composition of the closed NounNumber
  // and quantitative Suffix classes (いち+まい, よん+にん).  MeCab may split
  // these at arbitrary syllable boundaries, but Suzume already owns both
  // grammatical components in L1; emit the complete quantity search unit
  // without registering any open-class word.
  if (dict_manager != nullptr && char_types[start_pos] == normalize::CharType::Hiragana) {
    const size_t number_probe_end = std::min(codepoints.size(), start_pos + kMaxKanaNounNumberLength);
    for (const auto& number_result : lookupResultsInRange(*dict_manager, codepoints, start_pos, number_probe_end)) {
      if (number_result.entry == nullptr || number_result.entry->pos != core::PartOfSpeech::Noun ||
          number_result.entry->extended_pos != core::ExtendedPOS::NounNumber) {
        continue;
      }
      const size_t number_end = start_pos + number_result.length;
      const size_t suffix_probe_end = std::min(codepoints.size(), number_end + kMaxKanaCounterSuffixLength);
      for (const auto& suffix_result : lookupResultsInRange(*dict_manager, codepoints, number_end, suffix_probe_end)) {
        if (suffix_result.entry == nullptr || suffix_result.entry->pos != core::PartOfSpeech::Suffix) {
          continue;
        }
        const size_t suffix_end = number_end + suffix_result.length;
        std::string surface = extractSubstring(codepoints, start_pos, suffix_end);
        auto cand = makeCandidate(surface, start_pos, suffix_end, core::PartOfSpeech::Noun,
                                  candidate::kKanaNumeralCounterMergeBonus, true, CandidateOrigin::Counter,
                                  core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "kana_numeral_counter";
#endif
        candidates.push_back(cand);
      }
    }
  }

  // Indefinite approximate duration: 数+か+temporal counter (数か月,
  // 数か年).  数 supplies the quantity and か is the counter linker, so the
  // three-character quantity remains one search unit even before a following
  // kanji noun such as 後.
  if (start_pos + 2 < codepoints.size() && codepoints[start_pos] == U'数' && codepoints[start_pos + 1] == U'か' &&
      normalize::isTemporalCounterKanji(codepoints[start_pos + 2])) {
    std::string surface = extractSubstring(codepoints, start_pos, start_pos + 3);
    auto cand =
        makeCandidate(surface, start_pos, start_pos + 3, core::PartOfSpeech::Noun, candidate::kNumeralCounterMergeBonus,
                      false, CandidateOrigin::Counter, core::ExtendedPOS::NounNumber);
    cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
    cand.pattern = "indefinite_approximate_duration";
#endif
    candidates.push_back(cand);
  }

  // Repeated numeral-counter units are distributive quantity expressions
  // (一人一人, 一日一日). Keep two identical units as one search unit rather
  // than allowing each discounted counter candidate to split the expression.
  if (normalize::isNumeralCodepoint(codepoints[start_pos])) {
    size_t numeral_end = start_pos;
    while (numeral_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[numeral_end])) {
      ++numeral_end;
    }
    if (numeral_end < codepoints.size() && normalize::isCounterKanji(codepoints[numeral_end])) {
      const size_t repeated_end = repeatedNumeralNounUnitEndAt(codepoints, char_types, start_pos);
      if (repeated_end != 0) {
        std::string surface = extractSubstring(codepoints, start_pos, repeated_end);
        if (!surface.empty()) {
          auto cand = makeCandidate(surface, start_pos, repeated_end, core::PartOfSpeech::Noun,
                                    candidate::kNumeralCounterMergeBonus, false, CandidateOrigin::Counter,
                                    core::ExtendedPOS::NounNumber);
          cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
          cand.pattern = "repeated_numeral_counter";
#endif
          candidates.push_back(cand);
        }
      }
    }
  }

  // A native つ counter repeated in its kana spelling is the same distributive
  // expression written across two scripts (一つひとつ). The kana member has to be
  // a registered closed-class quantity ending in the same counter, so an
  // ordinary kana run behind a quantity cannot enter the rule and neither can a
  // short kana numeral that also opens a common word (一つ|とおもう).
  if (dict_manager != nullptr && normalize::isNumeralCodepoint(codepoints[start_pos])) {
    size_t numeral_end = start_pos;
    while (numeral_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[numeral_end])) {
      ++numeral_end;
    }
    if (numeral_end < codepoints.size() && codepoints[numeral_end] == U'つ') {
      const size_t unit_end = numeral_end + 1;
      size_t repeated_end = 0;
      const size_t kana_probe_end = std::min(codepoints.size(), unit_end + kMaxKanaNounNumberLength);
      for (const auto& result : lookupResultsInRange(*dict_manager, codepoints, unit_end, kana_probe_end)) {
        if (result.entry == nullptr || result.entry->extended_pos != core::ExtendedPOS::NounNumber ||
            result.length == 0 || codepoints[unit_end + result.length - 1] != U'つ') {
          continue;
        }
        repeated_end = unit_end + result.length;
        break;
      }
      if (repeated_end != 0) {
        std::string surface = extractSubstring(codepoints, start_pos, repeated_end);
        auto cand = makeCandidate(surface, start_pos, repeated_end, core::PartOfSpeech::Noun,
                                  candidate::kMixedScriptRepeatedQuantityBonus, false, CandidateOrigin::Counter,
                                  core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "mixed_script_repeated_quantity";
#endif
        candidates.push_back(cand);
      }
    }
  }

  // A repeated numeral+noun unit before a kanji サ変 predicate is a
  // distributive quantity phrase (一語一語|確認する, 一件一件|点検する).
  // Its unit kanji is intentionally not limited to the closed counter inventory:
  // ordinary nouns such as 語 and 歩 productively form this shape.
  // Requiring a complete kanji+する predicate keeps lexical kanji compounds
  // and standalone repetitions outside this boundary rule.
  const size_t repeated_noun_end = repeatedNumeralNounUnitEndAt(codepoints, char_types, start_pos);
  if (repeated_noun_end != 0) {
    const bool has_registered_predicate =
        dict_manager != nullptr &&
        lookupResultsHavePartOfSpeech(
            lookupResultsInRange(*dict_manager, codepoints, repeated_noun_end, codepoints.size()),
            partOfSpeechMask(core::PartOfSpeech::Verb));
    if (hasKanjiSuruPredicateAt(codepoints, char_types, repeated_noun_end, 2) || has_registered_predicate) {
      std::string surface = extractSubstring(codepoints, start_pos, repeated_noun_end);
      if (!surface.empty()) {
        auto cand = makeCandidate(surface, start_pos, repeated_noun_end, core::PartOfSpeech::Noun,
                                  candidate::kRepeatedNumeralNounPredicateSplitBonus, false, CandidateOrigin::Counter,
                                  core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "repeated_numeral_noun_predicate_split";
#endif
        candidates.push_back(cand);
      }
    }
  }

  // Ordinal compounds start with 第 followed by a numeral sequence. Counter
  // tails have dedicated structural boundaries, while lexicalized ordinal
  // nouns are supplied by the dictionary. This prevents an arbitrary one-kanji
  // noun following an ordinal from being fabricated as a compound.
  if (start_pos + 1 < codepoints.size() && codepoints[start_pos] == U'第' &&
      normalize::isNumeralCodepoint(codepoints[start_pos + 1])) {
    size_t ordinal_end = start_pos + 1;
    bool ordinal_has_numeral = false;
    while (ordinal_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[ordinal_end])) {
      ordinal_has_numeral = true;
      ++ordinal_end;
    }
    if (ordinal_end < char_types.size() && char_types[ordinal_end] == normalize::CharType::Kanji) {
      size_t tail_end = ordinal_end;
      while (tail_end < char_types.size() && char_types[tail_end] == normalize::CharType::Kanji) {
        ++tail_end;
      }
      size_t tail_len = tail_end - ordinal_end;
      if (tail_len == 1 && ordinal_has_numeral && normalize::isCounterKanji(codepoints[ordinal_end])) {
        std::string ordinal_surface = extractSubstring(codepoints, start_pos, ordinal_end);
        std::string counter_surface = extractSubstring(codepoints, ordinal_end, tail_end);
        if (!ordinal_surface.empty()) {
          auto ordinal = makeCandidate(ordinal_surface, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                                       candidate::kOrdinalDigitCounterSplitBonus, false, CandidateOrigin::Counter,
                                       core::ExtendedPOS::NounNumber);
          ordinal.lemma = ordinal_surface;
#ifdef SUZUME_DEBUG_INFO
          ordinal.pattern = "ordinal_digit_counter_prefix";
#endif
          candidates.push_back(ordinal);
        }
        if (!counter_surface.empty()) {
          auto counter = makeCandidate(counter_surface, ordinal_end, tail_end, core::PartOfSpeech::Suffix,
                                       candidate::kOrdinalDigitCounterSplitBonus, false, CandidateOrigin::Counter);
          counter.lemma = counter_surface;
#ifdef SUZUME_DEBUG_INFO
          counter.pattern = "ordinal_digit_counter_suffix";
#endif
          candidates.push_back(counter);
        }
      } else if (tail_len == 2 && ordinal_has_numeral && normalize::isCounterKanji(codepoints[ordinal_end]) &&
                 codepoints[ordinal_end + 1] == U'目') {
        std::string ordinal_surface = extractSubstring(codepoints, start_pos, ordinal_end);
        std::string counter_surface = extractSubstring(codepoints, ordinal_end, ordinal_end + 1);
        std::string ordinal_suffix_surface = extractSubstring(codepoints, ordinal_end + 1, tail_end);
        if (!ordinal_surface.empty() && !counter_surface.empty() && !ordinal_suffix_surface.empty()) {
          auto ordinal = makeCandidate(ordinal_surface, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                                       candidate::kOrdinalDigitCounterSplitBonus, false, CandidateOrigin::Counter,
                                       core::ExtendedPOS::NounNumber);
          ordinal.lemma = ordinal_surface;
          auto counter = makeCandidate(counter_surface, ordinal_end, ordinal_end + 1, core::PartOfSpeech::Suffix,
                                       candidate::kOrdinalDigitCounterSplitBonus, false, CandidateOrigin::Counter);
          counter.lemma = counter_surface;
          auto ordinal_suffix =
              makeCandidate(ordinal_suffix_surface, ordinal_end + 1, tail_end, core::PartOfSpeech::Suffix,
                            candidate::kOrdinalDigitCounterSplitBonus, false, CandidateOrigin::Counter);
          ordinal_suffix.lemma = ordinal_suffix_surface;
#ifdef SUZUME_DEBUG_INFO
          ordinal.pattern = "ordinal_counter_ordinal_suffix_prefix";
          counter.pattern = "ordinal_counter_ordinal_suffix_counter";
          ordinal_suffix.pattern = "ordinal_counter_ordinal_suffix_tail";
#endif
          candidates.push_back(ordinal);
          candidates.push_back(counter);
          candidates.push_back(ordinal_suffix);
        }
      } else if (tail_len >= 2 && ordinal_has_numeral && normalize::isCounterKanji(codepoints[ordinal_end]) &&
                 codepoints[ordinal_end] != U'次') {
        // A longer kanji tail behind the ordinal is a word of its own, and the
        // counter reading of its first kanji cuts inside it (第2|部門, not
        // 第2部|門). Only the ordinal boundary is emitted; the tail keeps the
        // ordinary kanji-run analysis. A tail that does not open with a counter
        // needs no boundary here, so a lexicalized ordinal noun (第三者, 第一
        // 印象) retains its whole-word path.
        std::string ordinal_surface = extractSubstring(codepoints, start_pos, ordinal_end);
        if (!ordinal_surface.empty()) {
          auto ordinal = makeCandidate(ordinal_surface, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                                       candidate::kOrdinalDigitCounterSplitBonus, false, CandidateOrigin::Counter,
                                       core::ExtendedPOS::NounNumber);
          ordinal.lemma = ordinal_surface;
#ifdef SUZUME_DEBUG_INFO
          ordinal.pattern = "ordinal_kanji_word_prefix";
#endif
          candidates.push_back(ordinal);
        }
      } else if (tail_len >= 2 && codepoints[ordinal_end] == U'次') {
        std::string ordinal_surface = extractSubstring(codepoints, start_pos, ordinal_end);
        std::string tail_surface = extractSubstring(codepoints, ordinal_end, tail_end);
        if (!ordinal_surface.empty()) {
          auto ordinal = makeCandidate(ordinal_surface, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                                       candidate::kOrdinalSequentialSplitBonus, false, CandidateOrigin::Counter,
                                       core::ExtendedPOS::NounNumber);
          ordinal.lemma = ordinal_surface;
#ifdef SUZUME_DEBUG_INFO
          ordinal.pattern = "ordinal_sequential_prefix";
#endif
          candidates.push_back(ordinal);
        }
        if (!tail_surface.empty()) {
          auto tail = makeCandidate(tail_surface, ordinal_end, tail_end, core::PartOfSpeech::Suffix,
                                    candidate::kOrdinalSequentialSplitBonus, false, CandidateOrigin::Counter);
          tail.lemma = tail_surface;
#ifdef SUZUME_DEBUG_INFO
          tail.pattern = "ordinal_sequential_tail";
#endif
          candidates.push_back(tail);
        }
      }
    }
  }

  counter_detail::appendTemporalCounterCandidates(codepoints, start_pos, char_types, dict_manager, candidates);

  // Quantity + object/temporal counter + independent kanji noun: a numeral+counter
  // phrase followed by exactly two more kanji is compositional (三名|参加, 二台|故障,
  // 一度|確認) — the counter phrase is a search-unit boundary. The whole run is
  // otherwise emitted as one kanji_seq token that beats the split on total cost,
  // so a discounted duplicate of the counter phrase lets the split path win.
  // Structural gates keep lexical wholes intact:
  //   - discrete-object and temporal counters only; measure/rank counters head
  //     lexical compounds and never fire here
  //   - exactly two trailing kanji ending the run: one trailing kanji is a
  //     lexical suffix compound (一人前, 一年生, 二階建て), three or more a
  //     longer lexical term (三人称単数, 二世帯住宅)
  //   - a numeral/quantity kanji heading the trailing pair marks a reduplicated
  //     idiom (十人十色, 一日千秋) and blocks the split
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
    if (has_quantity && scan < codepoints.size() &&
        (isObjectCounterKanji(codepoints[scan]) || normalize::isTemporalCounterKanji(codepoints[scan]))) {
      size_t counter_end = scan + 1;
      bool trailing_two_kanji = counter_end + 1 < char_types.size() &&
                                char_types[counter_end] == normalize::CharType::Kanji &&
                                char_types[counter_end + 1] == normalize::CharType::Kanji;
      bool run_ends_after_pair =
          counter_end + 2 >= char_types.size() || char_types[counter_end + 2] != normalize::CharType::Kanji;
      bool trailing_is_reduplication =
          trailing_two_kanji && (normalize::isNumeralCodepoint(codepoints[counter_end]) ||
                                 normalize::isQuantityPrefixKanji(codepoints[counter_end]));
      const bool repeated_predicate_unit = isRepeatedNumeralNounPredicateUnitAt(codepoints, char_types, start_pos);
      // A pair that ends in a quantity-phrase suffix is not a lexical noun: the
      // suffix binds to the counter phrase on its left, so splitting here strands
      // it (1週+間目, 3年+間半). Pairs headed by a counter but ending in ordinary
      // kanji are still lexical and keep the split (5分+間隔).
      bool trailing_closes_quantity =
          trailing_two_kanji && normalize::isQuantityPhraseSuffixKanji(codepoints[counter_end + 1]);
      if (trailing_two_kanji && run_ends_after_pair && !trailing_is_reduplication && !repeated_predicate_unit &&
          !trailing_closes_quantity) {
        std::string surface = extractSubstring(codepoints, start_pos, counter_end);
        if (!surface.empty()) {
          auto cand = makeCandidate(surface, start_pos, counter_end, core::PartOfSpeech::Noun,
                                    candidate::kCounterNounSplitBonus, false, CandidateOrigin::Counter);
          cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
          cand.pattern = "counter_object_split";
#endif
          candidates.push_back(cand);
        }
      }
    }
  }

  // Leading kanji noun/prefix + numeral(s) + counter: split before the numeral so the
  // numeral+counter search unit stays intact (徒歩|五分, 約|二時間, 気温|三十度,
  // 定員|五名). The whole run is otherwise one kanji_seq NOUN that beats the split on
  // total cost, so a discounted duplicate of the leading token lets the split win.
  // Structural gates keep lexical wholes intact: the leading run is either a numeric-
  // aggregation prefix (約/計/総) or 2+ kanji — a single non-prefix kanji heads a
  // lexical compound (中二階, 高三) — and the numeral must be immediately followed by a
  // counter kanji or a katakana unit (五メートル, 五キロ), so a bare numeral compound
  // (十字路, 百貨店, 世界一) or a kanji-run non-counter (東京五輪, 富士五湖) never fires.
  {
    size_t lead = start_pos;
    while (lead < codepoints.size() && lead < char_types.size() && char_types[lead] == normalize::CharType::Kanji &&
           !normalize::isNumeralCodepoint(codepoints[lead])) {
      ++lead;
    }
    size_t lead_len = lead - start_pos;
    bool lead_is_prefix = lead_len == 1 && normalize::isNumericApproxPrefixKanji(codepoints[start_pos]);
    if ((lead_len >= 2 || lead_is_prefix) && lead < codepoints.size() &&
        normalize::isNumeralCodepoint(codepoints[lead])) {
      size_t num_end = lead;
      while (num_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[num_end])) {
        ++num_end;
      }
      bool counter_follows = num_end < codepoints.size() &&
                             (normalize::isCounterKanji(codepoints[num_end]) ||
                              (num_end < char_types.size() && char_types[num_end] == normalize::CharType::Katakana));
      if (counter_follows) {
        std::string surface = extractSubstring(codepoints, start_pos, lead);
        if (!surface.empty()) {
          // An approximation prefix (約/計/総) is a Prefix modifying the quantity; a
          // multi-kanji leading run is an ordinary Noun.
          core::PartOfSpeech lead_pos = lead_is_prefix ? core::PartOfSpeech::Prefix : core::PartOfSpeech::Noun;
          core::ExtendedPOS lead_epos = lead_is_prefix ? core::ExtendedPOS::Prefix : core::ExtendedPOS::Unknown;
          auto cand = makeCandidate(surface, start_pos, lead, lead_pos, candidate::kLeadingNounCounterSplitBonus, false,
                                    CandidateOrigin::Counter, lead_epos);
          cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
          cand.pattern = "leading_noun_counter_split";
#endif
          candidates.push_back(cand);
        }
      }
    }
  }

  // Approximate-quantity prefix + numeral run + counter: 数/何 modify the numeral
  // run they head (数十件, 何十回, 数百万円) and belong inside the quantity token.
  // The merged numeral+counter candidate below (十件) otherwise undercuts the whole
  // kanji_seq token (数十件) and strands the prefix (数|十件), so the same discounted
  // merge is emitted extended over the prefix. Gates mirror the plain merge: a lone
  // counter kanji at a kanji→non-kanji boundary. The prefix must be directly
  // followed by a numeral, so a prefix bound straight to a counter (数日, 半年,
  // 何回), a prefix heading an ordinary noun (数値, 数学), and the reverse pattern
  // (十数年: 数 binds the following 年, not the preceding numeral) never fire.
  if (normalize::isQuantityPrefixKanji(codepoints[start_pos]) &&
      normalize::isNumeralCodepoint(codepoints[start_pos + 1])) {
    size_t num_end = start_pos + 1;
    while (num_end < codepoints.size() && normalize::isNumeralCodepoint(codepoints[num_end])) {
      ++num_end;
    }
    bool lone_counter_at_boundary =
        num_end < char_types.size() && normalize::isCounterKanji(codepoints[num_end]) &&
        (num_end + 1 >= char_types.size() || char_types[num_end + 1] != normalize::CharType::Kanji);
    if (lone_counter_at_boundary) {
      std::string surface = extractSubstring(codepoints, start_pos, num_end + 1);
      if (!surface.empty()) {
        auto cand = makeCandidate(surface, start_pos, num_end + 1, core::PartOfSpeech::Noun,
                                  candidate::kNumeralCounterMergeBonus, false, CandidateOrigin::Counter);
        cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
        cand.pattern = "quantity_prefix_counter_merge";
#endif
        candidates.push_back(cand);
      }
    }

    // An approximate quantity can also precede a kanji サ変 predicate
    // (数十件|確認する, 何十回|実施する). This parallels the ordinary
    // numeral+counter predicate boundary below, but retains the quantity
    // prefix inside the number phrase.
    if (num_end < char_types.size() && normalize::isCounterKanji(codepoints[num_end])) {
      if (hasKanjiSuruPredicateAt(codepoints, char_types, num_end + 1)) {
        std::string surface = extractSubstring(codepoints, start_pos, num_end + 1);
        if (!surface.empty()) {
          auto cand = makeCandidate(surface, start_pos, num_end + 1, core::PartOfSpeech::Noun,
                                    candidate::kCounterNounSplitBonus, false, CandidateOrigin::Counter,
                                    core::ExtendedPOS::NounNumber);
          cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
          cand.pattern = "quantity_prefix_counter_suru_predicate_split";
#endif
          candidates.push_back(cand);
        }
      }
    }
  }
}

}  // namespace suzume::analysis::counter_detail
