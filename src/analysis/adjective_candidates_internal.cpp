/**
 * @file adjective_candidates_internal.cpp
 * @brief Shared adjective candidate transformation helpers
 */

#include "adjective_candidates_internal.h"

#include <algorithm>
#include <utility>

#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "core/utf8_constants.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis::adj_detail {

namespace {

// Productive second elements of compound adjectives: they attach to a nominal
// or a verb continuative to derive a new adjective rather than predicating over
// a separate preceding word.
constexpr std::array<std::string_view, 15> kCompoundFormingAdjectives = {
    "苦しい", "深い", "強い",   "臭い",   "くさい", "難い",   "にくい", "易い",
    "やすい", "辛い", "づらい", "がたい", "ぽい",   "っぽい", "らしい"};

// A one-mora host is indistinguishable from an inflectional ending that the
// analyzer folded into the reconstructed base, so a derivation needs two.
constexpr size_t kMinDerivedAdjectiveHostLength = 2;

// Particle classes that bind a nominal phrase and therefore close it. A
// 接続助詞 or 終助詞 attaches to a predicate instead, so it cannot mark the end
// of a host (めんど ends in the concessive ど without being a phrase).
constexpr std::array<core::ExtendedPOS, 5> kNominalPhraseParticles = {
    core::ExtendedPOS::ParticleCase, core::ExtendedPOS::ParticleTopic, core::ExtendedPOS::ParticleAdverbial,
    core::ExtendedPOS::ParticleNo, core::ExtendedPOS::ParticleBinding};

core::ExtendedPOS detectIAdjEpos(const std::string& surface) {
  if (utf8::endsWith(surface, "かっ")) {
    return core::ExtendedPOS::AdjKatt;
  }
  if (utf8::endsWith(surface, "けれ")) {
    return core::ExtendedPOS::AdjKeForm;
  }
  if (utf8::endsWith(surface, "かろ")) {
    return core::ExtendedPOS::AdjMizenkei;
  }
  if (utf8::endsWith(surface, "く")) {
    return core::ExtendedPOS::AdjRenyokei;
  }
  return core::ExtendedPOS::AdjBasic;
}

}  // namespace

bool opensAdjectivePastConnective(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos + 1 < codepoints.size() && codepoints[pos] == U'か' && codepoints[pos + 1] == U'っ' &&
         (pos == 0 || codepoints[pos - 1] != U'な');
}

bool spansPastAdjectiveEnding(const std::string& surface, const std::string& base_form) {
  if (!utf8::endsWith(base_form, "い")) {
    return false;
  }
  const std::string_view stem = std::string_view(base_form).substr(0, base_form.size() - core::kJapaneseCharBytes);
  if (surface.size() <= stem.size() || !utf8::startsWith(surface, stem)) {
    return false;
  }
  const std::string_view ending = std::string_view(surface).substr(stem.size());
  if (utf8::startsWith(ending, scorer::kSuffixSou)) {
    return true;
  }
  // The connective is medial only when the ending continues past it. Both
  // spellings close a clause, and the voiced one follows the same onbin stems the
  // analyzer mistakes for a stem (読ん|で|いく).
  for (const std::string_view connective : {"て", "で"}) {
    for (size_t pos = ending.find(connective); pos != std::string_view::npos;
         pos = ending.find(connective, pos + connective.size())) {
      if (pos + connective.size() < ending.size()) {
        return true;
      }
    }
  }
  return false;
}

bool isCompoundFormingAdjective(const std::string& base_form) {
  return std::find(kCompoundFormingAdjectives.begin(), kCompoundFormingAdjectives.end(), base_form) !=
         kCompoundFormingAdjectives.end();
}

bool derivesFromCompoundFormingAdjective(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::string& base_form,
                                         const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return false;
  }
  // Longest match: ぽい and っぽい share a tail, and the shorter one would
  // leave the promoted っ at the end of the host.
  size_t suffix_length = 0;
  for (const std::string_view suffix : kCompoundFormingAdjectives) {
    if (utf8::endsWith(base_form, suffix)) {
      suffix_length = std::max(suffix_length, normalize::utf8Length(suffix));
    }
  }
  if (suffix_length == 0) {
    return false;
  }
  const size_t base_length = normalize::utf8Length(base_form);
  if (base_length < suffix_length + kMinDerivedAdjectiveHostLength) {
    return false;
  }
  const size_t host_end = start_pos + base_length - suffix_length;
  if (host_end > codepoints.size()) {
    return false;
  }
  constexpr PartOfSpeechMask kFunctionWordMask =
      partOfSpeechMask(core::PartOfSpeech::Particle) | partOfSpeechMask(core::PartOfSpeech::Auxiliary) |
      partOfSpeechMask(core::PartOfSpeech::Determiner) | partOfSpeechMask(core::PartOfSpeech::Conjunction) |
      partOfSpeechMask(core::PartOfSpeech::Adverb) | partOfSpeechMask(core::PartOfSpeech::Prefix);
  if (hasExactPartOfSpeech(*dict_manager, extractSubstring(codepoints, start_pos, host_end), kFunctionWordMask)) {
    return false;
  }
  for (size_t particle_start = start_pos + 1; particle_start < host_end; ++particle_start) {
    const auto* particle =
        lookupEntryInRange(*dict_manager, codepoints, particle_start, host_end, core::PartOfSpeech::Particle);
    if (particle != nullptr && std::find(kNominalPhraseParticles.begin(), kNominalPhraseParticles.end(),
                                         particle->extended_pos) != kNominalPhraseParticles.end()) {
      return false;
    }
  }
  return true;
}

const std::array<std::string_view, 14> kIAdjStemAuxPatterns = {
    "しそう", "しそうだ", "しそうな", "しそうに", "しすぎ", "しすぎる", "しすぎた",
    "きそう", "きそうだ", "きそうな", "きそうに", "きすぎ", "きすぎる", "きすぎた",
};

float firstConfidenceAtLeast(const std::vector<grammar::InflectionCandidate>& candidates, grammar::VerbType type,
                             float minimum) {
  for (const auto& candidate : candidates) {
    if (candidate.verb_type == type && candidate.confidence >= minimum) {
      return candidate.confidence;
    }
  }
  return float{};
}

float maxConfidenceFor(const std::vector<grammar::InflectionCandidate>& candidates,
                       std::initializer_list<grammar::VerbType> types) {
  float confidence{};
  for (const auto& candidate : candidates) {
    if (std::find(types.begin(), types.end(), candidate.verb_type) != types.end()) {
      confidence = std::max(confidence, candidate.confidence);
    }
  }
  return confidence;
}

bool hasDictionaryVerbAnalysis(const std::vector<grammar::InflectionCandidate>& candidates,
                               const dictionary::DictionaryManager* dict_manager) {
  for (const auto& candidate : candidates) {
    if (candidate.verb_type != grammar::VerbType::IAdjective &&
        verb_helpers::isVerbInDictionary(dict_manager, candidate.base_form)) {
      return true;
    }
  }
  return false;
}

bool isVerbOnbinContextAfterI(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return false;
  }
  const char32_t next = codepoints[pos];
  if (next == U'て' || next == U'た' || next == U'だ' || next == U'や') {
    return true;
  }
  if (next == U'で') {
    return pos + 1 >= codepoints.size() || codepoints[pos + 1] != U'す';
  }
  return false;
}

UnknownCandidate makeIAdjCandidate(const std::string& surface, size_t start, size_t end, const std::string& lemma,
                                   float cost, [[maybe_unused]] CandidateOrigin origin,
                                   [[maybe_unused]] float confidence, [[maybe_unused]] const char* pattern) {
  auto candidate =
      makeCandidate(surface, start, end, core::PartOfSpeech::Adjective, cost, false, origin, detectIAdjEpos(surface));
  candidate.lemma = lemma;
#ifdef SUZUME_DEBUG_INFO
  candidate.confidence = confidence;
  candidate.pattern = pattern;
#endif
  return candidate;
}

UnknownCandidate makeNaAdjCandidate(const std::string& surface, size_t start, size_t end, float cost, bool has_suffix,
                                    [[maybe_unused]] CandidateOrigin origin, [[maybe_unused]] float confidence,
                                    [[maybe_unused]] const char* pattern) {
  auto candidate = makeCandidate(surface, start, end, core::PartOfSpeech::Adjective, cost, has_suffix, origin,
                                 core::ExtendedPOS::AdjNaAdj);
#ifdef SUZUME_DEBUG_INFO
  candidate.confidence = confidence;
  candidate.pattern = pattern;
#endif
  return candidate;
}

UnknownCandidate makeIAdjStemCandidate(const std::string& surface, size_t start, size_t end, const std::string& lemma,
                                       float cost, [[maybe_unused]] CandidateOrigin origin,
                                       [[maybe_unused]] float confidence, [[maybe_unused]] const char* pattern) {
  auto candidate =
      makeCandidate(surface, start, end, core::PartOfSpeech::Adjective, cost, true, origin, core::ExtendedPOS::AdjStem);
  candidate.lemma = lemma;
#ifdef SUZUME_DEBUG_INFO
  candidate.confidence = confidence;
  candidate.pattern = pattern;
#endif
  return candidate;
}

UnknownCandidate makeTrimmedAdjVariant(const UnknownCandidate& candidate, size_t char_trim, float cost_bonus,
                                       core::ExtendedPOS epos, [[maybe_unused]] const char* pattern) {
  UnknownCandidate variant;
  variant.surface = candidate.surface.substr(0, candidate.surface.size() - char_trim * core::kJapaneseCharBytes);
  variant.start = candidate.start;
  variant.end = candidate.end - char_trim;
  variant.pos = core::PartOfSpeech::Adjective;
  variant.lemma = candidate.lemma;
  variant.cost = candidate.cost + cost_bonus;
  variant.has_suffix = true;
  variant.extended_pos = epos;
#ifdef SUZUME_DEBUG_INFO
  variant.origin = candidate.origin;
  variant.confidence = candidate.confidence;
  variant.pattern = pattern;
#endif
  return variant;
}

void appendTrimmedAdjVariants(std::vector<UnknownCandidate>& candidates, const TrimmedAdjVariantRule* rules,
                              size_t rule_count, size_t first_index,
                              const dictionary::DictionaryManager* dict_manager) {
  const size_t source_count = candidates.size();
  size_t group_begin = 0;
  while (group_begin < rule_count) {
    size_t group_end = group_begin + 1;
    while (group_end < rule_count && rules[group_end].group == rules[group_begin].group) {
      ++group_end;
    }
    for (size_t candidate_idx = first_index; candidate_idx < source_count; ++candidate_idx) {
      for (size_t rule_idx = group_begin; rule_idx < group_end; ++rule_idx) {
        const TrimmedAdjVariantRule& rule = rules[rule_idx];
        const std::string& surface = candidates[candidate_idx].surface;
        if (!utf8::endsWith(surface, rule.suffix)) {
          continue;
        }
        if (rule.reject_contracted_n_past && utf8::endsWith(surface, "んかった")) {
          continue;
        }
        if (rule.require_nonempty_stem && surface.size() <= rule.char_trim * core::kJapaneseCharBytes) {
          continue;
        }

        UnknownCandidate variant =
            makeTrimmedAdjVariant(candidates[candidate_idx], rule.char_trim, rule.cost_bonus, rule.epos,
#ifdef SUZUME_DEBUG_INFO
                                  rule.pattern
#else
                                  nullptr
#endif
            );
        if (rule.prefer_dictionary_lemma && dict_manager != nullptr &&
            verb_helpers::isAdjectiveInDictionary(dict_manager, candidates[candidate_idx].lemma)) {
          variant.cost = candidate::verb_cost::kStrongBonus;
        }
        candidates.push_back(std::move(variant));
      }
    }
    group_begin = group_end;
  }
}

}  // namespace suzume::analysis::adj_detail
