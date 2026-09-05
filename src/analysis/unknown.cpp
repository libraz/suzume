/**
 * @file unknown.cpp
 * @brief Unknown word candidate generation orchestrator
 *
 * This file delegates specialized candidate generation to:
 * - suffix_candidates.h: Suffix-based and nominalized noun candidates
 * - adjective_candidates.h: I-adjective and na-adjective candidates
 * - verb_candidates.h: Verb and compound verb candidates
 */

#include "analysis/unknown.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "adjective_candidates.h"
#include "analysis/scorer_constants.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates.h"
#include "verb_candidates_helpers.h"

namespace {

void appendCandidates(std::vector<suzume::analysis::UnknownCandidate>& destination,
                      std::vector<suzume::analysis::UnknownCandidate>&& source) {
  destination.reserve(destination.size() + source.size());
  for (auto& candidate : source) {
    destination.push_back(std::move(candidate));
  }
}

// A closed function word is a hard lexical boundary. Unknown candidates may
// not consume its prefix (あく|まで, 本又|は), even when their own surface
// stops before the function word's final character.
bool spansConjunctionStart(const suzume::analysis::UnknownCandidate& candidate, const std::vector<char32_t>& codepoints,
                           const suzume::dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || candidate.end <= candidate.start + 1) {
    return false;
  }
  if (candidate.lemma_verified ||
      (candidate.pos == suzume::core::PartOfSpeech::Verb && !candidate.lemma.empty() &&
       dict_manager->lookupExact(candidate.lemma, suzume::core::PartOfSpeech::Verb) != nullptr)) {
    return false;
  }

  constexpr size_t kConjunctionWindowChars = 8;
  for (size_t boundary = candidate.start; boundary < candidate.end; ++boundary) {
    size_t window_end = std::min(codepoints.size(), boundary + kConjunctionWindowChars);
    for (size_t conjunction_end = boundary + 1; conjunction_end <= window_end; ++conjunction_end) {
      std::string conjunction = suzume::analysis::extractSubstring(codepoints, boundary, conjunction_end);
      const auto* conjunction_entry = dict_manager->lookupExact(conjunction, suzume::core::PartOfSpeech::Conjunction);
      const bool closed_function_word = conjunction_entry != nullptr;
      if (closed_function_word && boundary < candidate.end &&
          (boundary > candidate.start || conjunction_end > candidate.end)) {
        // A complete generated i-adjective may contain a kana-homographic
        // conjunction (慌ただしい contains ただし).  That is ordinary native
        // morphology.  A kanji-starting closed conjunction, by contrast,
        // owns its lexical start against an unattested adjective spanning in
        // from the left (本+若しくは, not 本若しく+は).
        if (candidate.pos == suzume::core::PartOfSpeech::Adjective &&
            candidate.origin == suzume::core::CandidateOrigin::AdjectiveI && !candidate.lemma.empty() &&
            suzume::normalize::classifyChar(codepoints[boundary]) != suzume::normalize::CharType::Kanji) {
          continue;
        }
        // A two-kanji content noun can overlap the first kanji of a
        // conjunction whose last mora is an independent case particle
        // (変更+に, not 変+更に).  Keep that noun candidate so the lattice can
        // evaluate the grammatical case boundary.  Topic-final conjunctions
        // such as 又は remain protected by the ordinary hard boundary below.
        const bool conjunction_leaves_one_case_particle =
            boundary + 1 == candidate.end && conjunction_end == candidate.end + 1;
        if (conjunction_entry != nullptr && conjunction_leaves_one_case_particle) {
          const std::string trailing = suzume::analysis::extractSubstring(codepoints, candidate.end, conjunction_end);
          const auto* particle = dict_manager->lookupExact(trailing, suzume::core::PartOfSpeech::Particle);
          if (particle != nullptr && particle->extended_pos == suzume::core::ExtendedPOS::ParticleCase) {
            continue;
          }
        }
        return true;
      }
    }
  }
  return false;
}

// A generic kanji noun candidate may end by consuming the stem kanji of a
// Godan continuative compound (確認申|し上げる). Preserve the boundary when the
// final kanji plus the following i-row ending reconstructs an attested verb
// and a second kanji verb follows. Dictionary verification prevents an
// arbitrary noun-final kanji followed by し from triggering the rule.
bool endsInsideVerifiedCompoundVerb(const suzume::analysis::UnknownCandidate& candidate,
                                    const std::vector<char32_t>& codepoints,
                                    const std::vector<suzume::normalize::CharType>& char_types,
                                    const suzume::dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || candidate.pos != suzume::core::PartOfSpeech::Noun ||
      candidate.end <= candidate.start + 1 || candidate.end + 1 >= codepoints.size() ||
      char_types[candidate.start] != suzume::normalize::CharType::Kanji ||
      char_types[candidate.end] != suzume::normalize::CharType::Hiragana ||
      char_types[candidate.end + 1] != suzume::normalize::CharType::Kanji) {
    return false;
  }

  const std::string_view base_suffix = suzume::grammar::godanBaseSuffixFromIRow(codepoints[candidate.end]);
  if (base_suffix.empty()) {
    return false;
  }
  const std::string verb_base = suzume::normalize::concat(
      suzume::analysis::extractSubstring(codepoints, candidate.end - 1, candidate.end), base_suffix);
  return dict_manager->lookupExact(verb_base, suzume::core::PartOfSpeech::Verb) != nullptr;
}

bool containsInternalPunctuation(const suzume::analysis::UnknownCandidate& candidate,
                                 const std::vector<char32_t>& codepoints) {
  if (candidate.pos == suzume::core::PartOfSpeech::Symbol) {
    return false;
  }
  for (size_t pos = candidate.start + 1; pos < candidate.end; ++pos) {
    if (suzume::normalize::classifyChar(codepoints[pos]) == suzume::normalize::CharType::Symbol &&
        codepoints[pos] != U'_' && !suzume::normalize::isVariationSelector(codepoints[pos]) &&
        !suzume::normalize::isTransparentFormatControl(codepoints[pos])) {
      return true;
    }
  }
  return false;
}

// A closed adverb followed by an attested adjective is a grammatical phrase,
// not an unknown all-kanji noun (比較的+安全).  Require dictionary evidence on
// both sides so ordinary lexical compounds remain untouched.
bool spansAdverbAdjectiveBoundary(const suzume::analysis::UnknownCandidate& candidate,
                                  const std::vector<char32_t>& codepoints,
                                  const suzume::dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || candidate.pos != suzume::core::PartOfSpeech::Noun ||
      candidate.end <= candidate.start + 1 ||
      candidate.end - candidate.start > suzume::analysis::candidate::kMaxAdverbAdjectiveBoundaryChars) {
    return false;
  }
  return suzume::analysis::hasDictionarySplit(
      *dict_manager, codepoints, candidate.start, candidate.end,
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Adverb),
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Adjective));
}

// A leading particle followed by a registered predicate remains compositional
// (も+よろしい). This is gated by the complete following dictionary predicate,
// so ordinary lexical words beginning with the same mora remain untouched.
bool startsWithParticleBeforeRegisteredPredicate(const suzume::analysis::UnknownCandidate& candidate,
                                                 const std::vector<char32_t>& codepoints,
                                                 const suzume::grammar::Inflection& inflection,
                                                 const suzume::dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || candidate.end <= candidate.start + 2 || candidate.lemma_verified ||
      (candidate.pos != suzume::core::PartOfSpeech::Verb && candidate.pos != suzume::core::PartOfSpeech::Adjective &&
       candidate.pos != suzume::core::PartOfSpeech::Noun)) {
    return false;
  }
  // A complete dictionary-form predicate whose slot was fixed from outside it
  // is not a particle plus a shorter predicate. The evidence is the case
  // particle in front of the candidate together with that particle's own host,
  // so it cannot be manufactured by the same kana the candidate is made of
  // (道 + を + とおる, 街 + に + でかける). Only the whole-span dictionary form
  // qualifies: a bound cell does not close the clause it would have to close.
  if (candidate.pos == suzume::core::PartOfSpeech::Verb && !candidate.lemma.empty() &&
      candidate.lemma == candidate.surface && candidate.start >= 2) {
    const size_t host_boundary = candidate.start - 1;
    const auto* slot_particle =
        dict_manager->lookupExact(suzume::analysis::extractSubstring(codepoints, host_boundary, candidate.start),
                                  suzume::core::PartOfSpeech::Particle);
    constexpr size_t kHostLookback = 12;
    constexpr suzume::analysis::PartOfSpeechMask kNominalHostMask =
        suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Noun) |
        suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Pronoun);
    const size_t min_host_start = host_boundary > kHostLookback ? host_boundary - kHostLookback : 0;
    if (slot_particle != nullptr && slot_particle->extended_pos == suzume::core::ExtendedPOS::ParticleCase &&
        (suzume::normalize::isKanjiCodepoint(codepoints[host_boundary - 1]) ||
         suzume::analysis::hasDictionaryEntryEndingAt(*dict_manager, codepoints, min_host_start, host_boundary,
                                                      kNominalHostMask))) {
      return false;
    }
  }

  const std::string particle = suzume::analysis::extractSubstring(codepoints, candidate.start, candidate.start + 1);
  const auto* particle_entry = dict_manager->lookupExact(particle, suzume::core::PartOfSpeech::Particle);
  if (particle_entry == nullptr || (particle_entry->extended_pos != suzume::core::ExtendedPOS::ParticleTopic &&
                                    particle_entry->extended_pos != suzume::core::ExtendedPOS::ParticleCase &&
                                    particle_entry->extended_pos != suzume::core::ExtendedPOS::ParticleNo)) {
    return false;
  }
  const std::string predicate = suzume::analysis::extractSubstring(codepoints, candidate.start + 1, candidate.end);
  const size_t predicate_length = candidate.end - candidate.start - 1;
  for (const auto& result : dict_manager->lookup(predicate, 0)) {
    if (result.entry == nullptr || result.length != predicate_length ||
        (result.entry->pos != suzume::core::PartOfSpeech::Verb &&
         result.entry->pos != suzume::core::PartOfSpeech::Adjective)) {
      continue;
    }
    // The predicate has to be complete, so the span must be the predicate's own
    // dictionary form. A bound inflectional cell does not close the clause it is
    // claimed to close, and a renyokei is spelled like the tail of any number of
    // nouns: となり matches the continuative of なる, and reading it that way
    // rewrites the noun as a particle plus half a verb — the analysis the
    // bracketed-noun rescue exists to outrank.
    if (!result.entry->lemma.empty() && result.entry->lemma != result.entry->surface) {
      continue;
    }
    return true;
  }

  // A leading closed particle may be followed by one complete content word
  // and then a complete predicate (の+ほか+冷たい).  Require both pieces to
  // be independently attested; this avoids treating particle-like kana at
  // the start of an ordinary native word as a boundary.
  for (size_t split = candidate.start + 2; split < candidate.end; ++split) {
    const std::string content = suzume::analysis::extractSubstring(codepoints, candidate.start + 1, split);
    constexpr suzume::analysis::PartOfSpeechMask kContentMask =
        suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Noun) |
        suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Pronoun) |
        suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Adverb);
    const bool content_verified = suzume::analysis::hasExactPartOfSpeech(*dict_manager, content, kContentMask);
    if (!content_verified) {
      continue;
    }
    const std::string tail = suzume::analysis::extractSubstring(codepoints, split, candidate.end);
    constexpr suzume::analysis::PartOfSpeechMask kPredicateMask =
        suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Verb) |
        suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Adjective);
    if (suzume::analysis::hasExactPartOfSpeech(*dict_manager, tail, kPredicateMask)) {
      return true;
    }
    const auto best = inflection.getBest(tail);
    if (best.confidence >= suzume::analysis::candidate::verb_cost::kConstructedVerbMinConfidence &&
        best.verb_type != suzume::grammar::VerbType::Unknown) {
      return true;
    }
  }
  return false;
}

bool hasDictionaryContentEndingAt(const std::vector<char32_t>& codepoints, size_t boundary,
                                  const suzume::dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || boundary == 0) {
    return false;
  }
  constexpr size_t kContentLookback = 4;
  const size_t first = boundary > kContentLookback ? boundary - kContentLookback : 0;
  constexpr suzume::analysis::PartOfSpeechMask kContentMask =
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Noun) |
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Pronoun) |
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Adverb) |
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Adjective) |
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Verb);
  return suzume::analysis::hasDictionaryEntryEndingAt(*dict_manager, codepoints, first, boundary, kContentMask);
}

bool isGeneratedPredicate(const std::vector<char32_t>& codepoints, size_t start, size_t end,
                          const suzume::grammar::Inflection& inflection,
                          const suzume::dictionary::DictionaryManager* dict_manager) {
  if (start >= end) {
    return false;
  }
  const std::string surface = suzume::analysis::extractSubstring(codepoints, start, end);
  constexpr suzume::analysis::PartOfSpeechMask kPredicateMask =
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Verb) |
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Adjective);
  if (suzume::analysis::hasExactPartOfSpeech(*dict_manager, surface, kPredicateMask)) {
    return true;
  }
  const auto best = inflection.getBest(surface);
  return best.confidence >= suzume::analysis::candidate::verb_cost::kConstructedVerbMinConfidence &&
         best.verb_type != suzume::grammar::VerbType::Unknown &&
         best.verb_type != suzume::grammar::VerbType::IAdjective;
}

bool isKanjiContentSpan(const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  if (start >= end) {
    return false;
  }
  return std::all_of(codepoints.begin() + static_cast<std::ptrdiff_t>(start),
                     codepoints.begin() + static_cast<std::ptrdiff_t>(end), [](char32_t codepoint) {
                       return suzume::normalize::classifyChar(codepoint) == suzume::normalize::CharType::Kanji;
                     });
}

bool hasClosedFollowerForGeneratedVerb(const suzume::analysis::UnknownCandidate& candidate,
                                       const std::vector<char32_t>& codepoints,
                                       const suzume::dictionary::DictionaryManager* dict_manager) {
  if (candidate.pos != suzume::core::PartOfSpeech::Verb || candidate.end >= codepoints.size()) {
    return false;
  }
  const std::string remaining = suzume::analysis::extractSubstring(codepoints, candidate.end, codepoints.size());
  for (const auto& result : dict_manager->lookup(remaining, 0)) {
    if (result.entry == nullptr) {
      continue;
    }
    if (candidate.extended_pos == suzume::core::ExtendedPOS::VerbRenyokei &&
        result.entry->extended_pos == suzume::core::ExtendedPOS::AuxAspectHajimeru) {
      return true;
    }
    if (candidate.extended_pos == suzume::core::ExtendedPOS::VerbMizenkei &&
        (result.entry->extended_pos == suzume::core::ExtendedPOS::AuxPassive ||
         result.entry->extended_pos == suzume::core::ExtendedPOS::AuxCausative ||
         result.entry->extended_pos == suzume::core::ExtendedPOS::AuxPotential)) {
      return true;
    }
  }
  return false;
}

// A speculative predicate cannot absorb an internal case particle when a
// complete dictionary content word ends at that boundary and a productive
// predicate follows (よそ+に+置く, 水+が+あふれ). The two-sided gate
// also removes candidates that begin inside the left word (よ+そに置く),
// while lemma-verified lexical verbs and particle-like kana inside native
// words remain untouched.
bool spansCaseParticleBeforeVerifiedPredicate(const suzume::analysis::UnknownCandidate& candidate,
                                              const std::vector<char32_t>& codepoints,
                                              const suzume::grammar::Inflection& inflection,
                                              const suzume::dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || candidate.start + 2 >= candidate.end || candidate.lemma_verified ||
      (candidate.pos != suzume::core::PartOfSpeech::Verb && candidate.pos != suzume::core::PartOfSpeech::Adjective)) {
    return false;
  }
  for (size_t boundary = candidate.start + 1; boundary < candidate.end; ++boundary) {
    // A single mora after a particle-like kana is commonly just part of the
    // okurigana (積もり).  The compositional predicate side must contain at
    // least two moras (水+が+あふれ、友+と+わか).
    if (boundary + 2 >= candidate.end) {
      continue;
    }
    // A bound derivational suffix owns its own opening mora, so the kana there
    // is not a particle at all (押しつけ+がまし+さ). Without this the guard
    // reads the suffix as 押しつけ+が+まし and refuses the derived adjective.
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (suzume::analysis::verb_helpers::startsInsideGaMashiiSuffix(codepoints, boundary)) {
      continue;
    }
    const std::string particle = suzume::analysis::extractSubstring(codepoints, boundary, boundary + 1);
    const auto* entry = dict_manager->lookupExact(particle, suzume::core::PartOfSpeech::Particle);
    if (entry == nullptr || (entry->extended_pos != suzume::core::ExtendedPOS::ParticleCase &&
                             entry->extended_pos != suzume::core::ExtendedPOS::ParticleTopic &&
                             entry->extended_pos != suzume::core::ExtendedPOS::ParticleNo)) {
      continue;
    }
    const bool left_content = hasDictionaryContentEndingAt(codepoints, boundary, dict_manager) ||
                              (candidate.pos == suzume::core::PartOfSpeech::Verb &&
                               isKanjiContentSpan(codepoints, candidate.start, boundary));
    const bool right_predicate =
        isGeneratedPredicate(codepoints, boundary + 1, candidate.end, inflection, dict_manager) ||
        hasClosedFollowerForGeneratedVerb(candidate, codepoints, dict_manager);
    if (left_content && right_predicate) {
      return true;
    }
  }
  return false;
}

// A speculative candidate beginning inside a contiguous kanji noun cannot
// absorb the complete closed negative ない (問+題ない versus 問題+ない).
// Restrict this to that closed surface so productive suffixes remain intact.
bool startsInsideKanjiRunBeforeClosedNai(const suzume::analysis::UnknownCandidate& candidate,
                                         const std::vector<char32_t>& codepoints) {
  if (candidate.lemma_verified || candidate.start == 0 || candidate.end <= candidate.start + 2 ||
      suzume::normalize::classifyChar(codepoints[candidate.start - 1]) != suzume::normalize::CharType::Kanji ||
      suzume::normalize::classifyChar(codepoints[candidate.start]) != suzume::normalize::CharType::Kanji) {
    return false;
  }
  size_t boundary = candidate.start;
  while (boundary < candidate.end &&
         suzume::normalize::classifyChar(codepoints[boundary]) == suzume::normalize::CharType::Kanji) {
    ++boundary;
  }
  return boundary > candidate.start && boundary < candidate.end &&
         suzume::analysis::extractSubstring(codepoints, boundary, candidate.end) == "ない";
}

// The first mora of the closed negative-past auxiliary belongs to なかっ,
// never to the preceding speculative predicate (逃さ|なかっ, 強く|は|なかっ).
bool endsInsideNegativePast(const suzume::analysis::UnknownCandidate& candidate,
                            const std::vector<char32_t>& codepoints) {
  return candidate.end >= candidate.start + 2 && candidate.end + 1 < codepoints.size() &&
         codepoints[candidate.end - 1] == U'な' && codepoints[candidate.end] == U'か' &&
         codepoints[candidate.end + 1] == U'っ';
}

bool fusesPastAuxiliary(const suzume::analysis::UnknownCandidate& candidate, const std::vector<char32_t>& codepoints,
                        const suzume::dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || candidate.end < candidate.start + 2 ||
      (codepoints[candidate.end - 1] != U'た' && codepoints[candidate.end - 1] != U'だ')) {
    return false;
  }
  if (candidate.pos == suzume::core::PartOfSpeech::Verb &&
      candidate.extended_pos == suzume::core::ExtendedPOS::VerbTaForm) {
    // A repeated vowel after a finite past form is a separate colloquial
    // emphasis unit (きた+ああああ), not evidence that た was swallowed from
    // an auxiliary chain. Keep the finite candidate available at that boundary.
    if (candidate.end + 1 < codepoints.size() && codepoints[candidate.end] == codepoints[candidate.end + 1] &&
        suzume::analysis::verb_helpers::getHiraganaVowel(codepoints[candidate.end]) == codepoints[candidate.end]) {
      return false;
    }
    return true;
  }
  const std::string prefix = suzume::analysis::extractSubstring(codepoints, candidate.start, candidate.end - 1);
  constexpr suzume::analysis::PartOfSpeechMask kPredicateMask =
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Verb) |
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Adjective) |
      suzume::analysis::partOfSpeechMask(suzume::core::PartOfSpeech::Auxiliary);
  return suzume::analysis::hasExactPartOfSpeech(*dict_manager, prefix, kPredicateMask);
}

bool fusesPassivePastAsNoun(const suzume::analysis::UnknownCandidate& candidate) {
  return candidate.pos == suzume::core::PartOfSpeech::Noun &&
         candidate.origin == suzume::core::CandidateOrigin::KanjiHiraganaCompound &&
         utf8::endsWithAny(candidate.surface, {"れた", "られた", "された", "せられた"});
}

bool endsInsideIchidanPassive(const suzume::analysis::UnknownCandidate& candidate,
                              const std::vector<char32_t>& codepoints) {
  if (candidate.end < candidate.start + 3 || candidate.end >= codepoints.size() ||
      codepoints[candidate.end - 1] != U'ら' || codepoints[candidate.end] != U'れ') {
    return false;
  }
  const char32_t preceding = codepoints[candidate.end - 2];
  return suzume::grammar::isIRowCodepoint(preceding) || suzume::grammar::isERowCodepoint(preceding);
}

}  // namespace

namespace suzume::analysis {

UnknownCandidate makeVerbCandidate(const std::string& surface, size_t start, size_t end, float cost,
                                   const std::string& lemma, dictionary::ConjugationType conj_type, bool has_suffix,
                                   CandidateOrigin origin, [[maybe_unused]] float confidence,
                                   [[maybe_unused]] const char* pattern, core::ExtendedPOS extended_pos,
                                   [[maybe_unused]] const char* epos_source) {
  UnknownCandidate candidate;
  candidate.surface = surface;
  candidate.start = start;
  candidate.end = end;
  candidate.pos = core::PartOfSpeech::Verb;
  const bool godan_i_onbin_hint =
      conj_type == dictionary::ConjugationType::GodanKa || conj_type == dictionary::ConjugationType::GodanGa;
  candidate.extended_pos = extended_pos != core::ExtendedPOS::Unknown
                               ? extended_pos
                               : core::detectVerbForm(surface, {}, false, godan_i_onbin_hint);
  candidate.cost = cost;
  candidate.lemma = lemma;
  candidate.conj_type = conj_type;
  candidate.has_suffix = has_suffix;
  candidate.origin = origin;
#ifdef SUZUME_DEBUG_INFO
  candidate.confidence = confidence;
  if (pattern != nullptr) {
    candidate.pattern = pattern;
  }
  if (epos_source != nullptr) {
    candidate.epos_source = epos_source;
  } else if (extended_pos != core::ExtendedPOS::Unknown) {
    candidate.epos_source = "verb_cand_explicit";
  } else {
    candidate.epos_source = "verb_cand_auto";
  }
#endif
  return candidate;
}

UnknownCandidate makeNounCandidate(const std::string& surface, size_t start, size_t end, float cost, bool has_suffix,
                                   CandidateOrigin origin, core::ExtendedPOS extended_pos,
                                   [[maybe_unused]] const char* epos_source) {
  UnknownCandidate candidate;
  candidate.surface = surface;
  candidate.start = start;
  candidate.end = end;
  candidate.pos = core::PartOfSpeech::Noun;
  candidate.extended_pos = extended_pos != core::ExtendedPOS::Unknown ? extended_pos : core::ExtendedPOS::Noun;
  candidate.cost = cost;
  candidate.has_suffix = has_suffix;
  candidate.origin = origin;
#ifdef SUZUME_DEBUG_INFO
  if (epos_source != nullptr) {
    candidate.epos_source = epos_source;
  } else if (extended_pos != core::ExtendedPOS::Unknown) {
    candidate.epos_source = "noun_cand_explicit";
  } else {
    candidate.epos_source = "noun_cand_default";
  }
#endif
  return candidate;
}

UnknownCandidate makeCandidate(const std::string& surface, size_t start, size_t end, core::PartOfSpeech pos, float cost,
                               bool has_suffix, CandidateOrigin origin, core::ExtendedPOS extended_pos,
                               [[maybe_unused]] const char* epos_source) {
  UnknownCandidate candidate;
  candidate.surface = surface;
  candidate.start = start;
  candidate.end = end;
  candidate.pos = pos;
  candidate.extended_pos = extended_pos != core::ExtendedPOS::Unknown ? extended_pos : core::posToExtendedPos(pos);
  candidate.cost = cost;
  candidate.has_suffix = has_suffix;
  candidate.origin = origin;
#ifdef SUZUME_DEBUG_INFO
  if (epos_source != nullptr) {
    candidate.epos_source = epos_source;
  } else if (extended_pos != core::ExtendedPOS::Unknown) {
    candidate.epos_source = "make_cand_explicit";
  } else {
    candidate.epos_source = "make_cand_default";
  }
#endif
  return candidate;
}

UnknownCandidate makeVerbCandidate(const std::vector<char32_t>& codepoints, size_t start, size_t end, float cost,
                                   const std::string& lemma, dictionary::ConjugationType conj_type, bool has_suffix,
                                   CandidateOrigin origin, float confidence, const char* pattern,
                                   core::ExtendedPOS extended_pos, const char* epos_source) {
  return makeVerbCandidate(extractSubstring(codepoints, start, end), start, end, cost, lemma, conj_type, has_suffix,
                           origin, confidence, pattern, extended_pos, epos_source);
}

UnknownCandidate makeCandidate(const std::vector<char32_t>& codepoints, size_t start, size_t end,
                               core::PartOfSpeech pos, float cost, bool has_suffix, CandidateOrigin origin,
                               core::ExtendedPOS extended_pos, const char* epos_source) {
  return makeCandidate(extractSubstring(codepoints, start, end), start, end, pos, cost, has_suffix, origin,
                       extended_pos, epos_source);
}

UnknownWordGenerator::UnknownWordGenerator(const UnknownOptions& options,
                                           const dictionary::DictionaryManager* dict_manager)
    : options_(options), dict_manager_(dict_manager), inflection_(options.inflection_scorer_options) {}

size_t UnknownWordGenerator::getMaxLength(normalize::CharType ctype) const {
  switch (ctype) {
    case normalize::CharType::Kanji:
      return options_.max_kanji_length;
    case normalize::CharType::Katakana:
      return options_.max_katakana_length;
    case normalize::CharType::Hiragana:
      return options_.max_hiragana_length;
    case normalize::CharType::Alphabet:
      return options_.max_alphabet_length;
    case normalize::CharType::Digit:
      return options_.max_alphanumeric_length;
    default:
      return options_.max_unknown_length;
  }
}

core::PartOfSpeech UnknownWordGenerator::getPosForType(normalize::CharType ctype) {
  switch (ctype) {
    case normalize::CharType::Kanji:
    case normalize::CharType::Katakana:
    case normalize::CharType::Alphabet:
    case normalize::CharType::Digit:
      return core::PartOfSpeech::Noun;
    case normalize::CharType::Hiragana:
      return core::PartOfSpeech::Other;
    case normalize::CharType::Emoji:
      // Emoji are text-bearing input and therefore remain OTHER for full
      // offset coverage. preserve_symbols controls punctuation-like SYMBOL
      // tokens; it must not make emoji disappear by default.
      return core::PartOfSpeech::Other;
    case normalize::CharType::Symbol:
      return core::PartOfSpeech::Symbol;
    case normalize::CharType::Unknown:
    default:
      // Preserve unclassified Unicode text by default. A future character
      // class omission may produce a coarse POS, but must never make user
      // input disappear when remove_symbols is enabled.
      return core::PartOfSpeech::Other;
  }
}

float UnknownWordGenerator::getCostForType(normalize::CharType ctype, size_t length) {
  float base_cost = 1.0F;

  switch (ctype) {
    case normalize::CharType::Kanji:
      // Kanji: 2-6 characters are common compound word lengths
      // Keep compounds connected by default - splitting should be driven by
      // PREFIX/SUFFIX markers or dictionary entries, not length heuristics
      // E.g., 自然言語処理 should stay as one token (no evidence to split)
      // But 今夏最高 splits because 今 is marked as PREFIX
      if (length == 1) {
        return base_cost + 0.4F;  // 1.4: prefer over suffix entries (1.5)
      }
      // 2+ chars: all valid compound lengths, no penalty
      // Long compounds like 独立行政法人情報処理推進機構 should stay as one token
      return base_cost;

    case normalize::CharType::Katakana:
      // Katakana: prefer 4+ characters for loanwords (マスカラ, デスクトップ)
      // Penalize short sequences to prevent splits like マ+スカラ
      if (length == 1) {
        return base_cost + 1.5F;  // Strong penalty for 1-char
      }
      if (length == 2) {
        return base_cost + 1.0F;  // Moderate penalty for 2-char
      }
      if (length == 3) {
        return base_cost + 0.3F;  // Light penalty for 3-char
      }
      if (length >= 4 && length <= 10) {
        return base_cost;  // Optimal: 4-10 chars
      }
      return base_cost + 0.3F;  // 11+ chars: light penalty

    case normalize::CharType::Alphabet:
      // Alphabet: prefer longer sequences for identifiers/words
      // Longer sequences (like "getUserData") should not be penalized
      if (length >= 2 && length <= 20) {
        // Give bonus to longer sequences to prefer them over splits
        // This helps keep "getUserData" together vs "getUser" + "Data"
        float length_bonus = (length >= 8) ? -0.3F : 0.0F;
        return base_cost + 0.2F + length_bonus;
      }
      return base_cost + 0.5F;

    case normalize::CharType::Digit:
      // Digits: always reasonable
      return base_cost - 0.2F;

    case normalize::CharType::Hiragana:
      // Hiragana only: usually function words
      // 1-char: high cost (almost always a particle/aux from L1 dict)
      // 2-3 char: moderate cost to compete with particle chains
      //   e.g., はし(1.4) vs は(0.2)+し(0.2)+conn(0.5)=0.9 — still loses,
      //   but closer, and bigram penalties on bad connections can tip it
      // 4+: increasing penalty to force segmentation
      if (length == 1) {
        return base_cost + 1.0F;  // 2.0: original cost
      }
      if (length <= 3) {
        return base_cost + 0.4F;  // 1.4: compete with particle chains
      }
      return base_cost + 0.5F + (static_cast<float>(length) - 3.0F) * 0.5F;

    default:
      return base_cost + 1.5F;
  }
}

std::vector<UnknownCandidate> UnknownWordGenerator::generate(std::string_view text,
                                                             const std::vector<char32_t>& codepoints, size_t start_pos,
                                                             const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size()) {
    return candidates;
  }

  // Generate ABAB-type onomatopoeia candidates first (わくわく, きらきら, etc.)
  // This needs to be checked before isNeverVerbStemAtStart filters out わ, etc.
  // Also handles katakana patterns (ニャーニャー, ワンワン, etc.)
  if (char_types[start_pos] == normalize::CharType::Hiragana ||
      char_types[start_pos] == normalize::CharType::Katakana) {
    generateOnomatopoeiaCandidates(codepoints, start_pos, char_types, candidates);
  }

  // Generate verb candidates (kanji + hiragana conjugation endings)
  if (char_types[start_pos] == normalize::CharType::Kanji) {
    analysis::generateVerbCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_,
                                     options_.verb_candidate_options, candidates);

    // Generate compound verb candidates (kanji + hiragana + kanji + hiragana)
    // e.g., 恐れ入ります, 差し上げます, 申し上げます
    analysis::generateCompoundVerbCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_,
                                             options_.verb_candidate_options, candidates);

    // Generate i-adjective candidates (kanji + hiragana conjugation endings)
    analysis::generateAdjectiveCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_, candidates);

    // Generate i-adjective STEM candidates (難し, 美し for 難しそう, 美しすぎる)
    // This preserves the adjective stem and appearance-auxiliary boundary.
    analysis::generateAdjectiveStemCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_,
                                              candidates);

    // Generate productive nominal-base suffix verbs (春めく、謎めく).
    generateProductiveSuffixVerbCandidates(codepoints, start_pos, char_types, candidates);

    // Generate na-adjective candidates (〜的 patterns)
    analysis::generateNaAdjectiveCandidates(codepoints, start_pos, char_types, options_, dict_manager_, candidates);

    // Generate nominalized noun candidates (kanji + short hiragana)
    // e.g., 手助け, 片付け, 引き上げ
    analysis::generateNominalizedNounCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_,
                                                candidates);

    // Generate kanji + hiragana compound noun candidates
    // e.g., 玉ねぎ, 水たまり
    // Pass dict_manager to skip compounds when hiragana portion is a known word
    generateKanjiHiraganaCompoundCandidates(codepoints, start_pos, char_types, dict_manager_, candidates);

    // Generate counter candidates for numeral + つ patterns
    // e.g., 一つ, 二つ, ..., 九つ (closed class)
    generateCounterCandidates(codepoints, start_pos, char_types, dict_manager_, candidates);

    // Generate prefix + single kanji compound candidates
    // e.g., 今日, 今週, 本日, 全国 (prefix-like compounds)
    generatePrefixCompoundCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_, candidates);

    // Generate temporal-noun boundary split candidates (現在|担当者, 昨日|会議)
    generateTemporalNounBoundaryCandidates(codepoints, start_pos, char_types, candidates);
  }

  // Generate the deverbal nominal of the humble 敬語接頭辞 + V連用形 + する frame
  // (お伝えする, おかけする). Script-independent: the frame is delimited by the
  // honorific prefix and the suru auxiliary, not by the stem's script.
  analysis::generateHumbleNominalCandidates(codepoints, start_pos, inflection_, dict_manager_, candidates);

  // Generate reciprocal-action deverbal nouns (にらめっこ, かけっこ)
  analysis::generateReciprocalActionNounCandidates(codepoints, start_pos, char_types, dict_manager_, candidates);

  // Generate hiragana verb candidates (pure hiragana verbs like いく, くる)
  if (char_types[start_pos] == normalize::CharType::Hiragana) {
    appendCandidates(candidates,
                     analysis::generateHiraganaVerbCandidates(codepoints, start_pos, char_types, inflection_,
                                                              dict_manager_, options_.verb_candidate_options));
    analysis::generateNaAdjectiveCandidates(codepoints, start_pos, char_types, options_, dict_manager_, candidates);

    // Generate hiragana i-adjective candidates (まずい, おいしい, etc.)
    analysis::generateHiraganaAdjectiveCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_,
                                                  candidates);

    // Generate productive suffix candidates (ありがち, 忘れっぽい, etc.)
    generateProductiveSuffixCandidates(codepoints, start_pos, char_types, candidates);

    // Generate finite kana NounNumber + quantitative Suffix search units.
    generateCounterCandidates(codepoints, start_pos, char_types, dict_manager_, candidates);
  }

  // Generate katakana verb/adjective candidates (slang: バズる, エモい, etc.)
  if (char_types[start_pos] == normalize::CharType::Katakana) {
    analysis::generateKatakanaVerbCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_,
                                             options_.verb_candidate_options, candidates);

    analysis::generateKatakanaAdjectiveCandidates(codepoints, start_pos, char_types, inflection_, candidates);
  }

  // Generate counter candidates for digit + つ patterns (e.g., 3つ, 10個)
  if (char_types[start_pos] == normalize::CharType::Digit) {
    generateCounterCandidates(codepoints, start_pos, char_types, dict_manager_, candidates);
  }

  // Generate by same type
  generateBySameType(codepoints, start_pos, char_types, candidates);

  // The closed adjective suffix がまし〜 can follow a mixed-script nominal host
  // that the ordinary leading-kanji adjective scan cannot cross.
  generateGaMashiiHostAdjectiveCandidates(codepoints, start_pos, char_types, inflection_, candidates);

  // Generate only the noun head selected by a verified genitive, determiner,
  // or attributive i-adjective and closed by a nominal particle.
  generateSelectedNominalHeadCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_, candidates);

  // Generate alphanumeric sequences
  generateAlphanumeric(text, codepoints, start_pos, char_types, candidates);

  // Generate with suffix separation for kanji
  if (options_.separate_suffix && char_types[start_pos] == normalize::CharType::Kanji) {
    analysis::generateWithSuffix(codepoints, start_pos, char_types, options_, candidates);
  }

  // Generate character speech candidates (キャラ語尾)
  if (options_.enable_character_speech) {
    generateCharacterSpeechCandidates(text, codepoints, start_pos, char_types, candidates);
  }

  // Generate the colloquial contraction of the hypothetical (行きゃ, 食べりゃ).
  // Script-independent: the fused mora is kana whatever the stem's script is.
  // Runs last because it stands down on any span another generator already
  // claimed — a subsidiary auxiliary reads the same contraction with context
  // this reconstruction cannot see (読んで + みりゃ).
  analysis::generateContractedConditionalCandidates(codepoints, start_pos, char_types, inflection_, dict_manager_,
                                                    candidates);

  candidates.erase(
      std::remove_if(
          candidates.begin(), candidates.end(),
          [&](const UnknownCandidate& candidate) {
            if (spansConjunctionStart(candidate, codepoints, dict_manager_)) {
              return true;
            }
            if (containsInternalPunctuation(candidate, codepoints)) {
              return true;
            }
            if (spansAdverbAdjectiveBoundary(candidate, codepoints, dict_manager_)) {
              return true;
            }
            if (endsInsideNegativePast(candidate, codepoints) ||
                fusesPastAuxiliary(candidate, codepoints, dict_manager_) || fusesPassivePastAsNoun(candidate) ||
                endsInsideIchidanPassive(candidate, codepoints)) {
              return true;
            }
            // SelectedNominalHead has already proved both nominal
            // boundaries. Its first kana may still be homographic
            // with a particle, so the leading-particle filter does
            // not get to reinterpret that same evidence.
            const bool has_verified_nominal_boundaries = candidate.origin == CandidateOrigin::SelectedNominalHead;
            if (!has_verified_nominal_boundaries &&
                startsWithParticleBeforeRegisteredPredicate(candidate, codepoints, inflection_, dict_manager_)) {
              return true;
            }
            if (spansCaseParticleBeforeVerifiedPredicate(candidate, codepoints, inflection_, dict_manager_)) {
              return true;
            }
            if (startsInsideKanjiRunBeforeClosedNai(candidate, codepoints)) {
              return true;
            }
            return endsInsideVerifiedCompoundVerb(candidate, codepoints, char_types, dict_manager_);
          }),
      candidates.end());

  return candidates;
}

void UnknownWordGenerator::generateAlphanumeric(std::string_view /*text*/, const std::vector<char32_t>& codepoints,
                                                size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                                std::vector<UnknownCandidate>& candidates) const {
  if (start_pos >= char_types.size()) {
    return;
  }

  normalize::CharType start_type = char_types[start_pos];

  // Only for alphabet or digit start
  if (start_type != normalize::CharType::Alphabet && start_type != normalize::CharType::Digit) {
    return;
  }

  // Find mixed alphanumeric sequence (including underscores for identifiers)
  // Supports snake_case identifiers like user_name, first_name, etc.
  size_t end_pos = start_pos;
  bool has_alpha = false;
  bool has_digit = false;
  bool has_underscore = false;

  while (end_pos < char_types.size() && end_pos - start_pos < options_.max_alphanumeric_length) {
    normalize::CharType ctype = char_types[end_pos];
    char32_t ch = codepoints[end_pos];
    if (ctype == normalize::CharType::Alphabet) {
      has_alpha = true;
      ++end_pos;
    } else if (ctype == normalize::CharType::Digit) {
      has_digit = true;
      ++end_pos;
    } else if (ch == U'_') {
      // Include underscore in identifier patterns
      // Only if followed by alphanumeric (avoid trailing underscore)
      if (end_pos + 1 < char_types.size()) {
        normalize::CharType next_type = char_types[end_pos + 1];
        if (next_type == normalize::CharType::Alphabet || next_type == normalize::CharType::Digit) {
          has_underscore = true;
          ++end_pos;
          continue;
        }
      }
      break;
    } else {
      break;
    }
  }

  // Generate candidate if mixed alphanumeric OR identifier with underscore
  // Pure alpha/digit sequences are handled by generateBySameType
  bool is_mixed = has_alpha && has_digit;
  bool is_identifier = has_underscore && (has_alpha || has_digit);
  if ((is_mixed || is_identifier) && end_pos > start_pos + 1) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    if (!surface.empty()) {
      // Give identifiers with underscores a bonus to prefer them over splits
      float cost = is_identifier ? 0.5F : 0.8F;
      auto cand = makeCandidate(surface, start_pos, end_pos, core::PartOfSpeech::Noun, cost, false,
                                CandidateOrigin::Alphanumeric);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 1.0F;
      cand.pattern = is_identifier ? "identifier" : "alphanum_mixed";
#endif
      candidates.push_back(cand);
    }
  }
}

}  // namespace suzume::analysis
