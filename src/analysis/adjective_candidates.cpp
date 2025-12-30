/**
 * @file adjective_candidates.cpp
 * @brief Adjective-based unknown word candidate generation
 */

#include "adjective_candidates.h"

#include <algorithm>

#include "core/utf8_constants.h"
#include "grammar/patterns.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"

namespace suzume::analysis {

namespace {

// Na-adjective forming suffixes (〜的 patterns)
const std::vector<std::string_view> kNaAdjSuffixes = {
    "的",  // 理性的, 論理的, etc.
};

// Get the vowel for a given hiragana/katakana character (for prolonged sound mark expansion)
// Returns the appropriate long vowel character based on the preceding character's row
char32_t getVowelForChar(char32_t ch) {
  // Hiragana vowel rows:
  // あ row (a): あ か が さ ざ た だ な は ば ぱ ま や ら わ
  // い row (i): い き ぎ し じ ち ぢ に ひ び ぴ み り
  // う row (u): う く ぐ す ず つ づ ぬ ふ ぶ ぷ む ゆ る
  // え row (e): え け げ せ ぜ て で ね へ べ ぺ め れ
  // お row (o): お こ ご そ ぞ と ど の ほ ぼ ぽ も よ ろ を

  // A-row characters
  if (ch == U'あ' || ch == U'か' || ch == U'が' || ch == U'さ' || ch == U'ざ' ||
      ch == U'た' || ch == U'だ' || ch == U'な' || ch == U'は' || ch == U'ば' ||
      ch == U'ぱ' || ch == U'ま' || ch == U'や' || ch == U'ら' || ch == U'わ') {
    return U'あ';
  }
  // I-row characters
  if (ch == U'い' || ch == U'き' || ch == U'ぎ' || ch == U'し' || ch == U'じ' ||
      ch == U'ち' || ch == U'ぢ' || ch == U'に' || ch == U'ひ' || ch == U'び' ||
      ch == U'ぴ' || ch == U'み' || ch == U'り') {
    return U'い';
  }
  // U-row characters
  if (ch == U'う' || ch == U'く' || ch == U'ぐ' || ch == U'す' || ch == U'ず' ||
      ch == U'つ' || ch == U'づ' || ch == U'ぬ' || ch == U'ふ' || ch == U'ぶ' ||
      ch == U'ぷ' || ch == U'む' || ch == U'ゆ' || ch == U'る') {
    return U'う';
  }
  // E-row characters
  if (ch == U'え' || ch == U'け' || ch == U'げ' || ch == U'せ' || ch == U'ぜ' ||
      ch == U'て' || ch == U'で' || ch == U'ね' || ch == U'へ' || ch == U'べ' ||
      ch == U'ぺ' || ch == U'め' || ch == U'れ') {
    return U'え';
  }
  // O-row characters
  if (ch == U'お' || ch == U'こ' || ch == U'ご' || ch == U'そ' || ch == U'ぞ' ||
      ch == U'と' || ch == U'ど' || ch == U'の' || ch == U'ほ' || ch == U'ぼ' ||
      ch == U'ぽ' || ch == U'も' || ch == U'よ' || ch == U'ろ' || ch == U'を') {
    return U'お';
  }

  // Small kana (ゃゅょ) - treat as their base vowel
  if (ch == U'ゃ') return U'あ';
  if (ch == U'ゅ') return U'う';
  if (ch == U'ょ') return U'お';

  // Default to the character itself if not recognized
  return ch;
}

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
      char32_t vowel = getVowelForChar(prev);
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
  char32_t extended_vowel = getVowelForChar(prev_char);

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
          hiragana_part.substr(core::kJapaneseCharBytes, core::kTwoJapaneseCharBytes) == "そう") {
        // This is verb renyokei + そう pattern (with optional だ/です/etc.)
        continue;  // Skip - likely verb renyokei + そう, not i-adjective
      }
    }

    // For し + そう patterns (話しそう, 難しそう, 美味しそう, etc.), check dictionary
    // to distinguish verb renyokei + そう from adjective + そう.
    // Works for both single and multi-kanji stems.
    // 難しそう → base: 難しい (in dictionary) → allow adjective candidate
    // 美味しそう → base: 美味しい (in dictionary) → allow adjective candidate
    // 話しそう → base: 話しい (not in dictionary) → skip
    bool is_dict_adjective = false;
    if (hiragana_part.size() >= core::kThreeJapaneseCharBytes &&
        hiragana_part.substr(0, core::kJapaneseCharBytes) == "し" &&
        hiragana_part.substr(core::kJapaneseCharBytes, core::kTwoJapaneseCharBytes) == "そう") {
      if (dict_manager != nullptr) {
        // Construct base form: kanji + しい
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = kanji_stem + "しい";
        auto lookup = dict_manager->lookup(base_form, 0);
        if (lookup.empty()) {
          continue;  // Base form not in dictionary, skip adjective candidate
        }
        // Check if any entry is an adjective with exact match
        size_t base_form_chars = 0;
        for (size_t i = 0; i < base_form.size(); ) {
          auto byte = static_cast<uint8_t>(base_form[i]);
          if ((byte & 0x80) == 0) { i += 1; }
          else if ((byte & 0xE0) == 0xC0) { i += 2; }
          else if ((byte & 0xF0) == 0xE0) { i += 3; }
          else { i += 4; }
          ++base_form_chars;
        }
        for (const auto& result : lookup) {
          if (result.length == base_form_chars &&
              result.entry != nullptr &&
              result.entry->pos == core::PartOfSpeech::Adjective) {
            is_dict_adjective = true;
            break;
          }
        }
        if (!is_dict_adjective) {
          continue;  // Not an adjective in dictionary, skip
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

  // Need at least 2 kanji (stem + 的)
  if (kanji_end - start_pos < 2) {
    return candidates;
  }

  std::string kanji_seq = extractSubstring(codepoints, start_pos, kanji_end);

  // Check for na-adjective suffixes (的)
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

  // Skip if starting character is a particle that is NEVER an adjective stem
  // Note: や is NOT included because やばい, やわらかい are valid i-adjectives
  // Note: か and わ are included in isExtendedParticle, but we handle かわいい
  // specially below by checking if the sequence forms a valid adjective
  char32_t first_char = codepoints[start_pos];

  // Find hiragana sequence, breaking at particle boundaries
  // Note: For i-adjectives, we allow longer sequences because conjugation
  // patterns include か (かった), く (くない), etc.
  // Also include prolonged sound mark (ー) as part of the sequence
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() &&
         hiragana_end - start_pos < 10) {  // Max 10 chars for adjective + endings
    normalize::CharType curr_type = char_types[hiragana_end];
    char32_t curr_char = codepoints[hiragana_end];

    // Allow hiragana and prolonged sound mark (ー)
    bool is_valid = (curr_type == normalize::CharType::Hiragana);
    if (!is_valid && normalize::isProlongedSoundMark(curr_char)) {
      is_valid = true;
    }

    if (!is_valid) {
      break;
    }

    // Only break at strong particle boundaries (を, が, は, に, etc.)
    // after the first 3 characters (minimum adjective stem length)
    // This allows patterns like まずかった, おいしくない
    // But skip this check for prolonged sound marks and when followed by ー
    if (hiragana_end - start_pos >= 3 && !normalize::isProlongedSoundMark(curr_char)) {
      // Check if next char is ー - if so, don't break (e.g., かわいー)
      bool next_is_prolonged = (hiragana_end + 1 < char_types.size() &&
                                 normalize::isProlongedSoundMark(codepoints[hiragana_end + 1]));
      if (!next_is_prolonged) {
        // Break at extended particle boundaries + や
        if (normalize::isExtendedParticle(curr_char) || curr_char == U'や') {
          break;  // Stop before the particle
        }
      }
    }
    ++hiragana_end;
  }

  // Need at least 3 characters for an i-adjective (e.g., あつい)
  if (hiragana_end <= start_pos + 2) {
    return candidates;
  }

  // Check if this is an extended particle at start that should be skipped
  // unless the sequence contains a prolonged sound mark (かわいー pattern)
  if (normalize::isExtendedParticle(first_char) &&
      !containsProlongedSoundMark(codepoints, start_pos, hiragana_end)) {
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
      float confidence_threshold = has_prolonged ? 0.40F : 0.55F;
      if (cand.confidence >= confidence_threshold &&
          cand.verb_type == grammar::VerbType::IAdjective) {
        UnknownCandidate candidate;
        candidate.surface = surface;  // Keep original surface with ー
        candidate.start = start_pos;
        candidate.end = end_pos;
        candidate.pos = core::PartOfSpeech::Adjective;
        // Lower cost for higher confidence matches
        // Slightly boost prolonged sound mark patterns as they're common in colloquial speech
        float cost = 0.3F + (1.0F - cand.confidence) * 0.3F;
        if (has_prolonged) {
          cost -= 0.1F;  // Bonus for colloquial patterns
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

  // Sort by cost
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

}  // namespace suzume::analysis
