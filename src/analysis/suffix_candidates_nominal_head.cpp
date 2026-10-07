/**
 * @file suffix_candidates_nominal_head.cpp
 * @brief Short noun heads selected by a left modifier and closed by a nominal particle
 */

#include <algorithm>

#include "adjective_candidates.h"
#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "suffix_candidates.h"
#include "suffix_candidates_compound_internal.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"
#include "verb_candidates_verb_stems.h"

namespace suzume::analysis {

namespace {

bool isNominalClosingParticle(const dictionary::DictionaryEntry& entry) {
  return entry.pos == core::PartOfSpeech::Particle && (entry.extended_pos == core::ExtendedPOS::ParticleCase ||
                                                       entry.extended_pos == core::ExtendedPOS::ParticleTopic ||
                                                       entry.extended_pos == core::ExtendedPOS::ParticleBinding);
}

bool isNominalBoundaryParticle(const dictionary::DictionaryEntry& entry) {
  return entry.pos == core::PartOfSpeech::Particle && isNominalForcingParticle(entry.extended_pos);
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
      if (hasExactPartOfSpeech(*dict_manager, remainder, kPredicateHostMask)) {
        return true;
      }
      // The particle joins two complete constituents, so a head may only span
      // it when what follows finishes a lexicalized nominal (目の前). A
      // remainder that is no word at all means the head cut into the word
      // after it instead (日のこ|と, where こと is the formal noun).
      constexpr PartOfSpeechMask kNominalMask = partOfSpeechMask(core::PartOfSpeech::Noun) |
                                                partOfSpeechMask(core::PartOfSpeech::Pronoun) |
                                                partOfSpeechMask(core::PartOfSpeech::Suffix);
      // Only the genitive joins constituents into a lexicalized nominal; the
      // subject, object and topic markers never do (音+が+ねこ).
      if (!hasExactPartOfSpeech(*dict_manager, remainder, kNominalMask) ||
          match.entry->extended_pos != core::ExtendedPOS::ParticleNo) {
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
  const size_t first_selector = lookbehindStart(start_pos, kMaximumSelectorLength);
  const auto* attributive_copula =
      codepoints[start_pos - 1] == U'な' ? dict_manager->lookupExact("な", core::PartOfSpeech::Auxiliary) : nullptr;
  constexpr size_t kClosedClassProbeChars = 3;
  const auto head_matches = lookupResultsInRange(*dict_manager, codepoints, start_pos,
                                                 std::min(codepoints.size(), start_pos + kClosedClassProbeChars));
  const bool startsClosedClassWord = std::any_of(head_matches.begin(), head_matches.end(), [](const auto& match) {
    return match.entry != nullptr && match.length >= 2 &&
           (match.entry->pos == core::PartOfSpeech::Particle || match.entry->pos == core::PartOfSpeech::Auxiliary ||
            match.entry->pos == core::PartOfSpeech::Suffix);
  });
  for (size_t selector_start = first_selector; selector_start < start_pos; ++selector_start) {
    // A na-adjective selects a nominal head through its explicit attributive
    // copula (AdjNa+な+X).  The existing adjective probe only recognizes a
    // single AdjBasic edge ending at the head, so inspect this closed two-edge
    // selector separately.  Requiring both AdjNaAdj and AuxCopulaDa leaves
    // ordinary i-adjective attribution (美しい+人) on its existing path.
    // A た/て right after a verb continuative is that verb's past or te-form
    // (帯び+た+まなざし), so no na-adjective stem opens on it or runs across it.
    const bool crosses_selected_ta = [&] {
      constexpr size_t kMaxContinuative = 4;
      for (size_t ta_pos = selector_start; ta_pos + 1 < start_pos; ++ta_pos) {
        if (ta_pos == 0 || !utf8::equalsAny(normalize::encodeUtf8(codepoints[ta_pos]), {"た", "て", "だ", "で"})) {
          continue;
        }
        for (size_t verb_start = ta_pos > kMaxContinuative ? ta_pos - kMaxContinuative : 0; verb_start < ta_pos;
             ++verb_start) {
          if (verb_helpers::isVerbContinuativeSpan(dict_manager, codepoints, verb_start, ta_pos)) {
            return true;
          }
        }
      }
      return false;
    }();
    if (attributive_copula != nullptr && attributive_copula->extended_pos == core::ExtendedPOS::AuxCopulaDa &&
        selector_start + 1 < start_pos && !crosses_selected_ta) {
      const auto* na_adjective =
          lookupEntryInRange(*dict_manager, codepoints, selector_start, start_pos - 1, core::PartOfSpeech::Adjective);
      if (na_adjective != nullptr && na_adjective->extended_pos == core::ExtendedPOS::AdjNaAdj) {
        return true;
      }
      std::vector<UnknownCandidate> na_adjective_candidates;
      generateNaAdjectiveCandidates(codepoints, selector_start, char_types, UnknownOptions{}, inflection, dict_manager,
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
    // A verb's attributive form is its terminal one (疑う+きらい, すぎる+きらい),
    // which selects a nominal head just as an adjective does. The head cannot
    // open on a registered closed-class word of two morae or more, though: that
    // word is the next morpheme of the clause (やる+べき+こと, 急ぐ+より+ほか).
    const auto* exact_verb = dict_manager->lookupExact(selector_surface, core::PartOfSpeech::Verb);
    if (exact_verb != nullptr && selector_surface == exact_verb->lemma && !startsClosedClassWord) {
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

// True when a registered suffix, particle, auxiliary or numeral starts in
// [first_start, end_pos) and ends after end_pos.
bool closedClassEntryCrossesEnd(const dictionary::DictionaryManager& dict_manager,
                                const std::vector<char32_t>& codepoints, size_t first_start, size_t end_pos) {
  for (size_t split = first_start; split < end_pos; ++split) {
    const std::string rest = extractSubstring(codepoints, split, codepoints.size());
    for (const auto& match : dict_manager.lookup(rest, 0)) {
      if (match.entry == nullptr || split + match.length <= end_pos) {
        continue;
      }
      const auto pos = match.entry->pos;
      // A listed numeral is closed class too (身+ひとつ, not 身ひ+とつ).
      if (pos == core::PartOfSpeech::Suffix || pos == core::PartOfSpeech::Particle ||
          pos == core::PartOfSpeech::Auxiliary || match.entry->extended_pos == core::ExtendedPOS::NounNumber) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

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
  // No word opens on a small kana (な+ったん of なった+ん), and no native
  // word on the ら column, so a hiragana head there starts mid-word
  // (なき+らい of な+きらい).
  if (kana::isSmallKanaCodepoint(codepoints[start_pos]) || kana::isRaColumnCodepoint(codepoints[start_pos])) {
    return;
  }
  for (size_t length = 2; length <= kMaximumSelectedHeadLength; ++length) {
    const size_t head_end = start_pos + length;
    // A geminate mora needs the consonant after it, so no nominal closes on っ
    // (ほんの+ちょっ+と is the adverb ちょっと read through its own ending).
    if (head_end > codepoints.size() || codepoints[head_end - 1] == core::hiragana::kSmallTsu ||
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
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
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

    // A registered function word that opens inside the head and runs past its
    // end owns that right edge, so the head would cut it in two (店+ならでは).
    if (closedClassEntryCrossesEnd(*dict_manager, codepoints, start_pos + 1, head_end)) {
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
    SUZUME_DEBUG_CANDIDATE_PATTERN(noun_candidate,
                                   has_genitive_selector ? "genitive_selected_noun" : "attributive_selected_noun");
    candidates.push_back(std::move(noun_candidate));
  }
}

}  // namespace suzume::analysis
