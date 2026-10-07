/**
 * @file adjective_candidates_hiragana.cpp
 * @brief Hiragana and katakana i-adjective candidate generation
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

using adj_detail::appendHiraganaPrefixedKanjiIAdjCandidates;
using adj_detail::dropCoinedAdjectivesOverDictionaryAdjective;
using adj_detail::makeIAdjCandidate;
using adj_detail::makeIAdjCellCandidate;
using adj_detail::makeIAdjStemCandidate;
using adj_detail::makeNaAdjCandidate;

namespace {

// A genitive の after a substantive two-mora prefix is a phrase boundary for
// an i-adjective candidate assembled from unknown hiragana. Keep exact lexical
// adjectives and early word-internal の (たのしい); only fabricated analyses
// that cross a plausible nominal host are rejected by the caller.
bool embedsGenitiveParticle(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                            size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos < start_pos + 3 || end_pos > codepoints.size()) {
    return false;
  }
  for (size_t particle_pos = start_pos + 1; particle_pos + 1 < end_pos; ++particle_pos) {
    const auto* entry =
        lookupEntryInRange(*dict_manager, codepoints, particle_pos, particle_pos + 1, core::PartOfSpeech::Particle);
    if (entry != nullptr && entry->extended_pos == core::ExtendedPOS::ParticleNo) {
      if (particle_pos >= start_pos + 2) {
        return true;
      }
      if (start_pos > 0) {
        const auto* bridged_head = lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, particle_pos);
        if (bridged_head != nullptr &&
            (bridged_head->pos == core::PartOfSpeech::Noun || bridged_head->pos == core::PartOfSpeech::Particle ||
             bridged_head->pos == core::PartOfSpeech::Auxiliary)) {
          return true;
        }
      }
    }
  }
  return false;
}

// A generated i-adjective cannot begin with the genitive particle.  The
// ordinary interior-boundary guard above cannot see this case because the
// particle is the candidate's first character (紙+の+ごとく).  Preserve an
// exact lexical adjective if one exists, but otherwise leave the closed
// particle edge available to the lattice.
bool startsWithGenitiveParticle(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || start_pos >= end_pos || codepoints[start_pos] != U'の') {
    return false;
  }
  const auto* entry = dict_manager->lookupExact("の", core::PartOfSpeech::Particle);
  return entry != nullptr && entry->extended_pos == core::ExtendedPOS::ParticleNo;
}

// Emit a whole-word i-adjective candidate for a spelled-out reduplicated 〜しい
// adjective (バカバカしい, ばかばかしくない). The doubled stem is otherwise pre-empted
// by an onomatopoeia ADV candidate (aa_doubled / abab_pattern) plus a split-off しい
// tail, so this bypasses the particle-boundary and ending gates the regular scanners
// apply and lets inflection analyze the full surface directly. The caller's existing
// ku/katt/ke trim loops spin the conjugation splits (…しく, …しかっ) out of the emitted
// base. Shared by the hiragana and katakana generators (the kanji path handles its own
// stem-length case), so the reduplication rule lives in one place for all three scripts.
void addReduplicatedShiiAdjective(std::vector<UnknownCandidate>& candidates, const std::vector<char32_t>& codepoints,
                                  size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                  const grammar::Inflection& inflection, CandidateOrigin origin) {
  if (!verb_helpers::isReduplicatedShiiAdjectiveHead(codepoints, start_pos)) {
    return;
  }
  // し and every inflection ending after it are hiragana; scan that run.
  size_t shi_pos = start_pos + 4;
  size_t hira_end = findCharRegionEnd(char_types, shi_pos, 8, normalize::CharType::Hiragana);
  // Longest-first so the full conjugated surface (…しくない) is chosen; the caller's
  // trim loops then derive …しく. Minimum end covers し + い (base form しい).
  for (size_t end_pos = hira_end; end_pos >= shi_pos + 2; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    for (const auto& cand : inflection.analyze(surface)) {
      if (cand.verb_type != grammar::VerbType::IAdjective || cand.confidence < candidate::kIAdjConfMin) {
        continue;
      }
      float cost = candidate::confidenceScaledCost(candidate::kKanjiAdjBaseCost, cand.confidence,
                                                   candidate::kKanjiAdjConfScale) +
                   candidate::kReduplicatedShiiAdjBonus;
      auto adj = makeIAdjCandidate(surface, start_pos, end_pos, cand.base_form, cost, origin, cand.confidence,
                                   "i_adjective_reduplicated");
      adj.has_suffix = true;  // Morphologically recognized; skip exceeds_dict_length penalty
      candidates.push_back(std::move(adj));
      return;
    }
  }
}

// The kana spelling いたい is the existential いる plus the desiderative たい and
// also the adjective 痛い. After the nominative が the verb would mark its own
// subject as the wanted one, so the adjective of what hurts (頭がいたい) is
// offered; after any other morpheme the verb reading stays the only one.
void appendPainAdjectiveAfterNominative(const std::vector<char32_t>& codepoints, size_t start_pos,
                                        const std::vector<normalize::CharType>& char_types,
                                        const grammar::Inflection& inflection,
                                        const dictionary::DictionaryManager* dict_manager,
                                        std::vector<UnknownCandidate>& candidates) {
  if (dict_manager == nullptr || start_pos == 0 || start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana ||
      lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle) ==
          nullptr ||
      codepoints[start_pos - 1] != U'が') {
    return;
  }
  const size_t hira_end = findCharRegionEnd(char_types, start_pos, 6, normalize::CharType::Hiragana);
  for (size_t end_pos = start_pos + 3; end_pos <= hira_end; ++end_pos) {
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    // The past cell かっ is analyzed only together with its た (いたかった), and
    // the adjective itself is the span up to かっ.
    const bool past_cell = utf8::endsWith(surface, "かった");
    const size_t cell_end = past_cell ? end_pos - 1 : end_pos;
    for (const auto& cand : inflection.analyze(surface)) {
      if (cand.verb_type != grammar::VerbType::IAdjective || cand.confidence < candidate::kIAdjConfMin ||
          !utf8::startsWith(cand.base_form, "いた") || (utf8::endsWithAny(surface, {"た", "て"}) && !past_cell) ||
          !isAdjectiveInDictionary(
              dict_manager,
              normalize::concat("痛", std::string_view(cand.base_form).substr(core::kTwoJapaneseCharBytes)))) {
        continue;
      }
      const float cost = candidate::confidenceScaledCost(candidate::kKanjiAdjBaseCost, cand.confidence,
                                                         candidate::kKanjiAdjConfScale) +
                         candidate::kReduplicatedShiiAdjBonus;
      auto adj =
          makeIAdjCandidate(extractSubstring(codepoints, start_pos, cell_end), start_pos, cell_end, cand.base_form,
                            cost, CandidateOrigin::AdjectiveIHiragana, cand.confidence, "i_adjective_pain_nominative");
      adj.has_suffix = true;
      candidates.push_back(std::move(adj));
      return;
    }
  }
}

// The kana a fused long e respells: the stem's last mora on the a row
// (やばい → やべえ) or on the o row (すごい → すげえ).
struct FusedESource {
  char32_t fused;
  char32_t a_row;
  char32_t o_row;
};
constexpr std::array<FusedESource, 13> kFusedESources = {{
    {U'え', U'あ', U'お'},
    {U'け', U'か', U'こ'},
    {U'げ', U'が', U'ご'},
    {U'せ', U'さ', U'そ'},
    {U'ぜ', U'ざ', U'ぞ'},
    {U'て', U'た', U'と'},
    {U'で', U'だ', U'ど'},
    {U'ね', U'な', U'の'},
    {U'へ', U'は', U'ほ'},
    {U'べ', U'ば', U'ぼ'},
    {U'ぺ', U'ぱ', U'ぽ'},
    {U'め', U'ま', U'も'},
    {U'れ', U'ら', U'ろ'},
}};

// A registered i-adjective whose ending has fused into a long e (すげえ for
// すごい, やべえ for やばい) is that adjective, with its standard lemma.
void appendVowelFusedAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         size_t max_hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates) {
  constexpr size_t kMaxStemMorae = 4;
  if (dict_manager == nullptr) {
    return;
  }
  for (size_t fused_at = start_pos + 1; fused_at + 1 < max_hiragana_end && fused_at - start_pos <= kMaxStemMorae;
       ++fused_at) {
    if (codepoints[fused_at + 1] != U'え') {
      continue;
    }
    const auto source = std::find_if(kFusedESources.begin(), kFusedESources.end(),
                                     [&](const FusedESource& entry) { return entry.fused == codepoints[fused_at]; });
    if (source == kFusedESources.end()) {
      continue;
    }
    const std::string stem = extractSubstring(codepoints, start_pos, fused_at);
    for (const char32_t restored : {source->a_row, source->o_row}) {
      const std::string base_form = stem + normalize::encodeUtf8(restored) + "い";
      if (!isAdjectiveInDictionary(dict_manager, base_form)) {
        continue;
      }
      auto fused = makeCandidate(codepoints, start_pos, fused_at + 2, core::PartOfSpeech::Adjective,
                                 candidate::kAdjStemDictionaryCost, true, CandidateOrigin::AdjectiveIHiragana,
                                 core::ExtendedPOS::AdjBasic);
      fused.lemma = base_form;
      fused.lemma_verified = true;
      SUZUME_DEBUG_CANDIDATE(fused, candidate::kDictionaryOriginConfidence, "adj_vowel_fused_e");
      candidates.push_back(std::move(fused));
      return;
    }
  }
}

}  // namespace

namespace {

// The maximal hiragana run an adjective scan reads, with the inputs every
// phase of the scan shares.
struct HiraganaAdjectiveRun {
  const std::vector<char32_t>& codepoints;
  size_t start_pos;
  size_t max_hiragana_end;
  const std::vector<normalize::CharType>& char_types;
  const grammar::Inflection& inflection;
  const dictionary::DictionaryManager* dict_manager;
};

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
  // すぎ opens every cell of the auxiliary (すぎて, すぎない), so it is found
  // anywhere in the run, and the appearance そう takes the same bare stem of a
  // derived adjective (けちくさ+そう).  A stem opening on a particle mora (や, し)
  // is left to the inflection check of stem+い rather than rejected outright.
  if (!follows_kanji_continuative) {
    for (size_t stem_end = start_pos + 2; stem_end + 1 < run.max_hiragana_end; ++stem_end) {
      const bool excessive_follows = codepoints[stem_end] == U'す' && codepoints[stem_end + 1] == U'ぎ';
      const bool appearance_follows = codepoints[stem_end] == U'そ' && codepoints[stem_end + 1] == U'う';
      if (!excessive_follows && !appearance_follows) {
        continue;
      }
      // A ない-adjective inserts さ before the appearance そう (つまらな+さ+そう),
      // so the stem ends one mora earlier than the run before そう.
      const bool inserted_sa = appearance_follows && stem_end >= start_pos + 3 && codepoints[stem_end - 1] == U'さ' &&
                               codepoints[stem_end - 2] == U'な';
      const size_t own_stem_end = inserted_sa ? stem_end - 1 : stem_end;
      const std::string stem = extractSubstring(codepoints, start_pos, own_stem_end);
      // A verb continuative takes すぎる too (なり+すぎ); it is not a stem.
      if (utf8::contains(stem, "て") || utf8::contains(stem, "で") ||
          (dict_manager != nullptr && dict_manager->lookupExact(stem, core::PartOfSpeech::Verb) != nullptr)) {
        continue;
      }
      const std::string base_form = stem + "い";
      // そう also follows verb continuatives and phrases (ふり+そう, それは+そう),
      // so before it the stem has to carry its own derivation (けちくさ+そう)
      // or be a registered adjective (うざ+そう).
      const bool registered_adjective =
          dict_manager != nullptr && dict_manager->lookupExact(base_form, core::PartOfSpeech::Adjective) != nullptr;
      if (inserted_sa && !registered_adjective) {
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
      const float confidence = adj_detail::firstConfidenceAtLeast(
          run.inflection.analyze(base_form), grammar::VerbType::IAdjective, candidate::kCompoundAdjConfMin);
      if (confidence == candidate::kNoOriginConfidence) {
        continue;
      }
      const float cost =
          candidate::confidenceScaledCost(candidate::kAdjStemExtCost, confidence, candidate::kAdjStemConfScale);
      candidates.push_back(makeIAdjStemCandidate(stem, start_pos, own_stem_end, base_form, cost,
                                                 CandidateOrigin::AdjectiveIHiragana, confidence,
                                                 "adj_stem_hira_excessive"));
      break;
    }
  }
}

// Whether the run opens on a closed-class particle followed by a multi-mora
// auxiliary inflection, and the whole run is no confident i-adjective.
bool opensOnParticleBeforeAuxiliary(const HiraganaAdjectiveRun& run) {
  const auto& codepoints = run.codepoints;
  const size_t start_pos = run.start_pos;
  const size_t max_hiragana_end = run.max_hiragana_end;
  const auto* dict_manager = run.dict_manager;

  // A closed-class particle immediately followed by a registered auxiliary
  // inflection is a grammatical boundary, not an i-adjective stem. This keeps
  // など+いない (and the same particle+auxiliary shape) from becoming a
  // fabricated adjective candidate.
  if (dict_manager != nullptr) {
    constexpr size_t kMaxParticleChars = 4;
    size_t max_particle_end = std::min(max_hiragana_end, start_pos + kMaxParticleChars);
    for (size_t particle_end = start_pos + 1; particle_end <= max_particle_end; ++particle_end) {
      if (lookupEntryInRange(*dict_manager, codepoints, start_pos, particle_end, core::PartOfSpeech::Particle) ==
          nullptr) {
        continue;
      }
      for (size_t aux_end = max_hiragana_end; aux_end > particle_end; --aux_end) {
        // A one-mora dictionary auxiliary such as い is too ambiguous to
        // establish a closed-class boundary by itself: it is also the final
        // mora of ordinary i-adjectives (かまびすしい).  The protected
        // particle+auxiliary patterns have a multi-mora inflection (が+いない,
        // など+いない), so require that grammatical evidence here.
        if (aux_end - particle_end < 2) {
          continue;
        }
        if (lookupEntryInRange(*dict_manager, codepoints, particle_end, aux_end, core::PartOfSpeech::Auxiliary) !=
            nullptr) {
          std::string full_surface = extractSubstring(codepoints, start_pos, max_hiragana_end);
          if (utf8::endsWith(full_surface, "く")) {
            full_surface = normalize::replaceFinalChar(full_surface, "い");
          }
          const auto& full_candidates = run.inflection.analyze(full_surface);
          const bool has_full_i_adjective =
              std::any_of(full_candidates.begin(), full_candidates.end(),
                          [](const grammar::InflectionCandidate& inflection_candidate) {
                            return inflection_candidate.verb_type == grammar::VerbType::IAdjective &&
                                   inflection_candidate.confidence >= candidate::kHiraAdjConfParticle &&
                                   normalize::utf8Length(inflection_candidate.stem) >= 2;
                          });
          if (!has_full_i_adjective) {
            return true;
          }
        }
      }
    }
  }
  return false;
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
    const float adjective_confidence = adj_detail::firstConfidenceAtLeast(
        base_candidates, grammar::VerbType::IAdjective, candidate::kDerivedSuffixAdjectiveConfidence);
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

// Longest end at which a particle-started run forms a confident i-adjective,
// or start_pos when no length does.
size_t particleStartedAdjectiveEnd(const HiraganaAdjectiveRun& run) {
  const auto& codepoints = run.codepoints;
  const size_t start_pos = run.start_pos;

  size_t valid_adj_min_end = start_pos;  // Minimum end position for a valid adjective
  // Check if the full sequence (or any length) forms a valid adjective
  // Use lower threshold (0.50) for particle-starting sequences to catch
  // words like かわいい (confidence=0.51). The nominalizer ん closes the
  // terminal here too (から+い is からい+ん+だ, not one adjective からいんだ).
  size_t particle_scan_end = run.max_hiragana_end;
  for (size_t pos = start_pos + 3; pos < run.max_hiragana_end; ++pos) {
    if (codepoints[pos] == U'ん') {
      particle_scan_end = pos;
      break;
    }
  }
  for (size_t end = particle_scan_end; end > start_pos + 2; --end) {
    std::string test_surface = extractSubstring(codepoints, start_pos, end);

    // A bare -く is an adverbial connective, not an adjective terminal.
    // A long full-run form immediately before a lexical head is the regular
    // i-adjective continuative (たやすく+答え); allow the inflection analyzer
    // below to validate that bounded form instead of splitting its initial
    // mora as a homographic particle.
    const bool bounded_long_ku_form = utf8::endsWith(test_surface, "く") && end - start_pos >= 4 &&
                                      end < codepoints.size() &&
                                      (normalize::isKanjiCodepoint(codepoints[end]) ||
                                       normalize::classifyChar(codepoints[end]) == normalize::CharType::Katakana);
    if (utf8::endsWith(test_surface, "く") && !utf8::endsWith(test_surface, "くない") && !bounded_long_ku_form &&
        !adj_detail::predicateFollowsContinuative(codepoints, start_pos, end, run.dict_manager)) {
      continue;
    }

    // Skip patterns ending with just ない (negative auxiliary misidentified as adjective)
    // This prevents でもない from being validated as an adjective
    // Valid patterns: くない (adjective negative), but ない alone after particles is auxiliary
    if (utf8::endsWith(test_surface, "ない") && !utf8::endsWith(test_surface, "くない")) {
      continue;  // Skip - likely negative auxiliary, not adjective
    }

    std::string analysis_surface = test_surface;
    if (utf8::endsWith(analysis_surface, "く")) {
      analysis_surface = normalize::replaceFinalChar(analysis_surface, "い");
    }
    const auto& test_candidates = run.inflection.analyze(analysis_surface);
    for (const auto& cand : test_candidates) {
      if (cand.verb_type == grammar::VerbType::IAdjective && cand.confidence >= candidate::kHiraAdjConfParticle) {
        // For particle-starting sequences, require stem length >= 2 characters
        // This prevents に+そうな from being recognized as にい (invalid)
        // Real adjectives have stems of at least 2 chars: あつい, かわいい, etc.
        if (normalize::utf8Length(cand.stem) < 2) {
          continue;  // Stem too short for a valid adjective
        }
        valid_adj_min_end = end;
        break;
      }
    }
    if (valid_adj_min_end > start_pos) {
      break;  // Found a valid adjective length
    }
  }
  return valid_adj_min_end;
}

// End of the run an adjective may span when it does not open on a particle:
// the whole bounded -く continuative, or the run up to a strong particle
// boundary.
size_t boundedAdjectiveRunEnd(const HiraganaAdjectiveRun& run, const std::string& full_hiragana_surface) {
  const auto& codepoints = run.codepoints;
  const size_t start_pos = run.start_pos;
  const size_t max_hiragana_end = run.max_hiragana_end;

  // For non-particle-starting sequences, apply particle boundary breaking
  // This handles cases like おいしい where we don't want to extend past particles
  const bool bounded_long_ku_form =
      utf8::endsWith(full_hiragana_surface, "く") && max_hiragana_end - start_pos >= 4 &&
      max_hiragana_end < codepoints.size() &&
      (normalize::isKanjiCodepoint(codepoints[max_hiragana_end]) ||
       normalize::classifyChar(codepoints[max_hiragana_end]) == normalize::CharType::Katakana);
  const std::string bounded_analysis_surface = bounded_long_ku_form
                                                   ? normalize::replaceFinalChar(full_hiragana_surface, "い")
                                                   : std::string(full_hiragana_surface);
  const auto& bounded_candidates = run.inflection.analyze(bounded_analysis_surface);
  const bool has_bounded_i_adjective =
      bounded_long_ku_form && adj_detail::firstConfidenceAtLeast(bounded_candidates, grammar::VerbType::IAdjective,
                                                                 candidate::kHiraAdjConfParticle) != float{};
  size_t hiragana_end = has_bounded_i_adjective ? max_hiragana_end : start_pos;
  while (!has_bounded_i_adjective && hiragana_end < max_hiragana_end) {
    char32_t curr_char = codepoints[hiragana_end];

    // Only break at strong particle boundaries after minimum stem length
    if (hiragana_end - start_pos >= 3 && !normalize::isProlongedSoundMark(curr_char)) {
      bool next_is_prolonged =
          (hiragana_end + 1 < run.char_types.size() && normalize::isProlongedSoundMark(codepoints[hiragana_end + 1]));
      if (!next_is_prolonged) {
        // か heading the i-adjective past connective かっ (…かった/…かっ) is a
        // conjugation, not the question particle — keep scanning so the whole past
        // form becomes one adjective candidate (うれしかった, たのしかった). Without this
        // the scan truncates at か and only the bare stem (うれし) is emitted, letting
        // a fake godan verb (うれしかう) win. A non-adjective tail is still rejected by
        // the inflection confidence gate below. Exclude なかっ (negative auxiliary past):
        // 〜たくなかった/〜くなかった split as aux (たく|なかっ|た), so a な directly before
        // かっ must still break — the rare ない-family adjective (少なかった) is left to the
        // pre-existing split rather than mis-scored as one token.
        bool is_katt_past = adj_detail::opensAdjectivePastConnective(codepoints, hiragana_end);
        // か before the shiku し is likewise the stem's own mora (なまめか+しい,
        // もどか+しい), not the question particle.
        const bool opens_shiku_ending =
            curr_char == U'か' && hiragana_end + 1 < max_hiragana_end && codepoints[hiragana_end + 1] == U'し';
        // The contracted nominalizer ん closes the terminal the way の does
        // (つらい+ん+だ, つらい+の).
        if (!is_katt_past && !opens_shiku_ending &&
            (normalize::isExtendedParticle(curr_char) || curr_char == U'や' || curr_char == U'ん')) {
          break;  // Stop before the particle
        }
      }
    }
    ++hiragana_end;
  }
  return hiragana_end;
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
      const std::string stem = full_hiragana_surface.substr(0, pattern_pos + 3);  // +3 for し
      const std::string base_form = stem + "い";                                  // e.g., おいし → おいしい

      // A te-form connective cannot be part of an i-adjective stem. Keep its
      // boundary in desiderative-looking chains (読ん+で+ほし+そう,
      // 書い+て+ほし+そう) instead of fabricating an adjective that absorbs it.
      if (utf8::contains(stem, "て") || utf8::contains(stem, "で")) {
        continue;
      }

      // Validate that this forms a valid i-adjective
      const auto& adj_results = run.inflection.analyze(base_form);
      const float adj_confidence =
          adj_detail::firstConfidenceAtLeast(adj_results, grammar::VerbType::IAdjective, candidate::kIAdjConfMin);

      if (adj_confidence == 0.0F) {
        continue;
      }

      // Check that this is NOT a verb renyokei (e.g., 話し from 話す)
      // For pure hiragana, check if stem + す would be a valid verb
      // We compare adjective vs verb confidence - if adjective is significantly higher, prefer it
      // e.g., おい + す = おいす (not real)
      const std::string verb_form = normalize::concat(std::string_view(stem).substr(0, stem.size() - 3), "す");

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
    const bool bounded_nominalizer =
        !adj_detail::opensAdjectivePastConnective(codepoints, after_sa) &&
        (after_sa >= codepoints.size() || normalize::isExtendedParticle(codepoints[after_sa]) || copula_boundary ||
         (after_sa < run.char_types.size() && run.char_types[after_sa] == normalize::CharType::Symbol));
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
          adj_detail::firstConfidenceAtLeast(run.inflection.analyze(remainder_base), grammar::VerbType::IAdjective,
                                             candidate::kIAdjConfMin) != candidate::kNoOriginConfidence) {
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
        is_dict_adjective ? candidate::kDictionaryOriginConfidence
                          : adj_detail::firstConfidenceAtLeast(run.inflection.analyze(base_form),
                                                               grammar::VerbType::IAdjective, candidate::kIAdjConfMin);
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
  const auto closes_utterance = [&](size_t pos) {
    return pos >= codepoints.size() ||
           (pos < run.char_types.size() && run.char_types[pos] == normalize::CharType::Symbol);
  };
  // After a case particle the run is a verb's onbin instead (東京に+いっ+か);
  // で is the evaluative frame itself (これで+いっ+か) and stays out.
  const auto* preceding_particle =
      start_pos > 0 && dict_manager != nullptr && codepoints[start_pos - 1] != U'で'
          ? lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle)
          : nullptr;
  const bool follows_case_particle =
      preceding_particle != nullptr && preceding_particle->extended_pos == core::ExtendedPOS::ParticleCase;
  for (size_t stem_end = start_pos + 1; stem_end < max_hiragana_end; ++stem_end) {
    if (codepoints[stem_end] != core::hiragana::kSmallTsu) {
      continue;
    }
    const size_t after_sokuon = stem_end + 1;
    const bool clipped_before_ka = codepoints[stem_end - 1] == U'い' && after_sokuon < codepoints.size() &&
                                   codepoints[after_sokuon] == U'か' && closes_utterance(after_sokuon + 1) &&
                                   !follows_case_particle;
    if (!clipped_before_ka && (stem_end < start_pos + 2 || !closes_utterance(after_sokuon))) {
      break;
    }
    // Reconstructing the base form is not enough on its own here: the sokuon
    // carries no information about the word in front of it, so any two morae
    // plus い pass an inflection check and the run turns into a coined
    // adjective (たぞっ as たぞい). The nominalizer above can afford that check
    // because さ is itself the evidence; this cell needs the entry.
    const std::string base_form = extractSubstring(codepoints, start_pos, stem_end) + "い";
    if (!isAdjectiveInDictionary(dict_manager, base_form)) {
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

void appendHiraganaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                       const std::vector<normalize::CharType>& char_types,
                                       const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates) {
  const size_t candidate_start = candidates.size();

  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Hiragana) {
    return;
  }

  const char32_t first_char = codepoints[start_pos];

  // Skip if first character is を (wo) - this is always a particle, never an adjective stem
  // Unlike other particles (は, か, わ, etc.) that can start valid adjectives,
  // を is exclusively an object marker and never begins a Japanese adjective
  if (first_char == U'を') {
    return;
  }

  // Skip if starting with a small kana (拗音・促音: ゃ/ゅ/ょ/っ/ぁ…). No Japanese
  // word starts with a small kana, so an adjective candidate here would cut
  // through the preceding digraph.
  if (kana::isSmallKanaCodepoint(first_char)) {
    return;
  }

  // Fully spelled-out reduplicated 〜しい adjective (ばかばかしい): the doubled stem is
  // otherwise pre-empted by the abab_pattern ADV candidate and the particle-boundary
  // scan below truncates at the internal か. The trim loops later in this function turn
  // the emitted base into …しく for the negative/adverbial forms.
  addReduplicatedShiiAdjective(candidates, codepoints, start_pos, char_types, inflection,
                               CandidateOrigin::AdjectiveIHiragana);
  appendHiraganaPrefixedKanjiIAdjCandidates(candidates, codepoints, start_pos, char_types, inflection, dict_manager);

  // STEP 1: Find maximum hiragana sequence (without breaking at particles)
  // This allows us to analyze the full sequence first for adjectives like
  // はなはだしい, かわいい, わびしい that contain particle characters
  // Hiragana and the prolonged sound mark (ー), at most 10 chars for adjective + endings.
  size_t max_hiragana_end = start_pos;
  while (max_hiragana_end < char_types.size() && max_hiragana_end - start_pos < 10 &&
         (char_types[max_hiragana_end] == normalize::CharType::Hiragana ||
          normalize::isProlongedSoundMark(codepoints[max_hiragana_end]))) {
    ++max_hiragana_end;
  }

  // The classical attributive is the one cell spelled in two morae (よき).
  appendVowelFusedAdjectiveCandidates(codepoints, start_pos, max_hiragana_end, dict_manager, candidates);

  appendIAdjClassicalAttributiveCandidates(codepoints, start_pos, start_pos + 1, max_hiragana_end, dict_manager,
                                           candidates);

  // Need at least 3 characters for an i-adjective (e.g., あつい)
  if (max_hiragana_end <= start_pos + 2) {
    return;
  }

  const HiraganaAdjectiveRun run{codepoints, start_pos, max_hiragana_end, char_types, inflection, dict_manager};
  appendStemBeforeExcessiveOrAppearance(run, candidates);
  if (opensOnParticleBeforeAuxiliary(run)) {
    return;
  }

  // Add mizenkei (かろ) conjectural candidates (うれしかろう, よかろう) up front, before
  // the particle-boundary early-returns below: よ / な heads are treated as particle
  // starts and would otherwise skip the かろ generation. The inflection analyzer does
  // not emit this form, and it is gated on a decisive i-adjective base to reject the
  // verb-volitional homograph (わかろう).
  appendIAdjKaroCandidates(codepoints, start_pos, start_pos, max_hiragana_end, inflection, dict_manager, candidates);
  appendIAdjOnbinRenyokeiCandidates(codepoints, start_pos, start_pos, max_hiragana_end, inflection, dict_manager,
                                    candidates);
  appendIAdjClassicalTerminalCandidates(codepoints, start_pos, start_pos, max_hiragana_end, dict_manager, candidates);
  appendIAdjKaraZuCandidates(codepoints, start_pos, start_pos, max_hiragana_end, inflection, dict_manager, candidates);

  const std::string full_hiragana_surface = extractSubstring(codepoints, start_pos, max_hiragana_end);
  appendStemBeforeGe(run, full_hiragana_surface, candidates);

  // STEP 2: Determine the hiragana_end for candidate generation
  // If first char is a particle, we only allow the full sequence if it's a valid adjective
  // Otherwise, we break at particle boundaries for shorter subsequences
  size_t hiragana_end = max_hiragana_end;
  bool starts_with_particle = normalize::isExtendedParticle(first_char);
  bool has_prolonged = adj_detail::containsProlongedSoundMark(codepoints, start_pos, max_hiragana_end);

  // For particle-starting sequences without prolonged sound marks,
  // we first check if the full sequence is a valid adjective.
  // If not, we'll skip generating candidates (the lattice will find the particle split)
  if (starts_with_particle && !has_prolonged) {
    const size_t valid_adj_min_end = particleStartedAdjectiveEnd(run);
    // If no valid adjective found, skip this sequence
    // (the lattice will find a better split with the particle)
    if (valid_adj_min_end == start_pos) {
      return;
    }
    // Use the valid adjective length as hiragana_end
    hiragana_end = valid_adj_min_end;
  } else if (!starts_with_particle) {
    hiragana_end = boundedAdjectiveRunEnd(run, full_hiragana_surface);
  }

  // Need at least 3 characters after determining hiragana_end
  if (hiragana_end <= start_pos + 2) {
    return;
  }

  const std::string bounded_surface = extractSubstring(codepoints, start_pos, hiragana_end);
  const bool has_exact_adjective = isAdjectiveInDictionary(dict_manager, bounded_surface);
  if (!has_exact_adjective && startsWithGenitiveParticle(dict_manager, codepoints, start_pos, hiragana_end)) {
    return;
  }
  if (!has_exact_adjective && embedsGenitiveParticle(dict_manager, codepoints, start_pos, hiragana_end)) {
    return;
  }

  adj_detail::appendHiraganaIAdjSurfaceCandidates(codepoints, start_pos, hiragana_end, starts_with_particle, inflection,
                                                  dict_manager, candidates);

  // Add emphatic variants (まずい → まずいっ, etc.)
  addEmphaticVariants(candidates, codepoints, candidate_start);

  // Preserve adjective/auxiliary boundaries. The contracted んかった guard is
  // intentionally specific to this pure-hiragana path.
  static constexpr std::array<adj_detail::TrimmedAdjVariantRule, 6> kHiraganaTrimRules = {{
      {"くない", 2, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 0, "i_adjective_hira_ku"},
      {"くなかった", 4, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 0, "i_adjective_ku_nakatta"},
      {"くなかっ", 3, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 0, "i_adjective_ku_nakatt"},
      {"くて", 1, candidate::kAdjKuSplitBonus, core::ExtendedPOS::AdjRenyokei, 1, "i_adjective_hira_ku_te"},
      {"かった", 1, candidate::kAdjKattSplitBonus, core::ExtendedPOS::AdjKatt, 2, "i_adjective_hira_katt", true},
      {"ければ", 1, candidate::kAdjKeSplitBonus, core::ExtendedPOS::AdjKeForm, 3, "i_adjective_hira_kere"},
  }};
  adj_detail::appendTrimmedAdjVariants(candidates, kHiraganaTrimRules.data(), kHiraganaTrimRules.size(),
                                       candidate_start);
  // The past た is always a separate auxiliary, as on the kanji path: every
  // span ending in かった produced its trimmed かっ variant above.
  candidates.erase(std::remove_if(candidates.begin() + candidate_start, candidates.end(),
                                  [](const UnknownCandidate& cand) { return utf8::endsWith(cand.surface, "かった"); }),
                   candidates.end());

  appendStemBeforeAuxiliaryPattern(run, full_hiragana_surface, candidates);
  appendStemBeforeNominalizerSa(run, candidates);
  appendClippedExclamative(run, candidates);

  // Sort by cost
  verb_helpers::sortCandidatesByCost(candidates, candidate_start);
}

}  // namespace

void generateHiraganaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::vector<normalize::CharType>& char_types,
                                         const grammar::Inflection& inflection,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates) {
  const size_t candidate_start = candidates.size();
  appendHiraganaAdjectiveCandidates(codepoints, start_pos, char_types, inflection, dict_manager, candidates);
  appendPainAdjectiveAfterNominative(codepoints, start_pos, char_types, inflection, dict_manager, candidates);
  dropCoinedAdjectivesOverDictionaryAdjective(dict_manager, codepoints, candidates, candidate_start);
}

void generateKatakanaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::vector<normalize::CharType>& char_types,
                                         const grammar::Inflection& inflection,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates) {
  const size_t candidate_start = candidates.size();

  // Only process katakana-starting positions
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Katakana) {
    return;
  }

  // Find katakana portion (1-6 characters for slang adjective stems)
  // e.g., エモ, キモ, ウザ, ダサ, etc.
  size_t kata_end = findCharRegionEnd(char_types, start_pos, 6, normalize::CharType::Katakana);

  // Need at least 1 katakana character
  if (kata_end == start_pos) {
    return;
  }

  // Fully spelled-out reduplicated 〜しい adjective (バカバカしい): the doubled katakana
  // stem is otherwise pre-empted by the aa_doubled ADV candidate, and its しい ending
  // starts with し, which the ending gate below rejects. Emit the whole-word adjective
  // here; the trim loops after the main loop derive the …しく split.
  addReduplicatedShiiAdjective(candidates, codepoints, start_pos, char_types, inflection, CandidateOrigin::AdjectiveI);

  // The main loop only runs for a katakana stem followed by a valid i-adjective ending
  // start. When it does not apply (e.g. the reduplicated しい handled above), fall through
  // to the shared emphatic/trim/sort tail so the emitted candidate still gets its splits.
  if (kata_end < char_types.size() && char_types[kata_end] == normalize::CharType::Hiragana) {
    // I-adjective endings: い, か(った), く(ない/て), け(れば), さ(そう), そ(う) etc.
    char32_t first_hira = codepoints[kata_end];
    size_t kata_len = kata_end - start_pos;
    bool valid_ending_start = (first_hira == U'い' || first_hira == U'か' || first_hira == U'く' ||
                               first_hira == U'け' || first_hira == U'さ' || first_hira == U'そ');
    // For さ (nominalization), restrict to short katakana stems (2 chars max)
    // Valid: エモさ, キモさ, ウザさ, ダサさ (2-char stems)
    // Invalid: レイプさ (3-char stem, レイプい doesn't exist)
    if (valid_ending_start && !(first_hira == U'さ' && kata_len > 2)) {
      // Find hiragana portion (up to 8 chars for conjugation endings)
      size_t hira_end = findCharRegionEnd(char_types, kata_end, 8, normalize::CharType::Hiragana);

      // Try different ending lengths, starting from longest
      for (size_t end_pos = hira_end; end_pos > kata_end; --end_pos) {
        const std::string surface = extractSubstring(codepoints, start_pos, end_pos);

        // Check all candidates for IAdjective
        const auto& all_candidates = inflection.analyze(surface);
        for (const auto& cand : all_candidates) {
          // Require confidence >= 0.5 for i-adjectives
          if (cand.confidence >= candidate::kIAdjConfMin && cand.verb_type == grammar::VerbType::IAdjective) {
            // Lower cost than pure katakana noun to prefer adjective reading
            // Cost: 0.2-0.35 based on confidence (lower = better)
            float cost = candidate::confidenceScaledCost(candidate::kKanjiAdjBaseCost, cand.confidence,
                                                         candidate::kKanjiAdjConfScale);
            // The nominalizer さ is a suffix on the bare stem (エモ+さ), as on the
            // kana and kanji sides, not a cell of the adjective.
            if (end_pos == kata_end + 1 && first_hira == U'さ') {
              candidates.push_back(makeIAdjStemCandidate(extractSubstring(codepoints, start_pos, kata_end), start_pos,
                                                         kata_end, cand.base_form, cost, CandidateOrigin::AdjectiveI,
                                                         cand.confidence, "i_adjective_kata_stem_sa"));
              break;
            }
            auto adj_cand = makeIAdjCandidate(surface, start_pos, end_pos, cand.base_form, cost,
                                              CandidateOrigin::AdjectiveI, cand.confidence, "i_adjective_kata");
            // Skip exceeds_dict_length penalty - this is a morphologically recognized pattern
            adj_cand.has_suffix = true;
            candidates.push_back(std::move(adj_cand));
            break;  // Only add one adjective candidate per surface
          }
        }
      }
    }
  }

  // Add emphatic variants (エグい → エグいっ, etc.)
  addEmphaticVariants(candidates, codepoints, candidate_start);

  // Preserve the same negative-family boundaries across scripts.
  static constexpr std::array<adj_detail::TrimmedAdjVariantRule, 7> kKatakanaTrimRules = {{
      {"かった", 1, candidate::kAdjKattSplitBonus, core::ExtendedPOS::AdjKatt, 0, "i_adjective_kata_katt"},
      {"くて", 1, candidate::kAdjKuSplitBonus, core::ExtendedPOS::AdjRenyokei, 1, "i_adjective_kata_ku_te"},
      {"くない", 2, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 2, "i_adjective_kata_ku_nai"},
      {"くなかった", 4, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 2,
       "i_adjective_kata_ku_nakatta"},
      {"くなかっ", 3, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 2, "i_adjective_kata_ku_nakatt"},
      {"ければ", 1, candidate::kAdjKeSplitBonus, core::ExtendedPOS::AdjKeForm, 3, "i_adjective_kata_kere"},
      {"そう", 2, candidate::kAdjStemSplitBonus, core::ExtendedPOS::AdjStem, 4, "i_adjective_kata_stem_sou", false,
       true},
  }};
  adj_detail::appendTrimmedAdjVariants(candidates, kKatakanaTrimRules.data(), kKatakanaTrimRules.size(),
                                       candidate_start);

  dropCoinedAdjectivesOverDictionaryAdjective(dict_manager, codepoints, candidates, candidate_start);

  // Sort by cost
  verb_helpers::sortCandidatesByCost(candidates, candidate_start);
}

}  // namespace suzume::analysis
