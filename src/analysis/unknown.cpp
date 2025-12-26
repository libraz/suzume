/**
 * @file unknown.cpp
 * @brief Unknown word candidate generation orchestrator
 *
 * This file delegates specialized candidate generation to:
 * - suffix_candidates.h: Suffix-based and nominalized noun candidates
 * - adjective_candidates.h: I-adjective and na-adjective candidates
 * - verb_candidates.h: Verb and compound verb candidates
 */

#include "analysis/unknown.h"

#include <algorithm>

#include "adjective_candidates.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "verb_candidates.h"

namespace suzume::analysis {

namespace {

// Extract substring from codepoints to UTF-8 (local copy for internal use)
std::string extractSubstringLocal(const std::vector<char32_t>& codepoints,
                                   size_t start, size_t end) {
  // Use encodeRange directly to avoid intermediate vector allocation
  return normalize::encodeRange(codepoints, start, end);
}

}  // namespace

UnknownWordGenerator::UnknownWordGenerator(
    const UnknownOptions& options,
    const dictionary::DictionaryManager* dict_manager)
    : options_(options), dict_manager_(dict_manager) {}

size_t UnknownWordGenerator::getMaxLength(normalize::CharType ctype) const {
  switch (ctype) {
    case normalize::CharType::Kanji:
      return options_.max_kanji_length;
    case normalize::CharType::Katakana:
      return options_.max_katakana_length;
    case normalize::CharType::Hiragana:
      return options_.max_hiragana_length;
    case normalize::CharType::Alphabet:
      return options_.max_alphabet_length;
    case normalize::CharType::Digit:
      return options_.max_alphanumeric_length;
    default:
      return options_.max_unknown_length;
  }
}

core::PartOfSpeech UnknownWordGenerator::getPosForType(normalize::CharType ctype) {
  switch (ctype) {
    case normalize::CharType::Kanji:
    case normalize::CharType::Katakana:
    case normalize::CharType::Alphabet:
    case normalize::CharType::Digit:
      return core::PartOfSpeech::Noun;
    case normalize::CharType::Hiragana:
      return core::PartOfSpeech::Other;
    default:
      return core::PartOfSpeech::Symbol;
  }
}

float UnknownWordGenerator::getCostForType(normalize::CharType ctype, size_t length) {
  float base_cost = 1.0F;

  switch (ctype) {
    case normalize::CharType::Kanji:
      // Kanji: prefer 2-4 characters, but single kanji is also common (家, 人, 本)
      // Single kanji should beat suffix entries (cost 1.5) in dictionary
      if (length >= 2 && length <= 4) {
        return base_cost;
      }
      if (length == 1) {
        return base_cost + 0.4F;  // 1.4: prefer over suffix entries (1.5)
      }
      return base_cost + 0.5F;

    case normalize::CharType::Katakana:
      // Katakana: prefer 3-8 characters
      if (length >= 3 && length <= 8) {
        return base_cost;
      }
      return base_cost + 0.3F;

    case normalize::CharType::Alphabet:
      // Alphabet: prefer longer sequences for identifiers/words
      // Longer sequences (like "getUserData") should not be penalized
      if (length >= 2 && length <= 20) {
        // Give bonus to longer sequences to prefer them over splits
        // This helps keep "getUserData" together vs "getUser" + "Data"
        float length_bonus = (length >= 8) ? -0.3F : 0.0F;
        return base_cost + 0.2F + length_bonus;
      }
      return base_cost + 0.5F;

    case normalize::CharType::Digit:
      // Digits: always reasonable
      return base_cost - 0.2F;

    case normalize::CharType::Hiragana:
      // Hiragana only: usually function words
      return base_cost + 1.0F;

    default:
      return base_cost + 1.5F;
  }
}

std::vector<UnknownCandidate> UnknownWordGenerator::generate(
    std::string_view text, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size()) {
    return candidates;
  }

  // Generate verb candidates (kanji + hiragana conjugation endings)
  if (char_types[start_pos] == normalize::CharType::Kanji) {
    auto verbs = generateVerbCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), verbs.begin(), verbs.end());

    // Generate compound verb candidates (kanji + hiragana + kanji + hiragana)
    // e.g., 恐れ入ります, 差し上げます, 申し上げます
    auto compound_verbs =
        generateCompoundVerbCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), compound_verbs.begin(),
                      compound_verbs.end());

    // Generate i-adjective candidates (kanji + hiragana conjugation endings)
    auto adjs =
        generateAdjectiveCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), adjs.begin(), adjs.end());

    // Generate na-adjective candidates (〜的 patterns)
    auto na_adjs =
        generateNaAdjectiveCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), na_adjs.begin(), na_adjs.end());

    // Generate nominalized noun candidates (kanji + short hiragana)
    // e.g., 手助け, 片付け, 引き上げ
    auto nom_nouns =
        generateNominalizedNounCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), nom_nouns.begin(), nom_nouns.end());
  }

  // Generate hiragana verb candidates (pure hiragana verbs like いく, くる)
  if (char_types[start_pos] == normalize::CharType::Hiragana) {
    auto hiragana_verbs =
        generateHiraganaVerbCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), hiragana_verbs.begin(),
                      hiragana_verbs.end());

    // Generate hiragana i-adjective candidates (まずい, おいしい, etc.)
    auto hiragana_adjs =
        generateHiraganaAdjectiveCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), hiragana_adjs.begin(),
                      hiragana_adjs.end());
  }

  // Generate by same type
  auto same_type = generateBySameType(text, codepoints, start_pos, char_types);
  candidates.insert(candidates.end(), same_type.begin(), same_type.end());

  // Generate alphanumeric sequences
  auto alphanum = generateAlphanumeric(text, codepoints, start_pos, char_types);
  candidates.insert(candidates.end(), alphanum.begin(), alphanum.end());

  // Generate with suffix separation for kanji
  if (options_.separate_suffix &&
      char_types[start_pos] == normalize::CharType::Kanji) {
    auto suffix = generateWithSuffix(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), suffix.begin(), suffix.end());
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateBySameType(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size()) {
    return candidates;
  }

  normalize::CharType start_type = char_types[start_pos];

  // For hiragana starting with single-character particles that are NEVER verb stems,
  // don't generate unknown candidates. These should be recognized by dictionary lookup.
  // Note: Exclude characters that CAN be verb stems:
  //   - な→なる/なくす
  //   - て→できる
  //   - や→やる (important: must NOT skip や)
  //   - か→かける/かえる/かう/かく (important: must NOT skip か)
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
        first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
        first_char == U'ね' || first_char == U'よ' || first_char == U'わ') {
      return candidates;  // Let dictionary handle these
    }

    // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
    // These should be recognized by dictionary lookup, not generated as unknown words.
    if (start_pos + 1 < codepoints.size()) {
      char32_t second_char = codepoints[start_pos + 1];
      // Check for こ/そ/あ/ど + れ/こ/ち patterns (demonstrative pronouns)
      if ((first_char == U'こ' || first_char == U'そ' ||
           first_char == U'あ' || first_char == U'ど') &&
          (second_char == U'れ' || second_char == U'こ' || second_char == U'ち')) {
        return candidates;
      }
    }
  }

  size_t max_len = getMaxLength(start_type);

  // Find end of same-type sequence
  size_t end_pos = start_pos + 1;
  while (end_pos < char_types.size() && end_pos - start_pos < max_len &&
         char_types[end_pos] == start_type) {
    // For hiragana, break at common particle characters to avoid
    // swallowing particles into unknown words (e.g., don't create "ぎをみじん")
    if (start_type == normalize::CharType::Hiragana) {
      char32_t curr_char = codepoints[end_pos];
      // Note: Don't include「や」as it's also the stem of「やる」verb
      if (curr_char == U'を' || curr_char == U'が' || curr_char == U'は' ||
          curr_char == U'に' || curr_char == U'へ' || curr_char == U'の' ||
          curr_char == U'で' || curr_char == U'と' || curr_char == U'も' ||
          curr_char == U'か') {
        break;  // Stop before the particle
      }
    }
    ++end_pos;
  }

  // Generate candidates for different lengths
  for (size_t len = 1; len <= end_pos - start_pos; ++len) {
    size_t candidate_end = start_pos + len;
    std::string surface = extractSubstringLocal(codepoints, start_pos, candidate_end);

    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = candidate_end;
      candidate.pos = getPosForType(start_type);
      candidate.cost = getCostForType(start_type, len);
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateAlphanumeric(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size()) {
    return candidates;
  }

  normalize::CharType start_type = char_types[start_pos];

  // Only for alphabet or digit start
  if (start_type != normalize::CharType::Alphabet &&
      start_type != normalize::CharType::Digit) {
    return candidates;
  }

  // Find mixed alphanumeric sequence
  size_t end_pos = start_pos;
  bool has_alpha = false;
  bool has_digit = false;

  while (end_pos < char_types.size() &&
         end_pos - start_pos < options_.max_alphanumeric_length) {
    normalize::CharType ctype = char_types[end_pos];
    if (ctype == normalize::CharType::Alphabet) {
      has_alpha = true;
      ++end_pos;
    } else if (ctype == normalize::CharType::Digit) {
      has_digit = true;
      ++end_pos;
    } else {
      break;
    }
  }

  // Only add if mixed (pure sequences handled by generateBySameType)
  if (has_alpha && has_digit && end_pos > start_pos + 1) {
    std::string surface = extractSubstringLocal(codepoints, start_pos, end_pos);

    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Noun;
      candidate.cost = 0.8F;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateWithSuffix(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateWithSuffix(codepoints, start_pos, char_types, options_);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateCompoundVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateCompoundVerbCandidates(
      codepoints, start_pos, char_types, inflection_, dict_manager_);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateVerbCandidates(
      codepoints, start_pos, char_types, inflection_, dict_manager_);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateHiraganaVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateHiraganaVerbCandidates(
      codepoints, start_pos, char_types, inflection_, dict_manager_);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateAdjectiveCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateAdjectiveCandidates(
      codepoints, start_pos, char_types, inflection_, dict_manager_);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateHiraganaAdjectiveCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateHiraganaAdjectiveCandidates(
      codepoints, start_pos, char_types, inflection_);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateNaAdjectiveCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateNaAdjectiveCandidates(
      codepoints, start_pos, char_types, options_);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateNominalizedNounCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateNominalizedNounCandidates(codepoints, start_pos, char_types);
}

}  // namespace suzume::analysis
