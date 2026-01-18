/**
 * @file adjective_candidates.cpp
 * @brief Adjective-based unknown word candidate generation
 */

#include "adjective_candidates.h"

#include <algorithm>

#include "analysis/scorer_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/patterns.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"

namespace suzume::analysis {

namespace {

// Na-adjective forming suffixes (〜的 patterns)
const std::vector<std::string_view> kNaAdjSuffixes = {
    "的",  // 理性的, 論理的, etc.
};

// Normalize prolonged sound marks (ー) to vowels based on preceding character
// e.g., すごーい → すごおい, やばーい → やばあい
// Also handles consecutive marks: すごーーい → すごおおい
std::string normalizeProlongedSoundMark(const std::vector<char32_t>& codepoints,
                                         size_t start, size_t end) {
  std::string result;
  result.reserve((end - start) * 3);  // Japanese chars are typically 3 bytes

  for (size_t i = start; i < end; ++i) {
    char32_t ch = codepoints[i];

    // Check for prolonged sound mark (ー, U+30FC)
    if (ch == 0x30FC && i > start) {
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

// Check if sequence contains a prolonged sound mark
bool containsProlongedSoundMark(const std::vector<char32_t>& codepoints,
                                 size_t start, size_t end) {
  for (size_t i = start; i < end; ++i) {
    if (codepoints[i] == 0x30FC) {
      return true;
    }
  }
  return false;
}

// Normalize the base form of an adjective by removing extra vowels created by
// prolonged sound mark normalization.
// Two patterns:
// 1. すごーい → すごおい → すごい (ー before final い)
// 2. かわいー → かわいい → かわいい (ー after い, extending the い)
// For consecutive marks:
// 1. すごーーい → すごおおい → すごい
// 2. かわいーー → かわいいい → かわいい
std::string normalizeBaseForm(const std::string& base_form,
                               const std::vector<char32_t>& original_codepoints,
                               size_t start, size_t end) {
  if (end < start + 2) {
    return base_form;
  }

  // Count total prolonged marks in the original
  size_t choon_count = 0;
  size_t first_choon_pos = 0;
  for (size_t i = start; i < end; ++i) {
    if (original_codepoints[i] == 0x30FC) {
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

// =============================================================================
// Emphatic Pattern Helpers (口語強調パターン)
// =============================================================================

/**
 * @brief Check if character is an emphatic suffix character
 *
 * Emphatic characters used in colloquial speech:
 * - Sokuon: っ, ッ
 * - Chouon: ー
 * - Small vowels: ぁぃぅぇぉ, ァィゥェォ
 */
inline bool isEmphaticChar(char32_t c) {
  return c == core::hiragana::kSmallTsu ||  // っ
         c == U'ッ' ||                       // katakana sokuon
         c == U'ー' ||                       // chouon
         // Small hiragana vowels
         c == U'ぁ' || c == U'ぃ' || c == U'ぅ' || c == U'ぇ' || c == U'ぉ' ||
         // Small katakana vowels
         c == U'ァ' || c == U'ィ' || c == U'ゥ' || c == U'ェ' || c == U'ォ';
}

/**
 * @brief Convert codepoint to UTF-8 string (3-byte Japanese chars)
 */
inline std::string codepointToUtf8(char32_t c) {
  std::string result;
  result += static_cast<char>(0xE0 | ((c >> 12) & 0x0F));
  result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
  result += static_cast<char>(0x80 | (c & 0x3F));
  return result;
}

/**
 * @brief Get the vowel character (あいうえお) for a hiragana's ending vowel
 *
 * Maps any hiragana to its vowel row character.
 * E.g., た→あ, き→い, す→う, て→え, の→お
 * Returns 0 for characters without vowels (ん, っ) or non-hiragana.
 */
inline char32_t getHiraganaVowel(char32_t c) {
  // あ-row (a-vowel)
  if (c == U'あ' || c == U'ぁ' || c == U'か' || c == U'が' || c == U'さ' ||
      c == U'ざ' || c == U'た' || c == U'だ' || c == U'な' || c == U'は' ||
      c == U'ば' || c == U'ぱ' || c == U'ま' || c == U'や' || c == U'ゃ' ||
      c == U'ら' || c == U'わ') {
    return U'あ';
  }
  // い-row (i-vowel)
  if (c == U'い' || c == U'ぃ' || c == U'き' || c == U'ぎ' || c == U'し' ||
      c == U'じ' || c == U'ち' || c == U'ぢ' || c == U'に' || c == U'ひ' ||
      c == U'び' || c == U'ぴ' || c == U'み' || c == U'り') {
    return U'い';
  }
  // う-row (u-vowel)
  if (c == U'う' || c == U'ぅ' || c == U'く' || c == U'ぐ' || c == U'す' ||
      c == U'ず' || c == U'つ' || c == U'づ' || c == U'ぬ' || c == U'ふ' ||
      c == U'ぶ' || c == U'ぷ' || c == U'む' || c == U'ゆ' || c == U'ゅ' ||
      c == U'る') {
    return U'う';
  }
  // え-row (e-vowel)
  if (c == U'え' || c == U'ぇ' || c == U'け' || c == U'げ' || c == U'せ' ||
      c == U'ぜ' || c == U'て' || c == U'で' || c == U'ね' || c == U'へ' ||
      c == U'べ' || c == U'ぺ' || c == U'め' || c == U'れ') {
    return U'え';
  }
  // お-row (o-vowel)
  if (c == U'お' || c == U'ぉ' || c == U'こ' || c == U'ご' || c == U'そ' ||
      c == U'ぞ' || c == U'と' || c == U'ど' || c == U'の' || c == U'ほ' ||
      c == U'ぼ' || c == U'ぽ' || c == U'も' || c == U'よ' || c == U'ょ' ||
      c == U'ろ' || c == U'を') {
    return U'お';
  }
  // No vowel: ん, っ, punctuation, non-hiragana
  return 0;
}

/**
 * @brief Check if position is likely part of a verb te/ta-form, not emphatic
 *
 * っ followed by て/た is almost always part of the verb te-form (e.g., いって, 行った)
 * rather than an emphatic sokuon. We should not treat this as emphatic.
 */
inline bool isTeTaFormSokuon(const std::vector<char32_t>& codepoints,
                             size_t sokuon_pos) {
  if (sokuon_pos + 1 >= codepoints.size()) {
    return false;  // Sokuon at end - could be emphatic
  }
  char32_t next = codepoints[sokuon_pos + 1];
  // っ+て, っ+た patterns are te/ta forms, not emphatic
  return next == core::hiragana::kTe || next == core::hiragana::kTa;
}

/**
 * @brief Extend candidates with emphatic suffix variants
 *
 * For each adjective candidate, checks if input continues with emphatic
 * characters (っ/ッ/ー/small vowels) and creates an extended variant.
 * E.g., すごい → すごいっっ, やばい → やばいー
 *
 * Special handling for っ (sokuon):
 * - っ followed by て/た is part of verb te/ta-form, not emphatic
 * - っ at end of input or followed by other emphatic chars is emphatic
 *
 * @param candidates Candidates to extend (modified in place)
 * @param codepoints Input text codepoints
 */
inline void addEmphaticVariants(std::vector<UnknownCandidate>& candidates,
                                const std::vector<char32_t>& codepoints) {
  std::vector<UnknownCandidate> emphatic_variants;

  for (const auto& cand : candidates) {
    // Only extend adjective candidates
    if (cand.pos != core::PartOfSpeech::Adjective) {
      continue;
    }

    // Check if there are emphatic characters after the candidate
    size_t emphatic_end = cand.end;
    std::string emphatic_suffix;

    while (emphatic_end < codepoints.size()) {
      char32_t c = codepoints[emphatic_end];
      if (isEmphaticChar(c)) {
        // Special case: っ followed by て/た is verb te-form, not emphatic
        if ((c == core::hiragana::kSmallTsu || c == U'ッ') &&
            isTeTaFormSokuon(codepoints, emphatic_end)) {
          break;  // Stop - this is part of a verb, not emphatic
        }
        emphatic_suffix += codepointToUtf8(c);
        ++emphatic_end;
      } else {
        break;
      }
    }

    // Track standard emphatic chars separately for cost calculation
    size_t standard_emphatic_chars = emphatic_suffix.size() / core::kJapaneseCharBytes;

    // Also check for repeated vowels matching the final character's vowel
    // E.g., すごい + いいい → すごいいいい (い ends in い-vowel)
    // Requires at least 2 consecutive vowels to be considered emphatic
    size_t vowel_repeat_count = 0;
    if (cand.end > 0 && emphatic_end < codepoints.size()) {
      char32_t final_char = codepoints[cand.end - 1];
      char32_t expected_vowel = getHiraganaVowel(final_char);

      if (expected_vowel != 0) {
        size_t vowel_start = emphatic_end;

        // Count consecutive occurrences of the expected vowel
        while (emphatic_end < codepoints.size() &&
               codepoints[emphatic_end] == expected_vowel) {
          ++vowel_repeat_count;
          ++emphatic_end;
        }

        // Require at least 2 repeated vowels for emphatic pattern
        if (vowel_repeat_count >= 2) {
          for (size_t i = 0; i < vowel_repeat_count; ++i) {
            emphatic_suffix += codepointToUtf8(expected_vowel);
          }
        } else {
          // Not enough repetition, reset position
          emphatic_end = vowel_start;
          vowel_repeat_count = 0;
        }
      }
    }

    // Add emphatic variant if we found any emphatic characters
    if (!emphatic_suffix.empty()) {
      UnknownCandidate emphatic_cand = cand;
      emphatic_cand.surface += emphatic_suffix;
      emphatic_cand.end = emphatic_end;
      float cost_adjustment;

      if (vowel_repeat_count >= 2) {
        // Give a BONUS for vowel repetition to compete with split alternatives
        float char_count =
            static_cast<float>(emphatic_suffix.size() / core::kJapaneseCharBytes);
        cost_adjustment = -0.5F + 0.05F * char_count;
      } else {
        // Standard emphatic chars (sokuon/chouon/small vowels) use penalty
        cost_adjustment = 0.3F * static_cast<float>(standard_emphatic_chars);
      }
      emphatic_cand.cost += cost_adjustment;
#ifdef SUZUME_DEBUG_INFO
      emphatic_cand.pattern += "_emphatic";
#endif
      emphatic_variants.push_back(std::move(emphatic_cand));
    }
  }

  // Add all emphatic variants
  for (auto& var : emphatic_variants) {
    candidates.push_back(std::move(var));
  }
}

}  // namespace

std::vector<UnknownCandidate> generateAdjectiveCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (typically 1-2 characters for i-adjectives)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 3 &&  // Max 3 kanji for adjective stem
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  if (kanji_end == start_pos) {
    return candidates;
  }

  // Look for hiragana after kanji (adjective endings like い, かった, くない)
  // Note: Some adjectives have hiragana in the stem (美しい, 楽しい, 涼しい, etc.)
  // so we allow any hiragana and let the inflection module decide
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Check if first hiragana is a particle that can NEVER be part of an adjective
  // Note: て is the te-form particle (接続助詞), not part of adjective stems
  // This prevents "来てい" from being parsed as an adjective (来ている = verb)
  char32_t first_hiragana = codepoints[kanji_end];
  if (normalize::isNeverAdjectiveStemAfterKanji(first_hiragana)) {
    return candidates;  // These particles follow nouns/verbs, not adjective stems
  }

  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 8 &&
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    ++hiragana_end;
  }

  if (hiragana_end <= kanji_end) {
    return candidates;
  }

  // Try different ending lengths
  for (size_t end_pos = hiragana_end; end_pos > kanji_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Skip single-kanji + single hiragana "い" patterns
    // These are typically godan verb renyokei (伴い, 用い, 買い, 追い)
    // not i-adjectives. Real single-kanji i-adjectives (怖い, 酸い)
    // should be in the dictionary, not generated as unknown words.
    if (kanji_end == start_pos + 1 && end_pos == kanji_end + 1) {
      continue;  // Skip - likely godan verb renyokei, not i-adjective
    }

    // Skip patterns ending with っ + hiragana (〜てく, 〜てない, etc.)
    // These are te-form contractions (〜ていく→〜てく), not i-adjectives.
    // E.g., 待ってく (matte-ku) = 待っていく, not an adjective
    std::string hiragana_part = extractSubstring(codepoints, kanji_end, end_pos);
    if (hiragana_part.size() >= core::kTwoJapaneseCharBytes &&
        hiragana_part.substr(0, core::kJapaneseCharBytes) == "っ") {  // Starts with っ
      continue;  // Skip - likely te-form contraction, not i-adjective
    }

    // Skip patterns ending with 〜んでい or 〜でい
    // These are te-form + auxiliary verb patterns (〜んでいく, 〜ている, etc.)
    // not i-adjectives. E.g., 学んでい (manande-i) = 学んでいく, not adj
    if (hiragana_part.size() >= core::kThreeJapaneseCharBytes &&
        hiragana_part.substr(hiragana_part.size() - core::kThreeJapaneseCharBytes) == "んでい") {
      continue;  // Skip - likely te-form + aux pattern
    }
    if (hiragana_part.size() >= core::kTwoJapaneseCharBytes &&
        hiragana_part.substr(hiragana_part.size() - core::kTwoJapaneseCharBytes) == "でい") {
      continue;  // Skip - likely te-form + aux pattern
    }

    // Skip patterns ending with verb passive/potential/causative negative renyokei
    // 〜られなく, 〜れなく, 〜させなく, 〜せなく, 〜されなく are all verb forms,
    // not i-adjectives. E.g., 食べられなく = 食べられる + ない (negative renyokei)
    if (grammar::endsWithPassiveCausativeNegativeRenyokei(hiragana_part)) {
      continue;  // Skip - passive/potential/causative negative renyokei
    }

    // Skip patterns ending with 〜れなくなった (passive negative + become + past)
    // E.g., 読まれなくなった = 読む + れる + なくなる + た
    if (grammar::endsWithNegativeBecomePattern(hiragana_part)) {
      continue;  // Skip - passive/causative negative + become pattern
    }

    // Skip patterns ending with 〜なく when followed by なった/なる
    // E.g., 食べなく + なった is actually 食べなくなった (verb form)
    // Check if the text continues with なった/なる/なって
    if (hiragana_part.size() >= core::kTwoJapaneseCharBytes &&
        hiragana_part.substr(hiragana_part.size() - core::kTwoJapaneseCharBytes) == "なく" &&
        end_pos < codepoints.size()) {
      // Check following characters for なった/なる/なって
      char32_t next_char = codepoints[end_pos];
      if (next_char == U'な') {
        continue;  // Skip - likely verb negative + なる pattern
      }
    }

    // Skip patterns that are causative stems (〜べさ, 〜ませ, etc.)
    // E.g., 食べさ is the start of 食べさせる (causative)
    // These end in さ but are followed by せ in the full form
    if (hiragana_part == "べさ" || hiragana_part == "べさせ" ||
        hiragana_part == "べさせら" || hiragana_part == "べさせられ") {
      continue;  // Skip - ichidan causative stem patterns
    }
    // Note: causative-passive 〜させられなくなった patterns are already covered
    // by endsWithNegativeBecomePattern above

    // Skip Godan verb renyokei + そう patterns (飲みそう, 降りそうだ, etc.)
    // These are verb + そう auxiliary patterns, not i-adjectives.
    // Pattern: single kanji + renyokei suffix (i-row) + そう/そうだ/そうです...
    // Renyokei suffixes: み, ぎ, ち, び, り, に (6 patterns, き handled below)
    // Note: し is handled specially below with dictionary validation
    // Note: き is also handled specially because 大きい exists as an adjective
    if (kanji_end == start_pos + 1 && hiragana_part.size() >= core::kThreeJapaneseCharBytes) {
      // Check if pattern starts with: renyokei suffix + そう
      std::string_view renyokei_char = std::string_view(hiragana_part).substr(0, core::kJapaneseCharBytes);
      if ((renyokei_char == "み" ||
           renyokei_char == "ぎ" || renyokei_char == "ち" ||
           renyokei_char == "び" || renyokei_char == "り" ||
           renyokei_char == "に") &&
          hiragana_part.size() >= core::kThreeJapaneseCharBytes &&
          hiragana_part.substr(core::kJapaneseCharBytes, core::kTwoJapaneseCharBytes) == scorer::kSuffixSou) {
        // This is verb renyokei + そう pattern (with optional だ/です/etc.)
        continue;  // Skip - likely verb renyokei + そう, not i-adjective
      }
    }

    // For し + そう patterns (話しそう, 難しそう, 美味しそう, etc.), use inflection
    // analysis to distinguish verb renyokei + そう from adjective + そう.
    // Works for both single and multi-kanji stems.
    // 難しそう → 難しい has higher confidence than 難す → allow adjective candidate
    // 美味しそう → 美味しい has higher confidence than 美味す → allow adjective candidate
    // 話しそう → 話す is in dictionary (handled by dict lookup below) → skip
    //
    // Strategy: Compare adjective confidence with verb confidence.
    // If adjective confidence is higher, prefer adjective interpretation.
    // For dictionary-registered verbs, the dictionary lookup below will handle them.
    bool is_dict_adjective = false;
    if (hiragana_part.size() >= core::kThreeJapaneseCharBytes &&
        hiragana_part.substr(0, core::kJapaneseCharBytes) == "し" &&
        hiragana_part.substr(core::kJapaneseCharBytes, core::kTwoJapaneseCharBytes) == "そう") {
      std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);

      // Get adjective confidence for kanji + しい
      std::string adj_form = kanji_stem + "しい";
      float adj_confidence = 0.0F;
      auto adj_results = inflection.analyze(adj_form);
      for (const auto& result : adj_results) {
        if (result.verb_type == grammar::VerbType::IAdjective &&
            result.confidence > adj_confidence) {
          adj_confidence = result.confidence;
        }
      }

      // Get verb confidence for kanji + す
      std::string verb_form = kanji_stem + "す";
      float verb_confidence = 0.0F;
      auto verb_results = inflection.analyze(verb_form);
      for (const auto& result : verb_results) {
        if (result.verb_type == grammar::VerbType::GodanSa &&
            result.confidence > verb_confidence) {
          verb_confidence = result.confidence;
        }
      }

      // Use dictionary to check for real verbs like 話す, 出す
      bool is_dict_verb = false;
      if (dict_manager != nullptr) {
        auto lookup = dict_manager->lookup(verb_form, 0);
        for (const auto& result : lookup) {
          if (result.entry != nullptr &&
              result.entry->pos == core::PartOfSpeech::Verb) {
            is_dict_verb = true;
            break;
          }
        }
      }

      if (is_dict_verb) {
        continue;  // Real verb exists in dictionary, prefer verb interpretation
      }

      // Check for suru-verb patterns: if kanji stem is 2+ characters and ends with
      // kanji, it's likely a suru-verb (遅刻しそう → 遅刻する, not 遅刻しい)
      // Single-kanji patterns like 難しそう (難しい) are valid adjectives.
      // Exception: known adjectives like 美味しい should still be allowed.
      size_t kanji_char_count = kanji_end - start_pos;
      if (kanji_char_count >= 2) {
        // Check if this is a known adjective in dictionary (e.g., 美味しい)
        std::string adj_full = kanji_stem + "しい";
        bool is_known_adj = false;
        if (dict_manager != nullptr) {
          auto lookup = dict_manager->lookup(adj_full, 0);
          for (const auto& result : lookup) {
            if (result.entry != nullptr &&
                result.entry->pos == core::PartOfSpeech::Adjective) {
              is_known_adj = true;
              break;
            }
          }
        }
        if (!is_known_adj) {
          // Multi-kanji + し + そう without known adjective is suru-verb + auxiliary
          // (遅刻しそう, 勉強しそう, 検討しそう, etc.)
          continue;
        }
        // Known adjective like 美味しい, allow the candidate
        is_dict_adjective = true;
      } else {
        // Single-kanji pattern, use confidence comparison
        // If adjective confidence is meaningfully higher than verb confidence,
        // allow adjective candidate
        // The 0.03 margin prevents ties from incorrectly producing adjective candidates
        if (adj_confidence >= 0.6F && adj_confidence >= verb_confidence + 0.03F) {
          is_dict_adjective = true;
        } else {
          continue;
        }
      }
    }

    // For き + そう patterns, check if stem + く exists as a verb.
    // If a godan-ku verb exists, this is likely verb renyoukei + そう, not adjective.
    // 書きそう → 書く (verb exists) → skip adjective candidate
    // 大きそう → 大く (verb doesn't exist) → allow adjective candidate
    // This is the inverse of し pattern: we check for verb existence, not adjective.
    if (hiragana_part.size() >= core::kThreeJapaneseCharBytes &&
        hiragana_part.substr(0, core::kJapaneseCharBytes) == "き" &&
        hiragana_part.substr(core::kJapaneseCharBytes, core::kTwoJapaneseCharBytes) == "そう") {
      if (dict_manager != nullptr) {
        // Construct potential verb form: kanji + く
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
        std::string verb_form = kanji_stem + "く";
        auto lookup = dict_manager->lookup(verb_form, 0);
        // Check if any entry is a verb with exact match
        size_t verb_form_chars = 0;
        for (size_t i = 0; i < verb_form.size(); ) {
          auto byte = static_cast<uint8_t>(verb_form[i]);
          if ((byte & 0x80) == 0) { i += 1; }
          else if ((byte & 0xE0) == 0xC0) { i += 2; }
          else if ((byte & 0xF0) == 0xE0) { i += 3; }
          else { i += 4; }
          ++verb_form_chars;
        }
        bool is_godan_ku_verb = false;
        for (const auto& result : lookup) {
          if (result.length == verb_form_chars &&
              result.entry != nullptr &&
              result.entry->pos == core::PartOfSpeech::Verb) {
            is_godan_ku_verb = true;
            break;
          }
        }
        if (is_godan_ku_verb) {
          continue;  // Verb exists, so this is verb renyoukei + そう, skip adjective
        }
        // No verb found, allow adjective candidate (e.g., 大きそう → 大きい)
      }
    }

    // B57: For single kanji + ければ patterns (叩ければ, 引ければ, etc.),
    // check if the kanji + く is a verb. If so, this is likely verb potential + conditional,
    // not an adjective pattern.
    // 叩ければ → 叩く (verb exists) → skip adjective (叩い is not a real adjective)
    // 寒ければ → 寒い (adjective) - handled separately as hiragana_part starts with け
    if (kanji_end == start_pos + 1 && hiragana_part == "ければ") {
      if (dict_manager != nullptr) {
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
        std::string verb_form = kanji_stem + "く";
        auto lookup = dict_manager->lookup(verb_form, 0);
        // Count characters in verb_form (kanji + く = 2 chars)
        size_t verb_form_chars = 2;  // Single kanji + く
        bool is_godan_ku_verb = false;
        for (const auto& result : lookup) {
          if (result.length == verb_form_chars &&
              result.entry != nullptr &&
              result.entry->pos == core::PartOfSpeech::Verb) {
            is_godan_ku_verb = true;
            break;
          }
        }
        if (is_godan_ku_verb) {
          continue;  // Verb exists, this is verb potential-conditional (叩ける + ば)
        }
      }
    }

    // Skip patterns that are clearly verb negatives, not adjectives
    // 〜かない, 〜がない, etc. are Godan verb mizenkei + ない patterns
    // 〜しない is Suru verb + ない, 〜べない is Ichidan verb + ない
    if (grammar::endsWithVerbNegative(hiragana_part)) {
      continue;  // Skip - verb negative pattern, not adjective
    }

    // Check all candidates for IAdjective, not just the best one
    // This handles cases like 美味しそう where Suru (美味する) may have higher
    // confidence than IAdjective (美味しい), but we still want to generate
    // an adjective candidate for the lattice to choose from
    auto all_candidates = inflection.analyze(surface);

    for (const auto& cand : all_candidates) {
      // Require confidence >= 0.5 for i-adjectives
      // Base forms like 寒い get exactly 0.5, conjugated forms like 美しかった get 0.68+
      if (cand.confidence >= 0.5F &&
          cand.verb_type == grammar::VerbType::IAdjective) {
        // Filter out false positives: いたす honorific pattern
        // Invalid patterns (all have た after the candidate):
        //   - サ変名詞 + いたす: 検討いたします, 勉強いたしました
        //   - Verb renyokei + いたす: 伝えいたします, 申しいたします
        // Valid patterns:
        //   - 面白いな (next char is な)
        //   - 寒いよ (next char is よ)
        //   - 面白い (end of text)
        // Key insight: if minimum confidence (0.5) and next char is た, skip
        if (cand.confidence <= 0.5F) {
          if (end_pos < codepoints.size() && codepoints[end_pos] == U'た') {
            continue;  // Skip - likely いたす honorific pattern
          }
        }

        UnknownCandidate candidate;
        candidate.surface = surface;
        candidate.start = start_pos;
        candidate.end = end_pos;
        candidate.pos = core::PartOfSpeech::Adjective;
        // Lower base cost (0.2F) to beat verb candidates after POS prior adjustment
        // ADJ prior (0.3) is higher than VERB prior (0.2), so we need lower edge cost
        float cost = 0.2F + (1.0F - cand.confidence) * 0.3F;
        // Bonus for adjectives confirmed in dictionary (美味しそう, 難しそう, etc.)
        // This helps beat false-positive suru verb candidates (美味する is invalid)
        if (is_dict_adjective) {
          cost -= 0.25F;
        }
        // Penalty for non-dictionary i-adjective nominalization (さ ending)
        // This prevents false positives like 勉強さ (from non-existent 勉強い)
        // from beating suru-verb split path (勉強 + さ + れる)
        if (!is_dict_adjective && surface.size() >= 3 &&
            surface.substr(surface.size() - 3) == "さ") {
          cost += 1.5F;  // Strong penalty for unconfirmed さ nominalization
        }
        candidate.cost = cost;
        candidate.has_suffix = false;
        // Set lemma to base form from inflection analysis (e.g., 使いやすく → 使いやすい)
        candidate.lemma = cand.base_form;
#ifdef SUZUME_DEBUG_INFO
        candidate.origin = CandidateOrigin::AdjectiveI;
        candidate.confidence = cand.confidence;
        candidate.pattern = "i_adjective";
#endif
        candidates.push_back(candidate);
        break;  // Only add one adjective candidate per surface
      }
    }
  }

  // Add emphatic variants (すごい → すごいっっ, etc.)
  addEmphaticVariants(candidates, codepoints);

  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> generateNaAdjectiveCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const UnknownOptions& options) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji sequence
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < options.max_kanji_length &&
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  size_t kanji_len = kanji_end - start_pos;

  // Pattern 0: Kanji(1) + やか/らか/か + な patterns (e.g., 華やかな, 豊かな, 静かな)
  // These are common na-adjective patterns with kanji stem + hiragana suffix
  if (kanji_len == 1 && kanji_end < char_types.size() &&
      char_types[kanji_end] == normalize::CharType::Hiragana) {
    // Check for やか/らか/か patterns
    size_t hira_end = kanji_end;
    while (hira_end < char_types.size() &&
           hira_end - kanji_end < 4 &&
           char_types[hira_end] == normalize::CharType::Hiragana) {
      ++hira_end;
    }

    std::string full_surface = extractSubstring(codepoints, start_pos, hira_end);
    size_t hira_len = hira_end - kanji_end;

    // Check if ends with な (na-adjective conjugation)
    bool ends_with_na = (hira_end > kanji_end && codepoints[hira_end - 1] == U'な');

    if (ends_with_na && hira_len >= 2) {
      // Extract stem (without な)
      std::string stem = extractSubstring(codepoints, start_pos, hira_end - 1);
      size_t stem_hira_len = hira_len - 1;

      // Check for やか/らか/か patterns
      bool is_yaka_pattern = false;
      if (stem_hira_len >= 2) {
        std::string stem_suffix = extractSubstring(codepoints, kanji_end, hira_end - 1);
        is_yaka_pattern = (stem_suffix == "やか" || stem_suffix == "らか" ||
                          stem_suffix == "か");
      } else if (stem_hira_len == 1) {
        // Single か (e.g., 豊か, 静か)
        is_yaka_pattern = (codepoints[kanji_end] == U'か');
      }

      if (is_yaka_pattern) {
        UnknownCandidate candidate;
        candidate.surface = stem;  // Stem without な
        candidate.start = start_pos;
        candidate.end = hira_end - 1;  // Exclude な
        candidate.pos = core::PartOfSpeech::Adjective;
        candidate.cost = 0.2F;  // Low cost for common pattern
        candidate.has_suffix = true;
#ifdef SUZUME_DEBUG_INFO
        candidate.origin = CandidateOrigin::AdjectiveNa;
        candidate.confidence = 0.9F;
        candidate.pattern = "na_adj_yaka_raka";
#endif
        candidates.push_back(candidate);
        return candidates;  // Return early for clear pattern match
      }
    }
  }

  // Need at least 2 kanji for other patterns
  if (kanji_len < 2) {
    return candidates;
  }

  std::string kanji_seq = extractSubstring(codepoints, start_pos, kanji_end);

  // Pattern 1: Check for na-adjective suffixes (的)
  for (const auto& suffix : kNaAdjSuffixes) {
    // Check if kanji_seq ends with suffix
    if (kanji_seq.size() >= suffix.size()) {
      std::string_view kanji_suffix(kanji_seq.data() + kanji_seq.size() - suffix.size(),
                                     suffix.size());
      if (kanji_suffix == suffix) {
        // Found a na-adjective pattern like 理性的, 論理的
        UnknownCandidate candidate;
        candidate.surface = kanji_seq;
        candidate.start = start_pos;
        candidate.end = kanji_end;
        candidate.pos = core::PartOfSpeech::Adjective;
        // Low cost to prefer this over noun interpretation
        candidate.cost = 0.3F;
        candidate.has_suffix = true;
#ifdef SUZUME_DEBUG_INFO
        candidate.origin = CandidateOrigin::AdjectiveNa;
        candidate.confidence = 1.0F;
        candidate.pattern = "na_adjective_teki";
#endif
        candidates.push_back(candidate);
        break;  // Use first matching suffix
      }
    }
  }

  // Pattern 2: Check for kanji compound + な pattern (e.g., 獰猛な)
  // Generate ADJ candidate for kanji portion when followed by な
  if (kanji_end < codepoints.size() && codepoints[kanji_end] == U'な') {
    // Skip if first character is a formal noun (形式名詞)
    // e.g., 時妙な should be 時+妙な, not 時妙(ADJ)+な
    // Formal nouns (時, 事, 所, etc.) are standalone grammatical words
    std::string first_char_str;
    normalize::encodeUtf8(codepoints[start_pos], first_char_str);
    if (normalize::isFormalNounSurface(first_char_str)) {
      // Don't generate na-adjective candidate for formal noun + kanji patterns
      return candidates;
    }

    // Found kanji compound + な - potential na-adjective stem
    UnknownCandidate candidate;
    candidate.surface = kanji_seq;
    candidate.start = start_pos;
    candidate.end = kanji_end;
    candidate.pos = core::PartOfSpeech::Adjective;
    // Cost similar to dictionary na-adjectives but with small penalty for unknown
    // Dictionary na-adj stems have cost 0.5, so use similar value
    candidate.cost = 0.5F;
    candidate.has_suffix = true;  // Has な suffix following
#ifdef SUZUME_DEBUG_INFO
    candidate.origin = CandidateOrigin::AdjectiveNa;
    candidate.confidence = 0.8F;  // Lower confidence for unknown pattern
    candidate.pattern = "na_adjective_stem";
#endif
    candidates.push_back(candidate);
  }

  return candidates;
}

std::vector<UnknownCandidate> generateHiraganaAdjectiveCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  char32_t first_char = codepoints[start_pos];

  // Skip if first character is を (wo) - this is always a particle, never an adjective stem
  // Unlike other particles (は, か, わ, etc.) that can start valid adjectives,
  // を is exclusively an object marker and never begins a Japanese adjective
  if (first_char == U'を') {
    return candidates;
  }

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
    return candidates;
  }

  // STEP 2: Determine the hiragana_end for candidate generation
  // If first char is a particle, we only allow the full sequence if it's a valid adjective
  // Otherwise, we break at particle boundaries for shorter subsequences
  size_t hiragana_end = max_hiragana_end;
  bool starts_with_particle = normalize::isExtendedParticle(first_char);
  bool has_prolonged = containsProlongedSoundMark(codepoints, start_pos, max_hiragana_end);

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

      // Skip patterns ending with just く (adverbial form)
      // This prevents よろしく, わくわく from being validated as adjectives
      if (test_surface.size() >= core::kJapaneseCharBytes &&
          test_surface.substr(test_surface.size() - core::kJapaneseCharBytes) == "く") {
        // Allow くない (negative form), but skip just く
        if (test_surface.size() < core::kThreeJapaneseCharBytes ||
            test_surface.substr(test_surface.size() - core::kThreeJapaneseCharBytes) != "くない") {
          continue;  // Skip - adverbial form, not adjective
        }
      }

      // Skip patterns ending with just ない (negative auxiliary misidentified as adjective)
      // This prevents でもない from being validated as an adjective
      // Valid patterns: くない (adjective negative), but ない alone after particles is auxiliary
      if (test_surface.size() >= core::kTwoJapaneseCharBytes &&
          test_surface.substr(test_surface.size() - core::kTwoJapaneseCharBytes) == "ない") {
        // Allow くない (adjective negative form)
        if (test_surface.size() < core::kThreeJapaneseCharBytes ||
            test_surface.substr(test_surface.size() - core::kThreeJapaneseCharBytes) != "くない") {
          continue;  // Skip - likely negative auxiliary, not adjective
        }
      }

      auto test_candidates = inflection.analyze(test_surface);
      for (const auto& cand : test_candidates) {
        if (cand.verb_type == grammar::VerbType::IAdjective &&
            cand.confidence >= 0.50F) {
          // For particle-starting sequences, require stem length >= 2 characters
          // This prevents に+そうな from being recognized as にい (invalid)
          // Real adjectives have stems of at least 2 chars: あつい, かわいい, etc.
          size_t stem_chars = 0;
          for (size_t i = 0; i < cand.stem.size(); ) {
            auto byte = static_cast<uint8_t>(cand.stem[i]);
            if ((byte & 0x80) == 0) { i += 1; }
            else if ((byte & 0xE0) == 0xC0) { i += 2; }
            else if ((byte & 0xF0) == 0xE0) { i += 3; }
            else { i += 4; }
            ++stem_chars;
          }
          if (stem_chars < 2) {
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
      return candidates;
    }
    // Use the valid adjective length as hiragana_end
    hiragana_end = valid_adj_min_end;
  } else if (!starts_with_particle) {
    // For non-particle-starting sequences, apply particle boundary breaking
    // This handles cases like おいしい where we don't want to extend past particles
    hiragana_end = start_pos;
    while (hiragana_end < max_hiragana_end) {
      char32_t curr_char = codepoints[hiragana_end];

      // Only break at strong particle boundaries after minimum stem length
      if (hiragana_end - start_pos >= 3 && !normalize::isProlongedSoundMark(curr_char)) {
        bool next_is_prolonged = (hiragana_end + 1 < char_types.size() &&
                                   normalize::isProlongedSoundMark(codepoints[hiragana_end + 1]));
        if (!next_is_prolonged) {
          if (normalize::isExtendedParticle(curr_char) || curr_char == U'や') {
            break;  // Stop before the particle
          }
        }
      }
      ++hiragana_end;
    }
  }

  // Need at least 3 characters after determining hiragana_end
  if (hiragana_end <= start_pos + 2) {
    return candidates;
  }

  // Try different lengths, starting from longest
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 2; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Skip patterns ending with verb passive/potential/causative negative renyokei
    // 〜られなく, 〜れなく, 〜させなく, 〜せなく, 〜されなく are all verb forms,
    // not i-adjectives. E.g., けられなく = ける + られ + ない
    if (grammar::endsWithPassiveCausativeNegativeRenyokei(surface)) {
      continue;  // Skip - passive/potential/causative negative renyokei
    }
    // Skip patterns ending with 〜かなく (verb negative renyokei of godan verbs)
    // E.g., いかなく = いく + ない
    if (grammar::endsWithGodanNegativeRenyokei(surface)) {
      continue;  // Skip - godan negative renyokei
    }

    // Skip patterns ending with just く (adverbial form of i-adjective)
    // This prevents よろしく, わくわく from being recognized as adjectives.
    // Valid i-adjective endings: い, かった, くない, ければ, さ, そう, etc.
    // Note: くない is valid (negative), but just く is adverbial (not adjective POS)
    if (surface.size() >= core::kJapaneseCharBytes &&
        surface.substr(surface.size() - core::kJapaneseCharBytes) == "く") {
      // Check if it's くない (negative form) - that's valid
      if (surface.size() < core::kThreeJapaneseCharBytes ||
          surface.substr(surface.size() - core::kThreeJapaneseCharBytes) != "くない") {
        continue;  // Skip - just く ending (adverbial form)
      }
    }

    // Skip patterns ending with just ない (negative auxiliary misidentified as adjective)
    // This prevents でもない from being recognized as an adjective when starting with particle
    // Valid patterns: くない (adjective negative), but ない alone after particles is auxiliary
    if (starts_with_particle &&
        surface.size() >= core::kTwoJapaneseCharBytes &&
        surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == "ない") {
      // Allow くない (adjective negative form)
      if (surface.size() < core::kThreeJapaneseCharBytes ||
          surface.substr(surface.size() - core::kThreeJapaneseCharBytes) != "くない") {
        continue;  // Skip - likely negative auxiliary, not adjective
      }
    }

    // Normalize prolonged sound marks before analysis
    // e.g., すごーい → すごおい, やばーい → やばあい
    std::string analysis_surface = surface;
    bool has_prolonged = containsProlongedSoundMark(codepoints, start_pos, end_pos);
    if (has_prolonged) {
      analysis_surface = normalizeProlongedSoundMark(codepoints, start_pos, end_pos);
    }

    // Check all candidates for IAdjective, not just the best one
    // This handles cases where Suru interpretation may have higher confidence
    auto all_candidates = inflection.analyze(analysis_surface);
    for (const auto& cand : all_candidates) {
      // For hiragana-only adjectives, require higher confidence (0.55) than
      // kanji+hiragana adjectives (0.50) to avoid false positives like しそう → しい
      // For patterns with prolonged sound marks, lower threshold (0.40) since these
      // are intentional colloquial expressions (すごーい, やばーい)
      // Multiple consecutive marks (すごーーい) result in even lower confidence
      // For particle-starting sequences, lower threshold (0.50) since these have
      // already been validated as forming valid adjectives (はなはだしい, かわいい)
      float confidence_threshold = has_prolonged ? 0.40F :
                                   starts_with_particle ? 0.50F : 0.55F;
      if (cand.confidence >= confidence_threshold &&
          cand.verb_type == grammar::VerbType::IAdjective) {
        // For particle-starting sequences, require stem length >= 2 characters
        // This prevents に+そうな from being recognized as にい (invalid)
        if (starts_with_particle) {
          size_t stem_chars = 0;
          for (size_t i = 0; i < cand.stem.size(); ) {
            auto byte = static_cast<uint8_t>(cand.stem[i]);
            if ((byte & 0x80) == 0) { i += 1; }
            else if ((byte & 0xE0) == 0xC0) { i += 2; }
            else if ((byte & 0xF0) == 0xE0) { i += 3; }
            else { i += 4; }
            ++stem_chars;
          }
          if (stem_chars < 2) {
            continue;  // Stem too short for a valid adjective
          }
        }
        UnknownCandidate candidate;
        candidate.surface = surface;  // Keep original surface with ー
        candidate.start = start_pos;
        candidate.end = end_pos;
        candidate.pos = core::PartOfSpeech::Adjective;
        // Base cost for hiragana i-adjective candidates
        // Use neutral base (0.0F) to avoid false positives like につい
        // which should be parsed as に(PARTICLE)+ついて(VERB)
        float cost = 0.0F + (1.0F - cand.confidence) * 0.5F;
        if (has_prolonged) {
          cost -= 0.1F;  // Bonus for colloquial patterns like すごーい
        }
        // Length-based bonus for adjectives starting with particle characters
        // Short sequences (3 chars like につい) are likely verb te-forms
        // Longer sequences (4+ chars like かわいい, はなはだしい) are real adjectives
        if (starts_with_particle) {
          size_t char_count = end_pos - start_pos;
          if (char_count >= 5) {
            cost -= 0.5F;  // Strong bonus for long adjectives (はなはだしい)
          } else if (char_count >= 4) {
            cost -= 0.35F;  // Moderate bonus for medium adjectives (かわいい)
          }
          // No bonus for 3-char sequences (につい) - let dictionary entries win
        }
        candidate.cost = cost;
        candidate.has_suffix = false;
        // Set lemma to base form from inflection analysis
        // For prolonged sound mark patterns, normalize the base form
        // e.g., すごおい → すごい, やばあい → やばい
        if (has_prolonged) {
          candidate.lemma = normalizeBaseForm(cand.base_form, codepoints, start_pos, end_pos);
        } else {
          candidate.lemma = cand.base_form;
        }
#ifdef SUZUME_DEBUG_INFO
        candidate.origin = CandidateOrigin::AdjectiveIHiragana;
        candidate.confidence = cand.confidence;
        candidate.pattern = has_prolonged ? "i_adjective_hira_choon" : "i_adjective_hira";
#endif
        candidates.push_back(candidate);
        break;  // Only add one adjective candidate per surface
      }
    }
  }

  // Add emphatic variants (まずい → まずいっ, etc.)
  addEmphaticVariants(candidates, codepoints);

  // Add ku-form candidates for kunai patterns
  // This enables MeCab-compatible split: しんどくない → しんどく + ない
  // For each candidate ending with くない, generate a ku-form variant ending with く
  std::vector<UnknownCandidate> ku_form_candidates;
  for (const auto& cand : candidates) {
    // Check if surface ends with くない
    if (cand.surface.size() >= core::kThreeJapaneseCharBytes &&
        cand.surface.substr(cand.surface.size() - core::kThreeJapaneseCharBytes) == "くない") {
      // Generate ku-form variant: しんどくない → しんどく
      UnknownCandidate ku_cand;
      ku_cand.surface = cand.surface.substr(0, cand.surface.size() - core::kTwoJapaneseCharBytes);  // Remove ない
      ku_cand.start = cand.start;
      ku_cand.end = cand.end - 2;  // 2 characters (ない)
      ku_cand.pos = core::PartOfSpeech::Adjective;
      ku_cand.lemma = cand.lemma;  // Same lemma (しんどい)
      ku_cand.cost = cand.cost + 0.1F;  // Slightly higher cost than full form
      ku_cand.has_suffix = true;  // This is a conjugated form
#ifdef SUZUME_DEBUG_INFO
      ku_cand.origin = cand.origin;
      ku_cand.confidence = cand.confidence;
      ku_cand.pattern = "i_adjective_hira_ku";
#endif
      ku_form_candidates.push_back(ku_cand);
    }
  }

  // Add all ku-form candidates
  for (auto& var : ku_form_candidates) {
    candidates.push_back(std::move(var));
  }

  // Sort by cost
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> generateKatakanaAdjectiveCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection) {
  std::vector<UnknownCandidate> candidates;

  // Only process katakana-starting positions
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Katakana) {
    return candidates;
  }

  // Find katakana portion (1-6 characters for slang adjective stems)
  // e.g., エモ, キモ, ウザ, ダサ, etc.
  size_t kata_end = start_pos;
  while (kata_end < char_types.size() &&
         kata_end - start_pos < 6 &&
         char_types[kata_end] == normalize::CharType::Katakana) {
    ++kata_end;
  }

  // Need at least 1 katakana character
  if (kata_end == start_pos) {
    return candidates;
  }

  // Must be followed by hiragana (i-adjective endings)
  if (kata_end >= char_types.size() ||
      char_types[kata_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Check if first hiragana is a valid i-adjective ending start
  // I-adjective endings: い, か(った), く(ない/て), け(れば), さ(そう), etc.
  char32_t first_hira = codepoints[kata_end];
  if (first_hira != U'い' && first_hira != U'か' &&
      first_hira != U'く' && first_hira != U'け' &&
      first_hira != U'さ') {
    return candidates;
  }

  // Find hiragana portion (up to 8 chars for conjugation endings)
  size_t hira_end = kata_end;
  while (hira_end < char_types.size() &&
         hira_end - kata_end < 8 &&
         char_types[hira_end] == normalize::CharType::Hiragana) {
    ++hira_end;
  }

  // Need at least 1 hiragana for the い ending
  if (hira_end <= kata_end) {
    return candidates;
  }

  // Try different ending lengths, starting from longest
  for (size_t end_pos = hira_end; end_pos > kata_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Check all candidates for IAdjective
    auto all_candidates = inflection.analyze(surface);
    for (const auto& cand : all_candidates) {
      // Require confidence >= 0.5 for i-adjectives
      if (cand.confidence >= 0.5F &&
          cand.verb_type == grammar::VerbType::IAdjective) {
        UnknownCandidate candidate;
        candidate.surface = surface;
        candidate.start = start_pos;
        candidate.end = end_pos;
        candidate.pos = core::PartOfSpeech::Adjective;
        // Lower cost than pure katakana noun to prefer adjective reading
        // Cost: 0.2-0.35 based on confidence (lower = better)
        candidate.cost = 0.2F + (1.0F - cand.confidence) * 0.3F;
        candidate.has_suffix = false;
        candidate.lemma = cand.base_form;
#ifdef SUZUME_DEBUG_INFO
        candidate.origin = CandidateOrigin::AdjectiveI;  // Katakana i-adj
        candidate.confidence = cand.confidence;
        candidate.pattern = "i_adjective_kata";
#endif
        candidates.push_back(candidate);
        break;  // Only add one adjective candidate per surface
      }
    }
  }

  // Add emphatic variants (エグい → エグいっ, etc.)
  addEmphaticVariants(candidates, codepoints);

  // Sort by cost
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> generateAdjectiveStemCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager) {
  std::vector<UnknownCandidate> candidates;

  // Must start with kanji
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (1-3 characters for adjective stem)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 3 &&
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

  // Find hiragana ending with し + auxiliary pattern (そう, すぎ, etc.)
  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 8 &&
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    ++hiragana_end;
  }

  if (hiragana_end <= kanji_end) {
    return candidates;
  }

  std::string hiragana_part = extractSubstring(codepoints, kanji_end, hiragana_end);

  // =============================================================================
  // Pattern 1: Regular i-adjective stem + すぎる/がる/さ/そう (ガル接続)
  // =============================================================================
  // MeCab handles regular i-adjectives (高い, 尊い, 寒い) differently from しい-adjectives.
  // For patterns like 高すぎる, MeCab splits as: 高(ADJ, ガル接続) + すぎる(VERB)
  // The adjective stem is just the kanji portion (without い).
  //
  // Patterns handled:
  // - 高すぎる → 高 (ADJ stem) + すぎる (VERB)
  // - 尊すぎて → 尊 (ADJ stem) + すぎ (VERB) + て (PARTICLE)
  // - 高がる → 高 (ADJ stem) + がる (VERB)
  // - 高さ → 高 (ADJ stem) + さ (NOUN/SUFFIX)
  // - 高そう → 高 (ADJ stem) + そう (AUX)
  static const std::vector<std::string_view> kIAdjGaruPatterns = {
      "すぎ",    // excessive: 高すぎる, 高すぎ, 高すぎて
      "がる",    // emotional verb: 高がる, 怖がる
      "がり",    // nominalized: 怖がり
      "がっ",    // te/ta form: 怖がって, 怖がった
      "さ",      // nominalization: 高さ, 重さ (1 char)
      "そう",    // appearance: 高そう (2 chars)
      "み",      // nominalization: 痛み, 深み (1 char)
  };

  // Check if hiragana starts with a garu-connection pattern
  for (const auto& pattern : kIAdjGaruPatterns) {
    if (hiragana_part.size() >= pattern.size() &&
        hiragana_part.substr(0, pattern.size()) == pattern) {
      // Found potential i-adjective stem + garu-connection pattern
      // The stem is just the kanji portion (e.g., 高, 尊, 寒)
      std::string stem = extractSubstring(codepoints, start_pos, kanji_end);
      std::string base_form = stem + "い";  // e.g., 高 → 高い

      // Validate that stem + い is a real i-adjective
      // Use lower threshold (0.35) for garu-connection patterns because:
      // - Single-kanji adjectives like 高い get lower confidence (0.42)
      // - The presence of すぎる/がる/さ strongly indicates adjective interpretation
      auto adj_results = inflection.analyze(base_form);
      bool is_valid_adjective = false;
      float adj_confidence = 0.0F;
      for (const auto& result : adj_results) {
        if (result.verb_type == grammar::VerbType::IAdjective &&
            result.confidence >= 0.35F) {
          is_valid_adjective = true;
          adj_confidence = result.confidence;
          break;
        }
      }

      if (!is_valid_adjective) {
        continue;
      }

      // Check for false positives: single-kanji stems that are also verb renyokei
      // E.g., 落ちすぎ could be 落ち(verb renyokei) + すぎ(verb)
      // We should prefer the verb renyokei interpretation if kanji+ちる/きる/etc. is a verb
      if (kanji_end - start_pos == 1) {
        // Check if stem + る, stem + す, etc. forms a verb
        std::string verb_base = stem + "ちる";  // ichidan check
        bool is_likely_verb_stem = false;
        if (dict_manager != nullptr) {
          // Check for common verb patterns
          for (const auto& suffix : {"ちる", "きる", "ぎる", "しる", "びる", "みる", "りる"}) {
            std::string potential_verb = stem + suffix;
            auto verb_lookup = dict_manager->lookup(potential_verb, 0);
            for (const auto& result : verb_lookup) {
              if (result.entry != nullptr &&
                  result.entry->pos == core::PartOfSpeech::Verb &&
                  result.entry->surface == potential_verb) {
                is_likely_verb_stem = true;
                break;
              }
            }
            if (is_likely_verb_stem) break;
          }
        }
        if (is_likely_verb_stem) {
          continue;  // Skip - likely verb renyokei, not adjective stem
        }
      }

      UnknownCandidate candidate;
      candidate.surface = stem;
      candidate.start = start_pos;
      candidate.end = kanji_end;
      candidate.pos = core::PartOfSpeech::Adjective;
      candidate.lemma = base_form;
      // Low cost to compete with single-token verb path (高すぎる as VERB)
      // Use negative cost to strongly prefer this split
      candidate.cost = -0.3F + (1.0F - adj_confidence) * 0.2F;
      candidate.has_suffix = true;  // This is a stem, expects suffix
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::AdjectiveI;
      candidate.confidence = adj_confidence;
      candidate.pattern = "adj_stem_garu_conn";
#endif
      candidates.push_back(candidate);
      // Don't break - allow multiple patterns to generate candidates
    }
  }

  // Check for しそう, しすぎ patterns (adjective stem + auxiliary)
  // The stem ends with し, and is followed by そう/すぎる/etc.
  // E.g., 難しそう → 難し (stem) + そう
  // E.g., 美しすぎる → 美し (stem) + すぎる
  static const std::vector<std::string_view> kAdjStemAuxPatterns = {
      "しそう",     // appearance: 難しそう
      "しそうだ",   // appearance + copula
      "しそうな",   // attributive
      "しそうに",   // adverbial
      "しすぎ",     // excessive: 難しすぎ
      "しすぎる",   // excessive + dictionary form
      "しすぎた",   // excessive + past
  };

  for (const auto& pattern : kAdjStemAuxPatterns) {
    if (hiragana_part.size() >= pattern.size() &&
        hiragana_part.substr(0, pattern.size()) == pattern) {
      // Found adjective stem + auxiliary pattern
      // The stem is: kanji + し
      size_t stem_end = kanji_end + 1;  // kanji + し (one hiragana)

      std::string stem = extractSubstring(codepoints, start_pos, stem_end);
      std::string base_form = stem + "い";  // e.g., 難し → 難しい

      // Validate that this looks like a real adjective
      auto adj_results = inflection.analyze(base_form);
      bool is_valid_adjective = false;
      float adj_confidence = 0.0F;
      for (const auto& result : adj_results) {
        if (result.verb_type == grammar::VerbType::IAdjective &&
            result.confidence >= 0.5F) {
          is_valid_adjective = true;
          adj_confidence = result.confidence;
          break;
        }
      }

      if (!is_valid_adjective) {
        continue;
      }

      // Also check that this is NOT a verb renyokei (話し from 話す)
      // by comparing adjective vs verb confidence
      // The verb form would be: kanji_stem + す (e.g., 話 + す = 話す)
      std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
      std::string verb_form = kanji_stem + "す";  // e.g., 話す (not 話しす)
      auto verb_results = inflection.analyze(verb_form);
      float verb_confidence = 0.0F;
      for (const auto& result : verb_results) {
        if ((result.verb_type == grammar::VerbType::GodanSa ||
             result.verb_type == grammar::VerbType::Suru) &&
            result.confidence > verb_confidence) {
          verb_confidence = result.confidence;
        }
      }

      // Check if the verb form (kanji + す) is in the dictionary
      // If it is, this is likely a verb renyokei, not an adjective stem
      // E.g., 話す is in dictionary → 話し is verb renyokei, not adjective
      // E.g., 難す is NOT in dictionary → 難し could be adjective stem
      if (dict_manager != nullptr) {
        auto verb_results = dict_manager->lookup(verb_form, 0);
        bool is_dict_verb = false;
        for (const auto& result : verb_results) {
          // Check for exact match verb
          if (result.entry != nullptr &&
              result.entry->pos == core::PartOfSpeech::Verb &&
              result.entry->surface == verb_form) {
            is_dict_verb = true;
            break;
          }
        }
        if (is_dict_verb) {
          continue;  // Skip - this is a dictionary verb renyokei
        }
      }

      // Confidence-based fallback when no dictionary available
      // Only generate adjective stem if adjective confidence is SIGNIFICANTLY higher
      // than verb confidence. This prevents generating stems for verb renyokei
      // patterns like 話し (from 話す) where both get similar confidence.
      constexpr float kConfidenceThreshold = 0.15F;
      if (adj_confidence - verb_confidence < kConfidenceThreshold) {
        continue;
      }

      UnknownCandidate candidate;
      candidate.surface = stem;
      candidate.start = start_pos;
      candidate.end = stem_end;
      candidate.pos = core::PartOfSpeech::Adjective;
      candidate.lemma = base_form;
      // Low cost to compete with VERB path
      candidate.cost = 0.1F + (1.0F - adj_confidence) * 0.2F;
      candidate.has_suffix = true;  // This is a stem, expects suffix
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::AdjectiveI;
      candidate.confidence = adj_confidence;
      candidate.pattern = "adj_stem_shii";
#endif
      candidates.push_back(candidate);
      break;  // Only one stem candidate per pattern
    }
  }

  return candidates;
}

}  // namespace suzume::analysis
