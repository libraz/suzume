/**
 * @file verb_candidates_hiragana.cpp
 * @brief Hiragana-based verb candidate generation (generateHiraganaVerbCandidates)
 *
 * Handles verb candidate generation for pure hiragana patterns.
 * Split from verb_candidates.cpp for maintainability.
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
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis {

namespace vh = verb_helpers;
using namespace hiragana_verb_detail;

namespace {

bool hasInternalPredicateBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t onbin_pos,
                                  const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return false;
  }
  for (size_t boundary = start_pos + 1; boundary < onbin_pos; ++boundary) {
    const std::string tail = extractSubstring(codepoints, boundary, onbin_pos + 1);
    constexpr PartOfSpeechMask kPredicateMask = partOfSpeechMask(core::PartOfSpeech::Verb) |
                                                partOfSpeechMask(core::PartOfSpeech::Adjective) |
                                                partOfSpeechMask(core::PartOfSpeech::Auxiliary);
    if (hasExactPartOfSpeech(*dict_manager, tail, kPredicateMask)) {
      return true;
    }
  }
  return false;
}

bool startsPastAuxiliaryBeforeQuote(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0 || start_pos + 2 >= codepoints.size() ||
      (codepoints[start_pos] != U'た' && codepoints[start_pos] != U'だ') || codepoints[start_pos + 1] != U'っ' ||
      codepoints[start_pos + 2] != U'て') {
    return false;
  }
  const char32_t preceding = codepoints[start_pos - 1];
  const bool follows_onbin = preceding == U'い' || preceding == U'ん' || preceding == core::hiragana::kSmallTsu;
  return follows_onbin && lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1,
                                             core::PartOfSpeech::Auxiliary) != nullptr;
}

size_t closedOnbinTenseEnd(const std::vector<char32_t>& codepoints, size_t start_pos,
                           const std::vector<normalize::CharType>& char_types, const grammar::Inflection& inflection,
                           const dictionary::DictionaryManager* dict_manager) {
  const bool has_left_predicate_boundary =
      start_pos == 0 || normalize::classifyChar(codepoints[start_pos - 1]) == normalize::CharType::Symbol ||
      normalize::isExtendedParticle(codepoints[start_pos - 1]);
  if (!has_left_predicate_boundary) {
    return 0;
  }

  size_t end_pos = start_pos;
  while (end_pos < char_types.size() && end_pos - start_pos < 12 &&
         char_types[end_pos] == normalize::CharType::Hiragana) {
    ++end_pos;
  }
  for (size_t onbin_pos = start_pos + 1; onbin_pos + 1 < end_pos; ++onbin_pos) {
    const char32_t onbin = codepoints[onbin_pos];
    const char32_t tense = codepoints[onbin_pos + 1];
    // Preserve the established particle-initial の exception: a complete
    // inflected predicate such as のっとって is stronger than its internal
    // のっ/とっ homographs.  Other starts require the internal boundary.
    const bool has_internal_predicate = hasInternalPredicateBoundary(codepoints, start_pos, onbin_pos, dict_manager);
    if (codepoints[start_pos] != U'の' && has_internal_predicate) {
      continue;
    }
    const std::string closed_surface = extractSubstring(codepoints, start_pos, onbin_pos + 2);
    constexpr PartOfSpeechMask kClosedSurfaceMask =
        partOfSpeechMask(core::PartOfSpeech::Particle) | partOfSpeechMask(core::PartOfSpeech::Conjunction);
    if (dict_manager != nullptr && hasExactPartOfSpeech(*dict_manager, closed_surface, kClosedSurfaceMask)) {
      continue;
    }
    if (onbin == U'っ' && (tense == U'た' || tense == U'て')) {
      for (const auto& inflection_candidate : inflection.analyze(closed_surface)) {
        const bool is_sokuonbin_godan = inflection_candidate.verb_type == grammar::VerbType::GodanWa ||
                                        inflection_candidate.verb_type == grammar::VerbType::GodanRa ||
                                        inflection_candidate.verb_type == grammar::VerbType::GodanTa;
        if (is_sokuonbin_godan && inflection_candidate.confidence >= candidate::kParticleVerbBoundaryMinConfidence) {
          return onbin_pos + 2;
        }
      }
    }

    // The ma/ba/na rows are indistinguishable in ん+だ.  This is nevertheless
    // decisive boundary evidence for a long predicate stem; it does not claim
    // that the reconstructed lemma row is lexically verified.  Exclude ん+で,
    // which is also compatible with the classical negative ぬ.
    if (onbin == U'ん' && tense == U'だ' && onbin_pos - start_pos >= 3) {
      return onbin_pos + 2;
    }
  }
  return 0;
}

size_t completeIndependentGodanWaTerminalEnd(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const grammar::Inflection& inflection,
                                             const VerbCandidateOptions& verb_opts) {
  const bool has_left_predicate_boundary =
      start_pos == 0 || normalize::classifyChar(codepoints[start_pos - 1]) == normalize::CharType::Symbol ||
      normalize::isExtendedParticle(codepoints[start_pos - 1]);
  if (!has_left_predicate_boundary) {
    return 0;
  }

  size_t end_pos = start_pos;
  while (end_pos < char_types.size() && end_pos - start_pos < 12 &&
         char_types[end_pos] == normalize::CharType::Hiragana) {
    ++end_pos;
  }
  // A long, complete kana run has enough structure to distinguish a lexical
  // wa-row terminal from a short closed-class sequence.  Requiring the run to
  // end here also keeps this gate out of dependent verb/auxiliary chains.
  if (end_pos - start_pos < 4) {
    return 0;
  }

  const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
  for (const auto& candidate : inflection.analyze(surface)) {
    if (candidate.verb_type == grammar::VerbType::GodanWa && candidate.base_form == surface &&
        candidate.morphemes.empty() && candidate.confidence >= verb_opts.confidence_low) {
      return end_pos;
    }
  }
  return 0;
}

size_t completeGodanTerminalAfterCaseParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                              const std::vector<normalize::CharType>& char_types,
                                              const grammar::Inflection& inflection,
                                              const dictionary::DictionaryManager* dict_manager,
                                              const VerbCandidateOptions& verb_opts) {
  if (dict_manager == nullptr || start_pos == 0) {
    return 0;
  }
  const auto* preceding_particle =
      lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle);
  if (preceding_particle == nullptr || preceding_particle->extended_pos != core::ExtendedPOS::ParticleCase) {
    return 0;
  }
  // A case-particle homograph inside a kana predicate (読んでみよう,
  // ぷにぷにしてる) does not open a new predicate slot. Unknown kanji nouns
  // remain valid hosts without requiring lexical registration; kana hosts need
  // an explicit nominal entry ending before the particle.
  const size_t particle_start = start_pos - 1;
  const bool follows_kanji_host = particle_start > 0 && normalize::isKanjiCodepoint(codepoints[particle_start - 1]);
  constexpr PartOfSpeechMask kNominalHostMask =
      partOfSpeechMask(core::PartOfSpeech::Noun) | partOfSpeechMask(core::PartOfSpeech::Pronoun);
  const size_t min_host_start = particle_start > 12 ? particle_start - 12 : 0;
  const bool follows_nominal_host =
      hasDictionaryEntryEndingAt(*dict_manager, codepoints, min_host_start, particle_start, kNominalHostMask);
  if (!follows_kanji_host && !follows_nominal_host) {
    return 0;
  }

  size_t end_pos = start_pos;
  while (end_pos < char_types.size() && end_pos - start_pos < 12 &&
         char_types[end_pos] == normalize::CharType::Hiragana) {
    ++end_pos;
  }
  if (end_pos - start_pos < 3) {
    return 0;
  }

  const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
  for (const auto& candidate : inflection.analyze(surface)) {
    if (grammar::isGodanVerbType(candidate.verb_type) && candidate.base_form == surface &&
        candidate.morphemes.empty() && candidate.confidence >= verb_opts.confidence_standard) {
      return end_pos;
    }
  }
  return 0;
}

bool hasLongGodanWaNegativeEvidence(const std::vector<char32_t>& codepoints, size_t start_pos, size_t current_pos,
                                    const std::vector<normalize::CharType>& char_types) {
  for (size_t negative_pos = current_pos + 1; negative_pos + 2 < codepoints.size() && negative_pos - start_pos < 12;
       ++negative_pos) {
    if (char_types[negative_pos] != normalize::CharType::Hiragana) {
      break;
    }
    if (negative_pos >= start_pos + 3 && codepoints[negative_pos] == U'わ' && codepoints[negative_pos + 1] == U'な' &&
        codepoints[negative_pos + 2] == U'い') {
      return true;
    }
  }
  return false;
}

// A particle-homographic が/や may occur inside an open-class Godan verb.
// Derive the permitted terminal and sokuonbin shapes from the canonical row
// table, rather than treating those morae as unconditional particle breaks.
// The Sa-row escape uses the complete inflectional shape because がす has no
// row-specific marker before its terminal す.
// Returns the predicate stem end, or zero when the remaining run has no
// complete inflectional shape.
//
// The row is fixed to ra rather than taken from the row table, because the
// homograph identifies no row on its own: the sokuonbin っ is shared with
// ka/ta/wa, and the terminal of every other row closes an ordinary nominative
// clause as if it were one verb (ぼくがいく, とりがとぶ). What this rule fails
// to reach — ながす, ころがす, のがす — is held back by the prefix guard below,
// not by the row.
size_t godanContinuationStemEnd(const std::vector<char32_t>& codepoints, size_t start_pos, size_t current_pos,
                                const std::vector<normalize::CharType>& char_types,
                                const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager,
                                const VerbCandidateOptions& verb_opts) {
  if (current_pos >= codepoints.size() || (codepoints[current_pos] != U'が' && codepoints[current_pos] != U'や')) {
    return 0;
  }
  size_t run_end = current_pos;
  while (run_end < char_types.size() && run_end - start_pos < 12 &&
         char_types[run_end] == normalize::CharType::Hiragana) {
    ++run_end;
  }

  const auto* row = grammar::Conjugation::getGodanRow(grammar::VerbType::GodanRa);
  if (row == nullptr) {
    return 0;
  }

  // A complete Sa-row cell is stronger than the lexical status of its prefix:
  // の/な/さ are all closed-class homographs inside ordinary open-class stems.
  // A real predicate beginning at が/や still marks the boundary (で+やる).
  size_t godan_sa_end = 0;
  const std::string full_run = extractSubstring(codepoints, start_pos, run_end);
  for (const auto& candidate : inflection.analyze(full_run)) {
    if (candidate.verb_type == grammar::VerbType::GodanSa && candidate.confidence >= verb_opts.confidence_low) {
      godan_sa_end = run_end;
      break;
    }
  }
  if (dict_manager != nullptr) {
    const std::string prefix = extractSubstring(codepoints, start_pos, current_pos);
    const std::string suffix = extractSubstring(codepoints, current_pos, run_end);
    constexpr PartOfSpeechMask kPredicateMask =
        partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Auxiliary);
    const auto* suffix_entry = dict_manager->lookupExact(suffix);
    const bool suffix_is_garu = suffix_entry != nullptr && suffix_entry->extended_pos == core::ExtendedPOS::AuxGaru;
    const auto* noun_prefix = dict_manager->lookupExact(prefix, core::PartOfSpeech::Noun);
    const bool prefix_can_host_garu = dict_manager->lookupExact(prefix, core::PartOfSpeech::Adjective) != nullptr ||
                                      (noun_prefix != nullptr && noun_prefix->extended_pos == core::ExtendedPOS::Noun);
    const bool suffix_is_licensed_predicate =
        hasExactPartOfSpeech(*dict_manager, suffix, kPredicateMask) && (!suffix_is_garu || prefix_can_host_garu);
    const bool has_long_noun_prefix = prefix.size() >= 6 &&
                                      dict_manager->lookupExact(prefix, core::PartOfSpeech::Particle) == nullptr &&
                                      dict_manager->lookupExact(prefix, core::PartOfSpeech::Noun) != nullptr;
    if (godan_sa_end != 0 && !suffix_is_licensed_predicate) {
      return godan_sa_end;
    }

    // Preserve the established Ra-row prefix guards for shapes where the
    // particle homograph does not itself identify a conjugation row.
    if (const auto* particle = dict_manager->lookupExact(prefix, core::PartOfSpeech::Particle);
        particle != nullptr && particle->extended_pos != core::ExtendedPOS::ParticleTopic &&
        particle->extended_pos != core::ExtendedPOS::ParticleBinding) {
      return 0;
    }
    constexpr core::PartOfSpeech kClosedClassPrefixes[] = {
        core::PartOfSpeech::Adverb,  core::PartOfSpeech::Conjunction, core::PartOfSpeech::Determiner,
        core::PartOfSpeech::Pronoun, core::PartOfSpeech::Prefix,
    };
    for (const core::PartOfSpeech pos : kClosedClassPrefixes) {
      if (dict_manager->lookupExact(prefix, pos) != nullptr) {
        return 0;
      }
    }
    if (has_long_noun_prefix && !(suffix_is_garu && !prefix_can_host_garu)) {
      return 0;
    }
  } else if (godan_sa_end != 0) {
    return godan_sa_end;
  }

  const char32_t onbin = utf8::decodeFirstChar(grammar::onbinFormOf(*row));
  for (size_t pos = current_pos + 1; pos < char_types.size() && pos - start_pos < 12; ++pos) {
    if (char_types[pos] != normalize::CharType::Hiragana) {
      break;
    }
    if (codepoints[pos] == row->base_vowel) {
      return pos + 1;
    }
    if (codepoints[pos] == onbin && pos + 1 < char_types.size() &&
        (codepoints[pos + 1] == U'た' || codepoints[pos + 1] == U'だ' || codepoints[pos + 1] == core::hiragana::kTe)) {
      return pos + 1;
    }
  }

  return 0;
}

bool hasInternalLexicalParticleBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                        const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return false;
  }
  for (size_t lexical_start = start_pos; lexical_start + 1 < end_pos; ++lexical_start) {
    // A leading closed particle is already sufficient evidence that the run
    // did not begin at a lexical verb boundary (は+ここ+で).  Accept a complete
    // compound particle as well, while leaving merely particle-homographic
    // prefixes of real verbs untouched.
    if (lexical_start > start_pos) {
      if (lookupEntryInRange(*dict_manager, codepoints, start_pos, lexical_start, core::PartOfSpeech::Particle) ==
          nullptr) {
        continue;
      }
    }
    for (size_t split = lexical_start + 1; split < end_pos; ++split) {
      if (lookupEntryInRange(*dict_manager, codepoints, split, end_pos, core::PartOfSpeech::Particle) == nullptr) {
        continue;
      }
      const std::string left = extractSubstring(codepoints, lexical_start, split);
      constexpr PartOfSpeechMask kLexicalMask =
          partOfSpeechMask(core::PartOfSpeech::Pronoun) | partOfSpeechMask(core::PartOfSpeech::Noun) |
          partOfSpeechMask(core::PartOfSpeech::Adverb) | partOfSpeechMask(core::PartOfSpeech::Determiner) |
          partOfSpeechMask(core::PartOfSpeech::Conjunction);
      if (hasExactPartOfSpeech(*dict_manager, left, kLexicalMask)) {
        return true;
      }
    }
  }
  return false;
}

void appendHiraganaRenyokeiBeforeAspect(const std::vector<char32_t>& codepoints, size_t start_pos,
                                        const std::vector<normalize::CharType>& char_types,
                                        const dictionary::DictionaryManager* dict_manager,
                                        std::vector<UnknownCandidate>& candidates) {
  if (dict_manager == nullptr) {
    return;
  }
  size_t stem_end = start_pos;
  while (stem_end < char_types.size() && char_types[stem_end] == normalize::CharType::Hiragana) {
    ++stem_end;
  }
  if (stem_end < start_pos + 2 || stem_end >= codepoints.size()) {
    return;
  }
  if (hasInternalLexicalParticleBoundary(codepoints, start_pos, stem_end, dict_manager)) {
    return;
  }

  const std::string following = extractClosedClassProbe(codepoints, stem_end);
  // Kanji-led aspect candidates are generated rather than dictionary-backed;
  // the leading aspect kanji is therefore also accepted as structural evidence.
  const bool aspect_follows =
      codepoints[stem_end] == U'始' ||
      lookupResultsHaveExtendedPOS(dict_manager->lookup(following, 0), core::ExtendedPOS::AuxAspectHajimeru);
  if (!aspect_follows) {
    return;
  }

  const char32_t final_cp = codepoints[stem_end - 1];
  const std::string_view suffix = grammar::godanBaseSuffixFromIRow(final_cp);
  std::string lemma;
  if (grammar::isERowCodepoint(final_cp) || final_cp == U'じ') {
    lemma = extractSubstring(codepoints, start_pos, stem_end) + "る";
  } else if (!suffix.empty()) {
    lemma = normalize::concat(extractSubstring(codepoints, start_pos, stem_end - 1), suffix);
  }
  if (lemma.empty()) {
    return;
  }

  auto candidate = makeVerbCandidate(
      extractSubstring(codepoints, start_pos, stem_end), start_pos, stem_end, candidate::verb_cost::kStrongBonus, lemma,
      dictionary::ConjugationType::None, true, CandidateOrigin::VerbHiragana, candidate::kHighOriginConfidence,
      "hiragana_renyokei_before_aspect", core::ExtendedPOS::VerbRenyokei, "aspect_follower");
  candidate.lemma_verified = true;
  candidates.push_back(std::move(candidate));
}

void appendHiraganaRenyokeiBeforeFormalNoun(const std::vector<char32_t>& codepoints, size_t start_pos,
                                            const std::vector<normalize::CharType>& char_types,
                                            const grammar::Inflection& inflection,
                                            const dictionary::DictionaryManager* dict_manager,
                                            std::vector<UnknownCandidate>& candidates) {
  if (dict_manager == nullptr) {
    return;
  }
  size_t run_end = start_pos;
  while (run_end < char_types.size() && run_end - start_pos < 12 &&
         char_types[run_end] == normalize::CharType::Hiragana) {
    ++run_end;
  }

  // A two-or-more-mora e-row stem immediately before a formal noun is the
  // productive verb-continuative nominal construction (たて+もの). The
  // closed noun supplies the right boundary, so this does not turn arbitrary
  // kana runs into verbs.
  for (size_t stem_end = start_pos + 2; stem_end < run_end; ++stem_end) {
    if (stem_end + 2 > run_end) {
      continue;
    }
    const std::string following = extractSubstring(codepoints, stem_end, stem_end + 2);
    if (following != "もの" ||
        !lookupResultsHaveExtendedPOS(dict_manager->lookup(following, 0), core::ExtendedPOS::NounFormal) ||
        !grammar::isERowCodepoint(codepoints[stem_end - 1])) {
      continue;
    }
    const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
    const std::string terminal = stem + "る";
    float ichidan_terminal_confidence = candidate::kNoConfidence;
    float godan_ra_terminal_confidence = candidate::kNoConfidence;
    for (const auto& candidate : inflection.analyze(terminal)) {
      if (candidate.base_form != terminal || !candidate.morphemes.empty()) {
        continue;
      }
      if (candidate.verb_type == grammar::VerbType::Ichidan) {
        ichidan_terminal_confidence = std::max(ichidan_terminal_confidence, candidate.confidence);
      }
      if (candidate.verb_type == grammar::VerbType::GodanRa) {
        godan_ra_terminal_confidence = std::max(godan_ra_terminal_confidence, candidate.confidence);
      }
    }
    // An e-row stem whose only terminal analysis is Godan-ra is not an
    // independently supported Ichidan continuative. Before a formal noun,
    // keep it as a nominal stem instead of fabricating a verb lemma
    // (たて+もの). A true Ichidan terminal retains the productive verb path.
    if (godan_ra_terminal_confidence >= candidate::verb_cost::kConstructedVerbMinConfidence &&
        godan_ra_terminal_confidence > ichidan_terminal_confidence) {
      candidates.push_back(makeNounCandidate(stem, start_pos, stem_end, candidate::verb_cost::kStrongBonus, true,
                                             CandidateOrigin::VerbHiragana, core::ExtendedPOS::Noun));
      return;
    }
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] " << stem << " hiragana_renyokei_before_formal_noun\n");
    candidates.push_back(makeVerbCandidate(stem, start_pos, stem_end, candidate::verb_cost::kStrongBonus, stem + "る",
                                           dictionary::ConjugationType::Ichidan, true, CandidateOrigin::VerbHiragana,
                                           candidate::kHighOriginConfidence, "hiragana_renyokei_before_formal_noun",
                                           core::ExtendedPOS::VerbRenyokei));
    return;
  }
}

}  // namespace

std::vector<UnknownCandidate> generateHiraganaVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                             const std::vector<normalize::CharType>& char_types,
                                                             const grammar::Inflection& inflection,
                                                             const dictionary::DictionaryManager* dict_manager,
                                                             const VerbCandidateOptions& verb_opts) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // This construction is licensed by its formal-noun follower, even when the
  // stem starts with a mora that is also a closed auxiliary (たてもの).
  appendHiraganaRenyokeiBeforeFormalNoun(codepoints, start_pos, char_types, inflection, dict_manager, candidates);

  const size_t closed_onbin_tense_end =
      closedOnbinTenseEnd(codepoints, start_pos, char_types, inflection, dict_manager);
  const size_t complete_godan_wa_terminal_end =
      completeIndependentGodanWaTerminalEnd(codepoints, start_pos, char_types, inflection, verb_opts);
  const size_t complete_case_particle_terminal_end =
      completeGodanTerminalAfterCaseParticle(codepoints, start_pos, char_types, inflection, dict_manager, verb_opts);

  if (vh::startsInsideDictionaryParticle(codepoints, start_pos, dict_manager)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " inside_dictionary_particle\n");
    return candidates;
  }
  if (vh::startsInsideDictionaryAuxiliary(codepoints, start_pos, dict_manager)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " inside_dictionary_auxiliary\n");
    return candidates;
  }
  if (startsPastAuxiliaryBeforeQuote(codepoints, start_pos, dict_manager)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " past_auxiliary_before_quote\n");
    return candidates;
  }
  // A candidate cannot begin inside an already complete formal noun.  This
  // keeps もの+だっ+た from becoming the fabricated onbin verb のだっ.
  if (start_pos > 0 && dict_manager != nullptr) {
    const auto* preceding_noun =
        lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos + 1, core::PartOfSpeech::Noun);
    if (preceding_noun != nullptr && preceding_noun->extended_pos == core::ExtendedPOS::NounFormal) {
      return candidates;
    }
  }

  // Context-gated irregular 来る mizenkei before a selecting auxiliary.
  appendKkoNominalizerCandidates(codepoints, start_pos, candidates);
  appendSuruInabilityCandidates(codepoints, start_pos, candidates);
  appendEruObligationCandidates(codepoints, start_pos, candidates);
  appendKuruMizenkeiCandidates(codepoints, start_pos, candidates);
  appendKuruRenyokeiCandidates(codepoints, start_pos, dict_manager, candidates);

  // Context-gated directional いく inflections after a clear te-form.
  appendIkuAuxiliaryCandidates(codepoints, start_pos, candidates);

  // Context-gated benefactive やる irrealis before negation.
  appendYaruBenefactiveCandidates(codepoints, start_pos, candidates);

  // Context-gated 試行補助動詞 みる after a clear te-form boundary.
  appendMiruAuxiliaryCandidates(codepoints, start_pos, dict_manager, candidates);
  appendMiseruAuxiliaryCandidates(codepoints, start_pos, dict_manager, candidates);
  appendAgeruBenefactiveCandidates(codepoints, start_pos, dict_manager, candidates);
  // Context-gated 準備補助動詞 おく after a clear te-form boundary.
  appendOkuAuxiliaryCandidates(codepoints, start_pos, candidates);
  appendHiraganaRenyokeiBeforeAspect(codepoints, start_pos, char_types, dict_manager, candidates);

  // Skip if starting with a small kana (拗音・促音: ゃ/ゅ/ょ/っ/ぁ…). No Japanese
  // word starts with a small kana — it always continues the preceding digraph, so
  // any candidate here would cut through it. E.g., おっしゃい must not spawn a
  // fragment verb ゃい (fabricated godan-wa ゃう).
  char32_t first_char = codepoints[start_pos];
  if (kana::isSmallKanaCodepoint(first_char)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " small_kana_start (impossible word start)\n");
    return candidates;
  }

  // A high-confidence inflection analysis is stronger evidence than the
  // conservative initial-particle blacklist. This admits kana-written verbs
  // such as のぞいて and つまずいて without opening bare particle sequences.
  bool has_verified_initial_inflection = false;
  if (first_char == U'の') {
    size_t probe_end = start_pos;
    while (probe_end < char_types.size() && probe_end - start_pos < 12 &&
           char_types[probe_end] == normalize::CharType::Hiragana) {
      ++probe_end;
      if (probe_end <= start_pos + 1) {
        continue;
      }
      const std::string probe = extractSubstring(codepoints, start_pos, probe_end);
      for (const auto& candidate : inflection.analyze(probe)) {
        if (candidate.verb_type == grammar::VerbType::Unknown || candidate.verb_type == grammar::VerbType::IAdjective ||
            candidate.suffix.empty()) {
          continue;
        }
        // A span spelled like its own base form is the one cell the paradigm
        // cannot distinguish from an arbitrary run: every hiragana sequence
        // ending in a う-row kana reconstructs some dictionary form. It is
        // therefore held to the standard acceptance confidence rather than the
        // low bar the inflected cells get, which is the difference between a
        // full stem (のぼる, のこる) and a single leading mora (のる).
        const float threshold =
            candidate.base_form == probe ? verb_opts.confidence_standard : verb_opts.confidence_ichidan_dict;
        if (candidate.confidence >= threshold) {
          has_verified_initial_inflection = true;
          break;
        }
      }
      if (has_verified_initial_inflection) {
        break;
      }
    }
  }

  // Skip if starting character is a particle that is NEVER a verb stem
  // Note: Characters that CAN be verb stems are NOT skipped:
  //   - な→なる/なくす, て→できる, や→やる, か→かける/かえる
  // The initial の is normally a particle, but a following っ+た/て sequence
  // is independent Godan inflectional evidence.  Admit that structurally
  // verified path so kana-written verbs are not cut through their onbin stem.
  bool crossed_particle_guard = normalize::isNeverVerbStemAtStart(first_char);
  if (crossed_particle_guard && closed_onbin_tense_end == 0 && complete_godan_wa_terminal_end == 0 &&
      !has_verified_initial_inflection) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_BLACKLIST] pos=" << start_pos << " char=U+" << std::hex
                                                     << static_cast<uint32_t>(first_char) << std::dec
                                                     << " blocked (isNeverVerbStemAtStart)\n");
    return candidates;
  }

  // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
  // These are commonly mistaken for verbs (これる, それる, etc.)
  // Exception: あれば is the conditional form of ある (verb), not pronoun + particle
  if (start_pos + 1 < codepoints.size()) {
    char32_t second_char = codepoints[start_pos + 1];
    if (normalize::isDemonstrativeStart(first_char, second_char)) {
      // Check if followed by conditional ば - if so, it might be verb conditional form
      // E.g., あれば = ある (verb) + ば, not あれ (pronoun) + ば
      bool is_conditional_form = (start_pos + 2 < codepoints.size() && codepoints[start_pos + 2] == U'ば');
      if (!is_conditional_form && complete_godan_wa_terminal_end == 0) {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos
                                                    << " demonstrative_pronoun (これ/それ/あれ/どれ pattern)\n");
        return candidates;
      }
    }

    // Skip if starting with 「ない」(auxiliary verb/i-adjective for negation)
    // These should be recognized as AUX by dictionary, not as hiragana verbs.
    // E.g., 「ないんだ」→「ない」+「んだ」, not a single verb「ないむ」
    if (first_char == U'な' && second_char == U'い') {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " nai_pattern (ない is auxiliary/adjective)\n");
      return candidates;
    }

    // Skip if starting with 「く」+ な行 (くな, くに, くぬ, くね, くの)
    // These are i-adjective ku-form + なる/ない patterns, not verbs
    // E.g., 「たくなる」→「たく」+「なる」, not a verb「くなる」
    //       「くなってきた」→「く」+「なっ」+「て」+...
    // Note: くる (来る) is a valid verb but has kanji and is handled by dictionary
    if (first_char == U'く') {
      // く + な行 hiragana = i-adjective pattern
      if (second_char == U'な' || second_char == U'に' || second_char == U'ぬ' || second_char == U'ね' ||
          second_char == U'の') {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos
                                                    << " ku_naru_pattern (i-adjective ku-form + なる/ない)\n");
        return candidates;
      }
    }

    // Skip if starting with 「であり」(copula de + aru renyokei)
    // The copula で and the verb stem あり are separate grammatical units.
    // E.g., 「であります」→「で」+「あり」+「ます」, not a single verb「でありる」
    if (first_char == U'で' && second_char == U'あ') {
      char32_t third_char = (start_pos + 2 < codepoints.size()) ? codepoints[start_pos + 2] : 0;
      if (third_char == U'り' || third_char == U'れ' || third_char == U'る' || third_char == U'ろ') {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " deari_pattern (copula de + aru conjugation)\n");
        return candidates;
      }
    }
  }

  // A valid bare continuative before the literal Japanese comma may contain a
  // particle-homographic mora in its lexical stem (かがやき).  Validate the
  // complete run first using both the left case-particle context and the
  // inflection analyzer; only then may the scanner cross those internal morae.
  size_t comma_clause_end = 0;
  size_t comma_probe = start_pos;
  while (comma_probe < char_types.size() && comma_probe - start_pos < 12 &&
         char_types[comma_probe] == normalize::CharType::Hiragana) {
    ++comma_probe;
  }
  if (comma_probe > start_pos + 1 &&
      vh::isCommaClauseChainingRenyokei(codepoints, start_pos, comma_probe, dict_manager)) {
    const std::string comma_surface = extractSubstring(codepoints, start_pos, comma_probe);
    const auto& comma_inflections = inflection.analyze(comma_surface);
    const bool has_valid_renyokei =
        std::any_of(comma_inflections.begin(), comma_inflections.end(), [&](const auto& candidate) {
          return candidate.verb_type != grammar::VerbType::Unknown &&
                 candidate.verb_type != grammar::VerbType::IAdjective && candidate.morphemes.empty() &&
                 candidate.suffix.size() == core::kJapaneseCharBytes &&
                 grammar::isIRowCodepoint(codepoints[comma_probe - 1]) &&
                 candidate.confidence >= verb_opts.confidence_ichidan_dict;
        });
    if (has_valid_renyokei) {
      comma_clause_end = comma_probe;
    }
  }

  // Find hiragana sequence, breaking at particle boundaries
  // Note: Be careful not to break at characters that are part of verb conjugations:
  //   - か can be part of なかった (negative past) or かった (i-adj past)
  //   - で can be part of んで (te-form for godan) or できる (potential verb)
  //   - も can be part of ても (even if) or もらう (receiving verb)
  size_t hiragana_end = start_pos;
  size_t godan_ra_continuation_stem_end = 0;
  while (hiragana_end < char_types.size() && hiragana_end - start_pos < 12 &&  // Max 12 hiragana for verb + endings
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Don't include particles that appear after the first hiragana character.
    // E.g., for "りにする", stop at "り" to not include "にする".
    if (hiragana_end > start_pos) {
      char32_t curr = codepoints[hiragana_end];

      // A complete onbin+tense tail in a predicate slot is stronger evidence
      // than a particle-homograph inside its stem (しゃがんだ, ともった).  Carry
      // the scanner only as far as that closed tail; unrelated hiragana after
      // it still goes through the ordinary boundary checks.
      if (closed_onbin_tense_end != 0 && hiragana_end < closed_onbin_tense_end) {
        crossed_particle_guard =
            crossed_particle_guard || normalize::isNeverVerbStemAfterKanji(curr) || normalize::isExtendedParticle(curr);
        ++hiragana_end;
        continue;
      }
      if (complete_case_particle_terminal_end != 0 && hiragana_end < complete_case_particle_terminal_end) {
        ++hiragana_end;
        continue;
      }
      if (comma_clause_end != 0 && hiragana_end < comma_clause_end) {
        ++hiragana_end;
        continue;
      }

      // Check for particle-like characters (common particles + も, や)
      const bool has_godan_wa_negative =
          hasLongGodanWaNegativeEvidence(codepoints, start_pos, hiragana_end, char_types);
      const size_t godan_ra_stem_end = godanContinuationStemEnd(codepoints, start_pos, hiragana_end, char_types,
                                                                inflection, dict_manager, verb_opts);
      if (godan_ra_stem_end != 0) {
        godan_ra_continuation_stem_end = godan_ra_stem_end;
      }
      // A particle is licensed by the phrase in front of it, and one mora of
      // hiragana is not one unless the dictionary names it (ながれた, うながす:
      // the が is the second mora of a stem, not a case marker). The verb
      // window only widens here — the particle keeps its own edge and every
      // candidate the split reading already had, so the two readings still
      // compete on score rather than on which one was generated.
      const bool particle_lacks_nominal_host =
          hiragana_end == start_pos + 1 && dict_manager != nullptr &&
          lookupEntryInRange(*dict_manager, codepoints, start_pos, hiragana_end, core::PartOfSpeech::Noun) == nullptr &&
          lookupEntryInRange(*dict_manager, codepoints, start_pos, hiragana_end, core::PartOfSpeech::Pronoun) ==
              nullptr;
      if (normalize::isNeverVerbStemAfterKanji(curr) && !(curr == U'の' && has_godan_wa_negative) &&
          !particle_lacks_nominal_host && godan_ra_stem_end == 0) {
        SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] pos=" << hiragana_end << " char=U+" << std::hex
                                                 << static_cast<uint32_t>(curr) << std::dec
                                                 << " action=break (isNeverVerbStemAfterKanji)\n");
        break;  // These are always particles in this context
      }

      // For か, で, も, と: check if they're part of verb conjugation patterns
      // Don't break if they appear in known conjugation contexts
      if (curr == U'か' || curr == U'で' || curr == U'も' || curr == U'と') {
        // Check the preceding character for conjugation patterns
        char32_t prev = codepoints[hiragana_end - 1];

        // か: OK if preceded by な (なかった = negative past)
        //    Also OK if followed by れ (かれ = ichidan stem like つかれる, ふざける)
        //    Also OK if followed by んで/んだ (onbin te/ta-form: つかんで, 歩かんで)
        //    Also OK if followed by A-row + ん (mizenkei + contracted negative: わからん)
        if (curr == U'か') {
          if (prev == U'な') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by れ (ichidan stem pattern)
          if (hiragana_end + 1 < codepoints.size() && codepoints[hiragana_end + 1] == U'れ') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by んで/んだ (GodanMa/Na/Ba onbin te/ta-form)
          // e.g., つかんで (掴んで), 歩かんで (歩かない colloquial negative te-form)
          if (hiragana_end + 2 < codepoints.size() && codepoints[hiragana_end + 1] == U'ん' &&
              (codepoints[hiragana_end + 2] == U'で' || codepoints[hiragana_end + 2] == U'だ')) {
            ++hiragana_end;
            continue;
          }
          // Check if followed by A-row + ん (mizenkei + contracted negative)
          // e.g., わからん = わから (mizenkei of わかる) + ん
          if (hiragana_end + 2 < codepoints.size() && grammar::isARowCodepoint(codepoints[hiragana_end + 1]) &&
              codepoints[hiragana_end + 2] == U'ん') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by ら + な (godan-ra mizenkei + negative auxiliary)
          // e.g., わからない = わから (mizenkei of わかる) + ない
          if (hiragana_end + 2 < codepoints.size() && codepoints[hiragana_end + 1] == U'ら' &&
              codepoints[hiragana_end + 2] == U'な') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by ない (godan-ka mizenkei + negative auxiliary)
          // e.g., いかない = いか (mizenkei of いく) + ない
          if (hiragana_end + 1 < codepoints.size() && codepoints[hiragana_end + 1] == U'な') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by ん + the negative (godan-ra ん音便 + negative
          // auxiliary), e.g. わかんない = わか + ん (ら→ん音便) + ない. The
          // contraction is licensed by the negative that selects the irrealis,
          // and every cell of that paradigm selects the same one, so ask for
          // the paradigm rather than for its dictionary form: enumerating ない
          // alone stopped the run one mora in for わかんなかった.
          if (hiragana_end + 1 < codepoints.size() && codepoints[hiragana_end + 1] == U'ん' &&
              vh::naiNegativeFollowsAt(codepoints, hiragana_end + 2)) {
            ++hiragana_end;
            continue;
          }
          // Check if followed by godan-ra conjugation endings (り、る、れ、ろ、っ)
          // e.g., わかり (renyokei), わかる (shuushikei), わかれ (kateikei/meireikei)
          if (hiragana_end + 1 < codepoints.size()) {
            char32_t next = codepoints[hiragana_end + 1];
            if (next == U'り' || next == U'る' || next == U'れ' || next == U'ろ' || next == U'っ') {
              ++hiragana_end;
              continue;
            }
          }
          // Check if followed by せ/さ/ず (causative/transitive/classical negative)
          // e.g., つかせる, つかさどる, いかず
          if (hiragana_end + 1 < codepoints.size()) {
            char32_t next = codepoints[hiragana_end + 1];
            if (next == U'せ' || next == U'さ' || next == U'ず') {
              ++hiragana_end;
              continue;
            }
          }
          // Check if followed by い + ま (godan-wa renyokei before ます:
          // つかい→使う, むかい→向かう). Guarded by the following ま so bare
          // か + いる sequences (誰か+いる) still break here; the pronoun-か
          // cases are additionally discouraged by the renyokei cost gate.
          if (hiragana_end + 2 < codepoints.size() && codepoints[hiragana_end + 1] == U'い' &&
              codepoints[hiragana_end + 2] == U'ま') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by う (godan-wa shuushikei/rentaikei base form:
          // つかう→使う, むかう→向かう, すう is not か-initial). Scoring rejects
          // the impossible mizenkei+volitational reading (つか+う) separately.
          if (hiragana_end + 1 < codepoints.size() && codepoints[hiragana_end + 1] == U'う') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by わ + {な,れ,せ,ず} (godan-wa mizenkei:
          // つかわない, つかわれる, つかわせる, つかわず). か+わ+ん is already
          // covered by the A-row + ん rule above.
          if (hiragana_end + 2 < codepoints.size() && codepoints[hiragana_end + 1] == U'わ' &&
              (codepoints[hiragana_end + 2] == U'な' || codepoints[hiragana_end + 2] == U'れ' ||
               codepoints[hiragana_end + 2] == U'せ' || codepoints[hiragana_end + 2] == U'ず')) {
            ++hiragana_end;
            continue;
          }
        }

        // で: OK if preceded by ん (んで = te-form) or き (できる)
        if (curr == U'で' && (prev == U'ん' || prev == U'き')) {
          ++hiragana_end;
          continue;
        }

        // も: OK if preceded by て (ても = even if)
        if (curr == U'も' && prev == U'て') {
          ++hiragana_end;
          continue;
        }

        // と: OK if preceded by っ (っとく = ておく contraction)
        // やっとく = やって + おく where ておく → とく
        if (curr == U'と' && prev == U'っ') {
          ++hiragana_end;
          continue;
        }

        if (curr == U'と' && has_godan_wa_negative) {
          ++hiragana_end;
          continue;
        }

        // Otherwise, treat as particle
        SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] pos=" << hiragana_end << " char=U+" << std::hex
                                                 << static_cast<uint32_t>(curr) << std::dec
                                                 << " action=break (unrecognized_particle_context)\n");
        break;
      }
    }
    ++hiragana_end;
  }
  if (complete_godan_wa_terminal_end != 0) {
    hiragana_end = complete_godan_wa_terminal_end;
  }
  if (complete_case_particle_terminal_end != 0) {
    hiragana_end = complete_case_particle_terminal_end;
  }

  // A previously recognized onbin tail can carry the scanner across a
  // particle-homographic や before the ordinary boundary loop reaches it.
  // Probe the final bounded run once so the Godan-ra candidate below is still
  // emitted for forms such as はやった.
  if (godan_ra_continuation_stem_end == 0) {
    for (size_t probe_pos = start_pos + 1; probe_pos < hiragana_end; ++probe_pos) {
      const size_t stem_end =
          godanContinuationStemEnd(codepoints, start_pos, probe_pos, char_types, inflection, dict_manager, verb_opts);
      if (stem_end != 0) {
        godan_ra_continuation_stem_end = stem_end;
        break;
      }
    }
  }

  // Log final hiragana sequence bounds
  SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] final: start=" << start_pos << " end=" << hiragana_end
                                                    << " len=" << (hiragana_end - start_pos) << "\n");

  appendSuruSubsidiaryCandidates(codepoints, start_pos, dict_manager, candidates);

  // Need at least 2 hiragana for a verb
  if (hiragana_end <= start_pos + 1) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " too_short (need >=2 hiragana, got "
                                                << (hiragana_end - start_pos) << ")\n");
    return candidates;
  }

  // する未然形 + passive conditional is always a morpheme chain.  Starting a
  // fresh hiragana verb at its さ would manufacture されれ as an independent
  // predicate and hide the verbal-noun boundary before it.
  const bool starts_suru_passive_conditional =
      codepoints[start_pos] == U'さ' && vh::isPassiveAuxConditionalAt(codepoints, start_pos + 1);
  const bool has_inflected_candidate =
      !starts_suru_passive_conditional &&
      appendInflectedHiraganaVerbCandidates(codepoints, start_pos, hiragana_end, first_char, char_types, inflection,
                                            dict_manager, verb_opts, complete_godan_wa_terminal_end != 0,
                                            complete_case_particle_terminal_end != 0, candidates);
  if (godan_ra_continuation_stem_end != 0) {
    const std::string surface = extractSubstring(codepoints, start_pos, godan_ra_continuation_stem_end);
    // A lexical inflection (notably たがっ or ちがっ) must retain its
    // dictionary POS and conjugation class; the open-class fallback merely
    // fills a dictionary-free gap.
    if (dict_manager != nullptr && (dict_manager->lookupExact(surface, core::PartOfSpeech::Auxiliary) != nullptr ||
                                    dict_manager->lookupExact(surface, core::PartOfSpeech::Verb) != nullptr)) {
      godan_ra_continuation_stem_end = 0;
    }
  }
  if (vh::crossesCaseParticleBeforePredicate(dict_manager, codepoints, start_pos, godan_ra_continuation_stem_end)) {
    godan_ra_continuation_stem_end = 0;
  }
  // Crossing the particle must not also swallow the 結び a binding particle is
  // waiting for. ぞ and なむ demand an attributive at the end of their clause,
  // and a classical auxiliary spells that cell with a bare verbal mora the
  // paradigm tables read as the dictionary form of a non-word: 月ぞ出で+に+ける
  // is the perfect's continuative plus けり, not a form of にける. The agreement
  // is what licenses the split, so an ordinary verb of the same shape keeps its
  // span wherever no binding particle is demanding anything (雪がとける).
  if (godan_ra_continuation_stem_end == codepoints.size() &&
      vh::governingKakariMusubi(dict_manager, codepoints, start_pos) == vh::KakariMusubi::Rentaikei &&
      vh::endsWithClassicalAuxiliary(dict_manager, codepoints, start_pos, godan_ra_continuation_stem_end)) {
    godan_ra_continuation_stem_end = 0;
  }
  if (godan_ra_continuation_stem_end != 0) {
    const auto* row = grammar::Conjugation::getGodanRow(grammar::VerbType::GodanRa);
    const std::string surface = extractSubstring(codepoints, start_pos, godan_ra_continuation_stem_end);
    const bool is_onbin = row != nullptr && codepoints[godan_ra_continuation_stem_end - 1] ==
                                                utf8::decodeFirstChar(grammar::onbinFormOf(*row));
    const std::string lemma = is_onbin ? extractSubstring(codepoints, start_pos, godan_ra_continuation_stem_end - 1) +
                                             normalize::encodeUtf8(row->base_vowel)
                                       : surface;
    auto candidate =
        makeVerbCandidate(surface, start_pos, godan_ra_continuation_stem_end, candidate::verb_cost::kStrongBonus, lemma,
                          dictionary::ConjugationType::GodanRa, true, CandidateOrigin::VerbHiragana,
                          candidate::kHighOriginConfidence, "hiragana_godan_ra_particle_continuation",
                          is_onbin ? core::ExtendedPOS::VerbOnbinkei : core::ExtendedPOS::VerbShuushikei);
    // Crossing a particle-homographic mora verifies only the inflectional
    // shape.  It does not attest the reconstructed open-class lemma.  Marking
    // it as lexical evidence lets the candidate hide real dictionary
    // boundaries inside itself (ゆう|がた|だっ), so leave lemma verification to
    // an actual dictionary match.
    candidate.lemma_verified = false;
    candidates.push_back(std::move(candidate));
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] " << surface << " hiragana_godan_ra_particle_continuation lemma=" << lemma
                                            << " cost=" << candidate::verb_cost::kStrongBonus << "\n");
  }
  // A quoted negative predicate supplies the same irrealis evidence as the
  // ordinary inflected scan, even when a dictionary renyokei candidate was
  // already emitted.  Preserve the Ichidan mizenkei alternative for
  // でき+ない+という instead of allowing the nominal whole-span fallback.
  const bool negative_before_quotative =
      hiragana_end >= start_pos + 2 && codepoints[hiragana_end - 2] == U'な' && codepoints[hiragana_end - 1] == U'い' &&
      dict_manager != nullptr && hiragana_end + 3 <= codepoints.size() &&
      lookupEntryInRange(*dict_manager, codepoints, hiragana_end, hiragana_end + 3, core::PartOfSpeech::Determiner) !=
          nullptr;
  if (!has_inflected_candidate && !negative_before_quotative && closed_onbin_tense_end == 0 &&
      godan_ra_continuation_stem_end == 0) {
    return candidates;
  }
  if (has_inflected_candidate || negative_before_quotative) {
    appendHiraganaDerivedCandidates(codepoints, start_pos, hiragana_end, char_types, inflection, dict_manager,
                                    candidates);
  }
  bool has_i_onbin_tense = false;
  for (size_t pos = start_pos + 1; pos + 1 < hiragana_end; ++pos) {
    if (codepoints[pos] == U'い' && (codepoints[pos + 1] == U'た' || codepoints[pos + 1] == U'て' ||
                                     codepoints[pos + 1] == U'だ' || codepoints[pos + 1] == U'で')) {
      has_i_onbin_tense = true;
      break;
    }
  }
  if (!has_inflected_candidate || has_i_onbin_tense) {
    appendOnbinContractionCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);
  }

  // When the ordinary scanner had to cross a particle-homograph, reconstruct
  // the complete onbin stem from the same closed tense form that licensed the
  // crossing.  This avoids the short-stem row heuristic selecting a fragment
  // (よぎう) or rejecting the complete predicate (ともる).  The surface still
  // cannot prove which homophonous Godan row is lexically correct, so do not
  // mark the lemma as dictionary-verified.
  if (crossed_particle_guard && closed_onbin_tense_end != 0) {
    const size_t onbin_pos = closed_onbin_tense_end - 2;
    const char32_t onbin = codepoints[onbin_pos];
    const std::string inflected_surface = extractSubstring(codepoints, start_pos, closed_onbin_tense_end);
    for (const auto& inflection_candidate : inflection.analyze(inflected_surface)) {
      const bool matching_sokuon = onbin == U'っ' && (inflection_candidate.verb_type == grammar::VerbType::GodanWa ||
                                                      inflection_candidate.verb_type == grammar::VerbType::GodanRa ||
                                                      inflection_candidate.verb_type == grammar::VerbType::GodanTa);
      const bool matching_hatsuon = onbin == U'ん' && (inflection_candidate.verb_type == grammar::VerbType::GodanMa ||
                                                       inflection_candidate.verb_type == grammar::VerbType::GodanBa ||
                                                       inflection_candidate.verb_type == grammar::VerbType::GodanNa);
      if (!matching_sokuon && !matching_hatsuon) {
        continue;
      }
      // The onbin has to be the verb's own cell. The analysis reports where the
      // stem ends, and when that is further left the mora belongs to an
      // auxiliary inside the chain rather than to the verb: のらなかった analyses
      // as a form of のる, whose stem is の and whose onbin is のっ, so the っ
      // here is the negative なかっ. Rebuilding a stem up to it would invent a
      // predicate that swallows the negative auxiliary whole.
      if (start_pos + normalize::utf8Length(inflection_candidate.stem) != onbin_pos) {
        continue;
      }
      // The same reasoning bars an auxiliary standing at the head of the span.
      // An auxiliary predicates over something already complete, so no stem is
      // built on top of one: なかった+ん is the negative's past cell plus the
      // nominalizer, not a form of the non-word なかったむ, even though the
      // analysis places the stem boundary exactly where the ん sits.
      // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
      if (vh::opensOnCompleteAuxiliary(dict_manager, codepoints, start_pos, onbin_pos + 1)) {
        continue;
      }
      const std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_pos + 1);
      candidates.push_back(makeVerbCandidate(
          onbin_surface, start_pos, onbin_pos + 1, candidate::verb_cost::kStandardBonus, inflection_candidate.base_form,
          grammar::verbTypeToConjType(inflection_candidate.verb_type), true, CandidateOrigin::VerbHiragana,
          inflection_candidate.confidence, "hiragana_closed_onbin_tense", core::ExtendedPOS::VerbOnbinkei));
      break;
    }
  }

  // A particle-initial kana run normally loses to a particle analysis.  When
  // the full run independently proves a Godan sokuonbin tense form, however,
  // favor its complete stem over a shorter accidental particle-plus-auxiliary
  // path.  This is limited to the stem immediately before た/て, so internal
  // small-tsu contractions retain their ordinary component analysis.
  if (crossed_particle_guard && closed_onbin_tense_end != 0) {
    for (auto& verb_candidate : candidates) {
      const bool ends_before_tense =
          verb_candidate.extended_pos == core::ExtendedPOS::VerbOnbinkei && verb_candidate.end < codepoints.size() &&
          (codepoints[verb_candidate.end] == U'た' || codepoints[verb_candidate.end] == U'て' ||
           codepoints[verb_candidate.end] == U'だ');
      // What this bonus is meant to outrank is a particle reading of the run's
      // head. A negative auxiliary closing the span is a different competitor:
      // the shorter path there is a verb plus that auxiliary, which is the
      // correct analysis and needs no defending against (のら + なかっ + た, not
      // a form of the non-word のらなかる).
      bool closes_on_negative_auxiliary = false;
      for (size_t pos = start_pos + 1; pos < verb_candidate.end && !closes_on_negative_auxiliary; ++pos) {
        closes_on_negative_auxiliary =
            vh::negativeAuxiliaryLengthAt(dict_manager, codepoints, pos) == verb_candidate.end - pos;
      }
      if (ends_before_tense && !closes_on_negative_auxiliary) {
        verb_candidate.cost += bigram_cost::kDoubleVeryStrongBonus + bigram_cost::kExtraStrongBonus;
      }
    }
  }

  // Add emphatic variants (いくっ, するっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints);

  // Sort by cost
  vh::sortCandidatesByCost(candidates);

  return candidates;
}

}  // namespace suzume::analysis
