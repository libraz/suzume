/**
 * @file suffix_candidates.cpp
 * @brief Suffix-based unknown word candidate generation
 */

#include "suffix_candidates.h"

#include <unordered_set>

#include "core/utf8_constants.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "unknown.h"

namespace suzume::analysis {

// =============================================================================
// Suffix Candidate Factory Helpers
// =============================================================================

/**
 * @brief Create a suffix pattern candidate with lemma
 */
inline UnknownCandidate makeSuffixCandidate(
    const std::string& surface, size_t start, size_t end,
    core::PartOfSpeech pos, float cost, const std::string& lemma,
    [[maybe_unused]] float confidence,
    [[maybe_unused]] const char* pattern,
    dictionary::ConjugationType conj_type = dictionary::ConjugationType::None) {
  auto cand = makeCandidate(surface, start, end, pos, cost, true, CandidateOrigin::SuffixPattern);
  cand.lemma = lemma;
  cand.conj_type = conj_type;
#ifdef SUZUME_DEBUG_INFO
  cand.confidence = confidence;
  cand.pattern = pattern;
#endif
  return cand;
}

/**
 * @brief Create a suffix pattern candidate without lemma
 */
inline UnknownCandidate makeSuffixCandidateNoLemma(
    const std::string& surface, size_t start, size_t end,
    core::PartOfSpeech pos, float cost,
    [[maybe_unused]] float confidence,
    [[maybe_unused]] const char* pattern) {
  auto cand = makeCandidate(surface, start, end, pos, cost, true, CandidateOrigin::SuffixPattern);
#ifdef SUZUME_DEBUG_INFO
  cand.confidence = confidence;
  cand.pattern = pattern;
#endif
  return cand;
}

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
      {"方", core::PartOfSpeech::Suffix},  // 歩き方, やり方 (V連用形+方)
      // Note: 中 removed - it's a bound noun (形式名詞), not a suffix
      // N中 compounds (今日中, 世界中, 一日中) are handled as compound nouns
      // Administrative suffixes (行政接尾辞)
      {"県", core::PartOfSpeech::Suffix},
      {"都", core::PartOfSpeech::Suffix},
      {"府", core::PartOfSpeech::Suffix},
      {"道", core::PartOfSpeech::Suffix},
      {"市", core::PartOfSpeech::Suffix},
      {"区", core::PartOfSpeech::Suffix},
      {"町", core::PartOfSpeech::Suffix},
      {"村", core::PartOfSpeech::Suffix},
      {"庁", core::PartOfSpeech::Suffix},
      {"署", core::PartOfSpeech::Suffix},
      {"局", core::PartOfSpeech::Suffix},
      {"省", core::PartOfSpeech::Suffix},
      {"院", core::PartOfSpeech::Suffix},
      {"所", core::PartOfSpeech::Suffix},
  };
  return kSuffixes;
}

const std::vector<std::string_view>& getNaAdjSuffixes() {
  static const std::vector<std::string_view> kNaAdjSuffixes = {
      "的",  // 理性的, 論理的, etc.
  };
  return kNaAdjSuffixes;
}

// =============================================================================
// Productive Hiragana Suffix Patterns (生産的接尾辞)
// =============================================================================

namespace {

// Check if the stem looks like a verb renyokei (連用形)
// This is a heuristic based on common patterns
bool looksLikeVerbRenyokei(std::string_view stem) {
  if (stem.empty()) {
    return false;
  }

  // Minimum 2 characters (1 Japanese char = 3 bytes)
  if (stem.size() < 3) {
    return false;
  }

  // Get the last character (Japanese = 3 bytes)
  std::string_view last_char = utf8::lastChar(stem);

  // Common verb renyokei endings (i-row for godan, e-row for ichidan)
  // Godan: し、み、き、ぎ、ち、り、い、び、に、ひ
  // Ichidan: べ、め、け、げ、せ、ぜ、て、で、ね、へ、ぺ、え、れ
  return utf8::equalsAny(last_char, {
      "し", "み", "き", "ぎ", "ち", "り", "い", "び", "に", "ひ",  // Godan i-row
      "べ", "め", "け", "げ", "せ", "ぜ", "て", "で", "ね", "へ", "え", "れ"  // Ichidan e-row
  });
}

}  // namespace

std::vector<UnknownCandidate> generateProductiveSuffixCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) {
  std::vector<UnknownCandidate> candidates;

  // Only for hiragana sequences
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  constexpr size_t kGachiLen = 6;  // "がち" = 2 chars * 3 bytes
  constexpr size_t kPpoiLen = 9;   // "っぽい" = 3 chars * 3 bytes

  // Try different lengths of hiragana (3 to 6 chars for stem + がち/っぽい)
  for (size_t hira_len = 3; hira_len <= 8; ++hira_len) {
    size_t candidate_end = start_pos + hira_len;
    if (candidate_end > char_types.size()) {
      break;
    }

    // Check all positions are hiragana
    bool all_hiragana = true;
    for (size_t i = start_pos; i < candidate_end; ++i) {
      if (char_types[i] != normalize::CharType::Hiragana) {
        all_hiragana = false;
        break;
      }
    }
    if (!all_hiragana) {
      break;  // No more hiragana
    }

    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    // Pattern 1: V連用形 + がち (tendency suffix)
    // Examples: ありがち、なりがち
    if (surface.size() >= kGachiLen + 3 && utf8::endsWith(surface, "がち")) {
      std::string_view stem = std::string_view(surface).substr(0, surface.size() - kGachiLen);
      if (looksLikeVerbRenyokei(stem)) {
        candidates.push_back(makeSuffixCandidate(
            surface, start_pos, candidate_end, core::PartOfSpeech::Noun, -0.5F,
            surface, 0.9F, "verb_renyokei_gachi"));
        return candidates;  // Found valid がち candidate
      }
    }

    // Pattern 2: V連用形 + っぽい (resemblance suffix)
    // Examples: 子供っぽい、安っぽい、忘れっぽい
    if (surface.size() >= kPpoiLen + 3 && utf8::endsWith(surface, "っぽい")) {
      std::string_view stem = std::string_view(surface).substr(0, surface.size() - kPpoiLen);
      // っぽい attaches to nouns and verb stems, less strict check
      if (stem.size() >= 3) {  // At least 1 character stem
        candidates.push_back(makeSuffixCandidate(
            surface, start_pos, candidate_end, core::PartOfSpeech::Adjective, 0.4F,
            surface, 0.85F, "stem_ppoi", dictionary::ConjugationType::IAdjective));
        return candidates;  // Found valid っぽい candidate
      }
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> generateGachiSuffixCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) {
  std::vector<UnknownCandidate> candidates;

  // For kanji-starting sequences ending with がち
  // Pattern: Kanji+ Hiragana(renyokei) + がち
  // Examples: 忘れがち (忘れ = 忘れる renyokei), 遅れがち (遅れ = 遅れる renyokei)
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (1-3 chars)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 4 &&
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  if (kanji_end == start_pos) {
    return candidates;
  }

  // Need hiragana after kanji
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Look for がち pattern within the hiragana portion
  // We need to find positions where がち appears
  constexpr size_t kGachiLen = 6;  // "がち" = 2 chars * 3 bytes

  // Try different lengths of hiragana (2 to 4 chars for renyokei + がち)
  for (size_t hira_len = 2; hira_len <= 4; ++hira_len) {
    size_t candidate_end = kanji_end + hira_len;
    if (candidate_end > char_types.size()) {
      break;
    }

    // Check all positions are hiragana
    bool all_hiragana = true;
    for (size_t i = kanji_end; i < candidate_end; ++i) {
      if (char_types[i] != normalize::CharType::Hiragana) {
        all_hiragana = false;
        break;
      }
    }
    if (!all_hiragana) {
      continue;
    }

    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    // Check if ends with がち
    if (surface.size() >= kGachiLen + 3 && utf8::endsWith(surface, "がち")) {

      // Check if hiragana before がち is a valid renyokei ending
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, candidate_end);
      std::string_view renyokei_ending;
      if (hiragana_part.size() > kGachiLen) {
        renyokei_ending = std::string_view(hiragana_part).substr(0, hiragana_part.size() - kGachiLen);
      }

      // For ichidan verbs, renyokei is just one hiragana (れ for 忘れる, れ for 遅れる)
      // Empty or valid renyokei ending is acceptable
      if (renyokei_ending.empty() || looksLikeVerbRenyokei(renyokei_ending)) {
        candidates.push_back(makeSuffixCandidate(
            surface, start_pos, candidate_end, core::PartOfSpeech::Noun, -0.5F,
            surface, 0.9F, "kanji_verb_renyokei_gachi"));
        break;  // Found one valid candidate, no need to check longer patterns
      }
    }
  }

  return candidates;
}

// Administrative suffix codepoints for intermediate boundary detection
const std::vector<char32_t>& getAdminSuffixCodepoints() {
  static const std::vector<char32_t> kAdminSuffixes = {
      U'県', U'都', U'府', U'道', U'市', U'区', U'町', U'村'};
  return kAdminSuffixes;
}

std::vector<UnknownCandidate> generateAdminBoundaryCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  const auto& admin_suffixes = getAdminSuffixCodepoints();

  // Scan through kanji sequence looking for administrative suffixes
  for (size_t pos = start_pos + 1; pos < char_types.size() && pos < start_pos + 6; ++pos) {
    if (char_types[pos] != normalize::CharType::Kanji) {
      break;
    }

    char32_t cp = codepoints[pos];
    bool is_admin_suffix = std::find(admin_suffixes.begin(), admin_suffixes.end(), cp) !=
                           admin_suffixes.end();

    if (is_admin_suffix) {
      // Found administrative suffix at position pos
      // Generate candidate from start_pos to pos+1 (including the suffix)
      size_t end_with_suffix = pos + 1;
      std::string surface = extractSubstring(codepoints, start_pos, end_with_suffix);
      candidates.push_back(makeSuffixCandidateNoLemma(
          surface, start_pos, end_with_suffix, core::PartOfSpeech::Noun, 0.3F,
          0.95F, "admin_boundary"));
    }
  }

  return candidates;
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

  // First, generate candidates for administrative boundaries
  auto admin_candidates = generateAdminBoundaryCandidates(codepoints, start_pos, char_types);
  candidates.insert(candidates.end(), admin_candidates.begin(), admin_candidates.end());

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

  // Skip potential suru-verb patterns: 漢字2字+し followed by suru-auxiliary
  // e.g., 勉強しちゃった → 勉強 + し + ちゃっ + た (not 勉強し + ちゃった)
  size_t kanji_count = kanji_end - start_pos;
  if (first_hiragana == U'し' && kanji_count >= 2) {
    // Check for suru-auxiliary patterns following し
    size_t next_pos = kanji_end + 1;
    if (next_pos < codepoints.size()) {
      char32_t next_char = codepoints[next_pos];
      // Common suru-auxiliary starting characters
      // ちゃ: contracted (しちゃう)
      // て/た: te/ta form (して, した)
      // な: negative (しない)
      // ま: polite (します)
      // よ: volitional (しよう)
      // ろ/れ: imperative (しろ/しれ)
      // ば: conditional (すれば - but follows せ not し)
      // そ: そう pattern (しそう)
      if (next_char == U'ち' || next_char == U'て' || next_char == U'た' ||
          next_char == U'な' || next_char == U'ま' || next_char == U'よ' ||
          next_char == U'ろ' || next_char == U'そ' || next_char == U'と' ||
          next_char == U'か' || next_char == U'つ') {
        // This looks like a suru-verb pattern - skip nominalization
        return candidates;
      }
    }
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
        auto cand = makeCandidate(surface, start_pos, hiragana_end + 1, core::PartOfSpeech::Noun, 0.8F, false, CandidateOrigin::NominalizedNoun);
#ifdef SUZUME_DEBUG_INFO
        cand.confidence = 0.8F;
        cand.pattern = "nominalized_2hira";
#endif
        candidates.push_back(cand);
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
      auto cand = makeCandidate(surface, start_pos, kanji_end + 1, core::PartOfSpeech::Noun, 1.2F, false, CandidateOrigin::NominalizedNoun);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 0.6F;
      cand.pattern = "nominalized_1hira";
#endif
      candidates.push_back(cand);
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
  char32_t first_hira = codepoints[kanji_end];

  // Handle sokuon (っ) pattern FIRST, before the hiragana_len check
  // Pattern: 漢字 + っ + (漢字 or 平仮名) - e.g., 横っ面, 取っ手, 引っ込む
  // These are valid compound words where hiragana portion may be just 1 char (っ)
  if (first_hira == U'っ') {
    // Need at least one more character after っ
    size_t sokuon_pos = kanji_end;  // Position of っ
    if (sokuon_pos + 1 < char_types.size()) {
      normalize::CharType next_type = char_types[sokuon_pos + 1];

      if (next_type == normalize::CharType::Kanji) {
        // Pattern: 漢字 + っ + 漢字 (e.g., 横っ面, 取っ手)
        size_t kanji2_end = sokuon_pos + 1;
        while (kanji2_end < char_types.size() &&
               kanji2_end - (sokuon_pos + 1) < 3 &&
               char_types[kanji2_end] == normalize::CharType::Kanji) {
          ++kanji2_end;
        }

        // Generate candidates for each length
        for (size_t end_pos = sokuon_pos + 2; end_pos <= kanji2_end; ++end_pos) {
          std::string surface = extractSubstring(codepoints, start_pos, end_pos);
          if (!surface.empty()) {
            auto cand = makeCandidate(surface, start_pos, end_pos, core::PartOfSpeech::Noun, 0.5F, false, CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
            cand.confidence = 0.9F;
            cand.pattern = "kanji_sokuon_kanji";
#endif
            candidates.push_back(cand);
          }
        }
      } else if (next_type == normalize::CharType::Hiragana) {
        // Pattern: 漢字 + っ + 平仮名 (e.g., 引っ込む, 突っ走る)
        // BUT skip if っ is followed by た/て (verb conjugation endings)
        // e.g., 減った, 勝って are verb forms, not compound nouns
        char32_t next_hira = codepoints[sokuon_pos + 1];
        if (next_hira == U'た' || next_hira == U'て') {
          return candidates;  // Skip - this is a verb conjugation, not a compound noun
        }
        size_t hira2_end = sokuon_pos + 1;
        while (hira2_end < char_types.size() &&
               hira2_end - (sokuon_pos + 1) < 4 &&
               char_types[hira2_end] == normalize::CharType::Hiragana) {
          char32_t ch = codepoints[hira2_end];
          if (normalize::isParticleCodepoint(ch)) {
            break;
          }
          ++hira2_end;
        }

        if (hira2_end > sokuon_pos + 1) {
          std::string surface = extractSubstring(codepoints, start_pos, hira2_end);
          if (!surface.empty()) {
            auto cand = makeCandidate(surface, start_pos, hira2_end, core::PartOfSpeech::Noun, 1.0F, false, CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
            cand.confidence = 0.7F;
            cand.pattern = "kanji_sokuon_hira";
#endif
            candidates.push_back(cand);
          }
        }
      }
    }
    // Return after handling sokuon - don't continue to normal hiragana logic
    return candidates;
  }

  if (hiragana_len < 2) {
    return candidates;
  }
  char32_t second_hira = (hiragana_len >= 2) ? codepoints[kanji_end + 1] : 0;

  // Skip small kana at start - morphologically invalid
  // EXCEPTION: っ (sokuon) can appear in compound patterns like 横っ面, 取っ手, 引っ込む
  // These are valid words where kanji + っ + (kanji or hiragana) forms a compound
  if (first_hira == U'ゃ' || first_hira == U'ゅ' || first_hira == U'ょ' ||
      first_hira == U'ぁ' || first_hira == U'ぃ' || first_hira == U'ぅ' ||
      first_hira == U'ぇ' || first_hira == U'ぉ') {
    return candidates;
  }

  // Handle sokuon (っ) pattern: 漢字 + っ + (漢字 or 平仮名)
  // Examples: 横っ面, 取っ手, 引っ込む
  if (first_hira == U'っ') {
    // Need at least one more character after っ
    size_t sokuon_pos = kanji_end;  // Position of っ
    if (sokuon_pos + 1 >= char_types.size()) {
      return candidates;  // っ at end - cannot form valid compound
    }

    // Check what follows っ
    normalize::CharType next_type = char_types[sokuon_pos + 1];
    if (next_type == normalize::CharType::Kanji) {
      // Pattern: 漢字 + っ + 漢字 (e.g., 横っ面, 取っ手)
      // Find end of kanji sequence after っ
      size_t kanji2_end = sokuon_pos + 1;
      while (kanji2_end < char_types.size() &&
             kanji2_end - (sokuon_pos + 1) < 3 &&  // Max 3 kanji after っ
             char_types[kanji2_end] == normalize::CharType::Kanji) {
        ++kanji2_end;
      }

      // Generate candidate(s) for kanji + っ + kanji pattern
      for (size_t end_pos = sokuon_pos + 2; end_pos <= kanji2_end; ++end_pos) {
        std::string surface = extractSubstring(codepoints, start_pos, end_pos);
        if (!surface.empty()) {
          auto cand = makeCandidate(surface, start_pos, end_pos, core::PartOfSpeech::Noun, 0.5F, false, CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
          cand.confidence = 0.9F;
          cand.pattern = "kanji_sokuon_kanji";
#endif
          candidates.push_back(cand);
        }
      }

      // Also check if there's hiragana after the second kanji (e.g., patterns like 取っ手さばき)
      if (kanji2_end < char_types.size() &&
          char_types[kanji2_end] == normalize::CharType::Hiragana) {
        // Find hiragana portion after second kanji
        size_t hira_end = kanji2_end;
        while (hira_end < char_types.size() &&
               hira_end - kanji2_end < 4 &&
               char_types[hira_end] == normalize::CharType::Hiragana) {
          char32_t ch = codepoints[hira_end];
          if (normalize::isParticleCodepoint(ch)) {
            break;
          }
          ++hira_end;
        }

        if (hira_end > kanji2_end) {
          std::string surface = extractSubstring(codepoints, start_pos, hira_end);
          if (!surface.empty()) {
            auto cand = makeCandidate(surface, start_pos, hira_end, core::PartOfSpeech::Noun, 0.8F, false, CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
            cand.confidence = 0.8F;
            cand.pattern = "kanji_sokuon_kanji_hira";
#endif
            candidates.push_back(cand);
          }
        }
      }
    } else if (next_type == normalize::CharType::Hiragana) {
      // Pattern: 漢字 + っ + 平仮名 (e.g., 引っ込む, 突っ走る)
      // Find end of hiragana after っ
      size_t hira2_end = sokuon_pos + 1;
      while (hira2_end < char_types.size() &&
             hira2_end - (sokuon_pos + 1) < 4 &&
             char_types[hira2_end] == normalize::CharType::Hiragana) {
        char32_t ch = codepoints[hira2_end];
        if (normalize::isParticleCodepoint(ch)) {
          break;
        }
        ++hira2_end;
      }

      // Need at least one hiragana after っ (not counting っ itself)
      if (hira2_end > sokuon_pos + 1) {
        std::string surface = extractSubstring(codepoints, start_pos, hira2_end);
        if (!surface.empty()) {
          auto cand = makeCandidate(surface, start_pos, hira2_end, core::PartOfSpeech::Noun, 1.0F, false, CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
          cand.confidence = 0.7F;
          cand.pattern = "kanji_sokuon_hira";
#endif
          candidates.push_back(cand);
        }
      }
    }

    return candidates;  // Sokuon pattern handled, don't fall through to normal logic
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
    // Godan verb shuushikei (終止形) pattern
    // e.g., 休む, 行く, 泳ぐ, 話す, 立つ, 死ぬ, 飛ぶ, 取る
    // If first hiragana is a godan verb ending, kanji+first hiragana likely forms
    // a complete verb, and the rest starts a new word
    // 休むこと → 休む(VERB) + こと(NOUN), not 休むこ(NOUN) + と(PARTICLE)
    bool is_godan_shuushikei = (first_hira == U'む' || first_hira == U'う' ||
                                first_hira == U'く' || first_hira == U'ぐ' ||
                                first_hira == U'す' || first_hira == U'つ' ||
                                first_hira == U'ぬ' || first_hira == U'ぶ' ||
                                first_hira == U'る');
    if (is_godan_shuushikei) {
      looks_like_aux = true;
    }
    // Renyokei + そう/たい/ます
    // For godan verbs: し,み,き,ぎ,ち,り,い,び (i-row)
    // For ichidan verbs: べ,め,け,せ,て,ね,れ,え (e-row) - these are verb stems
    bool is_renyokei = (first_hira == U'し' || first_hira == U'み' ||
                        first_hira == U'き' || first_hira == U'ぎ' ||
                        first_hira == U'ち' || first_hira == U'り' ||
                        first_hira == U'い' || first_hira == U'び');
    bool is_ichidan_stem = (first_hira == U'べ' || first_hira == U'め' ||
                            first_hira == U'け' || first_hira == U'せ' ||
                            first_hira == U'て' || first_hira == U'ね' ||
                            first_hira == U'れ' || first_hira == U'え' ||
                            first_hira == U'げ' || first_hira == U'ぜ' ||
                            first_hira == U'で' || first_hira == U'へ' ||
                            first_hira == U'ぺ');
    if ((is_renyokei || is_ichidan_stem) && (second_hira == U'そ' || second_hira == U'た' ||
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

  // Patterns ending with お (prefix marker)
  // e.g., 一つお should be 一つ + お(PREFIX), not 一つお(NOUN)
  // お is very commonly used as honorific prefix, so it should not be absorbed
  // into compound nouns
  if (last_hira == U'お') {
    looks_like_aux = true;
  }

  // Skip NOUN generation for pure auxiliary patterns
  // These should always be verb stem + auxiliary, never a compound noun
  // e.g., 寝ます should be 寝(VERB) + ます(AUX), not 寝ます(NOUN)
  if (hiragana_len == 2) {
    using namespace suzume::core::hiragana;
    char32_t h1 = codepoints[kanji_end];
    char32_t h2 = codepoints[kanji_end + 1];
    // ます, ない - pure polite/negative auxiliaries
    if ((h1 == kMa && h2 == kSu) || (h1 == kNa && h2 == kI)) {
      return candidates;  // Skip NOUN generation entirely
    }
  }

  // Generate candidate with cost based on pattern
  std::string surface = extractSubstring(codepoints, start_pos, hiragana_end);
  if (!surface.empty()) {
    float cost = looks_like_aux ? 3.5F : 1.0F;
    auto cand = makeCandidate(surface, start_pos, hiragana_end, core::PartOfSpeech::Noun, cost, false, CandidateOrigin::KanjiHiraganaCompound);
#ifdef SUZUME_DEBUG_INFO
    cand.confidence = looks_like_aux ? 0.3F : 0.8F;
    cand.pattern = looks_like_aux ? "aux_like" : "compound";
#endif
    candidates.push_back(cand);
  }

  return candidates;
}

namespace {
// Check if a character is a numeral (Arabic or kanji)
bool isNumeralChar(char32_t c) {
  // Arabic numerals (half-width and full-width)
  if ((c >= U'0' && c <= U'9') || (c >= U'０' && c <= U'９')) {
    return true;
  }
  // Kanji numerals
  if (c == U'一' || c == U'二' || c == U'三' || c == U'四' || c == U'五' ||
      c == U'六' || c == U'七' || c == U'八' || c == U'九' || c == U'十' ||
      c == U'百' || c == U'千' || c == U'万') {
    return true;
  }
  return false;
}
}  // namespace

std::vector<UnknownCandidate> generateCounterCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) {
  std::vector<UnknownCandidate> candidates;

  // Need at least 2 characters (numeral + counter suffix)
  if (start_pos + 1 >= codepoints.size()) {
    return candidates;
  }

  // First character(s) must be numeral(s)
  if (!isNumeralChar(codepoints[start_pos])) {
    return candidates;
  }

  // Find the end of the numeral sequence
  size_t numeral_end = start_pos;
  while (numeral_end < codepoints.size() &&
         isNumeralChar(codepoints[numeral_end])) {
    ++numeral_end;
  }

  // Must have at least one character after numerals
  if (numeral_end >= codepoints.size()) {
    return candidates;
  }

  // Check for counter suffix (つ for native counters)
  char32_t next = codepoints[numeral_end];
  if (next == U'つ') {
    // Generate counter candidate: Nつ
    std::string surface =
        extractSubstring(codepoints, start_pos, numeral_end + 1);
    if (!surface.empty()) {
      auto cand = makeCandidate(surface, start_pos, numeral_end + 1, core::PartOfSpeech::Noun, -0.5F, false, CandidateOrigin::Counter);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 0.95F;
      cand.pattern = "counter_tsu";
#endif
      candidates.push_back(cand);
    }
  }

  // Check for katakana unit suffix (e.g., キロ, ドル, メートル, パーセント)
  // Generate digit + katakana unit candidates like 3キロ, 100ドル, 80パーセント
  if (numeral_end < char_types.size() &&
      char_types[numeral_end] == normalize::CharType::Katakana) {
    // Find end of katakana sequence
    size_t unit_end = numeral_end;
    while (unit_end < codepoints.size() &&
           unit_end < char_types.size() &&
           char_types[unit_end] == normalize::CharType::Katakana) {
      ++unit_end;
    }

    // Generate candidate for digit + katakana unit
    size_t unit_len = unit_end - numeral_end;
    if (unit_len >= 1 && unit_len <= 8) {  // Reasonable unit length 1-8 chars
      std::string surface = extractSubstring(codepoints, start_pos, unit_end);
      if (!surface.empty()) {
        // Penalize numbers starting with 0 (e.g., "00ポイント" is unnatural)
        // "0ドル" is fine, but "00ドル", "000キロ" are not typical Japanese patterns
        bool starts_with_zero_prefix = false;
        if (numeral_end - start_pos >= 2 && codepoints[start_pos] == U'0') {
          starts_with_zero_prefix = true;
        }
        // Give bonus to prefer combined token over split
        // Longer units get slightly more bonus (キロ, ドル vs キログラム, パーセント)
        // Strong bonus (-0.5) to beat optimal_length bonuses on split candidates
        float cost = starts_with_zero_prefix
                         ? 2.0F  // Penalize unnatural zero-prefix numbers
                         : -0.5F - (static_cast<float>(unit_len) * 0.05F);
        auto cand = makeCandidate(surface, start_pos, unit_end, core::PartOfSpeech::Noun, cost, false, CandidateOrigin::Counter);
#ifdef SUZUME_DEBUG_INFO
        cand.confidence = starts_with_zero_prefix ? 0.3F : 0.9F;
        cand.pattern = "numeric_unit_katakana";
#endif
        candidates.push_back(cand);
      }
    }
  }

  return candidates;
}

// =============================================================================
// Prefix + Single Kanji Compound Candidates (接頭的複合語)
// =============================================================================

namespace {

// Prefix-like kanji that can form compounds with single kanji
// These are kanji that commonly appear at the start of temporal compounds
// Note: 本 excluded - too many non-prefix uses (本当, 本人, 本社, etc.)
// Note: 全/各/両/諸 excluded - require more context to determine compound boundary
const std::unordered_set<char32_t>& getPrefixLikeKanji() {
  static const std::unordered_set<char32_t> kPrefixKanji = {
      U'今',  // 今日, 今週, 今月, 今年, 今朝, 今晩, 今夜
      U'来',  // 来日, 来週, 来月, 来年
      U'先',  // 先日, 先週, 先月, 先年
      U'昨',  // 昨日, 昨年
      U'翌',  // 翌日, 翌週, 翌月, 翌年
      U'毎',  // 毎日, 毎週, 毎月, 毎年
  };
  return kPrefixKanji;
}

// Interrogative kanji that should NOT form compounds
// These act as strong anchors in the dictionary
const std::unordered_set<char32_t>& getInterrogativeKanji() {
  static const std::unordered_set<char32_t> kInterrogatives = {
      U'何',  // 何 (なに/なん) - what
      U'誰',  // 誰 (だれ) - who
      U'幾',  // 幾 (いく) - how many (幾つ, 幾日)
  };
  return kInterrogatives;
}

}  // namespace

bool isPrefixLikeKanji(char32_t cp) {
  const auto& prefix_kanji = getPrefixLikeKanji();
  return prefix_kanji.find(cp) != prefix_kanji.end();
}

std::vector<UnknownCandidate> generatePrefixCompoundCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) {
  std::vector<UnknownCandidate> candidates;

  // Need at least 2 kanji characters
  if (start_pos + 1 >= codepoints.size()) {
    return candidates;
  }

  // First character must be kanji
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Check if first character is a prefix-like kanji
  char32_t first_char = codepoints[start_pos];
  const auto& prefix_kanji = getPrefixLikeKanji();
  if (prefix_kanji.find(first_char) == prefix_kanji.end()) {
    return candidates;
  }

  // Second character must also be kanji
  if (start_pos + 1 >= char_types.size() ||
      char_types[start_pos + 1] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Skip if second character is an interrogative (何, 誰, etc.)
  // These act as anchors and should not form compounds with prefix
  char32_t second_char = codepoints[start_pos + 1];
  const auto& interrogatives = getInterrogativeKanji();
  if (interrogatives.find(second_char) != interrogatives.end()) {
    return candidates;  // Don't generate compound, let dictionary anchor win
  }

  // Generate 2-character compound (prefix + single kanji) ONLY when:
  // - Not followed by more kanji, OR
  // - Followed by 中 (which becomes 3-char compound below)
  // This prevents invalid splits like 翌営|業日 (should be 翌営業日)
  bool followed_by_kanji = (start_pos + 2 < char_types.size() &&
                            char_types[start_pos + 2] == normalize::CharType::Kanji);
  bool followed_by_chuu = (followed_by_kanji &&
                           start_pos + 2 < codepoints.size() &&
                           codepoints[start_pos + 2] == U'中');

  if (!followed_by_kanji || followed_by_chuu) {
    std::string surface = extractSubstring(codepoints, start_pos, start_pos + 2);
    if (!surface.empty()) {
      // Strong bonus to prefer compound over split
      // Must beat: single_kanji(1.4+2) + single_kanji(1.4+2) = 6.8
      // And compete with dictionary entries
      auto cand = makeCandidate(surface, start_pos, start_pos + 2, core::PartOfSpeech::Noun, -1.0F, false, CandidateOrigin::PrefixCompound);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 0.9F;
      cand.pattern = "prefix_single_kanji";
#endif
      candidates.push_back(cand);
    }
  }

  // Also generate 3-character compound if followed by 中 (bound noun)
  // e.g., 今日中, 一日中, 世界中
  if (start_pos + 2 < codepoints.size() &&
      start_pos + 2 < char_types.size() &&
      char_types[start_pos + 2] == normalize::CharType::Kanji &&
      codepoints[start_pos + 2] == U'中') {
    std::string surface3 = extractSubstring(codepoints, start_pos, start_pos + 3);
    if (!surface3.empty()) {
      // Even stronger bonus for N中 compounds
      auto cand = makeCandidate(surface3, start_pos, start_pos + 3, core::PartOfSpeech::Noun, -1.5F, false, CandidateOrigin::PrefixCompound);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 0.95F;
      cand.pattern = "prefix_kanji_chuu";
#endif
      candidates.push_back(cand);
    }
  }

  return candidates;
}

}  // namespace suzume::analysis
