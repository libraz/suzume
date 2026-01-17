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

bool isVerbInDictionary(const dictionary::DictionaryManager* dict_manager,
                        std::string_view base_form) {
  if (dict_manager == nullptr || base_form.empty()) {
    return false;
  }
  auto results = dict_manager->lookup(base_form, 0);
  for (const auto& result : results) {
    if (result.entry != nullptr && result.entry->surface == base_form &&
        result.entry->pos == core::PartOfSpeech::Verb) {
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

std::string codepointToUtf8(char32_t c) {
  std::string result;
  result += static_cast<char>(0xE0 | ((c >> 12) & 0x0F));
  result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
  result += static_cast<char>(0x80 | (c & 0x3F));
  return result;
}

char32_t getHiraganaVowel(char32_t c) {
  // あ-row (a-vowel)
  if (c == U'あ' || c == U'ぁ' || c == U'か' || c == U'が' || c == U'さ' ||
      c == U'ざ' || c == U'た' || c == U'だ' || c == U'な' || c == U'は' ||
      c == U'ば' || c == U'ぱ' || c == U'ま' || c == U'や' || c == U'ゃ' ||
      c == U'ら' || c == U'わ') {
    return U'あ';
  }
  // い-row (i-vowel)
  if (c == U'い' || c == U'ぃ' || c == U'き' || c == U'ぎ' || c == U'し' ||
      c == U'じ' || c == U'ち' || c == U'ぢ' || c == U'に' || c == U'ひ' ||
      c == U'び' || c == U'ぴ' || c == U'み' || c == U'り') {
    return U'い';
  }
  // う-row (u-vowel)
  if (c == U'う' || c == U'ぅ' || c == U'く' || c == U'ぐ' || c == U'す' ||
      c == U'ず' || c == U'つ' || c == U'づ' || c == U'ぬ' || c == U'ふ' ||
      c == U'ぶ' || c == U'ぷ' || c == U'む' || c == U'ゆ' || c == U'ゅ' ||
      c == U'る') {
    return U'う';
  }
  // え-row (e-vowel)
  if (c == U'え' || c == U'ぇ' || c == U'け' || c == U'げ' || c == U'せ' ||
      c == U'ぜ' || c == U'て' || c == U'で' || c == U'ね' || c == U'へ' ||
      c == U'べ' || c == U'ぺ' || c == U'め' || c == U'れ') {
    return U'え';
  }
  // お-row (o-vowel)
  if (c == U'お' || c == U'ぉ' || c == U'こ' || c == U'ご' || c == U'そ' ||
      c == U'ぞ' || c == U'と' || c == U'ど' || c == U'の' || c == U'ほ' ||
      c == U'ぼ' || c == U'ぽ' || c == U'も' || c == U'よ' || c == U'ょ' ||
      c == U'ろ' || c == U'を') {
    return U'お';
  }
  // No vowel: ん, っ, punctuation, non-hiragana
  return 0;
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
        emphatic_suffix += codepointToUtf8(c);
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
            emphatic_suffix += codepointToUtf8(expected_vowel);
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
  if (surface.size() < core::kTwoJapaneseCharBytes) {
    return false;
  }

  // Check if surface ends with ます/ました/ましょう/ません
  bool has_masu_aux = false;
  if (surface.size() >= core::kFourJapaneseCharBytes &&
      surface.substr(surface.size() - core::kFourJapaneseCharBytes) == "ましょう") {
    has_masu_aux = true;
  } else if (surface.size() >= core::kThreeJapaneseCharBytes &&
             (surface.substr(surface.size() - core::kThreeJapaneseCharBytes) == "ました" ||
              surface.substr(surface.size() - core::kThreeJapaneseCharBytes) == "ません")) {
    has_masu_aux = true;
  } else if (surface.size() >= core::kTwoJapaneseCharBytes &&
             surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == "ます") {
    has_masu_aux = true;
  }

  if (!has_masu_aux) {
    return false;
  }

  // Don't skip suru-verb passive/causative patterns (され, させ)
  bool is_suru_passive_causative =
      (verb_type == grammar::VerbType::Suru &&
       (surface.find("され") != std::string::npos ||
        surface.find("させ") != std::string::npos));

  return !is_suru_passive_causative;
}

bool shouldSkipSouPattern(std::string_view surface, grammar::VerbType verb_type) {
  if (surface.size() < core::kTwoJapaneseCharBytes) {
    return false;
  }

  bool has_sou_pattern = false;

  // Check for そう at end
  if (surface.substr(surface.size() - core::kTwoJapaneseCharBytes) ==
      scorer::kSuffixSou) {
    has_sou_pattern = true;
  }
  // Check for そうです at end
  constexpr const char* kSouDesu = "そうです";
  if (surface.size() >= core::kFourJapaneseCharBytes &&
      surface.substr(surface.size() - core::kFourJapaneseCharBytes) == kSouDesu) {
    has_sou_pattern = true;
  }
  // Check for そうだ at end
  constexpr const char* kSouDa = "そうだ";
  if (surface.size() >= core::kThreeJapaneseCharBytes &&
      surface.substr(surface.size() - core::kThreeJapaneseCharBytes) == kSouDa) {
    has_sou_pattern = true;
  }

  // Don't skip i-adjective patterns
  return has_sou_pattern && verb_type != grammar::VerbType::IAdjective;
}

bool isCompoundAdjectivePattern(std::string_view surface) {
  if (surface.size() < core::kFourJapaneseCharBytes) {
    return false;
  }
  return surface.find("にくい") != std::string::npos ||
         surface.find("にくく") != std::string::npos ||
         surface.find("にくか") != std::string::npos ||
         surface.find("やすい") != std::string::npos ||
         surface.find("やすく") != std::string::npos ||
         surface.find("やすか") != std::string::npos ||
         surface.find("がたい") != std::string::npos ||
         surface.find("がたく") != std::string::npos;
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
  if (surface.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view last3 =
        surface.substr(surface.size() - core::kThreeJapaneseCharBytes);
    if (last3 == "れべき") {
      return true;
    }
  }

  // Only apply remaining checks to Godan verbs
  if (!isGodanVerbType(verb_type)) {
    return false;
  }

  // Passive patterns: れる, れた, れて, れない, れべき
  if (surface.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view last3 =
        surface.substr(surface.size() - core::kThreeJapaneseCharBytes);
    if (last3 == "れる" || last3 == "れた" || last3 == "れて") {
      return true;
    }
  }
  if (surface.size() >= core::kFourJapaneseCharBytes) {
    std::string_view last4 =
        surface.substr(surface.size() - core::kFourJapaneseCharBytes);
    if (last4 == "れない" || last4 == "れます") {
      return true;
    }
  }
  // Check for passive + desiderative: れたい, れたく
  if (surface.size() >= core::kFourJapaneseCharBytes) {
    std::string_view last4 =
        surface.substr(surface.size() - core::kFourJapaneseCharBytes);
    if (last4 == "れたい" || last4 == "れたく") {
      return true;
    }
  }
  return false;
}

bool shouldSkipCausativeAuxPattern(std::string_view surface, grammar::VerbType verb_type) {
  // Suru verb causative/passive: stay as single tokens
  if (verb_type == grammar::VerbType::Suru) {
    return false;
  }

  // Godan causative: せる, せた
  if (surface.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view last3 =
        surface.substr(surface.size() - core::kThreeJapaneseCharBytes);
    if (last3 == "せる" || last3 == "せた" || last3 == "せて") {
      if (isGodanVerbType(verb_type)) {
        return true;
      }
    }
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
    if (surface.size() > suffix.size() &&
        surface.substr(surface.size() - suffix.size()) == suffix) {
      return true;
    }
  }

  return false;
}

}  // namespace suzume::analysis::verb_helpers
