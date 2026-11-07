/**
 * @file adjective_candidates_hiragana_stem.cpp
 * @brief Bare hiragana adjective stems cut before a derivational follower (すぎ/そう/がる, げ,
 *        さ, auxiliary patterns) and the clipped exclamative
 */

#include <algorithm>
#include <array>

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_absorption_guards.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"
#include "verb_candidates_emphatic.h"

namespace suzume::analysis {

using verb_helpers::addEmphaticVariants;
using verb_helpers::isAdjectiveInDictionary;
using verb_helpers::isEmphaticChar;
using verb_helpers::isVerbInDictionary;

namespace adj_detail {

void appendStemBeforeExcessiveOrAppearance(const HiraganaAdjectiveRun& run, std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = run.codepoints;
  const size_t start_pos = run.start_pos;
  const auto* dict_manager = run.dict_manager;
  const char32_t first_char = codepoints[start_pos];

  // The excessive auxiliary follows every i-adjective stem, not only the
  // しい/きい subclasses handled by the shared pattern table below. Rebuild the
  // base with its terminal い and let inflection validate it, so だる+すぎる,
  // ゆる+すぎる, and きつ+すぎる share one productive path.  This must precede
  // the general genitive-particle guard: that guard correctly rejects broad
  // unknown sequences, but would prevent this independently verified stem.
  //
  // A kanji i-adjective's continuative く is already a complete edge at the
  // preceding position (高く+なり+すぎる).  Starting a second adjective stem
  // from that okurigana would reconstruct a non-word such as くなりい.  This
  // leaves ordinary hiragana stems after particles available while preserving
  // the kanji-adjective boundary.
  const bool follows_kanji_continuative =
      start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]) && first_char == U'く';
  if (follows_kanji_continuative) {
    return;
  }
  // すぎ opens every cell of the auxiliary (すぎて, すぎない), so it is found
  // anywhere in the run, and the appearance そう takes the same bare stem of a
  // derived adjective (けちくさ+そう).  A stem opening on a particle mora (や, し)
  // is left to the inflection check of stem+い rather than rejected outright.
  for (size_t stem_end = start_pos + 2; stem_end + 1 < run.max_hiragana_end; ++stem_end) {
    const bool excessive_follows = codepoints[stem_end] == U'す' && codepoints[stem_end + 1] == U'ぎ';
    const bool appearance_follows = codepoints[stem_end] == U'そ' && codepoints[stem_end + 1] == U'う';
    // The emotive がる takes the same bare stem (めんどくさ+がる, 寒+がる).
    const bool garu_follows = codepoints[stem_end] == U'が' && (kana::isRaColumnCodepoint(codepoints[stem_end + 1]) ||
                                                                codepoints[stem_end + 1] == U'っ');
    if (!excessive_follows && !appearance_follows && !garu_follows) {
      continue;
    }
    // A ない-adjective inserts さ before the appearance そう (つまらな+さ+そう),
    // so the stem ends one mora earlier than the run before そう.
    const bool inserted_sa = appearance_follows && stem_end >= start_pos + 3 && codepoints[stem_end - 1] == U'さ' &&
                             codepoints[stem_end - 2] == U'な';
    const size_t own_stem_end = inserted_sa ? stem_end - 1 : stem_end;
    const std::string stem = extractSubstring(codepoints, start_pos, own_stem_end);
    // A verb continuative takes すぎる too (なり+すぎ); it is not a stem.
    if (utf8::contains(stem, "て") || utf8::contains(stem, "で") || isVerbInDictionary(dict_manager, stem)) {
      continue;
    }
    const std::string base_form = stem + "い";
    // そう also follows verb continuatives and phrases (ふり+そう, それは+そう),
    // so before it the stem has to carry its own derivation (けちくさ+そう)
    // or be a registered adjective (うざ+そう).
    const bool registered_adjective = isAdjectiveInDictionary(dict_manager, base_form);
    if ((inserted_sa || garu_follows) && !registered_adjective) {
      continue;
    }
    // A word registered over the whole run owns it: かわいそう is the
    // adjective 可哀想, not かわいい plus the appearance そう.
    if (appearance_follows && dict_manager != nullptr &&
        lookupEntryInRange(*dict_manager, codepoints, start_pos, stem_end + 2) != nullptr) {
      continue;
    }
    if (!excessive_follows && !registered_adjective &&
        !adj_detail::derivesFromCompoundFormingAdjective(codepoints, start_pos, base_form, dict_manager)) {
      continue;
    }
    const float analyzed_confidence =
        adj_detail::iAdjectiveConfidenceAtLeast(run.inflection.analyze(base_form), candidate::kCompoundAdjConfMin);
    if (analyzed_confidence == candidate::kNoOriginConfidence) {
      continue;
    }
    // A registered base is attested, not guessed from the kana shape.
    const float confidence = registered_adjective ? candidate::kDictionaryOriginConfidence : analyzed_confidence;
    const float cost =
        candidate::confidenceScaledCost(candidate::kAdjStemExtCost, confidence, candidate::kAdjStemConfScale);
    candidates.push_back(makeIAdjStemCandidate(stem, start_pos, own_stem_end, base_form, cost,
                                               CandidateOrigin::AdjectiveIHiragana, confidence,
                                               "adj_stem_hira_excessive"));
    break;
  }
}

void appendStemBeforeGe(const HiraganaAdjectiveRun& run, const std::string& full_hiragana_surface,
                        std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = run.codepoints;
  const size_t start_pos = run.start_pos;
  const auto* dict_manager = run.dict_manager;

  // -げ derives from an i-adjective stem while retaining the morpheme boundary
  // before the closed suffix.  The base adjective is checked by the same
  // inflection engine as ordinary hiragana adjectives; a verified verb reading
  // blocks the derivation so a verb continuative plus the lexical suffix is
  // preserved.  A following ない-family belongs to a lexical ...げない form,
  // not to the derivational suffix construction.
  const size_t ge_pos = full_hiragana_surface.find("げ");
  const size_t derived_end = ge_pos == std::string::npos ? 0 : ge_pos + core::kJapaneseCharBytes;
  const size_t derived_end_pos = derived_end == 0
                                     ? codepoints.size()
                                     : start_pos + normalize::utf8Length(full_hiragana_surface.substr(0, derived_end));
  const bool has_na_adjective_continuation =
      derived_end_pos < codepoints.size() &&
      (codepoints[derived_end_pos] == U'に' || codepoints[derived_end_pos] == U'な' ||
       codepoints[derived_end_pos] == U'だ' || codepoints[derived_end_pos] == U'さ');
  const bool has_lexical_ge_nai = derived_end != 0 && verb_helpers::naiNegativeFollowsAt(codepoints, derived_end_pos);
  if (ge_pos != std::string::npos && ge_pos >= core::kTwoJapaneseCharBytes && has_na_adjective_continuation &&
      !has_lexical_ge_nai) {
    const std::string stem = full_hiragana_surface.substr(0, ge_pos);
    const std::string base_form = stem + "い";
    const auto& base_candidates = run.inflection.analyze(base_form);
    const float adjective_confidence =
        adj_detail::iAdjectiveConfidenceAtLeast(base_candidates, candidate::kDerivedSuffixAdjectiveConfidence);
    const bool has_verified_verb_reading = adj_detail::hasDictionaryVerbAnalysis(base_candidates, dict_manager);
    // A registered auxiliary cell (らし of らしい) keeps its own reading before
    // げ (子供+らし+げ+な); it is no coined adjective stem.
    const bool is_auxiliary_cell = verb_helpers::hasDictionaryEntry(dict_manager, stem, core::PartOfSpeech::Auxiliary);
    // Right behind a kanji, a coined stem is that kanji's okurigana run plus
    // the suffix's host (頼り+なげ), not an adjective of its own.
    // A base that is a registered closed-class word (なさい) is no coined
    // adjective; its stem is the negative stem plus さ (頼り+な+さ+げ).
    const bool closed_class_base =
        verb_helpers::hasDictionaryEntry(dict_manager, base_form, core::PartOfSpeech::Auxiliary);
    const bool coined_behind_kanji = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]) &&
                                     !verb_helpers::isAdjectiveInDictionary(dict_manager, base_form);
    if (adjective_confidence != candidate::kNoOriginConfidence && !has_verified_verb_reading && !is_auxiliary_cell &&
        !coined_behind_kanji && !closed_class_base) {
      candidates.push_back(makeIAdjStemCandidate(
          stem, start_pos, start_pos + normalize::utf8Length(stem), base_form, candidate::kDerivedSuffixAdjectiveCost,
          CandidateOrigin::AdjectiveIHiragana, adjective_confidence, "i_adjective_ge_stem"));
    }
  }
}

void appendStemBeforeAuxiliaryPattern(const HiraganaAdjectiveRun& run, const std::string& full_hiragana_surface,
                                      std::vector<UnknownCandidate>& candidates) {
  // Add stem candidates for pure hiragana adjective + auxiliary patterns
  // This handles patterns like おいしそう → おいし (stem) + そう (aux)
  // Check for しそう, しすぎ patterns (adjective stem + auxiliary) over the
  // maximum hiragana sequence.
  for (size_t pattern_index = 0; pattern_index < adj_detail::kHiraganaIAdjStemAuxPatternCount; ++pattern_index) {
    const std::string_view aux_pattern = adj_detail::kIAdjStemAuxPatterns[pattern_index];
    // Need at least 2 chars before pattern
    const size_t pattern_pos = full_hiragana_surface.size() >= aux_pattern.size() + core::kTwoJapaneseCharBytes
                                   ? full_hiragana_surface.find(aux_pattern)
                                   : std::string::npos;
    if (pattern_pos != std::string::npos) {
      if (pattern_pos < core::kTwoJapaneseCharBytes) {
        continue;  // Stem too short (need at least 2 chars like おいし, うれし)
      }

      // The stem is everything before the auxiliary pattern, including the し
      const std::string stem =
          full_hiragana_surface.substr(0, pattern_pos + core::kJapaneseCharBytes);  // +1 char for し
      const std::string base_form = stem + "い";                                    // e.g., おいし → おいしい

      // A te-form connective cannot be part of an i-adjective stem. Keep its
      // boundary in desiderative-looking chains (読ん+で+ほし+そう,
      // 書い+て+ほし+そう) instead of fabricating an adjective that absorbs it.
      if (utf8::contains(stem, "て") || utf8::contains(stem, "で")) {
        continue;
      }

      // Validate that this forms a valid i-adjective
      const auto& adj_results = run.inflection.analyze(base_form);
      const float adj_confidence = adj_detail::iAdjectiveConfidenceAtLeast(adj_results, candidate::kIAdjConfMin);

      if (adj_confidence == 0.0F) {
        continue;
      }

      // Check that this is NOT a verb renyokei (e.g., 話し from 話す)
      // For pure hiragana, check if stem + す would be a valid verb
      // We compare adjective vs verb confidence - if adjective is significantly higher, prefer it
      // e.g., おい + す = おいす (not real)
      const std::string verb_form =
          normalize::concat(std::string_view(stem).substr(0, stem.size() - core::kJapaneseCharBytes), "す");

      // Check verb confidence from inflection analyzer
      const auto& verb_results = run.inflection.analyze(verb_form);
      const float verb_confidence =
          adj_detail::maxConfidenceFor(verb_results, {grammar::VerbType::GodanSa, grammar::VerbType::Suru});

      // Require adjective confidence to be higher than verb confidence
      // This filters out false positives like 話しそう (話す renyokei + そう)
      // but keeps valid adjectives like おいしそう (おいしい stem + そう)
      // Note: Both おいしい (0.66) and おいす (0.62) have similar confidence,
      // so we just need adj >= verb for pure hiragana patterns.
      if (verb_confidence > 0.0F && adj_confidence < verb_confidence) {
        SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM_HIRA] skip: adj_conf=" << adj_confidence << " verb_conf=" << verb_confidence
                                                                   << "\n");
        continue;  // Verb confidence higher, likely verb renyokei
      }

      // Calculate position
      size_t stem_char_count = normalize::utf8Length(stem);
      size_t stem_end = run.start_pos + stem_char_count;

      // Generate stem candidate with strong bonus
      // おい (INTJ) has cost -1, so stem needs very low cost to win
      float cost =
          candidate::confidenceScaledCost(candidate::kAdjStemExtCost, adj_confidence, candidate::kAdjStemConfScale);
      SUZUME_DEBUG_LOG("[ADJ_STEM_HIRA] ✓ candidate stem=\"" << stem << "\" base=\"" << base_form << "\" cost=" << cost
                                                             << "\n");
      candidates.push_back(makeIAdjStemCandidate(stem, run.start_pos, stem_end, base_form, cost,
                                                 CandidateOrigin::AdjectiveIHiragana, adj_confidence,
                                                 "adj_stem_hira_sou"));
      break;  // Only one stem candidate per pattern
    }
  }
}

void appendStemBeforeNominalizerSa(const HiraganaAdjectiveRun& run, std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = run.codepoints;
  const size_t start_pos = run.start_pos;
  const auto* dict_manager = run.dict_manager;

  // A complete i-adjective paradigm exposes its stem before the productive
  // nominalizer さ (やさし+さ, うれし+さ).  The ordinary hiragana scanner only
  // recognizes い/く/かっ forms. Reconstruct the base form and require both a
  // valid adjective analysis and a real boundary after さ; this admits open
  // adjective vocabulary without mistaking さん/さま inside nouns for the
  // nominalizer.
  for (size_t stem_end = start_pos + 2; stem_end < run.max_hiragana_end; ++stem_end) {
    if (codepoints[stem_end] != U'さ') {
      continue;
    }
    const size_t after_sa = stem_end + 1;
    // The past connective is not the question particle, so a さ before かっ is
    // still inside the adjective's own paradigm rather than the nominalizer
    // closing it (うそくさかった, not うそく + さ + かっ + た).
    // The nominalizer closes a noun, so a copula bounds it exactly as a
    // particle does: だ/です predicate over the nominal to their left
    // (うれしさだ). Without this the run has no adjective stem at all and
    // fragments into single morae.
    const bool copula_boundary = after_sa < codepoints.size() && grammar::startsPredicativeCopula(extractSubstring(
                                                                     codepoints, after_sa, codepoints.size()));
    const bool bounded_nominalizer = !adj_detail::opensAdjectivePastConnective(codepoints, after_sa) &&
                                     (adj_detail::closesAtTextEndOrSymbol(run.char_types, after_sa) ||
                                      normalize::isExtendedParticle(codepoints[after_sa]) || copula_boundary);
    if (!bounded_nominalizer) {
      continue;
    }

    const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
    // A closed-class word is not an i-adjective stem.  After an interjection
    // the さ belongs to an unknown noun reading (うわさ); after a particle or
    // conjunction it is the interjectory particle (けど+さ).
    bool is_closed_class_stem = false;
    if (dict_manager != nullptr) {
      for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, start_pos, stem_end)) {
        const auto pos = match.entry == nullptr ? core::PartOfSpeech::Unknown : match.entry->pos;
        is_closed_class_stem =
            is_closed_class_stem || (match.length == stem_end - start_pos &&
                                     (pos == core::PartOfSpeech::Interjection || pos == core::PartOfSpeech::Particle ||
                                      pos == core::PartOfSpeech::Auxiliary || pos == core::PartOfSpeech::Conjunction));
      }
    }
    if (is_closed_class_stem) {
      continue;
    }
    const std::string base_form = stem + "い";
    const bool is_dict_adjective = isAdjectiveInDictionary(dict_manager, base_form);
    if (!is_dict_adjective && kana::isERowCodepoint(codepoints[stem_end - 1])) {
      continue;
    }
    // The topic は in front of a stem that is itself a valid adjective (の+は+やさし+さ)
    // is that particle, not the first mora of an unregistered stem.
    const auto* opening_particle =
        dict_manager != nullptr
            ? lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Particle)
            : nullptr;
    if (!is_dict_adjective && stem_end > start_pos + 2 && opening_particle != nullptr &&
        opening_particle->extended_pos == core::ExtendedPOS::ParticleTopic) {
      const std::string remainder_base = extractSubstring(codepoints, start_pos + 1, stem_end) + "い";
      if (isAdjectiveInDictionary(dict_manager, remainder_base) ||
          adj_detail::iAdjectiveConfidenceAtLeast(run.inflection.analyze(remainder_base), candidate::kIAdjConfMin) !=
              candidate::kNoOriginConfidence) {
        continue;
      }
    }
    // A terminal auxiliary closed by a conjunctive particle is a predicate
    // chain (やり+たい+し+さ), not a stem. The interjectory さ closing such a
    // chain takes no case particle or copula, so with one after it the さ is
    // the nominalizer and the stem stands (くるし+さ+に).
    const auto* case_after_sa =
        dict_manager != nullptr && after_sa < codepoints.size()
            ? lookupEntryInRange(*dict_manager, codepoints, after_sa, after_sa + 1, core::PartOfSpeech::Particle)
            : nullptr;
    const bool nominal_frame_after_sa =
        copula_boundary || (case_after_sa != nullptr && case_after_sa->extended_pos == core::ExtendedPOS::ParticleCase);
    if (!is_dict_adjective && !nominal_frame_after_sa &&
        (verb_helpers::closesOnTerminalAuxiliaryAndConjunctive(dict_manager, codepoints, stem_end) ||
         verb_helpers::closesOnTerminalVerbAndConjunctive(dict_manager, codepoints, start_pos, stem_end))) {
      continue;
    }
    const float adjective_confidence =
        is_dict_adjective
            ? candidate::kDictionaryOriginConfidence
            : adj_detail::iAdjectiveConfidenceAtLeast(run.inflection.analyze(base_form), candidate::kIAdjConfMin);
    if (adjective_confidence == candidate::kNoOriginConfidence) {
      continue;
    }

    const float cost = is_dict_adjective
                           ? candidate::kAdjStemDictionaryCost
                           : candidate::confidenceScaledCost(candidate::kAdjStemBaseCost, adjective_confidence,
                                                             candidate::kAdjStemConfScale);
    auto adjective =
        makeIAdjStemCandidate(stem, start_pos, stem_end, base_form, cost, CandidateOrigin::AdjectiveIHiragana,
                              adjective_confidence, "adj_stem_hira_nominalizer_sa");
    adjective.lemma_verified = is_dict_adjective;
    candidates.push_back(std::move(adjective));
    break;
  }
}

void appendClippedExclamative(const HiraganaAdjectiveRun& run, std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = run.codepoints;
  const size_t start_pos = run.start_pos;
  const size_t max_hiragana_end = run.max_hiragana_end;
  const auto* dict_manager = run.dict_manager;

  // The clipped exclamative closes an i-adjective on its bare stem with an
  // emphatic sokuon (すごっ, あつっ).  Nothing follows the mark -- it ends the
  // utterance -- so no cell of the paradigm reaches the run and it falls back
  // on an unknown noun.  The stem is the one the nominalizer above exposes, so
  // the same reconstruction settles the reading; requiring the utterance end
  // keeps a genuine 促音便 (いっ+て) and a doubled consonant inside a word
  // (まっすぐ) out.
  // An adjective in いい also clips before the question particle, its second
  // い assimilated into the sokuon (まあいっか, これでいっか); the stem then
  // ends in い, which is what keeps a numeral such as よっか out.
  const auto closes_utterance = [&](size_t pos) { return adj_detail::closesAtTextEndOrSymbol(run.char_types, pos); };
  // After a case particle the run is a verb's onbin instead (東京に+いっ+か);
  // で is the evaluative frame itself (これで+いっ+か) and stays out.
  const auto* preceding_particle =
      start_pos > 0 && dict_manager != nullptr && codepoints[start_pos - 1] != U'で'
          ? lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle)
          : nullptr;
  const bool follows_case_particle =
      preceding_particle != nullptr && preceding_particle->extended_pos == core::ExtendedPOS::ParticleCase;
  // The mark is the sokuon or a run of ー (すご+ー), read by the same helper as
  // the kanji path. A ー after an i-row mora draws out the final い instead
  // (かわいー), which the ー normalization reads.
  for (size_t stem_end = start_pos + 1; stem_end <= max_hiragana_end && stem_end < codepoints.size(); ++stem_end) {
    const size_t mark_end = adj_detail::clippedExclamativeMarkEnd(codepoints, stem_end);
    if (mark_end == 0) {
      continue;
    }
    const bool is_sokuon = codepoints[stem_end] == core::hiragana::kSmallTsu;
    if (!is_sokuon && grammar::getVowelForChar(codepoints[stem_end - 1]) == U'い') {
      break;
    }
    const size_t after_sokuon = mark_end;
    const bool clipped_before_ka = is_sokuon && codepoints[stem_end - 1] == U'い' && after_sokuon < codepoints.size() &&
                                   codepoints[after_sokuon] == U'か' && closes_utterance(after_sokuon + 1) &&
                                   !follows_case_particle;
    // Reconstructing the base form is not enough on its own here: the sokuon
    // carries no information about the word in front of it, so any two morae
    // plus い pass an inflection check and the run turns into a coined
    // adjective (たぞっ as たぞい). The nominalizer above can afford that check
    // because さ is itself the evidence; this cell needs the entry.
    std::string base_form;
    if (clipped_before_ka) {
      base_form = extractSubstring(codepoints, start_pos, stem_end) + "い";
      if (!isAdjectiveInDictionary(dict_manager, base_form)) {
        break;
      }
    } else if (stem_end >= start_pos + 2) {
      base_form = adj_detail::clippedExclamativeBase(dict_manager, codepoints, run.char_types, start_pos, stem_end);
    }
    if (base_form.empty()) {
      break;
    }
    auto exclamative = makeIAdjCellCandidate(
        extractSubstring(codepoints, start_pos, after_sokuon), start_pos, after_sokuon, base_form,
        core::ExtendedPOS::AdjBasic, candidate::kAdjStemDictionaryCost, CandidateOrigin::AdjectiveIHiragana,
        candidate::kDictionaryOriginConfidence, "adj_stem_hira_exclamative_sokuon");
    exclamative.lemma_verified = true;
    candidates.push_back(std::move(exclamative));
    break;
  }
  // The bare stem closes the utterance the same way without the sokuon
  // (ありがた。, めでた。). Three morae and a registered adjective keep short
  // stems and verb fragments out (すご, やば, あり+がた).
  constexpr size_t kMinBareExclamativeMorae = 3;
  if (closes_utterance(max_hiragana_end) && max_hiragana_end >= start_pos + kMinBareExclamativeMorae &&
      codepoints[max_hiragana_end - 1] != core::hiragana::kSmallTsu) {
    const std::string base_form = extractSubstring(codepoints, start_pos, max_hiragana_end) + "い";
    if (isAdjectiveInDictionary(dict_manager, base_form)) {
      auto exclamative = makeIAdjCellCandidate(
          extractSubstring(codepoints, start_pos, max_hiragana_end), start_pos, max_hiragana_end, base_form,
          core::ExtendedPOS::AdjBasic, candidate::kAdjStemDictionaryCost, CandidateOrigin::AdjectiveIHiragana,
          candidate::kDictionaryOriginConfidence, "adj_stem_hira_exclamative_bare");
      exclamative.lemma_verified = true;
      candidates.push_back(std::move(exclamative));
    }
  }
}

}  // namespace adj_detail
}  // namespace suzume::analysis
