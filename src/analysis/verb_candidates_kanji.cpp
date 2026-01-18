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
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 3 &&  // Max 3 kanji for verb stem
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

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
    // For patterns like 金がない, the が should remain NOUN + PARTICLE + ADJ
    bool is_passive_pattern = false;
    if (grammar::isARowCodepoint(first_hiragana)) {
      size_t next_pos = kanji_end + 1;
      if (next_pos < codepoints.size() && codepoints[next_pos] == U'れ') {
        // A-row + れ pattern: could be passive verb stem (言われ, 書かれ, etc.)
        is_passive_pattern = true;
      }
    }
    if (!is_passive_pattern) {
      return candidates;  // Not a verb - these particles follow nouns
    }
  }

  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 12 &&  // Max 12 hiragana for conjugation + aux
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Note: We no longer break at particle-like characters here.
    // The inflection module will determine if the full string is a valid
    // conjugated verb. This allows patterns like "飲みながら" (nomi-nagara)
    // where "が" is part of the auxiliary "ながら", not a standalone particle.
    ++hiragana_end;
  }

  // Need at least some hiragana for a conjugated verb
  if (hiragana_end <= kanji_end) {
    return candidates;
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
      // Only skip if kanji is 2+ characters, as single kanji + suffix
      // could be a valid verb stem (立ち → 立つ)
      // Note: Only skip for OTHER (suffixes), not VERB (する is a verb, not suffix)
      bool is_suffix_pattern = false;
      if (kanji_end - start_pos >= 2 && dict_manager != nullptr) {
        auto suffix_results = dict_manager->lookup(hiragana_part, 0);
        for (const auto& result : suffix_results) {
          if (result.entry != nullptr &&
              result.entry->surface == hiragana_part &&
              result.entry->is_low_info &&
              result.entry->pos == core::PartOfSpeech::Other) {
            // This hiragana part is a registered suffix - skip verb candidate
            is_suffix_pattern = true;
            break;
          }
        }
      }
      if (is_suffix_pattern) {
        continue;
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
      auto inflection_results = inflection.analyze(surface);
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
          bool is_onbin_type = (cand.verb_type == grammar::VerbType::GodanRa ||
                                cand.verb_type == grammar::VerbType::GodanTa ||
                                cand.verb_type == grammar::VerbType::GodanWa ||
                                cand.verb_type == grammar::VerbType::GodanKa);
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
          bool is_godan = (best.verb_type == grammar::VerbType::GodanKa ||
                           best.verb_type == grammar::VerbType::GodanGa ||
                           best.verb_type == grammar::VerbType::GodanSa ||
                           best.verb_type == grammar::VerbType::GodanTa ||
                           best.verb_type == grammar::VerbType::GodanNa ||
                           best.verb_type == grammar::VerbType::GodanBa ||
                           best.verb_type == grammar::VerbType::GodanMa ||
                           best.verb_type == grammar::VerbType::GodanRa ||
                           best.verb_type == grammar::VerbType::GodanWa);
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

          // Lower cost for higher confidence matches
          float base_cost = verb_opts.base_cost_standard + (1.0F - best.confidence) * verb_opts.confidence_cost_scale;
          // MeCab compatibility: Suru verbs should split as noun + する
          // Add penalty for unified suru-verb candidates to prefer split
          // e.g., 勉強する → 勉強 + する (split preferred)
          if (best.verb_type == grammar::VerbType::Suru &&
              best.stem.size() >= core::kTwoJapaneseCharBytes) {
            // Penalize unified suru-verb to prefer noun + する/される/させる split
            base_cost += 3.0F;
          }
          // Penalize ALL verb candidates with prefix-like kanji at start
          // e.g., 今何する/今何してる should split, not be single verb
          // This applies to all verb types (suru, ichidan, godan)
          if (best.stem.size() >= core::kTwoJapaneseCharBytes) {
            auto stem_codepoints = normalize::utf8::decode(best.stem);
            if (!stem_codepoints.empty() && isPrefixLikeKanji(stem_codepoints[0])) {
              // Heavy penalty to force split
              base_cost += 3.0F;
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
          }
          // Penalty for verb candidates containing みたい suffix
          // みたい is a na-adjective (like ~, seems ~), not a verb suffix
          // E.g., 猫みたい should be 猫 + みたい, not VERB 猫む
          if (utf8::contains(surface, "みたい")) {
            base_cost += verb_opts.penalty_single_char;
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
          bool has_suffix = in_dict || recognized_ichidan;
          SUZUME_DEBUG_BLOCK {
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
              grammar::verbTypeToString(best.verb_type).data()));
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
        auto all_cands = inflection.analyze(surface);
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
        if (!prefer_suru && !prefer_godan && ichidan_cand.confidence > conf_threshold) {
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
      // Skip suru-verb passive pattern: 2+ kanji + さ + れ
      // e.g., 処理される should be 処理(noun) + される(aux), not godan passive
      // This check applies when: kanji_part is 2+ kanji AND first_hira is さ
      std::string kanji_check = extractSubstring(codepoints, start_pos, kanji_end);
      bool is_suru_passive_pattern = (first_hira == U'さ' &&
                                      kanji_check.size() >= core::kTwoJapaneseCharBytes &&
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
      auto all_candidates = inflection.analyze(passive_base);
      float ichidan_confidence = 0.0F;
      for (const auto& cand : all_candidates) {
        if (cand.verb_type == grammar::VerbType::Ichidan && cand.confidence >= 0.4F) {
          ichidan_confidence = std::max(ichidan_confidence, cand.confidence);
        }
      }

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
        float base_cost = verb_opts.bonus_ichidan + (1.0F - ichidan_confidence) * verb_opts.confidence_cost_scale_small;

        // Skip renyokei candidate for べき patterns
        if (!is_beki_pattern) {
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, renyokei_end, base_cost, base_lemma,
              dictionary::ConjugationType::Ichidan,
              false, CandidateOrigin::VerbKanji, ichidan_confidence, "godan_passive_renyokei"));
        }

        // Also generate conjugated forms of the passive verb:
        // 言われる (dictionary), 言われた (past), 言われて (te-form), 言われない (negative)
        // These should be single tokens with lemma = passive base form
        static const std::vector<std::pair<std::string, std::string>> passive_suffixes = {
            {"る", "godan_passive_dict"},      // 言われる
            {"た", "godan_passive_past"},      // 言われた
            {"て", "godan_passive_te"},        // 言われて
            {"ない", "godan_passive_neg"},     // 言われない
        };
        for (const auto& [suffix, pattern_name] : passive_suffixes) {
          size_t suffix_len = normalize::utf8Length(suffix);
          size_t conj_end = renyokei_end + suffix_len;
          if (conj_end <= hiragana_end) {
            std::string conj_surface = extractSubstring(codepoints, start_pos, conj_end);
            // Verify the suffix matches
            if (utf8::endsWith(conj_surface, suffix)) {
              // Lower cost than renyokei to prefer complete forms
              candidates.push_back(makeVerbCandidate(
                  conj_surface, start_pos, conj_end, base_cost - 0.1F, base_lemma,
                  dictionary::ConjugationType::Ichidan,
                  true, CandidateOrigin::VerbKanji, ichidan_confidence, pattern_name.c_str()));
            }
          }
        }
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
        auto infl_result = inflection.getBest(base_form);
        is_valid_verb = infl_result.confidence >= 0.3F &&
                        infl_result.verb_type == grammar::VerbType::Ichidan;
      }

      if (is_valid_verb) {
        // Negative cost to beat single-verb inflection path (which gets optimal_length -0.5 bonus)
        constexpr float kCost = -0.5F;
        SUZUME_DEBUG_BLOCK {
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
      // Check if followed by polite ます or negative ない
      using namespace suzume::core::hiragana;
      char32_t h1 = codepoints[kanji_end];
      char32_t h2 = (kanji_end + 1 < codepoints.size()) ? codepoints[kanji_end + 1] : 0;
      bool is_polite_aux = (h1 == kMa && h2 == kSu);
      bool is_negative_aux = (h1 == kNa && h2 == kI);

      if (is_polite_aux || is_negative_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = -0.5F;  // Strong bonus to beat NOUN candidate
        SUZUME_DEBUG_BLOCK {
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
        constexpr float kCost = -0.8F;  // Strong bonus to beat unified dictionary entry
        SUZUME_DEBUG_BLOCK {
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
        constexpr float kCost = -0.8F;  // Strong bonus to beat unified contraction entry
        SUZUME_DEBUG_BLOCK {
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
        constexpr float kCost = -0.8F;  // Strong bonus to beat godan mizenkei interpretation
        SUZUME_DEBUG_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " single_kanji_ichidan_rareru lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_rareru"));
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
        if (is_beki_pattern || is_nu_pattern || is_passive_pattern) {
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
              bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
              if (!is_valid_verb) {
                // Check if inflection analyzer recognizes this as a valid verb
                auto infl_result = inflection.getBest(base_form);
                is_valid_verb = infl_result.confidence > 0.5F &&
                                vh::isGodanVerbType(infl_result.verb_type);
              }

              if (is_valid_verb) {
                std::string surface = extractSubstring(codepoints, start_pos, mizenkei_end);
                // Cost varies by pattern:
                // - ぬ pattern: negative cost (-0.5F) to beat combined verb form
                //   揃わぬ(VERB) gets ~-0.1 total, so split needs lower cost
                // - passive pattern: negative cost (-0.5F) for MeCab-compatible split
                //   言われる(VERB) gets ~0.15, so split (言わ+れる) needs lower cost
                // - べき pattern: moderate cost (0.2F) for classical obligation
                float cost = 0.2F;  // default for beki
                if (is_nu_pattern) {
                  cost = -0.5F;
                } else if (is_passive_pattern) {
                  cost = -0.5F;
                }
                const char* debug_pattern = is_nu_pattern ? "nu" :
                                            is_passive_pattern ? "passive" : "beki";
                SUZUME_DEBUG_BLOCK {
                  SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                      << " godan_mizenkei lemma=" << base_form
                                      << " cost=" << cost
                                      << " pattern=" << debug_pattern
                                      << "\n";
                }
                const char* info_pattern = is_nu_pattern ? "godan_mizenkei_nu" :
                                           is_passive_pattern ? "godan_mizenkei_passive" :
                                           "godan_mizenkei";
                candidates.push_back(makeVerbCandidate(
                    surface, start_pos, mizenkei_end, cost, base_form,
                    grammar::verbTypeToConjType(verb_type),
                    true, CandidateOrigin::VerbKanji, 0.9F, info_pattern));
              }
            }
            }  // else (not Suru verb pattern)
          }
        }
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
        std::vector<std::pair<grammar::VerbType, std::string_view>> candidates_to_try;
        if (is_hatsuonbin) {
          candidates_to_try = {
            {grammar::VerbType::GodanMa, "む"},
            {grammar::VerbType::GodanBa, "ぶ"},
            {grammar::VerbType::GodanNa, "ぬ"},
          };
        } else {
          candidates_to_try = {
            {grammar::VerbType::GodanKa, "く"},
            {grammar::VerbType::GodanGa, "ぐ"},
          };
        }
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
          auto infl_results = inflection.analyze(full_surface);
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
          constexpr float kOnbinCost = -0.5F;  // Negative cost to beat unsplit forms
          SUZUME_DEBUG_BLOCK {
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

  // Add emphatic variants (来た → 来たっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints);

  // Sort by cost and return best candidates
  vh::sortCandidatesByCost(candidates);

  return candidates;
}

}  // namespace suzume::analysis
