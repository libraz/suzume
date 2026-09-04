/**
 * @file suffix_candidates_compound.cpp
 * @brief Suffix-based unknown word candidate generation
 */

#include <algorithm>

#include "adjective_candidates.h"
#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

bool hasNominalPhraseSelectorAt(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t pos) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  const auto* particle = lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Particle);
  if (particle != nullptr && (particle->extended_pos == core::ExtendedPOS::ParticleCase ||
                              particle->extended_pos == core::ExtendedPOS::ParticleTopic ||
                              particle->extended_pos == core::ExtendedPOS::ParticleNo)) {
    return true;
  }
  const auto* auxiliary = lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Auxiliary);
  return auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxCopulaDa;
}

// A particle-like kana may be the final mora of an open-class content noun. If
// the mixed span stops immediately before it and a real nominal selector follows
// it, treating the stranded mora as a particle creates an impossible particle
// stack (組みひ|も|を). Reject that cut so the lattice can instead place the
// content-word boundary to its left (組み|ひも|を).
bool strandsParticleLikeMoraBeforeNominalSelector(const dictionary::DictionaryManager* dict_manager,
                                                  const std::vector<char32_t>& codepoints, size_t candidate_end) {
  return candidate_end + 1 < codepoints.size() && normalize::isParticleCodepoint(codepoints[candidate_end]) &&
         hasNominalPhraseSelectorAt(dict_manager, codepoints, candidate_end + 1);
}

// A particle and a determiner are both fixed forms of a closed class: neither
// derives from anything or inflects, so neither has an interior a word boundary
// could fall on.
bool isUninflectedClosedClass(const dictionary::DictionaryEntry& entry) {
  return entry.pos == core::PartOfSpeech::Particle || entry.pos == core::PartOfSpeech::Determiner;
}

/**
 * @brief Whether the span boundary falls inside a fixed closed-class word.
 *
 * A compound noun cannot end part-way through one: 読まれども is 読ま + れ +
 * ども, so a span reaching only 読まれど has cut the concessive conjunction in
 * half and owes its score to that cut. A determiner is cut the same way when it
 * follows a predicate, because its first mora completes a plausible mixed-script
 * span and its second is a particle on its own (走るそ|の for 走る + その).
 */
bool boundarySplitsClosedClassWord(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t kanji_end, size_t end_pos) {
  if (dict_manager == nullptr || end_pos >= codepoints.size()) {
    return false;
  }
  const size_t scan_start = (end_pos > kanji_end + 1) ? end_pos - 2 : kanji_end;
  const size_t probe_end = std::min(codepoints.size(), end_pos + 2);
  for (size_t start = scan_start; start < end_pos; ++start) {
    if (hasDictionaryEntryFrom(dict_manager, codepoints, start, end_pos + 1 - start, probe_end - start,
                               core::PartOfSpeech::Unknown, &isUninflectedClosedClass)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Whether the hiragana portion opens with a multi-mora conjunctive particle.
 *
 * A 接続助詞 attaches to a predicate, so a kanji run immediately followed by one
 * is a verb or adjective stem, not the head of a compound noun (見+ちゃ+だめ,
 * 読ん+じゃ+だめ). This is the head-side counterpart of the て/で tail check
 * below; single-mora members are left out because their kana are also ordinary
 * word-internal morae (手しごと, 雨やどり).
 * @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
 */
bool startsWithConjunctiveParticle(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t kanji_end, size_t end_pos) {
  if (dict_manager == nullptr || end_pos <= kanji_end) {
    return false;
  }
  constexpr size_t kMinimumParticleLength = 2;
  constexpr size_t kParticleProbe = 4;
  return hasDictionaryEntryFrom(
      dict_manager, codepoints, kanji_end, kMinimumParticleLength, std::min(kParticleProbe, end_pos - kanji_end),
      core::PartOfSpeech::Particle,
      [](const dictionary::DictionaryEntry& entry) { return entry.extended_pos == core::ExtendedPOS::ParticleConj; });
}

bool isAdjectiveNominalizationSa(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1 || codepoints[end_pos - 1] != U'さ') {
    return false;
  }
  const std::string stem = extractSubstring(codepoints, start_pos, end_pos - 1);
  if (normalize::utf8Length(stem) >= 2 && utf8::endsWith(stem, "し")) {
    return true;
  }
  return dict_manager->lookupExact(stem, core::PartOfSpeech::Adjective) != nullptr ||
         dict_manager->lookupExact(stem + "い", core::PartOfSpeech::Adjective) != nullptr;
}

bool isNominalClosingParticle(const dictionary::DictionaryEntry& entry) {
  return entry.pos == core::PartOfSpeech::Particle && (entry.extended_pos == core::ExtendedPOS::ParticleCase ||
                                                       entry.extended_pos == core::ExtendedPOS::ParticleTopic ||
                                                       entry.extended_pos == core::ExtendedPOS::ParticleBinding);
}

bool isNominalBoundaryParticle(const dictionary::DictionaryEntry& entry) {
  return entry.pos == core::PartOfSpeech::Particle && (entry.extended_pos == core::ExtendedPOS::ParticleCase ||
                                                       entry.extended_pos == core::ExtendedPOS::ParticleTopic ||
                                                       entry.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
                                                       entry.extended_pos == core::ExtendedPOS::ParticleNo ||
                                                       entry.extended_pos == core::ExtendedPOS::ParticleBinding);
}

// A selected nominal-head rescue supplies an otherwise unavailable open-class
// noun after an attributive selector. It must not swallow a dictionary formal
// noun and the case particle that follows it (ない+わけ+に+は), because that
// closed pair already exposes the searchable boundary.
bool absorbsFormalNounCaseParticle(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1) {
    return false;
  }
  for (size_t formal_end = start_pos + 1; formal_end < end_pos; ++formal_end) {
    const auto* formal_noun =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, formal_end, core::PartOfSpeech::Noun);
    const auto* case_particle =
        lookupEntryInRange(*dict_manager, codepoints, formal_end, end_pos, core::PartOfSpeech::Particle);
    if (formal_noun != nullptr && formal_noun->extended_pos == core::ExtendedPOS::NounFormal &&
        case_particle != nullptr && case_particle->extended_pos == core::ExtendedPOS::ParticleCase) {
      return true;
    }
  }
  return false;
}

bool hasInternalNominalParticleBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                        const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1) {
    return false;
  }
  for (size_t particle_start = start_pos + 1; particle_start < end_pos; ++particle_start) {
    const size_t probe_end = std::min(codepoints.size(), particle_start + static_cast<size_t>(4));
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, particle_start, probe_end)) {
      if (match.entry == nullptr || !isNominalBoundaryParticle(*match.entry)) {
        continue;
      }
      const size_t particle_end = particle_start + match.length;
      if (particle_end >= end_pos) {
        return true;
      }
      const std::string remainder = extractSubstring(codepoints, particle_end, end_pos);
      constexpr PartOfSpeechMask kPredicateMask = partOfSpeechMask(core::PartOfSpeech::Verb) |
                                                  partOfSpeechMask(core::PartOfSpeech::Adjective) |
                                                  partOfSpeechMask(core::PartOfSpeech::Auxiliary);
      if (hasExactPartOfSpeech(*dict_manager, remainder, kPredicateMask)) {
        return true;
      }
      // The particle joins two complete constituents, so a head may only span
      // it when what follows finishes a lexicalized nominal (目の前). A
      // remainder that is no word at all means the head cut into the word
      // after it instead (日のこ|と, where こと is the formal noun).
      constexpr PartOfSpeechMask kNominalMask = partOfSpeechMask(core::PartOfSpeech::Noun) |
                                                partOfSpeechMask(core::PartOfSpeech::Pronoun) |
                                                partOfSpeechMask(core::PartOfSpeech::Suffix);
      if (!hasExactPartOfSpeech(*dict_manager, remainder, kNominalMask)) {
        return true;
      }
    }
  }
  return false;
}

bool hasNominalClosingParticleAt(const std::vector<char32_t>& codepoints, size_t start_pos,
                                 const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos >= codepoints.size()) {
    return false;
  }
  const size_t probe_end = std::min(codepoints.size(), start_pos + static_cast<size_t>(4));
  for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, start_pos, probe_end)) {
    if (match.entry != nullptr && isNominalClosingParticle(*match.entry)) {
      return true;
    }
  }
  return false;
}

bool hasGenitiveNominalSelector(const std::vector<char32_t>& codepoints,
                                const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos < 2 || codepoints[start_pos - 1] != U'の') {
    return false;
  }
  const auto* genitive = dict_manager->lookupExact("の", core::PartOfSpeech::Particle);
  if (genitive == nullptr || genitive->extended_pos != core::ExtendedPOS::ParticleNo) {
    return false;
  }
  if (char_types[start_pos - 2] != normalize::CharType::Hiragana) {
    return true;
  }

  // A case-marked nominal may itself select a head through の
  // (土地+へ+の+あこがれ). Require the complete case particle and a visible
  // non-hiragana host rather than treating every hiragana の as genitive.
  const size_t max_particle_length = std::min(start_pos - 1, static_cast<size_t>(4));
  for (size_t length = 1; length <= max_particle_length; ++length) {
    const size_t particle_start = start_pos - 1 - length;
    if (particle_start == 0 || char_types[particle_start - 1] == normalize::CharType::Hiragana) {
      continue;
    }
    const auto* particle =
        lookupEntryInRange(*dict_manager, codepoints, particle_start, start_pos - 1, core::PartOfSpeech::Particle);
    if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleCase) {
      return true;
    }
  }
  return false;
}

bool mayEndGeneratedAttributiveAdjective(char32_t terminal) {
  switch (terminal) {
    case U'い':  // modern basic form
    case U'き':  // classical attributive
    case U'し':  // classical terminal
    case U'る':  // classical supplementary attributive/terminal
    case U'れ':  // classical supplementary imperative
      return true;
    default:
      return false;
  }
}

bool hasGeneratedAttributiveAdjectiveEndingAt(const std::vector<char32_t>& codepoints,
                                              const std::vector<normalize::CharType>& char_types, size_t first_selector,
                                              size_t selector_end, const grammar::Inflection& inflection,
                                              const dictionary::DictionaryManager* dict_manager) {
  if (!mayEndGeneratedAttributiveAdjective(codepoints[selector_end - 1])) {
    return false;
  }
  for (size_t selector_start = first_selector; selector_start < selector_end; ++selector_start) {
    std::vector<UnknownCandidate> adjective_candidates;
    if (char_types[selector_start] == normalize::CharType::Kanji) {
      generateAdjectiveCandidates(codepoints, selector_start, char_types, inflection, dict_manager,
                                  adjective_candidates);
    } else if (char_types[selector_start] == normalize::CharType::Hiragana) {
      generateHiraganaAdjectiveCandidates(codepoints, selector_start, char_types, inflection, dict_manager,
                                          adjective_candidates);
    }
    if (std::any_of(adjective_candidates.begin(), adjective_candidates.end(), [selector_end](const auto& adjective) {
          return adjective.end == selector_end && adjective.pos == core::PartOfSpeech::Adjective &&
                 adjective.extended_pos == core::ExtendedPOS::AdjBasic &&
                 adjective.cost <= candidate::kAttributiveSelectorMaxCost;
        })) {
      return true;
    }
  }
  return false;
}

bool hasAttributiveNominalSelector(const std::vector<char32_t>& codepoints,
                                   const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                   const grammar::Inflection& inflection,
                                   const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0) {
    return false;
  }
  constexpr size_t kMaximumSelectorLength = 6;
  const size_t first_selector = start_pos > kMaximumSelectorLength ? start_pos - kMaximumSelectorLength : 0;
  const auto* attributive_copula =
      codepoints[start_pos - 1] == U'な' ? dict_manager->lookupExact("な", core::PartOfSpeech::Auxiliary) : nullptr;
  for (size_t selector_start = first_selector; selector_start < start_pos; ++selector_start) {
    // A na-adjective selects a nominal head through its explicit attributive
    // copula (AdjNa+な+X).  The existing adjective probe only recognizes a
    // single AdjBasic edge ending at the head, so inspect this closed two-edge
    // selector separately.  Requiring both AdjNaAdj and AuxCopulaDa leaves
    // ordinary i-adjective attribution (美しい+人) on its existing path.
    if (attributive_copula != nullptr && attributive_copula->extended_pos == core::ExtendedPOS::AuxCopulaDa &&
        selector_start + 1 < start_pos) {
      const auto* na_adjective =
          lookupEntryInRange(*dict_manager, codepoints, selector_start, start_pos - 1, core::PartOfSpeech::Adjective);
      if (na_adjective != nullptr && na_adjective->extended_pos == core::ExtendedPOS::AdjNaAdj) {
        return true;
      }
      std::vector<UnknownCandidate> na_adjective_candidates;
      generateNaAdjectiveCandidates(codepoints, selector_start, char_types, UnknownOptions{}, dict_manager,
                                    na_adjective_candidates);
      if (std::any_of(
              na_adjective_candidates.begin(), na_adjective_candidates.end(), [start_pos](const auto& adjective) {
                return adjective.end == start_pos - 1 && adjective.pos == core::PartOfSpeech::Adjective &&
                       adjective.extended_pos == core::ExtendedPOS::AdjNaAdj &&
                       adjective.origin == CandidateOrigin::AdjectiveNa && adjective.cost <= candidate::kNaAdjStemCost;
              })) {
        return true;
      }
    }
    const std::string selector_surface = extractSubstring(codepoints, selector_start, start_pos);
    if (dict_manager->lookupExact(selector_surface, core::PartOfSpeech::Determiner) != nullptr) {
      return true;
    }
    const auto* exact_adjective = dict_manager->lookupExact(selector_surface, core::PartOfSpeech::Adjective);
    if (exact_adjective != nullptr && exact_adjective->extended_pos == core::ExtendedPOS::AdjBasic) {
      return true;
    }
  }
  return hasGeneratedAttributiveAdjectiveEndingAt(codepoints, char_types, first_selector, start_pos, inflection,
                                                  dict_manager);
}

bool isSelectedNominalHeadShape(const std::vector<normalize::CharType>& char_types, size_t start_pos, size_t end_pos,
                                bool has_attributive_selector) {
  const size_t length = end_pos - start_pos;
  const bool all_hiragana = std::all_of(char_types.begin() + static_cast<std::ptrdiff_t>(start_pos),
                                        char_types.begin() + static_cast<std::ptrdiff_t>(end_pos),
                                        [](normalize::CharType type) { return type == normalize::CharType::Hiragana; });
  if (all_hiragana) {
    return length >= (has_attributive_selector ? 2U : 3U);
  }
  // Both an explicit attributive and genitive の select a nominal head.  A
  // mixed kanji-hiragana head (谷の向こうに) is therefore as well evidenced
  // as the existing attributive case; exact predicate readings and internal
  // auxiliary/particle decompositions are rejected by the caller.
  if (length < 2 || char_types[start_pos] != normalize::CharType::Kanji ||
      char_types[start_pos + 1] != normalize::CharType::Hiragana) {
    return false;
  }
  return std::all_of(char_types.begin() + static_cast<std::ptrdiff_t>(start_pos + 1),
                     char_types.begin() + static_cast<std::ptrdiff_t>(end_pos),
                     [](normalize::CharType type) { return type == normalize::CharType::Hiragana; });
}

}  // namespace

bool hasAuxiliaryParticleDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                       const dictionary::DictionaryManager* dict_manager) {
  // Same floor as hasAuxiliaryChainDecomposition below, for the same reason: at
  // two morae the decomposition is an accident of how many one-mora auxiliaries
  // and particles exist (く+も for くも, ひ+も for ひも), not evidence that the
  // span spells no word.
  if (dict_manager == nullptr || end_pos < start_pos + 3) {
    return false;
  }
  const std::string whole = extractSubstring(codepoints, start_pos, end_pos);
  constexpr PartOfSpeechMask kLexicalMask =
      partOfSpeechMask(core::PartOfSpeech::Noun) | partOfSpeechMask(core::PartOfSpeech::Verb) |
      partOfSpeechMask(core::PartOfSpeech::Adjective) | partOfSpeechMask(core::PartOfSpeech::Adverb);
  if (hasExactPartOfSpeech(*dict_manager, whole, kLexicalMask)) {
    return false;
  }
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    if (lookupEntryInRange(*dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Auxiliary) != nullptr &&
        lookupEntryInRange(*dict_manager, codepoints, split, end_pos, core::PartOfSpeech::Particle) != nullptr) {
      return true;
    }
  }
  return false;
}

bool hasAuxiliaryChainDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                    const dictionary::DictionaryManager* dict_manager) {
  // Two morae are spelled by too many one-mora auxiliaries to be evidence of
  // anything: な+す and か+ぬ decompose that way and are ordinary verbs.
  if (dict_manager == nullptr || end_pos < start_pos + 3) {
    return false;
  }
  constexpr PartOfSpeechMask kAuxiliaryMask = partOfSpeechMask(core::PartOfSpeech::Auxiliary);
  return hasDictionarySplit(*dict_manager, codepoints, start_pos, end_pos, kAuxiliaryMask, kAuxiliaryMask);
}

bool hasFunctionWordChainDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                       const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos < start_pos + 3) {
    return false;
  }
  constexpr PartOfSpeechMask kLexicalMask =
      partOfSpeechMask(core::PartOfSpeech::Noun) | partOfSpeechMask(core::PartOfSpeech::Verb) |
      partOfSpeechMask(core::PartOfSpeech::Adjective) | partOfSpeechMask(core::PartOfSpeech::Adverb);
  if (hasExactPartOfSpeech(*dict_manager, extractSubstring(codepoints, start_pos, end_pos), kLexicalMask)) {
    return false;
  }
  constexpr PartOfSpeechMask kFunctionMask =
      partOfSpeechMask(core::PartOfSpeech::Particle) | partOfSpeechMask(core::PartOfSpeech::Auxiliary);
  for (size_t particle_start = start_pos + 2; particle_start < end_pos; ++particle_start) {
    if (lookupEntryInRange(*dict_manager, codepoints, particle_start, end_pos, core::PartOfSpeech::Particle) ==
        nullptr) {
      continue;
    }
    if (maximalSegmentCount(*dict_manager, codepoints, start_pos, particle_start, core::PartOfSpeech::Auxiliary) >= 1) {
      return true;
    }
  }

  // A stack of sentence-final particles is closed on both sides, so no unknown
  // noun is hiding in it however short its members are (かなあ for か+なあ).
  // Requiring both halves to be final particles keeps this away from the runs a
  // one-mora head would otherwise claim (よそう, かばん).
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    const auto* stack_head =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Particle);
    const auto* stack_tail =
        lookupEntryInRange(*dict_manager, codepoints, split, end_pos, core::PartOfSpeech::Particle);
    if (stack_head != nullptr && stack_tail != nullptr &&
        stack_head->extended_pos == core::ExtendedPOS::ParticleFinal &&
        stack_tail->extended_pos == core::ExtendedPOS::ParticleFinal) {
      return true;
    }
  }

  for (size_t split = start_pos + 2; split < end_pos; ++split) {
    const std::string head_surface = extractSubstring(codepoints, start_pos, split);
    // A focus particle, a sentence-final particle, or a pronoun — the one
    // nominal that is itself closed class, so a run opening with one has no
    // unknown noun to recover either (なにが for なに+が, これから, それでも,
    // かなあ for か+なあ).
    const auto* head = dict_manager->lookupExact(head_surface, core::PartOfSpeech::Particle);
    const bool focus_particle_head = head != nullptr && (head->extended_pos == core::ExtendedPOS::ParticleAdverbial ||
                                                         head->extended_pos == core::ExtendedPOS::ParticleBinding ||
                                                         head->extended_pos == core::ExtendedPOS::ParticleFinal);
    if (!focus_particle_head &&
        !hasExactPartOfSpeech(*dict_manager, head_surface, partOfSpeechMask(core::PartOfSpeech::Pronoun))) {
      continue;
    }
    if (hasExactPartOfSpeech(*dict_manager, extractSubstring(codepoints, split, end_pos), kFunctionMask)) {
      return true;
    }
  }

  // A determiner only ever stands in front of a nominal, never inside one, so a
  // run that opens with one is two words however the rest reads. Requiring the
  // remainder to be attested keeps this to runs that actually have a
  // decomposition (この+すな) rather than any run that starts with those morae.
  constexpr PartOfSpeechMask kAttestedTailMask = kLexicalMask | kFunctionMask |
                                                 partOfSpeechMask(core::PartOfSpeech::Pronoun) |
                                                 partOfSpeechMask(core::PartOfSpeech::Determiner);
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    if (!hasExactPartOfSpeech(*dict_manager, extractSubstring(codepoints, start_pos, split),
                              partOfSpeechMask(core::PartOfSpeech::Determiner))) {
      continue;
    }
    if (hasExactPartOfSpeech(*dict_manager, extractSubstring(codepoints, split, end_pos), kAttestedTailMask)) {
      return true;
    }
  }
  return false;
}

void generateSelectedNominalHeadCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           const std::vector<normalize::CharType>& char_types,
                                           const grammar::Inflection& inflection,
                                           const dictionary::DictionaryManager* dict_manager,
                                           std::vector<UnknownCandidate>& candidates) {
  if (dict_manager == nullptr || start_pos >= codepoints.size()) {
    return;
  }

  const bool has_genitive_selector = hasGenitiveNominalSelector(codepoints, char_types, start_pos, dict_manager);
  const bool has_attributive_selector =
      hasAttributiveNominalSelector(codepoints, char_types, start_pos, inflection, dict_manager);
  if (!has_genitive_selector && !has_attributive_selector) {
    return;
  }

  constexpr size_t kMaximumSelectedHeadLength = 4;
  const auto* head_initial_particle =
      lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Particle);
  if (has_attributive_selector && head_initial_particle != nullptr &&
      isNominalBoundaryParticle(*head_initial_particle)) {
    return;
  }
  for (size_t length = 2; length <= kMaximumSelectedHeadLength; ++length) {
    const size_t head_end = start_pos + length;
    if (head_end > codepoints.size() ||
        !isSelectedNominalHeadShape(char_types, start_pos, head_end, has_attributive_selector) ||
        !hasNominalClosingParticleAt(codepoints, head_end, dict_manager)) {
      continue;
    }

    const std::string head_surface = extractSubstring(codepoints, start_pos, head_end);
    const auto* trailing_auxiliary =
        lookupEntryInRange(*dict_manager, codepoints, head_end - 1, head_end, core::PartOfSpeech::Auxiliary);
    // A selector licenses the nominal immediately after it, not a span that
    // has swallowed the copula before a closing particle. In
    // という話だが, the selected head is 話 and だ begins its predicate; treating
    // 話だ as the head erases an inflectional boundary.
    const bool absorbs_copula =
        trailing_auxiliary != nullptr && trailing_auxiliary->extended_pos == core::ExtendedPOS::AuxCopulaDa;
    const bool mixed_head = char_types[start_pos] == normalize::CharType::Kanji;
    bool has_productive_adjective_nominalization = false;
    if (codepoints[head_end - 1] == U'さ') {
      std::vector<UnknownCandidate> adjective_candidates;
      generateAdjectiveStemCandidates(codepoints, start_pos, char_types, inflection, dict_manager,
                                      adjective_candidates);
      has_productive_adjective_nominalization =
          std::any_of(adjective_candidates.begin(), adjective_candidates.end(), [head_end](const auto& adjective) {
            return adjective.end == head_end - 1 && adjective.pos == core::PartOfSpeech::Adjective;
          });
    }
    if (isAdjectiveNominalizationSa(dict_manager, codepoints, start_pos, head_end) ||
        has_productive_adjective_nominalization ||
        (mixed_head && hasInternalNominalParticleBoundary(codepoints, start_pos, head_end, dict_manager)) ||
        absorbsFormalNounCaseParticle(dict_manager, codepoints, start_pos, head_end)) {
      continue;
    }
    bool has_exact_noun = false;
    bool has_exact_renyokei = false;
    bool has_blocking_exact_reading = false;
    for (const auto& match : dict_manager->lookup(head_surface, 0)) {
      if (match.entry == nullptr || match.length != length) {
        continue;
      }
      if (match.entry->pos == core::PartOfSpeech::Noun) {
        has_exact_noun = true;
      } else if (match.entry->pos == core::PartOfSpeech::Verb &&
                 match.entry->extended_pos == core::ExtendedPOS::VerbRenyokei) {
        has_exact_renyokei = true;
      } else {
        has_blocking_exact_reading = true;
      }
    }
    // A chain of registered function words is never a nominal head, however
    // strong the left selector is (という+ほど+で), so that check is not gated on
    // the selector the way the auxiliary+particle one is. A run spelled by
    // auxiliaries alone is the same case: だろう is the copula's irrealis plus
    // the volitional, and an attributive predicate to its left is what puts it
    // there rather than evidence that it heads a phrase. The generic unknown-
    // noun rescue deliberately does not take this guard — it has no selector
    // asserting a phrase head, so for it a run that merely decomposes into
    // one-mora classical fragments (くるま as くる + ま) is still a noun.
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (has_exact_noun || has_blocking_exact_reading || absorbs_copula ||
        hasFunctionWordChainDecomposition(codepoints, start_pos, head_end, dict_manager) ||
        hasAuxiliaryChainDecomposition(codepoints, start_pos, head_end, dict_manager) ||
        (!has_attributive_selector &&
         hasAuxiliaryParticleDecomposition(codepoints, start_pos, head_end, dict_manager))) {
      continue;
    }

    // A selected nominal head cannot swallow a completed predicate followed
    // by one auxiliary.  Unlike an auxiliary-only chain, this shape starts
    // with an open-class continuative (降り+たる), so test its two grammatical
    // components directly.  It keeps a genitive selector from converting the
    // entire inflected predicate into an unknown noun.
    bool contains_predicate_auxiliary_boundary = false;
    for (size_t split = start_pos + 1; split < head_end; ++split) {
      const auto* predicate = lookupEntryInRange(*dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Verb);
      const auto* auxiliary =
          lookupEntryInRange(*dict_manager, codepoints, split, head_end, core::PartOfSpeech::Auxiliary);
      if (predicate != nullptr && predicate->extended_pos == core::ExtendedPOS::VerbRenyokei && auxiliary != nullptr) {
        contains_predicate_auxiliary_boundary = true;
        break;
      }
    }
    if (contains_predicate_auxiliary_boundary) {
      continue;
    }

    // A selector may rescue an unknown nominal head, but it cannot turn a
    // complete te-form predicate into a noun.  In particular, the productive
    // し+て form must retain both morphemes before a following particle instead
    // of becoming a selected nominal head.  Consult the shared hiragana verb
    // generator rather than special-casing して, so every independently
    // established te-form receives the same protection.
    const auto predicate_candidates =
        generateHiraganaVerbCandidates(codepoints, start_pos, char_types, inflection, dict_manager);
    const bool has_complete_te_predicate =
        std::any_of(predicate_candidates.begin(), predicate_candidates.end(), [head_end](const auto& predicate) {
          return predicate.end == head_end && predicate.extended_pos == core::ExtendedPOS::VerbTeForm;
        });
    if (has_complete_te_predicate) {
      continue;
    }

    const float noun_cost = length == kMaximumSelectedHeadLength ? candidate::kSelectedNominalFourMoraHeadCost
                                                                 : candidate::kSelectedNominalShortHeadCost;
    auto noun_candidate = makeCandidate(head_surface, start_pos, head_end, core::PartOfSpeech::Noun, noun_cost,
                                        /*has_suffix=*/true, CandidateOrigin::SelectedNominalHead);
    noun_candidate.extended_pos = has_exact_renyokei ? core::ExtendedPOS::NounVerbal : core::ExtendedPOS::Noun;
#ifdef SUZUME_DEBUG_INFO
    noun_candidate.pattern = has_genitive_selector ? "genitive_selected_noun" : "attributive_selected_noun";
#endif
    candidates.push_back(std::move(noun_candidate));
  }
  return;
}

void generateKanjiHiraganaCompoundCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const dictionary::DictionaryManager* dict_manager,
                                             std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // Skip if this kanji is preceded by another kanji - it's likely the tail end
  // of a longer kanji compound, not the start of a new kanji+hiragana word.
  // E.g., in 魔法少女まどか, skip generating 女まど at pos=3.
  // Dictionary entries (玉ねぎ etc.) are handled separately as dict candidates.
  if (start_pos > 0 && char_types[start_pos - 1] == normalize::CharType::Kanji) {
    return;
  }

  // A non-quantity nominal stem plus the closed comparison bound 以上/以下
  // forms one search unit (必要以上, 期待以下). Numeral+counter phrases retain
  // their compositional boundary (三名|以上, 百倍|以下), which is owned by the
  // counter generator. A following nominal selector proves the right edge.
  size_t comparison_end = start_pos;
  while (comparison_end < char_types.size() && char_types[comparison_end] == normalize::CharType::Kanji) {
    ++comparison_end;
  }
  if (comparison_end >= start_pos + 4 &&
      ((codepoints[comparison_end - 2] == U'以' && codepoints[comparison_end - 1] == U'上') ||
       (codepoints[comparison_end - 2] == U'以' && codepoints[comparison_end - 1] == U'下')) &&
      !normalize::isNumeralCodepoint(codepoints[comparison_end - 3]) &&
      !normalize::isCounterKanji(codepoints[comparison_end - 3]) &&
      hasNominalPhraseSelectorAt(dict_manager, codepoints, comparison_end)) {
    const std::string surface = extractSubstring(codepoints, start_pos, comparison_end);
    auto comparison = makeCandidate(surface, start_pos, comparison_end, core::PartOfSpeech::Noun,
                                    candidate::kComparisonCompoundNounCost, false, CandidateOrigin::SuffixPattern);
    comparison.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
    comparison.confidence = candidate::kDictionaryOriginConfidence;
    comparison.pattern = "comparison_bound_compound";
#endif
    candidates.push_back(std::move(comparison));
    return;
  }

  // Find kanji portion (1 character only for compound nouns)
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, 1, normalize::CharType::Kanji);

  size_t kanji_len = kanji_end - start_pos;
  if (kanji_len == 0) {
    return;
  }

  // -がかり and -がけ are nominal suffixes after a noun or a verb
  // continuative (手がかり, 通りがかり, 通りがけ, 一日がけ).  The
  // nominal-phrase-particle gate distinguishes these closed nominal constructions
  // from an ordinary subject marker followed by unrelated hiragana, while
  // keeping the complete compound as one search unit in a noun phrase.
  constexpr std::string_view kGakari = "がかり";
  constexpr std::string_view kGake = "がけ";
  size_t nominal_stem_end = kanji_end;
  while (nominal_stem_end < char_types.size() && char_types[nominal_stem_end] == normalize::CharType::Kanji) {
    ++nominal_stem_end;
  }
  for (size_t suffix_start = nominal_stem_end;
       suffix_start < codepoints.size() && char_types[suffix_start] == normalize::CharType::Hiragana; ++suffix_start) {
    for (std::string_view suffix : {kGakari, kGake}) {
      const size_t suffix_end = suffix_start + normalize::utf8Length(suffix);
      if (suffix_end > codepoints.size() || extractSubstring(codepoints, suffix_start, suffix_end) != suffix ||
          !hasNominalPhraseSelectorAt(dict_manager, codepoints, suffix_end)) {
        continue;
      }
      const std::string surface = extractSubstring(codepoints, start_pos, suffix_end);
      auto candidate = makeCandidate(surface, start_pos, suffix_end, core::PartOfSpeech::Noun,
                                     candidate::kDerivedSuffixCompoundNounCost, false, CandidateOrigin::SuffixPattern);
      candidate.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
      candidate.confidence = candidate::kDictionaryOriginConfidence;
      candidate.pattern = "nominal_gakari_gake";
#endif
      candidates.push_back(std::move(candidate));
      return;
    }
  }

  // Find hiragana portion (2-4 characters)
  if (kanji_end >= char_types.size() || char_types[kanji_end] != normalize::CharType::Hiragana) {
    return;
  }
  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() && hiragana_end - kanji_end < 4 &&
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    char32_t ch = codepoints[hiragana_end];
    if (normalize::isParticleCodepoint(ch)) {
      break;
    }
    ++hiragana_end;
  }

  size_t hiragana_len = hiragana_end - kanji_end;
  char32_t first_hira = codepoints[kanji_end];

  // A kanji numeral followed by つ is already a complete native counter
  // (一つ, 二つ). Do not extend it into an invented kanji-hiragana compound
  // when another hiragana word follows (一つ|ひとつ), because the counter
  // generator emits the natural boundary separately.
  if (normalize::isNumeralCodepoint(codepoints[start_pos]) && first_hira == U'つ') {
    return;
  }

  // Handle sokuon (っ) pattern FIRST, before the hiragana_len check
  // Pattern: 漢字 + っ + (漢字 or 平仮名) - e.g., 横っ面, 取っ手, 引っ込む
  // These are valid compound words where hiragana portion may be just 1 char (っ)
  if (first_hira == U'っ') {
    // Need at least one more character after っ
    size_t sokuon_pos = kanji_end;  // Position of っ
    if (sokuon_pos + 1 < char_types.size()) {
      normalize::CharType next_type = char_types[sokuon_pos + 1];

      if (next_type == normalize::CharType::Kanji) {
        // Pattern: 漢字 + っ + 漢字 (e.g., 横っ面, 取っ手)
        size_t kanji2_end = findCharRegionEnd(char_types, sokuon_pos + 1, 3, normalize::CharType::Kanji);

        // Generate candidates for each length
        for (size_t end_pos = sokuon_pos + 2; end_pos <= kanji2_end; ++end_pos) {
          std::string surface = extractSubstring(codepoints, start_pos, end_pos);
          if (!surface.empty()) {
            auto cand = makeCandidate(surface, start_pos, end_pos, core::PartOfSpeech::Noun,
                                      candidate::kInfixCompoundNounCost, false, CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
            cand.confidence = 0.9F;
            cand.pattern = "kanji_sokuon_kanji";
#endif
            candidates.push_back(cand);
          }
        }

        // Check for hatsuonbin verb: 漢字+っ+漢字+ん (e.g., 吹っ飛ん from 吹っ飛ぶ)
        // When the second kanji is followed by ん, check if kanji2+ぶ/む/ぬ is in dict
        if (kanji2_end < codepoints.size() && codepoints[kanji2_end] == U'ん' && dict_manager != nullptr) {
          std::string kanji2_stem = extractSubstring(codepoints, sokuon_pos + 1, kanji2_end);

          auto hatsuonbin_match = verb_helpers::firstGodanOnbinDictBase(dict_manager, kanji2_stem, "ん");
          if (hatsuonbin_match.matched) {
            size_t onbin_end = kanji2_end + 1;  // Include ん
            std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);
            constexpr float kHatsuonbinCost = -0.5F;
            auto cand = makeCandidate(onbin_surface, start_pos, onbin_end, core::PartOfSpeech::Verb, kHatsuonbinCost,
                                      false, CandidateOrigin::KanjiHiraganaCompound);
            // Full base form includes the first kanji + っ
            std::string full_kanji = extractSubstring(codepoints, start_pos, kanji2_end);
            cand.lemma = normalize::concat(full_kanji, hatsuonbin_match.base_suffix);
            cand.conj_type = grammar::verbTypeToConjType(hatsuonbin_match.verb_type);
            cand.extended_pos = core::ExtendedPOS::VerbOnbinkei;
#ifdef SUZUME_DEBUG_INFO
            cand.confidence = 0.9F;
            cand.pattern = "sokuon_kanji_hatsuonbin";
#endif
            SUZUME_DEBUG_LOG("[SUFFIX_CAND] " << onbin_surface << " sokuon_kanji_hatsuonbin lemma=" << cand.lemma
                                              << " cost=" << kHatsuonbinCost << "\n");
            candidates.push_back(cand);
          }
        }
      } else if (next_type == normalize::CharType::Hiragana) {
        // Pattern: 漢字 + っ + 平仮名 (e.g., 引っ込む, 突っ走る)
        // BUT skip if っ is followed by た/て (verb conjugation endings)
        // e.g., 減った, 勝って are verb forms, not compound nouns
        char32_t next_hira = codepoints[sokuon_pos + 1];
        if (next_hira == U'た' || next_hira == U'て') {
          return;  // Skip - this is a verb conjugation, not a compound noun
        }
        size_t hira2_end = sokuon_pos + 1;
        while (hira2_end < char_types.size() && hira2_end - (sokuon_pos + 1) < 4 &&
               char_types[hira2_end] == normalize::CharType::Hiragana) {
          char32_t ch = codepoints[hira2_end];
          if (normalize::isParticleCodepoint(ch)) {
            break;
          }
          ++hira2_end;
        }

        if (hira2_end > sokuon_pos + 1) {
          // A registered adjective beginning at the sokuon is a productive
          // suffix boundary (e.g. noun + っぽ + さ). Do not fabricate a
          // single compound noun across it; the dictionary candidates retain
          // the suffix inflection and any following nominalizer.
          if (dict_manager != nullptr) {
            for (const auto& entry : lookupResultsInRange(*dict_manager, codepoints, sokuon_pos, hira2_end)) {
              if (entry.entry != nullptr && entry.entry->pos == core::PartOfSpeech::Adjective) {
                const size_t ppoi_end = sokuon_pos + 2;
                const bool ppoi_stem_before_inflection = entry.entry->lemma == "っぽい" &&
                                                         entry.entry->extended_pos == core::ExtendedPOS::AdjStem &&
                                                         ppoi_end < hira2_end && codepoints[ppoi_end] != U'さ';
                if (ppoi_stem_before_inflection) {
                  continue;
                }
                // A single-kanji nominal/adjectival host forms one search unit
                // with the productive resemblance suffix (安っぽい, 水っぽい).
                // Longer nominal hosts retain the noun + suffix boundary
                // (子供 + っぽい), while verb continuatives are handled by the
                // dedicated productive path below.
                const bool precedes_nominalizer = ppoi_end < hira2_end && codepoints[ppoi_end] == U'さ';
                if (kanji_len == 1 && entry.entry->lemma == "っぽい" && !precedes_nominalizer) {
                  const size_t derived_end = sokuon_pos + entry.length;
                  auto adjective =
                      makeCandidate(extractSubstring(codepoints, start_pos, derived_end), start_pos, derived_end,
                                    core::PartOfSpeech::Adjective, candidate::kProductivePpoiAdjCost, false,
                                    CandidateOrigin::KanjiHiraganaCompound, entry.entry->extended_pos);
                  adjective.lemma = extractSubstring(codepoints, start_pos, sokuon_pos) + "っぽい";
                  adjective.conj_type = dictionary::ConjugationType::IAdjective;
                  candidates.push_back(std::move(adjective));
                  return;
                }
                const std::string base = extractSubstring(codepoints, start_pos, sokuon_pos);
                // An i-adjective stem productively forms 〜っぽい.  Keep its
                // stem before the following nominalizer (安っぽ+さ), while a
                // nominal base such as 男 retains the ordinary noun+suffix
                // boundary.  The dictionary gate is on the adjective base,
                // not on individual derived words.
                if (ppoi_end <= codepoints.size() && extractSubstring(codepoints, sokuon_pos, ppoi_end) == "っぽ") {
                  if (dict_manager->lookupExact(base + "い", core::PartOfSpeech::Adjective) != nullptr) {
                    auto stem = makeCandidate(extractSubstring(codepoints, start_pos, ppoi_end), start_pos, ppoi_end,
                                              core::PartOfSpeech::Adjective, candidate::kCompoundAdjBaseCost, true,
                                              CandidateOrigin::KanjiHiraganaCompound, core::ExtendedPOS::AdjStem);
                    stem.lemma = base + "っぽい";
                    stem.conj_type = dictionary::ConjugationType::IAdjective;
                    candidates.push_back(std::move(stem));
                  }
                }
                return;
              }
            }
          }

          std::string surface = extractSubstring(codepoints, start_pos, hira2_end);
          if (!surface.empty()) {
            auto cand = makeCandidate(surface, start_pos, hira2_end, core::PartOfSpeech::Noun, 1.0F, false,
                                      CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
            cand.confidence = 0.7F;
            cand.pattern = "kanji_sokuon_hira";
#endif
            candidates.push_back(cand);
          }
        }
      }
    }
    // Return after handling sokuon - don't continue to normal hiragana logic
    return;
  }

  // Pattern: 単漢字 + ん + 単漢字 — the moraic nasal infixed inside one lexical
  // compound (真ん前, 真ん丸, 赤ん坊), the phonological sibling of the っ
  // pattern above. The contracted genitive の spells the same mora (店+ん+中),
  // so this only adds a candidate: where both flanking kanji are attested
  // nouns their own dictionary edges keep the split cheaper.
  if (first_hira == U'ん' && kanji_end - start_pos == 1 && kanji_end + 1 < char_types.size() &&
      char_types[kanji_end + 1] == normalize::CharType::Kanji) {
    const size_t end_pos = kanji_end + 2;
    const bool second_kanji_is_single =
        end_pos >= char_types.size() || char_types[end_pos] != normalize::CharType::Kanji;
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    if (second_kanji_is_single && !surface.empty()) {
      // An attributive copula right after the compound identifies it as a
      // na-adjective stem rather than a plain noun (真ん丸+な+月).
      const bool has_attributive_copula = end_pos < codepoints.size() && codepoints[end_pos] == U'な';
      auto cand = makeCandidate(surface, start_pos, end_pos,
                                has_attributive_copula ? core::PartOfSpeech::Adjective : core::PartOfSpeech::Noun,
                                candidate::kInfixCompoundNounCost, false, CandidateOrigin::KanjiHiraganaCompound);
      if (has_attributive_copula) {
        cand.extended_pos = core::ExtendedPOS::AdjNaAdj;
      }
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 0.9F;
      cand.pattern = "kanji_hatsuon_kanji";
#endif
      candidates.push_back(cand);
    }
    return;
  }

  // A single さ after a kanji nominal has no reading of its own here. The
  // nominalizer derives a noun from an adjective stem and cannot take a plain
  // nominal host; the final particle さ only occurs clause-finally. When a
  // nominal-selecting particle follows, both are excluded by elimination and
  // the mixed-script span is one lexical compound (逆さに映る, but 今さ、…
  // keeps the final particle and 高さ keeps the nominalizer).
  if (hiragana_len == 1 && first_hira == U'さ' &&
      !isAdjectiveNominalizationSa(dict_manager, codepoints, start_pos, hiragana_end) &&
      hasNominalPhraseSelectorAt(dict_manager, codepoints, hiragana_end)) {
    auto cand = makeCandidate(extractSubstring(codepoints, start_pos, hiragana_end), start_pos, hiragana_end,
                              core::PartOfSpeech::Noun, candidate::kInfixCompoundNounCost, false,
                              CandidateOrigin::KanjiHiraganaNominalCompound);
#ifdef SUZUME_DEBUG_INFO
    cand.confidence = candidate::kHighOriginConfidence;
    cand.pattern = "kanji_nominalizer_sa_compound";
#endif
    candidates.push_back(cand);
    return;
  }

  if (hiragana_len < 2) {
    return;
  }
  char32_t second_hira = (hiragana_len >= 2) ? codepoints[kanji_end + 1] : 0;

  // A kanji verb continuative stem productively combines with the resemblance
  // suffix っぽい to form one i-adjective search unit (忘れっぽい, 飽きっぽい).
  // This is morphology, not a per-word lexicon: i-row marks Godan
  // continuative stems and e-row marks Ichidan continuative stems.
  const std::string hiragana_candidate = extractSubstring(codepoints, kanji_end, hiragana_end);
  if (utf8::endsWith(hiragana_candidate, "っぽい") &&
      (grammar::isIRowCodepoint(first_hira) || grammar::isERowCodepoint(first_hira))) {
    const std::string derived = extractSubstring(codepoints, start_pos, hiragana_end);
    auto adjective = makeCandidate(derived, start_pos, hiragana_end, core::PartOfSpeech::Adjective,
                                   candidate::kProductivePpoiAdjCost, false, CandidateOrigin::KanjiHiraganaCompound,
                                   core::ExtendedPOS::AdjBasic);
    adjective.lemma = derived;
    adjective.conj_type = dictionary::ConjugationType::IAdjective;
    candidates.push_back(std::move(adjective));
    return;
  }

  // Skip small kana at start - morphologically invalid
  // EXCEPTION: っ (sokuon) can appear in compound patterns like 横っ面, 取っ手, 引っ込む
  // These are valid words where kanji + っ + (kanji or hiragana) forms a compound
  if (first_hira == U'ゃ' || first_hira == U'ゅ' || first_hira == U'ょ' || first_hira == U'ぁ' || first_hira == U'ぃ' ||
      first_hira == U'ぅ' || first_hira == U'ぇ' || first_hira == U'ぉ') {
    return;
  }

  // Skip patterns ending with ん - likely honorific suffixes
  // e.g., さん, くん, ちゃん, たん should split as NOUN + SUFFIX
  // This is a grammatical pattern: hiragana ending with ん after single kanji
  // is typically an honorific suffix, not a compound noun
  if (kanji_len == 1 && hiragana_len >= 2) {
    char32_t last_hira = codepoints[hiragana_end - 1];
    if (last_hira == U'ん') {
      return;
    }
  }

  // Check if pattern looks like a grammatical suffix
  // These get high cost to let verb/adjective candidates win
  bool looks_like_aux = false;

  if (hiragana_len >= 2) {
    // te/ta form, copula patterns
    if (second_hira == U'て' || second_hira == U'た' || second_hira == U'で' || second_hira == U'だ') {
      looks_like_aux = true;
    }
    // ます, ない
    if ((first_hira == U'ま' && second_hira == U'す') || (first_hira == U'な' && second_hira == U'い')) {
      looks_like_aux = true;
    }
    // れる, られる, せる, させる
    if ((first_hira == U'れ' && second_hira == U'る') || (first_hira == U'せ' && second_hira == U'る')) {
      looks_like_aux = true;
    }
    // だった, だろう
    if (first_hira == U'だ' && (second_hira == U'っ' || second_hira == U'ろ')) {
      looks_like_aux = true;
    }
    // なら, なかった
    if (first_hira == U'な' && (second_hira == U'ら' || second_hira == U'か')) {
      looks_like_aux = true;
    }
    // Godan verb shuushikei (終止形) pattern
    // e.g., 休む, 行く, 泳ぐ, 話す, 立つ, 死ぬ, 飛ぶ, 取る
    // If first hiragana is a godan verb ending, kanji+first hiragana likely forms
    // a complete verb, and the rest starts a new word
    // 休むこと → 休む(VERB) + こと(NOUN), not 休むこ(NOUN) + と(PARTICLE)
    bool is_godan_shuushikei = (first_hira == U'む' || first_hira == U'う' || first_hira == U'く' ||
                                first_hira == U'ぐ' || first_hira == U'す' || first_hira == U'つ' ||
                                first_hira == U'ぬ' || first_hira == U'ぶ' || first_hira == U'る');
    if (is_godan_shuushikei) {
      // The 終止形 split hypothesis (kanji+first_hira is a complete verb, the rest starts
      // a new word) is only sound when the stranded remainder is lexically realizable.
      // When exactly one hiragana would be orphaned (hiragana_len == 2), require that a
      // dictionary word can start there; otherwise the "verb" reading strands junk (宝く|じ)
      // and we must keep the kanji+hiragana noun (宝くじ) whole. Standalone single hiragana
      // are a closed class (final particles よ/ね/な, copula, …) all in L1, and formal-noun
      // continuations (こと) are caught by scanning across the particle break — so 休むこと,
      // 飲むな, 帰るね, 行くよ still split as before.
      bool orphan_split_viable = true;
      if (hiragana_len == 2 && dict_manager != nullptr) {
        size_t orphan_pos = kanji_end + 1;
        size_t ctx_end = orphan_pos;
        while (ctx_end < char_types.size() && ctx_end - orphan_pos < 3 &&
               char_types[ctx_end] == normalize::CharType::Hiragana) {
          ++ctx_end;
        }
        orphan_split_viable =
            lookupResultsHavePartOfSpeech(lookupResultsInRange(*dict_manager, codepoints, orphan_pos, ctx_end),
                                          partOfSpeechMask(core::PartOfSpeech::Particle));
      }
      if (orphan_split_viable) {
        looks_like_aux = true;
      }
    }
    // Renyokei + そう/たい/ます
    // For godan verbs: し,み,き,ぎ,ち,り,い,び (i-row)
    // For ichidan verbs: べ,め,け,せ,て,ね,れ,え (e-row) - these are verb stems
    bool is_renyokei = (first_hira == U'し' || first_hira == U'み' || first_hira == U'き' || first_hira == U'ぎ' ||
                        first_hira == U'ち' || first_hira == U'り' || first_hira == U'い' || first_hira == U'び');
    bool is_ichidan_stem = (first_hira == U'べ' || first_hira == U'め' || first_hira == U'け' || first_hira == U'せ' ||
                            first_hira == U'て' || first_hira == U'ね' || first_hira == U'れ' || first_hira == U'え' ||
                            first_hira == U'げ' || first_hira == U'ぜ' || first_hira == U'で' || first_hira == U'へ' ||
                            first_hira == U'ぺ');
    if ((is_renyokei || is_ichidan_stem) && (second_hira == U'そ' || second_hira == U'た' || second_hira == U'ま')) {
      looks_like_aux = true;
    }
    // Negative + 様態 そう (なさそう): the negative auxiliary ない nominalized as
    // なさ, carrying 様態 そう. Attaches to a verb stem (見なさそう = 見 + なさそう,
    // 食べなさそう = 食べ + なさそう) and is never a compound noun. This is the
    // negative counterpart of the renyokei + そう handling above, so let the
    // verb + な + さ + そう decomposition win instead of merging into one noun.
    if (hiragana_len >= 3) {
      std::string hira_portion = extractSubstring(codepoints, kanji_end, hiragana_end);
      if (hira_portion.find("なさそ") != std::string::npos) {
        looks_like_aux = true;
      }
    }
    // Renyokei + なさい (polite imperative)
    // e.g., 書きなさい, 起きなさい - these should split as verb + なさい
    if ((is_renyokei || is_ichidan_stem) && hiragana_len >= 4) {
      // Check if hiragana portion ends with "さい" (last 2 chars of なさい)
      char32_t h_minus2 = codepoints[hiragana_end - 2];
      char32_t h_minus1 = codepoints[hiragana_end - 1];
      if (h_minus2 == U'さ' && h_minus1 == U'い') {
        looks_like_aux = true;
      }
    }
    // Renyokei + べき (classical auxiliary)
    // e.g., 読むべき, 食べるべき - these should split as verb + べき
    if (hiragana_len >= 3) {
      char32_t h_minus2 = codepoints[hiragana_end - 2];
      char32_t h_minus1 = codepoints[hiragana_end - 1];
      if (h_minus2 == U'べ' && h_minus1 == U'き') {
        looks_like_aux = true;
      }
    }
    // Patterns containing くださ (part of ください auxiliary)
    // e.g., 待ちくださ, 行きくださ - these should be verb + ください
    // Check if hiragana portion contains くださ
    if (hiragana_len >= 3) {
      std::string hira_portion = extractSubstring(codepoints, kanji_end, hiragana_end);
      if (hira_portion.find("くださ") != std::string::npos || hira_portion.find("ください") != std::string::npos) {
        looks_like_aux = true;
      }
    }
  }

  // Ichidan verb pattern (e-row + る)
  bool is_e_row =
      (first_hira == U'え' || first_hira == U'け' || first_hira == U'げ' || first_hira == U'せ' ||
       first_hira == U'て' || first_hira == U'ね' || first_hira == U'べ' || first_hira == U'め' || first_hira == U'れ');
  if (is_e_row && hiragana_len >= 2 && second_hira == U'る') {
    looks_like_aux = true;
  }

  // Patterns ending with る
  char32_t last_hira = codepoints[hiragana_end - 1];
  if (last_hira == U'る' && hiragana_len >= 2) {
    looks_like_aux = true;
  }

  // Patterns ending with るそう (verb dictionary form + hearsay そう)
  // e.g., 食べるそう, 降るそう - these are verb終止形 + そう(hearsay), not compound nouns
  // Valid i-adj+そう like 美味しそう are handled separately (don't have る before そう)
  if (hiragana_len >= 3 && last_hira == U'う') {
    char32_t h_minus2 = codepoints[hiragana_end - 2];
    char32_t h_minus3 = (hiragana_end >= 3) ? codepoints[hiragana_end - 3] : U'\0';
    // Check for るそう pattern (verb終止形 + hearsay)
    if (h_minus2 == U'そ' && h_minus3 == U'る') {
      looks_like_aux = true;
    }
    // Check for くそう pattern (godan-ku終止形 + hearsay: 行くそう)
    if (h_minus2 == U'そ' && h_minus3 == U'く') {
      looks_like_aux = true;
    }
    // Check for すそう pattern (godan-sa終止形 + hearsay: 話すそう, するそう)
    if (h_minus2 == U'そ' && h_minus3 == U'す') {
      looks_like_aux = true;
    }
  }

  // Patterns ending with て/で (verb te-form)
  // e.g., 基づいて, 考えて - these are verb conjugations, not compound nouns
  if ((last_hira == U'て' || last_hira == U'で') && hiragana_len >= 2) {
    looks_like_aux = true;
  }

  // Patterns opening with a conjunctive particle (見ちゃだめ, 読んじゃだめ)
  if (startsWithConjunctiveParticle(dict_manager, codepoints, kanji_end, hiragana_end)) {
    looks_like_aux = true;
  }

  // Patterns ending with お (prefix marker)
  // e.g., 一つお should be 一つ + お(PREFIX), not 一つお(NOUN)
  // お is very commonly used as honorific prefix, so it should not be absorbed
  // into compound nouns
  if (last_hira == U'お') {
    looks_like_aux = true;
  }

  // A terminal copular だ is always a separate grammatical boundary after a
  // nominal or na-adjective stem (平ら+だ), never part of an unknown mixed-
  // script compound noun.
  if (last_hira == U'だ') {
    return;
  }

  // X+さ is an adjective nominalization when X is a verified adjective stem;
  // keep that morpheme boundary instead of treating the mixed-script span as
  // an opaque noun merely because a case particle follows it.
  if (isAdjectiveNominalizationSa(dict_manager, codepoints, start_pos, hiragana_end)) {
    looks_like_aux = true;
  }

  // Skip NOUN generation for pure auxiliary patterns
  // These should always be verb stem + auxiliary, never a compound noun
  // e.g., 寝ます should be 寝(VERB) + ます(AUX), not 寝ます(NOUN)
  if (hiragana_len == 2) {
    using namespace suzume::core::hiragana;
    char32_t h1 = codepoints[kanji_end];
    char32_t h2 = codepoints[kanji_end + 1];
    // ます, ない - pure polite/negative auxiliaries
    if ((h1 == kMa && h2 == kSu) || (h1 == kNa && h2 == kI)) {
      return;  // Skip NOUN generation entirely
    }
  }

  // Check if the hiragana portion is a known dictionary word (exact match)
  // If so, skip compound generation to let the split path win
  // E.g., 火だるま: if だるま is in dictionary, don't generate compound
  // Only skip for exact matches - partial matches (like た in たまり) don't count
  if (dict_manager != nullptr && lookupEntryInRange(*dict_manager, codepoints, kanji_end, hiragana_end) != nullptr) {
    // This allows split like 火+だるま to win.
    return;
  }

  // Skip compound generation if the full surface is a known verb in dictionary
  // E.g., 下さい is dict verb (くださる), not compound noun
  {
    std::string full_surface = extractSubstring(codepoints, start_pos, hiragana_end);
    if (verb_helpers::isVerbInDictionary(dict_manager, full_surface)) {
      return;  // Skip - dict verb should win
    }
  }

  // Skip when the hiragana portion ends in a focus particle (副助詞/係助詞)
  // tail, optionally followed by ない: 金さえない is noun + 係助詞 さえ + ない,
  // never a single compound noun. A hiragana portion that IS exactly a
  // particle (先ほど, 中ほど) was already skipped by the exact-dictionary-word
  // check above, so this only rejects particle + negative absorption blobs.
  // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
  if (verb_helpers::endsWithFocusParticleTail(dict_manager, codepoints, start_pos, hiragana_end)) {
    return;  // Skip - noun + focus particle split should win
  }

  if (strandsParticleLikeMoraBeforeNominalSelector(dict_manager, codepoints, hiragana_end)) {
    return;
  }

  // Skip when the span boundary cuts a fixed closed-class word in half
  // (読まれど|も for 読ま + れ + ども, 走るそ|の for 走る + その).
  if (boundarySplitsClosedClassWord(dict_manager, codepoints, kanji_end, hiragana_end)) {
    return;
  }

  // Generate candidate with cost based on pattern
  std::string surface = extractSubstring(codepoints, start_pos, hiragana_end);
  if (!surface.empty()) {
    float cost = looks_like_aux ? 3.5F : 1.0F;
    // A clause that ends on the span is the same nominal frame a following case
    // particle provides: nothing there can be a predicate ending, so whatever
    // occupies the position is a nominal (草むら。 alongside 草むらに). Without
    // this the identical compound would be priced as an unverified run purely
    // because the sentence stopped.
    const bool ends_clause =
        hiragana_end >= char_types.size() || char_types[hiragana_end] == normalize::CharType::Symbol;
    const bool nominal_context =
        !looks_like_aux && (ends_clause || hasNominalPhraseSelectorAt(dict_manager, codepoints, hiragana_end));
    auto cand = makeCandidate(
        surface, start_pos, hiragana_end, core::PartOfSpeech::Noun, cost, false,
        nominal_context ? CandidateOrigin::KanjiHiraganaNominalCompound : CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
    cand.confidence = looks_like_aux ? 0.3F : 0.8F;
    cand.pattern = looks_like_aux ? "aux_like" : (nominal_context ? "nominal_compound" : "compound");
#endif
    candidates.push_back(cand);
  }

  return;
}

}  // namespace suzume::analysis
