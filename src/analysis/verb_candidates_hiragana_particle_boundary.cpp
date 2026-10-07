/**
 * @file verb_candidates_hiragana_particle_boundary.cpp
 * @brief Particle boundaries that open or close a hiragana predicate slot
 *
 * Decides whether a hiragana run opens on a closed-class particle followed by
 * a dictionary-verified verb, which makes the run a particle plus that verb
 * rather than the stem of an unknown hiragana verb, and whether a case
 * particle behind a kanji or nominal host fixes the predicate slot after it.
 */

#include <algorithm>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "core/debug.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

namespace vh = verb_helpers;

namespace {

bool immediatelyFollowsParticleHost(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0) {
    return false;
  }
  constexpr size_t kMaxHostChars = 12;
  constexpr PartOfSpeechMask kHostMask = kNounPronounMask | kVerbAdjectiveMask;
  const size_t min_host_start = lookbehindStart(start_pos, kMaxHostChars);
  return hasDictionaryEntryEndingAt(*dict_manager, codepoints, min_host_start, start_pos, kHostMask);
}

}  // namespace

namespace hiragana_verb_detail {

bool startsWithParticleThenVerifiedVerb(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                        const std::vector<normalize::CharType>& char_types,
                                        const grammar::Inflection& inflection,
                                        const dictionary::DictionaryManager* dict_manager,
                                        bool allow_single_char_particle_after_kanji) {
  if (dict_manager == nullptr || hiragana_end <= start_pos + 1) {
    return false;
  }
  size_t probe_end = hiragana_end;
  while (probe_end < char_types.size() && probe_end - start_pos < hiragana_verb_detail::kPredicateRunMax &&
         char_types[probe_end] == normalize::CharType::Hiragana) {
    ++probe_end;
  }
  constexpr size_t kMaxParticleChars = 4;
  size_t max_particle_end = std::min(probe_end, start_pos + kMaxParticleChars);
  const std::string full_surface = extractSubstring(codepoints, start_pos, probe_end);
  const bool full_surface_is_dictionary_verb =
      dict_manager->lookupExact(full_surface, core::PartOfSpeech::Verb) != nullptr;
  const auto& full_surface_candidates = inflection.analyze(full_surface);
  // The predicate slot is fixed by the case particle and its own host, so the
  // span that has to be a complete dictionary form is the one filling the slot.
  // The kana run can continue past it into a closed auxiliary the predicate
  // selects (道+を+とおる+なかれ), so that auxiliary is stripped before the
  // terminal is tested. Only a registered auxiliary is stripped: an arbitrary
  // tail would let any prefix certify the slot for the whole run.
  const bool follows_fixed_predicate_slot =
      hiragana_verb_detail::followsKanjiOrNominalHostBeforeCaseParticle(codepoints, start_pos, dict_manager);
  // A run standing bare in the predicate slot is one word only while nothing
  // reads it as an inflected form of an attested one. The analyzer's own
  // confidence cannot decide that: it scores the shape of the kana, so a coined
  // base outranks a real open-class verb whenever the real one is spelled
  // without kanji (すらなく as a godan-ka base scores above もたらす). The
  // attested decomposition is the evidence instead — すらなく is する plus the
  // negative, while every reading of もたらす coins its own base.
  const auto reads_as_inflected_dictionary_verb = [&](const auto& candidates) {
    return std::any_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
      return !candidate.morphemes.empty() &&
             dict_manager->lookupExact(candidate.base_form, core::PartOfSpeech::Verb) != nullptr;
    });
  };
  const auto is_complete_godan_terminal = [&](size_t terminal_end) {
    const std::string terminal = extractSubstring(codepoints, start_pos, terminal_end);
    const auto& terminal_candidates =
        terminal_end == probe_end ? full_surface_candidates : inflection.analyze(terminal);
    if (reads_as_inflected_dictionary_verb(terminal_candidates)) {
      return false;
    }
    return std::any_of(terminal_candidates.begin(), terminal_candidates.end(), [&](const auto& candidate) {
      return grammar::isGodanVerbType(candidate.verb_type) && candidate.base_form == terminal &&
             candidate.morphemes.empty();
    });
  };
  bool complete_terminal_after_case_particle = follows_fixed_predicate_slot && is_complete_godan_terminal(probe_end);
  for (size_t terminal_end = start_pos + 3;
       follows_fixed_predicate_slot && !complete_terminal_after_case_particle && terminal_end < probe_end;
       ++terminal_end) {
    if (lookupEntryInRange(*dict_manager, codepoints, terminal_end, probe_end, core::PartOfSpeech::Auxiliary) ==
        nullptr) {
      continue;
    }
    complete_terminal_after_case_particle = is_complete_godan_terminal(terminal_end);
  }
  // A complete Godan terminal immediately after a case particle occupies the
  // predicate slot. Its internal particle homograph (もどる, はしる) cannot
  // establish a competing particle boundary just because the suffix happens
  // to be registered as an auxiliary.
  if (complete_terminal_after_case_particle) {
    return false;
  }
  const bool follows_particle_host = immediatelyFollowsParticleHost(codepoints, start_pos, dict_manager);
  for (size_t particle_end = start_pos + 1; particle_end <= max_particle_end; ++particle_end) {
    std::string particle_surface = extractSubstring(codepoints, start_pos, particle_end);
    const auto* particle_entry = dict_manager->lookupExact(particle_surface, core::PartOfSpeech::Particle);
    if (particle_entry == nullptr) {
      continue;
    }
    // A sentence-final particle cannot introduce a dependent auxiliary or a
    // following verb inflection. Treating its homographic mora as a boundary
    // would suppress productive open-class verbs such as さける and かける.
    if (particle_entry->extended_pos == core::ExtendedPOS::ParticleFinal) {
      continue;
    }
    // A case particle immediately after a recognized independent host is a
    // strong boundary (そちら+で+やる must not become the fabricated でやる), but
    // it is only half the evidence: the host says the particle may start here,
    // not that the kana behind it is a separate word. The remainder still has
    // to be an attested predicate, which is what the loop below tests, so this
    // certificate lowers the confidence bar there rather than bypassing it.
    // Bypassing it costs the verbs whose own first mora is a particle
    // homograph, since every one of them stands after a host too (油+で+にじむ,
    // 踏み+にじる) and their remainder is fabricated.
    const bool host_certifies_particle =
        follows_particle_host && particle_entry->extended_pos == core::ExtendedPOS::ParticleCase;
    // A particle homograph can itself be the complete stem of a productive
    // open-class inflection (さえ+ない -> さえる). Preserve that lexical
    // candidate when the whole surface supplies independent morphological
    // evidence: the analyzed stem exactly covers the apparent particle, the
    // remainder is a real inflection suffix, and confidence clears the same
    // boundary threshold used below. A directly preceding noun or other
    // particle host keeps the closed-class reading (こと+さえ+ない). In
    // genuine particle chains such as さえ+い+ない, the whole-surface stem
    // extends beyond the particle and therefore does not receive this
    // exemption.
    const bool has_confident_whole_verb =
        !follows_particle_host &&
        std::any_of(full_surface_candidates.begin(), full_surface_candidates.end(), [&](const auto& candidate) {
          return candidate.verb_type != grammar::VerbType::IAdjective &&
                 candidate.verb_type != grammar::VerbType::Unknown && candidate.stem == particle_surface &&
                 !candidate.suffix.empty() && candidate.confidence >= candidate::kParticleVerbBoundaryMinConfidence &&
                 !vh::hasDictionaryEntry(dict_manager, candidate.base_form, core::PartOfSpeech::Auxiliary);
        });
    if (has_confident_whole_verb) {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_KEEP] \"" << full_surface << "\" confident_particle_homograph_inflection\n");
      continue;
    }
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_PARTICLE] \"" << particle_surface << "\" at pos=" << start_pos << "\n");
    // A closed nominal after a case or topic particle proves the boundary too,
    // when it ends the run or meets the copula or a particle (は+いつ+です,
    // は+いつ+まで): a predicate run has no pronoun inside it.
    if (particle_entry->extended_pos == core::ExtendedPOS::ParticleCase ||
        particle_entry->extended_pos == core::ExtendedPOS::ParticleTopic) {
      for (size_t nominal_end = particle_end + 2; nominal_end <= probe_end; ++nominal_end) {
        if (lookupEntryInRange(*dict_manager, codepoints, particle_end, nominal_end, core::PartOfSpeech::Pronoun) ==
            nullptr) {
          continue;
        }
        const bool closes_nominal =
            nominal_end >= codepoints.size() || char_types[nominal_end] != normalize::CharType::Hiragana ||
            lookupEntryInRange(*dict_manager, codepoints, nominal_end, nominal_end + 1, core::PartOfSpeech::Particle) !=
                nullptr ||
            vh::auxiliaryFollowsAt(dict_manager, codepoints, nominal_end, [](const dictionary::DictionaryEntry& entry) {
              return entry.extended_pos == core::ExtendedPOS::AuxCopulaDa ||
                     entry.extended_pos == core::ExtendedPOS::AuxCopulaDesu;
            });
        if (closes_nominal) {
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << particle_surface << "\" particle_then_closed_nominal\n");
          return true;
        }
      }
    }
    size_t verb_start = particle_end;
    for (size_t verb_end = probe_end; verb_end > verb_start + 1; --verb_end) {
      std::string verb_surface = extractSubstring(codepoints, verb_start, verb_end);
      // An exact open-class verb after a closed particle is stronger boundary
      // evidence than a generated particle-prefixed verb, even immediately
      // after kanji (結果+と+ひきかえる). Preserve an independently attested
      // whole verb such as できる before considering this split.
      if (allow_single_char_particle_after_kanji && !full_surface_is_dictionary_verb &&
          dict_manager->lookupExact(verb_surface, core::PartOfSpeech::Verb) != nullptr) {
        return true;
      }
      if (const auto* auxiliary = dict_manager->lookupExact(verb_surface, core::PartOfSpeech::Auxiliary);
          auxiliary != nullptr) {
        // A particle followed by an auxiliary is a boundary only when the
        // particle can actually host that auxiliary. The causative and passive
        // select a verb's irrealis form, so の+せる is not a competing reading
        // of のせる and must not remove it — the same reasoning that already
        // exempts a sentence-final particle above, read off the connection
        // table instead of restated per particle class.
        if (BigramTable::getCost(particle_entry->extended_pos, auxiliary->extended_pos) < bigram_cost::kAlmostNever) {
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << particle_surface << "+" << verb_surface
                                                    << " particle_then_auxiliary\n");
          return true;
        }
        continue;
      }
      for (const auto& candidate : inflection.analyze(verb_surface)) {
        const bool is_dictionary_verb = vh::isVerbInDictionary(dict_manager, candidate.base_form);
        const auto* auxiliary = dict_manager->lookupExact(candidate.base_form, core::PartOfSpeech::Auxiliary);
        if (auxiliary != nullptr) {
          const bool particle_can_host_auxiliary =
              BigramTable::getCost(particle_entry->extended_pos, auxiliary->extended_pos) < bigram_cost::kAlmostNever;
          // At a real predicate boundary (BOS or after a case particle), an
          // auxiliary reached only through a generated inflection cannot prove
          // that the apparent leading particle is genuine: のせられた is a
          // lexical stem plus passive, not の + causative. Inside an unbroken
          // kana run the same reading still suppresses a shorter fabricated
          // verb (とりもどせない must not admit どせない).
          const bool begins_predicate_slot =
              start_pos == 0 || vh::followsCaseParticle(dict_manager, codepoints, start_pos);
          if (!particle_can_host_auxiliary && begins_predicate_slot) {
            continue;
          }
        }
        const bool is_verified_verb = is_dictionary_verb || auxiliary != nullptr;
        // A connective て/で unambiguously ends the preceding predicate. When
        // its remainder inflects to a dictionary verb, do not fabricate a
        // larger hiragana verb across that boundary (嬉しく|て|なら|ない).
        // Other particle boundaries retain the confidence gate because their
        // surface forms can also begin lexical verbs.
        bool is_connective = particle_surface == "て" || particle_surface == "で";
        if (is_verified_verb && (is_connective || host_certifies_particle ||
                                 candidate.confidence >= candidate::kParticleVerbBoundaryMinConfidence)) {
          if (allow_single_char_particle_after_kanji && particle_end == start_pos + 1) {
            continue;
          }
          return true;
        }
      }
    }
  }
  return false;
}

bool followsKanjiOrNominalHostBeforeCaseParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                 const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos < 2) {
    return false;
  }
  // A two-mora limiting particle closes its clause the same way (降るまで+ならす):
  // the predicate that follows starts a fresh slot whatever the host was.
  constexpr size_t kLimitingParticleMorae = 2;
  const auto* limiting = start_pos > kLimitingParticleMorae
                             ? lookupEntryInRange(*dict_manager, codepoints, start_pos - kLimitingParticleMorae,
                                                  start_pos, core::PartOfSpeech::Particle)
                             : nullptr;
  if (limiting != nullptr && limiting->extended_pos == core::ExtendedPOS::ParticleAdverbial) {
    return true;
  }
  if (!vh::oneMoraParticleEndsAt(dict_manager, codepoints, start_pos, core::ExtendedPOS::ParticleCase)) {
    return false;
  }
  const size_t particle_start = start_pos - 1;
  if (normalize::isKanjiCodepoint(codepoints[particle_start - 1])) {
    return true;
  }
  constexpr size_t kMaxHostChars = 12;
  const size_t min_host_start = lookbehindStart(particle_start, kMaxHostChars);
  return hasDictionaryEntryEndingAt(*dict_manager, codepoints, min_host_start, particle_start, kNounPronounMask);
}

}  // namespace hiragana_verb_detail

}  // namespace suzume::analysis
