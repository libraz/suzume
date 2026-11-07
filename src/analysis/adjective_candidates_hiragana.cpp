/**
 * @file adjective_candidates_hiragana.cpp
 * @brief Hiragana i-adjective candidate generation
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

using adj_detail::addReduplicatedShiiAdjective;
using adj_detail::appendClippedExclamative;
using adj_detail::appendHiraganaPrefixedKanjiIAdjCandidates;
using adj_detail::appendStemBeforeAuxiliaryPattern;
using adj_detail::appendStemBeforeExcessiveOrAppearance;
using adj_detail::appendStemBeforeGe;
using adj_detail::appendStemBeforeNominalizerSa;
using adj_detail::dropCoinedAdjectivesOverDictionaryAdjective;
using adj_detail::HiraganaAdjectiveRun;
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

// A registered i-adjective whose ending has fused into a long e (すげえ・すげー
// for すごい, やべぇ for やばい) is that adjective, with its standard lemma.
void appendVowelFusedAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         size_t max_hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates) {
  constexpr size_t kMaxStemMorae = 4;
  if (dict_manager == nullptr) {
    return;
  }
  for (size_t fused_at = start_pos + 1; fused_at + 1 < max_hiragana_end && fused_at - start_pos <= kMaxStemMorae;
       ++fused_at) {
    const char32_t long_e = codepoints[fused_at + 1];
    if (long_e != U'え' && long_e != U'ぇ' && !normalize::isProlongedSoundMark(long_e)) {
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
  if (dict_manager == nullptr) {
    return false;
  }
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
  return false;
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
      bounded_long_ku_form &&
      adj_detail::iAdjectiveConfidenceAtLeast(bounded_candidates, candidate::kHiraAdjConfParticle) != float{};
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

  appendIAdjOnbinRenyokeiCandidates(codepoints, start_pos, start_pos, max_hiragana_end, inflection, dict_manager,
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

}  // namespace suzume::analysis
