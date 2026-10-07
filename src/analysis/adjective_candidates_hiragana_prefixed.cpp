/**
 * @file adjective_candidates_hiragana_prefixed.cpp
 * @brief Kanji i-adjective candidates opened by a lexical hiragana prefix
 */

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_absorption_guards.h"

namespace suzume::analysis {

using verb_helpers::embedsCaseParticle;

using adj_detail::makeIAdjCandidate;

namespace {

// Closed adjectival intensifiers that are particle-homographic.  They license
// a whole i-adjective candidate only when the lexical head is independently
// verified below; no standalone prefix edge is emitted, so the ordinary
// particle reading remains intact in frames such as か+どう+か.
constexpr std::array<std::string_view, 1> kParticleHomographicAdjectivalPrefixes = {"か"};

bool startsInsideMultiMoraParticle(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                   const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0 || end_pos <= start_pos) {
    return false;
  }
  constexpr size_t kMaxParticleChars = 4;
  const size_t earliest_start = lookbehindStart(start_pos, kMaxParticleChars);
  for (size_t particle_start = earliest_start; particle_start < start_pos; ++particle_start) {
    if (end_pos - particle_start < 2) {
      continue;
    }
    if (lookupEntryInRange(*dict_manager, codepoints, particle_start, end_pos, core::PartOfSpeech::Particle) !=
        nullptr) {
      return true;
    }
  }
  return false;
}

bool isParticleSequenceWithoutLexicalReading(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                             const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos >= end_pos) {
    return false;
  }
  if (hasExactPartOfSpeech(*dict_manager, codepoints, start_pos, end_pos, kOpenClassPartOfSpeechMask)) {
    return false;
  }
  return maximalSegmentCount(*dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Particle) > 0;
}

// A particle-spelled mora that closes a longer non-particle word belongs to
// that word, so it is no particle boundary in front of the prefix (the し of
// わたくし).
bool endsLongerContentWord(const std::vector<char32_t>& codepoints, size_t start_pos,
                           const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kMaxWordChars = 6;
  for (size_t word_start = lookbehindStart(start_pos, kMaxWordChars); word_start + 1 < start_pos; ++word_start) {
    const auto* entry = lookupEntryInRange(*dict_manager, codepoints, word_start, start_pos);
    if (entry != nullptr && entry->pos != core::PartOfSpeech::Particle) {
      return true;
    }
  }
  return false;
}

// A case or genitive particle opening the kana after the kanji marks an
// argument boundary: the kanji is a noun and the kana a phrase (やり場|の|ない).
bool tailOpensOnCaseParticle(const std::vector<char32_t>& codepoints, size_t kanji_end, size_t end_pos,
                             const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || kanji_end + 1 >= end_pos) {
    return false;
  }
  constexpr size_t kMaxParticleChars = 3;
  return hasDictionaryEntryFrom(dict_manager, codepoints, kanji_end, 1,
                                std::min(kMaxParticleChars, end_pos - kanji_end - 1), core::PartOfSpeech::Particle,
                                [](const dictionary::DictionaryEntry& entry) {
                                  return entry.extended_pos == core::ExtendedPOS::ParticleCase ||
                                         entry.extended_pos == core::ExtendedPOS::ParticleNo;
                                });
}

}  // namespace

void adj_detail::appendHiraganaPrefixedKanjiIAdjCandidates(std::vector<UnknownCandidate>& candidates,
                                                           const std::vector<char32_t>& codepoints, size_t start_pos,
                                                           const std::vector<normalize::CharType>& char_types,
                                                           const grammar::Inflection& inflection,
                                                           const dictionary::DictionaryManager* dict_manager) {
  // A one-to-three-mora hiragana prefix is otherwise indistinguishable from
  // an attached particle within a sentence (いまだ+に続く).  Restrict this
  // recovery path to a lexical word boundary; ordinary kanji adjective and
  // particle candidates retain responsibility inside a clause.
  const bool follows_particle = start_pos > 0 && dict_manager != nullptr &&
                                lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos,
                                                   core::PartOfSpeech::Particle) != nullptr &&
                                !endsLongerContentWord(codepoints, start_pos, dict_manager);
  if (start_pos > 0 && char_types[start_pos - 1] != normalize::CharType::Symbol && !follows_particle) {
    return;
  }
  const size_t prefix_end = findCharRegionEnd(char_types, start_pos, 3, normalize::CharType::Hiragana);
  if (prefix_end == start_pos || prefix_end >= char_types.size() ||
      char_types[prefix_end] != normalize::CharType::Kanji) {
    return;
  }
  // A multi-mora closed particle owns its complete span.  A kana prefix that
  // begins inside that span cannot start a new compound adjective (から+早く,
  // ながら+歩く, なら+置く).
  if (startsInsideMultiMoraParticle(codepoints, start_pos, prefix_end, dict_manager)) {
    return;
  }
  if (dict_manager != nullptr) {
    const size_t lookbehind = std::min<size_t>(3, start_pos);
    for (size_t particle_start = start_pos - lookbehind; particle_start < start_pos; ++particle_start) {
      for (size_t particle_end = start_pos + 1; particle_end <= prefix_end; ++particle_end) {
        if (lookupEntryInRange(*dict_manager, codepoints, particle_start, particle_end, core::PartOfSpeech::Particle) !=
            nullptr) {
          return;
        }
      }
    }
  }
  const std::string prefix_surface = extractSubstring(codepoints, start_pos, prefix_end);
  if (start_pos > 0 && isParticleSequenceWithoutLexicalReading(codepoints, start_pos, prefix_end, dict_manager)) {
    return;
  }
  if (dict_manager != nullptr) {
    // Honorific お/ご is an independent closed prefix.  It must not be folded
    // into a fabricated whole i-adjective spanning the following verb and the
    // beginning of its dependent auxiliary (お+答え+ください).  Lexical kana
    // adjective prefixes remain handled by the evidence checks below.
    // The same holds when the prefix only closes on one (に+お+任せ): the
    // honorific attaches to the following verb, not to the kana before it.
    for (size_t cell_start = start_pos; cell_start < prefix_end; ++cell_start) {
      if (lookupEntryInRange(*dict_manager, codepoints, cell_start, prefix_end, core::PartOfSpeech::Prefix) !=
          nullptr) {
        return;
      }
    }
    // A determiner joins the mask for the same reason: it modifies a noun
    // phrase from outside and never binds as an adjectival prefix, so a span
    // opening with one is a phrase boundary (その|薄暗い, この|小汚い).
    constexpr PartOfSpeechMask kFunctionWordMask = partOfSpeechMask(core::PartOfSpeech::Particle) |
                                                   partOfSpeechMask(core::PartOfSpeech::Auxiliary) |
                                                   partOfSpeechMask(core::PartOfSpeech::Determiner);
    const bool is_closed_particle_or_auxiliary = hasExactPartOfSpeech(*dict_manager, prefix_surface, kFunctionWordMask);
    const bool is_adjectival_prefix =
        std::find(kParticleHomographicAdjectivalPrefixes.begin(), kParticleHomographicAdjectivalPrefixes.end(),
                  prefix_surface) != kParticleHomographicAdjectivalPrefixes.end();
    if (is_closed_particle_or_auxiliary && !is_adjectival_prefix) {
      return;
    }
    // The whole-surface test above only sees a prefix that is itself one
    // function word. A closed cell just as often *closes* a longer kana run,
    // and the boundary then sits at the kanji: the cell predicates over what
    // precedes it and the kanji opens the next word (ような|白い, せず|立ちつくす,
    // しなく|続く). Only an inflected cell counts, on the same ground the rest of
    // this family uses — a base form is homographic with too many ordinary
    // words, while a cell whose surface differs from its lemma exists only
    // inside its paradigm. That is what separates these from the lexical
    // intensifiers, whose final mora is a word in its own right (the の of ほの,
    // the す of うす, the ら of そら).
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
    for (size_t cell_start = start_pos; cell_start < prefix_end; ++cell_start) {
      const auto* cell =
          lookupEntryInRange(*dict_manager, codepoints, cell_start, prefix_end, core::PartOfSpeech::Auxiliary);
      if (cell == nullptr) {
        cell = lookupEntryInRange(*dict_manager, codepoints, cell_start, prefix_end, core::PartOfSpeech::Particle);
      }
      // An empty lemma is the dictionary's shorthand for "same as the
      // surface", so it marks a base form exactly as an equal lemma does.
      if (cell != nullptr && !cell->lemma.empty() && cell->lemma != cell->surface) {
        return;
      }
    }
  }
  // る is the terminal and attributive ending of every conjugating class and
  // heads no word of its own, so a lone る in front of the kanji is the tail of
  // the predicate behind it rather than a prefix bound to what follows
  // (活け|る|白い菊, not 活け + the non-word る白い).
  if (prefix_end == start_pos + 1 && codepoints[start_pos] == U'る') {
    return;
  }
  const size_t kanji_end = findCharRegionEnd(char_types, prefix_end, 2, normalize::CharType::Kanji);
  if (kanji_end == prefix_end || kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return;
  }
  const size_t hiragana_end = findCharRegionEnd(char_types, kanji_end, 5, normalize::CharType::Hiragana);
  for (size_t end_pos = hiragana_end; end_pos > kanji_end; --end_pos) {
    // A case particle inside the run marks an argument boundary, so the kana
    // prefix is a preceding phrase rather than part of one compound adjective
    // (さきに + 食べとく, not the non-word さきに食べとい).
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
    if (embedsCaseParticle(dict_manager, codepoints, start_pos, end_pos) ||
        tailOpensOnCaseParticle(codepoints, kanji_end, end_pos, dict_manager)) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    const std::string tail_observed_surface = extractSubstring(codepoints, prefix_end, end_pos);
    // Recover both the adverbial -く form and a complete attributive form
    // before its lexical head.  A leading particle/auxiliary prefix was
    // rejected above, so compositional phrases such as から+美しい are not
    // absorbed into the compound candidate.
    // く spells both the adjective continuative and the Godan-ka terminal. A
    // continuative has to attach to something, so where nothing follows it the
    // terminal reading is the only one left and the kana in front is a separate
    // word (すぐ|着く, an adverb and a verb, not a cell of the non-word すぐ着い).
    const bool is_renyokei = utf8::endsWith(tail_observed_surface, "く") && end_pos < codepoints.size();
    const bool is_attributive = utf8::endsWith(tail_observed_surface, "い") && end_pos < codepoints.size() &&
                                (normalize::isKanjiCodepoint(codepoints[end_pos]) ||
                                 normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Katakana);
    if (!is_renyokei && !is_attributive) {
      continue;
    }
    const std::string tail_surface =
        is_renyokei ? normalize::replaceFinalChar(tail_observed_surface, "い") : std::string(tail_observed_surface);
    const auto& tail_candidates = inflection.analyze(tail_surface);
    const bool tail_has_i_adjective = adj_detail::firstConfidenceAtLeast(tail_candidates, grammar::VerbType::IAdjective,
                                                                         candidate::kCompoundAdjConfMin) != float{};
    const bool tail_has_verified_verb = adj_detail::hasDictionaryVerbAnalysis(tail_candidates, dict_manager);
    const auto& observed_tail_candidates = inflection.analyze(tail_observed_surface);
    const bool observed_tail_has_verified_verb =
        adj_detail::hasDictionaryVerbAnalysis(observed_tail_candidates, dict_manager);
    if (!tail_has_i_adjective || tail_has_verified_verb || observed_tail_has_verified_verb) {
      continue;
    }
    const std::string analysis_surface =
        is_renyokei ? normalize::replaceFinalChar(surface, "い") : std::string(surface);
    const auto& inflection_candidates = inflection.analyze(analysis_surface);
    const bool has_verified_verb_reading = adj_detail::hasDictionaryVerbAnalysis(inflection_candidates, dict_manager);
    if (has_verified_verb_reading) {
      continue;
    }
    for (const auto& inflection_candidate : inflection_candidates) {
      if (inflection_candidate.verb_type != grammar::VerbType::IAdjective ||
          inflection_candidate.confidence < candidate::kCompoundAdjConfMin) {
        continue;
      }
      float cost = adj_detail::lexicalCompoundIAdjCost(inflection_candidate.confidence);
      if (follows_particle) {
        cost += candidate::kPrefixedIAdjectiveAfterParticleBonus;
      }
      auto adjective = makeIAdjCandidate(surface, start_pos, end_pos, inflection_candidate.base_form, cost,
                                         CandidateOrigin::AdjectiveI, inflection_candidate.confidence,
                                         "hiragana_prefixed_kanji_i_adjective");
      adjective.has_suffix = true;
      candidates.push_back(std::move(adjective));
      return;
    }
  }
}

}  // namespace suzume::analysis
