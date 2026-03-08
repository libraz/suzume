/**
 * @file verb_candidates.cpp
 * @brief Verb-based unknown word candidate generation
 */

#include "verb_candidates.h"

#include <algorithm>
#include <cmath>

#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"

namespace suzume::analysis {

// Alias for helper functions
namespace vh = verb_helpers;

std::vector<UnknownCandidate> generateCompoundVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager,
    const VerbCandidateOptions& verb_opts) {
  std::vector<UnknownCandidate> candidates;

  // Requires dictionary to verify base forms
  if (dict_manager == nullptr) {
    return candidates;
  }

  // Pattern: Kanji+ Hiragana(1-3) Kanji+ Hiragana+
  // e.g., 恐(K)れ(H)入(K)ります(H), 差(K)し(H)上(K)げます(H)
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find first kanji portion (1-2 chars)
  size_t kanji1_end = vh::findCharRegionEnd(char_types, start_pos, 3,
                                             normalize::CharType::Kanji);

  if (kanji1_end == start_pos || kanji1_end >= char_types.size()) {
    return candidates;
  }

  // Find first hiragana portion (1-3 chars, typically verb renyoukei ending)
  if (char_types[kanji1_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  size_t hira1_end = vh::findCharRegionEnd(char_types, kanji1_end, 4,
                                            normalize::CharType::Hiragana);

  // Find second kanji portion (must exist for compound verb)
  if (hira1_end >= char_types.size() ||
      char_types[hira1_end] != normalize::CharType::Kanji) {
    return candidates;
  }

  size_t kanji2_end = vh::findCharRegionEnd(char_types, hira1_end, 3,
                                             normalize::CharType::Kanji);

  // Skip compound verb candidates when second verb is aspectual/grammatical
  // These should be tokenized separately for MeCab compatibility:
  // 終わる/終える, 始める, 続く/続ける, 過ぎる
  // e.g., 読み終わる → 読み + 終わる (not single token)
  if (hira1_end < codepoints.size()) {
    char32_t second_kanji = codepoints[hira1_end];
    if (second_kanji == U'終' || second_kanji == U'始' ||
        second_kanji == U'続' || second_kanji == U'過') {
      return candidates;
    }
  }

  // Find second hiragana portion (conjugation ending)
  if (kanji2_end >= char_types.size() ||
      char_types[kanji2_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  size_t hira2_end = vh::findCharRegionEnd(char_types, kanji2_end, 10,
                                            normalize::CharType::Hiragana);

  // Try different ending lengths
  for (size_t end_pos = hira2_end; end_pos > kanji2_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    if (surface.empty()) {
      continue;
    }

    // Skip verb + ます auxiliary patterns for MeCab-compatible split
    // e.g., 申し上げます → 申し上げ + ます
    if (utf8::endsWith(surface, "ます") ||
        utf8::endsWith(surface, "ました") ||
        utf8::endsWith(surface, "ません") ||
        utf8::endsWith(surface, "ましょう")) {
      continue;
    }

    // Use inflection analyzer to get potential base forms
    auto inflection_candidates = inflection.analyze(surface);

    for (const auto& infl_cand : inflection_candidates) {
      if (infl_cand.confidence < verb_opts.confidence_low) {
        continue;
      }

      // Check if base form exists in dictionary as a verb
      auto results = dict_manager->lookup(infl_cand.base_form, 0);
      for (const auto& result : results) {
        if (result.entry == nullptr) {
          continue;
        }
        if (result.entry->surface != infl_cand.base_form) {
          continue;
        }
        if (result.entry->pos != core::PartOfSpeech::Verb) {
          continue;
        }

        // v0.8: conj_type removed - just verify verb exists in dictionary
        // Found a match! Generate candidate
        // Note: Don't set lemma here - let lemmatizer derive it more accurately
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, end_pos, verb_opts.base_cost_low, "",
            dictionary::ConjugationType::None,  // v0.8: conj_type no longer used
            false, CandidateOrigin::VerbCompound, infl_cand.confidence,
            grammar::verbTypeToString(infl_cand.verb_type).data()));
        return candidates;  // Return first valid match
      }
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> generateKatakanaVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager,
    const VerbCandidateOptions& verb_opts) {
  std::vector<UnknownCandidate> candidates;

  // Only process katakana-starting positions
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Katakana) {
    return candidates;
  }

  // Find katakana portion (1-8 characters for slang verb stems)
  size_t kata_end = vh::findCharRegionEnd(char_types, start_pos, 8,
                                           normalize::CharType::Katakana);

  // Need at least 1 katakana character
  if (kata_end == start_pos) {
    return candidates;
  }

  // Must be followed by hiragana (conjugation endings)
  if (kata_end >= char_types.size() ||
      char_types[kata_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Check if first hiragana could be a verb ending
  // Common verb endings start with: る, っ, ん, ら, り, れ, ろ, さ, し, せ, た, て, etc.
  char32_t first_hira = codepoints[kata_end];
  // Skip if it's clearly a particle
  if (normalize::isParticleCodepoint(first_hira)) {
    return candidates;
  }

  // Find hiragana portion (conjugation endings, up to 10 chars)
  size_t hira_end = vh::findCharRegionEnd(char_types, kata_end, 10,
                                           normalize::CharType::Hiragana);

  // Need at least 1 hiragana for conjugation
  if (hira_end <= kata_end) {
    return candidates;
  }

  // Reject katakana stem + すぎ pattern (any length)
  // MeCab splits KATAKANA + すぎる into separate tokens:
  //   シンプル 名詞,一般,*,*,*,*,*
  //   すぎる   動詞,自立,*,*,一段,基本形,すぎる,スギル,スギル
  // So we skip verb candidates like シンプルすぎない to force split path
  std::string hira_part = extractSubstring(codepoints, kata_end, hira_end);
  // C++17 compatible: check if starts with "すぎ" (6 bytes)
  if (hira_part.size() >= 6 && hira_part.compare(0, 6, "すぎ") == 0) {
    return candidates;  // Skip this candidate - force split path
  }

  // Reject katakana stem + そう pattern (appearance auxiliary)
  // MeCab splits カタカナ + そう into separate tokens:
  //   キモ 名詞,一般,*,*,*,*,*  (or adjective stem)
  //   そう 名詞,接尾,助動詞語幹,*,*,*,そう,ソウ,ソー
  // So we skip verb candidates like キモそう to force split path
  // C++17 compatible: check if starts with "そう" (6 bytes)
  if (hira_part.size() >= 6 && hira_part.compare(0, 6, "そう") == 0) {
    return candidates;  // Skip this candidate - force split path
  }

  // Reject katakana stem + する conjugation patterns for サ変 splitting
  // MeCab splits KATAKANA + する into separate tokens:
  //   エッチ + し + て + いる    (progressive)
  //   レイプ + さ + れる        (passive)
  //   セックス + さ + せる      (causative)
  // So we skip verb candidates where hiragana starts with し/さ/せ
  // to force noun + する-conjugation split path
  if (hira_part.size() >= 3 &&
      (hira_part.compare(0, 3, "し") == 0 ||   // している, した, して, しない
       hira_part.compare(0, 3, "さ") == 0 ||   // される, させる, さない
       hira_part.compare(0, 3, "せ") == 0)) {  // せる (causative short form)
    return candidates;  // Skip this candidate - force split path
  }

  // Reject katakana stem + たい/たく (desiderative auxiliary)
  // MeCab splits KATAKANA + たい/たく into separate tokens:
  //   ハメ + たい      (desiderative)
  //   ハメ + たく + なる (desiderative renyokei + naru)
  // Note: たXX with 2+ hiragana after た triggers skip; ハメた (past) is OK
  if (hira_part.size() >= 6 &&
      (hira_part.compare(0, 6, "たい") == 0 ||   // たい (desiderative)
       hira_part.compare(0, 6, "たく") == 0)) {   // たく (desiderative renyokei)
    return candidates;  // Skip this candidate - force split path
  }

  // Try different ending lengths, starting from longest
  for (size_t end_pos = hira_end; end_pos > kata_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Check if this looks like a conjugated verb using inflection analyzer
    auto best = inflection.getBest(surface);

    // Only accept verb types (not IAdjective) and require reasonable confidence
    // Reject Suru type when hiragana doesn't start with する conjugation (し/さ/せ/す)
    // e.g., ハメた → inflection says Suru but た is not する conjugation → use ichidan instead
    if (best.verb_type == grammar::VerbType::Suru) {
      // Suru inflection was incorrectly matched; try next best candidate
      auto all_results = inflection.analyze(surface);
      best.confidence = 0.0F;
      for (const auto& cand : all_results) {
        if (cand.verb_type != grammar::VerbType::Suru &&
            cand.verb_type != grammar::VerbType::IAdjective &&
            cand.confidence > best.confidence) {
          best = cand;
        }
      }
    }
    if (best.confidence > verb_opts.confidence_katakana &&
        best.verb_type != grammar::VerbType::IAdjective) {
      // Lower cost than pure katakana noun to prefer verb reading
      // Cost: 0.4-0.55 based on confidence (lower = better)
      float cost = verb_opts.base_cost_standard + (1.0F - best.confidence) * verb_opts.confidence_cost_scale;
      candidates.push_back(makeVerbCandidate(
          surface, start_pos, end_pos, cost, best.base_form,
          grammar::verbTypeToConjType(best.verb_type),
          false, CandidateOrigin::VerbKatakana, best.confidence,
          grammar::verbTypeToString(best.verb_type).data()));
    }
  }

  // Add emphatic variants (パニくるっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints);

  // Generate katakana sokuonbin (っ) candidates for ta/te-form splitting
  // E.g., バズった → バズっ (onbin of バズる) + た (auxiliary)
  //       ググった → ググっ (onbin of ググる) + た (auxiliary)
  // This allows MeCab-compatible splitting for katakana slang verbs
  {
    // Check if hiragana part starts with っ followed by た/て/だ/で
    if (hira_part.size() >= 6 &&  // っ(3 bytes) + た/て/だ/で(3 bytes) = 6 bytes min
        hira_part.compare(0, 3, "っ") == 0) {
      std::string_view second_char(hira_part.data() + 3, 3);
      if (second_char == "た" || second_char == "て" ||
          second_char == "だ" || second_char == "で") {
        // Found katakana + っ + た/て pattern
        // Generate sokuonbin stem candidate: カタカナ + っ
        std::string onbin_surface = extractSubstring(codepoints, start_pos, kata_end + 1);
        std::string kata_part = extractSubstring(codepoints, start_pos, kata_end);
        std::string base_form = kata_part + "る";  // Assume godan-ra (most common for slang)

        // Skip if katakana stem is already a dict noun (e.g., フェラ, ネタ)
        // These should be noun+って, not verb sokuonbin+て
        bool skip_sokuonbin = false;
        if (dict_manager) {
          auto stem_results = dict_manager->lookup(kata_part, 0);
          for (const auto& r : stem_results) {
            if (r.entry && r.entry->surface == kata_part &&
                r.entry->pos == core::PartOfSpeech::Noun) {
              skip_sokuonbin = true;
              break;
            }
          }
        }
        if (skip_sokuonbin) {
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_SKIP] \"" << kata_part
                                << "\" is dict noun, skip katakana_sokuonbin\n";
          }
        } else {
          // Neutral cost — let bigram connections decide between verb+て and noun+って
          constexpr float kSokuonbinCost = 0.1F;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                << " katakana_sokuonbin lemma=" << base_form
                                << " cost=" << kSokuonbinCost << "\n";
          }
          candidates.push_back(makeVerbCandidate(
              onbin_surface, start_pos, kata_end + 1, kSokuonbinCost, base_form,
              dictionary::ConjugationType::GodanRa,
              true, CandidateOrigin::VerbKatakana, 0.9F, "katakana_sokuonbin",
              core::ExtendedPOS::VerbOnbinkei));
        }
      }
    }
  }

  // Generate katakana mizenkei (未然形) candidates for negative splitting
  // E.g., ググらない → ググら (mizenkei of ググる) + ない (auxiliary)
  //       バズらせる → バズら (mizenkei of バズる) + せる (auxiliary)
  // This allows MeCab-compatible splitting for katakana slang verbs
  {
    // Check if hiragana part starts with ら followed by ない/なく/なかっ/なけれ/せ/され
    if (hira_part.size() >= 6 &&  // ら(3 bytes) + な/せ(3 bytes) = 6 bytes min
        hira_part.compare(0, 3, "ら") == 0) {
      std::string_view second_char(hira_part.data() + 3, 3);
      bool is_negative = (second_char == "な");  // なx patterns
      bool is_causative = (second_char == "せ"); // せる pattern
      if (is_negative || is_causative) {
        // Found katakana + ら + な/せ pattern
        // Generate mizenkei stem candidate: カタカナ + ら
        std::string mizenkei_surface = extractSubstring(codepoints, start_pos, kata_end + 1);
        std::string kata_part = extractSubstring(codepoints, start_pos, kata_end);
        std::string base_form = kata_part + "る";  // Assume godan-ra (most common for slang)

        // Negative cost to beat unsplit forms (same as sokuonbin)
        constexpr float kMizenkeiCost = -0.5F;
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << mizenkei_surface
                              << " katakana_mizenkei lemma=" << base_form
                              << " cost=" << kMizenkeiCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            mizenkei_surface, start_pos, kata_end + 1, kMizenkeiCost, base_form,
            dictionary::ConjugationType::GodanRa,
            true, CandidateOrigin::VerbKatakana, 0.9F, "katakana_mizenkei",
            core::ExtendedPOS::VerbMizenkei));
      }
    }
  }

  // Generate katakana volitional (意志形) candidates for splitting
  // E.g., ググろう → ググろ (volitional stem of ググる) + う (auxiliary)
  // MeCab splits these as: ググろ + う
  {
    // Check if hiragana part starts with ろう
    if (hira_part.size() >= 6 &&  // ろ(3 bytes) + う(3 bytes) = 6 bytes
        hira_part.compare(0, 6, "ろう") == 0) {
      // Found katakana + ろう pattern
      // Generate volitional stem candidate: カタカナ + ろ
      std::string volitional_surface = extractSubstring(codepoints, start_pos, kata_end + 1);
      std::string kata_part = extractSubstring(codepoints, start_pos, kata_end);
      std::string base_form = kata_part + "る";  // Assume godan-ra

      // Negative cost to beat unsplit forms
      constexpr float kVolitionalCost = -0.5F;
      SUZUME_DEBUG_VERBOSE_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_CAND] " << volitional_surface
                            << " katakana_volitional lemma=" << base_form
                            << " cost=" << kVolitionalCost << "\n";
      }
      candidates.push_back(makeVerbCandidate(
          volitional_surface, start_pos, kata_end + 1, kVolitionalCost, base_form,
          dictionary::ConjugationType::GodanRa,
          true, CandidateOrigin::VerbKatakana, 0.9F, "katakana_volitional",
          core::ExtendedPOS::VerbMizenkei));
    }
  }

  // Sort by cost
  vh::sortCandidatesByCost(candidates);

  return candidates;
}

}  // namespace suzume::analysis
