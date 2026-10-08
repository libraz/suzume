/**
 * @file verb_candidates_helpers_dictionary.cpp
 * @brief Dictionary-backed verb candidate helpers
 */

#include <algorithm>
#include <array>

#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/tokenizer_utils.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "verb_candidates_absorption_guards.h"
#include "verb_candidates_classical.h"
#include "verb_candidates_dictionary_probes.h"
#include "verb_candidates_verb_stems.h"

namespace suzume::analysis::verb_helpers {

// =============================================================================
// Dictionary Lookup Helpers
// =============================================================================

bool isVerbInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view base_form) {
  return hasDictionaryEntry(dict_manager, base_form, core::PartOfSpeech::Verb);
}

bool isVerbBaseFormInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view base_form) {
  if (dict_manager == nullptr || base_form.empty()) {
    return false;
  }
  const auto matches = dict_manager->lookup(base_form, 0);
  return std::any_of(matches.begin(), matches.end(), [&](const auto& match) {
    const auto* entry = match.entry;
    return entry != nullptr && entry->pos == core::PartOfSpeech::Verb && entry->surface.size() == base_form.size() &&
           (entry->lemma.empty() || entry->lemma == base_form);
  });
}

bool isStrandedPotentialStem(const dictionary::DictionaryManager* dict_manager, std::string_view ichidan_base,
                             const std::vector<char32_t>& codepoints, size_t stem_end) {
  const bool kana_follows = stem_end < codepoints.size() && kana::isHiraganaCodepoint(codepoints[stem_end]);
  return !kana_follows && isVerbInDictionary(dict_manager, ichidan_base) &&
         !isVerbBaseFormInDictionary(dict_manager, ichidan_base);
}

bool isAdjectiveInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view base_form) {
  return hasDictionaryEntry(dict_manager, base_form, core::PartOfSpeech::Adjective);
}

bool opensNonCopularParticleAt(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t pos) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  constexpr size_t kMaxParticleLength = 3;
  const size_t probe_end = std::min(codepoints.size(), pos + kMaxParticleLength);
  for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, pos, probe_end)) {
    if (match.entry != nullptr && match.length > 1 && match.entry->pos == core::PartOfSpeech::Particle &&
        match.entry->extended_pos != core::ExtendedPOS::ParticleFinal &&
        lookupEntryInRange(*dict_manager, codepoints, pos, pos + match.length, core::PartOfSpeech::Auxiliary) ==
            nullptr) {
      return true;
    }
  }
  return false;
}

bool isIAdjectiveInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view base_form) {
  if (dict_manager == nullptr || base_form.empty()) {
    return false;
  }
  const auto* entry = dict_manager->lookupExact(base_form, core::PartOfSpeech::Adjective);
  return entry != nullptr && entry->extended_pos != core::ExtendedPOS::AdjNaAdj;
}

bool isVerbInDictionary(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start, size_t end) {
  return hasDictionaryEntry(dict_manager, extractSubstring(codepoints, start, end), core::PartOfSpeech::Verb);
}

bool hasDictionaryGodanBaseFromIRow(const dictionary::DictionaryManager* dict_manager, std::string_view stem,
                                    char32_t i_row_kana) {
  const std::string_view godan_ending = grammar::godanBaseSuffixFromIRow(i_row_kana);
  return !godan_ending.empty() && isVerbInDictionary(dict_manager, normalize::concat(stem, godan_ending));
}

bool isAdjectiveInDictionary(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                             size_t start, size_t end) {
  return hasDictionaryEntry(dict_manager, extractSubstring(codepoints, start, end), core::PartOfSpeech::Adjective);
}

bool namesDictionaryVerbContinuative(const dictionary::DictionaryManager* dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t stem_start, size_t okurigana_pos) {
  if (dict_manager == nullptr || stem_start >= okurigana_pos || okurigana_pos >= codepoints.size()) {
    return false;
  }
  const std::string stem = extractSubstring(codepoints, stem_start, okurigana_pos);
  const char32_t okurigana = codepoints[okurigana_pos];
  if (hasDictionaryGodanBaseFromIRow(dict_manager, stem, okurigana)) {
    return true;
  }
  return grammar::isMonogradeStemFinalKana(okurigana) &&
         isVerbInDictionary(dict_manager,
                            stem + normalize::encodeUtf8(okurigana) + normalize::encodeUtf8(core::hiragana::kRu));
}

bool absorbsRegisteredClosedCell(const dictionary::DictionaryManager* dict_manager,
                                 const grammar::Inflection& inflection, const std::vector<char32_t>& codepoints,
                                 size_t start_pos, size_t kanji_end, size_t end) {
  if (dict_manager == nullptr || start_pos >= kanji_end || end > codepoints.size()) {
    return false;
  }
  for (size_t tail = kanji_end; tail + 2 <= end; ++tail) {
    bool closed_cell = false;
    for (const auto& result : lookupResultsInRange(*dict_manager, codepoints, tail, end)) {
      closed_cell = closed_cell || (result.entry != nullptr && tail + result.length == end &&
                                    result.entry->pos == core::PartOfSpeech::Auxiliary &&
                                    (result.entry->extended_pos == core::ExtendedPOS::AuxTenseTa ||
                                     result.entry->extended_pos == core::ExtendedPOS::AuxClassicalTari ||
                                     result.entry->extended_pos == core::ExtendedPOS::AuxClassicalNari));
    }
    if (!closed_cell) {
      continue;
    }
    const bool nominal_host = tail == kanji_end && (kanji_end - start_pos >= 2 ||
                                                    lookupEntryInRange(*dict_manager, codepoints, start_pos, kanji_end,
                                                                       core::PartOfSpeech::Pronoun) != nullptr);
    const std::string_view godan_ending = grammar::godanBaseSuffixFromIRow(codepoints[kanji_end]);
    const bool continuative_host =
        tail == kanji_end + 1 &&
        (namesDictionaryVerbContinuative(dict_manager, codepoints, start_pos, kanji_end) ||
         (!godan_ending.empty() &&
          isVerifiedVerbBase(dict_manager, inflection,
                             normalize::concat(extractSubstring(codepoints, start_pos, kanji_end), godan_ending),
                             candidate::verb_cost::kConstructedVerbMinConfidence, true)));
    if (nominal_host || continuative_host) {
      return true;
    }
  }
  return false;
}

NominalBeforeSokuon nominalEndsBeforeSokuon(const dictionary::DictionaryManager* dict_manager,
                                            const std::vector<char32_t>& codepoints, size_t stem_start,
                                            size_t sokuon_pos) {
  if (dict_manager == nullptr || stem_start >= sokuon_pos || sokuon_pos > codepoints.size()) {
    return NominalBeforeSokuon::None;
  }
  constexpr size_t kMaxNominalChars = 4;
  NominalBeforeSokuon found = NominalBeforeSokuon::None;
  for (size_t nominal_end = stem_start + 1; nominal_end <= sokuon_pos; ++nominal_end) {
    const bool suffix_closes =
        nominal_end == sokuon_pos ||
        lookupEntryInRange(*dict_manager, codepoints, nominal_end, sokuon_pos, core::PartOfSpeech::Suffix) != nullptr;
    if (!suffix_closes) {
      continue;
    }
    const size_t earliest = lookbehindStart(nominal_end, kMaxNominalChars);
    for (size_t begin = earliest; begin <= stem_start; ++begin) {
      if (lookupEntryInRange(*dict_manager, codepoints, begin, nominal_end, core::PartOfSpeech::Pronoun) != nullptr) {
        return NominalBeforeSokuon::Pronoun;
      }
      if (lookupEntryInRange(*dict_manager, codepoints, begin, nominal_end, core::PartOfSpeech::Noun) != nullptr) {
        found = NominalBeforeSokuon::Noun;
      }
    }
  }
  return found;
}

bool isQuantityClosingSuffixAt(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t pos) {
  if (dict_manager == nullptr || pos >= codepoints.size() || !normalize::isKanjiCodepoint(codepoints[pos]) ||
      normalize::isTemporalRelationSuffixKanji(codepoints[pos]) ||
      lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Suffix) == nullptr) {
    return false;
  }
  const size_t next = pos + 1;
  if (next >= codepoints.size() || !normalize::isKanjiCodepoint(codepoints[next])) {
    return true;
  }
  // A following kanji with okurigana is a predicate stem (三割強+増える).
  return grammar::nominalKanjiRunEnd(codepoints, next) == next + 1 && grammar::mayBeOkuriganaAt(codepoints, next + 1);
}

bool opensWordAfterQuantity(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                            size_t pos) {
  return pos < codepoints.size() &&
         (normalize::isTemporalRelationSuffixKanji(codepoints[pos]) ||
          (dict_manager != nullptr &&
           lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Pronoun) != nullptr));
}

size_t counterKanjiRunEnd(const std::vector<char32_t>& codepoints, size_t pos, size_t limit) {
  size_t end = pos;
  while (end < limit && normalize::isCounterKanji(codepoints[end]) &&
         (end == pos || !normalize::isQuantityOpeningCounterKanji(codepoints[end]))) {
    ++end;
  }
  return end;
}

bool kanjiRunMayContinueAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                           size_t run_end) {
  if (run_end < codepoints.size() && normalize::isKanjiCodepoint(codepoints[run_end])) {
    return true;
  }
  if (!grammar::mayBeOkuriganaAt(codepoints, run_end)) {
    return false;
  }
  // A small kana after a kanji is always an onbin ending (残っ+て), whatever
  // particle it also spells (って).
  if (dict_manager == nullptr || run_end == 0 || kana::isSmallKanaCodepoint(codepoints[run_end])) {
    return true;
  }
  constexpr size_t kProbeLength = 6;
  const size_t probe_end = std::min(codepoints.size(), run_end + kProbeLength);
  bool particle_starts = false;
  for (const auto& match : dict_manager->lookup(extractSubstring(codepoints, run_end, probe_end), 0)) {
    particle_starts = particle_starts ||
                      (match.entry != nullptr && match.length >= 2 && match.entry->pos == core::PartOfSpeech::Particle);
  }
  if (!particle_starts) {
    return true;
  }
  for (const auto& match : dict_manager->lookup(extractSubstring(codepoints, run_end - 1, probe_end), 0)) {
    if (match.entry != nullptr && match.length >= 2 &&
        (match.entry->pos == core::PartOfSpeech::Verb || match.entry->pos == core::PartOfSpeech::Adjective)) {
      return true;
    }
  }
  return false;
}

bool isProductiveShiiAdjectiveTerminal(std::string_view surface, const grammar::Inflection& inflection) {
  // A productive formation needs a stem in front of the suffix: bare しい is
  // the classical しかり paradigm's own shape (しかるべく, しかれども), not an
  // adjective derived from anything.
  if (!utf8::endsWith(surface, "しい") || normalize::utf8Length(surface) < 3) {
    return false;
  }
  return readsAsBaseForm(inflection, surface, surface, grammar::VerbType::IAdjective, candidate::kCompoundAdjConfMin);
}

bool readsAsIAdjectiveTerminal(std::string_view surface, const grammar::Inflection& inflection) {
  return utf8::endsWith(surface, "い") && readsAsBaseForm(inflection, surface, surface, grammar::VerbType::IAdjective);
}

bool readsAsBaseForm(const grammar::Inflection& inflection, std::string_view surface, std::string_view base_form,
                     grammar::VerbType verb_type, float min_confidence) {
  const auto& analyses = inflection.analyze(surface);
  return std::any_of(analyses.begin(), analyses.end(), [&](const grammar::InflectionCandidate& analysis) {
    return analysis.verb_type == verb_type && analysis.base_form == base_form && analysis.confidence >= min_confidence;
  });
}

bool isNounInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view surface) {
  return hasDictionaryEntry(dict_manager, surface, core::PartOfSpeech::Noun);
}

bool attestsGodanRaIrrealis(const dictionary::DictionaryManager* dict_manager, std::string_view stem) {
  if (dict_manager == nullptr) {
    return false;
  }
  const std::string stem_text(stem);
  const auto* irrealis = dict_manager->lookupExact(stem_text + "ら", core::PartOfSpeech::Verb);
  return irrealis != nullptr && irrealis->lemma == stem_text + "る";
}

bool stemHasDictionaryVerbOnOtherRow(const dictionary::DictionaryManager* dict_manager, std::string_view stem,
                                     std::string_view base_form) {
  if (dict_manager == nullptr || stem.empty()) {
    return false;
  }
  const auto other_registered = [&](const std::string& candidate) {
    return candidate != base_form && isVerbInDictionary(dict_manager, candidate);
  };
  if (other_registered(normalize::concat(stem, "る"))) {
    return true;
  }
  for (const auto& [verb_type, row] : grammar::Conjugation::getGodanRows()) {
    if (other_registered(normalize::concat(stem, normalize::encodeUtf8(row.base_vowel)))) {
      return true;
    }
  }
  return false;
}

bool endsOnRegisteredGodanIrrealis(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos < start_pos + 2 || end_pos > codepoints.size()) {
    return false;
  }
  const char32_t irrealis = codepoints[end_pos - 1];
  for (const auto& [verb_type, row] : grammar::Conjugation::getGodanRows()) {
    if (row.a_row != irrealis) {
      continue;
    }
    const std::string base_form =
        normalize::encodeRange(codepoints, start_pos, end_pos - 1) + normalize::encodeUtf8(row.base_vowel);
    if (isVerbInDictionary(dict_manager, base_form)) {
      return true;
    }
  }
  return false;
}

bool isNounOrAdjectiveInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view surface) {
  return hasDictionaryEntry(dict_manager, surface, core::PartOfSpeech::Noun) ||
         hasDictionaryEntry(dict_manager, surface, core::PartOfSpeech::Adjective);
}

bool hasNominalHostBefore(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos == 0 || start_pos > codepoints.size()) {
    return false;
  }
  const normalize::CharType host_end = normalize::classifyChar(codepoints[start_pos - 1]);
  return host_end == normalize::CharType::Kanji || host_end == normalize::CharType::Katakana;
}

bool isBoundSuffixAfterNominalHost(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t start_pos,
                                   std::string_view surface) {
  return hasNominalHostBefore(codepoints, start_pos) &&
         hasDictionaryEntry(dict_manager, surface, core::PartOfSpeech::Suffix);
}

bool hasDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface,
                        core::PartOfSpeech pos) {
  if (dict_manager == nullptr || surface.empty()) {
    return false;
  }
  const auto* entry = dict_manager->lookupExact(surface, pos);
  if (entry != nullptr) {
    SUZUME_DEBUG_LOG_TRACE("[DICT] \"" << surface << "\" (" << core::posToString(pos) << "/"
                                       << core::extendedPosToString(entry->extended_pos) << ") = FOUND\n");
    return true;
  }
  SUZUME_DEBUG_LOG_TRACE("[DICT] \"" << surface << "\" (" << core::posToString(pos) << ") = NOT_FOUND\n");
  return false;
}

bool hasNonVerbDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface) {
  if (dict_manager == nullptr) {
    return false;
  }
  auto results = dict_manager->lookup(surface, 0);
  for (const auto& result : results) {
    if (result.entry != nullptr && result.entry->surface == surface && result.entry->pos != core::PartOfSpeech::Verb) {
      return true;
    }
  }
  return false;
}

bool hasParticleDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface) {
  return dict_manager != nullptr && dict_manager->lookupExact(surface, core::PartOfSpeech::Particle) != nullptr;
}

bool hasCaseParticleDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface) {
  if (dict_manager == nullptr) {
    return false;
  }
  const auto* entry = dict_manager->lookupExact(surface, core::PartOfSpeech::Particle);
  return entry != nullptr && entry->extended_pos == core::ExtendedPOS::ParticleCase;
}

bool oneMoraParticleEndsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                           size_t pos, core::ExtendedPOS particle_pos) {
  if (dict_manager == nullptr || pos == 0) {
    return false;
  }
  const auto* particle = lookupEntryInRange(*dict_manager, codepoints, pos - 1, pos, core::PartOfSpeech::Particle);
  return particle != nullptr && particle->extended_pos == particle_pos;
}

bool followsCaseParticle(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                         size_t pos) {
  if (dict_manager == nullptr || pos == 0) {
    return false;
  }
  constexpr size_t kMaxParticleChars = 4;
  const size_t min_particle_start = lookbehindStart(pos, kMaxParticleChars);
  for (size_t particle_start = pos; particle_start > min_particle_start;) {
    --particle_start;
    if (hasCaseParticleDictionaryEntry(dict_manager, extractSubstring(codepoints, particle_start, pos))) {
      return true;
    }
  }
  return false;
}

bool followsCaseMarkedArgument(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t pos) {
  // A focus particle stacks on top of the case marking without changing the
  // argument structure, and more than one may stack (半数に+も, 東京に+は+も…).
  constexpr size_t kMaxStackedFocusParticles = 2;
  size_t argument_end = pos;
  for (size_t stacked = 0; stacked <= kMaxStackedFocusParticles; ++stacked) {
    if (followsCaseParticle(dict_manager, codepoints, argument_end)) {
      return true;
    }
    if (dict_manager == nullptr || argument_end == 0) {
      return false;
    }
    const auto* particle =
        lookupEntryInRange(*dict_manager, codepoints, argument_end - 1, argument_end, core::PartOfSpeech::Particle);
    if (particle == nullptr || (particle->extended_pos != core::ExtendedPOS::ParticleBinding &&
                                particle->extended_pos != core::ExtendedPOS::ParticleTopic &&
                                particle->extended_pos != core::ExtendedPOS::ParticleAdverbial)) {
      return false;
    }
    --argument_end;
  }
  return false;
}

bool isCommaClauseChainingRenyokei(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                   const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0 || end_pos >= codepoints.size() || codepoints[end_pos] != U'、') {
    return false;
  }
  const std::string particle_surface = extractSubstring(codepoints, start_pos - 1, start_pos);
  const auto* particle = dict_manager->lookupExact(particle_surface, core::PartOfSpeech::Particle);
  // A topic marker closes its argument just as a case particle does (彼は+微睡み、).
  const bool follows_argument = particle != nullptr && particle_surface != "と" && particle_surface != "で" &&
                                (particle->extended_pos == core::ExtendedPOS::ParticleCase ||
                                 particle->extended_pos == core::ExtendedPOS::ParticleTopic);
  if (follows_argument) {
    return true;
  }

  // A quantified focus phrase also supplies a predicate boundary
  // (何度も+試み、). Restrict this to the closed counter property so a
  // noun in an enumerated …も、 sequence does not become verbal evidence.
  return start_pos >= 2 && codepoints[start_pos - 1] == U'も' && normalize::isCounterKanji(codepoints[start_pos - 2]);
}

namespace {

// True when a @p pos entry taken by @p accept opens within @p lookback before
// start_pos and runs past it, ending at most @p probe after it.
bool startsInsideDictionaryEntry(const std::vector<char32_t>& codepoints, size_t start_pos,
                                 const dictionary::DictionaryManager* dict_manager, size_t lookback, size_t probe,
                                 core::PartOfSpeech pos, EntryAccept accept) {
  if (dict_manager == nullptr || start_pos == 0) {
    return false;
  }
  const size_t first_start = lookbehindStart(start_pos, lookback);
  const size_t probe_end = std::min(codepoints.size(), start_pos + probe);
  for (size_t entry_start = first_start; entry_start < start_pos; ++entry_start) {
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, entry_start, probe_end)) {
      if (match.entry != nullptr && match.entry->pos == pos && (accept == nullptr || accept(*match.entry)) &&
          entry_start + normalize::utf8Length(match.entry->surface) > start_pos) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

bool startsInsideDictionaryParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kParticleLookback = 4;
  constexpr size_t kParticleProbe = 5;
  return startsInsideDictionaryEntry(codepoints, start_pos, dict_manager, kParticleLookback, kParticleProbe,
                                     core::PartOfSpeech::Particle, nullptr);
}

bool startsInsideDictionaryAuxiliary(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kAuxiliaryLookback = 4;
  constexpr size_t kAuxiliaryProbe = 5;
  // Only the terminal polite copula itself owns this interior.  Its
  // inflected dictionary cells (でし/でしたら) can occur across an ordinary
  // conjunctive-particle boundary, as in 読んでしまう.
  return startsInsideDictionaryEntry(
      codepoints, start_pos, dict_manager, kAuxiliaryLookback, kAuxiliaryProbe, core::PartOfSpeech::Auxiliary,
      [](const dictionary::DictionaryEntry& auxiliary) { return auxiliary.surface.compare("です") == 0; });
}

bool startsInsideDictionaryIAdjective(const std::vector<char32_t>& codepoints, size_t start_pos,
                                      const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kAdjectiveLookback = 5;
  constexpr size_t kAdjectiveProbe = 2;
  // Only the uninflected terminal form owns its interior. An inflected cell
  // shares its stem with the te-form and the conditional, whose kana do open
  // a following predicate (寒く+なる).
  return startsInsideDictionaryEntry(
      codepoints, start_pos, dict_manager, kAdjectiveLookback, kAdjectiveProbe, core::PartOfSpeech::Adjective,
      [](const dictionary::DictionaryEntry& adjective) { return utf8::endsWith(adjective.surface, "い"); });
}

bool startsWithMultiMoraDictionaryParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kMinimumParticleLength = 2;
  constexpr size_t kParticleProbe = 4;
  return hasDictionaryEntryFrom(dict_manager, codepoints, start_pos, kMinimumParticleLength, kParticleProbe,
                                core::PartOfSpeech::Unknown, [](const dictionary::DictionaryEntry& entry) {
                                  return entry.extended_pos == core::ExtendedPOS::ParticleBinding;
                                });
}

bool embedsAuxiliaryOnOnbinStem(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos > codepoints.size()) {
    return false;
  }
  for (size_t aux_start = start_pos + 1; aux_start < end_pos; ++aux_start) {
    const char32_t onbin = codepoints[aux_start - 1];
    if (onbin != U'い' && onbin != U'ん' && onbin != U'っ') {
      continue;
    }
    if (hasDictionaryEntryFrom(dict_manager, codepoints, aux_start, 1, end_pos - aux_start,
                               core::PartOfSpeech::Auxiliary, nullptr)) {
      return true;
    }
  }
  return false;
}

bool embedsTeFormVerbCell(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                          size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr) {
    return false;
  }
  for (size_t te_pos = start_pos + 1; te_pos + 1 < end_pos; ++te_pos) {
    if (codepoints[te_pos] == core::hiragana::kTe &&
        lookupEntryInRange(*dict_manager, codepoints, te_pos + 1, end_pos, core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
  }
  return false;
}

bool auxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        size_t pos, EntryAccept accept) {
  constexpr size_t kAuxiliaryProbe = 3;
  return hasDictionaryEntryFrom(dict_manager, codepoints, pos, 1, kAuxiliaryProbe, core::PartOfSpeech::Auxiliary,
                                accept);
}

bool classicalAuxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t pos) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  constexpr size_t kAuxiliaryProbe = 3;
  const size_t max_end = std::min(codepoints.size(), pos + kAuxiliaryProbe);
  for (size_t aux_end = pos + 1; aux_end <= max_end; ++aux_end) {
    const auto* entry = lookupEntryInRange(*dict_manager, codepoints, pos, aux_end, core::PartOfSpeech::Auxiliary);
    if (entry == nullptr || !core::isClassicalAuxiliaryType(entry->extended_pos)) {
      continue;
    }
    // A final particle closes a clause after any word class, so a spelling that
    // can be one says nothing about what precedes it: the 已然形 ね of ぬ and the
    // 終助詞 ね are the same mora, and only the latter stands after a noun.
    const auto* particle = lookupEntryInRange(*dict_manager, codepoints, pos, aux_end, core::PartOfSpeech::Particle);
    if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleFinal) {
      continue;
    }
    return true;
  }
  return false;
}

bool predicateAuxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t pos) {
  constexpr size_t kAuxiliaryProbe = 4;
  return hasDictionaryEntryFrom(dict_manager, codepoints, pos, 1, kAuxiliaryProbe, core::PartOfSpeech::Auxiliary,
                                [](const dictionary::DictionaryEntry& entry) {
                                  return entry.extended_pos != core::ExtendedPOS::AuxCopulaDa &&
                                         entry.extended_pos != core::ExtendedPOS::AuxCopulaDesu;
                                });
}

KakariMusubi governingKakariMusubi(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t clause_pos) {
  struct KakariParticle {
    char32_t head;
    char32_t tail;  // U'\0' for a one-mora particle.
    KakariMusubi musubi;
  };
  static constexpr std::array<KakariParticle, 3> kKakariParticles = {{
      {U'\u3053', U'\u305d', KakariMusubi::Izenkei},    // こそ
      {U'\u305e', U'\0', KakariMusubi::Rentaikei},      // ぞ
      {U'\u306a', U'\u3080', KakariMusubi::Rentaikei},  // なむ
  }};
  // A 結び sits in the same clause as its particle, and a clause is short. The
  // window keeps the scan constant-time per candidate, which matters because
  // every candidate generator asks this question.
  constexpr size_t kMaxClauseChars = 24;
  if (dict_manager == nullptr) {
    return KakariMusubi::None;
  }
  const size_t scan_end = std::min(clause_pos, codepoints.size());
  const size_t scan_start = lookbehindStart(scan_end, kMaxClauseChars);
  for (size_t end = scan_end; end > scan_start; --end) {
    if (normalize::classifyChar(codepoints[end - 1]) == normalize::CharType::Symbol) {
      return KakariMusubi::None;
    }
    for (const auto& particle : kKakariParticles) {
      const size_t length = particle.tail == U'\0' ? 1 : 2;
      if (end < length || codepoints[end - 1] != (particle.tail == U'\0' ? particle.head : particle.tail)) {
        continue;
      }
      if (particle.tail != U'\0' && codepoints[end - 2] != particle.head) {
        continue;
      }
      // The morae have to be the particle itself rather than the tail of a word
      // that happens to spell it (かぞえる ends its second mora in ぞ).
      if (lookupEntryInRange(*dict_manager, codepoints, end - length, end, core::PartOfSpeech::Particle) != nullptr) {
        return particle.musubi;
      }
    }
  }
  return KakariMusubi::None;
}

bool endsWithClassicalAuxiliary(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos > codepoints.size() || end_pos < start_pos + 2) {
    return false;
  }
  for (size_t auxiliary_start = start_pos + 1; auxiliary_start < end_pos; ++auxiliary_start) {
    const auto* auxiliary =
        lookupEntryInRange(*dict_manager, codepoints, auxiliary_start, end_pos, core::PartOfSpeech::Auxiliary);
    if (auxiliary != nullptr && core::isClassicalAuxiliaryType(auxiliary->extended_pos)) {
      return true;
    }
  }
  return false;
}

bool spellsVerbCellWithClassicalAuxiliaries(const dictionary::DictionaryManager* dict_manager,
                                            const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos > codepoints.size() || end_pos < start_pos + 2) {
    return false;
  }
  const size_t chain_end = codepoints[end_pos - 1] == U'ば' || codepoints[end_pos - 1] == U'ど' ? end_pos - 1 : end_pos;
  // chain_starts[i]: [i, chain_end) is a run of classical auxiliary cells.
  std::vector<bool> chain_starts(chain_end + 1, false);
  chain_starts[chain_end] = true;
  for (size_t from = chain_end; from-- > start_pos + 1;) {
    for (size_t to = from + 1; to <= chain_end && !chain_starts[from]; ++to) {
      if (!chain_starts[to]) {
        continue;
      }
      const auto* auxiliary = lookupEntryInRange(*dict_manager, codepoints, from, to, core::PartOfSpeech::Auxiliary);
      chain_starts[from] = auxiliary != nullptr && core::isClassicalAuxiliaryType(auxiliary->extended_pos);
    }
  }
  // The usual 2+ mora floor: き, し, つ, り, に, ぬ alone are also okurigana
  // (着き, 飛ばし, 染まり), so a one-mora chain is no evidence.
  constexpr size_t kMinChainLength = 2;
  for (size_t aux_start = start_pos + 1; aux_start + kMinChainLength <= chain_end; ++aux_start) {
    if (!chain_starts[aux_start]) {
      continue;
    }
    const char32_t head_last = codepoints[aux_start - 1];
    const bool kanji_stem = normalize::isKanjiCodepoint(head_last) &&
                            (isSingleKanjiIchidan(head_last) || grammar::isKuruKanjiStem(head_last));
    // A kanji stem with one i/e-row okurigana is a continuative cell (咲き, 起き).
    const bool continuative_shape = aux_start >= start_pos + 2 &&
                                    normalize::isKanjiCodepoint(codepoints[aux_start - 2]) &&
                                    (kana::isIRowCodepoint(head_last) || kana::isERowCodepoint(head_last));
    if (kanji_stem || continuative_shape ||
        (!normalize::isKanjiCodepoint(head_last) &&
         lookupEntryInRange(*dict_manager, codepoints, start_pos, aux_start, core::PartOfSpeech::Verb) != nullptr)) {
      return true;
    }
  }
  return false;
}

bool formalNounFollowsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                         size_t pos) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  const std::string remaining = extractClosedClassProbe(codepoints, pos);
  return lookupResultsHaveExtendedPOS(dict_manager->lookup(remaining, 0), core::ExtendedPOS::NounFormal);
}

std::string lookupVerbLemma(const dictionary::DictionaryManager* dict_manager, std::string_view surface,
                            std::string_view fallback) {
  if (dict_manager != nullptr) {
    const auto* entry = dict_manager->lookupExact(surface, core::PartOfSpeech::Verb);
    if (entry != nullptr && !entry->lemma.empty()) {
      return entry->lemma;
    }
  }
  return std::string(fallback);
}

bool isVerifiedVerbBase(const dictionary::DictionaryManager* dict_manager, const grammar::Inflection& inflection,
                        std::string_view base_form, float min_confidence, bool require_godan) {
  if (isVerbInDictionary(dict_manager, base_form)) {
    return true;
  }
  auto infl_result = inflection.getBest(base_form);
  bool type_ok = require_godan ? grammar::isGodanVerbType(infl_result.verb_type)
                               : infl_result.verb_type == grammar::VerbType::Ichidan;
  return infl_result.confidence > min_confidence && type_ok;
}

}  // namespace suzume::analysis::verb_helpers
