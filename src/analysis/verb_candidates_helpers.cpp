/**
 * @file verb_candidates_helpers.cpp
 * @brief Implementation of internal helpers for verb candidate generation
 */

#include <algorithm>
#include <utility>

#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/stable_insertion_sort.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"
#include "verb_candidates_absorption_guards.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_classical.h"
#include "verb_candidates_dictionary_probes.h"
#include "verb_candidates_emphatic.h"
#include "verb_candidates_verb_stems.h"

namespace suzume::analysis::verb_helpers {

bool startsInsideKanjiRunBeforeShi(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos == 0 || start_pos >= codepoints.size() || !normalize::isKanjiCodepoint(codepoints[start_pos - 1]) ||
      !normalize::isKanjiCodepoint(codepoints[start_pos])) {
    return false;
  }

  size_t kanji_end = start_pos + 1;
  while (kanji_end < codepoints.size() && normalize::isKanjiCodepoint(codepoints[kanji_end])) {
    ++kanji_end;
  }
  return kanji_end < codepoints.size() && codepoints[kanji_end] == U'し';
}

bool embedsTeFormAuxiliary(std::string_view surface) {
  static constexpr std::string_view kPatterns[] = {
      "ていく", "ていっ", "ていけ", "ていか",                // 〜ていく directional aspect
      "てもら", "てくれ", "てあげ", "てほしい", "てくださ",  // benefactive / request
      "てある", "である",                                    // completed-state existential
  };
  return utf8::containsAny(surface, kPatterns);
}

bool embedsTeFormMiruAuxiliary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (end_pos > codepoints.size()) {
    return false;
  }
  for (size_t pos = start_pos + 1; pos + 1 < end_pos; ++pos) {
    if ((codepoints[pos] == core::hiragana::kTe || codepoints[pos] == U'で') && codepoints[pos + 1] == U'み') {
      return true;
    }
  }
  return false;
}

bool masuAuxFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos + 1 >= codepoints.size() || codepoints[pos] != U'ま') {
    return false;
  }
  const char32_t next = codepoints[pos + 1];
  return next == U'す' || next == U'し' || next == U'せ';
}

size_t finiteMasuFormLengthAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos + 1 >= codepoints.size() || codepoints[pos] != U'ま') {
    return 0;
  }
  if (codepoints[pos + 1] == U'す') {
    if (pos + 3 < codepoints.size() && codepoints[pos + 2] == U'れ' && codepoints[pos + 3] == U'ば') {
      return 4;  // ますれば
    }
    return 2;  // ます
  }
  if (codepoints[pos + 1] == U'し') {
    if (pos + 2 < codepoints.size() && (codepoints[pos + 2] == U'た' || codepoints[pos + 2] == U'て')) {
      return 3;  // ました / まして
    }
    if (pos + 3 < codepoints.size() && codepoints[pos + 2] == U'ょ' && codepoints[pos + 3] == U'う') {
      return 4;  // ましょう
    }
  }
  if (pos + 2 < codepoints.size() && codepoints[pos + 1] == U'せ' && codepoints[pos + 2] == U'ん') {
    return 3;  // ません
  }
  return 0;
}

bool causativeSaseFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos + 1 < codepoints.size() && codepoints[pos] == U'さ' && codepoints[pos + 1] == U'せ';
}

bool isSuruAuxiliaryStarter(char32_t next_char) {
  return next_char == U'ち' || next_char == U'て' || next_char == U'た' || next_char == U'な' || next_char == U'ま' ||
         next_char == U'よ' || next_char == U'ろ' || next_char == U'そ' || next_char == U'と' || next_char == U'か' ||
         next_char == U'つ';
}

size_t naiNegativeFormLengthAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos + 1 >= codepoints.size() || codepoints[pos] != U'な') {
    return 0;
  }
  const char32_t second = codepoints[pos + 1];
  if (second == U'い') {
    return 2;
  }
  if (second == U'く') {
    return pos + 2 < codepoints.size() && codepoints[pos + 2] == U'て' ? 3 : 2;
  }
  if (pos + 2 >= codepoints.size()) {
    return 0;
  }
  const char32_t third = codepoints[pos + 2];
  if (second == U'か' && third == U'っ') {
    return pos + 3 < codepoints.size() && codepoints[pos + 3] == U'た' ? 4 : 3;
  }
  if (second == U'け' && third == U'れ') {
    return pos + 3 < codepoints.size() && codepoints[pos + 3] == U'ば' ? 4 : 3;
  }
  if (second == U'け' && third == U'り') {
    return pos + 3 < codepoints.size() && codepoints[pos + 3] == U'ゃ' ? 4 : 3;
  }
  return second == U'き' && third == U'ゃ' ? 3 : 0;
}

bool naiNegativeFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  return naiNegativeFormLengthAt(codepoints, pos) != 0;
}

bool colloquialNegativeFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos + 1 < codepoints.size() && codepoints[pos] == U'ね' &&
         (codepoints[pos + 1] == U'え' || codepoints[pos + 1] == U'ぇ' || codepoints[pos + 1] == U'ー');
}

bool contractedTeContinuationFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos < codepoints.size() &&
         (codepoints[pos] == U'る' || codepoints[pos] == U'た' || codepoints[pos] == U'ち' || codepoints[pos] == U'な');
}

bool particleClosesClauseBeforeSokuon(const std::vector<char32_t>& codepoints, size_t sokuon_pos) {
  if (sokuon_pos < 2 || sokuon_pos >= codepoints.size()) {
    return false;
  }
  const char32_t particle = codepoints[sokuon_pos - 1];
  const char32_t predicate_end = codepoints[sokuon_pos - 2];
  const bool closing_particle = particle == U'よ' || particle == U'の' || particle == U'か' || particle == U'ね' ||
                                particle == U'わ' || particle == U'ぞ';
  return closing_particle &&
         (grammar::isModernGodanTerminalKana(predicate_end) || kana::isERowCodepoint(predicate_end) ||
          predicate_end == U'ろ' || predicate_end == U'い' || predicate_end == U'な' || predicate_end == U'だ');
}

bool volitionalEndingFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos < codepoints.size() &&
         (codepoints[pos] == U'う' ||
          (codepoints[pos] == U'っ' && pos + 1 < codepoints.size() && codepoints[pos + 1] == U'か'));
}

bool startsInsideKanjiRun(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos > 0 && pos < codepoints.size() && kana::isKanjiCodepoint(codepoints[pos]) &&
         kana::isKanjiCodepoint(codepoints[pos - 1]);
}

bool crossesCaseParticleBeforePredicate(const dictionary::DictionaryManager* dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos > codepoints.size()) {
    return false;
  }
  // A one-mora predicate behind the particle is no evidence: every godan-sa
  // stem ends in one (ながす, さがす), and the classical す is a dictionary verb.
  for (size_t pos = start_pos + 1; pos + 3 <= end_pos; ++pos) {
    const auto* particle = lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Particle);
    if (particle == nullptr || particle->extended_pos != core::ExtendedPOS::ParticleCase) {
      continue;
    }
    if (hasExactPartOfSpeech(*dict_manager, codepoints, pos + 1, end_pos, kVerbAdjectiveMask)) {
      return true;
    }
  }
  return false;
}

bool splitsDictionaryKanjiWord(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos > codepoints.size() || !startsInsideKanjiRun(codepoints, pos)) {
    return false;
  }
  size_t run_start = pos;
  while (run_start > 0 && kana::isKanjiCodepoint(codepoints[run_start - 1])) {
    --run_start;
  }
  const std::string run = extractSubstring(codepoints, run_start, end_pos);
  if (isNounInDictionary(dict_manager, run)) {
    return true;
  }
  // A two-kanji run with no boundary of its own reads as one Sino-Japanese
  // noun by default, which is what a denominal 〜る verb is built on (皮肉る,
  // 牛耳る). A longer run has room for a noun plus a separate predicate
  // (複数+残った), so it keeps the split available.
  return end_pos - run_start == 2;
}

bool lexicalWordFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos < codepoints.size() && kana::isKanjiCodepoint(codepoints[pos]);
}

bool crossesKkoNominalizer(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (start_pos >= end_pos || end_pos > codepoints.size()) {
    return false;
  }
  for (size_t pos = start_pos + 1; pos < end_pos; ++pos) {
    if (codepoints[pos] == U'っ' && pos + 1 < codepoints.size() && codepoints[pos + 1] == U'こ' &&
        naiNegativeFollowsAt(codepoints, pos + 2)) {
      return true;
    }
  }
  return false;
}

bool startsInsideGaMashiiSuffix(const std::vector<char32_t>& codepoints, size_t pos) {
  // The suffix is が+ま+し followed by an i-adjective cell, so a candidate opens
  // inside it when any of those three morae is at most two positions back.
  constexpr size_t kSuffixStemLength = 3;
  for (size_t offset = 0; offset < kSuffixStemLength; ++offset) {
    if (offset > pos) {
      break;
    }
    const size_t suffix_start = pos - offset;
    if (suffix_start + kSuffixStemLength >= codepoints.size() || codepoints[suffix_start] != U'が' ||
        codepoints[suffix_start + 1] != U'ま' || codepoints[suffix_start + 2] != U'し') {
      continue;
    }
    // Only an adjective cell proves the suffix. The nominal まし takes the
    // copula and case particles instead (こちらの方がましだ), and that が is the
    // ordinary subject marker.
    const char32_t cell = codepoints[suffix_start + kSuffixStemLength];
    if (cell == U'い' || cell == U'く' || cell == U'さ' || cell == U'か' || cell == U'け') {
      return true;
    }
  }
  return false;
}

bool clauseEndsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return true;
  }
  const char32_t following = codepoints[pos];
  return following == U'。' || following == U'、' || following == U'！' || following == U'？' || following == U'」' ||
         following == U'）';
}

// The function words that follow a classical cell are at most three kana long
// (ども, ばや), so a probe of that width reaches every one of them.
constexpr size_t kFollowerProbeChars = 3;

bool dictionaryTailFollowsAt(const std::vector<char32_t>& codepoints, size_t pos,
                             const dictionary::DictionaryManager* dict_manager, core::PartOfSpeech pos_class,
                             std::initializer_list<core::ExtendedPOS> accepted) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  const size_t probe_end = std::min(codepoints.size(), pos + kFollowerProbeChars);
  for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, pos, probe_end)) {
    if (match.entry == nullptr || match.entry->pos != pos_class) {
      continue;
    }
    for (const core::ExtendedPOS candidate_pos : accepted) {
      if (match.entry->extended_pos == candidate_pos) {
        return true;
      }
    }
  }
  return false;
}

bool shuushikeiEndsAt(const std::vector<char32_t>& codepoints, size_t pos,
                      const dictionary::DictionaryManager* dict_manager) {
  return clauseEndsAt(codepoints, pos) ||
         dictionaryTailFollowsAt(
             codepoints, pos, dict_manager, core::PartOfSpeech::Auxiliary,
             {core::ExtendedPOS::AuxClassicalBeshi, core::ExtendedPOS::AuxNegativeMai, core::ExtendedPOS::AuxVolitional,
              core::ExtendedPOS::AuxClassicalNari, core::ExtendedPOS::AuxClassicalConjectureTerminal});
}

bool isListedGodanSaContinuativeRun(const dictionary::DictionaryManager* dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  if (dict_manager == nullptr || end >= codepoints.size() || codepoints[end] != U'し' || end <= start) {
    return false;
  }
  return lookupEntryInRange(*dict_manager, codepoints, start, end, core::PartOfSpeech::Noun) == nullptr &&
         dict_manager->lookupExact(normalize::concat(extractSubstring(codepoints, start, end), "す"),
                                   core::PartOfSpeech::Verb) != nullptr;
}

bool caseParticleFollowsAt(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                           size_t pos) {
  return hasDictionaryEntryFrom(
      &dict_manager, codepoints, pos, 1, kFollowerProbeChars, core::PartOfSpeech::Particle,
      [](const dictionary::DictionaryEntry& entry) { return entry.extended_pos == core::ExtendedPOS::ParticleCase; });
}

bool hypotheticalParticleFollowsAt(const dictionary::DictionaryManager& dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t pos) {
  return hasDictionaryEntryFrom(&dict_manager, codepoints, pos, 1, kFollowerProbeChars, core::PartOfSpeech::Particle,
                                [](const dictionary::DictionaryEntry& entry) {
                                  return entry.extended_pos == core::ExtendedPOS::ParticleConj &&
                                         grammar::isHypotheticalSelectingConjunctiveParticle(entry.surface);
                                });
}

bool classicalPastEnvironmentFollows(const dictionary::DictionaryManager& dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t end_pos, bool is_izenkei) {
  if (is_izenkei) {
    return hasDictionaryEntryFrom(
        &dict_manager, codepoints, end_pos, 1, kFollowerProbeChars, core::PartOfSpeech::Particle,
        [](const dictionary::DictionaryEntry& entry) { return entry.extended_pos == core::ExtendedPOS::ParticleConj; });
  }
  if (clauseEndsAt(codepoints, end_pos)) {
    return true;
  }
  const char32_t following = codepoints[end_pos];
  if (normalize::isKanjiCodepoint(following) || normalize::classifyChar(following) == normalize::CharType::Katakana) {
    return true;
  }
  return hasDictionaryEntryFrom(&dict_manager, codepoints, end_pos, 1, kFollowerProbeChars, core::PartOfSpeech::Noun,
                                nullptr);
}

bool literaryPastAuxiliaryFollowsAt(const dictionary::DictionaryManager& dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t pos) {
  return auxiliaryFollowsAt(&dict_manager, codepoints, pos, [](const dictionary::DictionaryEntry& entry) {
    return entry.extended_pos == core::ExtendedPOS::AuxClassicalKeri ||
           entry.extended_pos == core::ExtendedPOS::AuxClassicalKi;
  });
}

bool naiConditionalFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos + 2 < codepoints.size() && codepoints[pos] == U'な' && codepoints[pos + 1] == U'け' &&
         codepoints[pos + 2] == U'れ';
}

bool itadakuParadigmStartsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos + 3 >= codepoints.size() || codepoints[pos] != U'い' || codepoints[pos + 1] != U'た' ||
      codepoints[pos + 2] != U'だ') {
    return false;
  }
  const char32_t inflected = codepoints[pos + 3];
  return inflected == U'か' || inflected == U'き' || inflected == U'く' || inflected == U'け' || inflected == U'こ' ||
         inflected == U'い';
}

bool hasInternalVerbChainBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                  const grammar::Inflection& inflection,
                                  const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 3) {
    return false;
  }
  auto is_verified_verb_form = [&](size_t form_start, size_t form_end) {
    const std::string form = extractSubstring(codepoints, form_start, form_end);
    if (hasDictionaryEntry(dict_manager, form, core::PartOfSpeech::Verb) ||
        hasDictionaryEntry(dict_manager, form, core::PartOfSpeech::Auxiliary)) {
      return true;
    }
    if (form_end > form_start + 1 && kana::isOnbinCodepoint(codepoints[form_end - 1])) {
      const std::string stem = extractSubstring(codepoints, form_start, form_end - 1);
      const std::string onbin = extractSubstring(codepoints, form_end - 1, form_end);
      if (firstGodanOnbinDictBase(dict_manager, stem, onbin).matched) {
        return true;
      }
    }
    for (const auto& candidate : inflection.analyze(form)) {
      if (candidate.verb_type != grammar::VerbType::IAdjective &&
          (isVerbInDictionary(dict_manager, candidate.base_form) ||
           hasDictionaryEntry(dict_manager, candidate.base_form, core::PartOfSpeech::Auxiliary))) {
        return true;
      }
    }
    if (form_end > form_start) {
      const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(codepoints[form_end - 1]);
      if (!base_suffix.empty()) {
        const std::string stem = extractSubstring(codepoints, form_start, form_end - 1);
        if (isVerbInDictionary(dict_manager, normalize::concat(stem, base_suffix))) {
          return true;
        }
      }
    }
    return false;
  };

  for (size_t connective_pos = start_pos + 2; connective_pos + 2 < end_pos; ++connective_pos) {
    if (codepoints[connective_pos] != U'て' && codepoints[connective_pos] != U'で') {
      continue;
    }
    if (is_verified_verb_form(start_pos, connective_pos) && is_verified_verb_form(connective_pos + 1, end_pos)) {
      return true;
    }
  }
  for (size_t negative_pos = start_pos + 2; negative_pos + 1 < end_pos; ++negative_pos) {
    if (codepoints[negative_pos] == U'ず' && is_verified_verb_form(start_pos, negative_pos) &&
        is_verified_verb_form(negative_pos + 1, end_pos)) {
      return true;
    }
  }
  return false;
}

// =============================================================================
// Single-kanji Ichidan verbs
// =============================================================================

namespace {
constexpr char32_t kSingleKanjiIchidanList[] = {U'見', U'居', U'着', U'寝', U'煮', U'似',
                                                U'経', U'干', U'射', U'得', U'出', U'鋳'};
}  // namespace

bool isSingleKanjiIchidan(char32_t c) {
  for (char32_t k : kSingleKanjiIchidanList) {
    if (c == k)
      return true;
  }
  return false;
}

bool closesIchidanIrrealis(const grammar::Inflection& inflection, const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos == 0) {
    return false;
  }
  const char32_t last = codepoints[pos - 1];
  if (normalize::isKanjiCodepoint(last)) {
    return isSingleKanjiPoliteStem(last);
  }
  if ((!kana::isERowCodepoint(last) && !kana::isIRowCodepoint(last)) || pos < 2 ||
      !normalize::isKanjiCodepoint(codepoints[pos - 2])) {
    return false;
  }
  const std::string base_form = extractSubstring(codepoints, pos - 2, pos) + "る";
  return readsAsBaseForm(inflection, base_form, base_form, grammar::VerbType::Ichidan);
}

bool isSingleKanjiPoliteStem(char32_t c) {
  return isSingleKanjiIchidan(c) || grammar::isKuruKanjiStem(c);
}

bool isSingleKanjiIchidanSurface(std::string_view surface) {
  if (normalize::utf8Length(surface) != 1) {
    return false;
  }
  return isSingleKanjiIchidan(utf8::decodeFirstChar(surface));
}

bool isVerbContinuativeSpan(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                            size_t start, size_t end) {
  if (end == start + 1 && isSingleKanjiIchidan(codepoints[start])) {
    return true;
  }
  const auto* verb = dict_manager == nullptr
                         ? nullptr
                         : lookupEntryInRange(*dict_manager, codepoints, start, end, core::PartOfSpeech::Verb);
  return verb != nullptr && verb->extended_pos == core::ExtendedPOS::VerbRenyokei;
}

bool startsWithVerbContinuative(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  for (size_t boundary = start + 1; boundary < end; ++boundary) {
    if (isVerbContinuativeSpan(dict_manager, codepoints, start, boundary)) {
      return true;
    }
  }
  return false;
}

// =============================================================================
// Candidate Sorting
// =============================================================================

void sortCandidatesByCost(std::vector<UnknownCandidate>& candidates, size_t first_index) {
  core::stableInsertionSort(candidates, first_index, [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
    return lhs.cost < rhs.cost;
  });
}

// =============================================================================
// Pattern Skip Helpers
// =============================================================================

bool shouldSkipMasuAuxPattern(std::string_view surface, grammar::VerbType verb_type) {
  if (!utf8::endsWithAny(surface, {"ましょう", "ました", "ません", "ます"})) {
    return false;
  }
  // Suru-verb passive/causative chains (され, させ) are kept whole
  return verb_type != grammar::VerbType::Suru || !utf8::containsAny(surface, {"され", "させ"});
}

bool shouldSkipSouPattern(std::string_view surface, grammar::VerbType verb_type) {
  return verb_type != grammar::VerbType::IAdjective &&
         utf8::endsWithAny(surface, {"そうです", "そうだ", scorer::kSuffixSou});
}

bool isCompoundAdjectivePattern(std::string_view surface) {
  if (surface.size() < core::kFourJapaneseCharBytes) {
    return false;
  }
  // Check for auxiliary adjective patterns in various conjugation forms
  if (utf8::containsAny(surface, {
                                     "にくい", "にくく", "にくか", "にくけ", "にくさ",  // difficult to do
                                     "やすい", "やすく", "やすか", "やすけ", "やすさ",  // easy to do
                                     "がたい", "がたく", "がたか", "がたけ", "がたさ",  // hard to do
                                     "難い",   "難く",   "難か",   "難け",   "難さ"     // kanji spelling
                                 })) {
    return true;
  }
  // Also check stem forms at end of surface (e.g., 使いにく for 使いにく+い split)
  return utf8::endsWithAny(surface, {"にく", "やす", "がた"});
}

bool containsKuNaruPattern(std::string_view surface) {
  return utf8::containsAny(surface, {"くなっ", "くなり", "くなる", "くなれ", "くなら"});
}

bool isReduplicatedShiiAdjectiveHead(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos + 5 >= codepoints.size()) {
    return false;
  }
  // Doubled two-character unit XYXY, compared by codepoint so the same rule
  // serves kanji and both kana scripts.
  if (codepoints[start_pos] != codepoints[start_pos + 2] || codepoints[start_pos + 1] != codepoints[start_pos + 3]) {
    return false;
  }
  if (codepoints[start_pos + 4] != U'し') {
    return false;
  }
  // い/く/か/け start the i-adjective inflection endings after し:
  // しい, しく(ない/て), しかっ(た)/しかろ(う), しけれ(ば).
  const char32_t onset = codepoints[start_pos + 5];
  return onset == U'い' || onset == U'く' || onset == U'か' || onset == U'け';
}

grammar::GodanOnbinRange getGodanTypesByOnbin(std::string_view onbin) {
  return grammar::Conjugation::getGodanTypesByOnbin(onbin);
}

GodanOnbinDictMatch firstGodanOnbinDictBase(const dictionary::DictionaryManager* dict_manager, std::string_view stem,
                                            std::string_view onbin) {
  for (const auto& [verb_type, base_suffix] : getGodanTypesByOnbin(onbin)) {
    std::string base_form = normalize::concat(stem, base_suffix);
    if (onbin == "っ" && !grammar::admitsSokuonbin(verb_type, base_form)) {
      continue;
    }
    if (isVerbInDictionary(dict_manager, base_form)) {
      return GodanOnbinDictMatch{verb_type, std::move(base_form), base_suffix, true};
    }
  }
  return GodanOnbinDictMatch{};
}

bool shouldSkipPassiveAuxPattern(std::string_view surface, grammar::VerbType verb_type) {
  // Skip patterns containing classical passive + べき
  if (utf8::endsWith(surface, "れべき")) {
    return true;
  }

  // A Godan irrealis (any a-row cell) or the する irrealis さ plus a complete
  // passive is a voice chain; Sahen predicates are search-tokenized as a
  // nominal stem plus さ+れる (勉強+さ+れる), so a unified 勉強される hides it.
  const bool is_suru = verb_type == grammar::VerbType::Suru;
  if (!is_suru && !grammar::isGodanVerbType(verb_type)) {
    return false;
  }
  const auto codepoints = normalize::toCodepoints(surface);
  for (size_t index = 1; index < codepoints.size(); ++index) {
    const char32_t irrealis = codepoints[index - 1];
    if ((is_suru ? irrealis == U'さ' : kana::isARowCodepoint(irrealis)) &&
        isCompletePassiveAuxiliaryAt(codepoints, index)) {
      return true;
    }
  }
  return false;
}

bool isPassiveAuxContinuation(const std::vector<char32_t>& codepoints, size_t pos_after_re, bool strict_masu) {
  if (pos_after_re >= codepoints.size()) {
    return false;
  }
  char32_t after_re = codepoints[pos_after_re];
  // れる, れた, れて
  if (after_re == U'る' || after_re == U'た' || after_re == U'て') {
    return true;
  }
  // れ + ない family (れない, れなかった, れなくて, れなければ, ...), and the
  // classical negative on the same irrealis (れず, れずに, れぬ)
  if (naiNegativeFollowsAt(codepoints, pos_after_re) || after_re == U'ず' || after_re == U'ぬ') {
    return true;
  }
  // れま (れます, れました); the strict form requires す/せ (excludes bare ま)
  if (after_re == U'ま') {
    if (!strict_masu) {
      return true;
    }
    return pos_after_re + 1 < codepoints.size() &&
           (codepoints[pos_after_re + 1] == U'す' || codepoints[pos_after_re + 1] == U'せ');
  }
  // れ + ば is the passive auxiliary's conditional cell.
  if (after_re == U'れ') {
    return pos_after_re + 1 < codepoints.size() && codepoints[pos_after_re + 1] == U'ば';
  }
  // A lexical word starting here continues the clause instead of the paradigm
  // (使わ+れ+続ける, 使わ+れ+方). The auxiliary is still the auxiliary, so the
  // callers that use this to confirm the voice reading need it to hold.
  return lexicalWordFollowsAt(codepoints, pos_after_re);
}

bool isCompletePassiveAuxiliaryAt(const std::vector<char32_t>& codepoints, size_t passive_re_pos) {
  if (passive_re_pos >= codepoints.size() || codepoints[passive_re_pos] != U'れ') {
    return false;
  }
  const size_t pos_after_re = passive_re_pos + 1;
  if (!isPassiveAuxContinuation(codepoints, pos_after_re, true)) {
    return false;
  }
  const char32_t after_re = codepoints[pos_after_re];
  if (after_re == U'る' || after_re == U'た' || after_re == U'て') {
    return pos_after_re + 1 == codepoints.size();
  }
  if (after_re == U'ま' || after_re == U'れ') {
    return pos_after_re + 2 == codepoints.size();
  }
  const size_t negative_length = naiNegativeFormLengthAt(codepoints, pos_after_re);
  return negative_length != 0 && pos_after_re + negative_length == codepoints.size();
}

bool isCompleteCausativeAuxiliaryAt(const std::vector<char32_t>& codepoints, size_t causative_se_pos) {
  if (causative_se_pos + 1 >= codepoints.size() || codepoints[causative_se_pos] != U'せ') {
    return false;
  }
  const char32_t after_se = codepoints[causative_se_pos + 1];
  if (after_se == U'る' || after_se == U'た' || after_se == U'て') {
    return causative_se_pos + 2 == codepoints.size();
  }
  return after_se == U'れ' && causative_se_pos + 2 < codepoints.size() && codepoints[causative_se_pos + 2] == U'ば' &&
         causative_se_pos + 3 == codepoints.size();
}

bool shouldSkipCausativeAuxPattern(std::string_view surface, grammar::VerbType verb_type) {
  // Suru verb causative/passive: stay as single tokens
  if (verb_type == grammar::VerbType::Suru) {
    return false;
  }

  // Godan causative: derive every closed auxiliary cell through the shared
  // continuation helper instead of a surface-form list.
  if (grammar::isGodanVerbType(verb_type)) {
    const auto codepoints = normalize::toCodepoints(surface);
    for (size_t index = 1; index < codepoints.size(); ++index) {
      if (kana::isARowCodepoint(codepoints[index - 1]) && isCompleteCausativeAuxiliaryAt(codepoints, index)) {
        return true;
      }
    }
    return false;
  }

  // An unverified Ichidan candidate ending in A-row + せ is the stem of a
  // Godan causative (読ま+せ, 書か+せ), not an independent verb. Dictionary
  // candidates remain available for lexicalized derivatives such as 泳がせる.
  if (verb_type == grammar::VerbType::Ichidan) {
    const auto codepoints = normalize::toCodepoints(surface);
    if (codepoints.size() >= 2 && kana::isARowCodepoint(codepoints[codepoints.size() - 2]) &&
        codepoints.back() == U'せ') {
      return true;
    }
  }

  // Causative-passive and passive-causative patterns for all verb types
  // (including Ichidan). These look like Ichidan verbs but contain a voice
  // auxiliary chain, so retain each auxiliary boundary.
  // E.g., 聞かせられた → 聞か + せ + られ + た;
  //       書かれさせる → 書か + れ + させる.
  return utf8::endsWithAny(surface, {"せられる", "せられた", "せられて", "せられない"}) ||
         containsPassiveCausativeAuxPattern(surface);
}

namespace {

// Check if a hiragana tail analyzes as a conjugation of する with an auxiliary
// chain (して, しました, してもらっている); bare し and plain する have none.
bool isSuruAuxChainTail(std::string_view tail, const grammar::Inflection& inflection) {
  // Empty-stem する conjugations start with し/す/せ; される/させる need the
  // mizenkei さ with a stem (whole-surface check in the caller covers them)
  if (!utf8::startsWithAny(tail, {"し", "す", "せ"})) {
    return false;
  }
  if (utf8::equalsAny(tail, {"しろ", "せよ"})) {  // Imperatives carry no auxiliary chain
    return true;
  }
  for (const auto& cand : inflection.analyze(tail)) {
    if (cand.verb_type == grammar::VerbType::Suru && cand.stem.empty() && !cand.morphemes.empty()) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool shouldSkipSuruVerbAuxPattern(std::string_view surface, size_t kanji_count, const grammar::Inflection& inflection) {
  // Only apply to patterns with 2+ kanji (typical サ変 noun stems: 勉強, 対応)
  if (kanji_count < 2) {
    return false;
  }
  // Scan codepoint suffixes of the hiragana tail after the kanji run for a
  // する-auxiliary chain (勉強して, 空回りして) — ends-with semantics
  size_t tail_start = normalize::charToByteOffset(surface, kanji_count);
  std::string_view tail = surface.substr(std::min(tail_start, surface.size()));
  // Plain する is itself the productive サ変 predicate following the nominal
  // stem.  Preserve that search boundary even without a further auxiliary;
  // one-kanji lexical verbs such as 愛する are excluded by kanji_count above.
  if (tail == "する") {
    return true;
  }
  for (size_t pos = 0; pos < tail.size(); normalize::decodeUtf8(tail, pos)) {
    if (isSuruAuxChainTail(tail.substr(pos), inflection)) {
      return true;
    }
  }
  // される/させる need the mizenkei さ with a stem: use a whole-surface サ変
  // parse whose conjugated part starts with さ (対応される, 実行させた)
  for (const auto& cand : inflection.analyze(surface)) {
    if (cand.verb_type == grammar::VerbType::Suru && !cand.morphemes.empty() && utf8::startsWith(cand.suffix, "さ")) {
      return true;
    }
  }
  return false;
}

// =============================================================================
// Verb Type / Stem Analysis Helpers
// =============================================================================

std::string baseFormSuffix(grammar::VerbType verb_type) {
  if (verb_type == grammar::VerbType::Ichidan) {
    return "る";
  }
  const auto* row = grammar::Conjugation::getGodanRow(verb_type);
  if (row == nullptr) {
    return "";
  }
  return normalize::encodeUtf8(row->base_vowel);
}

bool isValidIRowIchidanStem(std::string_view stem) {
  if (stem.size() < 2 * core::kJapaneseCharBytes) {
    return false;
  }
  std::string_view last_char(stem.data() + stem.size() - core::kJapaneseCharBytes, core::kJapaneseCharBytes);
  if (!grammar::endsWithIRow(last_char)) {
    return false;
  }
  std::string_view kanji_part(stem.data(), stem.size() - core::kJapaneseCharBytes);
  bool is_single_kanji_i = (kanji_part.size() == core::kJapaneseCharBytes && last_char == "い");
  return !is_single_kanji_i;
}

bool containsTeFormAuxPattern(std::string_view surface) {
  return utf8::containsAny(surface, scorer::kTeFormAuxPenaltyPatterns);
}

bool containsCausativeAuxPattern(std::string_view surface) {
  return utf8::containsAny(surface, scorer::kCausativeAuxPenaltyPatterns);
}

bool containsPassiveCausativeAuxPattern(std::string_view surface) {
  return utf8::containsAny(surface, {"れさせ", "られさせ"});
}

VerbClassBests bestByVerbClass(const std::vector<grammar::InflectionCandidate>& candidates) {
  // Value-initialize so every field (including each accumulator's confidence) starts
  // at zero; the loop then keeps the highest-confidence candidate per verb class.
  VerbClassBests bests{};
  for (const auto& cand : candidates) {
    if (cand.has_explanatory_suffix) {
      continue;
    }
    if (cand.verb_type == grammar::VerbType::Ichidan && cand.confidence > bests.ichidan.confidence) {
      bests.ichidan = cand;
    }
    if (cand.verb_type == grammar::VerbType::Suru && cand.confidence > bests.suru.confidence) {
      bests.suru = cand;
    }
    if (grammar::isGodanVerbType(cand.verb_type) && cand.confidence > bests.godan.confidence) {
      bests.godan = cand;
    }
  }
  return bests;
}

}  // namespace suzume::analysis::verb_helpers
