/**
 * @file split_candidates.cpp
 * @brief Split-based candidate generation for tokenizer
 */

#include "split_candidates.h"

#include "grammar/inflection.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

namespace {

// Cost bonuses for mixed script joining (lower cost = preferred)
constexpr float kAlphaKanjiBonus = -0.3F;    // Web開発, AI研究
constexpr float kAlphaKatakanaBonus = -0.3F; // APIリクエスト
constexpr float kDigitKanjiBonus = -0.2F;    // 3月, 100人

// Maximum lengths for mixed script segments
constexpr size_t kMaxAlphaLen = 12;   // Reasonable limit for English words
constexpr size_t kMaxDigitLen = 8;    // Reasonable limit for numbers
constexpr size_t kMaxJapaneseLen = 8; // Reasonable limit for Japanese part

// Minimum kanji sequence length for compound splitting
constexpr size_t kMinCompoundLen = 4;

// Maximum kanji sequence length to consider
constexpr size_t kMaxCompoundLen = 10;

// Bonus for dictionary-guided splits (lower = preferred)
constexpr float kDictSplitBonus = -0.5F;

// Base cost for split candidates
constexpr float kSplitBaseCost = 1.0F;

// Cost bonus for noun+verb splits (lower = preferred)
constexpr float kNounVerbSplitBonus = -1.0F;

// Additional bonus when verb base form is verified in dictionary
constexpr float kVerifiedVerbBonus = -0.8F;

// Maximum noun length (in characters) to consider for splitting
constexpr size_t kMaxNounLen = 6;

// Maximum hiragana length for verb suffix
constexpr size_t kMaxVerbHiraganaLen = 8;

}  // namespace

void addMixedScriptCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  CharType first_type = char_types[start_pos];

  // Only start from Alphabet or Digit
  if (first_type != CharType::Alphabet && first_type != CharType::Digit) {
    return;
  }

  // Find the end of the first segment (continuous same type)
  size_t first_end = start_pos + 1;
  size_t max_first_len =
      (first_type == CharType::Alphabet) ? kMaxAlphaLen : kMaxDigitLen;

  while (first_end < char_types.size() &&
         first_end - start_pos < max_first_len &&
         char_types[first_end] == first_type) {
    ++first_end;
  }

  // Check if there's a second segment to join with
  if (first_end >= char_types.size()) {
    return;
  }

  CharType second_type = char_types[first_end];

  // Determine if this is a valid mixed script pattern
  float bonus = 0.0F;
  if (first_type == CharType::Alphabet) {
    if (second_type == CharType::Kanji) {
      bonus = kAlphaKanjiBonus;
    } else if (second_type == CharType::Katakana) {
      bonus = kAlphaKatakanaBonus;
    } else {
      return;  // Not a valid pattern
    }
  } else if (first_type == CharType::Digit) {
    if (second_type == CharType::Kanji) {
      bonus = kDigitKanjiBonus;
    } else {
      return;  // Not a valid pattern
    }
  }

  // Find the end of the second segment
  size_t second_end = first_end + 1;
  while (second_end < char_types.size() &&
         second_end - first_end < kMaxJapaneseLen &&
         char_types[second_end] == second_type) {
    ++second_end;
  }

  // Generate merged candidate
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  size_t end_byte = charPosToBytePos(codepoints, second_end);

  std::string surface(text.substr(start_byte, end_byte - start_byte));

  // Base cost from POS prior, plus bonus for mixed script joining
  float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
  float final_cost = base_cost + bonus;

  uint8_t flags = core::LatticeEdge::kIsUnknown;
  lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(second_end), core::PartOfSpeech::Noun,
                  final_cost, flags, "");
}

void addCompoundSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Only for kanji sequences
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Find the end of the kanji sequence
  size_t kanji_end = start_pos + 1;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < kMaxCompoundLen &&
         char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  size_t kanji_len = kanji_end - start_pos;

  // Only generate split candidates for 4+ character sequences
  if (kanji_len < kMinCompoundLen) {
    return;
  }

  // Get byte positions
  size_t start_byte = charPosToBytePos(codepoints, start_pos);

  // Try different split points
  for (size_t split_point = 2; split_point < kanji_len; ++split_point) {
    size_t first_end = start_pos + split_point;
    size_t first_end_byte = charPosToBytePos(codepoints, first_end);

    // Check if the first part matches a dictionary entry
    auto first_results = dict_manager.lookup(text, start_byte);
    bool first_in_dict = false;
    bool first_is_formal_noun = false;
    float first_cost = kSplitBaseCost;

    for (const auto& result : first_results) {
      if (result.entry != nullptr && result.length == split_point) {
        first_in_dict = true;
        first_cost = result.entry->cost + kDictSplitBonus;
        first_is_formal_noun = result.entry->is_formal_noun;
        break;
      }
    }

    // Check if the second part matches a dictionary entry
    auto second_results = dict_manager.lookup(text, first_end_byte);
    bool second_in_dict = false;

    for (const auto& result : second_results) {
      if (result.entry != nullptr && result.length == kanji_len - split_point) {
        second_in_dict = true;
        break;
      }
    }

    // Only add split candidate if at least one part is in dictionary
    if (first_in_dict || second_in_dict) {
      // Add the first part as a candidate
      std::string first_surface(text.substr(start_byte, first_end_byte - start_byte));
      uint8_t flags = first_in_dict ? core::LatticeEdge::kFromDictionary
                                     : core::LatticeEdge::kIsUnknown;
      if (first_is_formal_noun) {
        flags |= core::LatticeEdge::kIsFormalNoun;
      }

      lattice.addEdge(first_surface, static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(first_end), core::PartOfSpeech::Noun,
                      first_cost, flags, "");

      // If both parts are in dictionary, give extra bonus
      if (first_in_dict && second_in_dict) {
        float bonus_cost = first_cost - 0.2F;
        lattice.addEdge(first_surface, static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(first_end), core::PartOfSpeech::Noun,
                        bonus_cost, flags, "");
      }
    }
  }
}

void addNounVerbSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Only for kanji-starting sequences
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Find the extent of kanji sequence
  size_t kanji_end = start_pos + 1;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < kMaxNounLen + 3 &&
         char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  // Need at least 2 kanji to consider noun+verb split
  if (kanji_end - start_pos < 2) {
    return;
  }

  // Check if hiragana follows (potential verb ending)
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != CharType::Hiragana) {
    return;
  }

  // Find the maximum extent of hiragana sequence
  size_t max_hiragana_end = kanji_end;
  while (max_hiragana_end < char_types.size() &&
         max_hiragana_end - kanji_end < 10 &&
         char_types[max_hiragana_end] == CharType::Hiragana) {
    ++max_hiragana_end;
  }

  // Need at least 1 hiragana for verb ending
  if (max_hiragana_end <= kanji_end) {
    return;
  }

  // Use inflection analysis to check if verb part looks conjugated
  static grammar::Inflection inflection;

  size_t start_byte = charPosToBytePos(codepoints, start_pos);

  // Try different noun lengths
  for (size_t noun_len = 1; noun_len < kanji_end - start_pos; ++noun_len) {
    size_t verb_start = start_pos + noun_len;
    size_t verb_start_byte = charPosToBytePos(codepoints, verb_start);

    // Check if noun part is in dictionary
    auto noun_results = dict_manager.lookup(text, start_byte);
    bool noun_in_dict = false;
    float noun_cost = 1.0F;

    for (const auto& result : noun_results) {
      if (result.entry != nullptr && result.length == noun_len) {
        noun_in_dict = true;
        noun_cost = result.entry->cost;
        break;
      }
    }

    // Try different hiragana lengths for verb ending
    size_t hiragana_extent = max_hiragana_end - kanji_end;
    size_t max_try_len = std::min(hiragana_extent, kMaxVerbHiraganaLen);

    for (size_t hira_len = 1; hira_len <= max_try_len; ++hira_len) {
      size_t verb_end = kanji_end + hira_len;
      size_t verb_end_byte = charPosToBytePos(codepoints, verb_end);

      // Extract the potential verb part
      std::string verb_part(text.substr(verb_start_byte, verb_end_byte - verb_start_byte));

      // Check if the verb part looks like a conjugated verb
      bool looks_like_verb = false;
      auto candidates = inflection.analyze(verb_part);
      for (const auto& cand : candidates) {
        if (cand.confidence > 0.5F) {
          looks_like_verb = true;
          break;
        }
      }

      // Check if any candidate's base form is in dictionary
      bool base_in_dict = false;
      for (const auto& cand : candidates) {
        if (cand.confidence < 0.5F) {
          continue;
        }
        auto base_results = dict_manager.lookup(cand.base_form, 0);
        for (const auto& result : base_results) {
          if (result.entry != nullptr &&
              result.entry->surface == cand.base_form &&
              result.entry->pos == core::PartOfSpeech::Verb) {
            base_in_dict = true;
            break;
          }
        }
        if (base_in_dict) {
          break;
        }
      }

      // Generate split candidates if conditions are met
      if ((noun_in_dict && looks_like_verb) || base_in_dict) {
        std::string noun_surface(text.substr(start_byte, verb_start_byte - start_byte));
        float final_noun_cost = noun_cost + kNounVerbSplitBonus;

        if (base_in_dict) {
          final_noun_cost += kVerifiedVerbBonus;
        }

        if (noun_in_dict && base_in_dict) {
          final_noun_cost -= 0.2F;
        }

        uint8_t noun_flags = noun_in_dict ? core::LatticeEdge::kFromDictionary
                                           : core::LatticeEdge::kIsUnknown;

        lattice.addEdge(noun_surface, static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(verb_start), core::PartOfSpeech::Noun,
                        final_noun_cost, noun_flags, "");

        break;
      }
    }
  }
}

}  // namespace suzume::analysis
