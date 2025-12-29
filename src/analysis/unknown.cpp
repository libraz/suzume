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
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "verb_candidates.h"

namespace suzume::analysis {

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
      // Kanji: prefer 2-8 characters for compound nouns and place names
      // E.g., 神奈川県横浜市 (7 chars) should not be penalized
      // Single kanji should beat suffix entries (cost 1.5) in dictionary
      if (length >= 2 && length <= 8) {
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
      // Add length penalty for longer sequences to encourage proper segmentation
      // e.g., まじやばい should split into まじ + やばい, not stay as one word
      // Penalty: +0.5 per character beyond 3 characters
      if (length >= 4) {
        return base_cost + 1.0F + (static_cast<float>(length) - 3.0F) * 0.5F;
      }
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

  // Generate ABAB-type onomatopoeia candidates first (わくわく, きらきら, etc.)
  // This needs to be checked before isNeverVerbStemAtStart filters out わ, etc.
  if (char_types[start_pos] == normalize::CharType::Hiragana) {
    auto onomatopoeia =
        generateOnomatopoeiaCandidates(codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), onomatopoeia.begin(),
                      onomatopoeia.end());
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

    // Generate kanji + hiragana compound noun candidates
    // e.g., 玉ねぎ, 水たまり
    auto compound_nouns =
        generateKanjiHiraganaCompoundCandidates(codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), compound_nouns.begin(),
                      compound_nouns.end());
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

  // Generate katakana verb/adjective candidates (slang: バズる, エモい, etc.)
  if (char_types[start_pos] == normalize::CharType::Katakana) {
    auto kata_verbs =
        generateKatakanaVerbCandidates(codepoints, start_pos, char_types,
                                       inflection_);
    candidates.insert(candidates.end(), kata_verbs.begin(), kata_verbs.end());

    auto kata_adjs =
        generateKatakanaAdjectiveCandidates(codepoints, start_pos, char_types,
                                            inflection_);
    candidates.insert(candidates.end(), kata_adjs.begin(), kata_adjs.end());
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

  // Generate character speech candidates (キャラ語尾)
  if (options_.enable_character_speech) {
    auto char_speech =
        generateCharacterSpeechCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), char_speech.begin(), char_speech.end());
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
  // Note: Characters that CAN be verb stems are NOT skipped:
  //   - な→なる/なくす, て→できる, や→やる, か→かける/かえる
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    if (normalize::isNeverVerbStemAtStart(first_char)) {
      return candidates;  // Let dictionary handle these
    }

    // Skip small kana (拗音・促音) - Japanese words don't start with these
    // ゃゅょぁぃぅぇぉっ are always part of compound sounds (e.g., きょう not ょう)
    if (first_char == U'ゃ' || first_char == U'ゅ' || first_char == U'ょ' ||
        first_char == U'ぁ' || first_char == U'ぃ' || first_char == U'ぅ' ||
        first_char == U'ぇ' || first_char == U'ぉ' || first_char == U'っ') {
      return candidates;  // Phonologically impossible word start
    }

    // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
    // These should be recognized by dictionary lookup, not generated as unknown words.
    if (start_pos + 1 < codepoints.size()) {
      char32_t second_char = codepoints[start_pos + 1];
      if (normalize::isDemonstrativeStart(first_char, second_char)) {
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
      // Common particles + で, と, も, か (additional word boundaries)
      // Note: Don't include「や」as it's also the stem of「やる」verb
      if (normalize::isCommonParticle(curr_char) ||
          curr_char == U'で' || curr_char == U'と' ||
          curr_char == U'も' || curr_char == U'か') {
        break;  // Stop before the particle
      }
    }
    ++end_pos;
  }

  // Generate candidates for different lengths
  for (size_t len = 1; len <= end_pos - start_pos; ++len) {
    size_t candidate_end = start_pos + len;
    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = candidate_end;
      candidate.pos = getPosForType(start_type);
      candidate.cost = getCostForType(start_type, len);

      // Penalize kanji sequences ending with honorific suffixes (様, 氏)
      // to encourage NOUN + SUFFIX separation (e.g., 客様 → 客 + 様)
      if (start_type == normalize::CharType::Kanji && len >= 2) {
        char32_t last_char = codepoints[candidate_end - 1];
        if (last_char == U'様' || last_char == U'氏') {
          candidate.cost += 4.0F;  // Strong penalty to prefer NOUN + SUFFIX path
        }
      }

      candidate.has_suffix = false;
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::SameType;
      candidate.confidence = 1.0F;
      switch (start_type) {
        case normalize::CharType::Kanji: candidate.pattern = "kanji_seq"; break;
        case normalize::CharType::Katakana: candidate.pattern = "kata_seq"; break;
        case normalize::CharType::Hiragana: candidate.pattern = "hira_seq"; break;
        case normalize::CharType::Alphabet: candidate.pattern = "alpha_seq"; break;
        case normalize::CharType::Digit: candidate.pattern = "digit_seq"; break;
        default: candidate.pattern = "other_seq"; break;
      }
#endif
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
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Noun;
      candidate.cost = 0.8F;
      candidate.has_suffix = false;
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::Alphanumeric;
      candidate.confidence = 1.0F;
      candidate.pattern = "alphanum_mixed";
#endif
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

std::vector<UnknownCandidate> UnknownWordGenerator::generateCharacterSpeechCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size()) {
    return candidates;
  }

  normalize::CharType start_type = char_types[start_pos];

  // Only for hiragana or katakana starting positions
  if (start_type != normalize::CharType::Hiragana &&
      start_type != normalize::CharType::Katakana) {
    return candidates;
  }

  // Skip if starting with common particles (these are handled by dictionary)
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
        first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
        first_char == U'で' || first_char == U'と' || first_char == U'も') {
      return candidates;
    }
    // Skip small kana (ゃゅょぁぃぅぇぉっ) - these don't start words
    if (first_char == U'ゃ' || first_char == U'ゅ' || first_char == U'ょ' ||
        first_char == U'ぁ' || first_char == U'ぃ' || first_char == U'ぅ' ||
        first_char == U'ぇ' || first_char == U'ぉ' || first_char == U'っ') {
      return candidates;
    }
  }

  // Skip small katakana as well
  if (start_type == normalize::CharType::Katakana) {
    char32_t first_char = codepoints[start_pos];
    if (first_char == U'ャ' || first_char == U'ュ' || first_char == U'ョ' ||
        first_char == U'ァ' || first_char == U'ィ' || first_char == U'ゥ' ||
        first_char == U'ェ' || first_char == U'ォ' || first_char == U'ッ') {
      return candidates;
    }
  }

  // Skip common suffixes and particles that are handled by dictionary
  // These are not character speech patterns
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    if (first_char == U'た' || first_char == U'さ' || first_char == U'ら' ||
        first_char == U'く' || first_char == U'あ' || first_char == U'け') {
      return candidates;
    }
  }

  size_t max_len = options_.max_character_speech_length;
  size_t text_len = char_types.size();

  // Find end of same-type sequence (limited to max_character_speech_length)
  size_t end_pos = start_pos + 1;
  while (end_pos < text_len && end_pos - start_pos < max_len &&
         char_types[end_pos] == start_type) {
    ++end_pos;
  }

  // Check if this could be a sentence-end position
  auto isSentenceEndPosition = [&](size_t pos) -> bool {
    if (pos >= text_len) {
      return true;  // End of text
    }

    char32_t next_char = codepoints[pos];

    // Punctuation marks
    if (next_char == U'。' || next_char == U'！' || next_char == U'？' ||
        next_char == U'、' || next_char == U',' || next_char == U'.' ||
        next_char == U'!' || next_char == U'?' || next_char == U'…' ||
        next_char == U'」' || next_char == U'』' || next_char == U'"' ||
        next_char == U'\n' || next_char == U'\r') {
      return true;
    }

    // Whitespace (space, full-width space, tab)
    if (next_char == U' ' || next_char == U'\u3000' || next_char == U'\t') {
      return true;
    }

    return false;
  };

  // Generate candidates for different lengths
  for (size_t len = 1; len <= end_pos - start_pos; ++len) {
    size_t candidate_end = start_pos + len;

    // Only generate if this position could be sentence-end
    if (!isSentenceEndPosition(candidate_end)) {
      continue;
    }

    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    if (!surface.empty()) {
      // Skip patterns ending with そう - these are aspectual auxiliary patterns
      // that should be handled by verb/adjective + そう analysis, not as character speech
      if (surface.size() >= core::kTwoJapaneseCharBytes &&
          surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == "そう") {
        continue;
      }

      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = candidate_end;
      // Mark as Auxiliary so it connects properly after verbs/adjectives
      candidate.pos = core::PartOfSpeech::Auxiliary;
      candidate.cost = options_.character_speech_cost;
      candidate.has_suffix = false;
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::CharacterSpeech;
      candidate.confidence = 0.5F;
      candidate.pattern = (start_type == normalize::CharType::Hiragana)
                              ? "char_speech_hira"
                              : "char_speech_kata";
#endif
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateOnomatopoeiaCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  // Need at least 4 characters for ABAB pattern
  if (start_pos + 3 >= codepoints.size()) {
    return candidates;
  }

  // Check if we have 4 consecutive hiragana characters
  for (size_t i = 0; i < 4; ++i) {
    if (start_pos + i >= char_types.size() ||
        char_types[start_pos + i] != normalize::CharType::Hiragana) {
      return candidates;
    }
  }

  // Check ABAB pattern: characters 0-1 must match characters 2-3
  char32_t ch0 = codepoints[start_pos];
  char32_t ch1 = codepoints[start_pos + 1];
  char32_t ch2 = codepoints[start_pos + 2];
  char32_t ch3 = codepoints[start_pos + 3];

  if (ch0 == ch2 && ch1 == ch3) {
    // ABAB pattern detected (e.g., わくわく, きらきら, どきどき)
    std::string surface = extractSubstring(codepoints, start_pos, start_pos + 4);

    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = start_pos + 4;
      candidate.pos = core::PartOfSpeech::Adverb;
      candidate.cost = 0.1F;  // Very low cost to prefer over particle + adj splits
      candidate.has_suffix = false;
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::Onomatopoeia;
      candidate.confidence = 1.0F;
      candidate.pattern = "abab_pattern";
#endif
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

}  // namespace suzume::analysis
