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
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

using verb_helpers::addEmphaticVariants;
using verb_helpers::embedsCaseParticle;
using verb_helpers::findCharRegionEnd;
using verb_helpers::isAdjectiveInDictionary;
using verb_helpers::isEmphaticChar;
using verb_helpers::isVerbInDictionary;

using adj_detail::makeIAdjCandidate;
using adj_detail::makeIAdjStemCandidate;
using adj_detail::makeNaAdjCandidate;

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
  const size_t earliest_start = start_pos > kMaxParticleChars ? start_pos - kMaxParticleChars : 0;
  for (size_t particle_start = earliest_start; particle_start < start_pos; ++particle_start) {
    if (end_pos - particle_start < 2) {
      continue;
    }
    const std::string particle_surface = extractSubstring(codepoints, particle_start, end_pos);
    if (dict_manager->lookupExact(particle_surface, core::PartOfSpeech::Particle) != nullptr) {
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
  const std::string whole_surface = extractSubstring(codepoints, start_pos, end_pos);
  constexpr PartOfSpeechMask kLexicalMask =
      partOfSpeechMask(core::PartOfSpeech::Noun) | partOfSpeechMask(core::PartOfSpeech::Verb) |
      partOfSpeechMask(core::PartOfSpeech::Adjective) | partOfSpeechMask(core::PartOfSpeech::Adverb);
  if (hasExactPartOfSpeech(*dict_manager, whole_surface, kLexicalMask)) {
    return false;
  }
  return maximalSegmentCount(*dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Particle) > 0;
}

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
  size_t hira_end = verb_helpers::findCharRegionEnd(char_types, shi_pos, 8, normalize::CharType::Hiragana);
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

void appendHiraganaPrefixedKanjiIAdjCandidates(std::vector<UnknownCandidate>& candidates,
                                               const std::vector<char32_t>& codepoints, size_t start_pos,
                                               const std::vector<normalize::CharType>& char_types,
                                               const grammar::Inflection& inflection,
                                               const dictionary::DictionaryManager* dict_manager) {
  // A one-to-three-mora hiragana prefix is otherwise indistinguishable from
  // an attached particle within a sentence (いまだ+に続く).  Restrict this
  // recovery path to a lexical word boundary; ordinary kanji adjective and
  // particle candidates retain responsibility inside a clause.
  const bool follows_particle =
      start_pos > 0 && dict_manager != nullptr &&
      lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle) != nullptr;
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
    if (dict_manager->lookupExact(prefix_surface, core::PartOfSpeech::Prefix) != nullptr) {
      return;
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
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (embedsCaseParticle(dict_manager, codepoints, start_pos, end_pos)) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    const std::string tail_observed_surface = extractSubstring(codepoints, prefix_end, end_pos);
    // Recover both the adverbial -く form and a complete attributive form
    // before its lexical head.  A leading particle/auxiliary prefix was
    // rejected above, so compositional phrases such as から+美しい are not
    // absorbed into the compound candidate.
    const bool is_renyokei = utf8::endsWith(tail_observed_surface, "く");
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
      float cost = candidate::confidenceScaledCost(candidate::kCompoundAdjBaseCost, inflection_candidate.confidence,
                                                   candidate::kKanjiAdjConfScale) +
                   candidate::kCompoundIAdjectiveLexicalBonus;
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

}  // namespace

void generateHiraganaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::vector<normalize::CharType>& char_types,
                                         const grammar::Inflection& inflection,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates) {
  const size_t candidate_start = candidates.size();

  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Hiragana) {
    return;
  }

  char32_t first_char = codepoints[start_pos];

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
  size_t max_hiragana_end = start_pos;
  while (max_hiragana_end < char_types.size() &&
         max_hiragana_end - start_pos < 10) {  // Max 10 chars for adjective + endings
    normalize::CharType curr_type = char_types[max_hiragana_end];
    char32_t curr_char = codepoints[max_hiragana_end];

    // Allow hiragana and prolonged sound mark (ー)
    bool is_valid = (curr_type == normalize::CharType::Hiragana);
    if (!is_valid && normalize::isProlongedSoundMark(curr_char)) {
      is_valid = true;
    }

    if (!is_valid) {
      break;
    }
    ++max_hiragana_end;
  }

  // Need at least 3 characters for an i-adjective (e.g., あつい)
  if (max_hiragana_end <= start_pos + 2) {
    return;
  }

  // The excessive auxiliary follows every i-adjective stem, not only the
  // しい/きい subclasses handled by the shared pattern table below. Rebuild the
  // base with its terminal い and let inflection validate it, so だる+すぎる,
  // ゆる+すぎる, and きつ+すぎる share one productive path.  This must precede
  // the general genitive-particle guard: that guard correctly rejects broad
  // unknown sequences, but would prevent this independently verified stem.
  const bool starts_with_closed_particle =
      dict_manager != nullptr &&
      lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Particle) != nullptr;
  // A kanji i-adjective's continuative く is already a complete edge at the
  // preceding position (高く+なり+すぎる).  Starting a second adjective stem
  // from that okurigana would reconstruct a non-word such as くなりい.  This
  // leaves ordinary hiragana stems after particles available while preserving
  // the kanji-adjective boundary.
  const bool follows_kanji_continuative =
      start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]) && first_char == U'く';
  if (!starts_with_closed_particle && !follows_kanji_continuative) {
    const std::string full_surface = extractSubstring(codepoints, start_pos, max_hiragana_end);
    static constexpr std::array<std::string_view, 3> kExcessiveSuffixes = {"すぎる", "すぎた", "すぎ"};
    for (const std::string_view suffix : kExcessiveSuffixes) {
      if (!utf8::endsWith(full_surface, suffix)) {
        continue;
      }
      const std::string stem = full_surface.substr(0, full_surface.size() - suffix.size());
      if (normalize::utf8Length(stem) < 2 || utf8::contains(stem, "て") || utf8::contains(stem, "で")) {
        continue;
      }
      const std::string base_form = stem + "い";
      const float confidence = adj_detail::firstConfidenceAtLeast(
          inflection.analyze(base_form), grammar::VerbType::IAdjective, candidate::kCompoundAdjConfMin);
      if (confidence == candidate::kNoOriginConfidence) {
        continue;
      }
      const size_t stem_end = start_pos + normalize::utf8Length(stem);
      const float cost =
          candidate::confidenceScaledCost(candidate::kAdjStemExtCost, confidence, candidate::kAdjStemConfScale);
      candidates.push_back(makeIAdjStemCandidate(stem, start_pos, stem_end, base_form, cost,
                                                 CandidateOrigin::AdjectiveIHiragana, confidence,
                                                 "adj_stem_hira_excessive"));
      break;
    }
  }

  // A closed-class particle immediately followed by a registered auxiliary
  // inflection is a grammatical boundary, not an i-adjective stem. This keeps
  // など+いない (and the same particle+auxiliary shape) from becoming a
  // fabricated adjective candidate.
  if (dict_manager != nullptr) {
    constexpr size_t kMaxParticleChars = 4;
    size_t max_particle_end = std::min(max_hiragana_end, start_pos + kMaxParticleChars);
    for (size_t particle_end = start_pos + 1; particle_end <= max_particle_end; ++particle_end) {
      std::string particle_surface = extractSubstring(codepoints, start_pos, particle_end);
      if (dict_manager->lookupExact(particle_surface, core::PartOfSpeech::Particle) == nullptr) {
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
        std::string aux_surface = extractSubstring(codepoints, particle_end, aux_end);
        if (dict_manager->lookupExact(aux_surface, core::PartOfSpeech::Auxiliary) != nullptr) {
          std::string full_surface = extractSubstring(codepoints, start_pos, max_hiragana_end);
          if (utf8::endsWith(full_surface, "く")) {
            full_surface = normalize::replaceFinalChar(full_surface, "い");
          }
          const auto& full_candidates = inflection.analyze(full_surface);
          const bool has_full_i_adjective =
              std::any_of(full_candidates.begin(), full_candidates.end(),
                          [](const grammar::InflectionCandidate& inflection_candidate) {
                            return inflection_candidate.verb_type == grammar::VerbType::IAdjective &&
                                   inflection_candidate.confidence >= candidate::kHiraAdjConfParticle &&
                                   normalize::utf8Length(inflection_candidate.stem) >= 2;
                          });
          if (!has_full_i_adjective) {
            return;
          }
        }
      }
    }
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

  // -げ derives from an i-adjective stem while retaining the morpheme boundary
  // before the closed suffix.  The base adjective is checked by the same
  // inflection engine as ordinary hiragana adjectives; a verified verb reading
  // blocks the derivation so a verb continuative plus the lexical suffix is
  // preserved.  A following ない-family belongs to a lexical ...げない form,
  // not to the derivational suffix construction.
  const std::string full_hiragana_surface = extractSubstring(codepoints, start_pos, max_hiragana_end);
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
    const auto& base_candidates = inflection.analyze(base_form);
    const float adjective_confidence = adj_detail::firstConfidenceAtLeast(
        base_candidates, grammar::VerbType::IAdjective, candidate::kDerivedSuffixAdjectiveConfidence);
    const bool has_verified_verb_reading = adj_detail::hasDictionaryVerbAnalysis(base_candidates, dict_manager);
    if (adjective_confidence != candidate::kNoOriginConfidence && !has_verified_verb_reading) {
      candidates.push_back(makeIAdjStemCandidate(
          stem, start_pos, start_pos + normalize::utf8Length(stem), base_form, candidate::kDerivedSuffixAdjectiveCost,
          CandidateOrigin::AdjectiveIHiragana, adjective_confidence, "i_adjective_ge_stem"));
    }
  }

  // STEP 2: Determine the hiragana_end for candidate generation
  // If first char is a particle, we only allow the full sequence if it's a valid adjective
  // Otherwise, we break at particle boundaries for shorter subsequences
  size_t hiragana_end = max_hiragana_end;
  bool starts_with_particle = normalize::isExtendedParticle(first_char);
  bool has_prolonged = adj_detail::containsProlongedSoundMark(codepoints, start_pos, max_hiragana_end);

  // For particle-starting sequences without prolonged sound marks,
  // we first check if the full sequence is a valid adjective.
  // If not, we'll skip generating candidates (the lattice will find the particle split)
  size_t valid_adj_min_end = start_pos;  // Minimum end position for a valid adjective
  if (starts_with_particle && !has_prolonged) {
    // Check if the full sequence (or any length) forms a valid adjective
    // Use lower threshold (0.50) for particle-starting sequences to catch
    // words like かわいい (confidence=0.51)
    for (size_t end = max_hiragana_end; end > start_pos + 2; --end) {
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
      if (utf8::endsWith(test_surface, "く") && !utf8::endsWith(test_surface, "くない") && !bounded_long_ku_form) {
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
      const auto& test_candidates = inflection.analyze(analysis_surface);
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
    // If no valid adjective found, skip this sequence
    // (the lattice will find a better split with the particle)
    if (valid_adj_min_end == start_pos) {
      return;
    }
    // Use the valid adjective length as hiragana_end
    hiragana_end = valid_adj_min_end;
  } else if (!starts_with_particle) {
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
    const auto& bounded_candidates = inflection.analyze(bounded_analysis_surface);
    const bool has_bounded_i_adjective =
        bounded_long_ku_form && adj_detail::firstConfidenceAtLeast(bounded_candidates, grammar::VerbType::IAdjective,
                                                                   candidate::kHiraAdjConfParticle) != float{};
    hiragana_end = has_bounded_i_adjective ? max_hiragana_end : start_pos;
    while (!has_bounded_i_adjective && hiragana_end < max_hiragana_end) {
      char32_t curr_char = codepoints[hiragana_end];

      // Only break at strong particle boundaries after minimum stem length
      if (hiragana_end - start_pos >= 3 && !normalize::isProlongedSoundMark(curr_char)) {
        bool next_is_prolonged =
            (hiragana_end + 1 < char_types.size() && normalize::isProlongedSoundMark(codepoints[hiragana_end + 1]));
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
          if (!is_katt_past && (normalize::isExtendedParticle(curr_char) || curr_char == U'や')) {
            break;  // Stop before the particle
          }
        }
      }
      ++hiragana_end;
    }
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

  // Add stem candidates for pure hiragana adjective + auxiliary patterns
  // This handles patterns like おいしそう → おいし (stem) + そう (aux)
  // Similar to the kanji adjective stem logic at lines 1673-1785
  // Check for しそう, しすぎ patterns (adjective stem + auxiliary)
  // Start from maximum hiragana sequence
  std::string full_surface = extractSubstring(codepoints, start_pos, max_hiragana_end);

  for (size_t pattern_index = 0; pattern_index < adj_detail::kHiraganaIAdjStemAuxPatternCount; ++pattern_index) {
    const std::string_view aux_pattern = adj_detail::kIAdjStemAuxPatterns[pattern_index];
    if (full_surface.size() >=
            aux_pattern.size() + core::kTwoJapaneseCharBytes &&  // Need at least 2 chars before pattern
        full_surface.find(aux_pattern) != std::string::npos) {
      // Find where the pattern starts
      size_t pattern_pos = full_surface.find(aux_pattern);
      if (pattern_pos < core::kTwoJapaneseCharBytes) {
        continue;  // Stem too short (need at least 2 chars like おいし, うれし)
      }

      // The stem is everything before the auxiliary pattern, including the し
      std::string stem = full_surface.substr(0, pattern_pos + 3);  // +3 for し
      std::string base_form = stem + "い";                         // e.g., おいし → おいしい

      // A te-form connective cannot be part of an i-adjective stem. Keep its
      // boundary in desiderative-looking chains (読ん+で+ほし+そう,
      // 書い+て+ほし+そう) instead of fabricating an adjective that absorbs it.
      if (utf8::contains(stem, "て") || utf8::contains(stem, "で")) {
        continue;
      }

      // Validate that this forms a valid i-adjective
      const auto& adj_results = inflection.analyze(base_form);
      const float adj_confidence =
          adj_detail::firstConfidenceAtLeast(adj_results, grammar::VerbType::IAdjective, candidate::kIAdjConfMin);

      if (adj_confidence == 0.0F) {
        continue;
      }

      // Check that this is NOT a verb renyokei (e.g., 話し from 話す)
      // For pure hiragana, check if stem + す would be a valid verb
      // We compare adjective vs verb confidence - if adjective is significantly higher, prefer it
      std::string verb_stem = stem.substr(0, stem.size() - 3);  // Remove し
      std::string verb_form = verb_stem + "す";                 // e.g., おい + す = おいす (not real)

      // Check verb confidence from inflection analyzer
      const auto& verb_results = inflection.analyze(verb_form);
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
      size_t stem_end = start_pos + stem_char_count;

      // Generate stem candidate with strong bonus
      // おい (INTJ) has cost -1, so stem needs very low cost to win
      float cost =
          candidate::confidenceScaledCost(candidate::kAdjStemExtCost, adj_confidence, candidate::kAdjStemConfScale);
      SUZUME_DEBUG_LOG("[ADJ_STEM_HIRA] ✓ candidate stem=\"" << stem << "\" base=\"" << base_form << "\" cost=" << cost
                                                             << "\n");
      candidates.push_back(makeIAdjStemCandidate(stem, start_pos, stem_end, base_form, cost,
                                                 CandidateOrigin::AdjectiveIHiragana, adj_confidence,
                                                 "adj_stem_hira_sou"));
      break;  // Only one stem candidate per pattern
    }
  }

  // A complete i-adjective paradigm exposes its stem before the productive
  // nominalizer さ (やさし+さ, うれし+さ).  The ordinary hiragana scanner only
  // recognizes い/く/かっ forms. Reconstruct the base form and require both a
  // valid adjective analysis and a real boundary after さ; this admits open
  // adjective vocabulary without mistaking さん/さま inside nouns for the
  // nominalizer.
  for (size_t stem_end = start_pos + 2; stem_end < max_hiragana_end; ++stem_end) {
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
         (after_sa < char_types.size() && char_types[after_sa] == normalize::CharType::Symbol));
    if (!bounded_nominalizer) {
      continue;
    }

    const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
    // A closed interjection is not an i-adjective stem.  In particular, its
    // following さ belongs to an unknown noun reading (うわさ), not to the
    // productive adjective nominalizer.
    if (dict_manager != nullptr && dict_manager->lookupExact(stem, core::PartOfSpeech::Interjection) != nullptr) {
      continue;
    }
    const std::string base_form = stem + "い";
    const bool is_dict_adjective = isAdjectiveInDictionary(dict_manager, base_form);
    if (!is_dict_adjective && grammar::isERowCodepoint(codepoints[stem_end - 1])) {
      continue;
    }
    const float adjective_confidence =
        is_dict_adjective ? candidate::kDictionaryOriginConfidence
                          : adj_detail::firstConfidenceAtLeast(inflection.analyze(base_form),
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

  // Sort by cost
  verb_helpers::sortCandidatesByCost(candidates, candidate_start);

  return;
}

void generateKatakanaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::vector<normalize::CharType>& char_types,
                                         const grammar::Inflection& inflection,
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
        std::string surface = extractSubstring(codepoints, start_pos, end_pos);

        if (surface.empty()) {
          continue;
        }

        // Check all candidates for IAdjective
        const auto& all_candidates = inflection.analyze(surface);
        for (const auto& cand : all_candidates) {
          // Require confidence >= 0.5 for i-adjectives
          if (cand.confidence >= candidate::kIAdjConfMin && cand.verb_type == grammar::VerbType::IAdjective) {
            // Lower cost than pure katakana noun to prefer adjective reading
            // Cost: 0.2-0.35 based on confidence (lower = better)
            float cost = candidate::confidenceScaledCost(candidate::kKanjiAdjBaseCost, cand.confidence,
                                                         candidate::kKanjiAdjConfScale);
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

  // Sort by cost
  verb_helpers::sortCandidatesByCost(candidates, candidate_start);

  return;
}

}  // namespace suzume::analysis
