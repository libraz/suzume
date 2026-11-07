/**
 * @file verb_candidates_hiragana.cpp
 * @brief Hiragana-based verb candidate generation (generateHiraganaVerbCandidates)
 *
 * Handles verb candidate generation for pure hiragana patterns.
 * Split from verb_candidates.cpp for maintainability.
 */

#include <algorithm>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/verb_candidates_absorption_guards.h"
#include "analysis/verb_candidates_auxiliary_patterns.h"
#include "analysis/verb_candidates_classical.h"
#include "analysis/verb_candidates_dictionary_probes.h"
#include "analysis/verb_candidates_emphatic.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection_scorer_constants.h"
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
  // The internal predicate only splits the run when what stands before it is
  // itself a word (それ+もっ); くぐ+もっ leaves a non-word head. Behind the
  // honorific prefix the head is a verb cell the dictionary need not list
  // (お+かけ+いたし), so there the internal predicate alone keeps the boundary.
  const bool opens_on_prefix =
      lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Prefix) != nullptr;
  for (size_t boundary = start_pos + 1; boundary < onbin_pos; ++boundary) {
    const std::string tail = extractSubstring(codepoints, boundary, onbin_pos + 1);
    if (hasExactPartOfSpeech(*dict_manager, tail, kPredicateHostMask) &&
        (opens_on_prefix || lookupEntryInRange(*dict_manager, codepoints, start_pos, boundary) != nullptr)) {
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
  if (!opensPredicateSlot(codepoints, start_pos)) {
    return 0;
  }

  const size_t end_pos = findCharRegionEnd(char_types, start_pos, kPredicateRunMax, normalize::CharType::Hiragana);
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

    // The sa row takes no onbin: its continuative し before た/て or the polite
    // ま closes the predicate the same way (うごか+し+た, ごまか+し+ます). The
    // polite tail only analyzes together with its own inflected morae.
    if (onbin == U'し' && (tense == U'た' || tense == U'て' || tense == U'ま') && onbin_pos - start_pos >= 2) {
      // The a-row mora before し is the transitive -asu derivation (あまやか+す),
      // so it does not lengthen the root.
      const std::u32string_view stem(codepoints.data() + start_pos, onbin_pos - start_pos);
      const float min_confidence =
          grammar::isTransitiveAsuStem(stem)
              ? candidate::kParticleVerbBoundaryMinConfidence -
                    (grammar::inflection::kPenaltyStemVeryLong - grammar::inflection::kPenaltyStemLong)
              : candidate::kParticleVerbBoundaryMinConfidence;
      constexpr size_t kPoliteTailMax = 4;
      const size_t probe_limit = std::min(end_pos, onbin_pos + 1 + kPoliteTailMax);
      for (size_t surface_end = onbin_pos + 2; surface_end <= probe_limit; ++surface_end) {
        for (const auto& inflection_candidate : analysesInRange(inflection, codepoints, start_pos, surface_end)) {
          if (inflection_candidate.verb_type == grammar::VerbType::GodanSa &&
              start_pos + normalize::utf8Length(inflection_candidate.stem) == onbin_pos &&
              inflection_candidate.confidence >= min_confidence) {
            return onbin_pos + 2;
          }
        }
      }
    }
  }
  return 0;
}

size_t completeIndependentGodanWaTerminalEnd(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const grammar::Inflection& inflection,
                                             const VerbCandidateOptions& verb_opts) {
  if (!opensPredicateSlot(codepoints, start_pos)) {
    return 0;
  }

  const size_t end_pos = findCharRegionEnd(char_types, start_pos, kPredicateRunMax, normalize::CharType::Hiragana);
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
  // A case-particle homograph inside a kana predicate (読んでみよう,
  // ぷにぷにしてる) does not open a new predicate slot. Unknown kanji nouns
  // remain valid hosts without requiring lexical registration; kana hosts need
  // an explicit nominal entry ending before the particle.
  if (!followsKanjiOrNominalHostBeforeCaseParticle(codepoints, start_pos, dict_manager)) {
    return 0;
  }

  const size_t end_pos = findCharRegionEnd(char_types, start_pos, kPredicateRunMax, normalize::CharType::Hiragana);
  if (end_pos - start_pos < 3) {
    return 0;
  }

  const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
  // One mora before する is a light-verb host, not a Godan stem: 声が+け+する is
  // the noun plus the suru-verb, while the Godan verbs of that shape (こする,
  // ゆする) are registered.
  if (end_pos - start_pos == 3 && utf8::endsWith(surface, "する") && !vh::isVerbInDictionary(dict_manager, surface)) {
    return 0;
  }
  for (const auto& candidate : inflection.analyze(surface)) {
    if (isCompleteGodanTerminal(candidate, surface) && candidate.confidence >= verb_opts.confidence_standard) {
      return end_pos;
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
  const size_t stem_end = findCharRegionEnd(char_types, start_pos, char_types.size(), normalize::CharType::Hiragana);
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
  if (kana::isERowCodepoint(final_cp) || final_cp == U'じ') {
    lemma = extractSubstring(codepoints, start_pos, stem_end) + "る";
  } else if (!suffix.empty()) {
    lemma = normalize::concat(extractSubstring(codepoints, start_pos, stem_end - 1), suffix);
  }
  if (lemma.empty()) {
    return;
  }

  auto candidate = makeVerbCandidate(codepoints, start_pos, stem_end, candidate::verb_cost::kStrongBonus, lemma,
                                     dictionary::ConjugationType::None, true, CandidateOrigin::VerbHiragana,
                                     candidate::kHighOriginConfidence, "hiragana_renyokei_before_aspect",
                                     core::ExtendedPOS::VerbRenyokei, "aspect_follower");
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
  const size_t run_end = findCharRegionEnd(char_types, start_pos, kPredicateRunMax, normalize::CharType::Hiragana);

  // A two-or-more-mora e-row stem immediately before a formal noun is the
  // productive verb-continuative nominal construction (たて+もの). The
  // closed noun supplies the right boundary, so this does not turn arbitrary
  // kana runs into verbs.
  for (size_t stem_end = start_pos + 2; stem_end < run_end; ++stem_end) {
    if (stem_end + 2 > run_end) {
      continue;
    }
    if (codepoints[stem_end] != U'も' || codepoints[stem_end + 1] != U'の') {
      continue;
    }
    const std::string following = extractSubstring(codepoints, stem_end, stem_end + 2);
    if (!lookupResultsHaveExtendedPOS(dict_manager->lookup(following, 0), core::ExtendedPOS::NounFormal) ||
        !kana::isERowCodepoint(codepoints[stem_end - 1])) {
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

bool opensInsideClosedClassWord(const HiraganaVerbScan& scan) {
  const auto& codepoints = scan.codepoints;
  const size_t start_pos = scan.start_pos;
  const auto* dict_manager = scan.dict_manager;

  if (vh::startsInsideDictionaryParticle(codepoints, start_pos, dict_manager)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " inside_dictionary_particle\n");
    return true;
  }
  if (vh::startsInsideDictionaryAuxiliary(codepoints, start_pos, dict_manager)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " inside_dictionary_auxiliary\n");
    return true;
  }
  if (vh::startsInsideDictionaryIAdjective(codepoints, start_pos, dict_manager)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " inside_dictionary_i_adjective\n");
    return true;
  }
  if (startsPastAuxiliaryBeforeQuote(codepoints, start_pos, dict_manager)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " past_auxiliary_before_quote\n");
    return true;
  }
  // A candidate cannot begin inside an already complete formal noun.  This
  // keeps もの+だっ+た from becoming the fabricated onbin verb のだっ.
  if (start_pos > 0 && dict_manager != nullptr) {
    const auto* preceding_noun =
        lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos + 1, core::PartOfSpeech::Noun);
    if (preceding_noun != nullptr && preceding_noun->extended_pos == core::ExtendedPOS::NounFormal) {
      return true;
    }
  }
  return false;
}

void appendContextGatedAuxiliaryCandidates(const HiraganaVerbScan& scan, std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = scan.codepoints;
  const size_t start_pos = scan.start_pos;
  const auto* dict_manager = scan.dict_manager;

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
  appendHiraganaRenyokeiBeforeAspect(codepoints, start_pos, scan.char_types, dict_manager, candidates);
}

bool hasVerifiedInitialInflection(const HiraganaVerbScan& scan) {
  const size_t start_pos = scan.start_pos;
  const char32_t first_char = scan.codepoints[start_pos];

  // A high-confidence inflection analysis is stronger evidence than the
  // conservative initial-particle blacklist. This admits kana-written verbs
  // such as のぞいて and つまずいて without opening bare particle sequences.
  if (first_char != U'の') {
    return false;
  }
  size_t probe_end = start_pos;
  while (probe_end < scan.char_types.size() && probe_end - start_pos < kPredicateRunMax &&
         scan.char_types[probe_end] == normalize::CharType::Hiragana) {
    ++probe_end;
    if (probe_end <= start_pos + 1) {
      continue;
    }
    const std::string probe = extractSubstring(scan.codepoints, start_pos, probe_end);
    for (const auto& candidate : scan.inflection.analyze(probe)) {
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
          candidate.base_form == probe ? scan.verb_opts.confidence_standard : scan.verb_opts.confidence_ichidan_dict;
      if (candidate.confidence >= threshold) {
        return true;
      }
    }
  }
  return false;
}

// A ra-row irrealis contracted to ん before the colloquial negative.
struct ContractedIrrealis {
  bool contracted_before_colloquial_negative{false};
  size_t contracted_irrealis_end{0};
};

ContractedIrrealis findContractedIrrealis(const HiraganaVerbScan& scan) {
  const auto& codepoints = scan.codepoints;

  // A ra-row irrealis contracted to ん before the colloquial negative
  // (わかん+ねえ) is no inflected run of its own, but its uncontracted spelling
  // (わから+ない) verifies the same cell the derived generators read before ない.
  bool contracted_before_colloquial_negative = false;
  size_t contracted_irrealis_end = 0;
  for (size_t pos = scan.start_pos + 1;
       pos < scan.char_types.size() && scan.char_types[pos] == normalize::CharType::Hiragana &&
       !contracted_before_colloquial_negative;
       ++pos) {
    if (codepoints[pos] != U'ん' || !vh::colloquialNegativeFollowsAt(codepoints, pos + 1)) {
      continue;
    }
    const std::string stem = extractSubstring(codepoints, scan.start_pos, pos);
    const std::string base_form = stem + "る";
    contracted_before_colloquial_negative =
        vh::isVerbInDictionary(scan.dict_manager, base_form) ||
        vh::readsAsBaseForm(scan.inflection, stem + "らない", base_form, grammar::VerbType::GodanRa);
    contracted_irrealis_end = contracted_before_colloquial_negative ? pos + 1 : 0;
  }
  return {contracted_before_colloquial_negative, contracted_irrealis_end};
}

bool opensOnNonVerbPattern(const HiraganaVerbScan& scan, size_t complete_godan_wa_terminal_end) {
  const auto& codepoints = scan.codepoints;
  const size_t start_pos = scan.start_pos;
  const char32_t first_char = codepoints[start_pos];

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
        return true;
      }
    }

    // Skip if starting with 「ない」(auxiliary verb/i-adjective for negation)
    // These should be recognized as AUX by dictionary, not as hiragana verbs.
    // E.g., 「ないんだ」→「ない」+「んだ」, not a single verb「ないむ」
    if (first_char == U'な' && second_char == U'い') {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " nai_pattern (ない is auxiliary/adjective)\n");
      return true;
    }

    // Skip if starting with 「く」+ な行 (くな, くに, くぬ, くね, くの)
    // These are i-adjective ku-form + なる/ない patterns, not verbs
    // E.g., 「たくなる」→「たく」+「なる」, not a verb「くなる」
    //       「くなってきた」→「く」+「なっ」+「て」+...
    // Note: くる (来る) is a valid verb but has kanji and is handled by dictionary
    if (first_char == U'く' && (second_char == U'な' || second_char == U'に' || second_char == U'ぬ' ||
                                second_char == U'ね' || second_char == U'の')) {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos
                                                  << " ku_naru_pattern (i-adjective ku-form + なる/ない)\n");
      return true;
    }

    // Skip if starting with 「であり」(copula de + aru renyokei)
    // The copula で and the verb stem あり are separate grammatical units.
    // E.g., 「であります」→「で」+「あり」+「ます」, not a single verb「でありる」
    if (first_char == U'で' && second_char == U'あ') {
      char32_t third_char = (start_pos + 2 < codepoints.size()) ? codepoints[start_pos + 2] : 0;
      if (third_char == U'り' || third_char == U'れ' || third_char == U'る' || third_char == U'ろ') {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " deari_pattern (copula de + aru conjugation)\n");
        return true;
      }
    }
  }
  return false;
}

// End of a run validated as a bare continuative before the Japanese comma, or
// 0 when there is none.
size_t commaClauseRenyokeiEnd(const HiraganaVerbScan& scan) {
  const auto& codepoints = scan.codepoints;
  const size_t start_pos = scan.start_pos;

  // A valid bare continuative before the literal Japanese comma may contain a
  // particle-homographic mora in its lexical stem (かがやき).  Validate the
  // complete run first using both the left case-particle context and the
  // inflection analyzer; only then may the scanner cross those internal morae.
  size_t comma_clause_end = 0;
  const size_t comma_probe =
      findCharRegionEnd(scan.char_types, start_pos, kPredicateRunMax, normalize::CharType::Hiragana);
  if (comma_probe > start_pos + 1 &&
      vh::isCommaClauseChainingRenyokei(codepoints, start_pos, comma_probe, scan.dict_manager)) {
    const auto& comma_inflections = analysesInRange(scan.inflection, codepoints, start_pos, comma_probe);
    const bool has_valid_renyokei =
        std::any_of(comma_inflections.begin(), comma_inflections.end(), [&](const auto& candidate) {
          return candidate.verb_type != grammar::VerbType::Unknown &&
                 candidate.verb_type != grammar::VerbType::IAdjective && candidate.morphemes.empty() &&
                 candidate.suffix.size() == core::kJapaneseCharBytes &&
                 kana::isIRowCodepoint(codepoints[comma_probe - 1]) &&
                 candidate.confidence >= scan.verb_opts.confidence_ichidan_dict;
        });
    if (has_valid_renyokei) {
      comma_clause_end = comma_probe;
    }
  }
  return comma_clause_end;
}

// The Godan-ra continuation end left once lexical inflections, an interior
// case particle, and a demanded musubi have claimed theirs; 0 when none is left.
size_t admittedGodanRaContinuationEnd(const HiraganaVerbScan& scan, size_t godan_ra_continuation_stem_end) {
  const auto& codepoints = scan.codepoints;
  const size_t start_pos = scan.start_pos;
  const auto* dict_manager = scan.dict_manager;

  if (godan_ra_continuation_stem_end != 0) {
    const std::string surface = extractSubstring(codepoints, start_pos, godan_ra_continuation_stem_end);
    // A lexical inflection (notably たがっ or ちがっ) must retain its
    // dictionary POS and conjugation class; the open-class fallback merely
    // fills a dictionary-free gap.
    if (dict_manager != nullptr && hasExactPartOfSpeech(*dict_manager, surface,
                                                        partOfSpeechMask(core::PartOfSpeech::Auxiliary) |
                                                            partOfSpeechMask(core::PartOfSpeech::Verb))) {
      godan_ra_continuation_stem_end = 0;
    }
  }
  if (vh::crossesCaseParticleBeforePredicate(dict_manager, codepoints, start_pos, godan_ra_continuation_stem_end)) {
    godan_ra_continuation_stem_end = 0;
  }
  // After a te-form the span opens a subsidiary verb: a one-mora continuative
  // the dictionary lists (as a verb or a subsidiary), then a listed auxiliary cell (て+き+やがっ, て+い+やがっ),
  // not one verb spelled across both.
  if (godan_ra_continuation_stem_end > start_pos + 2 && dict_manager != nullptr &&
      isClearTeFormBeforeSubsidiary(codepoints, start_pos, false) &&
      (lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Verb) != nullptr ||
       lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Auxiliary) !=
           nullptr) &&
      lookupEntryInRange(*dict_manager, codepoints, start_pos + 1, godan_ra_continuation_stem_end,
                         core::PartOfSpeech::Auxiliary) != nullptr) {
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
  return godan_ra_continuation_stem_end;
}

void appendGodanRaContinuationCandidate(const HiraganaVerbScan& scan, size_t godan_ra_continuation_stem_end,
                                        std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = scan.codepoints;
  const size_t start_pos = scan.start_pos;

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
}

bool hasIOnbinTense(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end) {
  bool has_i_onbin_tense = false;
  for (size_t pos = start_pos + 1; pos + 1 < hiragana_end; ++pos) {
    if (codepoints[pos] == U'い' && (codepoints[pos + 1] == U'た' || codepoints[pos + 1] == U'て' ||
                                     codepoints[pos + 1] == U'だ' || codepoints[pos + 1] == U'で')) {
      has_i_onbin_tense = true;
      break;
    }
  }
  return has_i_onbin_tense;
}

void appendClosedOnbinTenseStem(const HiraganaVerbScan& scan, size_t closed_onbin_tense_end,
                                std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = scan.codepoints;
  const size_t start_pos = scan.start_pos;

  // When the ordinary scanner had to cross a particle-homograph, reconstruct
  // the complete onbin stem from the same closed tense form that licensed the
  // crossing.  This avoids the short-stem row heuristic selecting a fragment
  // (よぎう) or rejecting the complete predicate (ともる).  The surface still
  // cannot prove which homophonous Godan row is lexically correct, so do not
  // mark the lemma as dictionary-verified.
  const size_t onbin_pos = closed_onbin_tense_end - 2;
  const char32_t onbin = codepoints[onbin_pos];
  // When one of the homophonous rows names a dictionary verb, that row is the
  // reading (まよっ+て is まよう, not まよる); otherwise keep the analysis order.
  const auto analyses = analysesInRange(scan.inflection, codepoints, start_pos, closed_onbin_tense_end);
  const grammar::InflectionCandidate* chosen = nullptr;
  for (const auto& inflection_candidate : analyses) {
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
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
    if (vh::opensOnCompleteAuxiliary(scan.dict_manager, codepoints, start_pos, onbin_pos + 1)) {
      continue;
    }
    if (vh::isVerbInDictionary(scan.dict_manager, inflection_candidate.base_form)) {
      chosen = &inflection_candidate;
      break;
    }
    if (chosen == nullptr) {
      chosen = &inflection_candidate;
    }
  }
  if (chosen == nullptr) {
    return;
  }
  const std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_pos + 1);
  candidates.push_back(makeVerbCandidate(onbin_surface, start_pos, onbin_pos + 1, candidate::verb_cost::kStandardBonus,
                                         chosen->base_form, grammar::verbTypeToConjType(chosen->verb_type), true,
                                         CandidateOrigin::VerbHiragana, chosen->confidence,
                                         "hiragana_closed_onbin_tense", core::ExtendedPOS::VerbOnbinkei));
}

void favorClosedOnbinTenseStems(const HiraganaVerbScan& scan, std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = scan.codepoints;

  // A particle-initial kana run normally loses to a particle analysis.  When
  // the full run independently proves a Godan sokuonbin tense form, however,
  // favor its complete stem over a shorter accidental particle-plus-auxiliary
  // path.  This is limited to the stem immediately before た/て, so internal
  // small-tsu contractions retain their ordinary component analysis.
  for (auto& verb_candidate : candidates) {
    const bool ends_before_tense = verb_candidate.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
                                   verb_candidate.end < codepoints.size() &&
                                   (codepoints[verb_candidate.end] == U'た' ||
                                    codepoints[verb_candidate.end] == U'て' || codepoints[verb_candidate.end] == U'だ');
    // What this bonus is meant to outrank is a particle reading of the run's
    // head. A negative auxiliary closing the span is a different competitor:
    // the shorter path there is a verb plus that auxiliary, which is the
    // correct analysis and needs no defending against (のら + なかっ + た, not
    // a form of the non-word のらなかる).
    bool closes_on_negative_auxiliary = false;
    for (size_t pos = scan.start_pos + 1; pos < verb_candidate.end && !closes_on_negative_auxiliary; ++pos) {
      closes_on_negative_auxiliary =
          vh::negativeAuxiliaryLengthAt(scan.dict_manager, codepoints, pos) == verb_candidate.end - pos;
    }
    if (ends_before_tense && !closes_on_negative_auxiliary) {
      verb_candidate.cost += bigram_cost::kDoubleVeryStrongBonus + bigram_cost::kExtraStrongBonus;
    }
  }
}

void dropUnattestedMisreadings(const std::vector<char32_t>& codepoints, std::vector<UnknownCandidate>& candidates) {
  // An unattested verb covering only the first half of a reduplication cuts a
  // mimetic in two (ぞく+ぞく+する, not ぞくぞく+する), and one spelling the whole
  // reduplication is the mimetic itself (みるみる+うち).
  // A terminal verb reaches the copula only through だろ (降る+だろ+う), so an
  // unattested one before a bare だ is a nominal misread (ふつう+だ).
  candidates.erase(
      std::remove_if(candidates.begin(), candidates.end(),
                     [&codepoints](const UnknownCandidate& verb_candidate) {
                       if (verb_candidate.lemma_verified) {
                         return false;
                       }
                       const size_t span = verb_candidate.end - verb_candidate.start;
                       const auto first = codepoints.begin() + verb_candidate.start;
                       const bool halves_reduplication = span >= 2 && verb_candidate.end + span <= codepoints.size() &&
                                                         std::equal(first, first + span, first + span);
                       const bool spells_reduplication =
                           span >= 4 && span % 2 == 0 && std::equal(first, first + span / 2, first + span / 2);
                       const size_t next = verb_candidate.end;
                       const bool bare_da_after_terminal =
                           verb_candidate.extended_pos == core::ExtendedPOS::VerbShuushikei &&
                           next < codepoints.size() && codepoints[next] == U'だ' &&
                           (next + 1 >= codepoints.size() || codepoints[next + 1] != U'ろ');
                       return halves_reduplication || spells_reduplication || bare_da_after_terminal;
                     }),
      candidates.end());
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
  const HiraganaVerbScan scan{codepoints, start_pos, char_types, inflection, dict_manager, verb_opts};

  // This construction is licensed by its formal-noun follower, even when the
  // stem starts with a mora that is also a closed auxiliary (たてもの).
  appendHiraganaRenyokeiBeforeFormalNoun(codepoints, start_pos, char_types, inflection, dict_manager, candidates);
  appendSubjectGenitiveIchidanBeforeFormalNoun(codepoints, start_pos, inflection, dict_manager, candidates);

  if (opensInsideClosedClassWord(scan)) {
    return candidates;
  }

  const size_t closed_onbin_tense_end =
      closedOnbinTenseEnd(codepoints, start_pos, char_types, inflection, dict_manager);
  const size_t complete_godan_wa_terminal_end =
      completeIndependentGodanWaTerminalEnd(codepoints, start_pos, char_types, inflection, verb_opts);
  const size_t complete_case_particle_terminal_end =
      completeGodanTerminalAfterCaseParticle(codepoints, start_pos, char_types, inflection, dict_manager, verb_opts);

  appendContextGatedAuxiliaryCandidates(scan, candidates);

  // Skip if starting with a small kana (拗音・促音: ゃ/ゅ/ょ/っ/ぁ…). No Japanese
  // word starts with a small kana — it always continues the preceding digraph, so
  // any candidate here would cut through it. E.g., おっしゃい must not spawn a
  // fragment verb ゃい (fabricated godan-wa ゃう).
  char32_t first_char = codepoints[start_pos];
  if (kana::isSmallKanaCodepoint(first_char)) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " small_kana_start (impossible word start)\n");
    return candidates;
  }

  // Skip if starting character is a particle that is NEVER a verb stem
  // Note: Characters that CAN be verb stems are NOT skipped:
  //   - な→なる/なくす, て→できる, や→やる, か→かける/かえる
  // The initial の is normally a particle, but a following っ+た/て sequence
  // is independent Godan inflectional evidence.  Admit that structurally
  // verified path so kana-written verbs are not cut through their onbin stem.
  const auto [contracted_before_colloquial_negative, contracted_irrealis_end] = findContractedIrrealis(scan);
  bool crossed_particle_guard = normalize::isNeverVerbStemAtStart(first_char);
  if (crossed_particle_guard && closed_onbin_tense_end == 0 && complete_godan_wa_terminal_end == 0 &&
      !hasVerifiedInitialInflection(scan) && !contracted_before_colloquial_negative) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_BLACKLIST] pos=" << start_pos << " char=U+" << std::hex
                                                     << static_cast<uint32_t>(first_char) << std::dec
                                                     << " blocked (isNeverVerbStemAtStart)\n");
    return candidates;
  }

  if (opensOnNonVerbPattern(scan, complete_godan_wa_terminal_end)) {
    return candidates;
  }

  const size_t comma_clause_end = commaClauseRenyokeiEnd(scan);
  size_t godan_ra_continuation_stem_end = 0;
  const size_t hiragana_end =
      scanPredicateRunEnd(scan,
                          {closed_onbin_tense_end, complete_godan_wa_terminal_end, complete_case_particle_terminal_end,
                           comma_clause_end, contracted_irrealis_end},
                          crossed_particle_guard, godan_ra_continuation_stem_end);

  appendSuruSubsidiaryCandidates(codepoints, start_pos, dict_manager, candidates);

  // Need at least 2 hiragana for a verb
  if (hiragana_end <= start_pos + 1) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos << " too_short (need >=2 hiragana, got "
                                                << (hiragana_end - start_pos) << ")\n");
    return candidates;
  }

  const bool has_inflected_candidate = appendInflectedHiraganaVerbCandidates(
      codepoints, start_pos, hiragana_end, first_char, char_types, inflection, dict_manager, verb_opts,
      complete_godan_wa_terminal_end != 0, complete_case_particle_terminal_end != 0, candidates);
  godan_ra_continuation_stem_end = admittedGodanRaContinuationEnd(scan, godan_ra_continuation_stem_end);
  appendGodanRaContinuationCandidate(scan, godan_ra_continuation_stem_end, candidates);
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
      godan_ra_continuation_stem_end == 0 && !contracted_before_colloquial_negative) {
    return candidates;
  }
  if (has_inflected_candidate || negative_before_quotative || contracted_before_colloquial_negative) {
    appendHiraganaDerivedCandidates(codepoints, start_pos, hiragana_end, char_types, inflection, dict_manager,
                                    candidates);
  }
  const bool has_i_onbin_tense = hasIOnbinTense(codepoints, start_pos, hiragana_end);
  if (!has_inflected_candidate || has_i_onbin_tense) {
    appendOnbinContractionCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);
  }

  if (crossed_particle_guard && closed_onbin_tense_end != 0) {
    appendClosedOnbinTenseStem(scan, closed_onbin_tense_end, candidates);
    favorClosedOnbinTenseStems(scan, candidates);
  }

  dropUnattestedMisreadings(codepoints, candidates);

  // Add emphatic variants (いくっ, するっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints);

  // Sort by cost
  vh::sortCandidatesByCost(candidates);

  return candidates;
}

}  // namespace suzume::analysis
