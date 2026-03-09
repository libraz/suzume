/**
 * @file verb_candidates_kanji.cpp
 * @brief Kanji-based verb candidate generation (generateVerbCandidates)
 *
 * Handles verb candidate generation for kanji+hiragana patterns.
 * Split from verb_candidates.cpp for maintainability.
 */

#include "verb_candidates.h"

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
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

std::vector<UnknownCandidate> generateVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager,
    const VerbCandidateOptions& verb_opts) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (typically 1-2 characters for verbs)
  size_t kanji_end = vh::findCharRegionEnd(char_types, start_pos, 3,
                                            normalize::CharType::Kanji);

  if (kanji_end == start_pos) {
    return candidates;
  }

  // Look for hiragana after kanji
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Check if first hiragana is a particle that can NEVER be part of a verb
  // E.g., "領収書を" - を is a particle, not part of a verb
  // Note about が and に:
  // - が can be part of verbs: 上がる, 下がる, 受かる, etc.
  // - が can be mizenkei: 泳がれる (泳ぐ → 泳が + れる)
  // Check if the hiragana after kanji is a particle (not a verb conjugation)
  // e.g., 金がない → 金 + が + ない, not 金ぐ
  // Note about か: excluded - can be part of verb conjugation (書かない, 動かす)
  char32_t first_hiragana = codepoints[kanji_end];
  if (normalize::isNeverVerbStemAfterKanji(first_hiragana)) {
    // Exception 1: A-row hiragana followed by れべき may be mizenkei pattern
    // e.g., 泳がれべき = 泳が (mizenkei) + れべき (passive + classical obligation)
    // Exception 2: A-row hiragana followed by れ is godan passive renyokei
    // e.g., 言われ = 言わ (mizenkei) + れ (passive renyokei of 言われる)
    // Exception 3: が followed by る is godan-ra verb pattern
    // e.g., 上がる, 下がる, 受かる - these are common godan-ra verbs
    // For patterns like 金がない, the が should remain NOUN + PARTICLE + ADJ
    bool is_verb_pattern = false;
    if (grammar::isARowCodepoint(first_hiragana)) {
      size_t next_pos = kanji_end + 1;
      if (next_pos < codepoints.size()) {
        char32_t next_char = codepoints[next_pos];
        if (next_char == U'れ') {
          // A-row + れ pattern: could be passive verb stem (言われ, 書かれ, etc.)
          is_verb_pattern = true;
        } else if (first_hiragana == U'が') {
          // が + る/ら/り/っ pattern: could be godan-ra verb (上がる, 下がる, 受かる)
          // Also handle conjugations: がら(mizenkei), がり(renyokei), がっ(onbin)
          // が + せ/さ/ず: godan-ga verb mizenkei patterns
          // E.g., 脱がせる, 脱がさない, 脱がず
          // が+な: only allow if kanji+ぐ is a known godan-ga verb
          // E.g., 脱がない (脱ぐ exists) vs 金がない (金ぐ doesn't exist)
          if (next_char == U'る' || next_char == U'ら' ||
              next_char == U'り' || next_char == U'っ' ||
              next_char == U'れ' ||
              next_char == U'せ' ||
              next_char == U'さ' || next_char == U'ず') {
            is_verb_pattern = true;
          }
          // が+な: verify kanji+ぐ exists as godan-ga verb in dictionary
          if (!is_verb_pattern && next_char == U'な' && dict_manager != nullptr) {
            std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
            std::string gu_form = kanji_stem + "ぐ";
            if (vh::isVerbInDictionary(dict_manager, gu_form)) {
              is_verb_pattern = true;
            }
          }
        }
      }
    }
    if (!is_verb_pattern) {
      return candidates;  // Not a verb - these particles follow nouns
    }
  }

  // Find hiragana portion (max 12 for conjugation + aux)
  // Note: We no longer break at particle-like characters here.
  // The inflection module will determine if the full string is a valid
  // conjugated verb. This allows patterns like "飲みながら" (nomi-nagara)
  // where "が" is part of the auxiliary "ながら", not a standalone particle.
  size_t hiragana_end = vh::findCharRegionEnd(char_types, kanji_end, 12,
                                               normalize::CharType::Hiragana);

  // Need at least some hiragana for a conjugated verb
  if (hiragana_end <= kanji_end) {
    return candidates;
  }

  // Check for kanji verb + verb renyokei + すぎ pattern for MeCab compatibility
  // MeCab splits: 書きすぎる → 書き + すぎる, not 書きすぎる as single verb
  // Pattern: kanji + (き/ぎ/し/ち/に/び/み/り/い) + すぎ...
  std::string hira_part = extractSubstring(codepoints, kanji_end, hiragana_end);
  // C++17 compatible: check if hiragana contains "すぎ" (6 bytes)
  bool is_sugi_pattern = (hira_part.find("すぎ") != std::string::npos);

  // Generate verb renyokei candidates when followed by すぎ
  // E.g., 書きすぎた → 書き (renyokei of 書く) + すぎ + た (Godan)
  //       食べすぎた → 食べ (renyokei of 食べる) + すぎ + た (Ichidan)
  if (is_sugi_pattern && kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];

    // Pattern 1: Godan verb renyokei (kanji + I-row hiragana + すぎ)
    // き→GodanKa, ぎ→GodanGa, し→GodanSa, ち→GodanTa, に→GodanNa,
    // び→GodanBa, み→GodanMa, り→GodanRa
    if (grammar::isIRowCodepoint(first_hira)) {
      // Verify this is followed by すぎ
      if (kanji_end + 1 < codepoints.size() &&
          codepoints[kanji_end + 1] == U'す' &&
          kanji_end + 2 < codepoints.size() &&
          codepoints[kanji_end + 2] == U'ぎ') {
        // Determine verb type from I-row ending
        grammar::VerbType verb_type = grammar::verbTypeFromIRowCodepoint(first_hira);
        if (verb_type != grammar::VerbType::Unknown) {
          // Get base suffix (e.g., き → く for GodanKa)
          std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(first_hira);
          if (!base_suffix.empty()) {
            // Construct base form: kanji + base_suffix (e.g., 書 + く = 書く)
            std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
            std::string base_form = kanji_stem + std::string(base_suffix);

            // Verify the base form is a valid verb
            bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
            if (!is_valid_verb) {
              auto infl_result = inflection.getBest(base_form);
              is_valid_verb = infl_result.confidence > 0.5F &&
                              vh::isGodanVerbType(infl_result.verb_type);
            }

            if (is_valid_verb) {
              size_t renyokei_end = kanji_end + 1;
              std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);
              // Negative cost to beat compound NOUN path
              // Compound NOUNs like 書きすぎた get cost ~1.0, so we need much lower
              constexpr float kCost = candidate::verb_cost::kStrongBonus;
              SUZUME_DEBUG_VERBOSE_BLOCK {
                SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                    << " godan_renyokei_sugi lemma=" << base_form
                                    << " cost=" << kCost << "\n";
              }
              candidates.push_back(makeVerbCandidate(
                  surface, start_pos, renyokei_end, kCost, base_form,
                  grammar::verbTypeToConjType(verb_type),
                  true, CandidateOrigin::VerbKanji, 0.9F, "godan_renyokei_sugi",
                  core::ExtendedPOS::VerbRenyokei));
            }
          }
        }
      }
    }

    // Pattern 2: Ichidan verb renyokei (kanji + E-row hiragana + すぎ)
    // E.g., 食べすぎた → 食べ (renyokei of 食べる) + すぎ + た
    //       見せすぎる → 見せ (renyokei of 見せる) + すぎる
    if (grammar::isERowCodepoint(first_hira)) {
      // Verify this is followed by すぎ
      if (kanji_end + 1 < codepoints.size() &&
          codepoints[kanji_end + 1] == U'す' &&
          kanji_end + 2 < codepoints.size() &&
          codepoints[kanji_end + 2] == U'ぎ') {
        // Construct base form: kanji + first_hira + る (e.g., 食 + べ + る = 食べる)
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
        std::string ichidan_stem = kanji_stem + normalize::encodeUtf8(first_hira);
        std::string base_form = ichidan_stem + "る";

        // Verify the base form is a valid ichidan verb
        bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
        if (!is_valid_verb) {
          auto infl_result = inflection.getBest(base_form);
          is_valid_verb = infl_result.confidence > 0.5F &&
                          infl_result.verb_type == grammar::VerbType::Ichidan;
        }

        if (is_valid_verb) {
          size_t renyokei_end = kanji_end + 1;
          std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);
          // Negative cost to beat compound NOUN path
          constexpr float kCost = candidate::verb_cost::kStrongBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                << " ichidan_renyokei_sugi lemma=" << base_form
                                << " cost=" << kCost << "\n";
          }
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, renyokei_end, kCost, base_form,
              dictionary::ConjugationType::Ichidan,
              true, CandidateOrigin::VerbKanji, 0.9F, "ichidan_renyokei_sugi",
              core::ExtendedPOS::VerbRenyokei));
        }
      }
    }

    // Early return to skip generating full verb forms containing すぎ
    // The split path (renyokei + すぎ + aux) is preferred for MeCab compatibility
    return candidates;
  }

  // Godan mizenkei pattern: single-kanji + A-row + れ/せ (passive/causative)
  // E.g., 騙される → 騙さ (mizenkei of 騙す) + れる (passive)
  //       囲まれる → 囲ま (mizenkei of 囲む) + れる (passive)
  //       書かせる → 書か (mizenkei of 書く) + せる (causative)
  // Only single-kanji stems: multi-kanji + さ + れ is suru-verb (処理される)
  // The inflection module doesn't recognize kanji+A-row as godan mizenkei,
  // so we generate this candidate explicitly.
  if (kanji_end - start_pos == 1 &&
      kanji_end < hiragana_end &&
      grammar::isARowCodepoint(codepoints[kanji_end])) {
    char32_t a_row = codepoints[kanji_end];
    size_t after_a_pos = kanji_end + 1;
    if (after_a_pos < codepoints.size()) {
      char32_t after_a = codepoints[after_a_pos];
      // A-row + れ (passive) or A-row + せ (causative)
      if (after_a == U'れ' || after_a == U'せ') {
        grammar::VerbType verb_type =
            grammar::verbTypeFromARowCodepoint(a_row);
        std::string_view base_suffix =
            grammar::godanBaseSuffixFromARow(a_row);
        if (verb_type != grammar::VerbType::Unknown && !base_suffix.empty()) {
          std::string kanji_stem =
              extractSubstring(codepoints, start_pos, kanji_end);
          std::string base_form = kanji_stem + std::string(base_suffix);
          std::string surface =
              extractSubstring(codepoints, start_pos, kanji_end + 1);

          // Verify via inflection analysis of base form
          const auto& results = inflection.analyze(base_form);
          bool is_valid = false;
          for (const auto& cand : results) {
            if (cand.verb_type == verb_type && cand.confidence >= 0.4F) {
              is_valid = true;
              break;
            }
          }

          if (is_valid) {
            // Skip if kanji+A-row+る is a known godan-ra verb in dictionary
            // (potential form conflict). E.g., 泊まれる = potential of
            // 泊まる (godan-ra), not passive of 泊む (godan-ma).
            // 囲まれる = passive of 囲む is OK because 囲まる is not
            // in the dictionary.
            bool has_competing_ra_verb = false;
            if (after_a == U'れ') {
              std::string ra_form = surface + "る";
              has_competing_ra_verb =
                  vh::isVerbInDictionary(dict_manager, ra_form);
            }

            if (!has_competing_ra_verb) {
              constexpr float kCost = candidate::verb_cost::kWeakPenalty;
              SUZUME_DEBUG_LOG("[VERB_CAND] " << surface
                              << " godan_mizenkei_passive lemma=" << base_form
                              << " cost=" << kCost << "\n");
              candidates.push_back(makeVerbCandidate(
                  surface, start_pos, kanji_end + 1, kCost, base_form,
                  grammar::verbTypeToConjType(verb_type), true,
                  CandidateOrigin::VerbKanji, 0.8F, "godan_mizenkei_passive",
                  core::ExtendedPOS::VerbMizenkei));
            }
          }
        }
      }
    }
  }

  // Contracted sa-row mizenkei: kanji + しゃ + れ/せ/し
  // Colloquial contraction さ→しゃ in passive/causative/emphatic negation
  // E.g., 殺しゃれる → 殺しゃ (contracted mizenkei of 殺す) + れる (passive)
  //       話しゃれる → 話しゃ (contracted mizenkei of 話す) + れる (passive)
  //       出しゃしない → 出しゃ (contracted) + し + ない (emphatic neg)
  // Only single-kanji stems (same constraint as godan-sa mizenkei above)
  if (kanji_end - start_pos == 1 &&
      kanji_end + 1 < hiragana_end &&
      codepoints[kanji_end] == U'し' &&
      codepoints[kanji_end + 1] == U'ゃ') {
    size_t after_sha = kanji_end + 2;
    if (after_sha < codepoints.size()) {
      char32_t after = codepoints[after_sha];
      // しゃ + れ (passive) or しゃ + せ (causative) or しゃ + し (emphatic)
      if (after == U'れ' || after == U'せ' || after == U'し') {
        std::string kanji_stem =
            extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = kanji_stem + "す";
        std::string surface = kanji_stem + "しゃ";

        const auto& sa_results = inflection.analyze(base_form);
        bool is_valid_godan_sa = false;
        for (const auto& cand : sa_results) {
          if (cand.verb_type == grammar::VerbType::GodanSa &&
              cand.confidence >= 0.4F) {
            is_valid_godan_sa = true;
            break;
          }
        }

        if (is_valid_godan_sa) {
          constexpr float kCost = candidate::verb_cost::kWeakPenalty;
          SUZUME_DEBUG_LOG("[VERB_CAND] "
                          << surface
                          << " godan_sa_contracted_mizenkei lemma=" << base_form
                          << " cost=" << kCost << "\n");
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, kanji_end + 2, kCost, base_form,
              grammar::verbTypeToConjType(grammar::VerbType::GodanSa), false,
              CandidateOrigin::VerbKanji, 0.8F, "godan_sa_contracted_mizenkei",
              core::ExtendedPOS::VerbMizenkei));
        }
      }
    }
  }

  // Godan mizenkei pattern: kanji + A-row hiragana + ず (classical negative)
  // E.g., 抜かずに → 抜か (mizenkei of 抜く) + ず + に
  //       行かずに → 行か (mizenkei of 行く) + ず + に
  //       書かずに → 書か (mizenkei of 書く) + ず + に
  // The main loop skips single A-row hiragana as particle (か, etc.)
  // so we generate mizenkei candidates explicitly when followed by ず.
  if (kanji_end + 1 < hiragana_end && codepoints[kanji_end + 1] == U'ず') {
    char32_t first_hira = codepoints[kanji_end];
    if (grammar::isARowCodepoint(first_hira)) {
      grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(first_hira);
      if (verb_type != grammar::VerbType::Unknown) {
        std::string_view base_suffix = grammar::godanBaseSuffixFromARow(first_hira);
        if (!base_suffix.empty()) {
          std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
          std::string base_form = kanji_stem + std::string(base_suffix);
          std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);

          // Verify via dictionary or inflection analysis of conjugated form
          bool is_valid = vh::isVerbInDictionary(dict_manager, base_form);
          if (!is_valid) {
            // Analyze mizenkei+ない form (standard negative) for better confidence
            // Base form alone (e.g., 躊躇う) may not be recognized
            std::string neg_form = surface + "ない";
            const auto& infl_results = inflection.analyze(neg_form);
            for (const auto& cand : infl_results) {
              if (cand.base_form == base_form && cand.verb_type == verb_type &&
                  cand.confidence >= 0.3F) {
                is_valid = true;
                break;
              }
            }
          }

          if (is_valid) {
            // Skip if verb+ず or verb+ずに is a dictionary entry (e.g., 思わず=ADV)
            // In that case, the dictionary entry should win over the split path
            bool dict_has_zu_form = false;
            if (dict_manager != nullptr) {
              std::string zu_form = surface + "ず";
              std::string zuni_form = surface + "ずに";
              auto zu_results = dict_manager->lookup(zu_form, 0);
              for (const auto& r : zu_results) {
                if (r.entry != nullptr && r.entry->surface == zu_form) {
                  dict_has_zu_form = true;
                  break;
                }
              }
              if (!dict_has_zu_form) {
                auto zuni_results = dict_manager->lookup(zuni_form, 0);
                for (const auto& r : zuni_results) {
                  if (r.entry != nullptr && r.entry->surface == zuni_form) {
                    dict_has_zu_form = true;
                    break;
                  }
                }
              }
            }
            if (!dict_has_zu_form) {
              constexpr float kCost = candidate::verb_cost::kWeakPenalty;
              SUZUME_DEBUG_LOG("[VERB_CAND] " << surface
                              << " godan_mizenkei_zu lemma=" << base_form
                              << " cost=" << kCost << "\n");
              candidates.push_back(makeVerbCandidate(
                  surface, start_pos, kanji_end + 1, kCost, base_form,
                  grammar::verbTypeToConjType(verb_type),
                  true, CandidateOrigin::VerbKanji, 0.8F, "godan_mizenkei_zu",
                  core::ExtendedPOS::VerbMizenkei));
            }
          }
        }
      }
    }
  }

  // Try different stem lengths (kanji only, or kanji + 1 hiragana for ichidan)
  // This handles both godan (kanji stem) and ichidan (kanji + hiragana stem)
  for (size_t stem_end = kanji_end; stem_end <= kanji_end + 1 && stem_end < hiragana_end; ++stem_end) {
    // Try different ending lengths, starting from longest
    for (size_t end_pos = hiragana_end; end_pos > stem_end; --end_pos) {
      std::string surface = extractSubstring(codepoints, start_pos, end_pos);

      if (surface.empty()) {
        continue;
      }

      // Check for particle/copula patterns that should NOT be treated as verbs
      // Kanji + particle or copula (で, に, を, が, は, も, へ, と, や, か, の, etc.)
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, end_pos);
      if (normalize::isParticleOrCopula(hiragana_part)) {
        continue;  // Skip particle/copula patterns
      }

      // Skip patterns where hiragana part is a known suffix in dictionary
      // (e.g., たち, さん, ら, etc.) - let NOUN+suffix split win instead
      // For multi-kanji stems (2+ kanji), skip any suffix pattern
      // For single-kanji stems, only skip Suffix POS entries (さん, 様, etc.)
      // This allows verb renyokei like 立ち (立つ) while blocking 姉さん
      // Note: Only skip for OTHER (suffixes), not VERB (する is a verb, not suffix)
      // Exception: さ followed by れ/せ is godan-sa mizenkei + passive/causative,
      // not nominalization suffix (騙される, 話される, 殺させる)
      bool is_suffix_pattern = false;
      if (dict_manager != nullptr) {
        auto suffix_results = dict_manager->lookup(hiragana_part, 0);
        for (const auto& result : suffix_results) {
          if (result.entry != nullptr &&
              result.entry->surface == hiragana_part) {
            // For single-kanji stems, only skip if POS is Suffix (honorifics like さん)
            // For multi-kanji stems, skip any suffix pattern
            bool is_suffix_pos = (result.entry->pos == core::PartOfSpeech::Suffix);
            bool is_multi_kanji = (kanji_end - start_pos >= 2);
            if (is_suffix_pos ||
                (is_multi_kanji && (result.entry->extended_pos == core::ExtendedPOS::Suffix ||
                                    result.entry->pos == core::PartOfSpeech::Other))) {
              // Exception: さ + れ/せ is godan-sa mizenkei + passive/causative
              if (hiragana_part == "さ" && end_pos < codepoints.size()) {
                char32_t next_char = codepoints[end_pos];
                if (next_char == U'れ' || next_char == U'せ') {
                  break;  // Not a suffix - godan-sa verb pattern
                }
              }
              // This hiragana part is a registered suffix - skip verb candidate
              is_suffix_pattern = true;
              break;
            }
          }
        }
      }
      if (is_suffix_pattern) {
        continue;
      }

      // Skip patterns that contain ください (polite request auxiliary)
      // e.g., 待ちください → 待ち + ください, not 待ちく + ださい
      // This prevents false compound verb analysis like 待ちく (待つ+来る renyokei)
      if (hiragana_part.find("ください") != std::string::npos ||
          hiragana_part.find("くださ") != std::string::npos) {
        continue;  // Skip - let VERB + ください split win
      }

      // Skip patterns that extend past te-form boundary into auxiliaries
      // e.g., 履いてない → 履い + て + ない, not a single verb
      //        着ている → 着 + て + いる, not a single verb
      //        飲んでいた → 飲ん + で + いた, not a single verb
      // Detect: onbin ending (い/っ/ん) + て/で + auxiliary content (ない/いる/いた/ある/しまう etc.)
      {
        auto te_pos = hiragana_part.find("て");
        if (te_pos == std::string::npos) {
          te_pos = hiragana_part.find("で");
        }
        if (te_pos != std::string::npos && te_pos >= core::kJapaneseCharBytes) {
          // Check if there's auxiliary content after て/で
          std::string after_te = hiragana_part.substr(te_pos + core::kJapaneseCharBytes);
          if (!after_te.empty()) {
            // Check if char before て/で is onbin ending (い/っ/ん) or
            // godan-sa renyokei (し) — e.g., 過ごしてみた → 過ごし+て+み+た
            std::string_view before_te(hiragana_part.data() + te_pos - core::kJapaneseCharBytes,
                                       core::kJapaneseCharBytes);
            if (before_te == "い" || before_te == "っ" || before_te == "ん" ||
                before_te == "し") {
              continue;  // Skip - let verb + て + auxiliary split win
            }
          }
        }
      }

      // Skip patterns ending with く when followed by ださ/ださい (part of ください)
      // e.g., 待ちく when followed by ださい → should be 待ち + ください
      {
        size_t hira_size = hiragana_part.size();
        if (hira_size >= core::kJapaneseCharBytes) {
          std::string_view last_char_view(hiragana_part.data() + hira_size - core::kJapaneseCharBytes,
                                          core::kJapaneseCharBytes);
          if (last_char_view == "く" && end_pos < codepoints.size()) {
            // Check if followed by だ or ださ or ださい
            std::string remaining = extractSubstring(codepoints, end_pos, std::min(end_pos + 3, codepoints.size()));
            if (remaining.compare(0, 6, "ださ") == 0 || remaining.compare(0, 3, "だ") == 0) {
              continue;  // Skip - likely part of ください pattern
            }
          }
        }
      }

      // Skip patterns that end with particles (noun renyokei + particle)
      // e.g., 切りに (切り + に), 飲みに (飲み + に), 行きに (行き + に)
      // These are nominalized verb stems followed by particles, not verb forms
      size_t hp_size = hiragana_part.size();
      if (hp_size >= core::kTwoJapaneseCharBytes) {  // At least 2 hiragana
        // Get last hiragana character (particle candidate)
        char32_t last_char = codepoints[end_pos - 1];
        if (normalize::isParticleCodepoint(last_char)) {
          // Check if the preceding part could be a valid verb renyokei
          // Renyokei typically ends in い/り/き/ぎ/し/み/び/ち/に
          char32_t second_last_char = codepoints[end_pos - 2];
          if (second_last_char == U'い' || second_last_char == U'り' ||
              second_last_char == U'き' || second_last_char == U'ぎ' ||
              second_last_char == U'し' || second_last_char == U'み' ||
              second_last_char == U'び' || second_last_char == U'ち') {
            continue;  // Skip - likely nominalized noun + particle
          }
        }
      }

      // Check if this looks like a conjugated verb
      // Get all inflection candidates, not just the best one
      // This handles cases where the best candidate has wrong stem but a lower-ranked
      // candidate has the correct stem (e.g., 見なければ where 見なける wins over 見る)
      const auto& inflection_results = inflection.analyze(surface);
      std::string expected_stem = extractSubstring(codepoints, start_pos, stem_end);

      // Find a candidate with matching stem and sufficient confidence
      // Prefer dictionary-verified candidates when multiple have similar confidence
      // This handles ambiguous っ-onbin patterns like 待って (待つ/待る/待う)
      grammar::InflectionCandidate best;
      best.confidence = 0.0F;
      grammar::InflectionCandidate dict_verified_best;
      dict_verified_best.confidence = 0.0F;

      for (const auto& cand : inflection_results) {
        // Use lower threshold for ichidan verbs with i-row stems (感じる, 信じる)
        // These get ichidan_kanji_i_row_stem penalty which reduces confidence
        // But NOT for e-row stems (て/で), which are often te-form splits
        // Also NOT for single-kanji + い patterns (人い → 人 + いる, not a verb)
        bool is_i_row_ichidan = false;
        if (cand.verb_type == grammar::VerbType::Ichidan &&
            !cand.stem.empty() &&
            cand.stem.size() >= core::kJapaneseCharBytes) {
          std::string_view last_char(
              cand.stem.data() + cand.stem.size() - core::kJapaneseCharBytes,
              core::kJapaneseCharBytes);
          if (grammar::endsWithIRow(last_char)) {
            // Stem has at least 2 chars?
            if (cand.stem.size() >= 2 * core::kJapaneseCharBytes) {
              // Check if stem is single-kanji + い (e.g., 人い)
              // This pattern is almost always NOUN + いる, not a single verb
              // Valid ichidan stems are typically multi-char (感じ, 信じ, etc.)
              std::string kanji_part(cand.stem.data(),
                                     cand.stem.size() - core::kJapaneseCharBytes);
              // If kanji part is exactly 1 kanji (3 bytes), it's likely NOUN + いる
              bool is_single_kanji_i =
                  (kanji_part.size() == core::kJapaneseCharBytes &&
                   last_char == "い");
              // Multi-char kanji portion like 感じ is a valid ichidan stem
              is_i_row_ichidan = !is_single_kanji_i;
            } else {
              // Single char stem - not a valid i-row ichidan pattern
              is_i_row_ichidan = false;
            }
          }
        }
        float conf_threshold = is_i_row_ichidan
                                   ? verb_opts.confidence_ichidan_dict
                                   : verb_opts.confidence_standard;
        if (cand.stem == expected_stem &&
            cand.confidence > conf_threshold &&
            cand.verb_type != grammar::VerbType::IAdjective) {
          // Check if this candidate's base_form exists in dictionary
          // For っ-onbin patterns (GodanRa/Ta/Wa/Ka), use type-aware lookup to avoid
          // mismatches like 経る(GodanRa) matching 経る(Ichidan) when 経つ(GodanTa) is correct.
          // For other patterns (Suru verbs, Ichidan, etc.), use simple lookup.
          bool is_onbin_type = vh::isSokuonbinGodanType(cand.verb_type);
          bool in_dict = is_onbin_type
                             ? vh::isVerbInDictionaryWithType(dict_manager, cand.base_form,
                                                          cand.verb_type)
                             : vh::isVerbInDictionary(dict_manager, cand.base_form);

          if (in_dict) {
            // Prefer dictionary-verified candidates
            if (cand.confidence > dict_verified_best.confidence) {
              dict_verified_best = cand;
            }
          }
          if (cand.confidence > best.confidence) {
            best = cand;
          }
        }
      }

      // Use dictionary-verified candidate if available
      // Dictionary verification trumps confidence penalties from hiragana stems
      bool is_dict_verified = dict_verified_best.confidence > 0.0F;
      if (is_dict_verified) {
        best = dict_verified_best;
      }

      // Only proceed if we found a matching candidate
      // Use lower threshold for valid i-row ichidan stems (感じ, 信じ, etc.)
      // but not single-kanji + い patterns (人い → 人 + いる)
      bool proceed_is_i_row_ichidan = false;
      if (best.verb_type == grammar::VerbType::Ichidan &&
          !best.stem.empty() &&
          best.stem.size() >= 2 * core::kJapaneseCharBytes) {
        std::string_view last_char(
            best.stem.data() + best.stem.size() - core::kJapaneseCharBytes,
            core::kJapaneseCharBytes);
        if (grammar::endsWithIRow(last_char)) {
          std::string kanji_part(best.stem.data(),
                                 best.stem.size() - core::kJapaneseCharBytes);
          bool is_single_kanji_i =
              (kanji_part.size() == core::kJapaneseCharBytes &&
               last_char == "い");
          proceed_is_i_row_ichidan = !is_single_kanji_i;
        }
      }

      // Skip fake ichidan candidates with stem ending in さ (a-row)
      // These are typically suru-verb causative/passive patterns:
      //   勉強させられた → 勉強 + さ + せ + られ + た (NOT ichidan 勉強さる)
      // Valid ichidan stems end in e-row or i-row, not a-row
      if (best.verb_type == grammar::VerbType::Ichidan &&
          !best.stem.empty() &&
          best.stem.size() >= 2 * core::kJapaneseCharBytes) {
        std::string_view last_char(
            best.stem.data() + best.stem.size() - core::kJapaneseCharBytes,
            core::kJapaneseCharBytes);
        // さ is a-row hiragana (not valid for ichidan verb stems)
        if (last_char == "さ") {
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface
              << "\" stem ends with さ (suru-verb causative pattern)\n");
          continue;
        }
      }
      // Dictionary-verified candidates use lower threshold (0.3)
      // This allows hiragana verbs like いわれる (conf=0.33) to be recognized
      float proceed_threshold = is_dict_verified
                                    ? verb_opts.confidence_ichidan_dict
                                    : (proceed_is_i_row_ichidan
                                           ? verb_opts.confidence_ichidan_dict
                                           : verb_opts.confidence_standard);
      if (best.confidence > proceed_threshold) {
          // Reject Godan verbs with stems ending in e-row hiragana
          // E-row endings (え,け,せ,て,ね,へ,め,れ) are typically ichidan stems
          // E.g., "伝えいた" falsely matches as GodanKa "伝えく" but 伝える is ichidan
          // Exception: GodanRa (passive/causative) with "られ" suffix is valid
          // E.g., "定められた" has stem "定め" (ichidan) + passive suffix
          bool is_godan = vh::isGodanVerbType(best.verb_type);
          if (is_godan && stem_end > kanji_end && stem_end <= codepoints.size()) {
            // Check if the last character of the stem is e-row hiragana
            char32_t last_char = codepoints[stem_end - 1];
            if (grammar::isERowCodepoint(last_char)) {
              // Exception: GodanRa with passive/causative suffix (られ) is valid
              // This occurs with ichidan verb stem + passive auxiliary
              bool is_passive_pattern =
                  (best.verb_type == grammar::VerbType::GodanRa &&
                   utf8::contains(surface, "られ"));
              if (!is_passive_pattern) {
                continue;  // Skip - e-row stem is typically ichidan, not godan
              }
            }
          }

          // Skip Suru verb renyokei (し) if followed by te/ta form particles
          // e.g., "勉強して" should be parsed as single token, not "勉強し" + "て"
          if (best.verb_type == grammar::VerbType::Suru &&
              hiragana_part == "し" && end_pos < codepoints.size()) {
            char32_t next_char = codepoints[end_pos];
            if (next_char == U'て' || next_char == U'た' ||
                next_char == U'で' || next_char == U'だ') {
              continue;  // Skip - let the longer te-form candidate win
            }
          }

          // Skip GodanSa renyokei (漢字+し/漢字+とし etc.) when not in dictionary
          // e.g., "得し" misrecognized as GodanSa "得す" renyokei, but actually "得+し"
          // e.g., "証とし" misrecognized as GodanSa "証とす" renyokei, but actually "証+として"
          // MeCab splits as: 得(名詞) + し(する連用形) + た(過去)
          // Exception: Real GodanSa verbs like "愛す", "汚す" should not be skipped
          if (best.verb_type == grammar::VerbType::GodanSa &&
              utf8::endsWith(hiragana_part, "し") && kanji_end - start_pos <= 3) {
            // Check if the base form (stem+す) is a registered GodanSa verb
            // For single-char hiragana_part "し": base = kanji + す
            // For multi-char like "とし": base = kanji + と + す = 証とす
            std::string base_stem = extractSubstring(codepoints, start_pos, stem_end);
            std::string base_form = base_stem + "す";
            if (!vh::isVerbInDictionary(dict_manager, base_form)) {
              // Not a registered verb - likely サ変 or compound particle pattern
              continue;
            }
          }

          // Skip GodanMa renyokei (漢字+み) when base form is not in dictionary.
          // Renyokei み competes with auxiliary みたい — without dict verification,
          // 猫みたい (noun+aux) cannot be distinguished from 読みたい (verb+aux).
          // Requires all single-kanji GODAN_MA verbs to be enumerated in L2 dict.
          if (best.verb_type == grammar::VerbType::GodanMa &&
              hiragana_part == "み" && kanji_end - start_pos <= 3) {
            std::string base_form = extractSubstring(codepoints, start_pos, kanji_end) + "む";
            if (!vh::isVerbInDictionary(dict_manager, base_form)) {
              continue;
            }
          }

          // Skip verb + ます auxiliary patterns
          if (vh::shouldSkipMasuAuxPattern(surface, best.verb_type)) {
            continue;  // Skip - let the split (verb + dictionary aux) win
          }

          // Skip verb + そう auxiliary patterns
          if (vh::shouldSkipSouPattern(surface, best.verb_type)) {
            continue;  // Skip - let the split (verb renyokei + そう) win
          }

          // Skip verb + passive auxiliary patterns (れる, れた, etc.)
          // For auxiliary separation: 書かれる → 書か + れる
          if (vh::shouldSkipPassiveAuxPattern(surface, best.verb_type)) {
            continue;  // Skip - let the split (verb mizenkei + passive aux) win
          }

          // Skip verb + causative auxiliary patterns (せる, させる, etc.)
          // For auxiliary separation: 書かせる → 書か + せる
          if (vh::shouldSkipCausativeAuxPattern(surface, best.verb_type)) {
            continue;  // Skip - let the split (verb mizenkei + causative aux) win
          }

          // Skip suru-verb auxiliary patterns (して, した, している, etc.)
          // For MeCab-compatible split: 勉強して → 勉強 + して
          size_t kanji_count = kanji_end - start_pos;
          if (vh::shouldSkipSuruVerbAuxPattern(surface, kanji_count)) {
            continue;  // Skip - let the split (noun + suru-aux) win
          }

          // Skip て+subsidiary verb patterns (てもらう, てくれ, てあげ, etc.)
          // For MeCab-compatible split: 助けてもらう → 助け + て + もらう
          //                            食べてくれる → 食べ + て + くれる
          // These patterns should be split to allow subsidiary verb analysis
          if (surface.find("てもらう") != std::string::npos ||
              surface.find("てもらっ") != std::string::npos ||
              surface.find("てもらい") != std::string::npos ||
              surface.find("てもらえ") != std::string::npos ||
              surface.find("てくれる") != std::string::npos ||
              surface.find("てくれま") != std::string::npos ||
              surface.find("てくれた") != std::string::npos ||
              surface.find("てくれて") != std::string::npos ||
              surface.find("てくれな") != std::string::npos ||
              surface.find("てほしい") != std::string::npos ||
              surface.find("てあげる") != std::string::npos ||
              surface.find("てあげま") != std::string::npos ||
              surface.find("てあげた") != std::string::npos) {
            continue;  // Skip - let the split (verb te-form + subsidiary verb) win
          }

          // Skip volitional patterns ending with よう (e.g., 食べよう)
          // For MeCab-compatible split: 食べよう → 食べよ + う
          if (surface.size() >= 6 && surface.compare(surface.size() - 6, 6, "よう") == 0) {
            continue;  // Skip - let the split (verb + volitional aux) win
          }

          // Skip godan volitional patterns ending with おう (e.g., 行こう, 書こう)
          // For MeCab-compatible split: 行こう → 行こ + う
          // O-row + う patterns: こう, ごう, そう, とう, のう, ぼう, もう, ろう, おう
          if (surface.size() >= 6) {
            std::string last_two = surface.substr(surface.size() - 6);  // 2 hiragana = 6 bytes
            if (last_two == "こう" || last_two == "ごう" || last_two == "そう" ||
                last_two == "とう" || last_two == "のう" || last_two == "ぼう" ||
                last_two == "もう" || last_two == "ろう" || last_two == "おう") {
              continue;  // Skip - let the split (verb mizenkei + う) win
            }
          }

          // Skip sokuonbin + auxiliary verb patterns (買っとく, 行っちゃう)
          // For MeCab-compatible split: 買っとく → 買っ + とく
          // Check if suffix after っ is a registered auxiliary verb (とく, ちゃう, ちまう)
          bool skip_sokuonbin_aux = false;
          if (dict_manager && surface.size() >= 9) {  // っ(3) + 2char auxiliary minimum
            // Find っ position and check if suffix is auxiliary verb in dictionary
            auto surface_cps = normalize::utf8::decode(surface);
            for (size_t i = 1; i < surface_cps.size() && !skip_sokuonbin_aux; ++i) {
              if (surface_cps[i] == U'っ' && i + 1 < surface_cps.size()) {
                // Get suffix after っ
                std::vector<char32_t> suffix_cps(surface_cps.begin() + i + 1, surface_cps.end());
                std::string suffix = normalize::utf8::encode(suffix_cps);
                // Check if suffix is an auxiliary verb (AuxAspectOku: とく, AuxAspectShimau: ちゃう/ちまう)
                auto results = dict_manager->lookup(suffix, 0);
                for (const auto& r : results) {
                  if (r.entry && r.entry->surface == suffix &&
                      (r.entry->extended_pos == core::ExtendedPOS::AuxAspectOku ||
                       r.entry->extended_pos == core::ExtendedPOS::AuxAspectShimau)) {
                    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface
                        << "\" sokuonbin+aux (" << suffix << ")\n");
                    skip_sokuonbin_aux = true;
                    break;
                  }
                }
              }
            }
          }
          if (skip_sokuonbin_aux) {
            continue;  // Skip - let the split (verb sokuonbin + auxiliary) win
          }

          // Lower cost for higher confidence matches
          float base_cost = verb_opts.base_cost_standard + (1.0F - best.confidence) * verb_opts.confidence_cost_scale;
          // MeCab compatibility: Suru verbs should split as noun + する
          // Add penalty for unified suru-verb candidates to prefer split
          // e.g., 勉強する → 勉強 + する (split preferred)
          if (best.verb_type == grammar::VerbType::Suru &&
              best.stem.size() >= core::kTwoJapaneseCharBytes) {
            // Penalize unified suru-verb to prefer noun + する/される/させる split
            base_cost += 3.0F;
            SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +3.0 (suru_split_penalty)\n");
          }
          // Penalize ALL verb candidates with prefix-like kanji at start
          // e.g., 今何する/今何してる should split, not be single verb
          // This applies to all verb types (suru, ichidan, godan)
          if (best.stem.size() >= core::kTwoJapaneseCharBytes) {
            auto stem_codepoints = normalize::utf8::decode(best.stem);
            if (!stem_codepoints.empty() && isPrefixLikeKanji(stem_codepoints[0])) {
              // Heavy penalty to force split
              base_cost += 3.0F;
              SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +3.0 (prefix_kanji_penalty)\n");
            }
          }
          // Penalize verb candidates starting with interrogative kanji (何, 誰, 幾)
          // e.g., 何してる should split as 何|し|てる, not be single verb
          // Interrogatives are standalone words, not verb stems
          {
            auto stem_codepoints = normalize::utf8::decode(best.stem);
            if (!stem_codepoints.empty() && isInterrogativeKanji(stem_codepoints[0])) {
              // Heavy penalty to force split
              base_cost += 3.0F;
              SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +3.0 (interrogative_kanji_penalty)\n");
            }
          }
          // Skip patterns where removing first kanji leaves a valid dictionary verb
          // e.g., 本買った → 本 + 買った, where 買う is a dict verb
          // This handles particleless noun+verb patterns: 本買った, 服買った, 車買った
          if (dict_manager != nullptr && kanji_count == 2) {
            auto stem_cps = normalize::utf8::decode(best.stem);
            if (stem_cps.size() == 2) {
              // Get second kanji as potential verb stem
              std::string remainder_stem = normalize::utf8::encode({stem_cps[1]});
              // Check if remainder + verb ending is a dictionary verb
              std::string_view conj_suffix;
              switch (best.verb_type) {
                case grammar::VerbType::GodanKa: conj_suffix = "く"; break;
                case grammar::VerbType::GodanGa: conj_suffix = "ぐ"; break;
                case grammar::VerbType::GodanSa: conj_suffix = "す"; break;
                case grammar::VerbType::GodanTa: conj_suffix = "つ"; break;
                case grammar::VerbType::GodanNa: conj_suffix = "ぬ"; break;
                case grammar::VerbType::GodanBa: conj_suffix = "ぶ"; break;
                case grammar::VerbType::GodanMa: conj_suffix = "む"; break;
                case grammar::VerbType::GodanRa: conj_suffix = "る"; break;
                case grammar::VerbType::GodanWa: conj_suffix = "う"; break;
                case grammar::VerbType::Ichidan: conj_suffix = "る"; break;
                default: conj_suffix = ""; break;
              }
              if (!conj_suffix.empty()) {
                std::string remainder_base = remainder_stem + std::string(conj_suffix);
                if (vh::isVerbInDictionary(dict_manager, remainder_base)) {
                  // Skip this candidate - prefer noun + verb split
                  SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface
                                            << "\" remainder \"" << remainder_base << "\" is dict verb\n");
                  continue;
                }
              }
            }
          }
          // Penalize single-kanji + いる verb candidates (both godan-ra and ichidan)
          // e.g., 人いる should split as 人 + いる (noun + verb), not be verb
          // Most single kanji + いる patterns are NOUN + existence verb いる
          // Valid single-kanji verbs: 入る, 走る, 居る (いる), 見る, etc.
          // These should be in dictionary, so dictionary bonus will override
          {
            auto surface_cps = normalize::utf8::decode(surface);
            // Check if pattern is: 1 kanji + いる
            if (surface_cps.size() == 3 &&
                normalize::isKanjiCodepoint(surface_cps[0]) &&
                surface_cps[1] == U'い' && surface_cps[2] == U'る') {
              // Single kanji + いる pattern - penalize to prefer NOUN + いる split
              base_cost += 2.5F;
              SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +2.5 (single_kanji_iru_penalty)\n");
            }
          }
          // Check if base form exists in dictionary - significant bonus for known verbs
          // This helps 行われた (base=行う) beat 行(suffix)+われた split
          // Skip compound adjective patterns (verb renyoukei + にくい/やすい/がたい)
          // Skip suru-verbs - they should split as noun + する for MeCab compatibility
          bool is_comp_adj = vh::isCompoundAdjectivePattern(surface);
          bool in_dict = vh::isVerbInDictionary(dict_manager, best.base_form);
          bool is_suru = (best.verb_type == grammar::VerbType::Suru);
          if (!is_comp_adj && in_dict && !is_suru) {
            // Found in dictionary - give strong bonus (not for suru-verbs)
            base_cost = verb_opts.base_cost_verified +
                        (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
            // Godan-ra renyokei ambiguity: 降り can be from 降る(godan-ra) or
            // 降りる(ichidan). When ichidan form exists in dict, penalize godan-ra
            // so the more specific ichidan interpretation wins.
            if (best.verb_type == grammar::VerbType::GodanRa &&
                utf8::endsWith(surface, "り")) {
              std::string ichidan_base = surface + "る";
              if (vh::isVerbInDictionary(dict_manager, ichidan_base)) {
                base_cost += 1.0F;
                SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface
                                          << "\" +1.0 (godan_ra_ichidan_ambiguity, "
                                          << ichidan_base << " in dict)\n");
              }
            }
          }
          // Penalty for compound adjective patterns (verb renyokei + やすい/にくい/がたい)
          // MeCab splits these: 使いにくい → 使い + にくい
          if (is_comp_adj) {
            base_cost += 2.0F;  // Strong penalty to force split
            SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +2.0 (compound_adj_penalty)\n");
          }
          // Penalty for verb candidates containing みたい suffix
          // みたい is a na-adjective (like ~, seems ~), not a verb suffix
          // E.g., 猫みたい should be 猫 + みたい, not VERB 猫む
          if (utf8::contains(surface, "みたい")) {
            base_cost += verb_opts.penalty_single_char;
          }
          // Penalty for verb candidates ending with auxiliary たい/たく/たかっ
          // MeCab splits verb + auxiliary たい (desiderative)
          // E.g., 戦いたい = 戦い + たい, not single VERB
          if (utf8::endsWith(surface, "たい") || utf8::endsWith(surface, "たく") ||
              utf8::endsWith(surface, "たかっ")) {
            base_cost += bigram_cost::kRare;  // Penalize to favor split path
          }
          // Penalty for verb candidates containing causative auxiliary chains
          // MeCab splits: 欠かせない → 欠か+せ+ない, 食べさせた → 食べ+させ+た
          if (vh::containsCausativeAuxPattern(surface)) {
            base_cost += bigram_cost::kStrong;  // Penalize to favor split path
          }
          // Penalty for verb candidates ending with auxiliary まい (negative volitional)
          // MeCab splits verb + auxiliary まい
          // E.g., 出来まい = 出来 + まい, 行くまい = 行く + まい
          if (utf8::endsWith(surface, "まい")) {
            base_cost += bigram_cost::kStrong;  // Penalize to favor split path
          }
          // Penalty for verb candidates ending with らしい (conjecture auxiliary)
          // MeCab splits verb/adj + らしい
          // E.g., 帰りたいらしい = 帰り + たい + らしい
          if (utf8::endsWith(surface, "らしい") || utf8::endsWith(surface, "らしく") ||
              utf8::endsWith(surface, "らしかっ")) {
            base_cost += bigram_cost::kStrong;  // Penalize to favor split path
          }
          // Penalty for verb candidates ending with passive+te form (〜まれて/〜られて)
          // MeCab splits compound verb passive+te: 読み込まれて → 読み込ま|れ|て
          // E.g., 読み込まれていない = 読み込ま + れ + て + い + ない
          if (utf8::endsWith(surface, "まれて") || utf8::endsWith(surface, "まれた") ||
              utf8::endsWith(surface, "られて") || utf8::endsWith(surface, "られた")) {
            base_cost += bigram_cost::kVeryRare + bigram_cost::kNegligible;  // 2.0F
          }
          // Penalty for verb candidates containing て+auxiliary verb chains
          // MeCab splits: 付いてくる → 付い+て+くる, 集まってくる → 集まっ+て+くる
          // These are syntactic constructions (V-te + auxiliary), not single verb forms
          if (vh::containsTeFormAuxPattern(surface)) {
            base_cost += bigram_cost::kStrong;  // Penalize to favor split path
          }
          // Set has_suffix to skip exceeds_dict_length penalty in tokenizer.cpp
          // This applies when:
          // 1. Base form exists in dictionary as verb (in_dict)
          // 2. OR: Ichidan verb with valid i-row stem (感じる, not 人いる)
          //    that passes confidence threshold
          bool is_ichidan = (best.verb_type == grammar::VerbType::Ichidan);
          bool has_valid_ichidan_stem = false;
          if (is_ichidan && !best.stem.empty() &&
              best.stem.size() >= 2 * core::kJapaneseCharBytes) {
            // Check if stem ends with i-row hiragana (not e-row)
            // E-row endings are often te-form (見て) or copula (嫌で), not ichidan stems
            // Also exclude single-kanji + い patterns (人い → 人 + いる)
            std::string_view last_char(
                best.stem.data() + best.stem.size() - core::kJapaneseCharBytes,
                core::kJapaneseCharBytes);
            if (grammar::endsWithIRow(last_char)) {
              std::string kanji_part(best.stem.data(),
                                     best.stem.size() - core::kJapaneseCharBytes);
              bool is_single_kanji_i =
                  (kanji_part.size() == core::kJapaneseCharBytes &&
                   last_char == "い");
              has_valid_ichidan_stem = !is_single_kanji_i;
            }
          }
          bool recognized_ichidan = is_ichidan && has_valid_ichidan_stem &&
                                    best.confidence > verb_opts.confidence_ichidan_dict;
          // Godan verbs with single-kanji stem + high confidence are also
          // recognized (残る, 立つ, 打つ, etc.)
          bool recognized_godan = !is_ichidan && !in_dict &&
              !best.stem.empty() &&
              best.stem.size() == core::kJapaneseCharBytes &&
              best.confidence >= verb_opts.confidence_ichidan_dict;
          bool has_suffix = in_dict || recognized_ichidan || recognized_godan;
          // Determine extended_pos based on verb type and surface ending
          // Godan-wa verbs ending in い are renyokei (戦い), not onbinkei
          // Godan-ka/ga verbs ending in い are onbinkei (書い, 泳い)
          core::ExtendedPOS verb_epos = core::ExtendedPOS::Unknown;  // Auto-detect
          if (utf8::endsWith(surface, "い")) {
            if (best.verb_type == grammar::VerbType::GodanWa) {
              verb_epos = core::ExtendedPOS::VerbRenyokei;
            } else if (best.verb_type == grammar::VerbType::GodanKa ||
                       best.verb_type == grammar::VerbType::GodanGa) {
              verb_epos = core::ExtendedPOS::VerbOnbinkei;
            }
          }
          // Skip if surface is registered as NOUN in dictionary
          // This prevents nominalized verb forms (思い, 間違い, 戦い, 願い) from
          // being tokenized as VERB when they are explicitly registered as nouns
          // This check applies to godan-wa renyokei forms (ending in い)
          bool skip_godan_wa_renyokei = false;
          if (verb_epos == core::ExtendedPOS::VerbRenyokei && dict_manager != nullptr) {
            auto results = dict_manager->lookup(surface, 0);
            for (const auto& result : results) {
              if (result.entry != nullptr &&
                  result.entry->pos == core::PartOfSpeech::Noun) {
                skip_godan_wa_renyokei = true;
                SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" is dict NOUN, skipping godan-wa renyokei\n");
                break;
              }
            }
          }
          if (skip_godan_wa_renyokei) {
            continue;  // Skip this candidate, use dictionary NOUN instead
          }
          // Skip ichidan ta-form if stem is registered as NOUN in dictionary
          // e.g., 感じた → stem 感じ is dict NOUN, so skip (prefer 感じ(NOUN) + た(AUX))
          // This prevents nominalized verb renyokei forms from appearing as conjugated verbs
          bool skip_ichidan_ta = false;
          if (best.verb_type == grammar::VerbType::Ichidan && dict_manager != nullptr &&
              !best.stem.empty()) {
            // The stem for ichidan ta-form is the renyokei (e.g., 感じ for 感じた)
            auto stem_results = dict_manager->lookup(best.stem, 0);
            for (const auto& result : stem_results) {
              if (result.entry != nullptr &&
                  result.entry->pos == core::PartOfSpeech::Noun) {
                skip_ichidan_ta = true;
                SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" stem \"" << best.stem << "\" is dict NOUN, skipping ichidan ta-form\n");
                break;
              }
            }
          }
          if (skip_ichidan_ta) {
            continue;  // Skip this candidate, prefer NOUN + た split
          }
          // Skip if surface is already a registered VERB in dictionary
          // The dict entry has correct lemma; this unknown candidate would have wrong lemma
          // E.g., 下さい is dict verb (lemma=下さる), skip godan-wa candidate (lemma=下さう)
          if (!in_dict && vh::isVerbInDictionary(dict_manager, surface)) {
            SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" is dict VERB, skipping unknown candidate\n");
            continue;
          }
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                << " base=" << best.base_form
                                << " cost=" << base_cost
                                << " in_dict=" << in_dict
                                << " has_suffix=" << has_suffix << "\n";
          }
          // Don't set lemma here - let lemmatizer derive it with dictionary verification
          // The lemmatizer will use stem-matching logic to pick the correct base form
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, end_pos, base_cost, "",
              grammar::verbTypeToConjType(best.verb_type),
              has_suffix, CandidateOrigin::VerbKanji, best.confidence,
              grammar::verbTypeToString(best.verb_type).data(),
              verb_epos));
          // Don't break - try other stem lengths too
        }
    }
  }

  // Try Ichidan renyokei pattern: kanji + e-row/i-row hiragana
  // 下一段 (shimo-ichidan): e-row ending (食べ, 見せ, 教え)
  // 上一段 (kami-ichidan): i-row ending (感じ, 見, 居)
  // These are standalone verb forms that connect to ます, ましょう, etc.
  // The stem IS the entire surface (no conjugation suffix)
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // E-row hiragana: え, け, せ, て, ね, へ, め, れ, げ, ぜ, で, べ, ぺ
    // I-row hiragana: い, き, し, ち, に, ひ, み, り, ぎ, じ, ぢ, び, ぴ
    if (grammar::isERowCodepoint(first_hira) || grammar::isIRowCodepoint(first_hira)) {
      // Skip hiragana commonly used as particles after single kanji
      // で (te-form/particle), に (particle), へ (particle) are rarely Ichidan stem endings
      // These almost always represent kanji + particle (雨で→雨+で, 本に→本+に)
      // Also skip い (i) - this is almost always an i-adjective suffix (面白い, 高い)
      // not an ichidan verb renyoukei
      bool is_common_particle = (first_hira == U'で' || first_hira == U'に' || first_hira == U'へ');
      bool is_i_adjective_suffix = (first_hira == U'い');
      bool is_single_kanji = (kanji_end == start_pos + 1);
      // Skip kuru irregular verb: 来 + て/た should not be treated as ichidan
      // 来る is kuru irregular, not ichidan (来て should have lemma 来る, not 来てる)
      bool is_kuru_verb = is_single_kanji && codepoints[start_pos] == U'来';
      if ((is_common_particle && is_single_kanji) || is_i_adjective_suffix || is_kuru_verb) {
        // Skip this pattern - almost certainly noun + particle, i-adjective, or kuru verb
      } else {
        // Surface is kanji + first e/i-row hiragana only (e.g., 食べ from 食べます, 感じ from 感じる)
        size_t renyokei_end = kanji_end + 1;
        std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);
        // Get all inflection candidates, not just the best
        // This is important for ambiguous cases like 入れ (godan 入る imperative vs ichidan 入れる renyoukei)
        const auto& all_cands = inflection.analyze(surface);
        // Find the best Ichidan, Suru, and Godan candidates
        grammar::InflectionCandidate ichidan_cand;
        grammar::InflectionCandidate suru_cand;
        grammar::InflectionCandidate godan_cand;
        ichidan_cand.confidence = 0.0F;
        suru_cand.confidence = 0.0F;
        godan_cand.confidence = 0.0F;
        for (const auto& cand : all_cands) {
          if (cand.verb_type == grammar::VerbType::Ichidan && cand.confidence > ichidan_cand.confidence) {
            ichidan_cand = cand;
          }
          if (cand.verb_type == grammar::VerbType::Suru && cand.confidence > suru_cand.confidence) {
            suru_cand = cand;
          }
          if (vh::isGodanVerbType(cand.verb_type) && cand.confidence > godan_cand.confidence) {
            godan_cand = cand;
          }
        }
        // Skip if there's a suru-verb or godan-verb candidate with higher confidence
        // e.g., 勉強し has suru conf=0.82 vs ichidan conf=0.3 - prefer suru
        // e.g., 走り has godan conf=0.61 vs ichidan conf=0.3 - prefer godan
        bool prefer_suru = (suru_cand.confidence > ichidan_cand.confidence);
        bool prefer_godan = (godan_cand.confidence > ichidan_cand.confidence);
        // Use different thresholds for e-row vs i-row patterns:
        // - I-row (じ, み, etc.): lower threshold (0.28) - these are distinctively verb stems
        //   and get penalized by ichidan_kanji_i_row_stem, so need lower threshold
        // - E-row (べ, れ, etc.): use 0.28 threshold to catch renyoukei like 入れ (conf=0.3)
        //   while avoiding too many false positives
        bool is_i_row = grammar::isIRowCodepoint(first_hira);
        float conf_threshold = is_i_row ? verb_opts.confidence_ichidan_dict : verb_opts.confidence_ichidan_dict;
        // Skip if surface is registered as NOUN in dictionary
        // This prevents nominalized verb forms (売り上げ, 楽しみ, 晴れ) from being tokenized as VERB
        // when they are explicitly registered as nouns
        bool surface_is_dict_noun = false;
        if (dict_manager != nullptr) {
          auto results = dict_manager->lookup(surface, 0);
          for (const auto& result : results) {
            if (result.entry != nullptr &&
                result.entry->pos == core::PartOfSpeech::Noun) {
              surface_is_dict_noun = true;
              SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" is dict NOUN, skipping ichidan_renyokei\n");
              break;
            }
          }
        }
        // Skip if splitting at a kanji boundary yields a known dictionary verb
        // E.g., 血浴び → 血 + 浴び(る) — 浴びる is a dict verb, so 血浴びる is not a real verb
        bool suffix_is_dict_verb = false;
        if (dict_manager != nullptr && kanji_end > start_pos + 1) {
          for (size_t split = start_pos + 1; split < kanji_end; ++split) {
            std::string remainder = extractSubstring(codepoints, split, renyokei_end);
            std::string remainder_base = remainder + "\xe3\x82\x8b";  // + る
            if (vh::isVerbInDictionary(dict_manager, remainder_base)) {
              suffix_is_dict_verb = true;
              SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" suffix \"" << remainder_base
                              << "\" is dict verb, skipping ichidan_renyokei\n");
              break;
            }
          }
        }
        if (!prefer_suru && !prefer_godan && ichidan_cand.confidence > conf_threshold && !surface_is_dict_noun && !suffix_is_dict_verb) {
          // Negative cost to strongly favor split over combined analysis
          // Combined forms get optimal_length bonus (-0.5), so we need to be lower
          float base_cost = verb_opts.bonus_ichidan + (1.0F - ichidan_cand.confidence) * verb_opts.confidence_cost_scale_small;
          // Set has_suffix=true to skip exceeds_dict_length penalty for MeCab compatibility
          // Ichidan renyokei stems are valid morphological units (論じ, 信じ, etc.)
          // Set lemma to the base form (e.g., 入れ → 入れる, 論じ → 論じる)
          // This is critical for correct lemmatization when the surface is ambiguous
          // (e.g., 入れ could be godan 入る imperative or ichidan 入れる renyoukei)
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, renyokei_end, base_cost, ichidan_cand.base_form,
              grammar::verbTypeToConjType(ichidan_cand.verb_type),
              true, CandidateOrigin::VerbKanji, ichidan_cand.confidence, "ichidan_renyokei"));
        }
      }
    }
  }

  // Try Godan-Sa renyokei stem pattern: kanji + hiragana ending in し
  // E.g., 過ごし (過ごす), 話し (話す), 取り消し (取り消す)
  // These are needed when the verb is not in the dictionary, to enable
  // correct splitting at て-form boundaries (過ごし+て+み+たい)
  // Check positions kanji_end+1 through kanji_end+3 for し-ending godan-sa renyokei
  if (hiragana_end > kanji_end) {
    size_t max_renyokei_end = std::min(kanji_end + 4, hiragana_end + 1);
    for (size_t renyokei_end = kanji_end + 1; renyokei_end <= max_renyokei_end && renyokei_end <= codepoints.size(); ++renyokei_end) {
      // Must end in し (godan-sa renyokei marker)
      if (codepoints[renyokei_end - 1] != U'し') continue;

      std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);
      const auto& all_cands = inflection.analyze(surface);

      // Find best godan-sa candidate
      grammar::InflectionCandidate best_sa;
      best_sa.confidence = 0.0F;
      for (const auto& cand : all_cands) {
        if (cand.verb_type == grammar::VerbType::GodanSa && cand.confidence > best_sa.confidence) {
          best_sa = cand;
        }
      }

      if (best_sa.confidence <= 0.5F) continue;

      // Skip if surface is a dictionary NOUN
      bool surface_is_dict_noun = false;
      if (dict_manager != nullptr) {
        auto results = dict_manager->lookup(surface, 0);
        for (const auto& result : results) {
          if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Noun) {
            surface_is_dict_noun = true;
            break;
          }
        }
      }
      if (surface_is_dict_noun) continue;

      // For short godan-sa patterns, require dict verification to avoid
      // false positives like 悲し (not a real verb 悲す) or 春らし (not 春らす).
      // Multi-kanji verbs (過ごし, 見逃し) are more likely real verbs.
      size_t kanji_chars = kanji_end - start_pos;  // actual kanji count
      if (kanji_chars <= 1 && dict_manager != nullptr) {
        if (!vh::isVerbInDictionary(dict_manager, best_sa.base_form)) {
          SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface
                          << "\" godan_sa single kanji, base \"" << best_sa.base_form
                          << "\" not in dict\n");
          continue;
        }
      }

      float base_cost = verb_opts.bonus_ichidan + (1.0F - best_sa.confidence) * verb_opts.confidence_cost_scale_small;
      SUZUME_DEBUG_VERBOSE_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                            << " godan_sa_renyokei lemma=" << best_sa.base_form
                            << " conf=" << best_sa.confidence
                            << " cost=" << base_cost << "\n";
      }
      candidates.push_back(makeVerbCandidate(
          surface, start_pos, renyokei_end, base_cost, best_sa.base_form,
          grammar::verbTypeToConjType(best_sa.verb_type),
          true, CandidateOrigin::VerbKanji, best_sa.confidence, "godan_sa_renyokei"));
    }
  }

  // Try Ichidan verb kateikei (conditional) stem pattern: renyokei + れ + ば
  // Ichidan conditional is formed as: stem + れ + ば
  // E.g., 食べる → 食べれば, 滅びる → 滅びれば
  // MeCab splits as: 食べれ(動詞,仮定形) + ば(接続助詞)
  // Generate kateikei stem candidate (renyokei + れ) when followed by ば
  // Check pattern: (kanji + e/i-row hiragana) + れ + ば
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // Check if first hiragana is e-row or i-row (ichidan renyokei ending)
    if (grammar::isERowCodepoint(first_hira) || grammar::isIRowCodepoint(first_hira)) {
      size_t renyokei_end = kanji_end + 1;  // kanji + e/i-row
      // Check for れ + ば pattern after renyokei
      if (renyokei_end + 1 < codepoints.size() &&
          codepoints[renyokei_end] == U'れ' &&
          codepoints[renyokei_end + 1] == U'ば') {
        // E.g., 食べ + れ + ば → 食べれ is kateikei
        size_t kateikei_end = renyokei_end + 1;  // renyokei + れ
        std::string surface = extractSubstring(codepoints, start_pos, kateikei_end);
        std::string renyokei_surface = extractSubstring(codepoints, start_pos, renyokei_end);
        std::string base_form = renyokei_surface + "る";  // 食べ + る = 食べる

        // Verify using inflection analysis on the kateikei form
        const auto& all_candidates = inflection.analyze(surface);
        float ichidan_confidence = vh::getIchidanConfidence(all_candidates, 0.3F);

        if (ichidan_confidence >= 0.3F) {
          // Negative cost to beat the split path 語幹+れ(受身)+ば
          constexpr float kKateikeiCost = candidate::verb_cost::kStrongBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                << " ichidan_kateikei lemma=" << base_form
                                << " conf=" << ichidan_confidence
                                << " cost=" << kKateikeiCost << "\n";
          }
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, kateikei_end, kKateikeiCost, base_form,
              dictionary::ConjugationType::Ichidan,
              true, CandidateOrigin::VerbKanji, ichidan_confidence, "ichidan_kateikei",
              core::ExtendedPOS::VerbKateikei));
        }
      }

      // Try Ichidan verb volitional stem pattern: renyokei + よ + う
      // Ichidan volitional is formed as: stem + よう
      // MeCab splits as: stem+よ (動詞,未然ウ接続) + う (助動詞)
      // E.g., 食べる → 食べよう → 食べよ + う
      //       始める → 始めよう → 始めよ + う
      // Generate volitional stem candidate (renyokei + よ) when followed by う
      // Check pattern: renyokei + よ + う
      if (renyokei_end + 1 < codepoints.size() &&
          codepoints[renyokei_end] == U'よ' &&
          codepoints[renyokei_end + 1] == U'う') {
        // Skip suru-verb pattern: 漢字 + し + よう
        // Suru-verbs (勉強しよう, 説明しよう) should be split as: 漢字|しよ|う
        // Check if renyokei ends with し preceded by kanji
        bool is_suru_pattern = false;
        if (renyokei_end > start_pos &&
            codepoints[renyokei_end - 1] == U'し' &&
            renyokei_end - 1 > start_pos) {
          // Check if there's at least one kanji before し
          bool has_kanji_before = false;
          for (size_t i = start_pos; i < renyokei_end - 1; ++i) {
            if (normalize::isKanjiCodepoint(codepoints[i])) {
              has_kanji_before = true;
              break;
            }
          }
          is_suru_pattern = has_kanji_before;
        }

        if (!is_suru_pattern) {
          // E.g., 食べ + よ + う → 食べよ is volitional stem
          size_t volitional_end = renyokei_end + 1;  // renyokei + よ
          std::string surface = extractSubstring(codepoints, start_pos, volitional_end);
          std::string renyokei_surface =
              extractSubstring(codepoints, start_pos, renyokei_end);
          std::string base_form = renyokei_surface + "る";  // 食べ + る = 食べる

          // Check if renyokei looks like an adjective (kanji+い pattern)
          // E.g., 良い, 高い, 赤い - these are adjectives, not ichidan verb stems
          // Require higher confidence to avoid false volitional candidates
          // like 良いよ(う) being parsed as volitional of non-existent 良いる
          bool could_be_adjective = false;
          if (renyokei_end > start_pos + 1 &&
              codepoints[renyokei_end - 1] == U'い') {
            // Check if chars before い are all kanji
            bool all_kanji_before_i = true;
            for (size_t k = start_pos; k < renyokei_end - 1; ++k) {
              if (!normalize::isKanjiCodepoint(codepoints[k])) {
                all_kanji_before_i = false;
                break;
              }
            }
            could_be_adjective = all_kanji_before_i;
          }

          // Verify using inflection analysis
          const auto& all_candidates = inflection.analyze(renyokei_surface + "よう");
          float min_confidence = could_be_adjective ? 0.5F : 0.3F;
          float ichidan_confidence = vh::getIchidanConfidence(all_candidates, min_confidence);

          if (ichidan_confidence >= min_confidence) {
            // Negative cost to beat the split path 語幹+よう
            constexpr float kVolitionalCost = candidate::verb_cost::kStrongBonus;
            SUZUME_DEBUG_VERBOSE_BLOCK {
              SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                  << " ichidan_volitional lemma=" << base_form
                                  << " conf=" << ichidan_confidence
                                  << " cost=" << kVolitionalCost << "\n";
            }
            candidates.push_back(makeVerbCandidate(
                surface, start_pos, volitional_end, kVolitionalCost, base_form,
                dictionary::ConjugationType::Ichidan,
                true, CandidateOrigin::VerbKanji, ichidan_confidence, "ichidan_volitional",
                core::ExtendedPOS::VerbMizenkei));
          }
        }
      }
    }
  }

  // Try Causative verb renyokei pattern: kanji + ら + せ
  // Causative verbs from Godan verbs follow this pattern:
  //   知る → 知らせる (causative, Ichidan verb)
  //   乗る → 乗らせる (causative, Ichidan verb)
  //   終わる → 終わらせる (causative, Ichidan verb)
  // The renyokei of these causative verbs ends with せ (e-row):
  //   知らせ (renyokei of 知らせる), connects to ます, られる, て, た, etc.
  // Pattern: kanji + ら + せ (followed by られ for causative-passive)
  if (kanji_end + 2 <= hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    char32_t second_hira = codepoints[kanji_end + 1];
    // ら + せ pattern (causative renyokei)
    if (first_hira == U'ら' && second_hira == U'せ') {
      // Generate causative renyokei when followed by valid ichidan verb endings
      // or causative-passive (られ). This covers:
      //   眠らせた (past), 眠らせて (te-form), 眠らせない (negative),
      //   眠らせます (polite), 眠らせられ (passive)
      bool followed_by_valid = false;
      if (kanji_end + 2 < codepoints.size()) {
        char32_t next_cp = codepoints[kanji_end + 2];
        followed_by_valid = (next_cp == U'ら' || next_cp == U'た' ||
                             next_cp == U'て' || next_cp == U'な' ||
                             next_cp == U'ま' || next_cp == U'ず' ||
                             next_cp == U'ば');
      }
      // Also allow at end of input (bare renyokei: 眠らせ)
      if (kanji_end + 2 >= codepoints.size()) {
        followed_by_valid = true;
      }
      if (followed_by_valid) {
        size_t renyokei_end = kanji_end + 2;  // kanji + ら + せ
        std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);

        // The causative base form is surface + る (e.g., 知らせ → 知らせる)
        std::string causative_base = surface + "る";

        // Verify this is a valid ichidan verb
        const auto& all_candidates = inflection.analyze(causative_base);
        float ichidan_confidence = vh::getIchidanConfidence(all_candidates);

        if (ichidan_confidence >= 0.4F) {
          float base_cost = verb_opts.bonus_ichidan +
                            (1.0F - ichidan_confidence) * verb_opts.confidence_cost_scale_small;
          SUZUME_DEBUG_LOG_VERBOSE(
              "[VERB_CAND] " << surface << " causative_renyokei lemma=" << causative_base
                             << " conf=" << ichidan_confidence << " cost=" << base_cost << "\n");
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, renyokei_end, base_cost, causative_base,
              grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
              true, CandidateOrigin::VerbKanji, ichidan_confidence, "causative_renyokei"));
        }
      }
    }
  }

  // Try Godan passive renyokei pattern: kanji + a-row + れ
  // Godan passive verbs (受身形) follow this pattern:
  //   言う → 言われる (passive, Ichidan verb)
  //   書く → 書かれる (passive, Ichidan verb)
  //   読む → 読まれる (passive, Ichidan verb)
  // The renyokei of these passive verbs ends with れ (e-row):
  //   言われ (renyokei of 言われる), connects to ます, ない, て, た, etc.
  // Pattern: kanji + a-row hiragana + れ
  if (kanji_end + 1 < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    char32_t second_hira = codepoints[kanji_end + 1];
    // A-row + れ pattern (godan passive renyokei)
    if (grammar::isARowCodepoint(first_hira) && second_hira == U'れ') {
      // Skip suru-verb passive pattern: kanji + さ + れ
      // e.g., 処理される should be 処理(noun) + される(aux), not godan passive
      // Also skip single kanji + さ + れ as these are typically not real verbs
      // e.g., 強される is not a verb (強い is adjective, 強 is noun)
      std::string kanji_check = extractSubstring(codepoints, start_pos, kanji_end);
      bool is_suru_passive_pattern = (first_hira == U'さ' &&
                                      grammar::isAllKanji(kanji_check));
      if (is_suru_passive_pattern) {
        // Skip - this should be handled as noun + される auxiliary
        // Continue to next pattern
      } else {

      size_t renyokei_end = kanji_end + 2;  // kanji + a-row + れ
      std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);

      // Check if this is a valid passive verb stem
      // The passive base form is surface + る (e.g., 言われ → 言われる)
      std::string passive_base = surface + "る";

      // Compute the original base verb lemma by converting A-row to U-row
      // e.g., 言われる: 言 + わ + れる → 言 + う = 言う
      std::string kanji_part = extractSubstring(codepoints, start_pos, kanji_end);
      std::string_view u_row_suffix = grammar::godanBaseSuffixFromARow(first_hira);
      std::string base_lemma = kanji_part + std::string(u_row_suffix);

      // Use analyze() to get all interpretations, not just the best one
      // The best overall interpretation might be Godan (言う + れる), but
      // there should also be an Ichidan interpretation (言われる as verb)
      const auto& all_candidates = inflection.analyze(passive_base);
      float ichidan_confidence = vh::getIchidanConfidence(all_candidates);

      // Passive verbs are Ichidan conjugation (言われる conjugates like 食べる)
      if (ichidan_confidence >= 0.4F) {
        // Check if followed by べき (classical obligation)
        // For 書かれべき pattern, we want 書か + れべき, not 書かれ + べき
        bool is_beki_pattern = false;
        if (renyokei_end < codepoints.size()) {
          char32_t next_char = codepoints[renyokei_end];
          if (next_char == U'べ') {
            is_beki_pattern = true;
          }
        }

        // Calculate base cost for passive candidates
        // Add penalty so the MeCab-compatible split path (縛ら+れ) can compete
        // Without this, the merged form (縛られ) has too low a cost (-0.16)
        // and always beats the split path (縛ら(0.1) + れ(aux))
        float base_cost = verb_opts.bonus_ichidan + (1.0F - ichidan_confidence) * verb_opts.confidence_cost_scale_small + bigram_cost::kMinor;

        // Skip renyokei candidate for べき patterns
        if (!is_beki_pattern) {
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, renyokei_end, base_cost, base_lemma,
              dictionary::ConjugationType::Ichidan,
              false, CandidateOrigin::VerbKanji, ichidan_confidence, "godan_passive_renyokei"));
        }

        // NOTE: Passive verb conjugated forms (言われる, 言われた, etc.) are NOT generated
        // as single tokens. MeCab splits them as: 言わ + れ + た
        // The renyokei form (言われ) generated above connects to auxiliary た/て/ない/etc.
      }
      }  // end else (not suru passive pattern)
    }
  }

  // NOTE: Ichidan passive forms (食べられる, 見られる) should split MeCab-style:
  //   食べられる → 食べ + られる (stem + passive auxiliary)
  //   見られる → 見 + られる
  // The ichidan stem candidates are generated in the section below
  // and the られる auxiliary is matched from entries.cpp.
  // We do NOT generate single-token passive candidates here to ensure split wins.

  // Generate Ichidan stem candidates for passive/potential auxiliary patterns
  // E.g., 信じられべき (信じ + られべき), 認められた (認め + られた)
  // These connect to られ+X (passive/potential auxiliary forms)
  // Unlike Godan mizenkei which uses れ+X, Ichidan uses られ+X
  {
    // Check if followed by られ+X pattern (られた, られる, られべき, られます, etc.)
    bool has_rare_suffix = false;
    size_t stem_end = 0;

    // Pattern 1: Kanji + E/I row hiragana + られ+X (e.g., 信じ+られべき, 認め+られた)
    if (kanji_end < hiragana_end) {
      char32_t first_hira = codepoints[kanji_end];
      if (grammar::isERowCodepoint(first_hira) || grammar::isIRowCodepoint(first_hira)) {
        size_t ichidan_stem_end = kanji_end + 1;
        // Check for られ pattern (at least 2 chars)
        if (ichidan_stem_end + 1 < codepoints.size() &&
            codepoints[ichidan_stem_end] == U'ら' &&
            codepoints[ichidan_stem_end + 1] == U'れ') {
          has_rare_suffix = true;
          stem_end = ichidan_stem_end;
        }
      }
    }

    // Pattern 2: Single kanji + られ+X (e.g., 見+られべき)
    // Only for known single-kanji Ichidan verbs
    if (!has_rare_suffix && kanji_end == start_pos + 1) {
      char32_t kanji_char = codepoints[start_pos];
      if (vh::isSingleKanjiIchidan(kanji_char)) {
        // Check for られ suffix right after the single kanji
        using namespace suzume::core::hiragana;
        if (kanji_end + 1 < codepoints.size() &&
            codepoints[kanji_end] == kRa &&
            codepoints[kanji_end + 1] == kRe) {
          has_rare_suffix = true;
          stem_end = kanji_end;
        }
      }
    }

    if (has_rare_suffix && stem_end > start_pos) {
      std::string surface = extractSubstring(codepoints, start_pos, stem_end);
      // Construct base form: stem + る (e.g., 信じ → 信じる, 見 → 見る)
      std::string base_form = surface + "る";

      // Verify the base form exists in dictionary or is valid Ichidan verb
      bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
      if (!is_valid_verb) {
        // Check if inflection analyzer recognizes this as Ichidan verb
        // Use >= threshold to include edge cases like 信じる (conf=0.3)
        // Check ALL candidates, not just best, because godan/ichidan may have same confidence
        const auto& all_cands = inflection.analyze(base_form);
        for (const auto& cand : all_cands) {
          if (cand.verb_type == grammar::VerbType::Ichidan && cand.confidence >= 0.3F) {
            is_valid_verb = true;
            break;
          }
        }
      }

      if (is_valid_verb) {
        // Negative cost to beat single-verb inflection path (which gets optimal_length -0.5 bonus)
        constexpr float kCost = candidate::verb_cost::kStandardBonus;
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " ichidan_stem_rare lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, stem_end, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "ichidan_stem_rare"));
      }
    }
  }

  // Generate single-kanji Ichidan verb candidates for polite auxiliary patterns
  // E.g., 寝ます → 寝(VERB) + ます(AUX), 見ます → 見(VERB) + ます(AUX)
  // These are single-kanji Ichidan verbs followed by ます or ない
  if (kanji_end == start_pos + 1 && hiragana_end > kanji_end) {
    char32_t kanji_char = codepoints[start_pos];

    if (vh::isSingleKanjiIchidan(kanji_char)) {
      // Check if followed by polite ます or negative auxiliary
      using namespace suzume::core::hiragana;
      char32_t h1 = codepoints[kanji_end];
      char32_t h2 = (kanji_end + 1 < codepoints.size()) ? codepoints[kanji_end + 1] : 0;
      bool is_polite_aux = (h1 == kMa && h2 == kSu);
      // Negative auxiliary ない and its conjugations:
      // ない (終止/連体), なく (連用), なかっ (た接続), なけれ (仮定)
      bool is_negative_aux =
          (h1 == kNa && (h2 == kI || h2 == kKu || h2 == kKa || h2 == kKe));

      if (is_polite_aux || is_negative_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStandardBonus;  // Strong bonus to beat NOUN candidate
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " single_kanji_ichidan_polite lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_polite"));
      }

      // Also handle た and て patterns for single-kanji Ichidan verbs
      // E.g., 寝た → 寝(VERB) + た(AUX), 見て → 見(VERB) + て(PARTICLE)
      // MeCab splits these as: 寝+た, 見+て
      bool is_ta_aux = (h1 == kTa);
      bool is_te_particle = (h1 == kTe);
      if (is_ta_aux || is_te_particle) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat unified dictionary entry
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " single_kanji_ichidan_ta_te lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_ta_te"));
      }

      // Handle colloquial contraction patterns for single-kanji Ichidan verbs
      // MeCab splits these as: 見+とく, 見+ちゃう, etc.
      // と → とく, といた, といて (ておく contraction: 見とく = 見ておく)
      // ち → ちゃう, ちゃった (てしまう contraction: 見ちゃう = 見てしまう)
      // ど → どく, どいた (voiced ておく: only for godan onbin, but check anyway)
      bool is_toku_aux = (h1 == kTo);
      bool is_chau_aux = (h1 == kChi);
      if (is_toku_aux || is_chau_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat unified contraction entry
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " single_kanji_ichidan_colloquial lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_colloquial"));
      }

      // Handle られる pattern for single-kanji Ichidan verbs
      // E.g., 見られる → 見(VERB) + られる(AUX), 寝られる → 寝(VERB) + られる(AUX)
      // MeCab splits these as: 見+られる (passive/potential form)
      // Note: For ichidan verbs, the passive/potential is られる (not れる)
      bool is_rareru_aux = (h1 == kRa && h2 == kRe);
      if (is_rareru_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat godan mizenkei interpretation
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " single_kanji_ichidan_rareru lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_rareru"));
      }

      // Handle volitional pattern for single-kanji Ichidan verbs
      // E.g., 見よう → 見よ (volitional stem) + う (aux)
      // MeCab splits as: 見よ (動詞,未然ウ接続) + う (助動詞)
      bool is_volitional_aux = (h1 == kYo && h2 == kU);
      if (is_volitional_aux) {
        // Generate 漢字+よ as volitional stem
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
        std::string base_form = extractSubstring(codepoints, start_pos, kanji_end) + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat compound interpretation
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " single_kanji_ichidan_volitional lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end + 1, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_volitional",
            core::ExtendedPOS::VerbMizenkei));
      }

      // Handle causative させ/させる/させられ pattern for single-kanji Ichidan verbs
      // E.g., 見させる → 見(VERB mizenkei) + させる(AUX causative)
      //       見させられた → 見(VERB mizenkei) + させ + られ + た
      // MeCab splits these as: 見+させる (not 見さ+せる like godan-sa)
      bool is_saseru_aux = (h1 == kSa && h2 == kSe);
      if (is_saseru_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat NOUN candidate
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " single_kanji_ichidan_causative lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_causative",
            core::ExtendedPOS::VerbMizenkei));
      }
    }
  }

  // Generate Godan mizenkei stem candidates for auxiliary separation
  // E.g., 書か (from 書く), 読ま (from 読む), 話さ (from 話す)
  // These connect to passive (れる), causative (せる), negative (ない)
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // A-row hiragana: あ, か, さ, た, な, ま, ら, わ, が, ざ, だ, ば, ぱ
    if (grammar::isARowCodepoint(first_hira)) {
      size_t mizenkei_end = kanji_end + 1;
      // Check if followed by passive/causative auxiliary pattern
      if (mizenkei_end < hiragana_end) {
        char32_t next_char = codepoints[mizenkei_end];
        // Generate mizenkei candidates for:
        // 1. Classical べき patterns: 書かれべき, 読まれべき
        // 2. Classical negation ぬ: 揃わぬ, 知らぬ, 行かぬ
        // 3. Passive patterns: 書かれる, 言われた (MeCab-compatible split)
        bool is_beki_pattern = false;
        bool is_nu_pattern = false;
        bool is_passive_pattern = false;
        if (next_char == U'れ') {
          if (mizenkei_end + 2 < codepoints.size() &&
              codepoints[mizenkei_end + 1] == U'べ' &&
              codepoints[mizenkei_end + 2] == U'き') {
            // Check for れべき pattern
            is_beki_pattern = true;
          } else {
            // Check for passive patterns: れる, れた, れて, れない, れます
            // E.g., 言われる → 言わ (mizenkei) + れる (passive AUX)
            size_t re_pos = mizenkei_end;
            if (re_pos + 1 < codepoints.size()) {
              char32_t after_re = codepoints[re_pos + 1];
              // れる, れた, れて
              if (after_re == U'る' || after_re == U'た' || after_re == U'て') {
                is_passive_pattern = true;
              }
              // れな (れない, れなかった)
              else if (after_re == U'な' && re_pos + 2 < codepoints.size() &&
                       codepoints[re_pos + 2] == U'い') {
                is_passive_pattern = true;
              }
              // れま (れます, れました, れません)
              else if (after_re == U'ま' && re_pos + 2 < codepoints.size() &&
                       (codepoints[re_pos + 2] == U'す' || codepoints[re_pos + 2] == U'せ')) {
                is_passive_pattern = true;
              }
            }
          }
        }
        // Check for classical negation ぬ pattern
        // E.g., 揃わぬ → 揃わ (mizenkei) + ぬ (AUX)
        if (next_char == U'ぬ') {
          is_nu_pattern = true;
        }
        // Check for colloquial contracted negative ん pattern
        // E.g., 行かん → 行か (mizenkei) + ん (contracted negative AUX)
        //       言わん → 言わ (mizenkei) + ん
        // Skip single-kanji + さ + ん pattern (honorific さん suffix)
        // E.g., 姉さん should be 姉 + さん (noun + suffix), not 姉さ + ん (verb + AUX)
        bool is_n_pattern = false;
        if (next_char == U'ん') {
          // Skip if single kanji + さ (potential さん honorific)
          bool is_honorific_san = (kanji_end == start_pos + 1 && first_hira == U'さ');
          if (!is_honorific_san) {
            is_n_pattern = true;
          }
        }
        // Check for standard negative ない pattern
        // E.g., 行かない → 行か (mizenkei) + ない (negative AUX)
        //       書かない → 書か (mizenkei) + ない
        bool is_nai_pattern = false;
        if (next_char == U'な' && mizenkei_end + 1 < codepoints.size() &&
            codepoints[mizenkei_end + 1] == U'い') {
          is_nai_pattern = true;
        }
        // Check for past negative なかっ pattern (MeCab-compatible split)
        // E.g., 書かなかった → 書か (mizenkei) + なかっ (negative past AUX) + た
        //       行かなかった → 行か (mizenkei) + なかっ + た
        bool is_nakatt_pattern = false;
        if (next_char == U'な' && mizenkei_end + 3 < codepoints.size() &&
            codepoints[mizenkei_end + 1] == U'か' &&
            codepoints[mizenkei_end + 2] == U'っ') {
          is_nakatt_pattern = true;
        }
        // Check for causative auxiliary せ pattern (MeCab-compatible split)
        // E.g., 聞かせられた → 聞か (mizenkei) + せ (causative AUX) + られ + た
        //       書かせる → 書か (mizenkei) + せる (causative AUX)
        bool is_causative_pattern = false;
        if (next_char == U'せ') {
          // せ followed by られ, る, た, て, etc.
          if (mizenkei_end + 1 < codepoints.size()) {
            char32_t after_se = codepoints[mizenkei_end + 1];
            // せら (せられる, せられた)
            if (after_se == U'ら') {
              is_causative_pattern = true;
            }
            // せる, せた, せて
            else if (after_se == U'る' || after_se == U'た' || after_se == U'て') {
              is_causative_pattern = true;
            }
            // せな (せない)
            else if (after_se == U'な' && mizenkei_end + 2 < codepoints.size() &&
                     codepoints[mizenkei_end + 2] == U'い') {
              is_causative_pattern = true;
            }
          }
        }
        if (is_beki_pattern || is_nu_pattern || is_n_pattern || is_nai_pattern || is_nakatt_pattern || is_passive_pattern || is_causative_pattern) {
          // Derive VerbType from the A-row ending (e.g., か → GodanKa)
          grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(first_hira);
          if (verb_type != grammar::VerbType::Unknown) {
            // Skip GodanSa mizenkei for all-kanji stems (likely サ変名詞 + される)
            // E.g., 装飾さ should be 装飾 + される, not 装飾す mizenkei
            bool is_suru_verb_pattern = false;
            if (verb_type == grammar::VerbType::GodanSa) {
              std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
              if (grammar::isAllKanji(kanji_stem) && kanji_stem.size() >= 6) {
                // This is likely a Suru verb pattern (2+ kanji followed by される)
                // The connection rules will handle 装飾 + される instead
                // Note: 6 bytes = 2 kanji characters in UTF-8
                is_suru_verb_pattern = true;
              }
              // Skip single-kanji GodanSa + causative pattern (likely ichidan verb + させ)
              // E.g., 見させられた = 見 + させ + られ + た (ichidan 見る + causative)
              //       Not: 見さ + せ + られ + た (godan 見す doesn't exist)
              // Real godan-sa verbs (話す, 出す, 消す) have multi-char stems (話さ, 出さ, 消さ)
              if (is_causative_pattern && kanji_stem.size() == 3) {  // 3 bytes = 1 kanji
                is_suru_verb_pattern = true;  // Skip generation
              }
            }
            if (is_suru_verb_pattern) {
              // Skip mizenkei generation for Suru verb patterns
            } else {
            // Get base suffix (e.g., か → く for GodanKa)
            std::string_view base_suffix = grammar::godanBaseSuffixFromARow(first_hira);
            if (!base_suffix.empty()) {
              // Construct base form: stem + base_suffix (e.g., 書 + く = 書く)
              std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
              std::string base_form = kanji_stem + std::string(base_suffix);

              // Verify the base form is a valid verb
              // First check dictionary, then fall back to inflection analysis
              // IMPORTANT: For passive pattern, require dictionary check only for
              // most verb rows. The inflection analyzer is too permissive and will
              // accept patterns like 泊む (from 泊まれる) which don't exist.
              // EXCEPTIONS that allow inflection fallback:
              // - WA-row (わ行): passive (奪われる) doesn't conflict with potential
              // - RA-row (ら行): Xらる is not a valid modern verb, so Xられる
              //   is always passive of Xる (e.g., 縛られる = passive of 縛る)
              bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
              // For passive pattern, allow inflection fallback for WA-row and RA-row
              bool allow_inflection_fallback = !is_passive_pattern ||
                  first_hira == U'わ' || first_hira == U'ら';
              if (!is_valid_verb && allow_inflection_fallback) {
                // For non-passive patterns (ない, ぬ, etc.), allow inflection fallback
                // For WA-row passive, also allow with higher confidence threshold
                float threshold = is_passive_pattern ? 0.6F : 0.5F;
                auto infl_result = inflection.getBest(base_form);
                is_valid_verb = infl_result.confidence > threshold &&
                                vh::isGodanVerbType(infl_result.verb_type);
              }

              // Skip irregular verb 来る for passive — its passive is 来+られる, not 来ら+れる
              if (is_valid_verb && is_passive_pattern && base_form == "来る") {
                is_valid_verb = false;
              }

              if (is_valid_verb) {
                std::string surface = extractSubstring(codepoints, start_pos, mizenkei_end);
                // Cost varies by pattern:
                // - ぬ pattern: negative cost (-0.5F) to beat combined verb form
                //   揃わぬ(VERB) gets ~-0.1 total, so split needs lower cost
                // - ん pattern: negative cost (-0.5F) for contracted negative
                //   行かん(VERB) should split to 行か + ん
                // - ない pattern: negative cost (-0.5F) for standard negative
                //   行かない(VERB) should split to 行か + ない
                // - passive pattern: negative cost (-0.5F) for MeCab-compatible split
                //   言われる(VERB) gets ~0.15, so split (言わ+れる) needs lower cost
                // - べき pattern: moderate cost (0.2F) for classical obligation
                float cost = 0.2F;  // default for beki
                if (is_nu_pattern || is_n_pattern || is_nai_pattern) {
                  cost = -0.5F;
                } else if (is_passive_pattern) {
                  cost = -0.5F;
                }
                const char* debug_pattern = is_nu_pattern ? "nu" :
                                            is_n_pattern ? "n" :
                                            is_nai_pattern ? "nai" :
                                            is_passive_pattern ? "passive" : "beki";
                SUZUME_DEBUG_VERBOSE_BLOCK {
                  SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                      << " godan_mizenkei lemma=" << base_form
                                      << " cost=" << cost
                                      << " pattern=" << debug_pattern
                                      << "\n";
                }
                const char* info_pattern = is_nu_pattern ? "godan_mizenkei_nu" :
                                           is_n_pattern ? "godan_mizenkei_n" :
                                           is_nai_pattern ? "godan_mizenkei_nai" :
                                           is_nakatt_pattern ? "godan_mizenkei_nakatt" :
                                           is_passive_pattern ? "godan_mizenkei_passive" :
                                           "godan_mizenkei";
                // Use explicit VerbMizenkei EPOS for negative/passive patterns to enable bigram connection
                core::ExtendedPOS epos = (is_nu_pattern || is_n_pattern || is_nai_pattern || is_nakatt_pattern || is_passive_pattern)
                    ? core::ExtendedPOS::VerbMizenkei
                    : core::ExtendedPOS::Unknown;
                candidates.push_back(makeVerbCandidate(
                    surface, start_pos, mizenkei_end, cost, base_form,
                    grammar::verbTypeToConjType(verb_type),
                    true, CandidateOrigin::VerbKanji, 0.9F, info_pattern, epos));
              }
            }
            }  // else (not Suru verb pattern)
          }
        }
      }
    }
  }

  // Generate mizenkei candidates for verbs with multiple okurigana + negative patterns
  // E.g., 分からない → 分から (mizenkei of 分かる) + ない
  //       分からなかった → 分から (mizenkei of 分かる) + なかっ + た
  //       始まらない → 始まら (mizenkei of 始まる) + ない
  // These are Godan verbs where the okurigana includes 2+ hiragana before the A-row ending
  if (kanji_end < hiragana_end && hiragana_end >= kanji_end + 3) {
    // Look for A-row hiragana + negative patterns (ない, なかっ, or ん)
    for (size_t scan_pos = kanji_end + 1; scan_pos < hiragana_end - 1; ++scan_pos) {
      char32_t cur_char = codepoints[scan_pos];
      char32_t next_char = codepoints[scan_pos + 1];
      // Check if cur_char is A-row and followed by negative pattern
      bool is_nai_pattern = grammar::isARowCodepoint(cur_char) && next_char == U'な' &&
          scan_pos + 2 < codepoints.size() && codepoints[scan_pos + 2] == U'い';
      bool is_nakatt_pattern = grammar::isARowCodepoint(cur_char) && next_char == U'な' &&
          scan_pos + 3 < codepoints.size() && codepoints[scan_pos + 2] == U'か' &&
          codepoints[scan_pos + 3] == U'っ';
      // Check for contracted negative ん pattern (分からん, 始まらん)
      // ん must be at the end of the string (hiragana_end == scan_pos + 2)
      bool is_n_pattern = grammar::isARowCodepoint(cur_char) && next_char == U'ん' &&
          scan_pos + 2 == hiragana_end;
      if (is_nai_pattern || is_nakatt_pattern || is_n_pattern) {
        // Found A-row + negative pattern at scan_pos
        // The mizenkei would be from start_pos to scan_pos + 1
        size_t multi_miz_end = scan_pos + 1;
        grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(cur_char);
        if (verb_type != grammar::VerbType::Unknown) {
          // Construct the base form
          // E.g., 分から → 分かる (replace A-row ending with U-row)
          std::string_view base_suffix = grammar::godanBaseSuffixFromARow(cur_char);
          if (!base_suffix.empty()) {
            std::string stem = extractSubstring(codepoints, start_pos, scan_pos);
            std::string base_form = stem + std::string(base_suffix);
            // Verify this is a valid verb
            bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
            if (!is_valid_verb) {
              auto infl_result = inflection.getBest(base_form);
              is_valid_verb = infl_result.confidence > 0.5F &&
                              vh::isGodanVerbType(infl_result.verb_type);
            }
            if (is_valid_verb) {
              std::string surface = extractSubstring(codepoints, start_pos, multi_miz_end);
              constexpr float kCost = candidate::verb_cost::kStandardBonus;  // Same as other negative patterns
              const char* pattern = is_nakatt_pattern ? "multi_mizenkei_nakatt" :
                                    is_n_pattern ? "multi_mizenkei_n" :
                                    "multi_mizenkei_nai";
              SUZUME_DEBUG_VERBOSE_BLOCK {
                SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                    << " " << pattern << " lemma=" << base_form
                                    << " cost=" << kCost << "\n";
              }
              candidates.push_back(makeVerbCandidate(
                  surface, start_pos, multi_miz_end, kCost, base_form,
                  grammar::verbTypeToConjType(verb_type),
                  true, CandidateOrigin::VerbKanji, 0.9F, pattern,
                  core::ExtendedPOS::VerbMizenkei));
            }
          }
        }
        break;  // Only generate one candidate per position
      }
    }
  }

  // Generate Godan onbin stem candidates for contraction auxiliary patterns
  // E.g., 読んでる → 読ん (onbin of 読む) + でる (ている contraction)
  //       書いとく → 書い (onbin of 書く) + とく (ておく contraction)
  // Key patterns:
  // - kanji + ん + (ど/じ/で): GodanMa/GodanBa/GodanNa verbs (読んでる, 飛んどく)
  // - kanji + い + (と/ち): GodanKa/GodanGa verbs (書いとく, 泳いちゃう)
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // Check for hatsuonbin (ん) or ikuon (い) patterns
    bool is_hatsuonbin = (first_hira == U'ん');
    bool is_ikuon = (first_hira == U'い');
    if ((is_hatsuonbin || is_ikuon) && kanji_end + 1 < hiragana_end) {
      char32_t next_char = codepoints[kanji_end + 1];
      bool is_contraction_pattern = false;
      if (is_hatsuonbin) {
        // ん + ど (どく/どいた) or じ (じゃう/じゃった) or で (でる/でた/でて)
        is_contraction_pattern = (next_char == U'ど' || next_char == U'じ' || next_char == U'で');
      } else {
        // い + と (とく/といた) or ち (ちゃう/ちゃった)
        is_contraction_pattern = (next_char == U'と' || next_char == U'ち');
      }
      if (is_contraction_pattern) {
        // Determine candidate verb types based on onbin type
        // Uses centralized GodanRow data instead of manual enumeration
        std::string_view onbin_str = is_hatsuonbin ? "ん" : "い";
        auto candidates_to_try = vh::getGodanTypesByOnbin(onbin_str);
        // Get the kanji stem
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
        // First, check dictionary for ALL verb types before falling back to inflection
        // This ensures dictionary-verified verbs take precedence
        grammar::VerbType matched_verb_type = grammar::VerbType::Unknown;
        std::string matched_base_form;
        // Phase 1: Dictionary check
        for (const auto& [verb_type, base_suffix] : candidates_to_try) {
          std::string base_form = kanji_stem + std::string(base_suffix);
          if (vh::isVerbInDictionaryWithType(dict_manager, base_form, verb_type) ||
              vh::isVerbInDictionary(dict_manager, base_form)) {
            matched_verb_type = verb_type;
            matched_base_form = base_form;
            break;
          }
        }
        // Phase 2: Inflection analysis fallback
        if (matched_verb_type == grammar::VerbType::Unknown && kanji_end > start_pos) {
          std::string full_surface = extractSubstring(codepoints, start_pos, hiragana_end);
          const auto& infl_results = inflection.analyze(full_surface);
          float best_conf = 0.0F;
          for (const auto& result : infl_results) {
            if (result.confidence >= 0.5F && result.confidence > best_conf) {
              // Check if this result matches one of our candidate verb types
              for (const auto& [verb_type, base_suffix] : candidates_to_try) {
                std::string base_form = kanji_stem + std::string(base_suffix);
                if (result.base_form == base_form && result.verb_type == verb_type) {
                  matched_verb_type = verb_type;
                  matched_base_form = base_form;
                  best_conf = result.confidence;
                  break;
                }
              }
            }
          }
        }
        if (matched_verb_type == grammar::VerbType::Unknown) {
          // No valid verb found
        } else {
          // Found valid verb - generate onbin stem candidate
          std::string onbin_surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
          constexpr float kOnbinCost = candidate::verb_cost::kStandardBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                << " kanji_onbin_contraction lemma=" << matched_base_form
                                << " cost=" << kOnbinCost << "\n";
          }
          const char* pattern = is_hatsuonbin ? "kanji_hatsuonbin" : "kanji_ikuon";
          candidates.push_back(makeVerbCandidate(
              onbin_surface, start_pos, kanji_end + 1, kOnbinCost, matched_base_form,
              grammar::verbTypeToConjType(matched_verb_type),
              true, CandidateOrigin::VerbKanji, 0.9F, pattern));
        }
      }
    }
  }

  // Generate Godan sokuonbin (っ) candidates for basic te/ta-form splitting
  // E.g., 言って → 言っ (onbin of 言う) + て (particle)
  //       言った → 言っ (onbin of 言う) + た (auxiliary)
  //       待って → 待っ (onbin of 待つ) + て (particle)
  //       買って → 買っ (onbin of 買う) + て (particle)
  // Key patterns:
  // - kanji + っ + て/た/たら/たり: GodanRa/GodanTa/GodanWa verbs
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // Check for sokuonbin (っ) pattern
    if (first_hira == U'っ' && kanji_end + 1 < hiragana_end) {
      char32_t next_char = codepoints[kanji_end + 1];
      // Basic te/ta form patterns (て, た, たら, たり), ちゃう (ち), and とく (と) contractions
      bool is_te_ta_pattern =
          (next_char == U'て' || next_char == U'た' || next_char == U'ち' || next_char == U'と');
      if (is_te_ta_pattern) {
        // Determine candidate verb types based on sokuonbin
        // っ-onbin: GodanRa, GodanTa, GodanWa, GodanKa (行く is irregular)
        static const std::vector<std::pair<grammar::VerbType, std::string_view>>
            sokuonbin_types = {
                {grammar::VerbType::GodanKa, "く"},  // 行く (irregular)
                {grammar::VerbType::GodanRa, "る"},
                {grammar::VerbType::GodanTa, "つ"},
                {grammar::VerbType::GodanWa, "う"},
            };
        // Get the kanji stem
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);

#ifdef SUZUME_DEBUG
        // TRACE: Collect all candidates for logging (debug builds only)
        std::string onbin_surface_for_log = extractSubstring(codepoints, start_pos, kanji_end + 1);
        struct SokuonbinCandidate {
          grammar::VerbType type;
          std::string base_form;
          bool dict_match;
        };
        std::vector<SokuonbinCandidate> all_sokuonbin_candidates;
#endif

        // First, check dictionary for ALL verb types
        grammar::VerbType matched_verb_type = grammar::VerbType::Unknown;
        std::string matched_base_form;
        bool matched_via_dict = false;
        for (const auto& [verb_type, base_suffix] : sokuonbin_types) {
          std::string base_form = kanji_stem + std::string(base_suffix);
          bool dict_match = vh::isVerbInDictionaryWithType(dict_manager, base_form, verb_type) ||
                            vh::isVerbInDictionary(dict_manager, base_form);
#ifdef SUZUME_DEBUG
          all_sokuonbin_candidates.push_back({verb_type, base_form, dict_match});
#endif
          if (dict_match && matched_verb_type == grammar::VerbType::Unknown) {
            matched_verb_type = verb_type;
            matched_base_form = base_form;
            matched_via_dict = true;
          }
        }
        // Phase 2: Inflection analysis fallback
        // Try progressively shorter surfaces to handle cases where hiragana_end
        // includes particles (e.g., "使っているが" vs "使っている")
        // Skip if kanji stem starts with a dictionary noun (e.g., 昨日買っ → skip)
        // This prevents false compound verb candidates like "昨日買う"
        bool starts_with_dict_noun = false;
        bool remainder_is_dict_verb = false;
        if (dict_manager != nullptr && kanji_end - start_pos >= 2) {
          // Check if any prefix of kanji_stem is a dictionary noun
          for (size_t prefix_len = 1; prefix_len < kanji_end - start_pos; ++prefix_len) {
            std::string prefix = extractSubstring(codepoints, start_pos, start_pos + prefix_len);
            auto lookup_results = dict_manager->lookup(prefix, 0);
            for (const auto& result : lookup_results) {
              if (result.entry != nullptr && result.entry->surface == prefix &&
                  result.entry->pos == core::PartOfSpeech::Noun) {
                starts_with_dict_noun = true;
                SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << kanji_stem
                                          << "\" starts with dict noun \"" << prefix << "\"\n");
                break;
              }
            }
            if (starts_with_dict_noun) break;
          }
          // Also check: if removing single-kanji prefix leaves a valid dict verb
          // E.g., 本買う → 本 + 買う, where 買う is a dict verb
          // This handles patterns like 本買った, 服買った, 車買った
          if (!starts_with_dict_noun && kanji_end - start_pos == 2) {
            // Get the second kanji + verb ending
            std::string remainder_stem = extractSubstring(codepoints, start_pos + 1, kanji_end);
            for (const auto& [verb_type, base_suffix] : sokuonbin_types) {
              std::string remainder_base = remainder_stem + std::string(base_suffix);
              if (vh::isVerbInDictionary(dict_manager, remainder_base)) {
                remainder_is_dict_verb = true;
                SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << kanji_stem
                                          << "\" remainder \"" << remainder_base << "\" is dict verb\n");
                break;
              }
            }
          }
        }
        if (matched_verb_type == grammar::VerbType::Unknown &&
            !starts_with_dict_noun && !remainder_is_dict_verb) {
          if (kanji_stem.size() == core::kJapaneseCharBytes) {
            // Single-kanji stem: use inflection analysis of the longer surface
            // (kanji + っ + following chars) to find verb type.
            // Common verbs like 残る, 立つ, 打つ may not be in L2 dictionary.
            // Try surfaces of increasing length to get inflection result.
            for (size_t try_end = kanji_end + 2;
                 try_end <= codepoints.size() && try_end <= kanji_end + 4;
                 ++try_end) {
              std::string try_surface = extractSubstring(codepoints, start_pos, try_end);
              auto infl_result = inflection.analyze(try_surface);
              if (!infl_result.empty()) {
                const auto& best = infl_result[0];
                if (best.confidence >= 0.6F) {
                  for (const auto& [verb_type, base_suffix] : sokuonbin_types) {
                    if (best.verb_type == verb_type) {
                      matched_verb_type = verb_type;
                      matched_base_form = kanji_stem + std::string(base_suffix);
                      SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] \"" << kanji_stem
                          << "\" single-kanji sokuonbin → " << matched_base_form
                          << " (infl, conf=" << best.confidence << ")\n");
                      break;
                    }
                  }
                  if (matched_verb_type != grammar::VerbType::Unknown) break;
                }
              }
            }
          } else {
            SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << kanji_stem
                                      << "\" skip non-dict sokuonbin\n");
          }
        }

#ifdef SUZUME_DEBUG
        // TRACE: Log all sokuonbin candidates
        SUZUME_DEBUG_TRACE_BLOCK {
          SUZUME_DEBUG_STREAM << "[SOKUONBIN_CANDIDATES] \"" << onbin_surface_for_log << "\":\n";
          constexpr float kSokuonbinCost = candidate::verb_cost::kStandardBonus;
          for (const auto& cand : all_sokuonbin_candidates) {
            bool is_selected = (cand.type == matched_verb_type);
            SUZUME_DEBUG_STREAM << "  - " << cand.base_form << " ("
                                << grammar::verbTypeToString(cand.type) << "): "
                                << "dict_match=" << (cand.dict_match ? "YES" : "NO")
                                << ", score=" << (cand.dict_match ? kSokuonbinCost : 0.0F)
                                << (is_selected ? "" : " (skipped)") << "\n";
          }
          if (matched_verb_type != grammar::VerbType::Unknown) {
            SUZUME_DEBUG_STREAM << "  → Selected: " << matched_base_form << " ("
                                << grammar::verbTypeToString(matched_verb_type) << ")\n";
          } else {
            SUZUME_DEBUG_STREAM << "  → No match found\n";
          }
        }
#endif

        if (matched_verb_type != grammar::VerbType::Unknown) {
          // Found valid verb - generate sokuonbin stem candidate
          // Dict-matched verbs get bonus (-0.5) to beat unsplit forms
          // Inflection-only matches get neutral cost (0) to avoid false positives
          // like 像っ (from 像る which is not a real verb)
          std::string onbin_surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
          // Dict-matched verbs get bonus (-0.5) to beat unsplit forms
          // Inflection-only matches (2-kanji stems only) get neutral cost
          const float sokuonbin_cost = matched_via_dict ? -0.5F : 0.0F;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                << " kanji_sokuonbin lemma=" << matched_base_form
                                << " cost=" << sokuonbin_cost
                                << (matched_via_dict ? " (dict)" : " (infl)") << "\n";
          }
          candidates.push_back(makeVerbCandidate(
              onbin_surface, start_pos, kanji_end + 1, sokuonbin_cost, matched_base_form,
              grammar::verbTypeToConjType(matched_verb_type),
              true, CandidateOrigin::VerbKanji, 0.9F, "kanji_sokuonbin",
              core::ExtendedPOS::VerbOnbinkei));
        }
      }
    }
  }

  // Generate Godan sokuonbin (っ) candidates for extended patterns
  // E.g., 閉まった → 閉まっ (onbin of 閉まる) + た (auxiliary)
  //       始まった → 始まっ (onbin of 始まる) + た (auxiliary)
  //       決まった → 決まっ (onbin of 決まる) + た (auxiliary)
  // Key patterns:
  // - kanji + hiragana + っ + た/て: GodanRa verbs with multi-char stem
  // Constraints:
  // - Hiragana portion (before っ) should be 1-2 chars (まった, まりった)
  // - Base form must exist in dictionary (prevents false positives)
  // - Require at least kanji + 2 hiragana (e.g., 閉まっ = 閉 + ま + っ)
  if (hiragana_end - kanji_end >= 2) {
    // Check for pattern ending in っ + た/て
    // Look for っ at position hiragana_end - 2 (second to last)
    char32_t second_last = codepoints[hiragana_end - 2];
    char32_t last_char = codepoints[hiragana_end - 1];
    bool is_sokuonbin_te_ta = (second_last == U'っ' &&
                               (last_char == U'た' || last_char == U'て'));
    // Hiragana between kanji and っ should be 1-2 chars
    // hiragana_end - kanji_end = total hiragana chars (including っ and た/て)
    // So hiragana before っ = (hiragana_end - kanji_end) - 2
    size_t hiragana_before_onbin = (hiragana_end - kanji_end) - 2;
    bool reasonable_length = (hiragana_before_onbin >= 1 && hiragana_before_onbin <= 2);
    if (is_sokuonbin_te_ta && reasonable_length && hiragana_end - kanji_end >= 3) {
      // We have kanji + 1-2 hiragana + っ + た/て
      // Generate candidate for kanji + hiragana + っ (without the た/て)
      size_t onbin_end = hiragana_end - 1;  // Position after っ
      std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);

      // Skip if hiragana portion is なかっ (negative past pattern: なかっ+た)
      // This prevents false positives like 来なかった → 来なかっ+た (来なかる doesn't exist)
      // The correct split is 来 + なかっ + た (kuru + negative aux + past)
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, onbin_end);
      if (hiragana_part == "なかっ") {
        // This is negative past, not extended sokuonbin - skip
      } else if (hiragana_part == "であっ") {
        // This is copula である pattern (重要であった = 重要 + で + あっ + た)
        // Skip candidate generation to allow proper copula splitting
      } else if (utf8::startsWith(hiragana_part, "といっ")) {
        // Skip と+いっ pattern - this is particle と + verb いう
        // E.g., 友人といった = 友人 + と + いっ + た, not 友人といる
      } else if (hiragana_part == "くなっ") {
        // Skip く+なっ pattern - this is i-adjective adverbial + なる verb
        // E.g., 良くなった = 良く + なっ + た, not 良くなる as single verb
        // MeCab splits: 高くなった → 高く + なっ + た
      } else {
      // Skip godan終止形 + っ + て pattern - this is verb + って(quotative)
      // E.g., 行くって = 行く + って, 食べるって = 食べる + って
      // Godan 終止形 endings: く, す, つ, う, ぐ, ぶ, む, ぬ, る
      // The sokuonbin of godan verbs drops the ending (行く→行っ), not adds っ (行くっ is invalid)
      // Check if hiragana_part ends with godan終止形 + っ
      bool is_quotative_pattern = false;
      if (last_char == U'て' && hiragana_part.size() >= 6 /* at least 2 chars: Xっ */) {
        // Get the character before っ (second to last in hiragana_part)
        // hiragana_part ends with っ (which is at onbin_end - 1)
        // The char before っ is at position onbin_end - 2
        char32_t char_before_sokuon = codepoints[onbin_end - 2];
        is_quotative_pattern = (char_before_sokuon == U'く' || char_before_sokuon == U'す' ||
                                char_before_sokuon == U'つ' || char_before_sokuon == U'う' ||
                                char_before_sokuon == U'ぐ' || char_before_sokuon == U'ぶ' ||
                                char_before_sokuon == U'む' || char_before_sokuon == U'ぬ' ||
                                char_before_sokuon == U'る');
      }
      if (is_quotative_pattern) {
        // Skip: this is likely quotative って, not extended sokuonbin
      } else {
      // Build potential base form and verify it exists in dictionary or inflection
      // This prevents false positives like 食べてしまる
      std::string stem = extractSubstring(codepoints, start_pos, onbin_end - 1);
      std::string potential_base = stem + "る";

      // Skip if hiragana before っ is だ (copula pattern)
      // E.g., 本だった = 本 + だっ + た (noun + copula), not 本だる (verb)
      // But 閉まった = 閉まっ + た (verb 閉まる) is valid
      char32_t char_before_sokuon = codepoints[hiragana_end - 3];
      if (char_before_sokuon == U'だ') {
        // This is a copula pattern (NOUN + だった), not a verb
        // Skip candidate generation
      } else {
      // Check dictionary first
      bool in_dict = vh::isVerbInDictionaryWithType(dict_manager, potential_base,
                                                     grammar::VerbType::GodanRa) ||
                     vh::isVerbInDictionary(dict_manager, potential_base);

      // Fallback: inflection analysis for common patterns like 閉まる
      // Only use inflection for short hiragana patterns (1 char before っ)
      // Longer patterns like となっ may be noun+particle+verb (一丸+と+なる)
      bool infl_verified = false;
      if (!in_dict && hiragana_before_onbin == 1) {
        const auto& infl_results = inflection.analyze(onbin_surface + "た");
        for (const auto& result : infl_results) {
          if (result.verb_type == grammar::VerbType::GodanRa &&
              result.base_form == potential_base &&
              result.confidence >= 0.3F) {
            infl_verified = true;
            break;
          }
        }
      }

      // Skip if this is an i-adjective katt-form (美しかっ → 美しい, 高かっ → 高い)
      // The stem ends with か, so remove か and add い to get adjective base form
      // E.g., stem="美しか" → adj_base="美しい"
      bool is_adj_katt_form = false;
      if (stem.size() >= core::kTwoJapaneseCharBytes && utf8::endsWith(stem, "か")) {
        std::string adj_base = stem.substr(0, stem.size() - core::kJapaneseCharBytes) + "い";
        if (vh::isAdjectiveInDictionary(dict_manager, adj_base)) {
          is_adj_katt_form = true;
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] " << onbin_surface
                                  << " skip: i-adj \"" << adj_base << "\" in dict\n");
        }
      }

      if (!is_adj_katt_form && (in_dict || infl_verified)) {
        // Verified - generate candidate
        constexpr float kExtendedSokuonbinCost = candidate::verb_cost::kModerateBonus;
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                              << " extended_sokuonbin lemma=" << potential_base
                              << (in_dict ? " [dict]" : " [infl]")
                              << " cost=" << kExtendedSokuonbinCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            onbin_surface, start_pos, onbin_end, kExtendedSokuonbinCost, potential_base,
            grammar::verbTypeToConjType(grammar::VerbType::GodanRa),
            true, CandidateOrigin::VerbKanji, 0.9F, "extended_sokuonbin",
            core::ExtendedPOS::VerbOnbinkei));
      }
      }  // end else (not copula だ pattern)
      }  // end else (not quotative って pattern)
      }  // end else (not なかっ pattern)
    }
  }

  // Generate sokuonbin (っ) candidates from surfaces containing て+auxiliary chains
  // E.g., 挙がっている → 挙がっ (onbin of 挙がる) + て + いる
  //       集まってくる → 集まっ (onbin of 集まる) + て + くる
  // This handles patterns where っ+て/で is followed by auxiliary verbs,
  // which the basic/extended sokuonbin sections miss (they only handle endings)
  if (hiragana_end - kanji_end >= 3) {
    // Scan for っ in hiragana portion (not at the very end - that's handled above)
    for (size_t pos = kanji_end; pos + 2 < hiragana_end; ++pos) {
      if (codepoints[pos] != U'っ') continue;
      char32_t after_sokuon = codepoints[pos + 1];
      if (after_sokuon != U'て' && after_sokuon != U'で') continue;
      // Found っ+て/で NOT at end of surface - check if followed by auxiliary
      size_t hiragana_before_onbin = pos - kanji_end;
      if (hiragana_before_onbin < 1 || hiragana_before_onbin > 2) continue;

      size_t onbin_end = pos + 1;  // Position after っ
      std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);
      std::string stem = extractSubstring(codepoints, start_pos, pos);
      std::string potential_base = stem + "る";

      // Check hiragana part for known false patterns
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, onbin_end);
      if (hiragana_part == "なかっ" || hiragana_part == "であっ" ||
          utf8::startsWith(hiragana_part, "といっ") || hiragana_part == "くなっ") {
        continue;
      }

      bool in_dict_check = vh::isVerbInDictionaryWithType(dict_manager, potential_base,
                                                           grammar::VerbType::GodanRa) ||
                           vh::isVerbInDictionary(dict_manager, potential_base);
      bool infl_verified = false;
      if (!in_dict_check && hiragana_before_onbin == 1) {
        const auto& infl_results = inflection.analyze(onbin_surface + "た");
        for (const auto& result : infl_results) {
          if (result.verb_type == grammar::VerbType::GodanRa &&
              result.base_form == potential_base &&
              result.confidence >= 0.3F) {
            infl_verified = true;
            break;
          }
        }
      }

      if (in_dict_check || infl_verified) {
        constexpr float kTeAuxSokuonbinCost = candidate::verb_cost::kModerateBonus;
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                              << " te_aux_sokuonbin lemma=" << potential_base
                              << (in_dict_check ? " [dict]" : " [infl]")
                              << " cost=" << kTeAuxSokuonbinCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            onbin_surface, start_pos, onbin_end, kTeAuxSokuonbinCost, potential_base,
            grammar::verbTypeToConjType(grammar::VerbType::GodanRa),
            true, CandidateOrigin::VerbKanji, 0.9F, "te_aux_sokuonbin",
            core::ExtendedPOS::VerbOnbinkei));
      }
      break;  // Only process first っ+て/で occurrence
    }
  }

  // Generate Godan hatsuonbin (ん) candidates for basic te/ta-form splitting
  // E.g., 読んだ → 読ん (onbin of 読む) + だ (auxiliary)
  //       読んで → 読ん (onbin of 読む) + で (particle)
  //       飛んだ → 飛ん (onbin of 飛ぶ) + だ (auxiliary)
  //       死んだ → 死ん (onbin of 死ぬ) + だ (auxiliary)
  // Key patterns:
  // - kanji + ん + で/だ: GodanMa/GodanBa/GodanNa verbs
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // Check for hatsuonbin (ん) pattern
    if (first_hira == U'ん' && kanji_end + 1 < hiragana_end) {
      char32_t next_char = codepoints[kanji_end + 1];
      // Basic te/ta form patterns (で, だ)
      bool is_de_da_pattern = (next_char == U'で' || next_char == U'だ');
      if (is_de_da_pattern) {
        // Determine candidate verb types based on hatsuonbin
        // ん-onbin: GodanMa, GodanBa, GodanNa
        static const std::vector<std::pair<grammar::VerbType, std::string_view>>
            hatsuonbin_types = {
                {grammar::VerbType::GodanMa, "む"},
                {grammar::VerbType::GodanBa, "ぶ"},
                {grammar::VerbType::GodanNa, "ぬ"},
            };
        // Get the kanji stem
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);

        // First, check dictionary for ALL verb types
        grammar::VerbType matched_verb_type = grammar::VerbType::Unknown;
        std::string matched_base_form;
        for (const auto& [verb_type, base_suffix] : hatsuonbin_types) {
          std::string base_form = kanji_stem + std::string(base_suffix);
          bool dict_match = vh::isVerbInDictionaryWithType(dict_manager, base_form, verb_type) ||
                            vh::isVerbInDictionary(dict_manager, base_form);
          if (dict_match && matched_verb_type == grammar::VerbType::Unknown) {
            matched_verb_type = verb_type;
            matched_base_form = base_form;
          }
        }
        // Phase 2: Inflection analysis fallback
        if (matched_verb_type == grammar::VerbType::Unknown) {
          std::string full_surface = extractSubstring(codepoints, start_pos, hiragana_end);
          const auto& infl_results = inflection.analyze(full_surface);
          float best_conf = 0.0F;
          for (const auto& result : infl_results) {
            if (result.confidence >= 0.5F && result.confidence > best_conf) {
              for (const auto& [verb_type, base_suffix] : hatsuonbin_types) {
                std::string base_form = kanji_stem + std::string(base_suffix);
                if (result.base_form == base_form && result.verb_type == verb_type) {
                  matched_verb_type = verb_type;
                  matched_base_form = base_form;
                  best_conf = result.confidence;
                  break;
                }
              }
            }
          }
        }

        if (matched_verb_type != grammar::VerbType::Unknown) {
          // Found valid verb - generate hatsuonbin stem candidate
          std::string onbin_surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
          constexpr float kHatsuonbinCost = candidate::verb_cost::kStandardBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                << " kanji_hatsuonbin lemma=" << matched_base_form
                                << " cost=" << kHatsuonbinCost << "\n";
          }
          candidates.push_back(makeVerbCandidate(
              onbin_surface, start_pos, kanji_end + 1, kHatsuonbinCost, matched_base_form,
              grammar::verbTypeToConjType(matched_verb_type),
              true, CandidateOrigin::VerbKanji, 0.9F, "kanji_hatsuonbin",
              core::ExtendedPOS::VerbOnbinkei));
        }
      }
    }
  }

  // Generate hatsuonbin candidates for multi-hiragana okurigana and standalone ん
  // Covers cases NOT handled by the de/da handler above:
  // - Multi-hira okurigana: 汗ばんだ → 汗ばん (onbin of 汗ばむ) + だ
  // - Standalone single ん: 死ん (end of token, no following で/だ)
  if (kanji_end < hiragana_end) {
    for (size_t n_pos = kanji_end; n_pos < hiragana_end; ++n_pos) {
      if (codepoints[n_pos] != U'ん') continue;

      bool at_end = (n_pos + 1 >= hiragana_end);
      bool followed_by_de_da =
          (!at_end) && (codepoints[n_pos + 1] == U'で' || codepoints[n_pos + 1] == U'だ');

      // Skip: n_pos == kanji_end && !at_end is already handled by de/da handler above
      if (n_pos == kanji_end && !at_end) continue;
      // Only valid: at end of hiragana region, or followed by で/だ
      if (!at_end && !followed_by_de_da) continue;

      std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
      std::string hira_stem =
          (n_pos > kanji_end) ? extractSubstring(codepoints, kanji_end, n_pos) : "";

      static const std::vector<std::pair<grammar::VerbType, std::string_view>>
          n_onbin_types = {
              {grammar::VerbType::GodanMa, "む"},
              {grammar::VerbType::GodanBa, "ぶ"},
              {grammar::VerbType::GodanNa, "ぬ"},
          };

      for (const auto& [verb_type, base_suffix] : n_onbin_types) {
        std::string base_form = kanji_stem + hira_stem + std::string(base_suffix);
        if (vh::isVerbInDictionaryWithType(dict_manager, base_form, verb_type) ||
            vh::isVerbInDictionary(dict_manager, base_form)) {
          std::string onbin_surface = extractSubstring(codepoints, start_pos, n_pos + 1);
          constexpr float kHatsuonbinCost = candidate::verb_cost::kStandardBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                << " kanji_hatsuonbin_standalone lemma=" << base_form
                                << " cost=" << kHatsuonbinCost << "\n";
          }
          candidates.push_back(makeVerbCandidate(
              onbin_surface, start_pos, n_pos + 1, kHatsuonbinCost, base_form,
              grammar::verbTypeToConjType(verb_type),
              true, CandidateOrigin::VerbKanji, 0.9F, "kanji_hatsuonbin",
              core::ExtendedPOS::VerbOnbinkei));
          break;
        }
      }
      break;  // Only process first ん in the region
    }
  }

  // Add emphatic variants (来た → 来たっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints);

  // Sort by cost and return best candidates
  vh::sortCandidatesByCost(candidates);

  return candidates;
}

}  // namespace suzume::analysis
