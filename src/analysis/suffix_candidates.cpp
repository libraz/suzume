/**
 * @file suffix_candidates.cpp
 * @brief Suffix-based unknown word candidate generation
 */

#include "suffix_candidates.h"

#include "normalize/exceptions.h"
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
      {"化", core::PartOfSpeech::Suffix},
      {"性", core::PartOfSpeech::Suffix},
      {"率", core::PartOfSpeech::Suffix},
      {"法", core::PartOfSpeech::Suffix},
      {"論", core::PartOfSpeech::Suffix},
      {"者", core::PartOfSpeech::Suffix},
      {"家", core::PartOfSpeech::Suffix},
      {"員", core::PartOfSpeech::Suffix},
      {"式", core::PartOfSpeech::Suffix},
      {"感", core::PartOfSpeech::Suffix},
      {"力", core::PartOfSpeech::Suffix},
      {"度", core::PartOfSpeech::Suffix},
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
#ifdef SUZUME_DEBUG_INFO
        stem.origin = CandidateOrigin::SuffixPattern;
        stem.confidence = 1.0F;
        stem.pattern = "stem_before_" + std::string(suffix);
#endif
        candidates.push_back(stem);

        // Add whole word candidate too
        UnknownCandidate whole;
        whole.surface = kanji_seq;
        whole.start = start_pos;
        whole.end = end_pos;
        whole.pos = core::PartOfSpeech::Noun;
        whole.cost = 1.2F;
        whole.has_suffix = true;
#ifdef SUZUME_DEBUG_INFO
        whole.origin = CandidateOrigin::SuffixPattern;
        whole.confidence = 1.0F;
        whole.pattern = "with_suffix_" + std::string(suffix);
#endif
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
  if (normalize::isParticleCodepoint(first_hiragana)) {
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
#ifdef SUZUME_DEBUG_INFO
        candidate.origin = CandidateOrigin::NominalizedNoun;
        candidate.confidence = 0.8F;
        candidate.pattern = "nominalized_2hira";
#endif
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
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::NominalizedNoun;
      candidate.confidence = 0.6F;
      candidate.pattern = "nominalized_1hira";
#endif
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> generateKanjiHiraganaCompoundCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (1 character only for compound nouns)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 1 &&
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  size_t kanji_len = kanji_end - start_pos;
  if (kanji_len == 0) {
    return candidates;
  }

  // Need hiragana after kanji
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Find hiragana portion (2-4 characters)
  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 4 &&
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    char32_t ch = codepoints[hiragana_end];
    if (normalize::isParticleCodepoint(ch)) {
      break;
    }
    ++hiragana_end;
  }

  size_t hiragana_len = hiragana_end - kanji_end;
  if (hiragana_len < 2) {
    return candidates;
  }

  char32_t first_hira = codepoints[kanji_end];
  char32_t second_hira = (hiragana_len >= 2) ? codepoints[kanji_end + 1] : 0;

  // Skip small kana at start - morphologically invalid
  if (first_hira == U'ゃ' || first_hira == U'ゅ' || first_hira == U'ょ' ||
      first_hira == U'ぁ' || first_hira == U'ぃ' || first_hira == U'ぅ' ||
      first_hira == U'ぇ' || first_hira == U'ぉ' || first_hira == U'っ') {
    return candidates;
  }

  // Skip patterns ending with ん - likely honorific suffixes
  // e.g., さん, くん, ちゃん, たん should split as NOUN + SUFFIX
  // This is a grammatical pattern: hiragana ending with ん after single kanji
  // is typically an honorific suffix, not a compound noun
  if (kanji_len == 1 && hiragana_len >= 2) {
    char32_t last_hira = codepoints[hiragana_end - 1];
    if (last_hira == U'ん') {
      return candidates;
    }
  }

  // Check if pattern looks like a grammatical suffix
  // These get high cost to let verb/adjective candidates win
  bool looks_like_aux = false;

  if (hiragana_len >= 2) {
    // te/ta form, copula patterns
    if (second_hira == U'て' || second_hira == U'た' ||
        second_hira == U'で' || second_hira == U'だ') {
      looks_like_aux = true;
    }
    // ます, ない
    if ((first_hira == U'ま' && second_hira == U'す') ||
        (first_hira == U'な' && second_hira == U'い')) {
      looks_like_aux = true;
    }
    // れる, られる, せる, させる
    if ((first_hira == U'れ' && second_hira == U'る') ||
        (first_hira == U'せ' && second_hira == U'る')) {
      looks_like_aux = true;
    }
    // だった, だろう
    if (first_hira == U'だ' && (second_hira == U'っ' || second_hira == U'ろ')) {
      looks_like_aux = true;
    }
    // なら, なかった
    if (first_hira == U'な' && (second_hira == U'ら' || second_hira == U'か')) {
      looks_like_aux = true;
    }
    // Renyokei + そう/たい/ます
    bool is_renyokei = (first_hira == U'し' || first_hira == U'み' ||
                        first_hira == U'き' || first_hira == U'ぎ' ||
                        first_hira == U'ち' || first_hira == U'り' ||
                        first_hira == U'い' || first_hira == U'び');
    if (is_renyokei && (second_hira == U'そ' || second_hira == U'た' ||
                        second_hira == U'ま')) {
      looks_like_aux = true;
    }
  }

  // Ichidan verb pattern (e-row + る)
  bool is_e_row = (first_hira == U'え' || first_hira == U'け' ||
                   first_hira == U'げ' || first_hira == U'せ' ||
                   first_hira == U'て' || first_hira == U'ね' ||
                   first_hira == U'べ' || first_hira == U'め' ||
                   first_hira == U'れ');
  if (is_e_row && hiragana_len == 2 && second_hira == U'る') {
    looks_like_aux = true;
  }

  // Patterns ending with る
  char32_t last_hira = codepoints[hiragana_end - 1];
  if (last_hira == U'る' && hiragana_len >= 2) {
    looks_like_aux = true;
  }

  // Patterns ending with て/で (verb te-form)
  // e.g., 基づいて, 考えて - these are verb conjugations, not compound nouns
  if ((last_hira == U'て' || last_hira == U'で') && hiragana_len >= 2) {
    looks_like_aux = true;
  }

  // Generate candidate with cost based on pattern
  std::string surface = extractSubstring(codepoints, start_pos, hiragana_end);
  if (!surface.empty()) {
    UnknownCandidate candidate;
    candidate.surface = surface;
    candidate.start = start_pos;
    candidate.end = hiragana_end;
    candidate.pos = core::PartOfSpeech::Noun;
    candidate.cost = looks_like_aux ? 3.5F : 1.0F;
    candidate.has_suffix = false;
#ifdef SUZUME_DEBUG_INFO
    candidate.origin = CandidateOrigin::KanjiHiraganaCompound;
    candidate.confidence = looks_like_aux ? 0.3F : 0.8F;
    candidate.pattern = looks_like_aux ? "aux_like" : "compound";
#endif
    candidates.push_back(candidate);
  }

  return candidates;
}

}  // namespace suzume::analysis
