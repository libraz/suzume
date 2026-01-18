/**
 * @file verb_candidates_hiragana.cpp
 * @brief Hiragana-based verb candidate generation (generateHiraganaVerbCandidates)
 *
 * Handles verb candidate generation for pure hiragana patterns.
 * Split from verb_candidates.cpp for maintainability.
 */

#include "verb_candidates.h"

#include <algorithm>
#include <cmath>

#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"

namespace suzume::analysis {

// Alias for helper functions
namespace vh = verb_helpers;

std::vector<UnknownCandidate> generateHiraganaVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager,
    const VerbCandidateOptions& verb_opts) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Skip if starting character is a particle that is NEVER a verb stem
  // Note: Characters that CAN be verb stems are NOT skipped:
  //   - な→なる/なくす, て→できる, や→やる, か→かける/かえる
  char32_t first_char = codepoints[start_pos];
  if (normalize::isNeverVerbStemAtStart(first_char)) {
    return candidates;
  }

  // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
  // These are commonly mistaken for verbs (これる, それる, etc.)
  // Exception: あれば is the conditional form of ある (verb), not pronoun + particle
  if (start_pos + 1 < codepoints.size()) {
    char32_t second_char = codepoints[start_pos + 1];
    if (normalize::isDemonstrativeStart(first_char, second_char)) {
      // Check if followed by conditional ば - if so, it might be verb conditional form
      // E.g., あれば = ある (verb) + ば, not あれ (pronoun) + ば
      bool is_conditional_form = (start_pos + 2 < codepoints.size() &&
                                  codepoints[start_pos + 2] == U'ば');
      if (!is_conditional_form) {
        return candidates;
      }
    }

    // Skip if starting with 「ない」(auxiliary verb/i-adjective for negation)
    // These should be recognized as AUX by dictionary, not as hiragana verbs.
    // E.g., 「ないんだ」→「ない」+「んだ」, not a single verb「ないむ」
    if (first_char == U'な' && second_char == U'い') {
      return candidates;
    }
  }

  // Find hiragana sequence, breaking at particle boundaries
  // Note: Be careful not to break at characters that are part of verb conjugations:
  //   - か can be part of なかった (negative past) or かった (i-adj past)
  //   - で can be part of んで (te-form for godan) or できる (potential verb)
  //   - も can be part of ても (even if) or もらう (receiving verb)
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() &&
         hiragana_end - start_pos < 12 &&  // Max 12 hiragana for verb + endings
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Don't include particles that appear after the first hiragana character.
    // E.g., for "りにする", stop at "り" to not include "にする".
    if (hiragana_end > start_pos) {
      char32_t curr = codepoints[hiragana_end];

      // Check for particle-like characters (common particles + も, や)
      if (normalize::isNeverVerbStemAfterKanji(curr)) {
        break;  // These are always particles in this context
      }

      // For か, で, も, と: check if they're part of verb conjugation patterns
      // Don't break if they appear in known conjugation contexts
      if (curr == U'か' || curr == U'で' || curr == U'も' || curr == U'と') {
        // Check the preceding character for conjugation patterns
        char32_t prev = codepoints[hiragana_end - 1];

        // か: OK if preceded by な (なかった = negative past)
        //    Also OK if followed by れ (かれ = ichidan stem like つかれる, ふざける)
        //    Also OK if followed by んで/んだ (onbin te/ta-form: つかんで, 歩かんで)
        if (curr == U'か') {
          if (prev == U'な') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by れ (ichidan stem pattern)
          if (hiragana_end + 1 < codepoints.size() &&
              codepoints[hiragana_end + 1] == U'れ') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by んで/んだ (GodanMa/Na/Ba onbin te/ta-form)
          // e.g., つかんで (掴んで), 歩かんで (歩かない colloquial negative te-form)
          if (hiragana_end + 2 < codepoints.size() &&
              codepoints[hiragana_end + 1] == U'ん' &&
              (codepoints[hiragana_end + 2] == U'で' ||
               codepoints[hiragana_end + 2] == U'だ')) {
            ++hiragana_end;
            continue;
          }
        }

        // で: OK if preceded by ん (んで = te-form) or き (できる)
        if (curr == U'で' && (prev == U'ん' || prev == U'き')) {
          ++hiragana_end;
          continue;
        }

        // も: OK if preceded by て (ても = even if)
        if (curr == U'も' && prev == U'て') {
          ++hiragana_end;
          continue;
        }

        // と: OK if preceded by っ (っとく = ておく contraction)
        // やっとく = やって + おく where ておく → とく
        if (curr == U'と' && prev == U'っ') {
          ++hiragana_end;
          continue;
        }

        // Otherwise, treat as particle
        break;
      }
    }
    ++hiragana_end;
  }

  // Need at least 2 hiragana for a verb
  if (hiragana_end <= start_pos + 1) {
    return candidates;
  }

  // Try different lengths, starting from longest
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 1; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Check if this looks like a conjugated verb
    // First try the best match, but also check all candidates for dictionary verbs
    auto all_candidates = inflection.analyze(surface);
    grammar::InflectionCandidate best;
    bool is_dictionary_verb = false;

    // Look through all candidates to find ones whose base form is in the dictionary
    // Collect all matches and select the best one based on:
    // 1. Higher confidence
    // 2. GodanWa > GodanRa/GodanTa when tied (う verbs are much more common for hiragana)
    // This helps with cases like しまった where しまう (GodanWa) should beat しまる (GodanRa)
    if (dict_manager != nullptr) {
      std::vector<grammar::InflectionCandidate> dict_matches;

      for (const auto& cand : all_candidates) {
        if (cand.verb_type == grammar::VerbType::IAdjective ||
            cand.base_form.empty()) {
          continue;
        }
        if (vh::isVerbInDictionary(dict_manager, cand.base_form)) {
          // Found a dictionary verb - collect this candidate
          dict_matches.push_back(cand);
        }
      }

      // Select the best dictionary match
      if (!dict_matches.empty()) {
        is_dictionary_verb = true;
        best = dict_matches[0];

        for (size_t i = 1; i < dict_matches.size(); ++i) {
          const auto& cand = dict_matches[i];
          // Higher confidence wins
          if (cand.confidence > best.confidence + 0.01F) {
            best = cand;
          } else if (std::abs(cand.confidence - best.confidence) <= 0.01F) {
            // When confidence is tied (within 0.01), prefer GodanWa over GodanRa/GodanTa
            // Rationale: For pure hiragana stems, う verbs (しまう, あらう, かう) are
            // much more common than る/つ verbs with the same stem pattern.
            // GodanRa: rare for pure hiragana (most are kanji: 走る, 帰る)
            // GodanTa: rare (持つ, 勝つ, etc. - usually with kanji)
            // GodanWa: very common in hiragana (しまう, あらう, まよう, etc.)
            if (cand.verb_type == grammar::VerbType::GodanWa &&
                (best.verb_type == grammar::VerbType::GodanRa ||
                 best.verb_type == grammar::VerbType::GodanTa)) {
              best = cand;
            }
          }
        }
      }
    }

    // If no dictionary match, select best candidate with GodanWa preference
    // When confidence is tied, GodanWa should beat GodanRa/GodanTa because
    // う verbs (あらう, かう, まよう) are much more common than る/つ verbs
    // for pure hiragana stems
    if (!is_dictionary_verb && !all_candidates.empty()) {
      best = all_candidates[0];
      for (size_t i = 1; i < all_candidates.size(); ++i) {
        const auto& cand = all_candidates[i];
        if (cand.verb_type == grammar::VerbType::IAdjective) {
          continue;
        }
        // Higher confidence wins
        if (cand.confidence > best.confidence + 0.01F) {
          best = cand;
        } else if (std::abs(cand.confidence - best.confidence) <= 0.01F) {
          // When confidence is tied (within 0.01), prefer GodanWa over GodanRa/GodanTa
          if (cand.verb_type == grammar::VerbType::GodanWa &&
              (best.verb_type == grammar::VerbType::GodanRa ||
               best.verb_type == grammar::VerbType::GodanTa)) {
            best = cand;
          }
        }
      }
    }

    // Filter out 2-char hiragana that don't end with valid verb endings
    // Valid endings: る (dictionary form), て/で (te-form), た/だ (past)
    // Also: れ (Ichidan renyokei/meireikei like くれ from くれる)
    // This prevents false positives like まじ, ため from being recognized as verbs
    size_t pre_filter_len = end_pos - start_pos;
    if (pre_filter_len == 2 && surface.size() >= core::kJapaneseCharBytes) {
      // Use string_view directly into surface to avoid dangling reference
      // (surface.substr() returns a temporary std::string)
      std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes,
                                  core::kJapaneseCharBytes);
      if (last_char != "る" && last_char != "て" && last_char != "で" &&
          last_char != "た" && last_char != "だ" && last_char != "れ") {
        continue;  // Skip 2-char hiragana not ending with valid verb suffix
      }
    }

    // Filter out i-adjective conjugation suffixes (standalone, not verb candidates)
    // See scorer_constants.h for documentation on these patterns.
    if (surface == scorer::kIAdjPastKatta || surface == scorer::kIAdjPastKattara ||
        surface == scorer::kIAdjTeKute || surface == scorer::kIAdjNegKunai ||
        surface == scorer::kIAdjCondKereba || surface == scorer::kIAdjStemKa ||
        surface == scorer::kIAdjNegStemKuna || surface == scorer::kIAdjCondStemKere) {
      continue;  // Skip i-adjective conjugation patterns
    }

    // Note: Common adverbs/onomatopoeia (ぴったり, はっきり, etc.) are filtered
    // by the dictionary lookup below - they are registered as Adverb in L1 dictionary.

    // Filter out old kana forms (ゐ=wi, ゑ=we) that look like verbs
    // ゐる is old kana for いる (auxiliary), not a standalone verb
    if (surface.size() >= core::kJapaneseCharBytes) {
      std::string_view first_char_view(surface.data(), core::kJapaneseCharBytes);
      if (first_char_view == "ゐ" || first_char_view == "ゑ") {
        continue;  // Skip old kana patterns
      }
    }

    // Filter out words that exist in dictionary as non-verb entries
    // e.g., あなた (pronoun), わたし (pronoun) should not be verb candidates
    if (vh::hasNonVerbDictionaryEntry(dict_manager, surface)) {
      continue;  // Skip - dictionary has non-verb entry for this surface
    }

    // Check for 3-4 char hiragana verb ending with た/だ (past form) BEFORE threshold check
    // e.g., つかれた (疲れた), ねむった (眠った), おきた (起きた)
    // These need lower threshold because ichidan_pure_hiragana_stem penalty reduces confidence
    size_t pre_check_len = end_pos - start_pos;
    bool looks_like_past_form = false;
    bool looks_like_te_form = false;
    if ((pre_check_len == 3 || pre_check_len == 4) &&
        surface.size() >= core::kJapaneseCharBytes) {
      std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes,
                                  core::kJapaneseCharBytes);
      if (last_char == "た" || last_char == "だ") {
        looks_like_past_form = true;
      } else if (last_char == "て" || last_char == "で") {
        // Te-form verbs (あらって, しまって, かって) need lower threshold too
        looks_like_te_form = true;
      }
    }

    // Check for ichidan dictionary form (e-row stem + る)
    // e.g., たべる (食べる), しらべる (調べる), つかれる (疲れる)
    // These need lower threshold because ichidan_pure_hiragana_stem penalty reduces confidence
    // Note: Check pattern structure directly, not verb_type, because when multiple
    // candidates have the same confidence, the godan candidate may be returned first
    // Exception: Exclude てる pattern (て + る) which is the ている contraction
    // e.g., してる should be する+ている, not しる (ichidan)
    bool looks_like_ichidan_dict_form = false;
    if (pre_check_len >= 3 && surface.size() >= core::kTwoJapaneseCharBytes) {
      std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes,
                                  core::kJapaneseCharBytes);
      if (last_char == "る" && end_pos >= 2) {
        // Check if second-to-last char is e-row or i-row hiragana (ichidan stem ending)
        // E-row: 食べる, 見える, 調べる
        // I-row: 感じる, 信じる (kanji + i-row + る pattern)
        char32_t stem_end = codepoints[end_pos - 2];
        if (grammar::isERowCodepoint(stem_end) || grammar::isIRowCodepoint(stem_end)) {
          // Exclude てる pattern (ている contraction) - this should be suru/godan + ている
          // not ichidan dictionary form
          bool is_te_iru_contraction = (stem_end == U'て' || stem_end == U'で');
          if (!is_te_iru_contraction) {
            // Find ichidan candidate to use for verb type and base form
            // For dictionary forms (e-row stem + る), prefer longer valid stems
            // Valid: つかれる (e-row ending), Invalid: つかれるる (るる pattern)
            grammar::InflectionCandidate best_ichidan;
            bool found_ichidan = false;
            for (const auto& cand : all_candidates) {
              if (cand.verb_type == grammar::VerbType::Ichidan &&
                  cand.confidence >= verb_opts.confidence_ichidan_dict) {
                // Skip invalid るる pattern (e.g., つかれるる)
                if (cand.base_form.size() >= 2 * core::kJapaneseCharBytes) {
                  std::string_view ending(
                      cand.base_form.data() + cand.base_form.size() - 2 * core::kJapaneseCharBytes,
                      2 * core::kJapaneseCharBytes);
                  if (ending == "るる") {
                    continue;  // Skip invalid pattern
                  }
                }
                if (!found_ichidan) {
                  best_ichidan = cand;
                  found_ichidan = true;
                } else if (cand.base_form.size() > best_ichidan.base_form.size()) {
                  // Prefer longer base form (e.g., つかれる > つかる)
                  best_ichidan = cand;
                }
              }
            }
            if (found_ichidan) {
              looks_like_ichidan_dict_form = true;
              // Use ichidan candidate as best if pattern matches
              if (best.verb_type != grammar::VerbType::Ichidan) {
                best = best_ichidan;
              } else if (best_ichidan.base_form.size() > best.base_form.size()) {
                // Even if already Ichidan, prefer longer base form
                best = best_ichidan;
              }
            }
          }
        }
      }
    }

    // Only accept verb types (not IAdjective) with sufficient confidence
    // Lower threshold for dictionary-verified verbs, past/te forms, and ichidan dict forms
    // Ichidan dict forms get very low threshold (0.28) because pure hiragana stems
    // with 3+ chars get multiple penalties (stem_long + ichidan_pure_hiragana_stem)
    // When both is_dictionary_verb AND (past/te form) apply, use the lower threshold
    // This handles cases like つかんで (掴んで) where confidence is ~0.3
    float conf_threshold;
    if (is_dictionary_verb && (looks_like_past_form || looks_like_te_form)) {
      // Dictionary verb in past/te form: use lower of the two thresholds
      conf_threshold = std::min(verb_opts.confidence_dict_verb, verb_opts.confidence_past_te);
    } else if (is_dictionary_verb) {
      conf_threshold = verb_opts.confidence_dict_verb;
    } else if (looks_like_past_form || looks_like_te_form) {
      conf_threshold = verb_opts.confidence_past_te;
    } else if (looks_like_ichidan_dict_form) {
      conf_threshold = verb_opts.confidence_ichidan_dict;
    } else {
      conf_threshold = verb_opts.confidence_standard;
    }
    if (best.confidence > conf_threshold &&
        best.verb_type != grammar::VerbType::IAdjective) {
      // Lower cost for higher confidence matches
      float base_cost = verb_opts.base_cost_high + (1.0F - best.confidence) * verb_opts.confidence_cost_scale;

      // Give significant bonus for dictionary-verified hiragana verbs
      // This helps them beat the particle+adj+particle split path
      // Only apply to longer forms (5+ chars) to avoid boosting short forms like
      // "あった" (ある) which can interfere with copula recognition (であった)
      // Exception: Conditional forms (ending with ば) are unambiguous and should
      // get the bonus even if short (e.g., あれば = ある conditional)
      size_t candidate_len = end_pos - start_pos;
      bool is_conditional = (surface.size() >= core::kJapaneseCharBytes &&
                             surface.substr(surface.size() - core::kJapaneseCharBytes) == "ば");
      // Check for っとく pattern (ておく contraction: やっとく, 見っとく)
      // This is a common colloquial pattern that should get bonus treatment
      bool is_teoku_contraction = (surface.size() >= core::kThreeJapaneseCharBytes &&
                                   surface.substr(surface.size() - core::kThreeJapaneseCharBytes) == "っとく");
      // Check for short te/de-form (e.g., ねて, でて, みて)
      // These are 2-char hiragana verbs that need a bonus to beat particle splits
      bool is_short_te_form = false;
      if (candidate_len == 2 && best.confidence >= verb_opts.confidence_high) {
        // Check last char in bytes (UTF-8)
        // て = E3 81 A6, で = E3 81 A7
        if (surface.size() >= 3) {
          char c1 = surface[surface.size() - 3];
          char c2 = surface[surface.size() - 2];
          char c3 = surface[surface.size() - 1];
          if (c1 == '\xE3' && c2 == '\x81' && (c3 == '\xA6' || c3 == '\xA7')) {
            is_short_te_form = true;
          }
        }
      }

      // Check for 3-4 char hiragana verb ending with た/だ (past form)
      // e.g., つかれた (疲れた), ねむった (眠った), おきた (起きた)
      // These medium-length verbs need a bonus to beat particle splits like つ+か+れた
      // Note: Lower confidence threshold (0.25) because ichidan_pure_hiragana_stem penalty
      // reduces confidence significantly for pure hiragana verbs
      bool is_medium_past_form = false;
      if ((candidate_len == 3 || candidate_len == 4) && best.confidence >= verb_opts.confidence_past_te) {
        std::string_view last_char(surface.data() + surface.size() - core::kJapaneseCharBytes,
                                    core::kJapaneseCharBytes);
        if (last_char == "た" || last_char == "だ") {
          is_medium_past_form = true;
        }
      }

      if (is_dictionary_verb &&
          (candidate_len >= 5 || is_conditional || is_teoku_contraction)) {
        base_cost = verb_opts.base_cost_verified + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
      } else if (is_short_te_form) {
        // Short te-form with high confidence: give strong bonus to beat particle splits
        // e.g., ねて (conf=0.79) should beat ね(PARTICLE) + て(PARTICLE)
        // Particle path total cost can be as low as 0.002 due to dictionary bonuses,
        // so we need negative cost to compete. After adding POS prior (0.2 for verb),
        // the total needs to be below 0.002, so base needs to be below -0.2.
        //
        // When the first char is a common particle (で, に, etc.), these particles
        // have very low cost (e.g., で: -0.4), making particle+て path even cheaper
        // (total around -0.5). Need extra strong bonus for these cases.
        bool starts_with_common_particle =
            (first_char == U'で' || first_char == U'に' || first_char == U'が' ||
             first_char == U'を' || first_char == U'は' || first_char == U'の' ||
             first_char == U'へ');
        if (starts_with_common_particle) {
          // Extra strong bonus: need to beat particle paths around -0.5
          base_cost = verb_opts.bonus_long_verified + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_small;
        } else {
          base_cost = verb_opts.bonus_long_dict + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_small;
        }
      } else if (is_medium_past_form) {
        // Medium-length past form verbs (3-4 chars ending with た/だ)
        // e.g., つかれた (conf=0.43) should beat つ+か+れた split
        // Give bonus to compete with particle splits
        base_cost = verb_opts.confidence_cost_scale_medium + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
      } else if (looks_like_ichidan_dict_form) {
        // Ichidan dictionary form (e-row stem + る)
        // e.g., たべる (conf=0.39), しらべる, つかれる
        // These are highly likely to be real verbs, give modest bonus
        // Starting with particle-like chars (た, etc.) needs stronger bonus
        bool starts_with_aux_like_char =
            (first_char == U'た' || first_char == U'で' || first_char == U'に');
        if (starts_with_aux_like_char) {
          // Extra bonus: need to beat た(AUX) + べる(AUX) split
          base_cost = verb_opts.base_cost_verified + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
        } else {
          base_cost = verb_opts.base_cost_low + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
        }
      } else if (candidate_len >= 7 && best.confidence >= verb_opts.confidence_very_high) {
        // For long hiragana verb forms (7+ chars) with high confidence,
        // give a bonus even without dictionary verification.
        // This helps forms like かけられなくなった (9 chars) beat the
        // particle+verb split path (か + けられなくなった).
        // The length requirement (7+ chars) helps avoid false positives.
        //
        // When the verb starts with a character that's commonly mistaken for
        // a particle (か, は, が, etc.), give an extra strong bonus because
        // the particle split path is very likely to compete.
        bool starts_with_particle_char =
            (first_char == U'か' || first_char == U'は' || first_char == U'が' ||
             first_char == U'を' || first_char == U'に' || first_char == U'で' ||
             first_char == U'と' || first_char == U'も' || first_char == U'へ');
        if (starts_with_particle_char) {
          // Extra strong bonus for forms starting with particle-like char
          // e.g., かけられなくなった should strongly beat か + けられなくなった
          base_cost = verb_opts.base_cost_long_verified + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_small;
        } else {
          base_cost = verb_opts.confidence_cost_scale_medium + (1.0F - best.confidence) * verb_opts.confidence_cost_scale_medium;
        }
      }

      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Verb;
      candidate.cost = base_cost;
      candidate.has_suffix = false;
      // Set lemma from inflection analysis for pure hiragana verbs
      // This is essential for P4 (ひらがな動詞活用展開) to work without dictionary
      // The lemmatizer can't derive lemma accurately for unknown verbs
      candidate.lemma = best.base_form;
      candidate.conj_type = grammar::verbTypeToConjType(best.verb_type);
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::VerbHiragana;
      candidate.confidence = best.confidence;
      candidate.pattern = grammar::verbTypeToString(best.verb_type);
#endif
      candidates.push_back(candidate);
    }
  }

  // Generate Godan mizenkei stem candidates for hiragana passive patterns
  // E.g., いわれる → いわ (mizenkei of いう) + れる (passive AUX)
  // This is similar to the kanji+hiragana path but for pure hiragana verbs
  // Key insight: A-row hiragana (わ,か,さ,た,な,ま,ら,が,etc.) + れ pattern
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 2; --end_pos) {
    // Check if position end_pos-1 is A-row hiragana (mizenkei ending)
    // and position end_pos is れ (passive pattern start)
    size_t mizenkei_end = end_pos - 1;  // Position after A-row char
    if (mizenkei_end <= start_pos) continue;

    char32_t a_row_char = codepoints[mizenkei_end - 1];  // The A-row character
    char32_t next_char = codepoints[mizenkei_end];       // Should be れ

    // Check for A-row followed by passive pattern (れる, れた, れて, etc.)
    if (!grammar::isARowCodepoint(a_row_char) || next_char != U'れ') {
      continue;
    }

    // Check for passive patterns after れ
    // All passive patterns split at mizenkei (いわ + れる/れ) for MeCab compatibility
    bool is_passive_pattern = false;
    if (mizenkei_end + 1 < codepoints.size()) {
      char32_t after_re = codepoints[mizenkei_end + 1];
      // れる, れた, れて
      if (after_re == U'る' || after_re == U'た' || after_re == U'て') {
        is_passive_pattern = true;
      }
      // れな (れない, れなかった)
      else if (after_re == U'な' && mizenkei_end + 2 < codepoints.size() &&
               codepoints[mizenkei_end + 2] == U'い') {
        is_passive_pattern = true;
      }
      // れま (れます, れました, れません, れませんでした)
      else if (after_re == U'ま') {
        is_passive_pattern = true;
      }
    }

    if (!is_passive_pattern) {
      continue;
    }

    // Derive VerbType from the A-row ending (e.g., わ → GodanWa)
    grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(a_row_char);
    if (verb_type == grammar::VerbType::Unknown) {
      continue;
    }

    // Get base suffix (e.g., わ → う for GodanWa)
    std::string_view base_suffix = grammar::godanBaseSuffixFromARow(a_row_char);
    if (base_suffix.empty()) {
      continue;
    }

    // Construct base form and mizenkei surface
    // E.g., for いわれる: mizenkei = いわ, stem = い, base_suffix = う → base_form = いう
    std::string mizenkei_surface = extractSubstring(codepoints, start_pos, mizenkei_end);
    std::string stem = extractSubstring(codepoints, start_pos, mizenkei_end - 1);
    std::string base_form = stem + std::string(base_suffix);

    // Check if mizenkei surface exists in dictionary as a verb
    // This handles cases like いわ which is registered with lemma いう
    // OR check if base form exists (for kanji compounds like 言わ)
    bool is_valid_verb = vh::isVerbInDictionary(dict_manager, mizenkei_surface);
    if (!is_valid_verb) {
      // Fallback: check base form with verb type matching for onbin types
      if (verb_type == grammar::VerbType::GodanWa ||
          verb_type == grammar::VerbType::GodanKa ||
          verb_type == grammar::VerbType::GodanTa ||
          verb_type == grammar::VerbType::GodanRa) {
        is_valid_verb = vh::isVerbInDictionaryWithType(dict_manager, base_form, verb_type);
      } else {
        is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
      }
    }

    if (!is_valid_verb) {
      continue;
    }

    // Get lemma from dictionary entry if mizenkei is registered
    // Otherwise use constructed base form
    std::string lemma = base_form;
    if (dict_manager != nullptr) {
      auto results = dict_manager->lookup(mizenkei_surface, 0);
      for (const auto& result : results) {
        if (result.entry != nullptr && result.entry->surface == mizenkei_surface &&
            result.entry->pos == core::PartOfSpeech::Verb && !result.entry->lemma.empty()) {
          lemma = result.entry->lemma;
          break;
        }
      }
    }

    // Always split at mizenkei (いわ + れる/れ) for MeCab compatibility
    // MeCab splits: いわれません → いわ + れ + ませ + ん (4 tokens)
    // Previous strategy of splitting at passive renyokei (いわれ + ません) was incorrect
    size_t split_end = mizenkei_end;
    std::string surface = extractSubstring(codepoints, start_pos, split_end);
    const char* pattern_name = "passive_mizenkei";

    UnknownCandidate candidate;
    candidate.surface = surface;
    candidate.start = start_pos;
    candidate.end = split_end;
    candidate.pos = core::PartOfSpeech::Verb;
    candidate.cost = -0.5F;  // Negative cost to beat OTHER + AUX split
    candidate.has_suffix = true;  // Skip exceeds_dict_length penalty
    candidate.lemma = lemma;  // Use lemma from dictionary if available
    candidate.conj_type = grammar::verbTypeToConjType(verb_type);
    SUZUME_DEBUG_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                          << " hiragana_" << pattern_name << " lemma=" << lemma
                          << " cost=" << candidate.cost << "\n";
    }
#ifdef SUZUME_DEBUG_INFO
    candidate.origin = CandidateOrigin::VerbHiragana;
    candidate.confidence = 0.9F;  // High confidence for dictionary-verified
    candidate.pattern = std::string("hiragana_") + pattern_name;
#endif
    candidates.push_back(candidate);
    break;  // Only generate one passive candidate per length
  }

  // Generate Godan onbin stem candidates for contraction auxiliary patterns
  // E.g., やっとく → やっ (onbin of やる) + とく (ておく contraction)
  //       読んでる → 読ん (onbin of 読む) + でる (ている contraction)
  // Key patterns:
  // - っ + と/ち: GodanRa/GodanTa/GodanWa verbs (やっとく, やっちゃう)
  // - ん + ど/じ/で: GodanMa/GodanBa/GodanNa verbs (読んどく, 読んじゃう, 読んでる)
  for (size_t onbin_pos = start_pos + 1; onbin_pos < hiragana_end; ++onbin_pos) {
    char32_t onbin_char = codepoints[onbin_pos];

    // Check for sokuonbin (っ) or hatsuonbin (ん)
    bool is_sokuonbin = (onbin_char == U'っ');
    bool is_hatsuonbin = (onbin_char == U'ん');
    if (!is_sokuonbin && !is_hatsuonbin) {
      continue;
    }

    // Check if followed by contraction auxiliary starter
    if (onbin_pos + 1 >= hiragana_end) {
      continue;
    }
    char32_t next_char = codepoints[onbin_pos + 1];

    bool is_contraction_pattern = false;
    if (is_sokuonbin) {
      // っ + と (とく/といた/といて) or ち (ちゃう/ちゃった/ちゃって)
      is_contraction_pattern = (next_char == U'と' || next_char == U'ち');
    } else {
      // ん + ど (どく/どいた/どいて) or じ (じゃう/じゃった/じゃって) or で (でる/でた/でて)
      is_contraction_pattern = (next_char == U'ど' || next_char == U'じ' || next_char == U'で');
    }

    if (!is_contraction_pattern) {
      continue;
    }

    // Get the stem (part before onbin character)
    std::string stem = extractSubstring(codepoints, start_pos, onbin_pos);
    if (stem.empty()) {
      continue;
    }

    // Try different verb types based on onbin type
    std::vector<std::pair<grammar::VerbType, std::string_view>> candidates_to_try;
    if (is_sokuonbin) {
      // っ-onbin: GodanRa, GodanTa, GodanWa
      candidates_to_try = {
        {grammar::VerbType::GodanRa, "る"},
        {grammar::VerbType::GodanWa, "う"},
        {grammar::VerbType::GodanTa, "つ"},
      };
    } else {
      // ん-onbin: GodanMa, GodanBa, GodanNa
      candidates_to_try = {
        {grammar::VerbType::GodanMa, "む"},
        {grammar::VerbType::GodanBa, "ぶ"},
        {grammar::VerbType::GodanNa, "ぬ"},
      };
    }

    // Try each verb type and check dictionary
    for (const auto& [verb_type, base_suffix] : candidates_to_try) {
      std::string base_form = stem + std::string(base_suffix);

      // Check if base form exists in dictionary as this verb type
      bool is_valid_verb = vh::isVerbInDictionaryWithType(dict_manager, base_form, verb_type);
      if (!is_valid_verb) {
        continue;
      }

      // Found a valid verb - generate onbin stem candidate
      std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_pos + 1);

      UnknownCandidate candidate;
      candidate.surface = onbin_surface;
      candidate.start = start_pos;
      candidate.end = onbin_pos + 1;
      candidate.pos = core::PartOfSpeech::Verb;
      candidate.cost = -0.5F;  // Negative cost to beat unsplit forms
      candidate.has_suffix = true;  // Skip exceeds_dict_length penalty
      candidate.lemma = base_form;
      candidate.conj_type = grammar::verbTypeToConjType(verb_type);
      SUZUME_DEBUG_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                            << " hiragana_onbin_contraction lemma=" << base_form
                            << " cost=" << candidate.cost << "\n";
      }
#ifdef SUZUME_DEBUG_INFO
      candidate.origin = CandidateOrigin::VerbHiragana;
      candidate.confidence = 0.9F;
      candidate.pattern = is_sokuonbin ? "hiragana_sokuonbin" : "hiragana_hatsuonbin";
#endif
      candidates.push_back(candidate);
      break;  // Found valid candidate for this position
    }
  }

  // Generate 1-char ichidan renyokei stem candidates
  // E.g., ねて → ね (renyokei of ねる) + て (particle)
  // MeCab splits: ねて → ね(動詞,一段,連用形) + て(助詞,接続助詞)
  // This handles pure hiragana ichidan verbs that need te/ta form splitting
  // Pattern: e-row hiragana followed by て or た
  // IMPORTANT: Only generate if the base form (stem + る) is a known verb in dictionary
  // to avoid false positives like めて → め + て (め is not a verb)
  if (start_pos < codepoints.size()) {
    char32_t first_char = codepoints[start_pos];
    // Check for e-row hiragana (ichidan renyokei ending)
    // Exclude て and で which are more commonly particles
    if (grammar::isERowCodepoint(first_char) && first_char != U'て' && first_char != U'で') {
      // Check if followed by te/ta particle
      if (start_pos + 1 < codepoints.size()) {
        char32_t next_char = codepoints[start_pos + 1];
        if (next_char == U'て' || next_char == U'た') {
          // Construct base form (stem + る)
          std::string stem_surface = extractSubstring(codepoints, start_pos, start_pos + 1);
          std::string base_form = stem_surface + "る";

          // Only generate if base form is a known verb in dictionary
          // This prevents false positives like め+て, け+て
          if (vh::isVerbInDictionary(dict_manager, base_form)) {
            // Generate candidate for the 1-char stem
            UnknownCandidate candidate;
            candidate.surface = stem_surface;
            candidate.start = start_pos;
            candidate.end = start_pos + 1;
            candidate.pos = core::PartOfSpeech::Verb;
            // Strong negative cost to beat particle split
            // Particle path can be as low as -0.2, so we need lower
            candidate.cost = -0.5F;
            candidate.has_suffix = true;  // Skip exceeds_dict_length penalty
            candidate.lemma = base_form;
            candidate.conj_type = dictionary::ConjugationType::Ichidan;
            SUZUME_DEBUG_BLOCK {
              SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem_surface
                                  << " hiragana_ichidan_renyokei_1char lemma=" << base_form
                                  << " cost=" << candidate.cost << "\n";
            }
#ifdef SUZUME_DEBUG_INFO
            candidate.origin = CandidateOrigin::VerbHiragana;
            candidate.confidence = 0.8F;  // High confidence for this pattern
            candidate.pattern = "hiragana_ichidan_renyokei_1char";
#endif
            candidates.push_back(candidate);
          }
        }
      }
    }
  }

  // Generate 2+ char ichidan renyokei stem candidates
  // E.g., つけて → つけ (renyokei of つける) + て (particle)
  //       たべて → たべ (renyokei of たべる) + て (particle)
  //       あけて → あけ (renyokei of あける) + て (particle)
  // MeCab splits: つけて → つけ(動詞,一段,連用形) + て(助詞,接続助詞)
  // Pattern: 2+ char sequence ending with e-row hiragana followed by て or た
  // Uses inflection analysis confidence to validate (dictionary lookup as bonus)
  for (size_t end_pos = start_pos + 2; end_pos < hiragana_end; ++end_pos) {
    // Check if position end_pos-1 is e-row hiragana (ichidan renyokei ending)
    char32_t stem_end_char = codepoints[end_pos - 1];
    if (!grammar::isERowCodepoint(stem_end_char)) {
      continue;
    }

    // Exclude て and で which are more commonly particles
    if (stem_end_char == U'て' || stem_end_char == U'で') {
      continue;
    }

    // Check if followed by te/ta particle
    if (end_pos >= codepoints.size()) {
      continue;
    }
    char32_t next_char = codepoints[end_pos];
    if (next_char != U'て' && next_char != U'た') {
      continue;
    }

    // Construct stem and base form
    std::string stem_surface = extractSubstring(codepoints, start_pos, end_pos);
    std::string base_form = stem_surface + "る";

    // Use inflection analysis to validate - check if stem is recognized as ichidan
    auto stem_analysis = inflection.analyze(stem_surface);
    bool found_ichidan = false;
    float ichidan_confidence = 0.0F;
    for (const auto& cand : stem_analysis) {
      if (cand.verb_type == grammar::VerbType::Ichidan &&
          cand.base_form == base_form) {
        found_ichidan = true;
        ichidan_confidence = cand.confidence;
        break;
      }
    }

    // Skip if not recognized as ichidan stem by inflection analysis
    // Threshold 0.3 catches most valid cases while filtering noise
    if (!found_ichidan || ichidan_confidence < 0.3F) {
      continue;
    }

    // Check if base form is in dictionary (gives confidence boost)
    bool is_dict_verb = vh::isVerbInDictionary(dict_manager, base_form);

    // Generate candidate for the ichidan stem
    UnknownCandidate candidate;
    candidate.surface = stem_surface;
    candidate.start = start_pos;
    candidate.end = end_pos;
    candidate.pos = core::PartOfSpeech::Verb;
    // Strong negative cost to beat NOUN + て(VERB from てる) split
    // Dictionary-verified verbs get stronger bonus
    candidate.cost = is_dict_verb ? -0.8F : -0.6F;
    candidate.has_suffix = true;  // Skip exceeds_dict_length penalty
    candidate.lemma = base_form;
    candidate.conj_type = dictionary::ConjugationType::Ichidan;
    SUZUME_DEBUG_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem_surface
                          << " hiragana_ichidan_renyokei lemma=" << base_form
                          << " conf=" << ichidan_confidence
                          << (is_dict_verb ? " [dict]" : "")
                          << " cost=" << candidate.cost << "\n";
    }
#ifdef SUZUME_DEBUG_INFO
    candidate.origin = CandidateOrigin::VerbHiragana;
    candidate.confidence = ichidan_confidence;
    candidate.pattern = "hiragana_ichidan_renyokei";
#endif
    candidates.push_back(candidate);
  }

  // Add emphatic variants (いくっ, するっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints);

  // Sort by cost
  vh::sortCandidatesByCost(candidates);

  return candidates;
}

}  // namespace suzume::analysis
