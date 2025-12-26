/**
 * @file suffix_candidates.cpp
 * @brief Suffix-based unknown word candidate generation
 */

#include "suffix_candidates.h"

#include "normalize/utf8.h"
#include "unknown.h"

namespace suzume::analysis {

std::string extractSubstring(const std::vector<char32_t>& codepoints,
                             size_t start, size_t end) {
  // Use encodeRange directly to avoid intermediate vector allocation
  return normalize::encodeRange(codepoints, start, end);
}

const std::vector<SuffixEntry>& getSuffixEntries() {
  static const std::vector<SuffixEntry> kSuffixes = {
      {"化する", core::PartOfSpeech::Verb},
      {"化", core::PartOfSpeech::Other},
      {"性", core::PartOfSpeech::Other},
      {"率", core::PartOfSpeech::Other},
      {"法", core::PartOfSpeech::Other},
      {"論", core::PartOfSpeech::Other},
      {"者", core::PartOfSpeech::Other},
      {"家", core::PartOfSpeech::Other},
      {"員", core::PartOfSpeech::Other},
      {"式", core::PartOfSpeech::Other},
      {"感", core::PartOfSpeech::Other},
      {"力", core::PartOfSpeech::Other},
      {"度", core::PartOfSpeech::Other},
  };
  return kSuffixes;
}

const std::vector<std::string_view>& getNaAdjSuffixes() {
  static const std::vector<std::string_view> kNaAdjSuffixes = {
      "的",  // 理性的, 論理的, etc.
  };
  return kNaAdjSuffixes;
}

std::vector<UnknownCandidate> generateWithSuffix(
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
  size_t end_pos = start_pos;
  while (end_pos < char_types.size() &&
         end_pos - start_pos < options.max_kanji_length &&
         char_types[end_pos] == normalize::CharType::Kanji) {
    ++end_pos;
  }

  if (end_pos <= start_pos + 1) {
    return candidates;
  }

  std::string kanji_seq = extractSubstring(codepoints, start_pos, end_pos);
  const auto& suffixes = getSuffixEntries();

  // Check for suffixes
  for (const auto& [suffix, suffix_pos] : suffixes) {
    if (kanji_seq.size() > suffix.size() &&
        kanji_seq.compare(kanji_seq.size() - suffix.size(), suffix.size(),
                          suffix) == 0) {
      // Calculate stem length in codepoints
      auto suffix_codepoints = normalize::utf8::decode(std::string(suffix));
      size_t stem_end = end_pos - suffix_codepoints.size();

      if (stem_end > start_pos + 1) {
        // Add stem candidate
        std::string stem_surface = extractSubstring(codepoints, start_pos, stem_end);

        UnknownCandidate stem;
        stem.surface = stem_surface;
        stem.start = start_pos;
        stem.end = stem_end;
        stem.pos = core::PartOfSpeech::Noun;
        stem.cost = 1.0F + options.suffix_separation_bonus;
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

std::vector<UnknownCandidate> generateNominalizedNounCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (typically 1-3 characters for nominalized nouns)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 4 &&  // Max 4 kanji
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  // Need at least 1 kanji
  if (kanji_end == start_pos) {
    return candidates;
  }

  // Look for 1-2 hiragana after kanji (nominalization endings)
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  char32_t first_hiragana = codepoints[kanji_end];

  // Skip particles that never form nominalizations
  if (first_hiragana == U'を' || first_hiragana == U'が' ||
      first_hiragana == U'は' || first_hiragana == U'も' ||
      first_hiragana == U'へ' || first_hiragana == U'の' ||
      first_hiragana == U'に' || first_hiragana == U'で' ||
      first_hiragana == U'と' || first_hiragana == U'や' ||
      first_hiragana == U'か') {
    return candidates;
  }

  // Common nominalization endings (renyokei stems)
  bool is_nominalization_ending = (
      first_hiragana == U'け' || first_hiragana == U'げ' ||
      first_hiragana == U'せ' || first_hiragana == U'い' ||
      first_hiragana == U'り' || first_hiragana == U'ち' ||
      first_hiragana == U'き' || first_hiragana == U'ぎ' ||
      first_hiragana == U'し' || first_hiragana == U'み' ||
      first_hiragana == U'び' || first_hiragana == U'え' ||
      first_hiragana == U'れ' || first_hiragana == U'め');

  if (!is_nominalization_ending) {
    return candidates;
  }

  // Check for 1 or 2 hiragana (e.g., け or 上げ)
  size_t hiragana_end = kanji_end + 1;

  // Check for 2-hiragana patterns if second char is also valid
  if (hiragana_end < char_types.size() &&
      char_types[hiragana_end] == normalize::CharType::Hiragana) {
    char32_t second_hiragana = codepoints[hiragana_end];
    // Common 2-char nominalization endings
    if (second_hiragana == U'げ' || second_hiragana == U'け' ||
        second_hiragana == U'り' || second_hiragana == U'い' ||
        second_hiragana == U'え' || second_hiragana == U'し') {
      // Generate 2-hiragana candidate
      std::string surface = extractSubstring(codepoints, start_pos, hiragana_end + 1);
      if (!surface.empty()) {
        UnknownCandidate candidate;
        candidate.surface = surface;
        candidate.start = start_pos;
        candidate.end = hiragana_end + 1;
        candidate.pos = core::PartOfSpeech::Noun;
        candidate.cost = 0.8F;
        candidate.has_suffix = false;
        candidates.push_back(candidate);
      }
    }
  }

  // Generate 1-hiragana candidate
  bool skip_single_char = false;
  if (kanji_end + 1 < char_types.size() &&
      char_types[kanji_end + 1] == normalize::CharType::Hiragana) {
    char32_t next_char = codepoints[kanji_end + 1];
    if (next_char == U'な') {
      skip_single_char = true;
    }
  }

  if (!skip_single_char) {
    std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = kanji_end + 1;
      candidate.pos = core::PartOfSpeech::Noun;
      candidate.cost = 1.2F;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

}  // namespace suzume::analysis
