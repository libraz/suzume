/**
 * @file adjective_candidates_hiragana_scan.cpp
 * @brief Surface qualification for hiragana i-adjective candidates
 */

#include <algorithm>
#include <string>

#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/scorer_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/patterns.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

using verb_helpers::isAdjectiveInDictionary;
using verb_helpers::isVerbInDictionary;

using adj_detail::makeIAdjCandidate;

namespace {

// Normalize prolonged sound marks (ー) to vowels based on preceding character
// e.g., すごーい → すごおい, やばーい → やばあい
// Also handles consecutive marks: すごーーい → すごおおい
std::string normalizeProlongedSoundMark(const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  std::string result;
  result.reserve((end - start) * 3);  // Japanese chars are typically 3 bytes

  for (size_t i = start; i < end; ++i) {
    char32_t ch = codepoints[i];

    // Check for prolonged sound mark (ー, U+30FC)
    if (normalize::isProlongedSoundMark(ch) && i > start) {
      // Find the first non-ー character before this position
      char32_t prev = 0;
      for (size_t j = i; j > start; --j) {
        if (!normalize::isProlongedSoundMark(codepoints[j - 1])) {
          prev = codepoints[j - 1];
          break;
        }
      }
      char32_t vowel = grammar::getVowelForChar(prev);
      normalize::encodeUtf8(vowel, result);
    } else {
      normalize::encodeUtf8(ch, result);
    }
  }

  return result;
}

}  // namespace

// Check if sequence contains a prolonged sound mark
bool adj_detail::containsProlongedSoundMark(const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  for (size_t i = start; i < end; ++i) {
    if (normalize::isProlongedSoundMark(codepoints[i])) {
      return true;
    }
  }
  return false;
}

namespace {

// Normalize the base form of an adjective by removing extra vowels created by
// prolonged sound mark normalization.
// Two patterns:
// 1. すごーい → すごおい → すごい (ー before final い)
// 2. かわいー → かわいい → かわいい (ー after い, extending the い)
// For consecutive marks:
// 1. すごーーい → すごおおい → すごい
// 2. かわいーー → かわいいい → かわいい
std::string normalizeBaseForm(const std::string& base_form, const std::vector<char32_t>& original_codepoints,
                              size_t start, size_t end) {
  if (end < start + 2) {
    return base_form;
  }

  // Count total prolonged marks in the original
  size_t choon_count = 0;
  size_t first_choon_pos = 0;
  for (size_t i = start; i < end; ++i) {
    if (normalize::isProlongedSoundMark(original_codepoints[i])) {
      if (choon_count == 0) {
        first_choon_pos = i;
      }
      ++choon_count;
    }
  }

  if (choon_count == 0) {
    return base_form;
  }

  // Get the character before the first ー to determine which vowel was extended
  char32_t prev_char = (first_choon_pos > start) ? original_codepoints[first_choon_pos - 1] : 0;
  char32_t extended_vowel = grammar::getVowelForChar(prev_char);

  // If the extended vowel is い (pattern: かわいー, かわいーー)
  // The base form should always be with double い (かわいい)
  if (extended_vowel == U'い') {
    if (choon_count <= 1) {
      return base_form;  // Single ー after い → keep as is (already correct: かわいい)
    }
    // Multiple ー's after い → remove extra い's
    // かわいいい (from かわいーー) → かわいい
    size_t extra_i_count = choon_count - 1;  // How many extra い's to remove
    size_t extra_i_bytes = extra_i_count * core::kJapaneseCharBytes;
    if (base_form.size() > extra_i_bytes) {
      // Verify the end has multiple い's
      bool all_is = true;
      for (size_t i = 0; i < extra_i_count && all_is; ++i) {
        size_t pos = base_form.size() - (i + 1) * core::kJapaneseCharBytes;
        if (base_form.substr(pos, core::kJapaneseCharBytes) != "い") {
          all_is = false;
        }
      }
      if (all_is) {
        return base_form.substr(0, base_form.size() - extra_i_bytes);
      }
    }
    return base_form;
  }

  // Other vowels (pattern: すごーい → すごおい → すごい)
  // Remove the extra vowels from base form
  std::string vowel_str;
  normalize::encodeUtf8(extended_vowel, vowel_str);
  size_t vowel_bytes = vowel_str.size();
  size_t total_extra_bytes = vowel_bytes * choon_count;

  if (base_form.size() >= total_extra_bytes + core::kJapaneseCharBytes) {
    // Check if base_form ends with (vowel * count) + い
    size_t check_pos = base_form.size() - total_extra_bytes - core::kJapaneseCharBytes;
    std::string_view suffix(base_form.data() + check_pos, total_extra_bytes + core::kJapaneseCharBytes);

    std::string expected_suffix;
    for (size_t i = 0; i < choon_count; ++i) {
      expected_suffix += vowel_str;
    }
    expected_suffix += "い";

    if (suffix == expected_suffix) {
      // Remove the extra vowels, keep the い
      return base_form.substr(0, check_pos) + "い";
    }
  }

  return base_form;
}

}  // namespace

void adj_detail::appendHiraganaIAdjSurfaceCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                     size_t hiragana_end, bool starts_with_particle,
                                                     const grammar::Inflection& inflection,
                                                     const dictionary::DictionaryManager* dict_manager,
                                                     std::vector<UnknownCandidate>& candidates) {
  // Try different lengths, starting from longest
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 2; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Negative-conjectural まい is an independent auxiliary after a terminal
    // verb form (ある+まい, 行く+まい).  The generic i-adjective analyzer can
    // otherwise reconstruct the whole chain as a fabricated adjective ending
    // in い.  Preserve genuine adjectives such as うまい by requiring the
    // prefix itself to be a dictionary-attested verb.
    if (utf8::endsWith(surface, "まい") && surface.size() > core::kTwoJapaneseCharBytes) {
      const std::string verb_prefix = surface.substr(0, surface.size() - core::kTwoJapaneseCharBytes);
      if (isVerbInDictionary(dict_manager, verb_prefix)) {
        continue;
      }
    }

    // Skip patterns ending with verb passive/potential/causative negative renyokei
    // 〜られなく, 〜れなく, 〜させなく, 〜せなく, 〜されなく are all verb forms,
    // not i-adjectives. E.g., けられなく = ける + られ + ない
    if (grammar::endsWithPassiveCausativeNegativeRenyokei(surface)) {
      continue;  // Skip - passive/potential/causative negative renyokei
    }
    // Skip patterns ending with 〜かなく (verb negative renyokei of godan verbs)
    // E.g., いかなく = いく + ない
    const std::string adjective_ku_lemma =
        utf8::endsWith(surface, "く") ? surface.substr(0, surface.size() - core::kJapaneseCharBytes) + "い" : "";
    if (grammar::endsWithGodanNegativeRenyokei(surface) && !isAdjectiveInDictionary(dict_manager, adjective_ku_lemma)) {
      continue;  // Skip - godan negative renyokei
    }

    // A bare -く is normally an adverbial connective.  The two productive
    // i-adjective continuatives -なく and -しく are recovered below only
    // after their reconstructed base has passed the adjective/verb checks.
    const bool bounded_long_ku_form = utf8::endsWith(surface, "く") && end_pos - start_pos >= 4 &&
                                      end_pos < codepoints.size() &&
                                      (normalize::isKanjiCodepoint(codepoints[end_pos]) ||
                                       normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Katakana);
    // A derived adjective's continuative is recovered the same way: the
    // reconstructed base carries the productive second element, so the -く is
    // its inflection rather than a free adverbial (めんどくさく感じる).
    const bool derived_ku_continuative =
        !adjective_ku_lemma.empty() &&
        adj_detail::derivesFromCompoundFormingAdjective(codepoints, start_pos, adjective_ku_lemma, dict_manager);
    if (utf8::endsWith(surface, "く") && !utf8::endsWith(surface, "くない") && !utf8::endsWith(surface, "なく") &&
        !utf8::endsWith(surface, "しく") && !bounded_long_ku_form && !derived_ku_continuative) {
      continue;
    }

    // Skip patterns ending with just ない (negative auxiliary misidentified as adjective)
    // This prevents でもない from being recognized as an adjective when starting with particle
    // Valid patterns: くない (adjective negative), but ない alone after particles is auxiliary
    if (starts_with_particle && utf8::endsWith(surface, "ない") && !utf8::endsWith(surface, "くない")) {
      continue;  // Skip - likely negative auxiliary, not adjective
    }

    // Skip short patterns starting with common case particles (で, に, を, と)
    // These are likely particle + adjective splits (でやばい = で + やばい)
    // Longer sequences (5+ chars) are less likely to be splits
    if (starts_with_particle) {
      size_t char_count = end_pos - start_pos;
      char32_t first_char = codepoints[start_pos];
      // Common case particles that frequently precede adjectives
      bool starts_with_case_particle =
          (first_char == U'で' || first_char == U'に' || first_char == U'を' || first_char == U'と');
      if (starts_with_case_particle && char_count <= 4) {
        continue;  // Skip - likely particle + adjective split
      }
    }

    // Normalize prolonged sound marks before analysis
    // e.g., すごーい → すごおい, やばーい → やばあい
    std::string analysis_surface = surface;
    bool has_prolonged = adj_detail::containsProlongedSoundMark(codepoints, start_pos, end_pos);
    if (has_prolonged) {
      analysis_surface = normalizeProlongedSoundMark(codepoints, start_pos, end_pos);
    }

    // The inflection analyzer accepts an i-adjective's dictionary form, but
    // not every productive hiragana renyokei directly.  Restore its final い
    // only for the analysis, while the candidate retains the observed く-form
    // and therefore receives AdjRenyokei from detectIAdjEpos().
    if (utf8::endsWith(analysis_surface, "く")) {
      analysis_surface = normalize::replaceFinalChar(analysis_surface, "い");
    }

    // Check all candidates for IAdjective, not just the best one
    // This handles cases where Suru interpretation may have higher confidence
    const auto& all_candidates = inflection.analyze(analysis_surface);
    const bool has_verified_verb_reading = adj_detail::hasDictionaryVerbAnalysis(all_candidates, dict_manager);
    for (const auto& cand : all_candidates) {
      // For hiragana-only adjectives, require higher confidence (0.55) than
      // kanji+hiragana adjectives (0.50) to avoid false positives like しそう → しい
      // For patterns with prolonged sound marks, lower threshold (0.40) since these
      // are intentional colloquial expressions (すごーい, やばーい)
      // Multiple consecutive marks (すごーーい) result in even lower confidence
      // For particle-starting sequences, lower threshold (0.50) since these have
      // already been validated as forming valid adjectives (はなはだしい, かわいい)
      // Four-or-more-character dictionary forms carry enough stem-length
      // evidence to use the same threshold as particle-headed adjectives.
      // This admits regular long forms such as まばゆい without weakening the
      // highly ambiguous three-character hiragana pattern.
      const bool has_long_dictionary_form = utf8::endsWith(surface, "い") && end_pos - start_pos >= 4;
      // A run built from a nominal host plus a productive second element is
      // adjectival by derivation, so the length-based confidence score — which
      // measures how ordinary the stem looks — carries no evidence against it
      // (めんどくさい, うそくさかった). Use the compound floor instead.
      const bool derives_from_suffix =
          adj_detail::derivesFromCompoundFormingAdjective(codepoints, start_pos, cand.base_form, dict_manager);
      float confidence_threshold = has_prolonged         ? candidate::kHiraAdjConfProlonged
                                   : derives_from_suffix ? candidate::kCompoundAdjConfMin
                                   : starts_with_particle || has_long_dictionary_form || bounded_long_ku_form
                                       ? candidate::kHiraAdjConfParticle
                                       : candidate::kHiraAdjConfMin;
      const bool is_unverified_adverbial_i_adjective =
          !starts_with_particle && !has_verified_verb_reading &&
          ((utf8::endsWith(surface, "なく") && utf8::endsWith(cand.base_form, "ない")) ||
           (utf8::endsWith(surface, "しく") && utf8::endsWith(cand.base_form, "しい"))) &&
          cand.confidence >= candidate::kHiraAdjUnverifiedNaiRenyokeiMin;
      // A particle-headed, unregistered -なく candidate has no lexical
      // evidence for its reconstructed adjective.  Leave the closed particle
      // sequence intact instead of accepting a near-threshold non-word such
      // as かとない. Registered adjectives (はかない, かわいい) remain on their
      // ordinary inflection path.
      const bool is_unverified_particle_naku = starts_with_particle && utf8::endsWith(surface, "なく") &&
                                               !isAdjectiveInDictionary(dict_manager, cand.base_form);
      if (cand.verb_type == grammar::VerbType::IAdjective && !is_unverified_particle_naku &&
          (cand.confidence >= confidence_threshold || is_unverified_adverbial_i_adjective)) {
        // A bare -げない reconstruction is ambiguous with the productive
        // suffix げ followed by the negative auxiliary.  Without lexical
        // evidence for the complete adjective, retain that grammatical
        // boundary (さりげ + なく) rather than coining a whole-word adjective.
        if (utf8::endsWith(surface, "げなく") && !isAdjectiveInDictionary(dict_manager, cand.base_form)) {
          continue;
        }
        // 様態 そう is a separate auxiliary, never an inflectional ending of
        // an i-adjective. This mirrors the kanji-adjective guard and keeps
        // derived forms split (ほし + そう + だ, やす + そう + だ).
        {
          std::string_view base_sv(cand.base_form);
          std::string_view surf_sv(surface);
          if (utf8::endsWith(base_sv, "い")) {
            std::string_view stem_sv = base_sv.substr(0, base_sv.size() - core::kJapaneseCharBytes);
            if (surf_sv.size() > stem_sv.size() && utf8::startsWith(surf_sv, stem_sv) &&
                utf8::startsWith(surf_sv.substr(stem_sv.size()), scorer::kSuffixSou)) {
              SUZUME_DEBUG_LOG_VERBOSE("[HIRA_ADJ_SKIP] \"" << surface
                                                            << "\" spans 様態そう, stem path handles split\n");
              continue;
            }
          }
        }
        // For particle-starting sequences, require stem length >= 2 characters
        // This prevents に+そうな from being recognized as にい (invalid)
        if (starts_with_particle && normalize::utf8Length(cand.stem) < 2) {
          continue;  // Stem too short for a valid adjective
        }
        // A particle followed by a dictionary verb in a complete past/te form is
        // not an adjective: にかかった → に + かかる, mis-reconstructed as the
        // non-word base にかい. Gate on a た/て/だ/で ending so a bare renyokei tail
        // (でかい → で + 買い) cannot fire — those are genuine adjectives, not verbs.
        if (starts_with_particle && !isAdjectiveInDictionary(dict_manager, cand.base_form) &&
            (utf8::endsWith(surface, "た") || utf8::endsWith(surface, "て") || utf8::endsWith(surface, "だ") ||
             utf8::endsWith(surface, "で"))) {
          std::string after_particle = extractSubstring(codepoints, start_pos + 1, end_pos);
          bool tail_is_dict_verb = false;
          for (const auto& vres : inflection.analyze(after_particle)) {
            if (vres.verb_type == grammar::VerbType::IAdjective) {
              continue;
            }
            if (isVerbInDictionary(dict_manager, vres.base_form)) {
              tail_is_dict_verb = true;
              break;
            }
          }
          if (tail_is_dict_verb) {
            SUZUME_DEBUG_LOG_VERBOSE("[HIRA_ADJ_SKIP] \"" << surface << "\" particle + dict verb, skipping\n");
            continue;
          }
        }
        // For prolonged sound mark patterns, require normalized stem >= 2 characters
        // The inflection analyzer works on the choon-expanded form (ばーい → ばあい),
        // so cand.stem may be 2+ chars (ばあ). We must check the final normalized
        // base form: normalizeBaseForm removes the duplicate vowel (ばあい → ばい),
        // giving stem "ば" (1 char) which is too short for a valid adjective.
        // e.g., ばーい → ばあい → ばい → stem "ば" (1 char) = invalid, skip
        //       やばーい → やばあい → やばい → stem "やば" (2 chars) = valid
        if (has_prolonged) {
          std::string normalized_base = normalizeBaseForm(cand.base_form, codepoints, start_pos, end_pos);
          // Stem = base form minus trailing い (3 bytes in UTF-8)
          size_t normalized_stem_len = normalize::utf8Length(normalized_base);
          if (normalized_stem_len >= 1) {
            // Subtract 1 for the trailing い
            if (normalized_stem_len - 1 < 2) {
              continue;  // Normalized stem too short for a valid adjective
            }
          }
        }
        // Skip なさそう pattern - should be split as な(ADJ stem) + さ(Suffix) + そう(AUX)
        // This pattern is the nominalization of ない + そう (appearance auxiliary)
        // The inflection analyzer incorrectly treats なさ as stem of なさい (honorific)
        // Check: surface ends with さそう AND (stem ends with さ OR surface is exactly なさそう)
        if (utf8::endsWith(surface, "さそう")) {
          // Check if this is the な+さ+そう pattern (ない nominalization)
          // Pattern: 1 char before さそう (like なさそう where な is the ない stem)
          size_t surface_len = normalize::utf8Length(surface);
          if (surface_len == 4 && utf8::endsWith(cand.stem, "さ")) {
            continue;  // Skip - should be split as な+さ+そう
          }
        }
        // Skip んかった pattern - this is contracted negative (ん) + past (かった)
        // e.g., らんかった would create らんい which is invalid
        // くだらんかった should be くだら+ん+かっ+た, not くだ+らんかっ+た
        if (utf8::endsWith(surface, "んかった")) {
          continue;  // Skip - should be split as ん+かっ+た
        }
        // Skip surfaces that are honorific verb renyokei (ending with さ)
        // e.g., くださ + い = ください is VERB (くださる renyokei), not i-adjective
        // These are typically honorific verb conjugations ending with さ
        if (surface == "くださ" || surface == "なさ" || surface == "いらっしゃ" || surface == "おっしゃ" ||
            surface == "ござ") {
          continue;  // Skip - honorific verb renyokei, not i-adjective
        }
        // Skip hiragana patterns ending with たい - these are verb renyokei + tai (desire)
        // e.g., ねたい should be ね + たい (寝たい), not i-adjective
        //       みたい context-dependent: auxiliary (見たい/似たい) vs mimetic (みたいな)
        //       したい should be し + たい, not i-adjective
        // Note: 痛い (itai) has kanji, so not affected
        if (utf8::endsWith(surface, "たい") && surface != "たい") {
          continue;  // Skip - should be split as verb renyokei + たい
        }
        // Skip pure hiragana patterns ending with さ - these are almost always part of
        // honorific suffix さん/さま, not i-adjective forms
        // e.g., おじさ, おばさ, おねえさ should not be recognized as i-adjectives
        if (utf8::endsWith(surface, "さ") && surface.size() >= 9) {  // 3+ chars ending with さ
          continue;  // Skip - likely part of honorific suffix さん/さま
        }
        // Skip さそう patterns (adj nominalization + appearance auxiliary)
        // e.g., よさそうに → よ + さ + そう + に, not よさい (invalid adj)
        //       なさそう → な + さ + そう (handled separately)
        if (utf8::endsWith(surface, "さそう") || utf8::endsWith(surface, "さそうに") ||
            utf8::endsWith(surface, "さそうな") || utf8::endsWith(surface, "さそうだ")) {
          continue;  // Skip - should be split as adj-stem + さ + そう
        }
        // Skip candidates containing て/で in stem - indicates verb te-form boundary
        // No genuine i-adjective has て or で in its stem
        // e.g., さましてほしい should be さまし+て+ほしい, not a single i-adj
        {
          auto stem = surface.substr(0, surface.size() - 3);  // Remove trailing い (3 bytes)
          if (stem.find("て") != std::string::npos || stem.find("で") != std::string::npos) {
            continue;
          }
        }
        // Skip adj renyokei + なる patterns — these are adj く-form + auxiliary verb なる
        // e.g., なくなった = なく(adj renyokei) + なっ(なる) + た, not a single adjective
        //       よくなった = よく(adj renyokei) + なっ(なる) + た
        if (verb_helpers::containsKuNaruPattern(surface)) {
          continue;
        }
        // Base cost for hiragana i-adjective candidates
        // Use slightly elevated base to avoid fragments like ろしい beating
        // kanji adjectives like 恐ろしい (kanji adj base=0.2F)
        float cost = candidate::confidenceScaledCost(candidate::kHiraganaAdjBaseCost, cand.confidence,
                                                     candidate::kHiraganaAdjConfScale);
        if (has_prolonged) {
          cost += candidate::kProlongedSoundBonus;  // Bonus for colloquial patterns like すごーい
          SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" -0.1 (prolonged_sound_bonus)\n");
        }
        // Length-based bonus for adjectives starting with particle characters
        // Short sequences (3-4 chars like につい, でやばい) are likely splits
        // Longer sequences (5+ chars like かわいい, はなはだしい) are real adjectives
        if (starts_with_particle) {
          size_t char_count = end_pos - start_pos;
          if (char_count >= 5) {
            cost += candidate::kLongParticleAdjBonus;  // Strong bonus for long adjectives (はなはだしい)
            SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" -0.5 (long_particle_adj_bonus)\n");
          }
          // No bonus for 3-4 char sequences (につい, でやばい) - likely particle + adjective split
        }
        if (bounded_long_ku_form) {
          cost += candidate::kBoundedHiraganaKuAdjBonus;
          SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" -0.5 (bounded_ku_adj_bonus)\n");
        }
        // Set lemma to base form from inflection analysis
        // For prolonged sound mark patterns, normalize the base form
        // e.g., すごおい → すごい, やばあい → やばい
        // normalizeBaseForm already collapses however many marks the emphasis used, so
        // the count does not change the dictionary form: すごーい and すごーーい are both
        // すごい. Falling back to the surface here left a non-word as the lemma.
        std::string lemma =
            has_prolonged ? normalizeBaseForm(cand.base_form, codepoints, start_pos, end_pos) : cand.base_form;
        const char* pattern = has_prolonged ? "i_adjective_hira_choon" : "i_adjective_hira";
        auto adjective = makeIAdjCandidate(surface, start_pos, end_pos, lemma, cost,
                                           CandidateOrigin::AdjectiveIHiragana, cand.confidence, pattern);
        if (bounded_long_ku_form) {
          adjective.has_suffix = true;
        }
        candidates.push_back(std::move(adjective));
        break;  // Only add one adjective candidate per surface
      }
    }
  }
}

}  // namespace suzume::analysis
