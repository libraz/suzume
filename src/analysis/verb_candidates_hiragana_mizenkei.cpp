/**
 * @file verb_candidates_hiragana_mizenkei.cpp
 * @brief Internal pure-hiragana verb candidate patterns
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

namespace suzume::analysis::hiragana_verb_detail {
namespace vh = verb_helpers;

// @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
bool endsWithParticleAfterVerb(const dictionary::DictionaryManager* dict_manager, const grammar::Inflection& inflection,
                               const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos <= start_pos) {
    return false;
  }
  const size_t total_len = end_pos - start_pos;
  if (total_len < 3) {  // need a 1+ char prefix and a 2+ char particle suffix
    return false;
  }
  for (size_t prefix_len = 1; prefix_len + 2 <= total_len; ++prefix_len) {
    size_t split = start_pos + prefix_len;
    const dictionary::DictionaryEntry* suffix_entry = lookupEntryInRange(*dict_manager, codepoints, split, end_pos);
    if (suffix_entry == nullptr || suffix_entry->pos != core::PartOfSpeech::Particle ||
        (suffix_entry->extended_pos != core::ExtendedPOS::ParticleBinding &&
         suffix_entry->extended_pos != core::ExtendedPOS::ParticleAdverbial)) {
      continue;
    }
    if (prefix_len == 1) {
      return true;
    }
    // Probe the prefix for a verb. Strip a leading te-form particle first, since
    // て/で + verb + しか (てみるしか = て + みる + しか) is a subsidiary-verb
    // sequence whose verb sits after て.
    size_t probe_start = start_pos;
    if (codepoints[start_pos] == U'て' || codepoints[start_pos] == U'で') {
      probe_start = start_pos + 1;
    }
    std::string probe = extractSubstring(codepoints, probe_start, split);
    if (vh::isVerbInDictionary(dict_manager, probe)) {
      return true;
    }
    const auto& analysis = inflection.analyze(probe);
    if (!analysis.empty() && analysis[0].verb_type != grammar::VerbType::Unknown &&
        analysis[0].confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
      return true;
    }
  }
  return false;
}

// True when a pronoun word ends exactly at @p pos (誰/何/だれ/なに/どこ/いつ …).
// A following か is then the particle か (誰か + いる), never the 2nd mora of a
// godan-wa verb stem, so a か…renyokei candidate at @p pos must be discouraged.
bool pronounEndsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                   size_t pos) {
  if (dict_manager == nullptr || pos == 0) {
    return false;
  }
  const size_t max_len = pos < 3 ? pos : 3;
  return hasDictionaryEntryEndingAt(*dict_manager, codepoints, pos - max_len, pos,
                                    partOfSpeechMask(core::PartOfSpeech::Pronoun));
}

bool deriveGodanMizenkeiForms(const std::vector<char32_t>& codepoints, size_t start_pos, size_t mizenkei_end,
                              GodanMizenkeiForms& out) {
  out.a_row_char = codepoints[mizenkei_end - 1];
  if (!grammar::isARowCodepoint(out.a_row_char)) {
    return false;
  }
  out.verb_type = grammar::verbTypeFromARowCodepoint(out.a_row_char);
  out.base_suffix = grammar::godanBaseSuffixFromARow(out.a_row_char);
  if (out.verb_type == grammar::VerbType::Unknown || out.base_suffix.empty()) {
    return false;
  }
  out.mizenkei_surface = extractSubstring(codepoints, start_pos, mizenkei_end);
  out.stem = extractSubstring(codepoints, start_pos, mizenkei_end - 1);
  out.base_form = normalize::concat(out.stem, out.base_suffix);
  return true;
}

// Passive mizenkei candidates for pure-hiragana verbs (いわれる → いわ + れる).
// Preserve the boundary between the A-row mizenkei and passive れ.
void appendPassiveMizenkeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  // Key insight: A-row hiragana (わ,か,さ,た,な,ま,ら,が,etc.) + れ pattern
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 2; --end_pos) {
    // Check if position end_pos-1 is A-row hiragana (mizenkei ending)
    // and position end_pos is れ (passive pattern start)
    size_t mizenkei_end = end_pos - 1;  // Position after A-row char
    if (mizenkei_end <= start_pos)
      continue;

    const char32_t next_char = codepoints[mizenkei_end];  // Should be れ

    // Check for passive pattern start (れる, れた, れて, etc.). The shared
    // reconstruction below validates the preceding A-row mizenkei ending.
    if (next_char != U'れ') {
      continue;
    }

    // Check for passive patterns after れ
    // All passive patterns split at the mizenkei (いわ + れる/れ).
    // Loose ま-branch: bare ま (れます, れました, れません, れませんでした) qualifies.
    bool is_passive_pattern = vh::isPassiveAuxContinuation(codepoints, mizenkei_end + 1, /*strict_masu=*/false);

    if (!is_passive_pattern) {
      continue;
    }

    GodanMizenkeiForms forms;
    if (!deriveGodanMizenkeiForms(codepoints, start_pos, mizenkei_end, forms)) {
      continue;
    }
    const grammar::VerbType verb_type = forms.verb_type;
    const std::string& mizenkei_surface = forms.mizenkei_surface;
    const std::string& stem = forms.stem;
    const std::string& base_form = forms.base_form;

    // Check if mizenkei surface exists in dictionary as a verb
    // This handles cases like いわ which is registered with lemma いう
    // OR check if base form exists (for kanji compounds like 言わ)
    bool is_valid_verb = vh::isVerbInDictionary(dict_manager, mizenkei_surface);
    if (!is_valid_verb) {
      // Fallback: check the reconstructed base form (kanji compounds like 言わ→言う)
      is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
    }
    // Fallback for GodanSa: use inflection analysis for causative verb patterns
    // E.g., やらされた = やらさ (mizenkei of やらす) + れ + た
    // やらす is the causative form of やる but not in dictionary
    if (!is_valid_verb && verb_type == grammar::VerbType::GodanSa) {
      is_valid_verb = vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                             candidate::verb_cost::kConstructedVerbMinConfidence, true);
    }

    if (!is_valid_verb) {
      continue;
    }

    // For GodanSa passive, check if this is causative+passive pattern
    // E.g., やらさ+れ+た = causative+passive of やる (not passive of やらす)
    // If the stem (without さ) is a valid godan verb mizenkei, penalize
    // the merged candidate so the decomposed path (やら+さ+れ+た) can compete
    float causative_passive_penalty = 0.0F;
    if (verb_type == grammar::VerbType::GodanSa && dict_manager != nullptr && mizenkei_end - start_pos >= 2) {
      char32_t stem_last = codepoints[mizenkei_end - 2];
      auto inner_suffix = grammar::godanBaseSuffixFromARow(stem_last);
      if (!inner_suffix.empty()) {
        std::string inner_stem = extractSubstring(codepoints, start_pos, mizenkei_end - 2);
        std::string inner_base = normalize::concat(inner_stem, inner_suffix);
        if (vh::isVerbInDictionary(dict_manager, inner_base)) {
          causative_passive_penalty = bigram_cost::kStrong;
        }
      }
    }

    // Skip GodanRa passive for known ichidan verbs (いる, きる, ねる, etc.)
    // These 2-char verbs ending in る are ichidan, not godan-ra.
    // The ichidan path (い+られる) is handled separately.
    // Only skip when stem is 1 hiragana char (3 bytes) = base form is 2 chars (6 bytes)
    if (verb_type == grammar::VerbType::GodanRa && stem.size() == 3) {
      // Known ichidan 2-char verbs (stem is 1 char before ら):
      // いる, きる, みる, ねる, でる, にる, ひる, etc.
      // Godan-ra 2-char verbs: やる, なる, ある, とる, のる, etc.
      char32_t stem_char = codepoints[start_pos];
      // ichidan stems: い,き,み,ね,で,に,ひ,び (E-row or I-row before る)
      bool is_known_ichidan =
          (stem_char == U'い' || stem_char == U'き' || stem_char == U'み' || stem_char == U'ね' || stem_char == U'で' ||
           stem_char == U'に' || stem_char == U'ひ' || stem_char == U'び' || stem_char == U'え');
      if (is_known_ichidan) {
        continue;
      }
    }

    // Get lemma from dictionary entry if mizenkei is registered
    // Otherwise use constructed base form
    std::string lemma = vh::lookupVerbLemma(dict_manager, mizenkei_surface, base_form);

    // Preserve the mizenkei boundary (いわ + れる/れ), including the chain
    // いわれません → いわ + れ + ませ + ん.
    // Previous strategy of splitting at passive renyokei (いわれ + ません) was incorrect
    size_t split_end = mizenkei_end;
    std::string surface = extractSubstring(codepoints, start_pos, split_end);
    const char* pattern_name = "passive_mizenkei";

    float cost = candidate::verb_cost::kStandardBonus + causative_passive_penalty;
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " hiragana_" << pattern_name << " lemma=" << lemma
                          << " cost=" << cost << "\n";
    }
    candidates.push_back(makeVerbCandidate(surface, start_pos, split_end, cost, lemma,
                                           grammar::verbTypeToConjType(verb_type), true, CandidateOrigin::VerbHiragana,
                                           0.9F, "hiragana_passive_mizenkei", core::ExtendedPOS::VerbMizenkei));
    break;  // Only generate one passive candidate per length
  }
}

// Ichidan renyokei candidates before られ (potential/passive) for pure hiragana
// (いられる → い + られる). Splits at the ichidan stem.
void appendIchidanRareruCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                   const grammar::Inflection& inflection,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates) {
  // Pattern: ichidan stem (E-row ending or い/え) + られ + る/た/て
  // Search for られ starting at positions from start_pos+1 to hiragana_end-2
  for (size_t ra_pos = start_pos + 1; ra_pos + 2 < hiragana_end; ++ra_pos) {
    // Check for られ pattern at this position
    if (codepoints[ra_pos] != U'ら' || codepoints[ra_pos + 1] != U'れ') {
      continue;
    }

    // Check for られる, られた, られて, られな, られま patterns
    // れ sits at ra_pos+1, so the continuation index is ra_pos+2 (loose ま-branch).
    bool is_potential_passive_pattern = vh::isPassiveAuxContinuation(codepoints, ra_pos + 2, /*strict_masu=*/false);

    if (!is_potential_passive_pattern) {
      continue;
    }

    // The stem is everything before ら
    size_t stem_end = ra_pos;  // Exclusive end of stem
    if (stem_end <= start_pos)
      continue;

    std::string stem = extractSubstring(codepoints, start_pos, stem_end);

    // A hiragana Ichidan stem ends in the e- or i-row before る.  Dictionary
    // membership alone is insufficient because Godan-ra verbs such as おる
    // would otherwise fabricate an Ichidan stem before られ.
    const char32_t stem_last = codepoints[stem_end - 1];
    if (!grammar::isERowCodepoint(stem_last) && !grammar::isIRowCodepoint(stem_last)) {
      continue;
    }

    // A hiragana tail starting immediately after kanji can otherwise turn a
    // preceding ichidan stem plus causative into a fabricated verb
    // (食+べさせ+られる). The causative させ is a closed auxiliary here, so
    // leave the kanji-aware stem candidate to retain 食べ+させ+られる. Other
    // kanji-adjacent hiragana verbs (夜かけて, 添付いたしました) remain
    // available to the ordinary candidate generators.
    const bool is_kanji_tail_causative =
        start_pos > 0 && normalize::classifyChar(codepoints[start_pos - 1]) == normalize::CharType::Kanji &&
        utf8::endsWith(stem, "させ");
    if (is_kanji_tail_causative) {
      continue;
    }

    // Skip stems containing て or で - these are te-form + subsidiary verb patterns
    // E.g., しておられた → stem=してお is actually して(te-form)+おる(subsidiary), not ichidan しておる
    //       つないでおられた → stem=つないでお is つないで(te-form)+おる, not ichidan
    if (stem.find("て") != std::string::npos || stem.find("で") != std::string::npos) {
      continue;
    }

    std::string base_form = stem + "る";  // Ichidan base form = stem + る

    // Validate: check if base form is a known ichidan verb
    // For pure hiragana like いる, check the dictionary
    bool is_valid_ichidan = vh::isVerbInDictionary(dict_manager, base_form);

    // The complete passive form is stronger evidence than the ambiguous bare
    // base. A long unknown hiragana stem may have a low-confidence Ichidan
    // reading by itself (たくわえる), while stem+られる still traces back to
    // that exact stem and lemma. Project that observed analysis instead of
    // selecting only the highest-scoring homographic Godan interpretation.
    if (!is_valid_ichidan && normalize::utf8Length(stem) >= 3) {
      const std::string observed_surface = extractSubstring(codepoints, start_pos, hiragana_end);
      for (const auto& observed : inflection.analyze(observed_surface)) {
        if (observed.verb_type == grammar::VerbType::Ichidan && observed.base_form == base_form &&
            observed.stem == stem && utf8::startsWith(observed.suffix, "られ")) {
          is_valid_ichidan = true;
          break;
        }
      }
    }

    // Special case: common hiragana ichidan verbs (いる, おきる, みる, etc.)
    // These may not always be in the L2 dictionary but are valid
    if (!is_valid_ichidan) {
      // Check if inflection analysis recognizes base_form as ichidan
      const auto& analysis = inflection.analyze(base_form);
      for (const auto& inflected : analysis) {
        if (inflected.verb_type == grammar::VerbType::Ichidan && inflected.base_form == base_form &&
            inflected.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
          is_valid_ichidan = true;
          break;
        }
      }
    }

    // A complete passive auxiliary supplies stronger evidence for a short
    // unknown Ichidan stem when the run begins in a predicate slot. This is
    // what licenses のせ+られた after a case particle, without admitting an
    // interior fabricated stem such as どせ+ない in とりもどせない.
    if (!is_valid_ichidan && (start_pos == 0 || vh::followsCaseParticle(dict_manager, codepoints, start_pos))) {
      for (const auto& inflected : inflection.analyze(base_form)) {
        if (inflected.verb_type == grammar::VerbType::Ichidan && inflected.base_form == base_form &&
            inflected.confidence >= candidate::kV1PrefixMinConfidence) {
          is_valid_ichidan = true;
          break;
        }
      }
    }

    if (!is_valid_ichidan) {
      continue;
    }

    // A direct dictionary match for the reconstructed lemma resolves an
    // otherwise ambiguous stem cell.  For example, the historical やむ
    // conditional and the Ichidan やめる share やめ, but only the complete
    // latter lemma licenses the passive construction here.
    std::string lemma = vh::isVerbInDictionary(dict_manager, base_form)
                            ? base_form
                            : vh::lookupVerbLemma(dict_manager, stem, base_form);

    // A stem can be a homograph of a different inflection.  In particular,
    // the classical する form せ must not be reinterpreted as the continuative
    // form of a fabricated lexical せる before passive られる.  Keep this
    // candidate only when the stem's lexical lemma agrees with the reconstructed
    // Ichidan base; otherwise the closed-class auxiliary candidate owns the
    // boundary.  This is lemma-based rather than surface-specific, so the same
    // guard rejects every conflicting inflectional homograph.
    if (lemma != base_form) {
      continue;
    }

    // Generate the ichidan renyokei candidate
    // Negative cost to beat the single-word verb candidate
    constexpr float kCost = candidate::verb_cost::kStandardBonus;
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem << " hiragana_ichidan_rareru lemma=" << lemma << " cost=" << kCost
                          << "\n";
    }
    // The specialized origin carries a scorer bonus for the quotative
    // と+み+られる construction.  Do not attach it to every one-character
    // hiragana stem, or an adjective tail can fabricate 確か+め+られる.
    const CandidateOrigin origin = start_pos > 0 && codepoints[start_pos - 1] == U'と'
                                       ? CandidateOrigin::VerbHiraganaPassiveRenyokei
                                       : CandidateOrigin::VerbHiragana;
    const bool validated_short_predicate =
        normalize::utf8Length(stem) == 2 &&
        (start_pos == 0 || vh::followsCaseParticle(dict_manager, codepoints, start_pos));
    candidates.push_back(
        makeVerbCandidate(stem, start_pos, stem_end, kCost, lemma, dictionary::ConjugationType::Ichidan, true, origin,
                          0.9F, "hiragana_ichidan_rareru",
                          validated_short_predicate ? core::ExtendedPOS::VerbMizenkei : core::ExtendedPOS::Unknown));
    break;  // Only generate one ichidan rareru candidate per starting position
  }
}

}  // namespace suzume::analysis::hiragana_verb_detail
