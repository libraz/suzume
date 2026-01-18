/**
 * @file verb_candidates_helpers.cpp
 * @brief Implementation of internal helpers for verb candidate generation
 */

#include "verb_candidates_helpers.h"

#include <algorithm>

#include "analysis/scorer_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/utf8.h"

namespace suzume::analysis::verb_helpers {

// =============================================================================
// Single-kanji Ichidan verbs
// =============================================================================

namespace {
constexpr char32_t kSingleKanjiIchidanList[] = {
    U'見', U'居', U'着', U'寝', U'煮', U'似',
    U'経', U'干', U'射', U'得', U'出', U'鋳'};
}  // namespace

bool isSingleKanjiIchidan(char32_t c) {
  for (char32_t k : kSingleKanjiIchidanList) {
    if (c == k) return true;
  }
  return false;
}

// =============================================================================
// Dictionary Lookup Helpers
// =============================================================================

bool hasDictionaryEntry(const dictionary::DictionaryManager* dict_manager,
                        std::string_view surface,
                        core::PartOfSpeech pos) {
  if (dict_manager == nullptr || surface.empty()) {
    return false;
  }
  auto results = dict_manager->lookup(surface, 0);
  for (const auto& result : results) {
    if (result.entry != nullptr && result.entry->surface == surface &&
        result.entry->pos == pos) {
      return true;
    }
  }
  return false;
}

bool isVerbInDictionaryWithType(const dictionary::DictionaryManager* dict_manager,
                                std::string_view base_form,
                                grammar::VerbType verb_type) {
  if (dict_manager == nullptr || base_form.empty()) {
    return false;
  }
  auto expected_conj = grammar::verbTypeToConjType(verb_type);
  auto results = dict_manager->lookup(base_form, 0);
  for (const auto& result : results) {
    if (result.entry != nullptr && result.entry->surface == base_form &&
        result.entry->pos == core::PartOfSpeech::Verb &&
        result.entry->conj_type == expected_conj) {
      return true;
    }
  }
  return false;
}

bool hasNonVerbDictionaryEntry(const dictionary::DictionaryManager* dict_manager,
                               std::string_view surface) {
  if (dict_manager == nullptr) {
    return false;
  }
  auto results = dict_manager->lookup(surface, 0);
  for (const auto& result : results) {
    if (result.entry != nullptr && result.entry->surface == surface &&
        result.entry->pos != core::PartOfSpeech::Verb) {
      return true;
    }
  }
  return false;
}

// =============================================================================
// Candidate Sorting
// =============================================================================

void sortCandidatesByCost(std::vector<UnknownCandidate>& candidates) {
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });
}

// =============================================================================
// Emphatic Pattern Helpers
// =============================================================================

bool isEmphaticChar(char32_t c) {
  return c == core::hiragana::kSmallTsu ||  // っ
         c == U'ッ' ||                       // katakana sokuon
         c == U'ー' ||                       // chouon
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
      'a','a','i','i','u','u','e','e','o','o','a','a','i','i','u',  // ぁ-く
      'u','e','e','o','o','a','a','i','i','u','u','e','e','o','o',  // ぐ-ぞ
      'a','a','i','i', 0 ,'u','u','e','e','o','o','a','i','u','e',  // た-ね
      'o','a','a','a','i','i','i','u','u','u','e','e','e','o','o',  // の-ぼ
      'o','a','i','u','e','o','a','a','u','u','o','o','a','i','u','e',  // ぽ-れ
      'o','a','a','i','e','o', 0 ,'u','a','e',  // ろ-ゖ
  };

  char vowel = kVowelTable[c - kHiraganaStart];
  switch (vowel) {
    case 'a': return U'あ';
    case 'i': return U'い';
    case 'u': return U'う';
    case 'e': return U'え';
    case 'o': return U'お';
    default:  return 0;
  }
}

bool isTeTaFormSokuon(const std::vector<char32_t>& codepoints, size_t sokuon_pos) {
  if (sokuon_pos + 1 >= codepoints.size()) {
    return false;  // Sokuon at end - could be emphatic
  }
  char32_t next = codepoints[sokuon_pos + 1];
  // っ+て, っ+た patterns are te/ta forms, not emphatic
  return next == core::hiragana::kTe || next == core::hiragana::kTa;
}

void addEmphaticVariants(std::vector<UnknownCandidate>& candidates,
                         const std::vector<char32_t>& codepoints) {
  std::vector<UnknownCandidate> emphatic_variants;

  for (const auto& cand : candidates) {
    // Only extend verb and adjective candidates
    if (cand.pos != core::PartOfSpeech::Verb &&
        cand.pos != core::PartOfSpeech::Adjective) {
      continue;
    }

    // Check if there are emphatic characters after the candidate
    size_t emphatic_end = cand.end;
    std::string emphatic_suffix;

    while (emphatic_end < codepoints.size()) {
      char32_t c = codepoints[emphatic_end];
      if (isEmphaticChar(c)) {
        // Special case: っ followed by て/た is verb te-form, not emphatic
        if ((c == core::hiragana::kSmallTsu || c == U'ッ') &&
            isTeTaFormSokuon(codepoints, emphatic_end)) {
          break;  // Stop - this is part of a verb, not emphatic
        }
        emphatic_suffix += normalize::encodeUtf8(c);
        ++emphatic_end;
      } else {
        break;
      }
    }

    // Track standard emphatic chars separately for cost calculation
    size_t standard_emphatic_chars = emphatic_suffix.size() / core::kJapaneseCharBytes;

    // Also check for repeated vowels matching the final character's vowel
    size_t vowel_repeat_count = 0;
    if (cand.end > 0 && emphatic_end < codepoints.size()) {
      char32_t final_char = codepoints[cand.end - 1];
      char32_t expected_vowel = getHiraganaVowel(final_char);

      if (expected_vowel != 0) {
        size_t vowel_start = emphatic_end;

        // Count consecutive occurrences of the expected vowel
        while (emphatic_end < codepoints.size() &&
               codepoints[emphatic_end] == expected_vowel) {
          ++vowel_repeat_count;
          ++emphatic_end;
        }

        // Require at least 2 repeated vowels for emphatic pattern
        if (vowel_repeat_count >= 2) {
          for (size_t i = 0; i < vowel_repeat_count; ++i) {
            emphatic_suffix += normalize::encodeUtf8(expected_vowel);
          }
        } else {
          // Not enough repetition, reset position
          emphatic_end = vowel_start;
          vowel_repeat_count = 0;
        }
      }
    }

    // Add emphatic variant if we found any emphatic characters
    if (!emphatic_suffix.empty()) {
      UnknownCandidate emphatic_cand = cand;
      emphatic_cand.surface += emphatic_suffix;
      emphatic_cand.end = emphatic_end;
      float cost_adjustment;

      if (vowel_repeat_count >= 2) {
        // Give a BONUS for vowel repetition to compete with split alternatives
        float char_count =
            static_cast<float>(emphatic_suffix.size() / core::kJapaneseCharBytes);
        cost_adjustment = -0.5F + 0.05F * char_count;
      } else {
        // Standard emphatic chars (sokuon/chouon/small vowels) use penalty
        cost_adjustment = 0.3F * static_cast<float>(standard_emphatic_chars);
      }
      emphatic_cand.cost += cost_adjustment;
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

// =============================================================================
// Pattern Skip Helpers
// =============================================================================

bool shouldSkipMasuAuxPattern(std::string_view surface, grammar::VerbType verb_type) {
  // Check if surface ends with ます/ました/ましょう/ません
  bool has_masu_aux = utf8::endsWith(surface, "ましょう") ||
                      utf8::endsWith(surface, "ました") ||
                      utf8::endsWith(surface, "ません") ||
                      utf8::endsWith(surface, "ます");

  if (!has_masu_aux) {
    return false;
  }

  // Don't skip suru-verb passive/causative patterns (され, させ)
  bool is_suru_passive_causative =
      (verb_type == grammar::VerbType::Suru &&
       utf8::containsAny(surface, {"され", "させ"}));

  return !is_suru_passive_causative;
}

bool shouldSkipSouPattern(std::string_view surface, grammar::VerbType verb_type) {
  // Check for そう/そうです/そうだ at end
  bool has_sou_pattern = utf8::endsWith(surface, "そうです") ||
                         utf8::endsWith(surface, "そうだ") ||
                         utf8::endsWith(surface, scorer::kSuffixSou);

  // Don't skip i-adjective patterns
  return has_sou_pattern && verb_type != grammar::VerbType::IAdjective;
}

bool isCompoundAdjectivePattern(std::string_view surface) {
  if (surface.size() < core::kFourJapaneseCharBytes) {
    return false;
  }
  return utf8::containsAny(surface, {
      "にくい", "にくく", "にくか",  // difficult to do
      "やすい", "やすく", "やすか",  // easy to do
      "がたい", "がたく"             // hard to do
  });
}

bool isGodanVerbType(grammar::VerbType verb_type) {
  return verb_type == grammar::VerbType::GodanKa ||
         verb_type == grammar::VerbType::GodanGa ||
         verb_type == grammar::VerbType::GodanSa ||
         verb_type == grammar::VerbType::GodanTa ||
         verb_type == grammar::VerbType::GodanNa ||
         verb_type == grammar::VerbType::GodanMa ||
         verb_type == grammar::VerbType::GodanBa ||
         verb_type == grammar::VerbType::GodanRa ||
         verb_type == grammar::VerbType::GodanWa;
}

bool shouldSkipPassiveAuxPattern(std::string_view surface, grammar::VerbType verb_type) {
  // Skip patterns containing classical passive + べき
  if (utf8::endsWith(surface, "れべき")) {
    return true;
  }

  // Only apply remaining checks to Godan verbs
  if (!isGodanVerbType(verb_type)) {
    return false;
  }

  // Passive patterns: れる, れた, れて, れない, れます, れたい, れたく
  return utf8::endsWith(surface, "れる") ||
         utf8::endsWith(surface, "れた") ||
         utf8::endsWith(surface, "れて") ||
         utf8::endsWith(surface, "れない") ||
         utf8::endsWith(surface, "れます") ||
         utf8::endsWith(surface, "れたい") ||
         utf8::endsWith(surface, "れたく");
}

bool shouldSkipCausativeAuxPattern(std::string_view surface, grammar::VerbType verb_type) {
  // Suru verb causative/passive: stay as single tokens
  if (verb_type == grammar::VerbType::Suru) {
    return false;
  }

  // Godan causative: せる, せた, せて
  if (isGodanVerbType(verb_type)) {
    return utf8::endsWith(surface, "せる") ||
           utf8::endsWith(surface, "せた") ||
           utf8::endsWith(surface, "せて");
  }
  return false;
}

bool shouldSkipSuruVerbAuxPattern(std::string_view surface, size_t kanji_count) {
  // Only apply to patterns with 2+ kanji
  if (kanji_count < 2) {
    return false;
  }

  // Check for suru-verb auxiliary suffixes
  static const std::vector<std::string_view> kSuruAuxSuffixes = {
      // Basic conjugations
      "して", "した", "しない", "します", "しました", "しません",
      "している", "していた", "していない", "しています", "していました",
      "したい", "しよう", "しろ", "せよ", "すれば", "しそう",
      "しなかった", "しませんでした",
      // Negative te-form
      "しなくて", "しないで", "しなく",
      // Conditional/conjunctive forms
      "しなければ", "しながら", "しつつ", "したら", "しましたら",
      // Colloquial contractions
      "しちゃう", "しちゃった", "しちゃって", "しちゃいます",
      "しちまう", "しちまった", "しちまって",
      "しとく", "しといた", "しといて", "しときます",
      "してる", "してた", "してます", "してました",
      // te-form + subsidiary verbs
      "してみる", "していく", "してくる", "してもらう", "してあげる",
      "してしまう", "してくれる", "してほしい", "してください", "してくれます",
      "してあります", "しておきます", "しておく",
      // Subsidiary verbs past/te-forms
      "してみた", "してみて", "していった", "していって",
      "してきた", "してきて", "してもらった", "してもらって",
      "してあげた", "してあげて", "してくれた", "してくれて",
      "してしまった", "してしまって", "しておいた", "しておいて",
      // Progressive forms of subsidiary verbs
      "してもらっている", "してもらっていた", "してもらっています",
      "してあげている", "してあげていた", "してあげています",
      "してくれている", "してくれていた", "してくれています",
      "していっている", "していっていた",
      "してきている", "してきていた", "してきています"
  };

  for (const auto& suffix : kSuruAuxSuffixes) {
    if (utf8::endsWith(surface, suffix) && surface.size() > suffix.size()) {
      return true;
    }
  }

  return false;
}

}  // namespace suzume::analysis::verb_helpers
