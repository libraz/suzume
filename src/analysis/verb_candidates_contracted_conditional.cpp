/**
 * @file verb_candidates_contracted_conditional.cpp
 * @brief Colloquial contraction of the hypothetical form
 */

#include <algorithm>

#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/verb_candidates_helpers.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis {

namespace vh = verb_helpers;

namespace {

// The hypothetical loses its ば by fusing the particle into the preceding e-row
// mora, which shifts to the i-row and takes a palatal ゃ: 行けば → 行きゃ,
// 話せば → 話しゃ, 食べれば → 食べりゃ, すれば → すりゃ, 早ければ → 早けりゃ.
// The reverse mapping is therefore the plain Godan row table, whatever the
// conjugation class of the word itself: every hypothetical ends in an e-row
// mora, and the classes that end in れ all map back through the ら row.
std::string uncontractHypothetical(const std::vector<char32_t>& codepoints, size_t start_pos, size_t contracted_end) {
  const grammar::VerbType row_type = grammar::verbTypeFromIRowCodepoint(codepoints[contracted_end - 2]);
  const auto* row = grammar::Conjugation::getGodanRow(row_type);
  if (row == nullptr) {
    return {};
  }
  return extractSubstring(codepoints, start_pos, contracted_end - 2) + normalize::encodeUtf8(row->e_row) + "ば";
}

// Lexical evidence outranks the analyzer's confidence when choosing which
// reading the reconstruction names: the hypothetical of an i-adjective and the
// hypothetical of a fabricated Ichidan verb spell the same けれ, and the
// fabrication scores higher on shape alone (早ければ reads as a form of the
// non-word 早ける before it reads as 早い). A Godan potential is derived rather
// than listed, so its own source verb counts as attestation (飲めりゃ → 飲める
// → 飲む).
bool namesAttestedPredicate(const dictionary::DictionaryManager* dict_manager, const std::string& base_form) {
  if (vh::isVerbInDictionary(dict_manager, base_form) || vh::isAdjectiveInDictionary(dict_manager, base_form)) {
    return true;
  }
  if (!utf8::endsWith(base_form, "る") || normalize::utf8Length(base_form) < 3) {
    return false;
  }
  const std::vector<char32_t> base_points = normalize::utf8::decode(base_form);
  const std::string_view godan_ending = grammar::godanBaseSuffixFromERow(base_points[base_points.size() - 2]);
  if (godan_ending.empty()) {
    return false;
  }
  return vh::isVerbInDictionary(
      dict_manager, normalize::concat(extractSubstring(base_points, 0, base_points.size() - 2), godan_ending));
}

// Pick the reading the reconstruction names, or nullptr when the span is not a
// contraction at all.
const grammar::InflectionCandidate* readContractedHypothetical(const std::vector<char32_t>& codepoints,
                                                               size_t start_pos, size_t contracted_end,
                                                               const grammar::Inflection& inflection,
                                                               const dictionary::DictionaryManager* dict_manager) {
  if (contracted_end < start_pos + 3 || contracted_end > codepoints.size() || codepoints[contracted_end - 1] != U'ゃ') {
    return nullptr;
  }
  // A case particle marks an argument boundary, so no single predicate spans
  // one (本が読めりゃ is 本 + が + 読めりゃ). The fused mora itself is exempt: the
  // i-row kana it starts with spells the case particle に for the な row
  // (死にゃ), and there the mora belongs to the inflection.
  // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
  if (vh::embedsCaseParticle(dict_manager, codepoints, start_pos, contracted_end - 1)) {
    return nullptr;
  }
  // A palatal mora also ends the stem of ordinary lexical verbs, whose own
  // inflections then continue past it (おっしゃ+い+ます, いらっしゃ+る). Reading
  // that stem as a whole contraction cuts the verb in half, so a span the
  // dictionary already knows as a stem is not one.
  for (const auto& [godan_type, godan_row] : grammar::Conjugation::getGodanRows()) {
    static_cast<void>(godan_type);
    if (vh::isVerbInDictionary(dict_manager, extractSubstring(codepoints, start_pos, contracted_end) +
                                                 normalize::encodeUtf8(godan_row.base_vowel))) {
      return nullptr;
    }
  }
  // The negative auxiliary contracts the same way and is registered as its own
  // paradigm cell, so a run ending in one is [verb] + auxiliary and the verb
  // keeps its own boundary (書か + なきゃ, never a form of the non-word 書かなく).
  // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
  for (size_t tail_start = start_pos + 1; dict_manager != nullptr && tail_start < contracted_end; ++tail_start) {
    if (lookupEntryInRange(*dict_manager, codepoints, tail_start, contracted_end, core::PartOfSpeech::Auxiliary) !=
        nullptr) {
      return nullptr;
    }
  }
  const std::string uncontracted = uncontractHypothetical(codepoints, start_pos, contracted_end);
  if (uncontracted.empty()) {
    return nullptr;
  }
  const auto& analyses = inflection.analyze(uncontracted);
  for (const auto& analysis : analyses) {
    if (namesAttestedPredicate(dict_manager, analysis.base_form)) {
      return &analysis;
    }
  }
  // Most verbs are derived by rule rather than listed, so an unattested reading
  // still stands when the reconstruction itself is a confident conjugation
  // (見りゃ → 見れば → 見る).
  if (!analyses.empty() && analyses.front().confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
    return &analyses.front();
  }
  return nullptr;
}

}  // namespace

bool spellsContractedHypothetical(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                  const grammar::Inflection& inflection,
                                  const dictionary::DictionaryManager* dict_manager) {
  return readContractedHypothetical(codepoints, start_pos, end_pos, inflection, dict_manager) != nullptr;
}

void generateContractedConditionalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const grammar::Inflection& inflection,
                                             const dictionary::DictionaryManager* dict_manager,
                                             std::vector<UnknownCandidate>& candidates) {
  // Like every other conjugated candidate, the contraction is a lexical stem
  // followed by kana inflection, so the span may leave the stem's script once.
  const size_t stem_end =
      char_types[start_pos] == normalize::CharType::Hiragana
          ? start_pos
          : findCharRegionEnd(char_types, start_pos, codepoints.size() - start_pos, char_types[start_pos]);
  // A one-mora host cannot be told apart from an ordinary palatal mora at the
  // head of a word (しゃべる, ちゃんと), so the contraction needs a stem before
  // the fused mora.
  for (size_t contracted_end = start_pos + 3; contracted_end <= codepoints.size(); ++contracted_end) {
    if (contracted_end - 1 > stem_end && char_types[contracted_end - 2] != normalize::CharType::Hiragana) {
      break;
    }
    // A generator that already read this exact span as a predicate had context
    // this reconstruction does not: after a te-form the same contraction
    // belongs to a subsidiary auxiliary rather than to a main verb
    // (読んで + みりゃ against 見りゃ). The generic same-type and nominal
    // fallbacks claim every span and carry no such reading, so they do not
    // count.
    const bool read_as_predicate =
        std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& existing) {
          return existing.start == start_pos && existing.end == contracted_end &&
                 (existing.pos == core::PartOfSpeech::Verb || existing.pos == core::PartOfSpeech::Adjective ||
                  existing.pos == core::PartOfSpeech::Auxiliary);
        });
    if (read_as_predicate) {
      continue;
    }
    const grammar::InflectionCandidate* chosen =
        readContractedHypothetical(codepoints, start_pos, contracted_end, inflection, dict_manager);
    if (chosen == nullptr) {
      continue;
    }
    const grammar::InflectionCandidate& best = *chosen;
    const bool is_adjective = best.verb_type == grammar::VerbType::IAdjective;
    const std::string surface = extractSubstring(codepoints, start_pos, contracted_end);
    auto candidate =
        makeVerbCandidate(surface, start_pos, contracted_end, candidate::verb_cost::kStrongBonus, best.base_form,
                          grammar::verbTypeToConjType(best.verb_type), true, CandidateOrigin::VerbHiragana,
                          best.confidence, "contracted_hypothetical",
                          is_adjective ? core::ExtendedPOS::AdjKeForm : core::ExtendedPOS::VerbContractedKateikei);
    if (is_adjective) {
      candidate.pos = core::PartOfSpeech::Adjective;
    }
    candidate.lemma_verified = namesAttestedPredicate(dict_manager, best.base_form);
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] " << surface << " contracted_hypothetical base=" << best.base_form
                                            << " conf=" << best.confidence << "\n");
    candidates.push_back(std::move(candidate));
  }
}

}  // namespace suzume::analysis
