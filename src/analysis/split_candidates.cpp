/**
 * @file split_candidates.cpp
 * @brief Split-based candidate generation for tokenizer
 */

#include "split_candidates.h"

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
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

using normalize::isCounterKanji;

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
    const Scorer& scorer,
    const dictionary::DictionaryManager& dict_manager) {
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
  size_t max_first_len =
      (first_type == CharType::Alphabet) ? kMaxAlphaLen : kMaxDigitLen;
  size_t first_end = findCharRegionEnd(char_types, start_pos, max_first_len,
                                        first_type);

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
  size_t max_end = findCharRegionEnd(char_types, first_end, max_second_len,
                                      second_type);

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

      // Count how many leading kanji are counter/unit kanji
      size_t counter_prefix_len = 0;
      for (size_t k = 0; k < kanji_len; ++k) {
        if (isCounterKanji(codepoints[first_end + k])) {
          counter_prefix_len = k + 1;
        } else {
          break;
        }
      }

      // Apply length-based bonus/penalty
      float length_adjustment;
      if (counter_prefix_len == 0) {
        // First kanji is not a counter (学園, 世界, etc.) — skip
        continue;
      } else if (kanji_len <= counter_prefix_len) {
        // All kanji are counters
        if (kanji_len == 1) {
          if (codepoints[first_end] == U'対') {
            length_adjustment = bigram_cost::kStrong;  // 2対1 → 2|対|1
          } else {
            length_adjustment = opts.digit_kanji_1_bonus;  // 5分, 3月
          }
        } else if (kanji_len == 2) {
          length_adjustment = opts.digit_kanji_2_bonus;  // 5分間, 3時間
        } else {
          length_adjustment = opts.digit_kanji_3_penalty;  // Rare
        }
      } else {
        // Counter prefix + non-counter kanji: only allow when the full kanji
        // portion exists as a dictionary entry (e.g., 次元 in dict → 2次元 OK)
        size_t kanji_start_byte = charPosToBytePos(codepoints, first_end);
        std::string kanji_part(text.substr(kanji_start_byte,
                                           end_byte - kanji_start_byte));
        auto lookup = dict_manager.lookup(kanji_part, 0);
        bool found_exact = false;
        for (const auto& r : lookup) {
          if (r.entry->surface == kanji_part) {
            found_exact = true;
            break;
          }
        }
        if (!found_exact) {
          continue;  // Skip: kanji portion not a known word
        }
        // Dict-verified extension gets stronger bonus than regular counters
        length_adjustment = -0.8F;
      }

      float final_cost = base_cost + length_adjustment;
      SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_MIX] \"" << surface << "\": digit+kanji"
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
    SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_MIX] \"" << surface << "\": alpha+"
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
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, kMaxCompoundLen,
                                        CharType::Kanji);

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
        // v0.8: cost from extended_pos
        first_cost = getCategoryCost(result.entry->extended_pos) + opts.dict_split_bonus;
        first_is_formal_noun = (result.entry->extended_pos == core::ExtendedPOS::NounFormal);
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
    const Scorer& scorer,
    const grammar::Inflection& inflection) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Only for kanji-starting sequences
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Find the extent of kanji sequence
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, kMaxNounLen + 3,
                                        CharType::Kanji);

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
  size_t max_hiragana_end = findCharRegionEnd(char_types, kanji_end, 10,
                                               CharType::Hiragana);

  // Need at least 1 hiragana for verb ending
  if (max_hiragana_end <= kanji_end) {
    return;
  }

  // Use inflection analysis to check if verb part looks conjugated

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
        // v0.8: cost from extended_pos
        noun_cost = getCategoryCost(result.entry->extended_pos);
        is_formal_noun = (result.entry->extended_pos == core::ExtendedPOS::NounFormal);
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
      const auto& candidates = inflection.analyze(verb_part);
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
        // Skip split if last kanji of noun + first kanji of verb forms a known
        // dictionary word. e.g., 作画崩+壊し → skip because 崩壊 is a dict word
        // (prefer 作画崩壊+し). Also check last kanji + full verb part for cases
        // like 大掃除+する → skip because 掃除 is a dict word.
        if (noun_surface.size() >= 6) {  // Noun has at least 2 kanji (6 bytes UTF-8)
          auto noun_cps = normalize::toCodepoints(noun_surface);
          if (!noun_cps.empty()) {
            std::string last_kanji = normalize::encodeUtf8(noun_cps.back());
            // Check last_kanji + verb_part (e.g., 除+する = 掃除する? no, but 除する? no)
            std::string alt_word = last_kanji + std::string(verb_part);
            auto alt_results = dict_manager.lookup(alt_word, 0);
            for (const auto& result : alt_results) {
              if (result.entry != nullptr && result.entry->surface == alt_word) {
                SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_NV] skip \"" << noun_surface
                    << "\" + \"" << verb_part
                    << "\": alt dict word \"" << alt_word << "\" exists\n");
                goto next_split;
              }
            }
            // Check last_kanji + first_kanji_of_verb (e.g., 崩+壊 = 崩壊)
            // This catches compounds where the verb's kanji belongs to a noun
            if (verb_start < kanji_end) {
              std::string first_verb_kanji = normalize::encodeUtf8(codepoints[verb_start]);
              std::string compound = last_kanji + first_verb_kanji;
              auto comp_results = dict_manager.lookup(compound, 0);
              for (const auto& result : comp_results) {
                if (result.entry != nullptr && result.entry->surface == compound) {
                  SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_NV] skip \"" << noun_surface
                      << "\" + \"" << verb_part
                      << "\": compound \"" << compound << "\" is dict word\n");
                  goto next_split;
                }
              }
            }
          }
        }

        {
        const auto& opts = scorer.splitOpts();
        float final_noun_cost = noun_cost + opts.noun_verb_split_bonus;

        if (base_in_dict) {
          final_noun_cost += opts.verified_verb_bonus;
        }

        if (noun_in_dict && base_in_dict) {
          final_noun_cost -= 0.2F;
        }

        // Penalty for single-kanji noun + verb split
        // E.g., 勘+違い should prefer 勘違い (compound noun)
        // Single-kanji nouns rarely form valid noun+verb compounds
        if (noun_surface.size() == 3) {  // Single kanji (3 bytes UTF-8)
          final_noun_cost += bigram_cost::kStrong;
        }

        SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_NV] \"" << noun_surface << "\" + \"" << verb_part
                          << "\": noun_dict=" << noun_in_dict
                          << " verb_dict=" << base_in_dict
                          << " cost=" << final_noun_cost << "\n");

        uint8_t noun_flags = noun_in_dict ? core::LatticeEdge::kFromDictionary
                                           : core::LatticeEdge::kIsUnknown;

        lattice.addEdge(noun_surface, static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(verb_start), core::PartOfSpeech::Noun,
                        final_noun_cost, noun_flags, "");

        break;
        }  // end alt-dict-word check scope
        next_split:;
      }
    }
  }
}

}  // namespace suzume::analysis
