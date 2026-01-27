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
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_BLACKLIST] pos=" << start_pos
        << " char=U+" << std::hex << static_cast<uint32_t>(first_char) << std::dec
        << " blocked (isNeverVerbStemAtStart)\n");
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
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos
            << " demonstrative_pronoun (これ/それ/あれ/どれ pattern)\n");
        return candidates;
      }
    }

    // Skip if starting with 「ない」(auxiliary verb/i-adjective for negation)
    // These should be recognized as AUX by dictionary, not as hiragana verbs.
    // E.g., 「ないんだ」→「ない」+「んだ」, not a single verb「ないむ」
    if (first_char == U'な' && second_char == U'い') {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos
          << " nai_pattern (ない is auxiliary/adjective)\n");
      return candidates;
    }

    // Skip if starting with 「く」+ な行 (くな, くに, くぬ, くね, くの)
    // These are i-adjective ku-form + なる/ない patterns, not verbs
    // E.g., 「たくなる」→「たく」+「なる」, not a verb「くなる」
    //       「くなってきた」→「く」+「なっ」+「て」+...
    // Note: くる (来る) is a valid verb but has kanji and is handled by dictionary
    if (first_char == U'く') {
      // く + な行 hiragana = i-adjective pattern
      if (second_char == U'な' || second_char == U'に' || second_char == U'ぬ' ||
          second_char == U'ね' || second_char == U'の') {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos
            << " ku_naru_pattern (i-adjective ku-form + なる/ない)\n");
        return candidates;
      }
    }

    // Skip if starting with 「であり」(copula de + aru renyokei)
    // These should be split as で + あり for MeCab compatibility
    // E.g., 「であります」→「で」+「あり」+「ます」, not a single verb「でありる」
    if (first_char == U'で' && second_char == U'あ') {
      char32_t third_char = (start_pos + 2 < codepoints.size()) ? codepoints[start_pos + 2] : 0;
      if (third_char == U'り' || third_char == U'れ' || third_char == U'る' || third_char == U'ろ') {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos
            << " deari_pattern (copula de + aru conjugation)\n");
        return candidates;
      }
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
        SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] pos=" << hiragana_end
            << " char=U+" << std::hex << static_cast<uint32_t>(curr) << std::dec
            << " action=break (isNeverVerbStemAfterKanji)\n");
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
        //    Also OK if followed by A-row + ん (mizenkei + contracted negative: わからん)
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
          // Check if followed by A-row + ん (mizenkei + contracted negative)
          // e.g., わからん = わから (mizenkei of わかる) + ん
          if (hiragana_end + 2 < codepoints.size() &&
              grammar::isARowCodepoint(codepoints[hiragana_end + 1]) &&
              codepoints[hiragana_end + 2] == U'ん') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by ら + な (godan-ra mizenkei + negative auxiliary)
          // e.g., わからない = わから (mizenkei of わかる) + ない
          if (hiragana_end + 2 < codepoints.size() &&
              codepoints[hiragana_end + 1] == U'ら' &&
              codepoints[hiragana_end + 2] == U'な') {
            ++hiragana_end;
            continue;
          }
          // Check if followed by ない (godan-ka mizenkei + negative auxiliary)
          // e.g., いかない = いか (mizenkei of いく) + ない
          if (hiragana_end + 1 < codepoints.size() &&
              codepoints[hiragana_end + 1] == U'な') {
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
        SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] pos=" << hiragana_end
            << " char=U+" << std::hex << static_cast<uint32_t>(curr) << std::dec
            << " action=break (unrecognized_particle_context)\n");
        break;
      }
    }
    ++hiragana_end;
  }

  // Log final hiragana sequence bounds
  SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] final: start=" << start_pos
      << " end=" << hiragana_end << " len=" << (hiragana_end - start_pos) << "\n");

  // Need at least 2 hiragana for a verb
  if (hiragana_end <= start_pos + 1) {
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] pos=" << start_pos
        << " too_short (need >=2 hiragana, got " << (hiragana_end - start_pos) << ")\n");
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

    // Filter out verb stems that would form compound particles with て/で
    // e.g., によっ + て = によって (particle), とし + て = として (particle)
    // These compound particles exist as dictionary entries and should not be
    // split into spurious verb + て patterns
    {
      std::string_view last_char = utf8::lastChar(surface);
      if (utf8::equalsAny(last_char, {"っ", "し", "つ", "い"})) {
        std::string te_form = std::string(surface) + "て";
        std::string de_form = std::string(surface) + "で";
        if (vh::hasParticleDictionaryEntry(dict_manager, te_form) ||
            vh::hasParticleDictionaryEntry(dict_manager, de_form)) {
          continue;  // Skip - would split a compound particle
        }
      }
    }

    // Filter out te-form compound verb patterns that should be split
    // e.g., なっております → なっ+て+おり+ます, してます → し+て+ます
    //       してください → し+て+ください
    // These contain て+auxiliary patterns that should be analyzed separately
    // Only skip for longer forms (5+ chars) to avoid blocking short verbs
    if (end_pos - start_pos >= 5) {
      // Check for ており/ていま/てい/てお/てくださ patterns (te-form + auxiliary)
      if (surface.find("ており") != std::string::npos ||
          surface.find("ていま") != std::string::npos ||
          surface.find("ている") != std::string::npos ||
          surface.find("ていた") != std::string::npos ||
          surface.find("てくださ") != std::string::npos ||
          surface.find("てお") != std::string::npos) {
        SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface
            << "\" skip te_compound_pattern\n");
        continue;  // Skip - let te-form split win
      }
    }

    // Check for 3-4 char hiragana verb ending with た/だ (past form) BEFORE threshold check
    // e.g., つかれた (疲れた), ねむった (眠った), おきた (起きた)
    // These need lower threshold because ichidan_pure_hiragana_stem penalty reduces confidence
    size_t pre_check_len = end_pos - start_pos;
    bool looks_like_past_form = false;
    bool looks_like_te_form = false;
    if ((pre_check_len == 3 || pre_check_len == 4) &&
        surface.size() >= core::kJapaneseCharBytes) {
      std::string_view last_char = utf8::lastChar(surface);
      if (utf8::equalsAny(last_char, {"た", "だ"})) {
        looks_like_past_form = true;
      } else if (utf8::equalsAny(last_char, {"て", "で"})) {
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
      std::string_view last_char = utf8::lastChar(surface);
      if (last_char == "る" && end_pos >= 2) {
        // Check if second-to-last char is e-row or i-row hiragana (ichidan stem ending)
        // E-row: 食べる, 見える, 調べる
        // I-row: 感じる, 信じる (kanji + i-row + る pattern)
        char32_t stem_end = codepoints[end_pos - 2];
        if (grammar::isERowCodepoint(stem_end) || grammar::isIRowCodepoint(stem_end)) {
          // Exclude てる pattern (ている contraction) - this should be suru/godan + ている
          // not ichidan dictionary form
          bool is_te_iru_contraction = (stem_end == U'て' || stem_end == U'で');
          // Exclude particle + いる pattern (にいる, でいる, etc.)
          // These should be split as particle + いる (existence verb), not a single verb
          // Valid hiragana verbs starting with particle chars: にる (煮る), にげる (逃げる)
          // But にいる, であるいる, etc. are not valid verbs
          bool is_particle_iru = false;
          if (pre_check_len == 3 && stem_end == U'い' && normalize::isCommonParticle(first_char)) {
            // 3-char pattern: particle + いる
            is_particle_iru = true;
          }
          if (!is_te_iru_contraction && !is_particle_iru) {
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
      // Skip long particle-starting verb candidates when remainder is a valid verb form
      // e.g., "になっております" should be "に" + "なっております", not a single verb
      // This prevents false verbs like "になる" + conjugation from being recognized
      // Only apply to 5+ char forms to preserve short verbs (にる, にげる, etc.)
      size_t len_check = end_pos - start_pos;
      if (len_check >= 5 && normalize::isCommonParticle(first_char)) {
        // Extract remainder (surface without first character)
        std::string remainder = surface.substr(core::kJapaneseCharBytes);
        auto remainder_cands = inflection.analyze(remainder);
        for (const auto& rem_cand : remainder_cands) {
          if (rem_cand.verb_type != grammar::VerbType::IAdjective &&
              rem_cand.verb_type != grammar::VerbType::Unknown &&
              rem_cand.confidence >= 0.5F) {
            // Remainder looks like a valid verb form - skip this candidate
            // to let particle + verb split win
            SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface
                << "\" skip particle_start_verb (particle=U+"
                << std::hex << static_cast<uint32_t>(first_char) << std::dec
                << ", remainder=" << remainder
                << ", conf=" << rem_cand.confidence << ")\n");
            goto next_length;  // Continue to next end_pos
          }
        }
      }

      // Skip 3-char particle + いる/ある patterns (にいる, にある, でいる, といる, etc.)
      // These should be particle + existence verb, not a single hiragana verb
      // Valid 3-char verbs: にる(煮る), にげる(逃げる) have different patterns
      // Include extended particles: で, と, も (in addition to common particles)
      if (len_check == 3 && normalize::isExtendedParticle(first_char)) {
        std::string remainder = surface.substr(core::kJapaneseCharBytes);
        if (remainder == "いる" || remainder == "ある") {
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface
              << "\" skip particle_iru_aru (particle=U+"
              << std::hex << static_cast<uint32_t>(first_char) << std::dec << ")\n");
          goto next_length;
        }
      }

      // Lower cost for higher confidence matches
      float base_cost = verb_opts.base_cost_high + (1.0F - best.confidence) * verb_opts.confidence_cost_scale;

      // Give significant bonus for dictionary-verified hiragana verbs
      // This helps them beat the particle+adj+particle split path
      // Only apply to longer forms (5+ chars) to avoid boosting short forms like
      // "あった" (ある) which can interfere with copula recognition (であった)
      // Exception: Conditional forms (ending with ば) are unambiguous and should
      // get the bonus even if short (e.g., あれば = ある conditional)
      size_t candidate_len = end_pos - start_pos;
      bool is_conditional = utf8::endsWith(surface, "ば");
      // Check for っとく pattern (ておく contraction: やっとく, 見っとく)
      // This is a common colloquial pattern that should get bonus treatment
      bool is_teoku_contraction = utf8::endsWith(surface, "っとく");
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
      // Skip if stem (without た/だ) is a known auxiliary (e.g., そうだ → そう is AUX)
      bool is_medium_past_form = false;
      if ((candidate_len == 3 || candidate_len == 4) && best.confidence >= verb_opts.confidence_past_te) {
        if (utf8::equalsAny(utf8::lastChar(surface), {"た", "だ"})) {
          // Extract stem (surface without last た/だ)
          std::string_view stem(surface.data(), surface.size() - core::kJapaneseCharBytes);
          // Skip if stem is a known auxiliary (e.g., そう+だ should not be verb candidate)
          if (!vh::hasDictionaryEntry(dict_manager, stem, core::PartOfSpeech::Auxiliary)) {
            is_medium_past_form = true;
          }
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

      // Set lemma from inflection analysis for pure hiragana verbs
      // This is essential for P4 (ひらがな動詞活用展開) to work without dictionary
      // The lemmatizer can't derive lemma accurately for unknown verbs
      candidates.push_back(makeVerbCandidate(
          surface, start_pos, end_pos, base_cost, best.base_form,
          grammar::verbTypeToConjType(best.verb_type),
          false, CandidateOrigin::VerbHiragana, best.confidence,
          grammar::verbTypeToString(best.verb_type).data()));
    }
next_length:;  // Label for goto from particle-starting verb skip
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

    constexpr float kCost = -0.5F;  // Negative cost to beat OTHER + AUX split
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                          << " hiragana_" << pattern_name << " lemma=" << lemma
                          << " cost=" << kCost << "\n";
    }
    candidates.push_back(makeVerbCandidate(
        surface, start_pos, split_end, kCost, lemma,
        grammar::verbTypeToConjType(verb_type),
        true, CandidateOrigin::VerbHiragana, 0.9F, "hiragana_passive_mizenkei"));
    break;  // Only generate one passive candidate per length
  }

  // Generate Ichidan verb stem candidates for hiragana られる pattern
  // E.g., いられる → い (renyokei of いる) + られる (potential/passive AUX)
  //       おきられる → おき (renyokei of おきる) + られる
  // MeCab splits: いられる → い(動詞,一段,連用形) + られる(助動詞)
  // Pattern: ichidan stem (E-row ending or い/え) + られ + る/た/て
  // Search for られ starting at positions from start_pos+1 to hiragana_end-2
  for (size_t ra_pos = start_pos + 1; ra_pos + 2 < hiragana_end; ++ra_pos) {
    // Check for られ pattern at this position
    if (codepoints[ra_pos] != U'ら' || codepoints[ra_pos + 1] != U'れ') {
      continue;
    }

    // Check for られる, られた, られて, られな, られま patterns
    bool is_potential_passive_pattern = false;
    if (ra_pos + 2 < codepoints.size()) {
      char32_t third_aux = codepoints[ra_pos + 2];
      // られる, られた, られて
      if (third_aux == U'る' || third_aux == U'た' || third_aux == U'て') {
        is_potential_passive_pattern = true;
      }
      // られな (られない, られなかった)
      else if (third_aux == U'な' && ra_pos + 3 < codepoints.size() &&
               codepoints[ra_pos + 3] == U'い') {
        is_potential_passive_pattern = true;
      }
      // られま (られます, られました)
      else if (third_aux == U'ま') {
        is_potential_passive_pattern = true;
      }
    }

    if (!is_potential_passive_pattern) {
      continue;
    }

    // The stem is everything before ら
    size_t stem_end = ra_pos;  // Exclusive end of stem
    if (stem_end <= start_pos) continue;

    std::string stem = extractSubstring(codepoints, start_pos, stem_end);
    std::string base_form = stem + "る";  // Ichidan base form = stem + る

    // Validate: check if base form is a known ichidan verb
    // For pure hiragana like いる, check the dictionary
    bool is_valid_ichidan = vh::isVerbInDictionaryWithType(
        dict_manager, base_form, grammar::VerbType::Ichidan);

    // Special case: common hiragana ichidan verbs (いる, おきる, みる, etc.)
    // These may not always be in the L2 dictionary but are valid
    if (!is_valid_ichidan) {
      // Check if inflection analysis recognizes base_form as ichidan
      auto analysis = inflection.analyze(base_form);
      if (!analysis.empty() && analysis[0].verb_type == grammar::VerbType::Ichidan &&
          analysis[0].confidence >= 0.5F) {
        is_valid_ichidan = true;
      }
    }

    if (!is_valid_ichidan) {
      continue;
    }

    // Get lemma from dictionary if available
    std::string lemma = base_form;
    if (dict_manager != nullptr) {
      auto results = dict_manager->lookup(stem, 0);
      for (const auto& result : results) {
        if (result.entry != nullptr && result.entry->surface == stem &&
            result.entry->pos == core::PartOfSpeech::Verb && !result.entry->lemma.empty()) {
          lemma = result.entry->lemma;
          break;
        }
      }
    }

    // Generate the ichidan renyokei candidate
    // Negative cost to beat the single-word verb candidate
    constexpr float kCost = -0.5F;
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem
                          << " hiragana_ichidan_rareru lemma=" << lemma
                          << " cost=" << kCost << "\n";
    }
    candidates.push_back(makeVerbCandidate(
        stem, start_pos, stem_end, kCost, lemma,
        dictionary::ConjugationType::Ichidan,
        true, CandidateOrigin::VerbHiragana, 0.9F, "hiragana_ichidan_rareru"));
    break;  // Only generate one ichidan rareru candidate per starting position
  }

  // Generate Godan mizenkei stem candidates for contracted negative ん pattern
  // E.g., くだらん → くだら (mizenkei of くだる) + ん (contracted negative)
  //       つまらん → つまら (mizenkei of つまる) + ん (contracted negative)
  //       行かん → 行か (mizenkei of 行く) + ん (contracted negative)
  // MeCab splits: くだらん → くだら(動詞,五段,未然形) + ん(助動詞)
  // Pattern: A-row hiragana (mizenkei ending) + ん
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 1; --end_pos) {
    // Check if the string ends with ん at this position
    if (end_pos >= codepoints.size() || codepoints[end_pos] != U'ん') {
      continue;
    }

    // Check if position end_pos-1 is A-row hiragana (mizenkei ending)
    size_t mizenkei_end = end_pos;  // Position of ん (exclusive end of mizenkei)
    if (mizenkei_end <= start_pos) continue;

    char32_t a_row_char = codepoints[mizenkei_end - 1];  // The A-row character
    if (!grammar::isARowCodepoint(a_row_char)) {
      continue;
    }

    // Determine verb type from A-row character
    grammar::VerbType verb_type = grammar::VerbType::Unknown;
    std::string_view base_suffix;
    switch (a_row_char) {
      case U'わ': verb_type = grammar::VerbType::GodanWa; base_suffix = "う"; break;
      case U'か': verb_type = grammar::VerbType::GodanKa; base_suffix = "く"; break;
      case U'が': verb_type = grammar::VerbType::GodanGa; base_suffix = "ぐ"; break;
      case U'さ': verb_type = grammar::VerbType::GodanSa; base_suffix = "す"; break;
      case U'た': verb_type = grammar::VerbType::GodanTa; base_suffix = "つ"; break;
      case U'な': verb_type = grammar::VerbType::GodanNa; base_suffix = "ぬ"; break;
      case U'ば': verb_type = grammar::VerbType::GodanBa; base_suffix = "ぶ"; break;
      case U'ま': verb_type = grammar::VerbType::GodanMa; base_suffix = "む"; break;
      case U'ら': verb_type = grammar::VerbType::GodanRa; base_suffix = "る"; break;
      default: continue;  // Not a recognized mizenkei ending
    }

    // Construct mizenkei surface and base form
    std::string mizenkei_surface = extractSubstring(codepoints, start_pos, mizenkei_end);
    std::string stem = extractSubstring(codepoints, start_pos, mizenkei_end - 1);
    std::string base_form = stem + std::string(base_suffix);

    // Validate: analyze the full form (including ん) to check if it's a valid verb
    // Since mizenkei alone may not be recognized, we check if (stem + ん) is recognized
    // as a verb with the expected type
    std::string full_form = mizenkei_surface + "ん";
    auto analysis = inflection.analyze(full_form);
    bool is_valid_verb = false;
    for (const auto& cand : analysis) {
      if (cand.verb_type == verb_type && cand.base_form == base_form) {
        is_valid_verb = true;
        break;
      }
    }

    // Also check if base form exists in dictionary
    if (!is_valid_verb) {
      is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
    }

    // Minimum stem length check: need at least 2 chars in mizenkei to be meaningful
    // This prevents false positives like "かん" → "か" + "ん"
    if (!is_valid_verb && mizenkei_surface.size() < 6) {  // 2 chars = 6 bytes in UTF-8
      continue;
    }

    // Get lemma from dictionary entry if available
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

    // Generate mizenkei candidate with explicit VerbMizenkei EPOS for bigram connection
    constexpr float kCost = -0.5F;  // Negative cost to beat unsplit form
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << mizenkei_surface
                          << " hiragana_mizenkei_n lemma=" << lemma
                          << " cost=" << kCost << "\n";
    }
    candidates.push_back(makeVerbCandidate(
        mizenkei_surface, start_pos, mizenkei_end, kCost, lemma,
        grammar::verbTypeToConjType(verb_type),
        true, CandidateOrigin::VerbHiragana, 0.9F, "hiragana_mizenkei_n",
        core::ExtendedPOS::VerbMizenkei));
    break;  // Only generate one candidate per position
  }

  // Generate Godan mizenkei stem candidates for negative auxiliary ない pattern
  // E.g., わからない → わから (mizenkei of わかる) + ない (negative auxiliary)
  //       動かない → 動か (mizenkei of 動く) + ない (negative auxiliary)
  // MeCab splits: わからない → わから(動詞,五段,未然形) + ない(助動詞)
  // Pattern: A-row hiragana (mizenkei ending) + ない
  // Loop includes end_pos == start_pos + 2 for 2-char stems like いか (いく)
  for (size_t end_pos = hiragana_end; end_pos >= start_pos + 2; --end_pos) {
    // Check if the string ends with ない at this position
    if (end_pos + 2 > codepoints.size()) continue;
    if (codepoints[end_pos] != U'な' || codepoints[end_pos + 1] != U'い') {
      continue;
    }

    // Check if position end_pos-1 is A-row hiragana (mizenkei ending)
    size_t mizenkei_end = end_pos;  // Position of な (exclusive end of mizenkei)
    if (mizenkei_end <= start_pos) continue;

    char32_t a_row_char = codepoints[mizenkei_end - 1];  // The A-row character
    if (!grammar::isARowCodepoint(a_row_char)) {
      continue;
    }

    // Determine verb type from A-row character
    grammar::VerbType verb_type = grammar::VerbType::Unknown;
    std::string_view base_suffix;
    switch (a_row_char) {
      case U'わ': verb_type = grammar::VerbType::GodanWa; base_suffix = "う"; break;
      case U'か': verb_type = grammar::VerbType::GodanKa; base_suffix = "く"; break;
      case U'が': verb_type = grammar::VerbType::GodanGa; base_suffix = "ぐ"; break;
      case U'さ': verb_type = grammar::VerbType::GodanSa; base_suffix = "す"; break;
      case U'た': verb_type = grammar::VerbType::GodanTa; base_suffix = "つ"; break;
      case U'な': verb_type = grammar::VerbType::GodanNa; base_suffix = "ぬ"; break;
      case U'ば': verb_type = grammar::VerbType::GodanBa; base_suffix = "ぶ"; break;
      case U'ま': verb_type = grammar::VerbType::GodanMa; base_suffix = "む"; break;
      case U'ら': verb_type = grammar::VerbType::GodanRa; base_suffix = "る"; break;
      default: continue;  // Not a recognized mizenkei ending
    }

    // Construct mizenkei surface and base form
    std::string mizenkei_surface = extractSubstring(codepoints, start_pos, mizenkei_end);
    std::string stem = extractSubstring(codepoints, start_pos, mizenkei_end - 1);
    std::string base_form = stem + std::string(base_suffix);

    // Validate: analyze the full form (including ない) to check if it's a valid verb
    std::string full_form = mizenkei_surface + "ない";
    auto analysis = inflection.analyze(full_form);
    bool is_valid_verb = false;
    for (const auto& cand : analysis) {
      if (cand.verb_type == verb_type && cand.base_form == base_form) {
        is_valid_verb = true;
        break;
      }
    }

    // Also check if base form exists in dictionary
    if (!is_valid_verb) {
      is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
    }

    // Minimum stem length check: need at least 2 chars in mizenkei to be meaningful
    // This prevents false positives like "かない" → "か" + "ない"
    if (!is_valid_verb && mizenkei_surface.size() < 6) {  // 2 chars = 6 bytes in UTF-8
      continue;
    }

    // Get lemma from dictionary entry if available
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

    // Generate mizenkei candidate with explicit VerbMizenkei EPOS for bigram connection
    constexpr float kCostNai = -0.5F;  // Negative cost to beat unsplit form
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << mizenkei_surface
                          << " hiragana_mizenkei_nai lemma=" << lemma
                          << " cost=" << kCostNai << "\n";
    }
    candidates.push_back(makeVerbCandidate(
        mizenkei_surface, start_pos, mizenkei_end, kCostNai, lemma,
        grammar::verbTypeToConjType(verb_type),
        true, CandidateOrigin::VerbHiragana, 0.9F, "hiragana_mizenkei_nai",
        core::ExtendedPOS::VerbMizenkei));
    break;  // Only generate one candidate per position
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
    bool is_tense_pattern = false;  // っ+た/て (past/te-form)
    if (is_sokuonbin) {
      // っ + と (とく/といた/といて) or ち (ちゃう/ちゃった/ちゃって)
      is_contraction_pattern = (next_char == U'と' || next_char == U'ち');
      // っ + た/て (past/te-form for GodanWa/Ra/Ta)
      // E.g., かった → かっ + た (from かう), やった → やっ + た (from やる)
      is_tense_pattern = (next_char == U'た' || next_char == U'て');
    } else {
      // ん + ど (どく/どいた/どいて) or じ (じゃう/じゃった/じゃって) or で (でる/でた/でて)
      is_contraction_pattern = (next_char == U'ど' || next_char == U'じ' || next_char == U'で');
      // ん + だ/で (past/te-form for GodanMa/Ba/Na)
      // E.g., 読んだ → 読ん + だ (from 読む), 飛んだ → 飛ん + だ (from 飛ぶ)
      is_tense_pattern = (next_char == U'だ' || next_char == U'で');
    }

    if (!is_contraction_pattern && !is_tense_pattern) {
      continue;
    }

    // Get the stem (part before onbin character)
    std::string stem = extractSubstring(codepoints, start_pos, onbin_pos);
    if (stem.empty()) {
      continue;
    }

    // Check if stem starts with common case particles (と、を、に、で、が、は、へ)
    // Used later to skip short particle+verb patterns unless dictionary-verified
    // E.g., となっ (stem=とな, 2 chars) → skip, particle + なる is more likely
    //       はじまっ (stem=はじま, 3 chars) → allow, longer stems are more likely verbs
    bool starts_with_short_particle_stem = false;
    size_t stem_char_count = suzume::normalize::utf8Length(stem);
    if (stem_char_count == 2) {  // Only skip 2-char stems
      char32_t first_char = codepoints[start_pos];
      starts_with_short_particle_stem = (first_char == U'と' || first_char == U'を' ||
                                         first_char == U'に' || first_char == U'で' ||
                                         first_char == U'が' || first_char == U'は' ||
                                         first_char == U'へ');
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

    // Try each verb type and check dictionary or inflection analysis
    for (const auto& [verb_type, base_suffix] : candidates_to_try) {
      std::string base_form = stem + std::string(base_suffix);

      // Check if base form exists in dictionary as this verb type
      bool is_valid_verb = vh::isVerbInDictionaryWithType(dict_manager, base_form, verb_type);

      // For tense patterns (っ+た/て, ん+だ/で), also use inflection analysis
      // This handles common verbs like かう, やる that may not be in dictionary
      // but can be validated via inflection analysis of the full pattern
      // Exception: skip short particle-starting stems (となっ should be と+なっ)
      // Longer stems like はじまっ (3+ chars) are allowed
      if (!is_valid_verb && is_tense_pattern && !starts_with_short_particle_stem) {
        // Construct full form: onbin + tense suffix (e.g., かった, やった)
        std::string_view tense_char = (next_char == U'た' || next_char == U'だ') ? "た" : "て";
        std::string full_form = stem + (is_sokuonbin ? "っ" : "ん") + std::string(tense_char);
        auto analysis = inflection.analyze(full_form);
        for (const auto& cand : analysis) {
          // Lower threshold (0.25) for short stems like かっ, やっ
          // since godan_single_hiragana_stem penalty reduces confidence
          if (cand.verb_type == verb_type && cand.base_form == base_form &&
              cand.confidence >= 0.25F) {
            is_valid_verb = true;
            break;
          }
        }
      }

      if (!is_valid_verb) {
        continue;
      }

      // Found a valid verb - generate onbin stem candidate
      std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_pos + 1);
      // For tense patterns, use higher cost to avoid false positives for short stems
      // Contraction patterns (っとく, っちゃう) are more reliable, use lower cost
      float cost = is_contraction_pattern ? -0.5F : 0.2F;
      SUZUME_DEBUG_VERBOSE_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                            << (is_tense_pattern ? " hiragana_onbin_tense" : " hiragana_onbin_contraction")
                            << " lemma=" << base_form
                            << " cost=" << cost << "\n";
      }
      const char* pattern = is_sokuonbin ? "hiragana_sokuonbin" : "hiragana_hatsuonbin";
      candidates.push_back(makeVerbCandidate(
          onbin_surface, start_pos, onbin_pos + 1, cost, base_form,
          grammar::verbTypeToConjType(verb_type),
          true, CandidateOrigin::VerbHiragana, 0.9F, pattern,
          core::ExtendedPOS::VerbOnbinkei));
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
            // Strong negative cost to beat particle split
            // Particle path can be as low as -0.2, so we need lower
            constexpr float kCost = -0.5F;
            SUZUME_DEBUG_VERBOSE_BLOCK {
              SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem_surface
                                  << " hiragana_ichidan_renyokei_1char lemma=" << base_form
                                  << " cost=" << kCost << "\n";
            }
            candidates.push_back(makeVerbCandidate(
                stem_surface, start_pos, start_pos + 1, kCost, base_form,
                dictionary::ConjugationType::Ichidan,
                true, CandidateOrigin::VerbHiragana, 0.8F, "hiragana_ichidan_renyokei_1char"));
          }
        }
      }
    }
  }

  // Generate 2+ char ichidan renyokei stem candidates
  // E.g., つけて → つけ (renyokei of つける) + て (particle)
  //       たべて → たべ (renyokei of たべる) + て (particle)
  //       あけて → あけ (renyokei of あける) + て (particle)
  //       すぎて → すぎ (renyokei of すぎる) + て (particle)
  // MeCab splits: つけて → つけ(動詞,一段,連用形) + て(助詞,接続助詞)
  // Pattern: 2+ char sequence ending with e-row or i-row hiragana followed by て or た
  // Note: Ichidan verbs have both e-row stems (食べる) and i-row stems (感じる, 過ぎる)
  // Uses inflection analysis confidence to validate (dictionary lookup as bonus)
  for (size_t end_pos = start_pos + 2; end_pos < hiragana_end; ++end_pos) {
    // Check if position end_pos-1 is e-row or i-row hiragana (ichidan renyokei ending)
    // E-row: 食べる, 見える → 食べ, 見え
    // I-row: 感じる, 過ぎる → 感じ, すぎ
    char32_t stem_end_char = codepoints[end_pos - 1];
    if (!grammar::isERowCodepoint(stem_end_char) && !grammar::isIRowCodepoint(stem_end_char)) {
      continue;
    }

    // Exclude て and で which are more commonly particles
    if (stem_end_char == U'て' || stem_end_char == U'で') {
      continue;
    }

    // Check if followed by te/ta particle, polite ます auxiliary, or conditional れば
    if (end_pos >= codepoints.size()) {
      continue;
    }
    char32_t next_char = codepoints[end_pos];
    bool is_followed_by_te_ta = (next_char == U'て' || next_char == U'た');
    bool is_followed_by_masu = false;
    bool is_followed_by_reba = false;
    // Check for polite ます auxiliary pattern (e.g., できます → でき + ます)
    if (next_char == U'ま' && end_pos + 1 < codepoints.size() &&
        codepoints[end_pos + 1] == U'す') {
      is_followed_by_masu = true;
    }
    // Check for conditional れば pattern (e.g., できれば → でき + れ + ば)
    // This case is handled separately below for kateikei stem generation
    if (next_char == U'れ' && end_pos + 1 < codepoints.size() &&
        codepoints[end_pos + 1] == U'ば') {
      is_followed_by_reba = true;
    }
    // Check for negative ない pattern (e.g., できない → でき + ない)
    bool is_followed_by_nai = false;
    if (next_char == U'な' && end_pos + 1 < codepoints.size() &&
        codepoints[end_pos + 1] == U'い') {
      is_followed_by_nai = true;
    }
    if (!is_followed_by_te_ta && !is_followed_by_masu && !is_followed_by_reba &&
        !is_followed_by_nai) {
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

    // Skip causative+passive auxiliary chain patterns
    // E.g., "せられ" should be split as せ(causative) + られ(passive), not single verb
    // MeCab-compatible split: 聞かせられた → 聞か + せ + られ + た
    if (utf8::endsWith(stem_surface, "せられ")) {
      continue;
    }

    // Strong negative cost to beat NOUN + て(VERB from てる) split
    // Dictionary-verified verbs get stronger bonus
    // Non-dictionary verbs get moderate positive cost to avoid spurious candidates
    // competing with dictionary compound particles like について
    // But not too high to break valid patterns like してほしい
    float cost = is_dict_verb ? -0.8F : 0.5F;
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem_surface
                          << " hiragana_ichidan_renyokei lemma=" << base_form
                          << " conf=" << ichidan_confidence
                          << (is_dict_verb ? " [dict]" : "")
                          << " cost=" << cost << "\n";
    }
    candidates.push_back(makeVerbCandidate(
        stem_surface, start_pos, end_pos, cost, base_form,
        dictionary::ConjugationType::Ichidan,
        true, CandidateOrigin::VerbHiragana, ichidan_confidence, "hiragana_ichidan_renyokei"));

    // Also generate kateikei stem if followed by れば
    // E.g., できれば → できれ (kateikei of できる) + ば
    // MeCab splits: できれば → できれ(動詞,仮定形) + ば(接続助詞)
    if (is_followed_by_reba) {
      std::string kateikei_surface = stem_surface + "れ";  // 連用形 + れ = 仮定形
      size_t kateikei_end = end_pos + 1;  // renyokei + れ
      constexpr float kKateikeiCost = -0.8F;
      SUZUME_DEBUG_VERBOSE_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_CAND] " << kateikei_surface
                            << " hiragana_ichidan_kateikei lemma=" << base_form
                            << " conf=" << ichidan_confidence
                            << " cost=" << kKateikeiCost << "\n";
      }
      candidates.push_back(makeVerbCandidate(
          kateikei_surface, start_pos, kateikei_end, kKateikeiCost, base_form,
          dictionary::ConjugationType::Ichidan,
          true, CandidateOrigin::VerbHiragana, ichidan_confidence, "hiragana_ichidan_kateikei",
          core::ExtendedPOS::VerbKateikei));
    }
  }

  // Generate Godan sokuonbin (っ) candidates for hiragana verbs
  // E.g., しまった → しまっ (onbin of しまう) + た (auxiliary)
  //       なくなった → なくなっ (onbin of なくなる) + た (auxiliary)
  // This allows MeCab-compatible splitting: しまった → しまっ + た
  {
    // Find hiragana extent (all consecutive hiragana from start_pos)
    size_t hira_extent_end = start_pos;
    while (hira_extent_end < char_types.size() &&
           char_types[hira_extent_end] == normalize::CharType::Hiragana) {
      ++hira_extent_end;
    }
    size_t hira_len = hira_extent_end - start_pos;

    // Need at least 3 chars: stem(1+) + っ + た/て
    if (hira_len >= 3) {
      char32_t second_last = codepoints[hira_extent_end - 2];
      char32_t last_char = codepoints[hira_extent_end - 1];
      bool is_sokuonbin_te_ta = (second_last == U'っ' &&
                                 (last_char == U'た' || last_char == U'て'));
      if (is_sokuonbin_te_ta) {
        // Generate candidate for stem + っ (without the た/て)
        size_t onbin_end = hira_extent_end - 1;  // Position after っ
        std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);
        std::string stem = extractSubstring(codepoints, start_pos, onbin_end - 1);

        // Try different base form patterns for っ-onbin
        // GodanWa: しま + う → しまう, GodanRa: なくな + る → なくなる
        // GodanKa: い + く → いく (irregular: いく uses 促音便 instead of イ音便)
        static const std::vector<std::pair<grammar::VerbType, std::string_view>>
            sokuonbin_types = {
                {grammar::VerbType::GodanKa, "く"},  // いく (irregular sokuonbin)
                {grammar::VerbType::GodanRa, "る"},
                {grammar::VerbType::GodanTa, "つ"},
                {grammar::VerbType::GodanWa, "う"},
            };

        bool found_dict_match = false;
        for (const auto& [verb_type, base_suffix] : sokuonbin_types) {
          std::string potential_base = stem + std::string(base_suffix);
          bool in_dict = vh::isVerbInDictionaryWithType(dict_manager, potential_base, verb_type) ||
                         vh::isVerbInDictionary(dict_manager, potential_base);
          if (in_dict) {
            constexpr float kHiraganaSokuonbinCost = -0.5F;
            SUZUME_DEBUG_VERBOSE_BLOCK {
              SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                  << " hiragana_sokuonbin lemma=" << potential_base
                                  << " type=" << grammar::verbTypeToString(verb_type)
                                  << " cost=" << kHiraganaSokuonbinCost << "\n";
            }
            candidates.push_back(makeVerbCandidate(
                onbin_surface, start_pos, onbin_end, kHiraganaSokuonbinCost, potential_base,
                grammar::verbTypeToConjType(verb_type),
                true, CandidateOrigin::VerbHiragana, 0.9F, "hiragana_sokuonbin",
                core::ExtendedPOS::VerbOnbinkei));
            found_dict_match = true;
            break;  // Found valid base, stop trying other types
          }
        }
        // Phase 2: Inflection analysis fallback for short hiragana stems (e.g., やっ)
        // Only for stems of 1-2 characters (e.g., や, やる → やっ)
        if (!found_dict_match && stem.size() <= 6) {  // 2 chars * 3 bytes max
          std::string full_surface = extractSubstring(codepoints, start_pos, hira_extent_end);
          auto infl_results = inflection.analyze(full_surface);
          for (const auto& result : infl_results) {
            if (result.confidence >= 0.5F) {
              for (const auto& [verb_type, base_suffix] : sokuonbin_types) {
                std::string potential_base = stem + std::string(base_suffix);
                if (result.base_form == potential_base && result.verb_type == verb_type) {
                  constexpr float kHiraganaSokuonbinCost = -0.3F;  // Slightly higher than dict match
                  SUZUME_DEBUG_VERBOSE_BLOCK {
                    SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                        << " hiragana_sokuonbin_infl lemma=" << potential_base
                                        << " type=" << grammar::verbTypeToString(verb_type)
                                        << " cost=" << kHiraganaSokuonbinCost << "\n";
                  }
                  candidates.push_back(makeVerbCandidate(
                      onbin_surface, start_pos, onbin_end, kHiraganaSokuonbinCost, potential_base,
                      grammar::verbTypeToConjType(verb_type),
                      true, CandidateOrigin::VerbHiragana, 0.8F, "hiragana_sokuonbin_infl",
                      core::ExtendedPOS::VerbOnbinkei));
                  found_dict_match = true;
                  break;
                }
              }
              if (found_dict_match) break;
            }
          }
        }
      }
    }
  }

  // Generate Godan hatsuonbin (ん) candidates for hiragana verbs
  // E.g., こんだ → こん (onbin of こむ) + だ (auxiliary)
  //       こんで → こん (onbin of こむ) + で (particle)
  //       よんだ → よん (onbin of よむ) + だ (auxiliary)
  // This allows MeCab-compatible splitting for godan-ma/ba/na verbs
  {
    // Find hiragana extent (all consecutive hiragana from start_pos)
    size_t hira_extent_end = start_pos;
    while (hira_extent_end < char_types.size() &&
           char_types[hira_extent_end] == normalize::CharType::Hiragana) {
      ++hira_extent_end;
    }
    size_t hira_len = hira_extent_end - start_pos;

    // Need at least 3 chars: stem(1+) + ん + だ/で
    if (hira_len >= 3) {
      char32_t second_last = codepoints[hira_extent_end - 2];
      char32_t last_char = codepoints[hira_extent_end - 1];
      bool is_hatsuonbin_de_da = (second_last == U'ん' &&
                                   (last_char == U'だ' || last_char == U'で'));
      if (is_hatsuonbin_de_da) {
        // Generate candidate for stem + ん (without the だ/で)
        size_t onbin_end = hira_extent_end - 1;  // Position after ん
        std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);
        std::string stem = extractSubstring(codepoints, start_pos, onbin_end - 1);

        // Try different base form patterns for ん-onbin
        // Godan-ma: こ + む → こむ, よ + む → よむ
        // Godan-ba: と + ぶ → とぶ
        // Godan-na: し + ぬ → しぬ
        static const std::vector<std::pair<grammar::VerbType, std::string_view>>
            hatsuonbin_types = {
                {grammar::VerbType::GodanMa, "む"},
                {grammar::VerbType::GodanBa, "ぶ"},
                {grammar::VerbType::GodanNa, "ぬ"},
            };

        for (const auto& [verb_type, base_suffix] : hatsuonbin_types) {
          std::string potential_base = stem + std::string(base_suffix);
          bool in_dict = vh::isVerbInDictionaryWithType(dict_manager, potential_base, verb_type) ||
                         vh::isVerbInDictionary(dict_manager, potential_base);
          if (in_dict) {
            constexpr float kHiraganaHatsuonbinCost = -0.5F;
            SUZUME_DEBUG_VERBOSE_BLOCK {
              SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                  << " hiragana_hatsuonbin lemma=" << potential_base
                                  << " type=" << grammar::verbTypeToString(verb_type)
                                  << " cost=" << kHiraganaHatsuonbinCost << "\n";
            }
            candidates.push_back(makeVerbCandidate(
                onbin_surface, start_pos, onbin_end, kHiraganaHatsuonbinCost, potential_base,
                grammar::verbTypeToConjType(verb_type),
                true, CandidateOrigin::VerbHiragana, 0.9F, "hiragana_hatsuonbin",
                core::ExtendedPOS::VerbOnbinkei));
            break;  // Found valid base, stop trying other types
          }
        }
      }
    }
  }

  // Add emphatic variants (いくっ, するっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints);

  // Sort by cost
  vh::sortCandidatesByCost(candidates);

  return candidates;
}

}  // namespace suzume::analysis
