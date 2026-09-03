/**
 * @file verb_candidates_hiragana_inflection.cpp
 * @brief Inflected candidate selection for hiragana verbs
 */

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis {

namespace vh = verb_helpers;

namespace {

bool immediatelyFollowsParticleHost(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0) {
    return false;
  }
  constexpr size_t kMaxHostChars = 12;
  constexpr PartOfSpeechMask kHostMask =
      partOfSpeechMask(core::PartOfSpeech::Noun) | partOfSpeechMask(core::PartOfSpeech::Pronoun) |
      partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Adjective);
  const size_t min_host_start = (start_pos > kMaxHostChars) ? start_pos - kMaxHostChars : 0;
  return hasDictionaryEntryEndingAt(*dict_manager, codepoints, min_host_start, start_pos, kHostMask);
}

bool followsKanjiOrNominalHostBeforeCaseParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                 const dictionary::DictionaryManager* dict_manager,
                                                 const dictionary::DictionaryEntry* preceding_particle) {
  if (dict_manager == nullptr || preceding_particle == nullptr || start_pos < 2 ||
      preceding_particle->extended_pos != core::ExtendedPOS::ParticleCase) {
    return false;
  }
  const size_t particle_start = start_pos - 1;
  if (normalize::isKanjiCodepoint(codepoints[particle_start - 1])) {
    return true;
  }
  constexpr size_t kMaxHostChars = 12;
  constexpr PartOfSpeechMask kNominalHostMask =
      partOfSpeechMask(core::PartOfSpeech::Noun) | partOfSpeechMask(core::PartOfSpeech::Pronoun);
  const size_t min_host_start = particle_start > kMaxHostChars ? particle_start - kMaxHostChars : 0;
  return hasDictionaryEntryEndingAt(*dict_manager, codepoints, min_host_start, particle_start, kNominalHostMask);
}

bool startsWithParticleThenVerifiedVerb(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                        const std::vector<normalize::CharType>& char_types,
                                        const grammar::Inflection& inflection,
                                        const dictionary::DictionaryManager* dict_manager,
                                        bool allow_single_char_particle_after_kanji) {
  if (dict_manager == nullptr || hiragana_end <= start_pos + 1) {
    return false;
  }
  size_t probe_end = hiragana_end;
  while (probe_end < char_types.size() && probe_end - start_pos < 12 &&
         char_types[probe_end] == normalize::CharType::Hiragana) {
    ++probe_end;
  }
  constexpr size_t kMaxParticleChars = 4;
  size_t max_particle_end = std::min(probe_end, start_pos + kMaxParticleChars);
  const std::string full_surface = extractSubstring(codepoints, start_pos, probe_end);
  const bool full_surface_is_dictionary_verb =
      dict_manager->lookupExact(full_surface, core::PartOfSpeech::Verb) != nullptr;
  const auto& full_surface_candidates = inflection.analyze(full_surface);
  const auto* preceding_particle =
      lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle);
  // The predicate slot is fixed by the case particle and its own host, so the
  // span that has to be a complete dictionary form is the one filling the slot.
  // The kana run can continue past it into a closed auxiliary the predicate
  // selects (道+を+とおる+なかれ), so that auxiliary is stripped before the
  // terminal is tested. Only a registered auxiliary is stripped: an arbitrary
  // tail would let any prefix certify the slot for the whole run.
  const bool follows_fixed_predicate_slot =
      followsKanjiOrNominalHostBeforeCaseParticle(codepoints, start_pos, dict_manager, preceding_particle);
  const auto is_complete_godan_terminal = [&](size_t terminal_end) {
    const std::string terminal = extractSubstring(codepoints, start_pos, terminal_end);
    const auto& terminal_candidates =
        terminal_end == probe_end ? full_surface_candidates : inflection.analyze(terminal);
    return std::any_of(terminal_candidates.begin(), terminal_candidates.end(), [&](const auto& candidate) {
      return grammar::isGodanVerbType(candidate.verb_type) && candidate.base_form == terminal &&
             candidate.morphemes.empty() && candidate.confidence >= candidate::kParticleVerbBoundaryMinConfidence;
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

}  // namespace

namespace hiragana_verb_detail {

bool appendInflectedHiraganaVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           size_t hiragana_end, char32_t first_char,
                                           const std::vector<normalize::CharType>& char_types,
                                           const grammar::Inflection& inflection,
                                           const dictionary::DictionaryManager* dict_manager,
                                           const VerbCandidateOptions& verb_opts, bool has_complete_godan_wa_terminal,
                                           bool has_complete_case_particle_terminal,
                                           std::vector<UnknownCandidate>& candidates) {
  // A closed-class particle followed by a dictionary-verified verb inflection
  // is a grammatical boundary, never the stem of an unknown hiragana verb.
  // This preserves て+さえ+いれ+ば and analogous binding-particle sequences.
  const bool follows_kanji = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]);
  if (startsWithParticleThenVerifiedVerb(codepoints, start_pos, hiragana_end, char_types, inflection, dict_manager,
                                         follows_kanji)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " particle_then_verified_verb\n");
    return false;
  }

  // Try different lengths, starting from longest
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 1; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }
    // A predicate behind an interior case particle proves the boundary the
    // conjugation table would otherwise hide inside a fabricated stem.
    if (vh::crossesCaseParticleBeforePredicate(dict_manager, codepoints, start_pos, end_pos)) {
      continue;
    }

    // Check if this looks like a conjugated verb
    // First try the best match, but also check all candidates for dictionary verbs
    const auto& all_candidates = inflection.analyze(surface);
    grammar::InflectionCandidate best;
    bool is_dictionary_verb = false;

    // Look through all candidates to find ones whose base form is in the dictionary
    // Collect all matches and select the best one based on:
    // 1. Higher confidence
    // 2. GodanWa > GodanRa/GodanTa when tied (う verbs are much more common for hiragana)
    // This helps with cases like しまった where しまう (GodanWa) should beat しまる (GodanRa)
    if (dict_manager != nullptr) {
      std::vector<grammar::InflectionCandidate> dict_matches;

      for (const auto& cand : all_candidates) {
        if (cand.verb_type == grammar::VerbType::IAdjective || cand.base_form.empty()) {
          continue;
        }
        if (vh::isVerbInDictionary(dict_manager, cand.base_form)) {
          // Found a dictionary verb - collect this candidate
          dict_matches.push_back(cand);
        }
      }

      // Select the best dictionary match
      if (!dict_matches.empty()) {
        is_dictionary_verb = true;
        best = dict_matches[0];

        for (size_t i = 1; i < dict_matches.size(); ++i) {
          const auto& cand = dict_matches[i];
          // Higher confidence wins
          if (cand.confidence > best.confidence + 0.01F) {
            best = cand;
          } else if (std::abs(cand.confidence - best.confidence) <= 0.01F) {
            // When confidence is tied (within 0.01), prefer GodanWa over GodanRa/GodanTa
            // Rationale: For pure hiragana stems, う verbs (しまう, あらう, かう) are
            // much more common than る/つ verbs with the same stem pattern.
            // GodanRa: rare for pure hiragana (most are kanji: 走る, 帰る)
            // GodanTa: rare (持つ, 勝つ, etc. - usually with kanji)
            // GodanWa: very common in hiragana (しまう, あらう, まよう, etc.)
            if (cand.verb_type == grammar::VerbType::GodanWa &&
                (best.verb_type == grammar::VerbType::GodanRa || best.verb_type == grammar::VerbType::GodanTa)) {
              best = cand;
            }
          }
        }
      }
    }

    // If no dictionary match, select best candidate with GodanWa preference
    // When confidence is tied, GodanWa should beat GodanRa/GodanTa because
    // う verbs (あらう, かう, まよう) are much more common than る/つ verbs
    // for pure hiragana stems
    if (!is_dictionary_verb && !all_candidates.empty()) {
      bool found_verb_candidate = false;
      for (const auto& cand : all_candidates) {
        if (cand.verb_type == grammar::VerbType::IAdjective) {
          continue;
        }
        if (!found_verb_candidate) {
          best = cand;
          found_verb_candidate = true;
          continue;
        }
        // Higher confidence wins
        if (cand.confidence > best.confidence + 0.01F) {
          best = cand;
        } else if (std::abs(cand.confidence - best.confidence) <= 0.01F) {
          // When confidence is tied (within 0.01), prefer GodanWa over GodanRa/GodanTa
          if (cand.verb_type == grammar::VerbType::GodanWa &&
              (best.verb_type == grammar::VerbType::GodanRa || best.verb_type == grammar::VerbType::GodanTa)) {
            best = cand;
          }
        }
      }
    }

    // A terminal hiragana run ending in く can be an unattested Godan-ka
    // dictionary form. Some stems are otherwise analyzed only as i-adjective
    // fragments, even though 〜く is their finite verb ending. Dictionary
    // adverbs and adjective forms remain protected by the non-verb gate below.
    if (!is_dictionary_verb && end_pos == hiragana_end && end_pos - start_pos >= 3 &&
        codepoints[end_pos - 1] == U'く' &&
        (best.verb_type == grammar::VerbType::IAdjective ||
         best.confidence < candidate::verb_cost::kTerminalHiraganaGodanConfidence)) {
      best.base_form = surface;
      best.stem = surface.substr(0, surface.size() - core::kJapaneseCharBytes);
      best.suffix.clear();
      best.verb_type = grammar::VerbType::GodanKa;
      best.confidence = candidate::verb_cost::kTerminalHiraganaGodanConfidence;
      best.morphemes.clear();
    }

    // A terminal hiragana run that the analyzer already reads as a Godan
    // dictionary form is an open-class verb with no dictionary entry
    // (くつろぐ, なごむ, たしなむ). The row's own paradigm supplying the finite
    // ending is the strongest evidence available for such a word, so it must not
    // be discarded as a low-confidence fragment. The reading is taken from the
    // analyzer rather than imposed, so ichidan and i-adjective readings of the
    // same run are unaffected. The GodanWa row is excluded because its terminal
    // う is also the volitional auxiliary and a frequent noun ending
    // (とうきょう, でしょう); that row keeps its own scanner-verified gate below.
    if (!is_dictionary_verb && end_pos == hiragana_end && end_pos - start_pos >= 3 &&
        grammar::isGodanVerbType(best.verb_type) && best.verb_type != grammar::VerbType::GodanWa &&
        best.base_form == surface && best.confidence < candidate::verb_cost::kTerminalHiraganaGodanConfidence) {
      best.confidence = candidate::verb_cost::kTerminalHiraganaGodanConfidence;
    }

    // A fabricated verb must not cross a productive te-form boundary between
    // two dictionary-verified verb forms (なっ+て+なら, やっ+て+みる).
    // Genuine lexical verbs are exempt because their full surface is verified.
    if (!is_dictionary_verb &&
        vh::hasInternalVerbChainBoundary(codepoints, start_pos, end_pos, inflection, dict_manager)) {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" internal_connective_verb_boundary\n");
      continue;
    }

    // Nor may it be spelled entirely by a chain of registered auxiliaries: the
    // paradigm tables endorse a base for any run ending in a verbal mora, so
    // ぬ+べし becomes the continuative of a non-word (ぬべす).
    if (!is_dictionary_verb && hasAuxiliaryChainDecomposition(codepoints, start_pos, end_pos, dict_manager)) {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" auxiliary_chain\n");
      continue;
    }

    // Nor may it open on the tail of a closed-class word that started before it:
    // しょう in 高いでしょうから is the last two morae of the polite copula でしょ
    // plus the volitional う, and the tables read that as the dictionary form of
    // the non-word しょう. The chain test above cannot see it, because the
    // auxiliary boundary lies outside the fabricated span.
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (!is_dictionary_verb && vh::opensOnClosedClassWordTail(dict_manager, codepoints, start_pos, end_pos)) {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" opens on a closed-class word tail\n");
      continue;
    }

    // Nor may a two-mora hypothesis — the shortest base the tables endorse, and
    // the one carrying the least evidence — start one mora inside a registered
    // verb that ends where it does (か+かる for かかる). Every two-mora verb of
    // the modern language is frequent enough to be registered, so an unattested
    // one competing with a dictionary word is a fragment of it.
    if (!is_dictionary_verb && end_pos - start_pos == 2 && start_pos > 0 &&
        vh::isVerbInDictionary(dict_manager, extractSubstring(codepoints, start_pos - 1, end_pos))) {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" starts inside a dictionary verb\n");
      continue;
    }

    const size_t pre_filter_len = end_pos - start_pos;
    const bool looks_like_short_godan_base = pre_filter_len == 2 && grammar::isGodanVerbType(best.verb_type) &&
                                             best.base_form == surface &&
                                             best.confidence >= verb_opts.confidence_short_godan_base;

    // Filter out 2-char hiragana that don't end with a recognized verb form.
    // Short Godan base forms are licensed by inflection structure rather than
    // by the surface-only form detector, which deliberately treats ambiguous
    // u-row endings conservatively.
    // Also allow れ (Ichidan renyokei/meireikei like くれ from くれる).
    // This prevents false positives like まじ, ため from being recognized as verbs
    if (pre_filter_len == 2 && surface.size() >= core::kJapaneseCharBytes) {
      // Use string_view directly into surface to avoid dangling reference
      // (surface.substr() returns a temporary std::string)
      std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes, core::kJapaneseCharBytes);
      const core::ExtendedPOS detected_form = core::detectVerbForm(surface);
      if (detected_form != core::ExtendedPOS::VerbShuushikei && detected_form != core::ExtendedPOS::VerbTeForm &&
          detected_form != core::ExtendedPOS::VerbTaForm && last_char != "れ" && !looks_like_short_godan_base) {
        continue;  // Skip 2-char hiragana not ending with valid verb suffix
      }
    }

    // Filter out i-adjective conjugation suffixes (standalone, not verb candidates)
    // See scorer_constants.h for documentation on these patterns.
    if (surface == scorer::kIAdjPastKatta || surface == scorer::kIAdjPastKattara || surface == scorer::kIAdjTeKute ||
        surface == scorer::kIAdjNegKunai || surface == scorer::kIAdjCondKereba || surface == scorer::kIAdjStemKa ||
        surface == scorer::kIAdjNegStemKuna || surface == scorer::kIAdjCondStemKere) {
      continue;  // Skip i-adjective conjugation patterns
    }

    // Note: Common adverbs/onomatopoeia (ぴったり, はっきり, etc.) are filtered
    // by the dictionary lookup below - they are registered as Adverb in L1 dictionary.

    // Filter out words that exist in dictionary as non-verb entries.  A complete
    // independent terminal may still be the lexical reading of an auxiliary
    // homograph; dependent uses remain on the ordinary auxiliary path.
    const bool is_independent_auxiliary_homograph =
        has_complete_godan_wa_terminal && end_pos == hiragana_end &&
        vh::hasDictionaryEntry(dict_manager, surface, core::PartOfSpeech::Auxiliary);
    if (vh::hasNonVerbDictionaryEntry(dict_manager, surface) && !is_independent_auxiliary_homograph) {
      continue;  // Skip - dictionary has non-verb entry for this surface
    }

    // A non-dictionary run ending in -げなく is a nominal/adjectival げ
    // construction followed by the negative continuative, not a terminal
    // Godan verb.  Preserve the productive boundary rather than accepting a
    // fabricated base whose surface merely happens to end in く.
    if (!is_dictionary_verb && utf8::endsWith(surface, "げなく")) {
      continue;
    }

    // Filter out volitional-shaped surfaces (お-row kana + う) of dictionary-
    // attested verbs. A conjugated verb surface can end in う only as its
    // dictionary form (しまう, まよう, おもう), in which case it equals the
    // analyzed base form. When the surface differs from the base form,
    // [お-row]+う is the volitional shape (未然形 お-row + auxiliary う):
    // なろう = なろ + う. Emitting it merged (auto-tagged 連用形 by
    // detectVerbForm's fallback) pre-empts the 未然形 + AuxVolitional split
    // path, so suppress the merged candidate. Dictionary evidence normally
    // licenses the split. An immediately quoted volitional supplies equivalent
    // closed right context, allowing an open verb without a word entry.
    if (pre_filter_len >= 2 && codepoints[end_pos - 1] == U'う' && grammar::isORowCodepoint(codepoints[end_pos - 2]) &&
        best.base_form != surface) {
      bool base_is_dict_aux = vh::hasDictionaryEntry(dict_manager, best.base_form, core::PartOfSpeech::Auxiliary);
      bool has_internal_auxiliary_suffix = false;
      if (dict_manager != nullptr) {
        const size_t stem_end = end_pos - 1;
        for (size_t auxiliary_start = start_pos + 1; auxiliary_start < stem_end; ++auxiliary_start) {
          if (lookupEntryInRange(*dict_manager, codepoints, auxiliary_start, stem_end, core::PartOfSpeech::Auxiliary) !=
              nullptr) {
            has_internal_auxiliary_suffix = true;
            break;
          }
        }
      }
      const bool particle_bounded_unknown_volitional =
          !is_dictionary_verb && !base_is_dict_aux && !has_internal_auxiliary_suffix && end_pos < codepoints.size() &&
          codepoints[end_pos] == U'と' && dict_manager != nullptr &&
          lookupEntryInRange(*dict_manager, codepoints, end_pos, end_pos + 1, core::PartOfSpeech::Particle) != nullptr;
      if (is_dictionary_verb || base_is_dict_aux || particle_bounded_unknown_volitional) {
        // Dictionary verbs already expose their 未然形 as a dict edge (なろ).
        // Aux-registered subsidiary verbs (しまう) list only hand-picked
        // forms, so generate the 未然形 stem here to complete the split path
        // (しまおう → しまお + う).
        const auto* godan_row = grammar::Conjugation::getGodanRow(best.verb_type);
        if (!is_dictionary_verb && godan_row != nullptr && godan_row->o_row == codepoints[end_pos - 2] &&
            pre_filter_len >= 3) {
          std::string stem_surface = extractSubstring(codepoints, start_pos, end_pos - 1);
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] " << stem_surface
                                                  << " hiragana_volitional_mizenkei lemma=" << best.base_form << "\n");
          candidates.push_back(makeVerbCandidate(
              stem_surface, start_pos, end_pos - 1, candidate::verb_cost::kWeakPenalty, best.base_form,
              grammar::verbTypeToConjType(best.verb_type), true, CandidateOrigin::VerbHiragana, best.confidence,
              "hiragana_volitional_mizenkei", core::ExtendedPOS::VerbMizenkei));
        }
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" skip volitional_shape (base=" << best.base_form
                                                  << ")\n");
        continue;  // Let 未然形 + う (AuxVolitional) split win
      }
    }

    // A pure-hiragana verb candidate must not absorb the closed appearance
    // auxiliary and following copula. In なさそうだ, a fabricated verb such as
    // さそうだ otherwise wins over the grammatical さ + そう + だ path.
    if (utf8::endsWith(surface, "そうだ")) {
      continue;
    }

    // Filter out verb stems that would form compound particles with て/で
    // e.g., によっ + て = によって (particle), とし + て = として (particle)
    // These compound particles exist as dictionary entries and should not be
    // split into spurious verb + て patterns
    {
      std::string_view last_char = utf8::lastChar(surface);
      if (utf8::equalsAny(last_char, {"っ", "し", "つ", "い"})) {
        std::string te_form = normalize::concat(surface, "て");
        std::string de_form = normalize::concat(surface, "で");
        if (vh::hasParticleDictionaryEntry(dict_manager, te_form) ||
            vh::hasParticleDictionaryEntry(dict_manager, de_form)) {
          continue;  // Skip - would split a compound particle
        }
      }
    }

    // Filter out te-form compound verb patterns that should be split
    // e.g., なっております → なっ+て+おり+ます, してます → し+て+ます
    //       してください → し+て+ください, してほしい → し+て+ほしい
    //       してくれます → し+て+くれ+ます
    // These contain て+auxiliary patterns that should be analyzed separately
    // Only skip for longer forms (5+ chars) to avoid blocking short verbs
    if (end_pos - start_pos >= 5) {
      // Check for ており/ていま/てい/てお/てくださ/てほしい/てくれ/てもら patterns
      // (te-form + auxiliary verb patterns)
      if (surface.find("ており") != std::string::npos || surface.find("ていま") != std::string::npos ||
          surface.find("ている") != std::string::npos || surface.find("ていた") != std::string::npos ||
          surface.find("てくださ") != std::string::npos || surface.find("てほしい") != std::string::npos ||
          surface.find("てくれ") != std::string::npos || surface.find("てもら") != std::string::npos ||
          surface.find("てお") != std::string::npos) {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" skip te_compound_pattern\n");
        continue;  // Skip - let te-form split win
      }
    }
    // Filter ていく/ていっ/ていけ (te + iku directional aspect) at 4+ chars
    // E.g., していく → し+て+いく (not a single verb)
    //       していった → し+て+いっ+た, していって → し+て+いっ+て
    if (end_pos - start_pos >= 4) {
      if (vh::guardIsWired(vh::GuardMember::EmbedTeAuxiliary, vh::GuardOrigin::HiraganaInflection) &&
          vh::embedsTeFormAuxiliary(surface)) {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" skip te_iku_pattern\n");
        continue;  // Skip - let te + iku split win
      }
    }

    // Check for 3-4 char hiragana verb ending with た/だ (past form) BEFORE threshold check
    // e.g., つかれた (疲れた), ねむった (眠った), おきた (起きた)
    // These need lower threshold because ichidan_pure_hiragana_stem penalty reduces confidence
    size_t pre_check_len = end_pos - start_pos;
    bool looks_like_past_form = false;
    bool looks_like_te_form = false;
    if ((pre_check_len == 3 || pre_check_len == 4) && surface.size() >= core::kJapaneseCharBytes) {
      std::string_view last_char = utf8::lastChar(surface);
      if (grammar::isPastMarkerTaDaSurface(last_char)) {
        looks_like_past_form = true;
      } else if (grammar::isTeDeSurface(last_char)) {
        // Te-form verbs (あらって, しまって, かって) need lower threshold too
        looks_like_te_form = true;
      }
    }

    // Check for ichidan dictionary form (e-row stem + る)
    // e.g., たべる (食べる), しらべる (調べる), つかれる (疲れる)
    // These need lower threshold because ichidan_pure_hiragana_stem penalty reduces confidence
    // Note: Check pattern structure directly, not verb_type, because when multiple
    // candidates have the same confidence, the godan candidate may be returned first
    // Exception: Exclude てる pattern (て + る) which is the ている contraction
    // e.g., してる should be する+ている, not しる (ichidan)
    bool looks_like_ichidan_dict_form = false;
    if (pre_check_len >= 3 && surface.size() >= core::kTwoJapaneseCharBytes) {
      std::string_view last_char = utf8::lastChar(surface);
      if (last_char == "る" && end_pos >= 2) {
        // Check if second-to-last char is e-row or i-row hiragana (ichidan stem ending)
        // E-row: 食べる, 見える, 調べる
        // I-row: 感じる, 信じる (kanji + i-row + る pattern)
        char32_t stem_end = codepoints[end_pos - 2];
        if (grammar::isERowCodepoint(stem_end) || grammar::isIRowCodepoint(stem_end)) {
          // Exclude てる pattern (ている contraction) - this should be suru/godan + ている
          // not ichidan dictionary form
          bool is_te_iru_contraction = (stem_end == U'て' || stem_end == U'で');
          // Exclude particle + いる pattern (にいる, でいる, etc.)
          // These should be split as particle + いる (existence verb), not a single verb
          // Valid hiragana verbs starting with particle chars: にる (煮る), にげる (逃げる)
          // But にいる, であるいる, etc. are not valid verbs
          bool is_particle_iru = false;
          if (pre_check_len == 3 && stem_end == U'い' && normalize::isCommonParticle(first_char)) {
            // 3-char pattern: particle + いる
            is_particle_iru = true;
          }
          if (!is_te_iru_contraction && !is_particle_iru) {
            // Find ichidan candidate to use for verb type and base form
            // For dictionary forms (e-row stem + る), prefer longer valid stems
            // Valid: つかれる (e-row ending), Invalid: つかれるる (るる pattern)
            grammar::InflectionCandidate best_ichidan;
            bool found_ichidan = false;
            for (const auto& cand : all_candidates) {
              if (cand.verb_type == grammar::VerbType::Ichidan &&
                  cand.confidence >= verb_opts.confidence_ichidan_dict) {
                // Skip invalid るる pattern (e.g., つかれるる)
                if (cand.base_form.size() >= 2 * core::kJapaneseCharBytes) {
                  std::string_view ending(cand.base_form.data() + cand.base_form.size() - 2 * core::kJapaneseCharBytes,
                                          2 * core::kJapaneseCharBytes);
                  if (ending == "るる") {
                    continue;  // Skip invalid pattern
                  }
                }
                if (!found_ichidan) {
                  best_ichidan = cand;
                  found_ichidan = true;
                } else if (cand.base_form.size() > best_ichidan.base_form.size()) {
                  // Prefer longer base form (e.g., つかれる > つかる)
                  best_ichidan = cand;
                }
              }
            }
            if (found_ichidan) {
              looks_like_ichidan_dict_form = true;
              // Use ichidan candidate as best if pattern matches
              if (best.verb_type != grammar::VerbType::Ichidan) {
                best = best_ichidan;
              } else if (best_ichidan.base_form.size() > best.base_form.size()) {
                // Even if already Ichidan, prefer longer base form
                best = best_ichidan;
              }
            }
          }
        }
      }
    }

    // At the case-particle predicate boundary, retain the complete Godan
    // analysis that licensed the scan. An e-row + る spelling also admits an
    // Ichidan hypothesis in isolation, but it cannot replace the structurally
    // complete terminal reading here.
    if (has_complete_case_particle_terminal && end_pos == hiragana_end) {
      for (const auto& candidate : all_candidates) {
        if (grammar::isGodanVerbType(candidate.verb_type) && candidate.base_form == surface &&
            candidate.morphemes.empty() && candidate.confidence >= verb_opts.confidence_standard) {
          best = candidate;
          looks_like_ichidan_dict_form = false;
          break;
        }
      }
    }

    // Only accept verb types (not IAdjective) with sufficient confidence
    // Lower threshold for dictionary-verified verbs, past/te forms, and ichidan dict forms
    // Ichidan dict forms get very low threshold (0.28) because pure hiragana stems
    // with 3+ chars get multiple penalties (stem_long + ichidan_pure_hiragana_stem)
    // When both is_dictionary_verb AND (past/te form) apply, use the lower threshold
    // This handles cases like つかんで (掴んで) where confidence is ~0.3
    float conf_threshold;
    if (is_dictionary_verb && (looks_like_past_form || looks_like_te_form)) {
      // Dictionary verb in past/te form: use lower of the two thresholds
      conf_threshold = std::min(verb_opts.confidence_dict_verb, verb_opts.confidence_past_te);
    } else if (is_dictionary_verb) {
      conf_threshold = verb_opts.confidence_dict_verb;
    } else if (looks_like_past_form || looks_like_te_form) {
      conf_threshold = verb_opts.confidence_past_te;
    } else if (looks_like_ichidan_dict_form) {
      conf_threshold = verb_opts.confidence_ichidan_dict;
    } else if (looks_like_short_godan_base) {
      conf_threshold = verb_opts.confidence_short_godan_base;
    } else if (has_complete_godan_wa_terminal && end_pos == hiragana_end &&
               best.verb_type == grammar::VerbType::GodanWa && best.base_form == surface) {
      conf_threshold = verb_opts.confidence_low;
    } else {
      conf_threshold = verb_opts.confidence_standard;
    }
    const bool comma_clause_chaining_renyokei =
        best.morphemes.empty() && best.suffix.size() == core::kJapaneseCharBytes &&
        grammar::isIRowCodepoint(codepoints[end_pos - 1]) &&
        vh::isCommaClauseChainingRenyokei(codepoints, start_pos, end_pos, dict_manager);
    if (comma_clause_chaining_renyokei) {
      conf_threshold = std::min(conf_threshold, verb_opts.confidence_ichidan_dict);
    }
    if (best.confidence > conf_threshold && best.verb_type != grammar::VerbType::IAdjective) {
      // Skip long particle-starting verb candidates when remainder is a valid verb form
      // e.g., "になっております" should be "に" + "なっております", not a single verb
      //       "はならぬ" should be "は" + "なら" + "ぬ", not a godan-ra negative
      // This prevents false verbs like "になる" + conjugation from being recognized
      // Apply to 4+ char forms; remainder check ensures genuine verbs are preserved
      size_t len_check = end_pos - start_pos;
      const bool verified_initial_no_inflection =
          first_char == U'の' && best.verb_type != grammar::VerbType::IAdjective && !best.suffix.empty() &&
          best.confidence >= verb_opts.confidence_ichidan_dict;
      if (len_check >= 4 && normalize::isCommonParticle(first_char) && !verified_initial_no_inflection) {
        // Extract remainder (surface without first character)
        std::string remainder = surface.substr(core::kJapaneseCharBytes);
        const auto& remainder_cands = inflection.analyze(remainder);
        // Use a relaxed confidence threshold (0.3) for 4-char surfaces — for
        // remainders with 1-char stems like なる→なら+ぬ the score is hit by
        // godan_ra_single_hiragana (-0.3) so confidence sits around 0.37 even
        // for genuinely valid forms. The 5+ char path keeps the stricter 0.5.
        float min_conf = (len_check >= 5) ? candidate::kParticlePrefixedVerbRemainderMinConfidenceLong
                                          : candidate::kParticlePrefixedVerbRemainderMinConfidenceShort;
        for (const auto& rem_cand : remainder_cands) {
          if (rem_cand.verb_type != grammar::VerbType::IAdjective && rem_cand.verb_type != grammar::VerbType::Unknown &&
              rem_cand.confidence >= min_conf) {
            // Remainder looks like a valid verb form - skip this candidate
            // to let particle + verb split win
            SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" skip particle_start_verb (particle=U+"
                                                      << std::hex << static_cast<uint32_t>(first_char) << std::dec
                                                      << ", remainder=" << remainder << ", conf=" << rem_cand.confidence
                                                      << ")\n");
            goto next_length;  // Continue to next end_pos
          }
        }
      }

      // Skip 3-char particle + いる/ある patterns (にいる, にある, でいる, といる, etc.)
      // These should be particle + existence verb, not a single hiragana verb
      // Valid 3-char verbs: にる(煮る), にげる(逃げる) have different patterns
      // Include extended particles: で, と, も (in addition to common particles)
      if (len_check == 3 && normalize::isExtendedParticle(first_char)) {
        std::string remainder = surface.substr(core::kJapaneseCharBytes);
        if (remainder == "いる" || remainder == "ある") {
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" skip particle_iru_aru (particle=U+" << std::hex
                                                    << static_cast<uint32_t>(first_char) << std::dec << ")\n");
          goto next_length;
        }
      }

      // Lower cost for higher confidence matches
      float base_cost =
          candidate::confidenceScaledCost(verb_opts.base_cost_high, best.confidence, verb_opts.confidence_cost_scale);

      // A complete Godan terminal directly after a case particle has an
      // independently determined predicate boundary. This is structural
      // evidence, not lexical evidence: it permits kana verbs whose stem
      // contains a particle homograph (犬とはしる, 家にもどる) to compete with
      // the otherwise cheap particle chain.
      if (has_complete_case_particle_terminal && end_pos == hiragana_end && best.base_form == surface &&
          best.morphemes.empty() && grammar::isGodanVerbType(best.verb_type)) {
        base_cost += bigram_cost::kVeryStrongBonus;
      }

      // Give significant bonus for dictionary-verified hiragana verbs
      // This helps them beat the particle+adj+particle split path
      // Only apply to longer forms (5+ chars) to avoid boosting short forms like
      // "あった" (ある) which can interfere with copula recognition (であった)
      // Exception: Conditional forms (ending with ば) are unambiguous and should
      // get the bonus even if short (e.g., あれば = ある conditional)
      size_t candidate_len = end_pos - start_pos;
      bool is_conditional = utf8::endsWith(surface, "ば");
      // Check for っとく pattern (ておく contraction: やっとく, 見っとく)
      // This is a common colloquial pattern that should get bonus treatment
      bool is_teoku_contraction = utf8::endsWith(surface, "っとく");
      // Check for short te/de-form (e.g., ねて, でて, みて)
      // These are 2-char hiragana verbs that need a bonus to beat particle splits
      bool is_short_te_form = false;
      if (candidate_len == 2 && best.confidence >= verb_opts.confidence_high) {
        is_short_te_form = utf8::endsWithAny(surface, {"て", "で"});
      }

      // Check for 3-4 char hiragana verb ending with た/だ (past form)
      // e.g., つかれた (疲れた), ねむった (眠った), おきた (起きた)
      // These medium-length verbs need a bonus to beat particle splits like つ+か+れた
      // Note: Lower confidence threshold (0.25) because ichidan_pure_hiragana_stem penalty
      // reduces confidence significantly for pure hiragana verbs
      // Skip if stem (without た/だ) is a known auxiliary (e.g., そうだ → そう is AUX)
      bool is_medium_past_form = false;
      if ((candidate_len == 3 || candidate_len == 4) && best.confidence >= verb_opts.confidence_past_te) {
        if (grammar::isPastMarkerTaDaSurface(utf8::lastChar(surface))) {
          // Extract stem (surface without last た/だ)
          std::string_view stem(surface.data(), surface.size() - core::kJapaneseCharBytes);
          // Skip if stem is a known auxiliary (e.g., そう+だ should not be verb candidate)
          if (!vh::hasDictionaryEntry(dict_manager, stem, core::PartOfSpeech::Auxiliary)) {
            is_medium_past_form = true;
          }
        }
      }

      if (is_dictionary_verb && (candidate_len >= 5 || is_conditional || is_teoku_contraction)) {
        base_cost = candidate::confidenceScaledCost(verb_opts.base_cost_verified, best.confidence,
                                                    verb_opts.confidence_cost_scale_medium);
      } else if (is_short_te_form) {
        // Short te-form with high confidence: give strong bonus to beat particle splits
        // e.g., ねて (conf=0.79) should beat ね(PARTICLE) + て(PARTICLE)
        // Particle path total cost can be as low as 0.002 due to dictionary bonuses,
        // so we need negative cost to compete. After adding POS prior (0.2 for verb),
        // the total needs to be below 0.002, so base needs to be below -0.2.
        //
        // When the first char is a common particle (で, に, etc.), these particles
        // have very low cost (e.g., で: -0.4), making particle+て path even cheaper
        // (total around -0.5). Need extra strong bonus for these cases.
        // EXCEPTION: If the 1-char stem is a known verb (e.g., でる, ねる in dictionary),
        // we want to prefer split path (で+て, ね+て), so use weaker bonus
        bool starts_with_common_particle =
            (first_char == U'で' || first_char == U'に' || first_char == U'が' || first_char == U'を' ||
             first_char == U'は' || first_char == U'の' || first_char == U'へ');
        // Check if 1-char stem + る is a known verb (e.g., でる, ねる)
        std::string one_char_stem = extractSubstring(codepoints, start_pos, start_pos + 1);
        std::string potential_verb = one_char_stem + "る";
        bool has_1char_verb_in_dict = vh::isVerbInDictionary(dict_manager, potential_verb);
        if (has_1char_verb_in_dict) {
          // Prefer split path (で+て) over combined (でて) when verb is in dictionary
          // Use moderate cost that can be beaten by 1-char renyokei candidate
          base_cost = candidate::confidenceScaledCost(verb_opts.base_cost_low, best.confidence,
                                                      verb_opts.confidence_cost_scale_small);
        } else if (starts_with_common_particle) {
          // Extra strong bonus: need to beat particle paths around -0.5
          base_cost = candidate::confidenceScaledCost(verb_opts.bonus_long_verified, best.confidence,
                                                      verb_opts.confidence_cost_scale_small);
        } else {
          base_cost = candidate::confidenceScaledCost(verb_opts.bonus_long_dict, best.confidence,
                                                      verb_opts.confidence_cost_scale_small);
        }
      } else if (is_medium_past_form) {
        // Medium-length past form verbs (3-4 chars ending with た/だ)
        // e.g., つかれた (conf=0.43) should beat つ+か+れた split
        // Give bonus to compete with particle splits
        base_cost = candidate::confidenceScaledCost(verb_opts.confidence_cost_scale_medium, best.confidence,
                                                    verb_opts.confidence_cost_scale_medium);
      } else if (looks_like_ichidan_dict_form) {
        // Ichidan dictionary form (e-row stem + る)
        // e.g., たべる (conf=0.39), しらべる, つかれる
        // These are highly likely to be real verbs, give modest bonus
        // Starting with particle-like chars (た, etc.) needs stronger bonus
        bool starts_with_aux_like_char = (first_char == U'た' || first_char == U'で' || first_char == U'に');
        if (starts_with_aux_like_char) {
          // Extra bonus: need to beat た(AUX) + べる(AUX) split
          base_cost = candidate::confidenceScaledCost(verb_opts.base_cost_verified, best.confidence,
                                                      verb_opts.confidence_cost_scale_medium);
        } else {
          base_cost = candidate::confidenceScaledCost(verb_opts.base_cost_low, best.confidence,
                                                      verb_opts.confidence_cost_scale_medium);
        }
      } else if (candidate_len >= 7 && best.confidence >= verb_opts.confidence_very_high) {
        // For long hiragana verb forms (7+ chars) with high confidence,
        // give a bonus even without dictionary verification.
        // This helps forms like かけられなくなった (9 chars) beat the
        // particle+verb split path (か + けられなくなった).
        // The length requirement (7+ chars) helps avoid false positives.
        //
        // When the verb starts with a character that's commonly mistaken for
        // a particle (か, は, が, etc.), give an extra strong bonus because
        // the particle split path is very likely to compete.
        bool starts_with_particle_char = (first_char == U'か' || first_char == U'は' || first_char == U'が' ||
                                          first_char == U'を' || first_char == U'に' || first_char == U'で' ||
                                          first_char == U'と' || first_char == U'も' || first_char == U'へ');
        if (starts_with_particle_char) {
          // Extra strong bonus for forms starting with particle-like char
          // e.g., かけられなくなった should strongly beat か + けられなくなった
          base_cost = candidate::confidenceScaledCost(verb_opts.base_cost_long_verified, best.confidence,
                                                      verb_opts.confidence_cost_scale_small);
        } else {
          base_cost = candidate::confidenceScaledCost(verb_opts.confidence_cost_scale_medium, best.confidence,
                                                      verb_opts.confidence_cost_scale_medium);
        }
      }

      // Penalty for unverified bare godan renyokei candidates
      // A godan renyokei with no auxiliary chain (suffix is a single i-row
      // char, e.g., もたち → もたつ, もだち → もだつ) whose base form is not
      // in the dictionary is rarely a genuine verb usage unless followed by
      // a renyokei-connecting auxiliary. Without this penalty, noun+suffix
      // splits like こども+たち lose to spurious verb readings (こど+もたち).
      // Exemptions:
      // - Dictionary-verified verbs keep their cost (e.g., わたし from わたす)
      // - Next char starting a renyokei continuation keeps the candidate
      //   viable for verb+aux splits: ま(ます), そ(そう), な(ながら/なさい),
      //   た(たい/たがる), や(やすい), に(にくい/purpose に), つ(つつ)
      if (!is_dictionary_verb && best.morphemes.empty() && best.suffix.size() == core::kJapaneseCharBytes &&
          grammar::isIRowCodepoint(codepoints[end_pos - 1])) {
        char32_t next_after = (end_pos < codepoints.size()) ? codepoints[end_pos] : 0;
        bool licenses_renyokei =
            (next_after == U'ま' || next_after == U'そ' || next_after == U'な' || next_after == U'た' ||
             next_after == U'や' || next_after == U'に' || next_after == U'つ' ||
             vh::isCommaClauseChainingRenyokei(codepoints, start_pos, end_pos, dict_manager));
        if (!licenses_renyokei) {
          base_cost += scorer::kPenaltyUnverifiedVerbLemma;
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_PENALTY] \"" << surface << "\" unverified_bare_renyokei +"
                                                       << scorer::kPenaltyUnverifiedVerbLemma << "\n");
        }
      }

      // A bare Ichidan continuative before past た has a closed right boundary.
      // When it also follows a nominal case-marked argument, retain that
      // predicate reading over a whole-span unknown noun (友と+わかれ+た).
      if (!is_dictionary_verb && end_pos < codepoints.size() && codepoints[end_pos] == U'た' &&
          best.morphemes.empty() && grammar::isIRowCodepoint(codepoints[end_pos - 1]) &&
          followsKanjiOrNominalHostBeforeCaseParticle(
              codepoints, start_pos, dict_manager,
              dict_manager == nullptr || start_pos == 0
                  ? nullptr
                  : lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos,
                                       core::PartOfSpeech::Particle))) {
        base_cost += candidate::verb_cost::kModerateBonus;
      }

      // Penalty for hiragana verb candidates containing auxiliary chains
      // Same as kanji verb penalties - て+auxiliary, causative, etc.
      // E.g., きなくなる should not win over でき+なく+なる
      if (vh::containsTeFormAuxPattern(surface)) {
        base_cost += bigram_cost::kStrong;
      }
      if (vh::containsCausativeAuxPattern(surface)) {
        base_cost += bigram_cost::kStrong;
      }
      // Penalty for negative auxiliary chains (なくなる = become unable to)
      if (utf8::contains(surface, "なくなる") || utf8::contains(surface, "なくなっ") ||
          utf8::contains(surface, "なくなり")) {
        base_cost += bigram_cost::kRare;
      }
      // Penalty for verb candidates absorbing auxiliary まい (negative volitional)
      // まい attaches to 終止形 as an independent AUX token: なるまい = なる + まい
      // Same rule as the kanji verb path (出来まい = 出来 + まい)
      if (utf8::endsWith(surface, "まい") && surface.size() > 2 * core::kJapaneseCharBytes) {
        base_cost += bigram_cost::kStrong;
      }

      // The conditional and classical-negative fast paths below must not
      // bypass the closed-class guards used by the ordinary candidate path.
      // In particular, やってみれば is やっ + て + みれ + ば, not a Godan
      // conditional whose stem happens to include てみ.
      const bool embeds_te_miru =
          vh::guardIsWired(vh::GuardMember::EmbedTeMiruAuxiliary, vh::GuardOrigin::HiraganaInflection) &&
          vh::embedsTeFormMiruAuxiliary(codepoints, start_pos, end_pos);
      const bool ends_with_focus_particle = vh::endsWithFocusParticleTail(dict_manager, codepoints, start_pos, end_pos);
      const bool is_exact_dictionary_verb = vh::hasDictionaryEntry(dict_manager, surface, core::PartOfSpeech::Verb);
      bool embeds_te_conditional_auxiliary = false;
      if (is_conditional && dict_manager != nullptr) {
        const size_t conditional_stem_end = end_pos - 1;
        for (size_t te_pos = start_pos + 1; te_pos + 1 < conditional_stem_end; ++te_pos) {
          if (codepoints[te_pos] == core::hiragana::kTe &&
              lookupEntryInRange(*dict_manager, codepoints, te_pos + 1, conditional_stem_end,
                                 core::PartOfSpeech::Verb) != nullptr) {
            embeds_te_conditional_auxiliary = true;
            break;
          }
        }
      }
      if (!is_exact_dictionary_verb &&
          (embeds_te_miru || ends_with_focus_particle || embeds_te_conditional_auxiliary)) {
        continue;
      }

      // A confident Godan analysis of e-row + ば identifies the productive
      // conditional boundary even when the pure-hiragana open-class lemma is
      // absent from L2 (くぐれ+ば). Keep the connective particle separate;
      // the row check prevents an arbitrary final えば sequence from being
      // promoted to a predicate.
      const auto* godan_row = grammar::Conjugation::getGodanRow(best.verb_type);
      if (is_conditional && godan_row != nullptr && end_pos >= start_pos + 3 &&
          godan_row->e_row == codepoints[end_pos - 2]) {
        const size_t stem_end = end_pos - 1;
        // The e-mora may belong to an auxiliary's own izenkei rather than to
        // the host verb (…ぎ+たれ+ば, …ら+ざれ+ば). Promoting the whole span
        // then fabricates a lemma out of the auxiliary, so leave the boundary
        // to the ordinary candidate path unless the base form is attested.
        // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
        if (vh::endsWithAuxiliaryAfterOkurigana(dict_manager, codepoints, start_pos, stem_end) &&
            !vh::isVerbInDictionary(dict_manager, best.base_form)) {
          continue;
        }
        candidates.push_back(makeVerbCandidate(
            extractSubstring(codepoints, start_pos, stem_end), start_pos, stem_end, candidate::verb_cost::kStrongBonus,
            best.base_form, grammar::verbTypeToConjType(best.verb_type), true, CandidateOrigin::VerbHiragana,
            best.confidence, "hiragana_godan_kateikei", core::ExtendedPOS::VerbKateikei));
        continue;
      }

      // The classical negative construction V未然形+ずに keeps the negative
      // auxiliary searchable. Inflection analysis also offers a fused
      // adverbial verb candidate, so replace it with the reconstructed stem
      // only when the following に closes this productive chain.
      if (end_pos < codepoints.size() && codepoints[end_pos] == U'に' && utf8::endsWith(surface, "ず") &&
          end_pos > start_pos + 1) {
        const size_t stem_end = end_pos - 1;
        const std::string stem_surface = extractSubstring(codepoints, start_pos, stem_end);
        candidates.push_back(makeVerbCandidate(stem_surface, start_pos, stem_end, candidate::verb_cost::kStrongBonus,
                                               best.base_form, grammar::verbTypeToConjType(best.verb_type), true,
                                               CandidateOrigin::VerbHiragana, best.confidence,
                                               "hiragana_mizenkei_before_zuni", core::ExtendedPOS::VerbMizenkei));
        continue;
      }

      // Set lemma from inflection analysis for pure hiragana verbs
      // This is essential for P4 (ひらがな動詞活用展開) to work without dictionary
      // The lemmatizer can't derive lemma accurately for unknown verbs
      const bool is_godan_dictionary_form = best.base_form == surface && grammar::isGodanVerbType(best.verb_type);
      const core::ExtendedPOS explicit_form = (looks_like_short_godan_base || is_godan_dictionary_form)
                                                  ? core::ExtendedPOS::VerbShuushikei
                                                  : core::ExtendedPOS::Unknown;
      // Keep the ordinary candidate path behind the same closed-class tail
      // guard as before. The conditional fast path above has an explicit
      // dictionary exemption; dropping this guard here lets a case particle
      // start a fabricated predicate such as でやる.
      if (!is_dictionary_verb &&
          verb_helpers::endsWithFocusParticleTail(dict_manager, codepoints, start_pos, end_pos)) {
        continue;
      }
      candidates.push_back(makeVerbCandidate(
          surface, start_pos, end_pos, base_cost, best.base_form, grammar::verbTypeToConjType(best.verb_type), false,
          CandidateOrigin::VerbHiragana, best.confidence, grammar::verbTypeToString(best.verb_type).data(),
          explicit_form, looks_like_short_godan_base ? "short_godan_base" : nullptr));
    }
  next_length:;  // Label for goto from particle-starting verb skip
  }
  return true;
}

}  // namespace hiragana_verb_detail

}  // namespace suzume::analysis
