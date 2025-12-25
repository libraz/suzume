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
    const grammar::Inflection& inflection) {
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

    auto best = inflection.getBest(surface);
    // Require confidence >= 0.5 for i-adjectives
    // Base forms like 寒い get exactly 0.5, conjugated forms like 美しかった get 0.68+
    if (best.confidence >= 0.5F &&
        best.verb_type == grammar::VerbType::IAdjective) {
      // Filter out false positives: いたす honorific pattern
      // Invalid patterns (all have た after the candidate):
      //   - サ変名詞 + いたす: 検討いたします, 勉強いたしました
      //   - Verb renyokei + いたす: 伝えいたします, 申しいたします
      // Valid patterns:
      //   - 面白いな (next char is な)
      //   - 寒いよ (next char is よ)
      //   - 面白い (end of text)
      // Key insight: if minimum confidence (0.5) and next char is た, skip
      if (best.confidence <= 0.5F) {
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
      candidate.cost = 0.2F + (1.0F - best.confidence) * 0.3F;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
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

    // Check if this looks like a conjugated i-adjective
    auto best = inflection.getBest(surface);
    if (best.confidence >= 0.5F &&
        best.verb_type == grammar::VerbType::IAdjective) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Adjective;
      // Lower cost for higher confidence matches
      candidate.cost = 0.3F + (1.0F - best.confidence) * 0.3F;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
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
