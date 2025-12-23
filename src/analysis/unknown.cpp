#include "analysis/unknown.h"

#include <algorithm>

#include "normalize/utf8.h"

namespace suzume::analysis {

namespace {

// Suffix list for kanji compounds
struct SuffixEntry {
  std::string_view suffix;
  core::PartOfSpeech pos;
};

const std::vector<SuffixEntry> kSuffixes = {
    {"的な", core::PartOfSpeech::Other}, {"的に", core::PartOfSpeech::Other},
    {"的", core::PartOfSpeech::Other},   {"化する", core::PartOfSpeech::Verb},
    {"化", core::PartOfSpeech::Other},   {"性", core::PartOfSpeech::Other},
    {"率", core::PartOfSpeech::Other},   {"法", core::PartOfSpeech::Other},
    {"論", core::PartOfSpeech::Other},   {"者", core::PartOfSpeech::Other},
    {"家", core::PartOfSpeech::Other},   {"員", core::PartOfSpeech::Other},
    {"式", core::PartOfSpeech::Other},   {"感", core::PartOfSpeech::Other},
    {"力", core::PartOfSpeech::Other},   {"度", core::PartOfSpeech::Other},
};

// Extract substring from codepoints to UTF-8
std::string extractSubstring(const std::vector<char32_t>& codepoints,
                             size_t start, size_t end) {
  if (start >= codepoints.size() || end > codepoints.size() || start >= end) {
    return "";
  }
  std::vector<char32_t> sub(codepoints.begin() + start,
                            codepoints.begin() + end);
  return normalize::utf8::encode(sub);
}

}  // namespace

UnknownWordGenerator::UnknownWordGenerator(const UnknownOptions& options)
    : options_(options) {}

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
      // Kanji: prefer 2-4 characters
      if (length >= 2 && length <= 4) {
        return base_cost;
      }
      return base_cost + 0.5F;

    case normalize::CharType::Katakana:
      // Katakana: prefer 3-8 characters
      if (length >= 3 && length <= 8) {
        return base_cost;
      }
      return base_cost + 0.3F;

    case normalize::CharType::Alphabet:
      // Alphabet: reasonable length preferred
      if (length >= 2 && length <= 10) {
        return base_cost + 0.2F;
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
  }

  // Generate hiragana verb candidates (pure hiragana verbs like いく, くる)
  if (char_types[start_pos] == normalize::CharType::Hiragana) {
    auto hiragana_verbs =
        generateHiraganaVerbCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), hiragana_verbs.begin(),
                      hiragana_verbs.end());
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
  // Note: Exclude characters that CAN be verb stems (な→なる/なくす, て→できる, etc.)
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
        first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
        first_char == U'か' || first_char == U'や' || first_char == U'ね' ||
        first_char == U'よ' || first_char == U'わ') {
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
    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;  // Note: This is temporary
      candidate.start = start_pos;
      candidate.end = candidate_end;
      candidate.pos = getPosForType(start_type);
      candidate.cost = getCostForType(start_type, len);
      candidate.has_suffix = false;

      // Store as string and update surface
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
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateWithSuffix(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji sequence
  size_t end_pos = start_pos;
  while (end_pos < char_types.size() &&
         end_pos - start_pos < options_.max_kanji_length &&
         char_types[end_pos] == normalize::CharType::Kanji) {
    ++end_pos;
  }

  if (end_pos <= start_pos + 1) {
    return candidates;
  }

  std::string kanji_seq = extractSubstring(codepoints, start_pos, end_pos);

  // Check for suffixes
  for (const auto& [suffix, suffix_pos] : kSuffixes) {
    if (kanji_seq.size() > suffix.size() &&
        kanji_seq.compare(kanji_seq.size() - suffix.size(), suffix.size(),
                          suffix) == 0) {
      // Calculate stem length in codepoints
      auto suffix_codepoints = normalize::utf8::decode(std::string(suffix));
      size_t stem_end = end_pos - suffix_codepoints.size();

      if (stem_end > start_pos + 1) {
        // Add stem candidate
        std::string stem_surface =
            extractSubstring(codepoints, start_pos, stem_end);

        UnknownCandidate stem;
        stem.surface = stem_surface;
        stem.start = start_pos;
        stem.end = stem_end;
        stem.pos = core::PartOfSpeech::Noun;
        stem.cost = 1.0F + options_.suffix_separation_bonus;
        stem.has_suffix = false;
        candidates.push_back(stem);

        // Add whole word candidate too
        UnknownCandidate whole;
        whole.surface = kanji_seq;
        whole.start = start_pos;
        whole.end = end_pos;
        whole.pos = core::PartOfSpeech::Noun;
        whole.cost = 1.2F;
        whole.has_suffix = true;
        candidates.push_back(whole);

        break;  // Use longest matching suffix
      }
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
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

  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 10 &&  // Max 10 hiragana for conjugation
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Don't include particles that appear after the first hiragana character.
    // E.g., for "切りにする", stop at "り" to not include "にする".
    // Exception: The first hiragana can be a particle (e.g., いる, する).
    if (hiragana_end > kanji_end) {
      char32_t curr = codepoints[hiragana_end];
      if (curr == U'を' || curr == U'が' || curr == U'は' ||
          curr == U'に' || curr == U'へ' || curr == U'の' ||
          curr == U'で' || curr == U'と' || curr == U'も' ||
          curr == U'か' || curr == U'や') {
        break;  // Stop before the particle
      }
    }
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
      if (hiragana_part == "で" || hiragana_part == "に" || hiragana_part == "を" ||
          hiragana_part == "が" || hiragana_part == "は" || hiragana_part == "も" ||
          hiragana_part == "へ" || hiragana_part == "と" || hiragana_part == "や" ||
          hiragana_part == "か" || hiragana_part == "の" || hiragana_part == "から" ||
          hiragana_part == "まで" || hiragana_part == "より" || hiragana_part == "ほど" ||
          hiragana_part == "です" || hiragana_part == "だ" || hiragana_part == "だった" ||
          hiragana_part == "でした" || hiragana_part == "である") {
        continue;  // Skip particle/copula patterns
      }

      // Check if this looks like a conjugated verb
      auto best = inflection_.getBest(surface);
      if (best.confidence > 0.48F) {  // Threshold for verb detection (base forms ~0.5)
        // Verify the stem matches
        std::string expected_stem = extractSubstring(codepoints, start_pos, stem_end);
        if (best.stem == expected_stem) {
          UnknownCandidate candidate;
          candidate.surface = surface;
          candidate.start = start_pos;
          candidate.end = end_pos;
          candidate.pos = core::PartOfSpeech::Verb;
          // Lower cost for higher confidence matches
          candidate.cost = 0.4F + (1.0F - best.confidence) * 0.3F;
          candidate.has_suffix = false;
          candidates.push_back(candidate);
          // Don't break - try other stem lengths too
        }
      }
    }
  }

  // Sort by cost and return best candidates
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateHiraganaVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Skip if starting character is a particle that is NEVER a verb stem
  // Note: Exclude characters that CAN be verb stems (な→なる/なくす, て→できる, etc.)
  char32_t first_char = codepoints[start_pos];
  if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
      first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
      first_char == U'か' || first_char == U'や' || first_char == U'ね' ||
      first_char == U'よ' || first_char == U'わ') {
    return candidates;
  }

  // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
  // These are commonly mistaken for verbs (これる, それる, etc.)
  if (start_pos + 1 < codepoints.size()) {
    char32_t second_char = codepoints[start_pos + 1];
    // Check for こ/そ/あ/ど + れ/こ/ち patterns (demonstrative pronouns)
    if ((first_char == U'こ' || first_char == U'そ' ||
         first_char == U'あ' || first_char == U'ど') &&
        (second_char == U'れ' || second_char == U'こ' || second_char == U'ち')) {
      return candidates;
    }
  }

  // Find hiragana sequence, breaking at particle boundaries
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() &&
         hiragana_end - start_pos < 12 &&  // Max 12 hiragana for verb + endings
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Don't include particles that appear after the first hiragana character.
    // E.g., for "りにする", stop at "り" to not include "にする".
    if (hiragana_end > start_pos) {
      char32_t curr = codepoints[hiragana_end];
      if (curr == U'を' || curr == U'が' || curr == U'は' ||
          curr == U'に' || curr == U'へ' || curr == U'の' ||
          curr == U'で' || curr == U'と' || curr == U'も' ||
          curr == U'か' || curr == U'や') {
        break;  // Stop before the particle
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
    auto best = inflection_.getBest(surface);
    if (best.confidence > 0.48F) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Verb;
      // Lower cost for higher confidence matches
      candidate.cost = 0.5F + (1.0F - best.confidence) * 0.3F;
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
