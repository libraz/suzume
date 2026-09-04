/**
 * @file verb_candidates_helpers_emphatic.cpp
 * @brief Emphatic suffix helpers for candidate generation
 */

#include "analysis/candidate_constants.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis::verb_helpers {

// =============================================================================
// Emphatic Pattern Helpers
// =============================================================================

bool isEmphaticChar(char32_t c) {
  return c == core::hiragana::kSmallTsu ||  // っ
         c == U'ッ' ||                      // katakana sokuon
         c == U'ー' ||                      // chouon
         // Small hiragana vowels
         c == U'ぁ' || c == U'ぃ' || c == U'ぅ' || c == U'ぇ' || c == U'ぉ' ||
         // Small katakana vowels
         c == U'ァ' || c == U'ィ' || c == U'ゥ' || c == U'ェ' || c == U'ォ';
}

char32_t getHiraganaVowel(char32_t c) {
  // Hiragana range: U+3041 (ぁ) to U+3096 (ゖ)
  constexpr char32_t kHiraganaStart = 0x3041;
  constexpr char32_t kHiraganaEnd = 0x3096;

  if (c < kHiraganaStart || c > kHiraganaEnd) {
    return 0;  // Not hiragana
  }

  // Vowel table: 'a'/'i'/'u'/'e'/'o' or 0 for no-vowel (ん, っ)
  // Index = codepoint - 0x3041, covers ぁ through ゖ (86 chars)
  static constexpr char kVowelTable[86] = {
      'a', 'a', 'i', 'i', 'u', 'u', 'e', 'e', 'o', 'o', 'a', 'a', 'i', 'i', 'u',       // ぁ-く
      'u', 'e', 'e', 'o', 'o', 'a', 'a', 'i', 'i', 'u', 'u', 'e', 'e', 'o', 'o',       // ぐ-ぞ
      'a', 'a', 'i', 'i', 0,   'u', 'u', 'e', 'e', 'o', 'o', 'a', 'i', 'u', 'e',       // た-ね
      'o', 'a', 'a', 'a', 'i', 'i', 'i', 'u', 'u', 'u', 'e', 'e', 'e', 'o', 'o',       // の-ぼ
      'o', 'a', 'i', 'u', 'e', 'o', 'a', 'a', 'u', 'u', 'o', 'o', 'a', 'i', 'u', 'e',  // ぽ-れ
      'o', 'a', 'a', 'i', 'e', 'o', 0,   'u', 'a', 'e',                                // ろ-ゖ
  };

  char vowel = kVowelTable[c - kHiraganaStart];
  switch (vowel) {
    case 'a':
      return U'あ';
    case 'i':
      return U'い';
    case 'u':
      return U'う';
    case 'e':
      return U'え';
    case 'o':
      return U'お';
    default:
      return 0;
  }
}

namespace {

bool isSuppressedSokuonOnset(const std::vector<char32_t>& codepoints, size_t sokuon_pos, core::PartOfSpeech base_pos,
                             char32_t base_final, SokuonOnsetPolicy policy) {
  if (sokuon_pos + 1 >= codepoints.size()) {
    return false;
  }
  const char32_t next = codepoints[sokuon_pos + 1];
  if (next == U'す' || next == U'さ' || next == U'せ') {
    return true;
  }
  const bool u_row_verb = base_pos == core::PartOfSpeech::Verb && normalize::isURowHiragana(base_final);
  if (next == U'と') {
    return u_row_verb;
  }
  if (policy == SokuonOnsetPolicy::DictionaryEntry) {
    return next == core::hiragana::kTe && u_row_verb;
  }
  return next == core::hiragana::kTe || next == core::hiragana::kTa;
}

}  // namespace

EmphaticSuffixMatch matchEmphaticSuffix(const std::vector<char32_t>& codepoints, size_t base_end,
                                        core::PartOfSpeech base_pos, SokuonOnsetPolicy policy) {
  EmphaticSuffixMatch match;
  match.end = base_end;
  if (base_end == 0 || base_end > codepoints.size()) {
    return match;
  }

  while (match.end < codepoints.size() && isEmphaticChar(codepoints[match.end])) {
    const char32_t codepoint = codepoints[match.end];
    if (policy == SokuonOnsetPolicy::Candidate && (codepoint == core::hiragana::kSmallTsu || codepoint == U'ッ') &&
        isSuppressedSokuonOnset(codepoints, match.end, base_pos, codepoints[base_end - 1], policy)) {
      break;
    }
    match.suffix += normalize::encodeUtf8(codepoint);
    ++match.standard_char_count;
    ++match.end;
  }

  if (policy == SokuonOnsetPolicy::DictionaryEntry && match.suffix == "っ" && match.end < codepoints.size() &&
      isSuppressedSokuonOnset(codepoints, base_end, base_pos, codepoints[base_end - 1], policy)) {
    match.suffix.clear();
    match.standard_char_count = 0;
    return match;
  }

  const char32_t repeated_vowel = getHiraganaVowel(codepoints[base_end - 1]);
  if (repeated_vowel == 0 || match.end >= codepoints.size()) {
    return match;
  }

  const size_t vowel_start = match.end;
  while (match.end < codepoints.size() && codepoints[match.end] == repeated_vowel) {
    ++match.repeated_vowel_count;
    ++match.end;
  }
  if (match.repeated_vowel_count < candidate::kEmphaticMinRepeatedVowels) {
    match.repeated_vowel_count = 0;
    match.end = vowel_start;
    return match;
  }

  for (size_t idx = 0; idx < match.repeated_vowel_count; ++idx) {
    match.suffix += normalize::encodeUtf8(repeated_vowel);
  }

  return match;
}

float emphaticCostAdjustment(const EmphaticSuffixMatch& match) {
  if (match.repeated_vowel_count >= candidate::kEmphaticMinRepeatedVowels) {
    const auto char_count = static_cast<float>(match.standard_char_count + match.repeated_vowel_count);
    return candidate::kEmphaticRepeatedVowelBonus + candidate::kEmphaticRepeatedVowelLengthPenalty * char_count;
  }
  return candidate::kEmphaticCharacterPenalty * static_cast<float>(match.standard_char_count);
}

void addEmphaticVariants(std::vector<UnknownCandidate>& candidates, const std::vector<char32_t>& codepoints,
                         size_t first_index) {
  std::vector<UnknownCandidate> emphatic_variants;

  // Only the sub-range appended by the current generator is inspected; earlier
  // generators may have shared this buffer with unrelated verb/adjective
  // candidates that must not spawn emphatic variants here.
  for (size_t idx = first_index; idx < candidates.size(); ++idx) {
    const UnknownCandidate& cand = candidates[idx];
    // Only extend verb and adjective candidates
    if (cand.pos != core::PartOfSpeech::Verb && cand.pos != core::PartOfSpeech::Adjective) {
      continue;
    }

    const auto emphatic = matchEmphaticSuffix(codepoints, cand.end, cand.pos);

    // Add emphatic variant if we found any emphatic characters
    if (!emphatic.empty()) {
      UnknownCandidate emphatic_cand = cand;
      emphatic_cand.surface += emphatic.suffix;
      emphatic_cand.end = emphatic.end;
      emphatic_cand.cost += emphaticCostAdjustment(emphatic);
      // A candidate carrying no lemma of its own is lemmatized to its surface,
      // and the emphasis would go into it (行くっ as its own base form). Only a
      // terminal form is its own dictionary form, so only there is the surface
      // the emphasis was added to the right lemma; a bound cell keeps its empty
      // lemma for the paradigm to resolve.
      if (emphatic_cand.lemma.empty() && cand.extended_pos == core::ExtendedPOS::VerbShuushikei) {
        emphatic_cand.lemma = cand.surface;
      }
      // A run of emphasis marks is itself a searchable colloquial form.
      // Keep modest elongation normalized (すごーい, やばいいい), but preserve
      // longer runs such as すごーーい and すごいいいい as their own lemma.
      if (cand.pos == core::PartOfSpeech::Adjective &&
          (emphatic.standard_char_count >= 2 || emphatic.repeated_vowel_count >= 3)) {
        emphatic_cand.lemma = emphatic_cand.surface;
      }
#ifdef SUZUME_DEBUG_INFO
      emphatic_cand.pattern += "_emphatic";
#endif
      emphatic_variants.push_back(std::move(emphatic_cand));
    }
  }

  // Add all emphatic variants
  for (auto& var : emphatic_variants) {
    candidates.push_back(std::move(var));
  }
}

}  // namespace suzume::analysis::verb_helpers
