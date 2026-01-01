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
#include "analysis/scorer_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
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
      // Kanji: prefer 2 characters as optimal (most common word length)
      // Apply graduated penalty for longer sequences to prevent over-concatenation
      // E.g., 今夏最高 should split to 今+夏+最高, not stay as single word
      // Penalties must overcome optimal_length bonus (-0.5) in scorer
      if (length == 1) {
        return base_cost + 0.4F;  // 1.4: prefer over suffix entries (1.5)
      }
      if (length == 2) {
        return base_cost;  // 2 chars: optimal (most common word length)
      }
      if (length == 3) {
        return base_cost + 0.3F;  // 3 chars: light penalty
      }
      if (length == 4) {
        return base_cost + 0.8F;  // 4 chars: moderate penalty (1.8 base)
      }
      if (length >= 5 && length <= 6) {
        return base_cost + 1.5F;  // 5-6 chars: strong penalty
      }
      return base_cost + 2.5F;  // 7+ chars: very strong penalty

    case normalize::CharType::Katakana:
      // Katakana: prefer 4+ characters for loanwords (マスカラ, デスクトップ)
      // Penalize short sequences to prevent splits like マ+スカラ
      if (length == 1) {
        return base_cost + 1.5F;  // Strong penalty for 1-char
      }
      if (length == 2) {
        return base_cost + 1.0F;  // Moderate penalty for 2-char
      }
      if (length == 3) {
        return base_cost + 0.3F;  // Light penalty for 3-char
      }
      if (length >= 4 && length <= 10) {
        return base_cost;  // Optimal: 4-10 chars
      }
      return base_cost + 0.3F;  // 11+ chars: light penalty

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
  // Also handles katakana patterns (ニャーニャー, ワンワン, etc.)
  if (char_types[start_pos] == normalize::CharType::Hiragana ||
      char_types[start_pos] == normalize::CharType::Katakana) {
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

    // Generate がち suffix candidates for kanji verb stems
    // e.g., 忘れがち, 遅れがち
    auto gachi_suffix =
        generateGachiSuffixCandidates(codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), gachi_suffix.begin(),
                      gachi_suffix.end());
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

    // Generate productive suffix candidates (ありがち, 忘れっぽい, etc.)
    auto productive_suffix =
        generateProductiveSuffixCandidates(codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), productive_suffix.begin(),
                      productive_suffix.end());
  }

  // Generate katakana verb/adjective candidates (slang: バズる, エモい, etc.)
  if (char_types[start_pos] == normalize::CharType::Katakana) {
    auto kata_verbs =
        generateKatakanaVerbCandidates(codepoints, start_pos, char_types,
                                       inflection_, options_.verb_candidate_options);
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

  // Track if sequence starts with a particle character
  // These sequences may be valid nouns (はし, はな, etc.) despite starting with particles
  bool started_with_particle = false;

  // For hiragana starting with common particle characters (は, に, へ, の),
  // we still generate candidates but with a penalty, as they could be nouns.
  // Examples: はし (橋/箸), はな (花/鼻), にく (肉), へや (部屋), のり (海苔), etc.
  // Note: を, が are excluded - they almost never start nouns
  // Note: よ, わ are excluded - they are sentence-final particles
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    // Only は, に, へ, の can start hiragana nouns
    if (first_char == U'は' || first_char == U'に' ||
        first_char == U'へ' || first_char == U'の') {
      started_with_particle = true;  // Generate but with penalty
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
  while (end_pos < char_types.size() && end_pos - start_pos < max_len) {
    normalize::CharType curr_type = char_types[end_pos];
    char32_t curr_char = codepoints[end_pos];

    // Check if current character matches the sequence type
    bool matches_type = (curr_type == start_type);

    // Special handling for prolonged sound mark (ー) in hiragana sequences
    // Colloquial expressions like すごーい, やばーい, かわいー use ー in hiragana
    // Also handle consecutive prolonged marks: すごーーい, やばーーーい
    if (!matches_type && start_type == normalize::CharType::Hiragana &&
        normalize::isProlongedSoundMark(curr_char)) {
      // Check if followed by hiragana, another ー, or end of text (かわいー)
      if (end_pos + 1 >= char_types.size() ||
          char_types[end_pos + 1] == normalize::CharType::Hiragana ||
          normalize::isProlongedSoundMark(codepoints[end_pos + 1])) {
        matches_type = true;  // Treat ー as part of hiragana sequence
      }
    }

    // Special handling for emoji modifiers (ZWJ, variation selectors, skin tones)
    // These should always be grouped with the preceding emoji
    if (!matches_type && start_type == normalize::CharType::Emoji &&
        normalize::isEmojiModifier(curr_char)) {
      matches_type = true;  // Treat modifiers as part of emoji sequence
    }

    // Special handling for regional indicators (country flags)
    // Two regional indicators together form a flag emoji (e.g., 🇯🇵)
    if (!matches_type && start_type == normalize::CharType::Emoji &&
        normalize::isRegionalIndicator(curr_char)) {
      matches_type = true;  // Treat regional indicators as part of emoji sequence
    }

    // Special handling for ideographic iteration mark (々) in kanji sequences
    // e.g., 人々, 日々, 堂々, 時々 should be grouped as single tokens
    // The iteration mark U+3005 is classified as Symbol, but it should be
    // treated as part of the kanji sequence when following kanji
    if (!matches_type && start_type == normalize::CharType::Kanji &&
        normalize::isIterationMark(curr_char)) {
      matches_type = true;  // Treat 々 as part of kanji sequence
    }

    if (!matches_type) {
      break;
    }

    // For hiragana, break at common particle characters to avoid
    // swallowing particles into unknown words (e.g., don't create "ぎをみじん")
    if (start_type == normalize::CharType::Hiragana) {
      // Always break at を and が (case particles that never start words)
      // This applies even if we started with a particle character
      if (curr_char == U'を' || curr_char == U'が') {
        break;
      }
      // For non-particle starts, also break at other particles
      // This allows generating nouns like はし, はな, にく, etc.
      if (!started_with_particle) {
        // Common particles (は, に, へ, の) + で, と, も, か (word boundaries)
        // Note: Don't include「や」as it's also the stem of「やる」verb
        if (curr_char == U'は' || curr_char == U'に' || curr_char == U'へ' ||
            curr_char == U'の' || curr_char == U'で' || curr_char == U'と' ||
            curr_char == U'も' || curr_char == U'か') {
          break;  // Stop before the particle
        }
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
      // Particle-start hiragana sequences are potential nouns (はし, はな, にく)
      // Use NOUN POS instead of OTHER to avoid exceeds_dict_length penalty
      candidate.pos = started_with_particle ? core::PartOfSpeech::Noun
                                            : getPosForType(start_type);
      candidate.cost = getCostForType(start_type, len);

      // Penalize kanji sequences ending with honorific suffixes (様, 氏)
      // to encourage NOUN + SUFFIX separation (e.g., 客様 → 客 + 様)
      if (start_type == normalize::CharType::Kanji && len >= 2) {
        char32_t last_char = codepoints[candidate_end - 1];
        if (last_char == U'様' || last_char == U'氏') {
          candidate.cost += 4.0F;  // Strong penalty to prefer NOUN + SUFFIX path
        }
      }

      // Penalize kanji sequences that extend past iteration mark (々)
      // e.g., 時々妙 should be split as 時々 + 妙, not kept as one compound
      // The pattern kanji+々 is a complete reduplication that rarely extends further
      if (start_type == normalize::CharType::Kanji && len >= 3) {
        for (size_t i = start_pos + 1; i < candidate_end - 1; ++i) {
          if (normalize::isIterationMark(codepoints[i])) {
            // Found 々 in the middle - penalize extending past it
            candidate.cost += 5.0F;
            break;
          }
        }
      }

      // Penalize hiragana sequences starting with particle characters
      // These could be nouns (はし, はな, にく) but are less likely than
      // the particle interpretation, unless the particle path has connection penalties
      // Constraints:
      // - Only len=2 is allowed (typical pattern: は+し, に+く, の+り)
      // - Longer sequences are too risky (e.g., によれ should be に+よれ, not a noun)
      if (started_with_particle) {
        if (len != 2) {
          continue;  // Only generate 2-char candidates
        }
        // Add moderate penalty - let connection rules decide which path is better
        candidate.cost += 1.0F;
        // Mark as has_suffix to skip exceeds_dict_length penalty in tokenizer
        // These are morphologically recognized patterns (potential nouns)
        candidate.has_suffix = true;
      } else {
        candidate.has_suffix = false;
      }
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::SameType;
      candidate.confidence = started_with_particle ? 0.7F : 1.0F;
      switch (start_type) {
        case normalize::CharType::Kanji: candidate.pattern = "kanji_seq"; break;
        case normalize::CharType::Katakana: candidate.pattern = "kata_seq"; break;
        case normalize::CharType::Hiragana:
          candidate.pattern = started_with_particle ? "hira_noun_seq" : "hira_seq";
          break;
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
      codepoints, start_pos, char_types, inflection_, dict_manager_,
      options_.verb_candidate_options);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateVerbCandidates(
      codepoints, start_pos, char_types, inflection_, dict_manager_,
      options_.verb_candidate_options);
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateHiraganaVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Delegate to the standalone function
  return analysis::generateHiraganaVerbCandidates(
      codepoints, start_pos, char_types, inflection_, dict_manager_,
      options_.verb_candidate_options);
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
    if (normalize::isExtendedParticle(first_char)) {
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
          surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == scorer::kSuffixSou) {
        continue;
      }

      // Calculate character count (not byte count)
      size_t char_count = surface.size() / core::kJapaneseCharBytes;

      // Apply length-based penalty for character speech
      // Short patterns (1-2 chars) like ぜ, のだ are common
      // Longer patterns like まむぎ (3+ chars) are rare
      float length_penalty = 0.0F;
      if (char_count >= 3) {
        // Penalty increases with length: 3chars=+2.0, 4chars=+4.0, etc.
        length_penalty = static_cast<float>(char_count - 2) * 2.0F;
      }

      // Katakana character speech is very rare (most katakana are loanword nouns)
      // Apply penalty to prefer NOUN interpretation for katakana words like パン, キロ
      if (start_type == normalize::CharType::Katakana) {
        length_penalty += 0.8F;  // Prefer katakana NOUN over char_speech AUX
      }

      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = candidate_end;
      // Mark as Auxiliary so it connects properly after verbs/adjectives
      candidate.pos = core::PartOfSpeech::Auxiliary;
      candidate.cost = options_.character_speech_cost + length_penalty;
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

  // Need at least 4 characters for patterns
  if (start_pos + 3 >= codepoints.size()) {
    return candidates;
  }

  normalize::CharType start_type = char_types[start_pos];

  // Helper to check if a character belongs to the same script group or is a modifier
  auto isSameScriptOrModifier = [&](size_t pos) -> bool {
    if (pos >= char_types.size()) return false;
    if (pos >= codepoints.size()) return false;
    // Same char type
    if (char_types[pos] == start_type) return true;
    // Prolonged sound mark (ー) can appear in both hiragana and katakana words
    if (normalize::isProlongedSoundMark(codepoints[pos])) return true;
    return false;
  };

  // Helper to check if a character is small kana (part of previous mora)
  auto isSmallKanaAt = [&](size_t pos) -> bool {
    if (pos >= codepoints.size()) return false;
    std::string ch = normalize::encodeRange(codepoints, pos, pos + 1);
    return grammar::isSmallKana(ch);
  };

  // Find the extent of same-script sequence (including ー)
  size_t seq_end = start_pos;
  while (seq_end < codepoints.size() && isSameScriptOrModifier(seq_end)) {
    ++seq_end;
  }

  size_t seq_len = seq_end - start_pos;

  // Try AA pattern: first half equals second half (ニャーニャー, ワンワン)
  // Sequence must have even length and be at least 4 chars
  if (seq_len >= 4 && seq_len % 2 == 0) {
    size_t half_len = seq_len / 2;
    bool is_aa = true;

    // Check if first half equals second half
    for (size_t i = 0; i < half_len; ++i) {
      if (codepoints[start_pos + i] != codepoints[start_pos + half_len + i]) {
        is_aa = false;
        break;
      }
    }

    if (is_aa) {
      // Verify the first char of each half is not small kana
      // (small kana should be part of previous mora, not start a unit)
      if (!isSmallKanaAt(start_pos) && !isSmallKanaAt(start_pos + half_len)) {
        std::string surface = extractSubstring(codepoints, start_pos, seq_end);

        if (!surface.empty()) {
          UnknownCandidate candidate;
          candidate.surface = surface;
          candidate.start = start_pos;
          candidate.end = seq_end;
          candidate.pos = core::PartOfSpeech::Adverb;
          candidate.cost = -1.0F;  // Very strong preference for doubled patterns
          candidate.has_suffix = true;  // Skip exceeds_dict_length penalty
#ifdef SUZUME_DEBUG_INFO
          candidate.origin = CandidateOrigin::Onomatopoeia;
          candidate.confidence = 1.0F;
          candidate.pattern = "aa_doubled";
#endif
          candidates.push_back(candidate);
          return candidates;  // Found a match, return early
        }
      }
    }
  }

  // Try ABAB pattern for exactly 4 chars (traditional pattern)
  if (seq_len >= 4) {
    // Check if all 4 chars are the expected type
    bool valid = true;
    for (size_t i = 0; i < 4; ++i) {
      if (!isSameScriptOrModifier(start_pos + i)) {
        valid = false;
        break;
      }
    }

    if (valid) {
      char32_t ch0 = codepoints[start_pos];
      char32_t ch1 = codepoints[start_pos + 1];
      char32_t ch2 = codepoints[start_pos + 2];
      char32_t ch3 = codepoints[start_pos + 3];

      if (ch0 == ch2 && ch1 == ch3 && !isSmallKanaAt(start_pos)) {
        // ABAB pattern detected (e.g., わくわく, きらきら, どきどき)
        std::string surface = extractSubstring(codepoints, start_pos, start_pos + 4);

        if (!surface.empty()) {
          UnknownCandidate candidate;
          candidate.surface = surface;
          candidate.start = start_pos;
          candidate.end = start_pos + 4;
          candidate.pos = core::PartOfSpeech::Adverb;
          candidate.cost = 0.1F;  // Very low cost to prefer over particle + adj splits
          candidate.has_suffix = true;  // Skip exceeds_dict_length penalty
#ifdef SUZUME_DEBUG_INFO
          candidate.origin = CandidateOrigin::Onomatopoeia;
          candidate.confidence = 1.0F;
          candidate.pattern = "abab_pattern";
#endif
          candidates.push_back(candidate);
        }
      }
    }
  }

  return candidates;
}

}  // namespace suzume::analysis
