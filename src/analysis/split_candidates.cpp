/**
 * @file split_candidates.cpp
 * @brief Split-based candidate generation for tokenizer
 */

#include "split_candidates.h"

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/honorific_verbs.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

namespace {

// Cost bonuses imported from candidate_constants.h:
// candidate::kAlphaKanjiBonus, kAlphaKatakanaBonus
// candidate::kDictSplitBonus, kSplitBaseCost
// candidate::kNounVerbSplitBonus, kVerifiedVerbBonus

// Maximum lengths for mixed script segments
constexpr size_t kMaxAlphaLen = 12;      // Reasonable limit for English words
constexpr size_t kMaxDigitLen = 8;       // Reasonable limit for numbers
constexpr size_t kMaxJapaneseLen = 8;    // Reasonable limit for Japanese part
constexpr size_t kMaxDigitKanjiLen = 3;  // Max kanji for digit+kanji (counters)

using normalize::isCounterKanji;
using normalize::isQuantityPhraseSuffixKanji;

// Minimum kanji sequence length for compound splitting
constexpr size_t kMinCompoundLen = 4;

// Maximum kanji sequence length to consider
constexpr size_t kMaxCompoundLen = 10;

// Maximum noun length (in characters) to consider for splitting
constexpr size_t kMaxNounLen = 6;

// Maximum hiragana length for verb suffix
constexpr size_t kMaxVerbHiraganaLen = 8;

bool hasSuruContinuation(const std::vector<char32_t>& codepoints, size_t suffix_start) {
  if (suffix_start >= codepoints.size()) {
    return false;
  }

  if (codepoints[suffix_start] == U'す') {
    return suffix_start + 1 < codepoints.size() && codepoints[suffix_start + 1] == U'る';
  }

  if (codepoints[suffix_start] != U'し' || suffix_start + 1 >= codepoints.size()) {
    return false;
  }

  char32_t next_char = codepoints[suffix_start + 1];
  return normalize::isKanjiCodepoint(next_char) || next_char == U'ち' || next_char == U'て' || next_char == U'た' ||
         next_char == U'な' || next_char == U'ま' || next_char == U'よ' || next_char == U'ろ' || next_char == U'そ' ||
         next_char == U'と' || next_char == U'か' || next_char == U'つ';
}

bool suffixHeadedRunAbsorbsVerifiedGodanStem(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             size_t kanji_end, const dictionary::DictionaryManager& dict_manager) {
  if (start_pos >= kanji_end || kanji_end >= codepoints.size() ||
      lookupEntryInRange(dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Suffix) == nullptr) {
    return false;
  }
  const std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(codepoints[kanji_end]);
  return !base_suffix.empty() &&
         dict_manager.lookupExact(
             normalize::concat(extractSubstring(codepoints, kanji_end - 1, kanji_end), base_suffix),
             core::PartOfSpeech::Verb) != nullptr;
}

bool hasDictionaryLexicalPrefix(const std::vector<dictionary::LookupResult>& results, size_t full_length) {
  for (const auto& result : results) {
    if (result.entry == nullptr || result.length >= full_length) {
      continue;
    }
    // A one-kanji suffix homograph is not evidence that a lexical search unit
    // begins here (中 in 中断, for example). Multi-kanji closed entries such as
    // 以後 and content entries such as 各自 still establish a real boundary.
    const bool is_single_kanji_suffix = result.length == 1 && result.entry->pos == core::PartOfSpeech::Suffix;
    if (!is_single_kanji_suffix) {
      return true;
    }
  }
  return false;
}

bool containsIterationMark(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  for (size_t index = start_pos; index < end_pos; ++index) {
    if (normalize::isIterationMark(codepoints[index])) {
      return true;
    }
  }
  return false;
}

// A suru verbal-noun candidate may not start inside a dictionary-backed
// modifier and absorb the following kanji predicate (ある程度|確認する,
// not ある|程度確認|する). Restrict this guard to the specialized
// suru-noun generator; applying it to all unknown candidates makes an
// overlapping modifier homograph at an invalid start position self-validating.
bool crossesModifierBoundaryForSuruNoun(std::string_view text, const ByteOffsets& byte_offsets, size_t candidate_start,
                                        size_t candidate_end, const dictionary::DictionaryManager& dict_manager) {
  constexpr size_t kModifierLookbackChars = 8;
  const size_t lookback = std::min(candidate_start, kModifierLookbackChars);
  for (size_t back = 1; back <= lookback; ++back) {
    const size_t modifier_start = candidate_start - back;
    for (const auto& result : dict_manager.lookup(text, byteOffsetAt(byte_offsets, modifier_start))) {
      if (result.entry == nullptr || result.length <= back) {
        continue;
      }
      const auto pos = result.entry->pos;
      const bool is_modifier = pos == core::PartOfSpeech::Adverb || pos == core::PartOfSpeech::Determiner ||
                               pos == core::PartOfSpeech::Conjunction;
      if (is_modifier && modifier_start + result.length < candidate_end) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

void addMixedScriptCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                              const ByteOffsets& byte_offsets, size_t start_pos,
                              const std::vector<normalize::CharType>& char_types, const Scorer& scorer,
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
  size_t max_first_len = (first_type == CharType::Alphabet) ? kMaxAlphaLen : kMaxDigitLen;
  size_t first_end = findCharRegionEnd(char_types, start_pos, max_first_len, first_type);

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
    // A letter run immediately after a number is an ASCII unit (1kg, 5cm,
    // 20MB), not the head of an alpha+kanji compound. Keeping this boundary
    // lets the preceding alphanumeric candidate remain the quantity token and
    // prevents kg未満 from swallowing the following Japanese predicate.
    if (start_pos > 0 && char_types[start_pos - 1] == CharType::Digit) {
      return;
    }
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
  size_t max_end = findCharRegionEnd(char_types, first_end, max_second_len, second_type);

  size_t start_byte = byteOffsetAt(byte_offsets, start_pos);
  float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
  uint8_t flags = core::LatticeEdge::kIsUnknown;

  if (is_digit_kanji) {
    // A 漢語 compound decomposes into two-kanji units, so the length cap above
    // is not enough to place the cut: it only bounds how much of the run the
    // quantity phrase may take. Measure the whole run so an odd leftover can be
    // charged below.
    size_t kanji_run_end = first_end;
    while (kanji_run_end < char_types.size() && char_types[kanji_run_end] == CharType::Kanji) {
      ++kanji_run_end;
    }
    // A quantity-phrase suffix closing the run sits outside those two-kanji
    // units: it binds to the whole counter phrase on its left (段階+目, 週+間).
    // Counting it would invert the parity and push the cut one kanji early.
    if (kanji_run_end > first_end && isQuantityPhraseSuffixKanji(codepoints[kanji_run_end - 1])) {
      --kanji_run_end;
    }
    // For digit+kanji, generate multiple candidates with length-based costs
    // This allows Viterbi to choose the best segmentation
    for (size_t kanji_len = 1; kanji_len <= max_end - first_end; ++kanji_len) {
      size_t candidate_end = first_end + kanji_len;
      size_t end_byte = byteOffsetAt(byte_offsets, candidate_end);
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

      float length_adjustment;  // length-based bonus/penalty
      if (counter_prefix_len == 0) {
        // First kanji is not a counter (学園, 世界, etc.) — skip
        continue;
      } else if (kanji_len <= counter_prefix_len) {  // all kanji are counters
        if (kanji_len == 1) {
          if (codepoints[first_end] == U'対') {
            length_adjustment = bigram_cost::kStrong;  // 2対1 → 2|対|1
          } else {
            length_adjustment = opts.digit_kanji_1_bonus;  // 5分, 3月
          }
        } else if (kanji_len == 2) {
          length_adjustment = opts.digit_kanji_2_bonus;  // 5分間, 3時間
          length_adjustment += periodSuffixSplitBonus(codepoints, candidate_end, opts.duration_period_bonus);
        } else {
          length_adjustment = opts.digit_kanji_3_penalty;  // Rare
        }
      } else {
        // Counter prefix + non-counter kanji: allow only if the kanji run is a dict entry (2次元).
        size_t kanji_start_byte = byteOffsetAt(byte_offsets, first_end);
        std::string kanji_part(text.substr(kanji_start_byte, end_byte - kanji_start_byte));
        bool found_exact = dict_manager.lookupExact(kanji_part) != nullptr;
        if (!found_exact) {
          continue;  // Skip: kanji portion not a known word
        }
        // Dict-verified extension gets stronger bonus than regular counters
        length_adjustment = -0.8F;
      }

      // A cut that strands an odd number of kanji has landed on the wrong
      // boundary: 3段階評価 cuts after the lone counter 段 and leaves the
      // fragment 階評価, while the even cut leaves the word 評価. Charge the odd
      // leftover so the even boundary wins. When the run offers no even cut the
      // charge is uniform across candidates, so the quantity phrase still beats
      // the bare numeral (3年 + 計画書).
      if ((kanji_run_end - candidate_end) % 2 == 1) {
        length_adjustment += bigram_cost::kMinor;
      }
      float final_cost = base_cost + length_adjustment;
      SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_MIX] \"" << surface << "\": digit+kanji" << kanji_len
                                                << " adj=" << length_adjustment << "\n");
      lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(candidate_end),
                      core::PartOfSpeech::Noun, final_cost, flags, "");
    }
  } else {
    // For alphabet+kanji/katakana, generate single candidate (original behavior)
    size_t end_byte = byteOffsetAt(byte_offsets, max_end);
    std::string surface(text.substr(start_byte, end_byte - start_byte));
    float final_cost = base_cost + base_bonus;
    SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_MIX] \"" << surface << "\": alpha+"
                                              << (second_type == CharType::Kanji ? "kanji" : "katakana")
                                              << " bonus=" << base_bonus << "\n");
    lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(max_end), core::PartOfSpeech::Noun,
                    final_cost, flags, "");
  }
}

void addCompoundSplitCandidates(core::Lattice& lattice, std::string_view text, const ByteOffsets& byte_offsets,
                                size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                const dictionary::DictionaryManager& dict_manager, const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Only for kanji sequences
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Find the end of the kanji sequence
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, kMaxCompoundLen, CharType::Kanji);

  size_t kanji_len = kanji_end - start_pos;

  // Only generate split candidates for 4+ character sequences
  if (kanji_len < kMinCompoundLen) {
    return;
  }

  // Get byte positions
  size_t start_byte = byteOffsetAt(byte_offsets, start_pos);

  // The first-part lookup depends only on start_byte, so it is loop-invariant;
  // hoist it out and filter by result.length per split point below.
  const auto first_results = dict_manager.lookup(text, start_byte);

  // Try different split points
  for (size_t split_point = 2; split_point < kanji_len; ++split_point) {
    size_t first_end = start_pos + split_point;
    size_t first_end_byte = byteOffsetAt(byte_offsets, first_end);

    // Check if the first part matches a dictionary entry
    bool first_in_dict = false;
    bool first_is_formal_noun = false;
    const auto& opts = scorer.splitOpts();
    float first_cost = opts.split_base_cost;

    for (const auto& result : first_results) {
      if (result.entry != nullptr && result.length == split_point &&
          (result.entry->pos == core::PartOfSpeech::Noun || result.entry->pos == core::PartOfSpeech::Adjective)) {
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
          (result.entry->pos == core::PartOfSpeech::Noun || result.entry->pos == core::PartOfSpeech::Adjective)) {
        second_in_dict = true;
        break;
      }
    }

    // Only add split candidate if at least one part is in dictionary
    if (first_in_dict || second_in_dict) {
      // Add the first part as a candidate
      std::string first_surface(text.substr(start_byte, first_end_byte - start_byte));
      uint8_t flags = first_in_dict ? core::LatticeEdge::kFromDictionary : core::LatticeEdge::kIsUnknown;
      if (first_is_formal_noun) {
        flags |= core::LatticeEdge::kIsFormalNoun;
      }

      // Both dictionary-backed halves strengthen the same split hypothesis;
      // encode that evidence in one edge instead of retaining a dominated
      // duplicate with the unbonused cost.
      const float edge_cost = first_in_dict && second_in_dict ? first_cost - 0.2F : first_cost;
      lattice.addEdge(first_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(first_end),
                      core::PartOfSpeech::Noun, edge_cost, flags, "");
    }
  }
}

void addNounVerbSplitCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                                const ByteOffsets& byte_offsets, size_t start_pos,
                                const std::vector<normalize::CharType>& char_types,
                                const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
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
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, kMaxNounLen + 3, CharType::Kanji);

  // Need at least 2 kanji to consider noun+verb split
  if (kanji_end - start_pos < 2) {
    return;
  }

  // Check if hiragana follows (potential verb ending)
  if (kanji_end >= char_types.size() || char_types[kanji_end] != CharType::Hiragana) {
    return;
  }

  // Find the maximum extent of hiragana sequence
  size_t max_hiragana_end = findCharRegionEnd(char_types, kanji_end, 10, CharType::Hiragana);

  // Need at least 1 hiragana for verb ending
  if (max_hiragana_end <= kanji_end) {
    return;
  }

  // Use inflection analysis to check if verb part looks conjugated

  size_t start_byte = byteOffsetAt(byte_offsets, start_pos);
  const auto noun_results = dict_manager.lookup(text, start_byte);

  // A kanji verbal noun before an inflected する is a separate search unit.
  // Preserve an earlier lexical noun or adjective boundary so compounds such
  // as temporal expressions remain decomposable before their final verbal noun.
  size_t kanji_length = kanji_end - start_pos;
  const size_t following_verb_start =
      longestNominalVerbContinuativeStart(codepoints, char_types, start_pos, kanji_end, inflection, &dict_manager);
  const std::string final_kanji = normalize::encodeUtf8(codepoints[kanji_end - 1]);
  const bool ends_with_derivational_suffix =
      dict_manager.lookupExact(final_kanji, core::PartOfSpeech::Suffix) != nullptr;
  // The last kanji of the run can itself open a closed humble subsidiary verb
  // (確認 + 致し + ます). Its continuative ends in し, so the run is shaped exactly
  // like an ordinary sahen verbal noun, and reading it as one absorbs the
  // subsidiary's kanji (確認致 + し).
  const size_t humble_subsidiary_start =
      (kanji_end > start_pos + 1 && kanji_end < codepoints.size() &&
       grammar::isHumbleHonorificRenyokei(extractSubstring(codepoints, kanji_end - 1, kanji_end + 1)))
          ? kanji_end - 1
          : kanji_end;
  const bool ends_at_humble_subsidiary = humble_subsidiary_start < kanji_end;
  if (hasSuruContinuation(codepoints, kanji_end) && following_verb_start == kanji_end && !ends_at_humble_subsidiary &&
      !hasDictionaryLexicalPrefix(noun_results, kanji_length) &&
      !crossesModifierBoundaryForSuruNoun(text, byte_offsets, start_pos, kanji_end, dict_manager) &&
      !(start_pos > 0 && normalize::isIterationMark(codepoints[start_pos - 1])) &&
      !containsIterationMark(codepoints, start_pos, kanji_end) && !ends_with_derivational_suffix &&
      !suffixHeadedRunAbsorbsVerifiedGodanStem(codepoints, start_pos, kanji_end, dict_manager)) {
    size_t noun_end_byte = byteOffsetAt(byte_offsets, kanji_end);
    std::string noun_surface(text.substr(start_byte, noun_end_byte - start_byte));
    float noun_cost = getCategoryCost(core::ExtendedPOS::Noun) + scorer.splitOpts().noun_verb_split_bonus +
                      candidate::kSuruVerbalNounContextBonus;
    lattice.addEdge(noun_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(kanji_end),
                    core::PartOfSpeech::Noun, noun_cost, core::LatticeEdge::kIsUnknown, "");
  }

  // Try different noun lengths
  for (size_t noun_len = 1; noun_len < kanji_end - start_pos; ++noun_len) {
    const size_t verb_start = start_pos + noun_len;
    // Once a productive continuative has been verified, no shorter V2 may
    // move the boundary into it (顔見|知り, 総合見|直し). Boundaries before the
    // verified start remain available to other grammatical evidence.
    if (following_verb_start < kanji_end && verb_start > following_verb_start) {
      continue;
    }
    // A contiguous kanji run followed by an inflected する is either a
    // productive verbal noun (提出+し, 頻出+する) or a lexical verb stem
    // (見直し).  In both cases, fabricating a noun+verb boundary inside the
    // kanji run is grammatically unsupported (提+出し, 見+直し).  Preserve
    // the complete run; the ordinary noun/verb candidates decide its POS.
    // The exception is the humble subsidiary above: that boundary is a real
    // morpheme boundary rather than a fabricated one inside a verbal noun.
    if (kanji_end < codepoints.size() && codepoints[kanji_end] == U'し' && verb_start != humble_subsidiary_start &&
        (kanji_length < 3 || verb_start != following_verb_start)) {
      continue;
    }

    size_t verb_start_byte = byteOffsetAt(byte_offsets, verb_start);

    // Check if noun part is in dictionary as NOUN
    // Only consider actual NOUN entries, not ADV/VERB/etc.
    // Skip formal nouns (中, 上, 下, etc.) - they shouldn't split from preceding noun
    bool noun_in_dict = false;
    bool is_formal_noun = false;
    bool noun_surface_is_non_noun_dict = false;
    float noun_cost = 1.0F;

    for (const auto& result : noun_results) {
      if (result.entry != nullptr && result.length == noun_len) {
        if (result.entry->pos == core::PartOfSpeech::Noun) {
          noun_in_dict = true;
          // v0.8: cost from extended_pos
          noun_cost = getCategoryCost(result.entry->extended_pos);
          is_formal_noun = (result.entry->extended_pos == core::ExtendedPOS::NounFormal);
          break;
        }
        // Surface exists in dict but as non-noun (Adverb, Conjunction, Determiner etc.).
        // Don't fabricate a fake NOUN split candidate that would shadow the dict POS.
        if (result.entry->pos == core::PartOfSpeech::Adverb || result.entry->pos == core::PartOfSpeech::Conjunction ||
            result.entry->pos == core::PartOfSpeech::Determiner || result.entry->pos == core::PartOfSpeech::Adjective) {
          noun_surface_is_non_noun_dict = true;
        }
      }
    }

    // Skip N+V split if noun is a formal/bound noun (e.g., 中, 上, 下)
    // These typically attach to preceding nouns, not verbs
    if (is_formal_noun) {
      continue;
    }

    // Skip N+V split if the noun surface is a non-noun dict entry (Adv/Conj/Det/Adj).
    // Generating a fake NOUN here suppresses the correct POS (e.g., 早速/Adverb)
    // because the SPLIT_NV bonus undercuts the dict candidate's category cost.
    if (noun_surface_is_non_noun_dict && !noun_in_dict) {
      continue;
    }

    // Try different hiragana lengths for verb ending
    size_t hiragana_extent = max_hiragana_end - kanji_end;
    size_t max_try_len = std::min(hiragana_extent, kMaxVerbHiraganaLen);

    for (size_t hira_len = 1; hira_len <= max_try_len; ++hira_len) {
      size_t verb_end = kanji_end + hira_len;
      size_t verb_end_byte = byteOffsetAt(byte_offsets, verb_end);

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
        if (dict_manager.lookupExact(cand.base_form, core::PartOfSpeech::Verb) != nullptr) {
          base_in_dict = true;
          break;
        }
      }

      // Generate split candidates if conditions are met
      if ((noun_in_dict && looks_like_verb) || base_in_dict) {
        std::string noun_surface(text.substr(start_byte, verb_start_byte - start_byte));

        // An ideographic iteration mark closes its reduplicated unit.  Do not
        // fabricate a noun+verb split whose noun side crosses that boundary
        // (月々支+払う): the regular 月々 candidate and the verb beginning at
        // 支 already express the grammatical segmentation.  Full repeated
        // compounds remain available through the ordinary same-type path.
        bool extends_past_iteration_mark = false;
        for (size_t index = start_pos; index + 1 < verb_start; ++index) {
          if (normalize::isIterationMark(codepoints[index])) {
            extends_past_iteration_mark = true;
            break;
          }
        }
        if (extends_past_iteration_mark) {
          continue;
        }

        // Skip split if noun + first kanji of verb forms a known compound
        // e.g., 上+手く should not split because 上手 is a dictionary word
        if (verb_start < kanji_end) {
          size_t compound_end_byte = byteOffsetAt(byte_offsets, verb_start + 1);
          std::string compound(text.substr(start_byte, compound_end_byte - start_byte));
          bool compound_in_dict = dict_manager.lookupExact(compound) != nullptr;
          if (compound_in_dict) {
            continue;  // Skip this split, prefer compound word
          }
        }
        // Skip split if last kanji of noun + first kanji of verb forms a known
        // dictionary word. e.g., 作画崩+壊し → skip because 崩壊 is a dict word
        // (prefer 作画崩壊+し). Also check last kanji + full verb part for cases
        // like 大掃除+する → skip because 掃除 is a dict word.
        if (noun_surface.size() >= 6) {  // Noun has at least 2 kanji (6 bytes UTF-8)
          const std::string last_kanji = normalize::encodeUtf8(codepoints[verb_start - 1]);
          // Check last_kanji + verb_part (e.g., 除+する = 掃除する? no, but 除する? no)
          std::string alt_word = normalize::concat(last_kanji, verb_part);
          if (dict_manager.lookupExact(alt_word) != nullptr) {
            SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_NV] skip \"" << noun_surface << "\" + \"" << verb_part
                                                          << "\": alt dict word \"" << alt_word << "\" exists\n");
            goto next_split;
          }
          // Check last_kanji + first_kanji_of_verb (e.g., 崩+壊 = 崩壊)
          // This catches compounds where the verb's kanji belongs to a noun
          if (verb_start < kanji_end) {
            std::string first_verb_kanji = normalize::encodeUtf8(codepoints[verb_start]);
            std::string compound = last_kanji + first_verb_kanji;
            if (dict_manager.lookupExact(compound) != nullptr) {
              SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_NV] skip \"" << noun_surface << "\" + \"" << verb_part
                                                            << "\": compound \"" << compound << "\" is dict word\n");
              goto next_split;
            }
          }
        }

        {
          const auto& opts = scorer.splitOpts();
          float final_noun_cost = noun_cost + opts.noun_verb_split_bonus;

          // Credit the verified-verb bonus only when the noun part is a real
          // dictionary noun or a single kanji. A fabricated multi-kanji noun
          // (noun_in_dict=0) would otherwise become cheaper than genuine
          // dictionary words and absorb characters across word boundaries
          // (やる気丸出し → やる + 気丸 + 出し). Single-kanji nouns are safe
          // because they already carry the single-kanji split penalty below.
          if (base_in_dict && (noun_in_dict || noun_len == 1)) {
            final_noun_cost += opts.verified_verb_bonus;
          }

          if (noun_in_dict && base_in_dict) {
            final_noun_cost -= 0.2F;
          }

          // Penalty for single-kanji noun + verb split
          // E.g., 勘+違い should prefer 勘違い (compound noun)
          // Single-kanji nouns rarely form valid noun+verb compounds
          if (noun_len == 1) {
            final_noun_cost += bigram_cost::kStrong;
          }

          SUZUME_DEBUG_LOG_VERBOSE("[SPLIT_NV] \"" << noun_surface << "\" + \"" << verb_part
                                                   << "\": noun_dict=" << noun_in_dict << " verb_dict=" << base_in_dict
                                                   << " cost=" << final_noun_cost << "\n");

          uint8_t noun_flags = noun_in_dict ? core::LatticeEdge::kFromDictionary : core::LatticeEdge::kIsUnknown;

          lattice.addEdge(noun_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(verb_start),
                          core::PartOfSpeech::Noun, final_noun_cost, noun_flags, "");

          break;
        }  // end alt-dict-word check scope
      next_split:;
      }
    }
  }
}

}  // namespace suzume::analysis
