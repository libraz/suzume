/**
 * @file suffix_candidates_counter_structural.cpp
 * @brief Structural counter boundaries outside the basic numeral scan
 */

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include "candidate_constants.h"
#include "core/debug.h"
#include "dictionary_probe.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "suffix_candidates.h"
#include "suffix_candidates_counter_internal.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"

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

struct KanaDigitReading {
  std::u32string_view surface;
  unsigned value;
};

constexpr std::array<KanaDigitReading, 10> kKanaDigitReadings = {{{U"いち", 1},
                                                                  {U"に", 2},
                                                                  {U"さん", 3},
                                                                  {U"よん", 4},
                                                                  {U"ご", 5},
                                                                  {U"ろく", 6},
                                                                  {U"なな", 7},
                                                                  {U"しち", 7},
                                                                  {U"はち", 8},
                                                                  {U"きゅう", 9}}};
constexpr std::u32string_view kKanaTenReading = U"じゅう";
constexpr std::u32string_view kKanaOrdinalCounter = U"ばん";
constexpr std::array<std::u32string_view, 4> kFiniteKanaOrdinalCopulas = {U"だった", U"でした", U"です", U"だ"};
static_assert(kKanaOrdinalCounter.size() == candidate::kKanaOrdinalCounterLength);

bool codepointsMatchAt(const std::vector<char32_t>& codepoints, size_t pos, size_t limit, std::u32string_view surface) {
  if (pos > limit || surface.size() > limit - pos) {
    return false;
  }
  return std::equal(surface.begin(), surface.end(), codepoints.begin() + pos);
}

bool kanaDigitAt(const std::vector<char32_t>& codepoints, size_t pos, size_t limit, KanaDigitReading& reading) {
  size_t longest_length = 0;
  for (const KanaDigitReading& candidate : kKanaDigitReadings) {
    if (candidate.surface.size() > longest_length && codepointsMatchAt(codepoints, pos, limit, candidate.surface)) {
      reading = candidate;
      longest_length = candidate.surface.size();
    }
  }
  return longest_length != 0;
}

size_t kanaOrdinalNumberEndAt(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos >= codepoints.size()) {
    return 0;
  }

  const size_t probe_end = std::min(codepoints.size(), start_pos + candidate::kMaxKanaOrdinalNumberLength);
  if (codepointsMatchAt(codepoints, start_pos, probe_end, kKanaTenReading)) {
    size_t number_end = start_pos + kKanaTenReading.size();
    KanaDigitReading ones{};
    if (kanaDigitAt(codepoints, number_end, probe_end, ones)) {
      number_end += ones.surface.size();
    }
    return number_end;
  }

  KanaDigitReading tens_or_ones{};
  if (!kanaDigitAt(codepoints, start_pos, probe_end, tens_or_ones)) {
    return 0;
  }

  size_t number_end = start_pos + tens_or_ones.surface.size();
  if (tens_or_ones.value < 2 || !codepointsMatchAt(codepoints, number_end, probe_end, kKanaTenReading)) {
    return number_end;
  }

  number_end += kKanaTenReading.size();
  KanaDigitReading ones{};
  if (kanaDigitAt(codepoints, number_end, probe_end, ones)) {
    number_end += ones.surface.size();
  }
  return number_end;
}

bool hasSingleFinalParticleAt(const std::vector<char32_t>& codepoints, size_t pos,
                              const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  const auto matches = lookupResultsInRange(*dict_manager, codepoints, pos, pos + 1);
  return std::any_of(matches.begin(), matches.end(), [](const auto& match) {
    return match.length == 1 && match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Particle &&
           match.entry->extended_pos == core::ExtendedPOS::ParticleFinal;
  });
}

bool hasKanaOrdinalRightBoundary(const std::vector<char32_t>& codepoints, size_t ordinal_end,
                                 const dictionary::DictionaryManager* dict_manager) {
  if (verb_helpers::clauseEndsAt(codepoints, ordinal_end)) {
    return true;
  }

  for (const std::u32string_view copula : kFiniteKanaOrdinalCopulas) {
    if (!codepointsMatchAt(codepoints, ordinal_end, codepoints.size(), copula)) {
      continue;
    }
    const size_t copula_end = ordinal_end + copula.size();
    if (verb_helpers::clauseEndsAt(codepoints, copula_end) ||
        (hasSingleFinalParticleAt(codepoints, copula_end, dict_manager) &&
         verb_helpers::clauseEndsAt(codepoints, copula_end + 1))) {
      return true;
    }
  }
  return false;
}

bool isClosedKanaNumberCounter(const std::vector<char32_t>& codepoints, size_t number_start, size_t number_end,
                               size_t suffix_end) {
  static constexpr std::array<std::string_view, 3> kNumberReadings = {"いち", "ふた", "よん"};
  static constexpr std::array<std::string_view, 2> kCounterReadings = {"まい", "にん"};
  const std::string number = extractSubstring(codepoints, number_start, number_end);
  const std::string counter = extractSubstring(codepoints, number_end, suffix_end);
  return std::find(kNumberReadings.begin(), kNumberReadings.end(), number) != kNumberReadings.end() &&
         std::find(kCounterReadings.begin(), kCounterReadings.end(), counter) != kCounterReadings.end();
}

bool isHiraganaSpan(const std::vector<normalize::CharType>& char_types, size_t start, size_t end) {
  return start < end && end <= char_types.size() &&
         std::all_of(char_types.begin() + start, char_types.begin() + end,
                     [](normalize::CharType char_type) { return char_type == normalize::CharType::Hiragana; });
}
}  // namespace

}  // namespace suzume::analysis::counter_detail

namespace suzume::analysis {

size_t repeatedNumeralNounUnitEndAt(const std::vector<char32_t>& codepoints,
                                    const std::vector<normalize::CharType>& char_types, size_t start_pos) {
  if (start_pos >= codepoints.size() || start_pos >= char_types.size() ||
      !normalize::isNumeralCodepoint(codepoints[start_pos])) {
    return 0;
  }

  const size_t numeral_end = counter_detail::scanQuantityHead(codepoints, start_pos, false);
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
    const size_t numeral_end = counter_detail::scanQuantityHead(codepoints, start_pos, false);
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
  // and quantitative Suffix classes (いち+まい, よん+にん). Hiragana suffixes
  // are restricted to those readings; kanji counters such as 月 keep the
  // mixed-script path.
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
        if (isHiraganaSpan(char_types, number_end, suffix_end) &&
            !isClosedKanaNumberCounter(codepoints, start_pos, number_end, suffix_end)) {
          continue;
        }
        std::string surface = extractSubstring(codepoints, start_pos, suffix_end);
        auto cand = makeCandidate(surface, start_pos, suffix_end, core::PartOfSpeech::Noun,
                                  candidate::kKanaNumeralCounterMergeBonus, true, CandidateOrigin::Counter,
                                  core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
        SUZUME_DEBUG_CANDIDATE_PATTERN(cand, "kana_numeral_counter");
        candidates.push_back(cand);
      }
    }
  }

  // The ordinal counter ばん is a closed suffix for the productive readings
  // of 1–99. It only opens a noun candidate when the complete ordinal is
  // clause-final or followed by a finite copula, optionally closed by one
  // single-character final particle.
  if (char_types[start_pos] == normalize::CharType::Hiragana) {
    const size_t number_end = kanaOrdinalNumberEndAt(codepoints, start_pos);
    if (number_end > start_pos && codepointsMatchAt(codepoints, number_end, codepoints.size(), kKanaOrdinalCounter)) {
      const size_t ordinal_end = number_end + kKanaOrdinalCounter.size();
      if (hasKanaOrdinalRightBoundary(codepoints, ordinal_end, dict_manager)) {
        std::string surface = extractSubstring(codepoints, start_pos, ordinal_end);
        auto cand = makeCandidate(surface, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                                  candidate::kKanaNumeralCounterMergeBonus, true, CandidateOrigin::Counter,
                                  core::ExtendedPOS::NounNumber);
        cand.lemma = surface;
        SUZUME_DEBUG_CANDIDATE_PATTERN(cand, "kana_ordinal_counter");
        candidates.push_back(std::move(cand));
      }
    }
  }

  // Indefinite approximate duration: 数+か+temporal counter (数か月,
  // 数か年).  数 supplies the quantity and か is the counter linker, so the
  // three-character quantity remains one search unit even before a following
  // kanji noun such as 後. The interrogative quantity heads take any linked
  // counter the same way (何か月, 幾か国), but only where the counter closes
  // the kanji run: 何か is also the indefinite pronoun (何か+月曜日).
  const char32_t quantity_head = codepoints[start_pos];
  const bool interrogative_head = quantity_head == U'何' || quantity_head == U'幾';
  const bool interrogative_linked_counter =
      interrogative_head && start_pos + 2 < codepoints.size() &&
      normalize::isCounterLinker(codepoints[start_pos + 1]) &&
      normalize::isLinkedCounterKanji(codepoints[start_pos + 2]) &&
      (start_pos + 3 >= codepoints.size() || !normalize::isKanjiCodepoint(codepoints[start_pos + 3]));
  if (interrogative_linked_counter ||
      (start_pos + 2 < codepoints.size() && quantity_head == U'数' && codepoints[start_pos + 1] == U'か' &&
       normalize::isTemporalCounterKanji(codepoints[start_pos + 2]))) {
    appendCounterCandidate(codepoints, start_pos, start_pos + 3, core::PartOfSpeech::Noun,
                           candidate::kNumeralCounterMergeBonus, core::ExtendedPOS::NounNumber,
                           "indefinite_approximate_duration", candidates);
  }

  // An interrogative or approximate head followed by a counter forms one
  // quantity (何人, 幾人, 数人). Keep the same kanji-run boundary as numeral+counter
  // candidates so a following kanji noun is not absorbed (何人家族), and leave
  // the linked 何か月/幾か国 path above with its existing NounNumber metadata.
  const size_t interrogative_counter_start = start_pos + 1;
  const bool approximate_head = quantity_head == U'数' &&
                                (start_pos == 0 || char_types[start_pos - 1] != normalize::CharType::Kanji) &&
                                interrogative_counter_start < codepoints.size() &&
                                !normalize::isTemporalCounterKanji(codepoints[interrogative_counter_start]);
  if ((interrogative_head || approximate_head) && interrogative_counter_start < codepoints.size()) {
    const size_t counter_run_end =
        verb_helpers::counterKanjiRunEnd(dict_manager, codepoints, interrogative_counter_start, codepoints.size());
    if (counter_run_end > interrogative_counter_start &&
        normalize::isCounterKanji(codepoints[interrogative_counter_start]) &&
        (counter_run_end >= char_types.size() || char_types[counter_run_end] != normalize::CharType::Kanji)) {
      appendCounterCandidate(codepoints, start_pos, counter_run_end, core::PartOfSpeech::Noun,
                             candidate::kNumeralCounterMergeBonus, core::ExtendedPOS::Unknown,
                             "direct_quantity_kanji_counter", candidates);
    }
  }

  // Repeated numeral-counter units are distributive quantity expressions
  // (一人一人, 一日一日). Keep two identical units as one search unit rather
  // than allowing each discounted counter candidate to split the expression.
  if (normalize::isNumeralCodepoint(codepoints[start_pos])) {
    const size_t numeral_end = scanQuantityHead(codepoints, start_pos, false);
    if (numeral_end < codepoints.size() && normalize::isCounterKanji(codepoints[numeral_end])) {
      const size_t repeated_end = repeatedNumeralNounUnitEndAt(codepoints, char_types, start_pos);
      if (repeated_end != 0) {
        appendCounterCandidate(codepoints, start_pos, repeated_end, core::PartOfSpeech::Noun,
                               candidate::kNumeralCounterMergeBonus, core::ExtendedPOS::NounNumber,
                               "repeated_numeral_counter", candidates);
      }
    }
  }

  // A native つ counter repeated in its kana spelling is the same distributive
  // expression written across two scripts (一つひとつ). The kana member has to be
  // a registered closed-class quantity ending in the same counter, so an
  // ordinary kana run behind a quantity cannot enter the rule and neither can a
  // short kana numeral that also opens a common word (一つ|とおもう).
  if (dict_manager != nullptr && normalize::isNumeralCodepoint(codepoints[start_pos])) {
    const size_t numeral_end = scanQuantityHead(codepoints, start_pos, false);
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
        appendCounterCandidate(codepoints, start_pos, repeated_end, core::PartOfSpeech::Noun,
                               candidate::kMixedScriptRepeatedQuantityBonus, core::ExtendedPOS::NounNumber,
                               "mixed_script_repeated_quantity", candidates);
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
      appendCounterCandidate(codepoints, start_pos, repeated_noun_end, core::PartOfSpeech::Noun,
                             candidate::kRepeatedNumeralNounPredicateSplitBonus, core::ExtendedPOS::NounNumber,
                             "repeated_numeral_noun_predicate_split", candidates);
    }
  }

  // Ordinal compounds start with 第 followed by a numeral sequence. Counter
  // tails have dedicated structural boundaries, while lexicalized ordinal
  // nouns are supplied by the dictionary. This prevents an arbitrary one-kanji
  // noun following an ordinal from being fabricated as a compound.
  if (start_pos + 1 < codepoints.size() && codepoints[start_pos] == U'第' &&
      normalize::isNumeralCodepoint(codepoints[start_pos + 1])) {
    const size_t ordinal_end = scanQuantityHead(codepoints, start_pos + 1, false);
    if (ordinal_end < char_types.size() && char_types[ordinal_end] == normalize::CharType::Kanji) {
      const size_t tail_end = findCharRegionEnd(char_types, ordinal_end, char_types.size(), normalize::CharType::Kanji);
      size_t tail_len = tail_end - ordinal_end;
      if (tail_len == 1 && normalize::isCounterKanji(codepoints[ordinal_end])) {
        appendCounterCandidate(codepoints, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                               candidate::kOrdinalDigitCounterSplitBonus, core::ExtendedPOS::NounNumber,
                               "ordinal_digit_counter_prefix", candidates);
        appendCounterCandidate(codepoints, ordinal_end, tail_end, core::PartOfSpeech::Suffix,
                               candidate::kOrdinalDigitCounterSplitBonus, core::ExtendedPOS::Unknown,
                               "ordinal_digit_counter_suffix", candidates);
      } else if (tail_len == 2 && normalize::isCounterKanji(codepoints[ordinal_end]) &&
                 codepoints[ordinal_end + 1] == U'目') {
        appendCounterCandidate(codepoints, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                               candidate::kOrdinalDigitCounterSplitBonus, core::ExtendedPOS::NounNumber,
                               "ordinal_counter_ordinal_suffix_prefix", candidates);
        appendCounterCandidate(codepoints, ordinal_end, ordinal_end + 1, core::PartOfSpeech::Suffix,
                               candidate::kOrdinalDigitCounterSplitBonus, core::ExtendedPOS::Unknown,
                               "ordinal_counter_ordinal_suffix_counter", candidates);
        appendCounterCandidate(codepoints, ordinal_end + 1, tail_end, core::PartOfSpeech::Suffix,
                               candidate::kOrdinalDigitCounterSplitBonus, core::ExtendedPOS::Unknown,
                               "ordinal_counter_ordinal_suffix_tail", candidates);
      } else if (tail_len >= 2 && normalize::isCounterKanji(codepoints[ordinal_end]) &&
                 codepoints[ordinal_end] != U'次') {
        // A longer kanji tail behind the ordinal is a word of its own, and the
        // counter reading of its first kanji cuts inside it (第2|部門, not
        // 第2部|門). Only the ordinal boundary is emitted; the tail keeps the
        // ordinary kanji-run analysis. A tail that does not open with a counter
        // needs no boundary here, so a lexicalized ordinal noun (第三者, 第一
        // 印象) retains its whole-word path.
        appendCounterCandidate(codepoints, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                               candidate::kOrdinalDigitCounterSplitBonus, core::ExtendedPOS::NounNumber,
                               "ordinal_kanji_word_prefix", candidates);
      } else if (tail_len >= 2 && codepoints[ordinal_end] == U'次') {
        // The sequential 次 closes its ordinal; what follows is a word of its
        // own (第二+次+計画).
        appendCounterCandidate(codepoints, start_pos, ordinal_end, core::PartOfSpeech::Noun,
                               candidate::kOrdinalSequentialSplitBonus, core::ExtendedPOS::NounNumber,
                               "ordinal_sequential_prefix", candidates);
        appendCounterCandidate(codepoints, ordinal_end, ordinal_end + 1, core::PartOfSpeech::Suffix,
                               candidate::kOrdinalSequentialSplitBonus, core::ExtendedPOS::Unknown,
                               "ordinal_sequential_counter", candidates);
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
    size_t scan = scanQuantityHead(codepoints, start_pos, true);
    const bool has_quantity = scan > start_pos;
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
        appendCounterCandidate(codepoints, start_pos, counter_end, core::PartOfSpeech::Noun,
                               candidate::kCounterNounSplitBonus, core::ExtendedPOS::Unknown, "counter_object_split",
                               candidates);
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
      const size_t num_end = scanQuantityHead(codepoints, lead, false);
      bool counter_follows = num_end < codepoints.size() &&
                             (normalize::isCounterKanji(codepoints[num_end]) ||
                              (num_end < char_types.size() && char_types[num_end] == normalize::CharType::Katakana));
      // A multi-kanji run closed by an approximation prefix ends its noun before
      // the prefix, which modifies the quantity (年間|約|2000件).
      if (lead_len >= 3 && normalize::isNumericApproxPrefixKanji(codepoints[lead - 1])) {
        --lead;
      }
      if (counter_follows) {
        // An approximation prefix (約/計/総) is a Prefix modifying the quantity; a
        // multi-kanji leading run is an ordinary Noun.
        appendCounterCandidate(codepoints, start_pos, lead,
                               lead_is_prefix ? core::PartOfSpeech::Prefix : core::PartOfSpeech::Noun,
                               candidate::kLeadingNounCounterSplitBonus,
                               lead_is_prefix ? core::ExtendedPOS::Prefix : core::ExtendedPOS::Unknown,
                               "leading_noun_counter_split", candidates);
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
    const size_t num_end = scanQuantityHead(codepoints, start_pos + 1, false);
    bool lone_counter_at_boundary =
        num_end < char_types.size() && normalize::isCounterKanji(codepoints[num_end]) &&
        (num_end + 1 >= char_types.size() || char_types[num_end + 1] != normalize::CharType::Kanji);
    if (lone_counter_at_boundary) {
      appendCounterCandidate(codepoints, start_pos, num_end + 1, core::PartOfSpeech::Noun,
                             candidate::kNumeralCounterMergeBonus, core::ExtendedPOS::Unknown,
                             "quantity_prefix_counter_merge", candidates);
    }

    // An approximate quantity can also precede a kanji サ変 predicate
    // (数十件|確認する, 何十回|実施する). This parallels the ordinary
    // numeral+counter predicate boundary below, but retains the quantity
    // prefix inside the number phrase.
    if (num_end < char_types.size() && normalize::isCounterKanji(codepoints[num_end])) {
      if (hasKanjiSuruPredicateAt(codepoints, char_types, num_end + 1)) {
        appendCounterCandidate(codepoints, start_pos, num_end + 1, core::PartOfSpeech::Noun,
                               candidate::kCounterNounSplitBonus, core::ExtendedPOS::NounNumber,
                               "quantity_prefix_counter_suru_predicate_split", candidates);
      }
    }
  }
}

}  // namespace suzume::analysis::counter_detail
