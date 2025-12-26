/**
 * @file adjective_candidates.cpp
 * @brief Adjective-based unknown word candidate generation
 */

#include "adjective_candidates.h"

#include <algorithm>

#include "suffix_candidates.h"
#include "unknown.h"

namespace suzume::analysis {

namespace {

// Na-adjective forming suffixes (〜的 patterns)
const std::vector<std::string_view> kNaAdjSuffixes = {
    "的",  // 理性的, 論理的, etc.
};

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
  if (first_hiragana == U'を' || first_hiragana == U'が' ||
      first_hiragana == U'は' || first_hiragana == U'も' ||
      first_hiragana == U'へ' || first_hiragana == U'の' ||
      first_hiragana == U'に' || first_hiragana == U'や' ||
      first_hiragana == U'て' || first_hiragana == U'で') {
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
    if (hiragana_part.size() >= 6 &&  // At least 2 hiragana (っ + く = 6 bytes)
        hiragana_part.substr(0, 3) == "っ") {  // Starts with っ
      continue;  // Skip - likely te-form contraction, not i-adjective
    }

    // Skip patterns ending with 〜んでい or 〜でい
    // These are te-form + auxiliary verb patterns (〜んでいく, 〜ている, etc.)
    // not i-adjectives. E.g., 学んでい (manande-i) = 学んでいく, not adj
    if (hiragana_part.size() >= 9 &&  // んでい = 9 bytes
        hiragana_part.substr(hiragana_part.size() - 9) == "んでい") {
      continue;  // Skip - likely te-form + aux pattern
    }
    if (hiragana_part.size() >= 6 &&  // でい = 6 bytes
        hiragana_part.substr(hiragana_part.size() - 6) == "でい") {
      continue;  // Skip - likely te-form + aux pattern
    }

    // Skip patterns ending with verb passive/potential/causative negative renyokei
    // 〜られなく, 〜れなく, 〜させなく, 〜せなく, 〜されなく are all verb forms,
    // not i-adjectives. E.g., 食べられなく = 食べられる + ない (negative renyokei)
    if (hiragana_part.size() >= 12 &&  // られなく = 12 bytes
        hiragana_part.substr(hiragana_part.size() - 12) == "られなく") {
      continue;  // Skip - passive/potential negative renyokei
    }
    if (hiragana_part.size() >= 9 &&  // れなく = 9 bytes
        hiragana_part.substr(hiragana_part.size() - 9) == "れなく") {
      continue;  // Skip - passive/potential negative renyokei
    }
    if (hiragana_part.size() >= 12 &&  // させなく = 12 bytes
        hiragana_part.substr(hiragana_part.size() - 12) == "させなく") {
      continue;  // Skip - causative negative renyokei
    }
    if (hiragana_part.size() >= 9 &&  // せなく = 9 bytes
        hiragana_part.substr(hiragana_part.size() - 9) == "せなく") {
      continue;  // Skip - causative negative renyokei
    }
    if (hiragana_part.size() >= 12 &&  // されなく = 12 bytes
        hiragana_part.substr(hiragana_part.size() - 12) == "されなく") {
      continue;  // Skip - passive negative renyokei
    }

    // Skip patterns ending with 〜れなくなった (passive negative + become + past)
    // E.g., 読まれなくなった = 読む + れる + なくなる + た
    if (hiragana_part.size() >= 21 &&  // れなくなった = 21 bytes
        hiragana_part.substr(hiragana_part.size() - 21) == "れなくなった") {
      continue;  // Skip - passive negative + become pattern
    }
    if (hiragana_part.size() >= 24 &&  // られなくなった = 24 bytes
        hiragana_part.substr(hiragana_part.size() - 24) == "られなくなった") {
      continue;  // Skip - passive/potential negative + become pattern
    }

    // Skip patterns ending with 〜なく when followed by なった/なる
    // E.g., 食べなく + なった is actually 食べなくなった (verb form)
    // Check if the text continues with なった/なる/なって
    if (hiragana_part.size() >= 6 &&  // なく = 6 bytes
        hiragana_part.substr(hiragana_part.size() - 6) == "なく" &&
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
    // Skip patterns ending with causative-passive 〜させられなくなった
    if (hiragana_part.size() >= 30 &&  // させられなくなった = 30 bytes
        hiragana_part.substr(hiragana_part.size() - 30) == "させられなくなった") {
      continue;  // Skip - causative-passive negative + become pattern
    }
    if (hiragana_part.size() >= 27 &&  // せられなくなった = 27 bytes
        hiragana_part.substr(hiragana_part.size() - 27) == "せられなくなった") {
      continue;  // Skip - causative-passive negative + become pattern
    }

    // Skip Godan verb renyokei + そう patterns (飲みそう, 降りそうだ, etc.)
    // These are verb + そう auxiliary patterns, not i-adjectives.
    // Pattern: single kanji + renyokei suffix (i-row) + そう/そうだ/そうです...
    // Renyokei suffixes: み, き, ぎ, ち, び, り, に (7 patterns)
    // Note: し is handled specially below with dictionary validation
    if (kanji_end == start_pos + 1 && hiragana_part.size() >= 9) {
      // Check if pattern starts with: renyokei suffix + そう
      std::string_view renyokei_char = std::string_view(hiragana_part).substr(0, 3);
      if ((renyokei_char == "み" || renyokei_char == "き" ||
           renyokei_char == "ぎ" || renyokei_char == "ち" ||
           renyokei_char == "び" || renyokei_char == "り" ||
           renyokei_char == "に") &&
          hiragana_part.size() >= 9 &&
          hiragana_part.substr(3, 6) == "そう") {
        // This is verb renyokei + そう pattern (with optional だ/です/etc.)
        continue;  // Skip - likely verb renyokei + そう, not i-adjective
      }

      // For し + そう patterns (話しそう, 話しそうだ, 難しそう, etc.), check dictionary
      // to distinguish verb renyokei + そう from adjective + そう.
      // 難しそう → base: 難しい (in dictionary) → allow adjective candidate
      // 話しそう → base: 話しい (not in dictionary) → skip
      if (renyokei_char == "し" &&
          hiragana_part.size() >= 9 &&
          hiragana_part.substr(3, 6) == "そう") {
        if (dict_manager != nullptr) {
          // Construct base form: kanji + しい
          std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
          std::string base_form = kanji_stem + "しい";
          auto lookup = dict_manager->lookup(base_form, 0);
          if (lookup.empty()) {
            continue;  // Base form not in dictionary, skip adjective candidate
          }
          // Check if any entry is an adjective with exact match
          // (length should match the entire base_form)
          size_t base_form_chars = 0;
          for (size_t i = 0; i < base_form.size(); ) {
            auto byte = static_cast<uint8_t>(base_form[i]);
            if ((byte & 0x80) == 0) { i += 1; }
            else if ((byte & 0xE0) == 0xC0) { i += 2; }
            else if ((byte & 0xF0) == 0xE0) { i += 3; }
            else { i += 4; }
            ++base_form_chars;
          }
          bool found_adjective = false;
          for (const auto& result : lookup) {
            if (result.length == base_form_chars &&
                result.entry != nullptr &&
                result.entry->pos == core::PartOfSpeech::Adjective) {
              found_adjective = true;
              break;
            }
          }
          if (!found_adjective) {
            continue;  // Not an adjective in dictionary, skip
          }
        }
        // If no dict_manager, fall through to generate candidate
        // (backwards compatibility)
      }
    }

    // Skip patterns that are clearly verb negatives, not adjectives
    // 〜かない, 〜がない, 〜さない, 〜たない, 〜ばない, 〜まない, 〜らない, 〜わない
    // are Godan verb mizenkei + ない patterns (書かない, 急がない, 話さない, etc.)
    // 〜しない is Suru verb + ない (説明しない, 勉強しない, etc.)
    // 〜べない is Ichidan verb + ない (食べない, etc.)
    if (hiragana_part.size() >= 9) {  // かない = 9 bytes
      std::string_view last9 = std::string_view(hiragana_part).substr(hiragana_part.size() - 9);
      if (last9 == "かない" || last9 == "がない" || last9 == "さない" ||
          last9 == "たない" || last9 == "ばない" || last9 == "まない" ||
          last9 == "らない" || last9 == "わない" || last9 == "なない" ||
          last9 == "しない" || last9 == "べない" || last9 == "めない" ||
          last9 == "せない" || last9 == "てない" || last9 == "ねない" ||
          last9 == "けない" || last9 == "げない" || last9 == "れない") {
        continue;  // Skip - verb negative pattern, not adjective
      }
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
        candidate.cost = 0.2F + (1.0F - cand.confidence) * 0.3F;
        candidate.has_suffix = false;
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
  char32_t first_char = codepoints[start_pos];
  if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
      first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
      first_char == U'か' || first_char == U'や' || first_char == U'ね' ||
      first_char == U'よ' || first_char == U'わ' || first_char == U'で' ||
      first_char == U'と' || first_char == U'も') {
    return candidates;
  }

  // Find hiragana sequence, breaking at particle boundaries
  // Note: For i-adjectives, we allow longer sequences because conjugation
  // patterns include か (かった), く (くない), etc.
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() &&
         hiragana_end - start_pos < 10 &&  // Max 10 hiragana for adjective + endings
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Only break at strong particle boundaries (を, が, は, に, etc.)
    // after the first 3 hiragana (minimum adjective stem length)
    // This allows patterns like まずかった, おいしくない
    if (hiragana_end - start_pos >= 3) {
      char32_t curr = codepoints[hiragana_end];
      if (curr == U'を' || curr == U'が' || curr == U'は' ||
          curr == U'に' || curr == U'へ' || curr == U'の' ||
          curr == U'で' || curr == U'と' || curr == U'も' ||
          curr == U'や') {
        break;  // Stop before the particle
      }
    }
    ++hiragana_end;
  }

  // Need at least 3 hiragana for an i-adjective (e.g., あつい)
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
    // 〜られなく, 〜れなく, 〜させなく, 〜せなく are all verb forms, not i-adjectives.
    // E.g., けられなく = ける + られ + ない (verb passive negative renyokei)
    // Also skip 〜かけられなく, 〜けられなく which are clearly verb patterns
    if (surface.size() >= 12 &&  // られなく = 12 bytes
        surface.substr(surface.size() - 12) == "られなく") {
      continue;  // Skip - passive/potential negative renyokei
    }
    if (surface.size() >= 9 &&  // れなく = 9 bytes
        surface.substr(surface.size() - 9) == "れなく") {
      continue;  // Skip - passive/potential negative renyokei
    }
    if (surface.size() >= 12 &&  // させなく = 12 bytes
        surface.substr(surface.size() - 12) == "させなく") {
      continue;  // Skip - causative negative renyokei
    }
    if (surface.size() >= 9 &&  // せなく = 9 bytes
        surface.substr(surface.size() - 9) == "せなく") {
      continue;  // Skip - causative negative renyokei
    }
    if (surface.size() >= 12 &&  // されなく = 12 bytes
        surface.substr(surface.size() - 12) == "されなく") {
      continue;  // Skip - passive negative renyokei
    }
    // Skip patterns ending with 〜かなく (verb negative renyokei of godan verbs)
    // E.g., いかなく = いく + ない, かかなく = かく + ない
    if (surface.size() >= 9 &&  // かなく = 9 bytes
        surface.substr(surface.size() - 9) == "かなく") {
      continue;  // Skip - godan negative renyokei
    }

    // Check all candidates for IAdjective, not just the best one
    // This handles cases where Suru interpretation may have higher confidence
    auto all_candidates = inflection.analyze(surface);
    for (const auto& cand : all_candidates) {
      // For hiragana-only adjectives, require higher confidence (0.55) than
      // kanji+hiragana adjectives (0.50) to avoid false positives like しそう → しい
      if (cand.confidence >= 0.55F &&
          cand.verb_type == grammar::VerbType::IAdjective) {
        UnknownCandidate candidate;
        candidate.surface = surface;
        candidate.start = start_pos;
        candidate.end = end_pos;
        candidate.pos = core::PartOfSpeech::Adjective;
        // Lower cost for higher confidence matches
        candidate.cost = 0.3F + (1.0F - cand.confidence) * 0.3F;
        candidate.has_suffix = false;
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
