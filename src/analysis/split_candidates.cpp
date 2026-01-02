/**
 * @file split_candidates.cpp
 * @brief Split-based candidate generation for tokenizer
 */

#include "split_candidates.h"

#include "candidate_constants.h"
#include "core/debug.h"
#include "grammar/inflection.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

namespace {

// Cost bonuses imported from candidate_constants.h:
// candidate::kAlphaKanjiBonus, kAlphaKatakanaBonus
// candidate::kDigitKanji1Bonus, kDigitKanji2Bonus, kDigitKanji3Penalty
// candidate::kDictSplitBonus, kSplitBaseCost
// candidate::kNounVerbSplitBonus, kVerifiedVerbBonus

// Maximum lengths for mixed script segments
constexpr size_t kMaxAlphaLen = 12;   // Reasonable limit for English words
constexpr size_t kMaxDigitLen = 8;    // Reasonable limit for numbers
constexpr size_t kMaxJapaneseLen = 8; // Reasonable limit for Japanese part
constexpr size_t kMaxDigitKanjiLen = 3; // Max kanji for digit+kanji (counters)

// Minimum kanji sequence length for compound splitting
constexpr size_t kMinCompoundLen = 4;

// Maximum kanji sequence length to consider
constexpr size_t kMaxCompoundLen = 10;

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

  // Validate and get base bonus based on pattern type
  bool is_digit_kanji = false;
  float base_bonus = 0.0F;
  size_t max_second_len = kMaxJapaneseLen;

  const auto& opts = scorer.splitOpts();
  if (first_type == CharType::Alphabet) {
    if (second_type == CharType::Kanji) {
      base_bonus = opts.alpha_kanji_bonus;
    } else if (second_type == CharType::Katakana) {
      base_bonus = opts.alpha_katakana_bonus;
    } else {
      return;  // Not a valid pattern
    }
  } else if (first_type == CharType::Digit) {
    if (second_type == CharType::Kanji) {
      is_digit_kanji = true;
      max_second_len = kMaxDigitKanjiLen;  // Limit kanji length for counters
    } else {
      return;  // Not a valid pattern
    }
  }

  // Find the maximum extent of the second segment
  size_t max_end = first_end + 1;
  while (max_end < char_types.size() &&
         max_end - first_end < max_second_len &&
         char_types[max_end] == second_type) {
    ++max_end;
  }

  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
  uint8_t flags = core::LatticeEdge::kIsUnknown;

  if (is_digit_kanji) {
    // For digit+kanji, generate multiple candidates with length-based costs
    // This allows Viterbi to choose the best segmentation
    for (size_t kanji_len = 1; kanji_len <= max_end - first_end; ++kanji_len) {
      size_t candidate_end = first_end + kanji_len;
      size_t end_byte = charPosToBytePos(codepoints, candidate_end);
      std::string surface(text.substr(start_byte, end_byte - start_byte));

      // Apply length-based bonus/penalty
      float length_adjustment;
      if (kanji_len == 1) {
        length_adjustment = opts.digit_kanji_1_bonus;  // Best: 5分, 3月
      } else if (kanji_len == 2) {
        length_adjustment = opts.digit_kanji_2_bonus;  // Good: 5分間, 3時間
      } else {
        length_adjustment = opts.digit_kanji_3_penalty;  // Rare: penalize
      }

      float final_cost = base_cost + length_adjustment;
      SUZUME_DEBUG_LOG("[SPLIT_MIX] \"" << surface << "\": digit+kanji"
                        << kanji_len << " adj=" << length_adjustment << "\n");
      lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(candidate_end),
                      core::PartOfSpeech::Noun, final_cost, flags, "");
    }
  } else {
    // For alphabet+kanji/katakana, generate single candidate (original behavior)
    size_t end_byte = charPosToBytePos(codepoints, max_end);
    std::string surface(text.substr(start_byte, end_byte - start_byte));
    float final_cost = base_cost + base_bonus;
    SUZUME_DEBUG_LOG("[SPLIT_MIX] \"" << surface << "\": alpha+"
                      << (second_type == CharType::Kanji ? "kanji" : "katakana")
                      << " bonus=" << base_bonus << "\n");
    lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(max_end), core::PartOfSpeech::Noun,
                    final_cost, flags, "");
  }
}

void addCompoundSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
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
    const auto& opts = scorer.splitOpts();
    float first_cost = opts.split_base_cost;

    for (const auto& result : first_results) {
      if (result.entry != nullptr && result.length == split_point &&
          (result.entry->pos == core::PartOfSpeech::Noun ||
           result.entry->pos == core::PartOfSpeech::Adjective)) {
        // Allow NOUN and ADJ (na-adjectives can function as nominal in compounds)
        // This prevents ADV/VERB from being incorrectly reregistered as NOUN
        first_in_dict = true;
        first_cost = result.entry->cost + opts.dict_split_bonus;
        first_is_formal_noun = result.entry->is_formal_noun;
        break;
      }
    }

    // Check if the second part matches a dictionary entry (NOUN or ADJ)
    auto second_results = dict_manager.lookup(text, first_end_byte);
    bool second_in_dict = false;

    for (const auto& result : second_results) {
      if (result.entry != nullptr && result.length == kanji_len - split_point &&
          (result.entry->pos == core::PartOfSpeech::Noun ||
           result.entry->pos == core::PartOfSpeech::Adjective)) {
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
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
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

    // Check if noun part is in dictionary as NOUN
    // Only consider actual NOUN entries, not ADV/VERB/etc.
    // Skip formal nouns (中, 上, 下, etc.) - they shouldn't split from preceding noun
    auto noun_results = dict_manager.lookup(text, start_byte);
    bool noun_in_dict = false;
    bool is_formal_noun = false;
    float noun_cost = 1.0F;

    for (const auto& result : noun_results) {
      if (result.entry != nullptr && result.length == noun_len &&
          result.entry->pos == core::PartOfSpeech::Noun) {
        noun_in_dict = true;
        noun_cost = result.entry->cost;
        is_formal_noun = result.entry->is_formal_noun;
        break;
      }
    }

    // Skip N+V split if noun is a formal/bound noun (e.g., 中, 上, 下)
    // These typically attach to preceding nouns, not verbs
    if (is_formal_noun) {
      continue;
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

        // Skip split if noun + first kanji of verb forms a known compound
        // e.g., 上+手く should not split because 上手 is a dictionary word
        if (verb_start < kanji_end) {
          size_t compound_end_byte = charPosToBytePos(codepoints, verb_start + 1);
          std::string compound(text.substr(start_byte, compound_end_byte - start_byte));
          auto compound_results = dict_manager.lookup(compound, 0);
          bool compound_in_dict = false;
          for (const auto& result : compound_results) {
            if (result.entry != nullptr && result.entry->surface == compound) {
              compound_in_dict = true;
              break;
            }
          }
          if (compound_in_dict) {
            continue;  // Skip this split, prefer compound word
          }
        }
        const auto& opts = scorer.splitOpts();
        float final_noun_cost = noun_cost + opts.noun_verb_split_bonus;

        if (base_in_dict) {
          final_noun_cost += opts.verified_verb_bonus;
        }

        if (noun_in_dict && base_in_dict) {
          final_noun_cost -= 0.2F;
        }

        SUZUME_DEBUG_LOG("[SPLIT_NV] \"" << noun_surface << "\" + \"" << verb_part
                          << "\": noun_dict=" << noun_in_dict
                          << " verb_dict=" << base_in_dict
                          << " cost=" << final_noun_cost << "\n");

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
