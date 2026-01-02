/**
 * @file verb_candidates.cpp
 * @brief Verb-based unknown word candidate generation
 */

#include "verb_candidates.h"

#include <algorithm>
#include <cmath>

#include "analysis/scorer_constants.h"
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

namespace {

// =============================================================================
// Common Utility Helpers
// =============================================================================

/**
 * @brief Check if a base form exists in dictionary as a verb
 *
 * @param dict_manager Dictionary manager
 * @param base_form The base form (lemma) to check
 * @return true if the base form is registered as a verb in dictionary
 */
inline bool isVerbInDictionary(const dictionary::DictionaryManager* dict_manager,
                               std::string_view base_form) {
  if (dict_manager == nullptr || base_form.empty()) {
    return false;
  }
  auto results = dict_manager->lookup(base_form, 0);
  for (const auto& result : results) {
    if (result.entry != nullptr && result.entry->surface == base_form &&
        result.entry->pos == core::PartOfSpeech::Verb) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Check if a surface has a non-verb entry in dictionary
 *
 * @param dict_manager Dictionary manager
 * @param surface The surface form to check
 * @return true if there's a non-verb entry for this surface
 */
inline bool hasNonVerbDictionaryEntry(
    const dictionary::DictionaryManager* dict_manager,
    std::string_view surface) {
  if (dict_manager == nullptr) {
    return false;
  }
  auto results = dict_manager->lookup(surface, 0);
  for (const auto& result : results) {
    if (result.entry != nullptr && result.entry->surface == surface &&
        result.entry->pos != core::PartOfSpeech::Verb) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Sort candidates by cost (lowest cost first)
 */
inline void sortCandidatesByCost(std::vector<UnknownCandidate>& candidates) {
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });
}

// =============================================================================
// Pattern Checking Helpers (extracted for maintainability)
// =============================================================================

/**
 * @brief Check if surface ends with ます auxiliary patterns
 *
 * Skip verb + dictionary auxiliary combinations (ます, ました, ましょう)
 * e.g., "食べます" should split into "食べ" + "ます" (dictionary AUX)
 * EXCEPTION: Don't skip suru-verb passive/causative patterns
 * e.g., "開催されました" should be a single VERB token
 *
 * @param surface Verb surface form
 * @param verb_type Detected verb type from inflection analysis
 * @return true if this pattern should be skipped
 */
inline bool shouldSkipMasuAuxPattern(std::string_view surface,
                                     grammar::VerbType verb_type) {
  if (surface.size() < core::kTwoJapaneseCharBytes) {
    return false;
  }

  // Check if surface ends with ます/ました/ましょう/ません
  bool has_masu_aux = false;
  if (surface.size() >= core::kFourJapaneseCharBytes &&
      surface.substr(surface.size() - core::kFourJapaneseCharBytes) ==
          "ましょう") {
    has_masu_aux = true;
  } else if (surface.size() >= core::kThreeJapaneseCharBytes &&
             (surface.substr(surface.size() - core::kThreeJapaneseCharBytes) ==
                  "ました" ||
              surface.substr(surface.size() - core::kThreeJapaneseCharBytes) ==
                  "ません")) {
    has_masu_aux = true;
  } else if (surface.size() >= core::kTwoJapaneseCharBytes &&
             surface.substr(surface.size() - core::kTwoJapaneseCharBytes) ==
                 "ます") {
    has_masu_aux = true;
  }

  if (!has_masu_aux) {
    return false;
  }

  // Don't skip suru-verb passive/causative patterns (され, させ)
  // These should be recognized as single verb tokens
  bool is_suru_passive_causative =
      (verb_type == grammar::VerbType::Suru &&
       (surface.find("され") != std::string::npos ||
        surface.find("させ") != std::string::npos));

  return !is_suru_passive_causative;
}

/**
 * @brief Check if surface ends with そう auxiliary patterns
 *
 * Skip verb + そう auxiliary combinations (様態の助動詞)
 * e.g., "話しそう" should split into "話し" (VERB) + "そう" (AUX/ADVERB)
 * This follows the 動詞+助動詞 → 分割 tokenization rule
 *
 * @param surface Verb surface form
 * @param verb_type Detected verb type from inflection analysis
 * @return true if this pattern should be skipped
 */
inline bool shouldSkipSouPattern(std::string_view surface,
                                 grammar::VerbType verb_type) {
  if (surface.size() < core::kTwoJapaneseCharBytes) {
    return false;
  }

  bool has_sou_pattern = false;

  // Check for そう at end
  if (surface.substr(surface.size() - core::kTwoJapaneseCharBytes) ==
      scorer::kSuffixSou) {
    has_sou_pattern = true;
  }
  // Check for そうです at end (そう + です)
  constexpr const char* kSouDesu = "そうです";
  if (surface.size() >= core::kFourJapaneseCharBytes &&
      surface.substr(surface.size() - core::kFourJapaneseCharBytes) ==
          kSouDesu) {
    has_sou_pattern = true;
  }
  // Check for そうだ at end (そう + だ)
  constexpr const char* kSouDa = "そうだ";
  if (surface.size() >= core::kThreeJapaneseCharBytes &&
      surface.substr(surface.size() - core::kThreeJapaneseCharBytes) ==
          kSouDa) {
    has_sou_pattern = true;
  }

  // Don't skip i-adjective patterns (handled by adjective candidate generation)
  return has_sou_pattern && verb_type != grammar::VerbType::IAdjective;
}

/**
 * @brief Check if surface contains compound adjective patterns
 *
 * Patterns like verb renyoukei + にくい/やすい/がたい should be ADJECTIVE
 * e.g., 使いにくい should be ADJ, not VERB
 *
 * @param surface Verb surface form
 * @return true if this is a compound adjective pattern
 */
inline bool isCompoundAdjectivePattern(std::string_view surface) {
  if (surface.size() < core::kFourJapaneseCharBytes) {
    return false;
  }
  return surface.find("にくい") != std::string::npos ||
         surface.find("にくく") != std::string::npos ||
         surface.find("にくか") != std::string::npos ||
         surface.find("やすい") != std::string::npos ||
         surface.find("やすく") != std::string::npos ||
         surface.find("やすか") != std::string::npos ||
         surface.find("がたい") != std::string::npos ||
         surface.find("がたく") != std::string::npos;
}

/**
 * @brief Check if verb type is a Godan verb
 * @param verb_type The verb type to check
 * @return true if the verb type is any Godan conjugation class
 */
inline bool isGodanVerbType(grammar::VerbType verb_type) {
  return verb_type == grammar::VerbType::GodanKa ||
         verb_type == grammar::VerbType::GodanGa ||
         verb_type == grammar::VerbType::GodanSa ||
         verb_type == grammar::VerbType::GodanTa ||
         verb_type == grammar::VerbType::GodanNa ||
         verb_type == grammar::VerbType::GodanMa ||
         verb_type == grammar::VerbType::GodanBa ||
         verb_type == grammar::VerbType::GodanRa ||
         verb_type == grammar::VerbType::GodanWa;
}

/**
 * @brief Check if surface contains passive/potential auxiliary patterns
 *
 * For auxiliary separation, we want to generate verb stem (mizenkei) + auxiliary
 * instead of combined form. Skip patterns like かれべき, かれる, etc.
 * This allows the Viterbi algorithm to find the split path with auxiliary entries.
 *
 * @param surface Verb surface form
 * @param verb_type Detected verb type from inflection analysis
 * @return true if this pattern should be skipped (generate stem only)
 */
inline bool shouldSkipPassiveAuxPattern(std::string_view surface,
                                        grammar::VerbType verb_type) {
  // For auxiliary separation, skip ALL verb candidates ending with passive patterns
  // This allows the split path (verb mizenkei + passive aux) to win
  // Note: We check all verb types because the inflection analyzer may misidentify
  // patterns like 書かれべき as Ichidan (書かる) instead of Godan (書く + れべき)

  // Skip patterns containing classical passive + べき (e.g., 書かれべき, 読まれべき)
  // Must check this first before checking れる/れた patterns
  if (surface.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view last3 =
        surface.substr(surface.size() - core::kThreeJapaneseCharBytes);
    if (last3 == "れべき") {
      return true;
    }
  }

  // Only apply remaining checks to Godan verbs
  // Ichidan passive uses られる (not れる), which is handled by られべき check
  if (!isGodanVerbType(verb_type)) {
    return false;
  }

  // Passive patterns: れる, れた, れて, れない, れべき
  // These follow mizenkei (A-row + れ)
  if (surface.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view last3 =
        surface.substr(surface.size() - core::kThreeJapaneseCharBytes);
    if (last3 == "れる" || last3 == "れた" || last3 == "れて") {
      return true;
    }
  }
  if (surface.size() >= core::kFourJapaneseCharBytes) {
    std::string_view last4 =
        surface.substr(surface.size() - core::kFourJapaneseCharBytes);
    if (last4 == "れない" || last4 == "れます") {
      return true;
    }
  }
  // Check for passive + desiderative: れたい, れたく
  if (surface.size() >= core::kFourJapaneseCharBytes) {
    std::string_view last4 =
        surface.substr(surface.size() - core::kFourJapaneseCharBytes);
    if (last4 == "れたい" || last4 == "れたく") {
      return true;
    }
  }
  return false;
}

/**
 * @brief Check if surface contains causative auxiliary patterns
 *
 * For auxiliary separation, skip verb forms containing causative せる/させる
 * to allow the split path (mizenkei + せる/させる) to win.
 *
 * EXCEPTION: Suru verb passive/causative patterns (され, させ) should NOT be
 * skipped - they should be recognized as single verb tokens (e.g., 勉強される).
 *
 * @param surface Verb surface form
 * @param verb_type Detected verb type from inflection analysis
 * @return true if this pattern should be skipped (generate stem only)
 */
inline bool shouldSkipCausativeAuxPattern(std::string_view surface,
                                          grammar::VerbType verb_type) {
  // Suru verb causative/passive: させる, される, etc.
  // These should remain as single tokens (勉強される, 勉強させる)
  // So we do NOT skip them - return false for suru verbs
  if (verb_type == grammar::VerbType::Suru) {
    return false;  // Keep suru verb passive/causative as single token
  }

  // Godan causative: せる, せた (after mizenkei)
  if (surface.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view last3 =
        surface.substr(surface.size() - core::kThreeJapaneseCharBytes);
    if (last3 == "せる" || last3 == "せた" || last3 == "せて") {
      // Only skip for godan patterns
      // Need to verify it's not a verb base form like 見せる (ichidan)
      if (isGodanVerbType(verb_type)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

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
  size_t kanji1_end = start_pos;
  while (kanji1_end < char_types.size() &&
         kanji1_end - start_pos < 3 &&
         char_types[kanji1_end] == normalize::CharType::Kanji) {
    ++kanji1_end;
  }

  if (kanji1_end == start_pos || kanji1_end >= char_types.size()) {
    return candidates;
  }

  // Find first hiragana portion (1-3 chars, typically verb renyoukei ending)
  if (char_types[kanji1_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  size_t hira1_end = kanji1_end;
  while (hira1_end < char_types.size() &&
         hira1_end - kanji1_end < 4 &&
         char_types[hira1_end] == normalize::CharType::Hiragana) {
    ++hira1_end;
  }

  // Find second kanji portion (must exist for compound verb)
  if (hira1_end >= char_types.size() ||
      char_types[hira1_end] != normalize::CharType::Kanji) {
    return candidates;
  }

  size_t kanji2_end = hira1_end;
  while (kanji2_end < char_types.size() &&
         kanji2_end - hira1_end < 3 &&
         char_types[kanji2_end] == normalize::CharType::Kanji) {
    ++kanji2_end;
  }

  // Find second hiragana portion (conjugation ending)
  if (kanji2_end >= char_types.size() ||
      char_types[kanji2_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  size_t hira2_end = kanji2_end;
  while (hira2_end < char_types.size() &&
         hira2_end - kanji2_end < 10 &&
         char_types[hira2_end] == normalize::CharType::Hiragana) {
    ++hira2_end;
  }

  // Try different ending lengths
  for (size_t end_pos = hira2_end; end_pos > kanji2_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    if (surface.empty()) {
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

        // Verify conjugation type matches
        auto dict_verb_type =
            grammar::conjTypeToVerbType(result.entry->conj_type);
        if (dict_verb_type == infl_cand.verb_type) {
          // Found a match! Generate candidate
          UnknownCandidate cand;
          cand.surface = surface;
          cand.start = start_pos;
          cand.end = end_pos;
          cand.pos = core::PartOfSpeech::Verb;
          // Low cost to prefer dictionary-verified compound verbs
          cand.cost = verb_opts.base_cost_low;
          cand.has_suffix = false;
          // Note: Don't set lemma here - let lemmatizer derive it more accurately
          // Only set conj_type for conjugation pattern info
          cand.conj_type = grammar::verbTypeToConjType(infl_cand.verb_type);
#ifdef SUZUME_DEBUG_INFO
          cand.origin = CandidateOrigin::VerbCompound;
          cand.confidence = infl_cand.confidence;
          cand.pattern = grammar::verbTypeToString(infl_cand.verb_type);
#endif
          candidates.push_back(cand);
          return candidates;  // Return first valid match
        }
      }
    }
  }

  return candidates;
}

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
    // Exception: A-row hiragana followed by れべき may be mizenkei pattern
    // e.g., 泳がれべき = 泳が (mizenkei) + れべき (passive + classical obligation)
    // Note: We only allow べき patterns here, not standard passive (れる)
    // because patterns like 金がない should remain NOUN + PARTICLE + ADJ
    bool is_beki_pattern = false;
    if (grammar::isARowCodepoint(first_hiragana)) {
      size_t next_pos = kanji_end + 1;
      if (next_pos + 2 < codepoints.size()) {
        // Check for れべき pattern
        if (codepoints[next_pos] == U'れ' &&
            codepoints[next_pos + 1] == U'べ' &&
            codepoints[next_pos + 2] == U'き') {
          is_beki_pattern = true;
        }
      }
    }
    if (!is_beki_pattern) {
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
        if (cand.stem == expected_stem &&
            cand.confidence > verb_opts.confidence_standard &&
            cand.verb_type != grammar::VerbType::IAdjective) {
          // Check if this candidate's base_form exists in dictionary
          bool in_dict = isVerbInDictionary(dict_manager, cand.base_form);

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

      // Use dictionary-verified candidate if available and has reasonable confidence
      if (dict_verified_best.confidence > verb_opts.confidence_standard) {
        best = dict_verified_best;
      }

      // Only proceed if we found a matching candidate
      if (best.confidence > verb_opts.confidence_standard) {
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
                   surface.find("られ") != std::string::npos);
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
          if (shouldSkipMasuAuxPattern(surface, best.verb_type)) {
            continue;  // Skip - let the split (verb + dictionary aux) win
          }

          // Skip verb + そう auxiliary patterns
          if (shouldSkipSouPattern(surface, best.verb_type)) {
            continue;  // Skip - let the split (verb renyokei + そう) win
          }

          // Skip verb + passive auxiliary patterns (れる, れた, etc.)
          // For auxiliary separation: 書かれる → 書か + れる
          if (shouldSkipPassiveAuxPattern(surface, best.verb_type)) {
            continue;  // Skip - let the split (verb mizenkei + passive aux) win
          }

          // Skip verb + causative auxiliary patterns (せる, させる, etc.)
          // For auxiliary separation: 書かせる → 書か + せる
          if (shouldSkipCausativeAuxPattern(surface, best.verb_type)) {
            continue;  // Skip - let the split (verb mizenkei + causative aux) win
          }

          UnknownCandidate candidate;
          candidate.surface = surface;
          candidate.start = start_pos;
          candidate.end = end_pos;
          candidate.pos = core::PartOfSpeech::Verb;
          // Lower cost for higher confidence matches
          float base_cost = verb_opts.base_cost_standard + (1.0F - best.confidence) * verb_opts.confidence_cost_scale;
          // Suru verbs get a bonus to recognize as single verb (勉強する, 延期する)
          // BUT don't give bonus if stem ends with し and is short (1-2 kanji)
          // because that pattern is likely a GodanSa renyokei, not a Suru stem
          // e.g., 押しする (stem=押し) shouldn't get bonus - 押し is renyokei of 押す
          if (best.verb_type == grammar::VerbType::Suru) {
            bool is_likely_godan_renyokei = false;
            // Check if stem is short (≤2 kanji+1 hiragana = 9 bytes) and ends with し
            if (!best.stem.empty() &&
                best.stem.size() <= core::kThreeJapaneseCharBytes &&
                best.stem.size() >= core::kJapaneseCharBytes &&
                best.stem.substr(best.stem.size() - core::kJapaneseCharBytes) == "し") {
              is_likely_godan_renyokei = true;
            }
            if (!is_likely_godan_renyokei) {
              base_cost += verb_opts.bonus_dict_match;
            }
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
          bool is_comp_adj = isCompoundAdjectivePattern(surface);
          bool in_dict = isVerbInDictionary(dict_manager, best.base_form);
          if (!is_comp_adj && in_dict) {
            // Found in dictionary - give strong bonus
            base_cost = verb_opts.base_cost_verified +
                        (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
          }
          // Penalty for verb candidates containing みたい suffix
          // みたい is a na-adjective (like ~, seems ~), not a verb suffix
          // E.g., 猫みたい should be 猫 + みたい, not VERB 猫む
          if (surface.find("みたい") != std::string::npos) {
            base_cost += verb_opts.penalty_single_char;
          }
          candidate.cost = base_cost;
          // Set has_suffix for dict-verified candidates to skip exceeds_dict_length penalty
          // in tokenizer.cpp (the base form exists in dictionary as a verb)
          candidate.has_suffix = in_dict;
          SUZUME_DEBUG_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                << " base=" << best.base_form
                                << " cost=" << base_cost
                                << " in_dict=" << in_dict
                                << " has_suffix=" << candidate.has_suffix << "\n";
          }
          // Don't set lemma here - let lemmatizer derive it with dictionary verification
          // The lemmatizer will use stem-matching logic to pick the correct base form
          candidate.conj_type = grammar::verbTypeToConjType(best.verb_type);
#ifdef SUZUME_DEBUG_INFO
          candidate.origin = CandidateOrigin::VerbKanji;
          candidate.confidence = best.confidence;
          candidate.pattern = grammar::verbTypeToString(best.verb_type);
#endif
          candidates.push_back(candidate);
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
      // Surface is kanji + first e/i-row hiragana only (e.g., 食べ from 食べます, 感じ from 感じる)
      size_t renyokei_end = kanji_end + 1;
      std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);
      auto best = inflection.getBest(surface);
      if (best.confidence > verb_opts.confidence_standard && best.verb_type == grammar::VerbType::Ichidan) {
        UnknownCandidate candidate;
        candidate.surface = surface;
        candidate.start = start_pos;
        candidate.end = renyokei_end;
        candidate.pos = core::PartOfSpeech::Verb;
        // Negative cost to strongly favor split over combined analysis
        // Combined forms get optimal_length bonus (-0.5), so we need to be lower
        float base_cost = verb_opts.bonus_ichidan + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_small;
        candidate.cost = base_cost;
        candidate.has_suffix = false;
        candidate.conj_type = grammar::verbTypeToConjType(best.verb_type);
#ifdef SUZUME_DEBUG_INFO
        candidate.origin = CandidateOrigin::VerbKanji;
        candidate.confidence = best.confidence;
        candidate.pattern = "ichidan_renyokei";
#endif
        candidates.push_back(candidate);
      }
    }
  }

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
      // Common single-kanji Ichidan verbs:
      // 見(みる), 居(いる), 着(きる), 寝(ねる), 煮(にる), 似(にる)
      // 経(へる), 干(ひる), 射(いる), 得(える/うる)
      bool is_single_kanji_ichidan =
          (kanji_char == U'見' || kanji_char == U'居' || kanji_char == U'着' ||
           kanji_char == U'寝' || kanji_char == U'煮' || kanji_char == U'似' ||
           kanji_char == U'経' || kanji_char == U'干' || kanji_char == U'射' ||
           kanji_char == U'得');
      if (is_single_kanji_ichidan) {
        // Check for られ suffix right after the single kanji
        if (kanji_end + 1 < codepoints.size() &&
            codepoints[kanji_end] == U'ら' &&
            codepoints[kanji_end + 1] == U'れ') {
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
      bool is_valid_verb = isVerbInDictionary(dict_manager, base_form);
      if (!is_valid_verb) {
        // Check if inflection analyzer recognizes this as Ichidan verb
        // Use >= threshold to include edge cases like 信じる (conf=0.3)
        auto infl_result = inflection.getBest(base_form);
        is_valid_verb = infl_result.confidence >= 0.3F &&
                        infl_result.verb_type == grammar::VerbType::Ichidan;
      }

      if (is_valid_verb) {
        UnknownCandidate candidate;
        candidate.surface = surface;
        candidate.start = start_pos;
        candidate.end = stem_end;
        candidate.pos = core::PartOfSpeech::Verb;
        // Negative cost to beat single-verb inflection path (which gets optimal_length -0.5 bonus)
        // For complex aux chains like られ+なくて, need more aggressive bonus
        // Split path: stem(-0.5) + pos(0.2) + aux_chain ≈ -0.5
        // Single path: base(~0.1) + pos(0.2) + optimal_length(-0.5) ≈ -0.2
        candidate.cost = -0.5F;
        candidate.has_suffix = true;  // Skip exceeds_dict_length penalty
        candidate.lemma = base_form;
        candidate.conj_type = grammar::verbTypeToConjType(grammar::VerbType::Ichidan);
        SUZUME_DEBUG_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << " ichidan_stem_rare lemma=" << base_form
                              << " cost=" << candidate.cost << "\n";
        }
#ifdef SUZUME_DEBUG_INFO
        candidate.origin = CandidateOrigin::VerbKanji;
        candidate.confidence = 0.9F;
        candidate.pattern = "ichidan_stem_rare";
#endif
        candidates.push_back(candidate);
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
        // Only generate mizenkei candidates for classical べき patterns
        // E.g., 書かれべき, 読まれべき (not 書かれる, 読まれる)
        // Standard passive (れる/られる) should remain as compound tokens
        bool is_beki_pattern = false;
        if (next_char == U'れ' && mizenkei_end + 2 < codepoints.size()) {
          // Check for れべき pattern
          if (codepoints[mizenkei_end + 1] == U'べ' &&
              codepoints[mizenkei_end + 2] == U'き') {
            is_beki_pattern = true;
          }
        }
        if (is_beki_pattern) {
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
              bool is_valid_verb = isVerbInDictionary(dict_manager, base_form);
              if (!is_valid_verb) {
                // Check if inflection analyzer recognizes this as a valid verb
                auto infl_result = inflection.getBest(base_form);
                is_valid_verb = infl_result.confidence > 0.5F &&
                                isGodanVerbType(infl_result.verb_type);
              }

              if (is_valid_verb) {
                std::string surface = extractSubstring(codepoints, start_pos, mizenkei_end);
                UnknownCandidate candidate;
                candidate.surface = surface;
                candidate.start = start_pos;
                candidate.end = mizenkei_end;
                candidate.pos = core::PartOfSpeech::Verb;
                // Low cost for dictionary-verified mizenkei candidates
                candidate.cost = 0.2F;
                candidate.has_suffix = false;
                candidate.lemma = base_form;  // Set lemma to base form
                candidate.conj_type = grammar::verbTypeToConjType(verb_type);
#ifdef SUZUME_DEBUG_INFO
                candidate.origin = CandidateOrigin::VerbKanji;
                candidate.confidence = 0.9F;  // High confidence for verified mizenkei
                candidate.pattern = "godan_mizenkei";
#endif
                candidates.push_back(candidate);
              }
            }
            }  // else (not Suru verb pattern)
          }
        }
      }
    }
  }

  // Sort by cost and return best candidates
  sortCandidatesByCost(candidates);

  return candidates;
}

std::vector<UnknownCandidate> generateHiraganaVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager,
    const VerbCandidateOptions& verb_opts) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Skip if starting character is a particle that is NEVER a verb stem
  // Note: Characters that CAN be verb stems are NOT skipped:
  //   - な→なる/なくす, て→できる, や→やる, か→かける/かえる
  char32_t first_char = codepoints[start_pos];
  if (normalize::isNeverVerbStemAtStart(first_char)) {
    return candidates;
  }

  // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
  // These are commonly mistaken for verbs (これる, それる, etc.)
  // Exception: あれば is the conditional form of ある (verb), not pronoun + particle
  if (start_pos + 1 < codepoints.size()) {
    char32_t second_char = codepoints[start_pos + 1];
    if (normalize::isDemonstrativeStart(first_char, second_char)) {
      // Check if followed by conditional ば - if so, it might be verb conditional form
      // E.g., あれば = ある (verb) + ば, not あれ (pronoun) + ば
      bool is_conditional_form = (start_pos + 2 < codepoints.size() &&
                                  codepoints[start_pos + 2] == U'ば');
      if (!is_conditional_form) {
        return candidates;
      }
    }

    // Skip if starting with 「ない」(auxiliary verb/i-adjective for negation)
    // These should be recognized as AUX by dictionary, not as hiragana verbs.
    // E.g., 「ないんだ」→「ない」+「んだ」, not a single verb「ないむ」
    if (first_char == U'な' && second_char == U'い') {
      return candidates;
    }
  }

  // Find hiragana sequence, breaking at particle boundaries
  // Note: Be careful not to break at characters that are part of verb conjugations:
  //   - か can be part of なかった (negative past) or かった (i-adj past)
  //   - で can be part of んで (te-form for godan) or できる (potential verb)
  //   - も can be part of ても (even if) or もらう (receiving verb)
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() &&
         hiragana_end - start_pos < 12 &&  // Max 12 hiragana for verb + endings
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Don't include particles that appear after the first hiragana character.
    // E.g., for "りにする", stop at "り" to not include "にする".
    if (hiragana_end > start_pos) {
      char32_t curr = codepoints[hiragana_end];

      // Check for particle-like characters (common particles + も, や)
      if (normalize::isNeverVerbStemAfterKanji(curr)) {
        break;  // These are always particles in this context
      }

      // For か, で, も, と: check if they're part of verb conjugation patterns
      // Don't break if they appear in known conjugation contexts
      if (curr == U'か' || curr == U'で' || curr == U'も' || curr == U'と') {
        // Check the preceding character for conjugation patterns
        char32_t prev = codepoints[hiragana_end - 1];

        // か: OK if preceded by な (なかった = negative past)
        //    Also OK if followed by れ (かれ = ichidan stem like つかれる, ふざける)
        if (curr == U'か') {
          if (prev == U'な') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by れ (ichidan stem pattern)
          if (hiragana_end + 1 < codepoints.size() &&
              codepoints[hiragana_end + 1] == U'れ') {
            ++hiragana_end;
            continue;
          }
        }

        // で: OK if preceded by ん (んで = te-form) or き (できる)
        if (curr == U'で' && (prev == U'ん' || prev == U'き')) {
          ++hiragana_end;
          continue;
        }

        // も: OK if preceded by て (ても = even if)
        if (curr == U'も' && prev == U'て') {
          ++hiragana_end;
          continue;
        }

        // と: OK if preceded by っ (っとく = ておく contraction)
        // やっとく = やって + おく where ておく → とく
        if (curr == U'と' && prev == U'っ') {
          ++hiragana_end;
          continue;
        }

        // Otherwise, treat as particle
        break;
      }
    }
    ++hiragana_end;
  }

  // Need at least 2 hiragana for a verb
  if (hiragana_end <= start_pos + 1) {
    return candidates;
  }

  // Try different lengths, starting from longest
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 1; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Check if this looks like a conjugated verb
    // First try the best match, but also check all candidates for dictionary verbs
    auto all_candidates = inflection.analyze(surface);
    grammar::InflectionCandidate best;
    bool is_dictionary_verb = false;

    // Look through all candidates to find ones whose base form is in the dictionary
    // Collect all matches and select the best one based on:
    // 1. Higher confidence
    // 2. GodanWa > GodanRa/GodanTa when tied (う verbs are much more common for hiragana)
    // This helps with cases like しまった where しまう (GodanWa) should beat しまる (GodanRa)
    if (dict_manager != nullptr) {
      std::vector<grammar::InflectionCandidate> dict_matches;

      for (const auto& cand : all_candidates) {
        if (cand.verb_type == grammar::VerbType::IAdjective ||
            cand.base_form.empty()) {
          continue;
        }
        if (isVerbInDictionary(dict_manager, cand.base_form)) {
          // Found a dictionary verb - collect this candidate
          dict_matches.push_back(cand);
        }
      }

      // Select the best dictionary match
      if (!dict_matches.empty()) {
        is_dictionary_verb = true;
        best = dict_matches[0];

        for (size_t i = 1; i < dict_matches.size(); ++i) {
          const auto& cand = dict_matches[i];
          // Higher confidence wins
          if (cand.confidence > best.confidence + 0.01F) {
            best = cand;
          } else if (std::abs(cand.confidence - best.confidence) <= 0.01F) {
            // When confidence is tied (within 0.01), prefer GodanWa over GodanRa/GodanTa
            // Rationale: For pure hiragana stems, う verbs (しまう, あらう, かう) are
            // much more common than る/つ verbs with the same stem pattern.
            // GodanRa: rare for pure hiragana (most are kanji: 走る, 帰る)
            // GodanTa: rare (持つ, 勝つ, etc. - usually with kanji)
            // GodanWa: very common in hiragana (しまう, あらう, まよう, etc.)
            if (cand.verb_type == grammar::VerbType::GodanWa &&
                (best.verb_type == grammar::VerbType::GodanRa ||
                 best.verb_type == grammar::VerbType::GodanTa)) {
              best = cand;
            }
          }
        }
      }
    }

    // If no dictionary match, select best candidate with GodanWa preference
    // When confidence is tied, GodanWa should beat GodanRa/GodanTa because
    // う verbs (あらう, かう, まよう) are much more common than る/つ verbs
    // for pure hiragana stems
    if (!is_dictionary_verb && !all_candidates.empty()) {
      best = all_candidates[0];
      for (size_t i = 1; i < all_candidates.size(); ++i) {
        const auto& cand = all_candidates[i];
        if (cand.verb_type == grammar::VerbType::IAdjective) {
          continue;
        }
        // Higher confidence wins
        if (cand.confidence > best.confidence + 0.01F) {
          best = cand;
        } else if (std::abs(cand.confidence - best.confidence) <= 0.01F) {
          // When confidence is tied (within 0.01), prefer GodanWa over GodanRa/GodanTa
          if (cand.verb_type == grammar::VerbType::GodanWa &&
              (best.verb_type == grammar::VerbType::GodanRa ||
               best.verb_type == grammar::VerbType::GodanTa)) {
            best = cand;
          }
        }
      }
    }

    // Filter out 2-char hiragana that don't end with valid verb endings
    // Valid endings: る (dictionary form), て/で (te-form), た/だ (past)
    // This prevents false positives like まじ, ため from being recognized as verbs
    size_t pre_filter_len = end_pos - start_pos;
    if (pre_filter_len == 2 && surface.size() >= core::kJapaneseCharBytes) {
      // Use string_view directly into surface to avoid dangling reference
      // (surface.substr() returns a temporary std::string)
      std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes,
                                  core::kJapaneseCharBytes);
      if (last_char != "る" && last_char != "て" && last_char != "で" &&
          last_char != "た" && last_char != "だ") {
        continue;  // Skip 2-char hiragana not ending with valid verb suffix
      }
    }

    // Filter out i-adjective conjugation suffixes (standalone, not verb candidates)
    // See scorer_constants.h for documentation on these patterns.
    if (surface == scorer::kIAdjPastKatta || surface == scorer::kIAdjPastKattara ||
        surface == scorer::kIAdjTeKute || surface == scorer::kIAdjNegKunai ||
        surface == scorer::kIAdjCondKereba || surface == scorer::kIAdjStemKa ||
        surface == scorer::kIAdjNegStemKuna || surface == scorer::kIAdjCondStemKere) {
      continue;  // Skip i-adjective conjugation patterns
    }

    // Note: Common adverbs/onomatopoeia (ぴったり, はっきり, etc.) are filtered
    // by the dictionary lookup below - they are registered as Adverb in L1 dictionary.

    // Filter out old kana forms (ゐ=wi, ゑ=we) that look like verbs
    // ゐる is old kana for いる (auxiliary), not a standalone verb
    if (surface.size() >= core::kJapaneseCharBytes) {
      std::string_view first_char(surface.data(), core::kJapaneseCharBytes);
      if (first_char == "ゐ" || first_char == "ゑ") {
        continue;  // Skip old kana patterns
      }
    }

    // Filter out words that exist in dictionary as non-verb entries
    // e.g., あなた (pronoun), わたし (pronoun) should not be verb candidates
    if (hasNonVerbDictionaryEntry(dict_manager, surface)) {
      continue;  // Skip - dictionary has non-verb entry for this surface
    }

    // Check for 3-4 char hiragana verb ending with た/だ (past form) BEFORE threshold check
    // e.g., つかれた (疲れた), ねむった (眠った), おきた (起きた)
    // These need lower threshold because ichidan_pure_hiragana_stem penalty reduces confidence
    size_t pre_check_len = end_pos - start_pos;
    bool looks_like_past_form = false;
    bool looks_like_te_form = false;
    if ((pre_check_len == 3 || pre_check_len == 4) &&
        surface.size() >= core::kJapaneseCharBytes) {
      std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes,
                                  core::kJapaneseCharBytes);
      if (last_char == "た" || last_char == "だ") {
        looks_like_past_form = true;
      } else if (last_char == "て" || last_char == "で") {
        // Te-form verbs (あらって, しまって, かって) need lower threshold too
        looks_like_te_form = true;
      }
    }

    // Check for ichidan dictionary form (e-row stem + る)
    // e.g., たべる (食べる), しらべる (調べる), つかれる (疲れる)
    // These need lower threshold because ichidan_pure_hiragana_stem penalty reduces confidence
    // Note: Check pattern structure directly, not verb_type, because when multiple
    // candidates have the same confidence, the godan candidate may be returned first
    // Exception: Exclude てる pattern (て + る) which is the ている contraction
    // e.g., してる should be する+ている, not しる (ichidan)
    bool looks_like_ichidan_dict_form = false;
    if (pre_check_len >= 3 && surface.size() >= core::kTwoJapaneseCharBytes) {
      std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes,
                                  core::kJapaneseCharBytes);
      if (last_char == "る" && end_pos >= 2) {
        // Check if second-to-last char is e-row hiragana (ichidan stem ending)
        char32_t stem_end = codepoints[end_pos - 2];
        if (grammar::isERowCodepoint(stem_end)) {
          // Exclude てる pattern (ている contraction) - this should be suru/godan + ている
          // not ichidan dictionary form
          bool is_te_iru_contraction = (stem_end == U'て' || stem_end == U'で');
          if (!is_te_iru_contraction) {
            // Find ichidan candidate to use for verb type and base form
            // For dictionary forms (e-row stem + る), prefer longer valid stems
            // Valid: つかれる (e-row ending), Invalid: つかれるる (るる pattern)
            grammar::InflectionCandidate best_ichidan;
            bool found_ichidan = false;
            for (const auto& cand : all_candidates) {
              if (cand.verb_type == grammar::VerbType::Ichidan &&
                  cand.confidence >= verb_opts.confidence_ichidan_dict) {
                // Skip invalid るる pattern (e.g., つかれるる)
                if (cand.base_form.size() >= 2 * core::kJapaneseCharBytes) {
                  std::string_view ending(
                      cand.base_form.data() + cand.base_form.size() - 2 * core::kJapaneseCharBytes,
                      2 * core::kJapaneseCharBytes);
                  if (ending == "るる") {
                    continue;  // Skip invalid pattern
                  }
                }
                if (!found_ichidan) {
                  best_ichidan = cand;
                  found_ichidan = true;
                } else if (cand.base_form.size() > best_ichidan.base_form.size()) {
                  // Prefer longer base form (e.g., つかれる > つかる)
                  best_ichidan = cand;
                }
              }
            }
            if (found_ichidan) {
              looks_like_ichidan_dict_form = true;
              // Use ichidan candidate as best if pattern matches
              if (best.verb_type != grammar::VerbType::Ichidan) {
                best = best_ichidan;
              } else if (best_ichidan.base_form.size() > best.base_form.size()) {
                // Even if already Ichidan, prefer longer base form
                best = best_ichidan;
              }
            }
          }
        }
      }
    }

    // Only accept verb types (not IAdjective) with sufficient confidence
    // Lower threshold for dictionary-verified verbs, past/te forms, and ichidan dict forms
    // Ichidan dict forms get very low threshold (0.28) because pure hiragana stems
    // with 3+ chars get multiple penalties (stem_long + ichidan_pure_hiragana_stem)
    float conf_threshold = is_dictionary_verb ? verb_opts.confidence_dict_verb :
                           (looks_like_past_form || looks_like_te_form) ? verb_opts.confidence_past_te :
                           looks_like_ichidan_dict_form ? verb_opts.confidence_ichidan_dict : verb_opts.confidence_standard;
    if (best.confidence > conf_threshold &&
        best.verb_type != grammar::VerbType::IAdjective) {
      // Lower cost for higher confidence matches
      float base_cost = verb_opts.base_cost_high + (1.0F - best.confidence) * verb_opts.confidence_cost_scale;

      // Give significant bonus for dictionary-verified hiragana verbs
      // This helps them beat the particle+adj+particle split path
      // Only apply to longer forms (5+ chars) to avoid boosting short forms like
      // "あった" (ある) which can interfere with copula recognition (であった)
      // Exception: Conditional forms (ending with ば) are unambiguous and should
      // get the bonus even if short (e.g., あれば = ある conditional)
      size_t candidate_len = end_pos - start_pos;
      bool is_conditional = (surface.size() >= core::kJapaneseCharBytes &&
                             surface.substr(surface.size() - core::kJapaneseCharBytes) == "ば");
      // Check for っとく pattern (ておく contraction: やっとく, 見っとく)
      // This is a common colloquial pattern that should get bonus treatment
      bool is_teoku_contraction = (surface.size() >= core::kThreeJapaneseCharBytes &&
                                   surface.substr(surface.size() - core::kThreeJapaneseCharBytes) == "っとく");
      // Check for short te/de-form (e.g., ねて, でて, みて)
      // These are 2-char hiragana verbs that need a bonus to beat particle splits
      bool is_short_te_form = false;
      if (candidate_len == 2 && best.confidence >= verb_opts.confidence_high) {
        // Check last char in bytes (UTF-8)
        // て = E3 81 A6, で = E3 81 A7
        if (surface.size() >= 3) {
          char c1 = surface[surface.size() - 3];
          char c2 = surface[surface.size() - 2];
          char c3 = surface[surface.size() - 1];
          if (c1 == '\xE3' && c2 == '\x81' && (c3 == '\xA6' || c3 == '\xA7')) {
            is_short_te_form = true;
          }
        }
      }

      // Check for 3-4 char hiragana verb ending with た/だ (past form)
      // e.g., つかれた (疲れた), ねむった (眠った), おきた (起きた)
      // These medium-length verbs need a bonus to beat particle splits like つ+か+れた
      // Note: Lower confidence threshold (0.25) because ichidan_pure_hiragana_stem penalty
      // reduces confidence significantly for pure hiragana verbs
      bool is_medium_past_form = false;
      if ((candidate_len == 3 || candidate_len == 4) && best.confidence >= verb_opts.confidence_past_te) {
        std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes,
                                    core::kJapaneseCharBytes);
        if (last_char == "た" || last_char == "だ") {
          is_medium_past_form = true;
        }
      }

      if (is_dictionary_verb &&
          (candidate_len >= 5 || is_conditional || is_teoku_contraction)) {
        base_cost = verb_opts.base_cost_verified + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
      } else if (is_short_te_form) {
        // Short te-form with high confidence: give strong bonus to beat particle splits
        // e.g., ねて (conf=0.79) should beat ね(PARTICLE) + て(PARTICLE)
        // Particle path total cost can be as low as 0.002 due to dictionary bonuses,
        // so we need negative cost to compete. After adding POS prior (0.2 for verb),
        // the total needs to be below 0.002, so base needs to be below -0.2.
        //
        // When the first char is a common particle (で, に, etc.), these particles
        // have very low cost (e.g., で: -0.4), making particle+て path even cheaper
        // (total around -0.5). Need extra strong bonus for these cases.
        bool starts_with_common_particle =
            (first_char == U'で' || first_char == U'に' || first_char == U'が' ||
             first_char == U'を' || first_char == U'は' || first_char == U'の' ||
             first_char == U'へ');
        if (starts_with_common_particle) {
          // Extra strong bonus: need to beat particle paths around -0.5
          base_cost = verb_opts.bonus_long_verified + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_small;
        } else {
          base_cost = verb_opts.bonus_long_dict + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_small;
        }
      } else if (is_medium_past_form) {
        // Medium-length past form verbs (3-4 chars ending with た/だ)
        // e.g., つかれた (conf=0.43) should beat つ+か+れた split
        // Give bonus to compete with particle splits
        base_cost = verb_opts.confidence_cost_scale_medium + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
      } else if (looks_like_ichidan_dict_form) {
        // Ichidan dictionary form (e-row stem + る)
        // e.g., たべる (conf=0.39), しらべる, つかれる
        // These are highly likely to be real verbs, give modest bonus
        // Starting with particle-like chars (た, etc.) needs stronger bonus
        bool starts_with_aux_like_char =
            (first_char == U'た' || first_char == U'で' || first_char == U'に');
        if (starts_with_aux_like_char) {
          // Extra bonus: need to beat た(AUX) + べる(AUX) split
          base_cost = verb_opts.base_cost_verified + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
        } else {
          base_cost = verb_opts.base_cost_low + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
        }
      } else if (candidate_len >= 7 && best.confidence >= verb_opts.confidence_very_high) {
        // For long hiragana verb forms (7+ chars) with high confidence,
        // give a bonus even without dictionary verification.
        // This helps forms like かけられなくなった (9 chars) beat the
        // particle+verb split path (か + けられなくなった).
        // The length requirement (7+ chars) helps avoid false positives.
        //
        // When the verb starts with a character that's commonly mistaken for
        // a particle (か, は, が, etc.), give an extra strong bonus because
        // the particle split path is very likely to compete.
        bool starts_with_particle_char =
            (first_char == U'か' || first_char == U'は' || first_char == U'が' ||
             first_char == U'を' || first_char == U'に' || first_char == U'で' ||
             first_char == U'と' || first_char == U'も' || first_char == U'へ');
        if (starts_with_particle_char) {
          // Extra strong bonus for forms starting with particle-like char
          // e.g., かけられなくなった should strongly beat か + けられなくなった
          base_cost = verb_opts.base_cost_long_verified + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_small;
        } else {
          base_cost = verb_opts.confidence_cost_scale_medium + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
        }
      }

      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Verb;
      candidate.cost = base_cost;
      candidate.has_suffix = false;
      // Set lemma from inflection analysis for pure hiragana verbs
      // This is essential for P4 (ひらがな動詞活用展開) to work without dictionary
      // The lemmatizer can't derive lemma accurately for unknown verbs
      candidate.lemma = best.base_form;
      candidate.conj_type = grammar::verbTypeToConjType(best.verb_type);
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::VerbHiragana;
      candidate.confidence = best.confidence;
      candidate.pattern = grammar::verbTypeToString(best.verb_type);
#endif
      candidates.push_back(candidate);
    }
  }

  // Sort by cost
  sortCandidatesByCost(candidates);

  return candidates;
}

std::vector<UnknownCandidate> generateKatakanaVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const VerbCandidateOptions& verb_opts) {
  std::vector<UnknownCandidate> candidates;

  // Only process katakana-starting positions
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Katakana) {
    return candidates;
  }

  // Find katakana portion (1-8 characters for slang verb stems)
  size_t kata_end = start_pos;
  while (kata_end < char_types.size() &&
         kata_end - start_pos < 8 &&
         char_types[kata_end] == normalize::CharType::Katakana) {
    ++kata_end;
  }

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
  size_t hira_end = kata_end;
  while (hira_end < char_types.size() &&
         hira_end - kata_end < 10 &&
         char_types[hira_end] == normalize::CharType::Hiragana) {
    ++hira_end;
  }

  // Need at least 1 hiragana for conjugation
  if (hira_end <= kata_end) {
    return candidates;
  }

  // Reject single-character katakana stem + すぎ pattern
  // e.g., "ンすぎた" from "ワンパターンすぎた" should not be a verb
  // This is almost always a misanalysis from incorrect tokenization boundary
  size_t kata_len = kata_end - start_pos;
  if (kata_len == 1) {
    std::string hira_part = extractSubstring(codepoints, kata_end, hira_end);
    // C++17 compatible: check if starts with "すぎ" (6 bytes)
    if (hira_part.size() >= 6 && hira_part.compare(0, 6, "すぎ") == 0) {
      return candidates;  // Skip this candidate
    }
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
    if (best.confidence > verb_opts.confidence_katakana &&
        best.verb_type != grammar::VerbType::IAdjective) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Verb;
      // Lower cost than pure katakana noun to prefer verb reading
      // Cost: 0.4-0.55 based on confidence (lower = better)
      candidate.cost = verb_opts.base_cost_standard + (1.0F - best.confidence) * verb_opts.confidence_cost_scale;
      candidate.has_suffix = false;
      candidate.lemma = best.base_form;  // Set lemma from inflection analysis
      candidate.conj_type = grammar::verbTypeToConjType(best.verb_type);
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::VerbKatakana;
      candidate.confidence = best.confidence;
      candidate.pattern = grammar::verbTypeToString(best.verb_type);
#endif
      candidates.push_back(candidate);
    }
  }

  // Sort by cost
  sortCandidatesByCost(candidates);

  return candidates;
}

}  // namespace suzume::analysis
