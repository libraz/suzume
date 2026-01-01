/**
 * @file inflection_scorer.cpp
 * @brief Confidence scoring for inflection analysis candidates
 */

#include "inflection_scorer.h"

#include <algorithm>

#include "char_patterns.h"
#include "connection.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "inflection_scorer_constants.h"

// Helper macro to get option value or fall back to constant
// Usage: GET_OPT(penalty_stem_very_long, inflection::kPenaltyStemVeryLong)
#define GET_OPT(field, default_val) \
  (opts ? suzume::grammar::InflectionScorerOptions::getOrDefault(opts->field, default_val) : default_val)

namespace {
// Helper to log confidence adjustments
void logConfidenceAdjustment(float amount, const char* reason) {
  if (amount != 0.0F) {
    SUZUME_DEBUG_LOG("  " << reason << ": "
                            << (amount > 0 ? "+" : "") << amount << "\n");
  }
}

// Helper to check if a string matches any item in a const array
template <size_t N>
bool isInArray(std::string_view s, const char* const (&arr)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (s == arr[i]) return true;
  }
  return false;
}
}  // namespace

namespace suzume::grammar {

float calculateConfidence(VerbType type, std::string_view stem,
                          size_t aux_total_len, size_t aux_count,
                          uint16_t required_conn, size_t suffix_len,
                          const InflectionScorerOptions* opts) {
  float base = GET_OPT(base_confidence, inflection::kBaseConfidence);
  size_t stem_len = stem.size();

  SUZUME_DEBUG_LOG("[INFL_SCORE] stem=\"" << stem << "\" type="
                     << static_cast<int>(type) << " aux_len=" << aux_total_len
                     << " aux_count=" << aux_count << " conn=" << required_conn
                     << " suffix_len=" << suffix_len
                     << ": base=" << base << "\n");

  // Stem length penalties/bonuses
  // Very long stems are suspicious (likely wrong analysis)
  if (stem_len >= core::kFourJapaneseCharBytes) {
    float pen = GET_OPT(penalty_stem_very_long, inflection::kPenaltyStemVeryLong);
    base -= pen;
    logConfidenceAdjustment(-pen, "stem_very_long");
  } else if (stem_len >= core::kThreeJapaneseCharBytes) {
    float pen = GET_OPT(penalty_stem_long, inflection::kPenaltyStemLong);
    base -= pen;
    logConfidenceAdjustment(-pen, "stem_long");
  } else if (stem_len >= core::kTwoJapaneseCharBytes) {
    // 2-char stems (6 bytes) are common
    float bon = GET_OPT(bonus_stem_two_char, inflection::kBonusStemTwoChar);
    base += bon;
    logConfidenceAdjustment(bon, "stem_two_char");
  } else if (stem_len >= core::kJapaneseCharBytes) {
    // 1-char stems (3 bytes) are possible but less common
    base += inflection::kBonusStemOneChar;
    logConfidenceAdjustment(inflection::kBonusStemOneChar, "stem_one_char");
  }

  // Small kana (拗音) cannot start a verb stem
  // ょ, ゃ, ゅ, ぁ, ぃ, ぅ, ぇ, ぉ, っ are always part of compound sounds
  // E.g., きょう is valid, but ょう alone cannot be a word
  if (stem_len >= core::kJapaneseCharBytes) {
    std::string_view first_char = stem.substr(0, core::kJapaneseCharBytes);
    if (isSmallKana(first_char)) {
      // Heavily penalize - this is grammatically impossible
      base -= inflection::kPenaltySmallKanaStemInvalid;
      logConfidenceAdjustment(-inflection::kPenaltySmallKanaStemInvalid, "small_kana_stem_invalid");
    }
    // ん cannot start a verb stem in Japanese
    // E.g., んじゃする is impossible - should be ん + じゃない
    if (first_char == "ん") {
      base -= inflection::kPenaltyNStartStemInvalid;
      logConfidenceAdjustment(-inflection::kPenaltyNStartStemInvalid, "n_start_stem_invalid");
    }
  }

  // Longer auxiliary chain = higher confidence (matched more grammar)
  float aux_per_byte = GET_OPT(bonus_aux_length_per_byte, inflection::kBonusAuxLengthPerByte);
  float aux_bonus = static_cast<float>(aux_total_len) * aux_per_byte;
  base += aux_bonus;
  logConfidenceAdjustment(aux_bonus, "aux_length");

  // Ichidan validation based on connection context
  if (type == VerbType::Ichidan) {
    // Ichidan verbs do NOT have onbin (音便) forms
    // Ichidan te-form uses renyokei + て: 食べて, 見て (NOT で)
    // Godan te-form uses onbinkei + て/で: 読んで, 書いて
    // If we're analyzing Ichidan in onbinkei context, it's USUALLY wrong
    // EXCEPTION: Ichidan stems end with E-row (下一段: 食べ, 忘れ) or I-row (上一段: 感じ, 見)
    // and their te-form IS connected via kVerbOnbinkei
    // EXCEPTION: すぎ (→ すぎる) is a legitimate Ichidan verb commonly used as auxiliary
    // Only apply penalty to stems that shouldn't be Ichidan (not E-row, not I-row)
    if (required_conn == conn::kVerbOnbinkei && !endsWithERow(stem) && !endsWithIRow(stem) &&
        !isInArray(stem, inflection::kValidHiraganaStemExceptions)) {
      base -= inflection::kPenaltyIchidanOnbinInvalid;
      logConfidenceAdjustment(-inflection::kPenaltyIchidanOnbinInvalid, "ichidan_onbin_invalid");
    }

    // Ichidan stems cannot end with onbin markers (っ, ん, い)
    // These are Godan onbin forms: 行っ(く), 読ん(む), 書い(く)
    // If Ichidan stem ends with these, it's a false match
    // E.g., 行っ + てた → 行っる (wrong) - should be 行く
    // P5-2: Exception for specific kanji + い ichidan stems (用い, 率い, 報い)
    if (stem_len >= core::kJapaneseCharBytes) {
      std::string_view last_char = stem.substr(stem_len - core::kJapaneseCharBytes);
      if (last_char == "っ" || last_char == "ん") {
        // っ and ん are always onbin markers
        base -= inflection::kPenaltyIchidanOnbinMarkerStemInvalid;
        logConfidenceAdjustment(-inflection::kPenaltyIchidanOnbinMarkerStemInvalid, "ichidan_onbin_marker_stem_invalid");
      } else if (last_char == "い") {
        // い can be onbin marker OR part of legitimate ichidan stem
        // Valid い-ending ichidan stems:
        // - Single-char い (from いる - to exist/be) P5-3
        // - 用い (用いる - to use), 率い (率いる - to lead), 報い (報いる - to repay) P5-2
        bool is_valid_i_stem = (stem == "い" ||  // P5-3: いる
                                isInArray(stem, inflection::kValidKanjiIStemExceptions));
        if (!is_valid_i_stem) {
          base -= inflection::kPenaltyIchidanOnbinMarkerStemInvalid;
          logConfidenceAdjustment(-inflection::kPenaltyIchidanOnbinMarkerStemInvalid, "ichidan_onbin_marker_stem_invalid");
        }
      }
    }

    // Ichidan volitional requires e-row stem ending (食べよう, 見せよう)
    // If stem ends with godan base endings (く, す, etc.), it's likely wrong
    // E.g., 続く + よう → 続くる (wrong) - should be 続こう
    if (required_conn == conn::kVerbVolitional && stem_len >= core::kJapaneseCharBytes) {
      std::string_view last_char = stem.substr(stem_len - core::kJapaneseCharBytes);
      bool is_godan_base_ending = (last_char == "く" || last_char == "す" ||
                                   last_char == "ぐ" || last_char == "つ" ||
                                   last_char == "ぬ" || last_char == "む" ||
                                   last_char == "ぶ" || last_char == "う");
      if (is_godan_base_ending) {
        base -= inflection::kPenaltyIchidanVolitionalGodanStem;
        logConfidenceAdjustment(-inflection::kPenaltyIchidanVolitionalGodanStem, "ichidan_volitional_godan_stem");
      }
    }

    if (endsWithERow(stem)) {
      // E-row endings (食べ, 見せ, etc.) are very common for Ichidan
      // But 2-char stems with e-row ending (書け, 読め) could be Godan potential
      //   - め (ma-row): 読む, 飲む, etc. - common
      //   - せ (sa-row): 話す, 出す, etc. - common
      //   - れ (ra-row): 取る, 乗る, etc. - common
      // But NOT:
      //   - べ (ba-row): 食べる is Ichidan, 飛ぶ → 飛べ is less common
      //   - え (wa-row): Many Ichidan verbs end in え (考える, 答える, 見える)
      //   - げ (ga-row): 泳ぐ, 急ぐ, etc. - moderately common
      //   - Others: て, ね, へ - less common as potential forms
      bool is_common_potential_ending = false;
      bool is_copula_de_pattern = false;
      if (stem_len >= core::kJapaneseCharBytes) {
        std::string_view last_char = stem.substr(stem_len - core::kJapaneseCharBytes);
        is_common_potential_ending = (last_char == "け" || last_char == "め" ||
                                      last_char == "せ" || last_char == "れ" ||
                                      last_char == "げ");
        // All-kanji + で patterns are usually copula, not verb stems
        // e.g., 嫌でない = 嫌 + で + ない, 公園でる is not a real verb
        // Valid Ichidan verbs ending in で are rare: 茹でる, 出でる (archaic)
        // These have single-kanji stems (茹, 出), not multi-kanji stems
        if (last_char == "で" && stem_len >= core::kTwoJapaneseCharBytes) {
          std::string_view stem_before_de = stem.substr(0, stem_len - core::kJapaneseCharBytes);
          if (isAllKanji(stem_before_de)) {
            // Kanji+ + で pattern: likely copula (だ/です) not Ichidan verb
            // 公園で, 速攻で, etc. are NOUN + copula patterns
            is_copula_de_pattern = true;
          }
        }
      }
      // Apply penalty only when:
      // 1. Stem is 2 chars (kanji + e-row hiragana)
      // 2. In a context where Godan potential interpretation is possible
      // 3. The e-row ending is a common Godan potential form (け, め, せ, れ)
      // Note: kVerbBase is included because pure potential forms like 読める
      // are parsed as Ichidan with る base ending, but should prefer Godan potential
      // WHEN auxiliaries are attached (e.g., 読めない → 読む potential + ない).
      // P5-1: Verified - current design is intentional:
      // Pure potential forms (書ける, 読める) are treated as independent Ichidan verbs
      // when aux_count==0. This is a deliberate design choice.
      // With auxiliaries (書けない, 読めなかった), they correctly trace back to Godan.
      bool is_potential_context =
          required_conn == conn::kVerbRenyokei ||
          required_conn == conn::kVerbMizenkei ||
          (required_conn == conn::kVerbBase && aux_count > 0);
      // Ichidan stems ending in て are suspicious as base forms
      // "来て" as Ichidan stem → "来てる" is wrong; it's actually 来る te-form
      // Exception: 捨てる, 棄てる have legitimate て-ending stems
      // Apply penalty when aux_count == 0 (analyzing as base/dictionary form)
      bool is_te_stem_in_base_context = false;
      if (stem_len >= core::kTwoJapaneseCharBytes && aux_count == 0) {
        std::string_view last_char = stem.substr(stem_len - core::kJapaneseCharBytes);
        if (last_char == "て") {
          // Check if this is a known exception (捨て, 棄て)
          std::string_view stem_before_te = stem.substr(0, stem_len - core::kJapaneseCharBytes);
          if (!isInArray(stem_before_te, inflection::kTeEndingStemExceptionKanji)) {
            is_te_stem_in_base_context = true;
          }
        }
      }

      // Check for suru-verb imperative pattern: multi-kanji + せ
      // e.g., 勉強せ, 検討せ, 運動せ - these are suru-verb imperative stems, not Ichidan
      // The pattern kanji+ + せ is more likely suru-verb せよ/しろ form
      // Note: Only applies to 2+ kanji stems, not single kanji + せ
      // Single kanji + せ (話せ, 見せ) is more likely Godan potential form
      bool is_suru_imperative_pattern = false;
      if (stem_len >= core::kTwoJapaneseCharBytes) {
        std::string_view last_char = stem.substr(stem_len - core::kJapaneseCharBytes);
        if (last_char == "せ") {
          std::string_view stem_before_se = stem.substr(0, stem_len - core::kJapaneseCharBytes);
          // Only apply to 2+ kanji stems (勉強, 検討, etc.), not single kanji (話, 見)
          if (isAllKanji(stem_before_se) && stem_before_se.size() >= core::kTwoJapaneseCharBytes) {
            // Multi-kanji + せ: likely suru-verb imperative, not Ichidan
            is_suru_imperative_pattern = true;
          }
        }
      }

      if (is_te_stem_in_base_context) {
        // Strong penalty: て-ending as base form is usually wrong
        // e.g., 来てる, 食べてる should be analyzed as て-form, not Ichidan base
        base -= inflection::kPenaltyIchidanTeStemBaseInvalid;
        logConfidenceAdjustment(-inflection::kPenaltyIchidanTeStemBaseInvalid, "ichidan_te_stem_base_invalid");
      } else if (is_copula_de_pattern) {
        // Strong penalty: kanji + で is almost always copula (だ/です), not Ichidan
        // e.g., 公園で = NOUN + copula, 嫌でない = 嫌 + で + ない
        base -= inflection::kPenaltyIchidanCopulaDePattern;
        logConfidenceAdjustment(-inflection::kPenaltyIchidanCopulaDePattern, "ichidan_copula_de_pattern");
      } else if (is_suru_imperative_pattern) {
        // Strong penalty: kanji+ + せ is suru-verb imperative, not Ichidan
        // e.g., 勉強せ = 勉強する imperative, not 勉強せる
        base -= inflection::kPenaltyIchidanSuruImperativeSePattern;
        logConfidenceAdjustment(-inflection::kPenaltyIchidanSuruImperativeSePattern, "ichidan_suru_imperative_se_pattern");
      } else if (stem_len == core::kTwoJapaneseCharBytes && is_potential_context &&
          endsWithKanji(stem.substr(0, core::kJapaneseCharBytes)) && is_common_potential_ending) {
        // 読め could be Ichidan 読める or Godan potential of 読む
        // Prefer Godan potential interpretation (読む is more common than treating 読める as Ichidan)
        // Strong penalty to overcome the 0.95 cap tie
        float pen = GET_OPT(penalty_ichidan_potential_ambiguity, inflection::kPenaltyIchidanPotentialAmbiguity);
        base -= pen;
        logConfidenceAdjustment(-pen, "ichidan_potential_ambiguity");
      } else {
        float bon = GET_OPT(bonus_ichidan_e_row, inflection::kBonusIchidanERow);
        base += bon;
        logConfidenceAdjustment(bon, "ichidan_e_row");
      }
    } else {
      // Check for context-specific Godan patterns
      bool looks_godan = false;

      // Onbin context: stems ending in い, っ, ん suggest Godan
      if (required_conn == conn::kVerbOnbinkei) {
        looks_godan = endsWithChar(stem, kOnbinEndings, kOnbinCount);
      }
      // Mizenkei context: stems ending in a-row suggest Godan
      else if (required_conn == conn::kVerbMizenkei) {
        looks_godan = endsWithChar(stem, kMizenkeiEndings, kMizenkeiCount);
      }
      // Renyokei context: stems ending in i-row suggest Godan
      else if (required_conn == conn::kVerbRenyokei) {
        looks_godan = endsWithChar(stem, kRenyokeiEndings, kRenyokeiCount);
      }

      if (looks_godan) {
        // Stem matches Godan conjugation pattern for this context
        float pen = GET_OPT(penalty_ichidan_looks_godan, inflection::kPenaltyIchidanLooksGodan);
        base -= pen;
        logConfidenceAdjustment(-pen, "ichidan_looks_godan");
      }

      // Ichidan stems cannot end in u-row hiragana (う, く, す, つ, ぬ, ふ, む, る)
      // U-row endings are Godan dictionary forms (読む, 書く, 話す, etc.)
      // This prevents "読む" from being analyzed as Ichidan with base form "読むる"
      // Note: This check applies even in kVerbBase context (aux_count == 0)
      // because it's detecting a grammatically impossible pattern
      if (stem_len >= core::kJapaneseCharBytes) {
        std::string_view last_char = stem.substr(stem_len - core::kJapaneseCharBytes);
        if (last_char == "う" || last_char == "く" || last_char == "す" ||
            last_char == "つ" || last_char == "ぬ" || last_char == "ふ" ||
            last_char == "む" || last_char == "る" ||
            last_char == "ぐ" || last_char == "ず" || last_char == "づ" ||
            last_char == "ぶ" || last_char == "ぷ") {
          // Strong penalty - this pattern is grammatically impossible for Ichidan
          base -= inflection::kPenaltyIchidanURowStemInvalid;
          logConfidenceAdjustment(-inflection::kPenaltyIchidanURowStemInvalid, "ichidan_u_row_stem_invalid");
        }
      }

      // Ichidan stem ending in い (kanji + い) in renyokei context is suspicious
      // Pattern: 行い + ます → 行いる (wrong) vs 行 + います → 行う (correct)
      // Pattern: 手伝い + ます → 手伝いる (wrong) vs 手伝 + います → 手伝う (correct)
      // Stems like 行い, 手伝い (kanji + い) are more likely Godan renyokei than Ichidan
      // Exception: 用い (from 用いる) is a valid Ichidan stem, but rare
      // Apply strong penalty when stem ends with kanji + い in renyokei context
      if (required_conn == conn::kVerbRenyokei && stem_len >= core::kTwoJapaneseCharBytes) {
        std::string_view last3 = stem.substr(stem_len - core::kJapaneseCharBytes);  // Last 3 bytes = い
        std::string_view prev3 = stem.substr(stem_len - core::kTwoJapaneseCharBytes, core::kJapaneseCharBytes);  // Previous char
        if (last3 == "い" && endsWithKanji(prev3)) {
          // Stem ends with kanji + い, likely Godan renyokei misanalysis
          // Use stronger penalty than generic "looks godan"
          float pen = GET_OPT(penalty_ichidan_kanji_i, inflection::kPenaltyIchidanKanjiI);
          base -= pen;
          logConfidenceAdjustment(-pen, "ichidan_kanji_i_renyokei");
        }
      }
    }

    // Single-kanji Ichidan stems are rare but valid (見る, 着る, 寝る, etc.)
    // Problem: 殺されて can be parsed as 殺 + されて (wrong) or 殺さ + れて (correct)
    // The させられた/させられて patterns (15 bytes) are legitimate Ichidan causative-passive
    // When aux_count == 1 and aux_total_len == 15, it's likely させられた (correct)
    // When aux_count >= 2 (e.g., きた + されて), it's likely wrong
    // Exception: Simple te-form (て/た alone, aux_total_len == 3) is common for 見る, 着る
    //   - 見て, 見た are legitimate single-kanji Ichidan forms
    //   - But 話せる (せる = 6 bytes) should NOT be exempt (that's GodanSa potential)
    if (stem_len == core::kJapaneseCharBytes && endsWithKanji(stem)) {
      if (aux_count == 0) {
        // Base form like 寝る, 見る - no penalty (valid dictionary form)
      } else if (aux_count == 1 && aux_total_len >= core::kFiveJapaneseCharBytes) {
        // Single long aux match like させられた (15 bytes) or させられる (15 bytes)
        // This is legitimate Ichidan causative-passive (見させられた → 見る)
        // NOTE: Threshold is 15 bytes (5 chars) to exclude せられる (12 bytes)
        //       寄せられた (lemma: 寄せる) should NOT get this bonus
        //       見させられた (lemma: 見る) SHOULD get this bonus
        base += inflection::kBonusIchidanCausativePassive;
        logConfidenceAdjustment(inflection::kBonusIchidanCausativePassive, "ichidan_causative_passive");
      } else if (aux_count == 1 && aux_total_len == core::kJapaneseCharBytes) {
        // Simple te-form: て/た (3 bytes only)
        // These are common for 見る, 着る, 寝る, etc.
        // BUT: Ichidan te-form uses て/た, NOT で
        // で is Godan onbin te-form (読んで from 読む), not Ichidan
        // If we're in onbinkei context with Ichidan, apply strong penalty
        if (required_conn == conn::kVerbOnbinkei) {
          // Ichidan verbs don't have onbin - this is wrong analysis
          // e.g., 侍で should NOT be analyzed as Ichidan stem + で (te-form)
          base -= inflection::kPenaltyIchidanSingleKanjiOnbinInvalid;
          logConfidenceAdjustment(-inflection::kPenaltyIchidanSingleKanjiOnbinInvalid, "ichidan_single_kanji_onbin_invalid");
        }
      } else if (aux_count == 1 && aux_total_len == core::kTwoJapaneseCharBytes &&
                 required_conn == conn::kVerbRenyokei) {
        // 2-char aux with renyokei connection: とく, ちゃう, てる, etc.
        // Valid colloquial patterns for Ichidan (見とく → 見る + とく)
        // No penalty - these are legitimate contractions
        // NOTE: 3-char patterns like すぎた should still get penalty (高い + すぎた, not 高る + すぎた)
      } else if (aux_count == 1 && aux_total_len == core::kTwoJapaneseCharBytes &&
                 required_conn == conn::kVerbMizenkei) {
        // P5-4: 2-char aux with mizenkei connection: ない, ぬ, etc.
        // Valid negative forms for single-kanji Ichidan (見ない → 見る + ない)
        // No penalty - these are legitimate conjugations
      } else {
        // Multiple aux matches or longer single match (like せる, されて)
        // Likely wrong match via potential/passive pattern
        // Apply penalty to prefer the Godan interpretation
        base -= inflection::kPenaltyIchidanSingleKanjiMultiAux;
        logConfidenceAdjustment(-inflection::kPenaltyIchidanSingleKanjiMultiAux, "ichidan_single_kanji_multi_aux");
      }
    }
  }

  // Ichidan with kanji+i-row hiragana stem pattern validation
  // Stems like 人い, 玉い are unnatural for Ichidan verbs
  // Real Ichidan verbs have e-row stems (食べ, 見え, 出来) not i-row
  // Kanji + i-row patterns are likely NOUN + verb (いる) misanalysis
  // E.g., 人いる = 人 + いる (not 人い + る)
  // P5-2: Exception for specific kanji + い stems (用い, 率い, 報い) - valid kami-ichidan
  if (type == VerbType::Ichidan && stem_len == core::kTwoJapaneseCharBytes && aux_count == 0) {
    // 6 bytes = 2 chars (kanji + hiragana)
    // Check if pattern is kanji + i-row hiragana
    char32_t first_cp = utf8::decodeFirstChar(stem);
    char32_t second_cp = utf8::decode3ByteUtf8At(stem, core::kJapaneseCharBytes);
    bool first_is_kanji = kana::isKanjiCodepoint(first_cp);
    bool is_i_row = kana::isIRowCodepoint(second_cp);
    // Exception: specific known kanji + い stems are valid
    bool is_known_kanji_i_stem = isInArray(stem, inflection::kValidKanjiIStemExceptions);
    if (first_is_kanji && is_i_row && !is_known_kanji_i_stem) {
      float pen = GET_OPT(penalty_ichidan_kanji_hiragana_stem, inflection::kPenaltyIchidanKanjiHiraganaStem);
      base -= pen;
      logConfidenceAdjustment(-pen, "ichidan_kanji_i_row_stem");
    }
  }

  // Ichidan pure hiragana multi-char stem penalty
  // Multi-character pure hiragana Ichidan stems are rare:
  // - Most Ichidan verbs have kanji stems: 食べる, 見る, 起きる
  // - Pure hiragana Ichidan exists (いる, できる) but are in dictionary
  // - Stems like まじ(る), ふえ(る) in hiragana are usually not verbs
  // Exception: single-char hiragana stems (み, き) are handled separately
  // Exception: すぎ (→ すぎる) is a very common auxiliary verb pattern
  //   - Used after verb renyokei: 食べすぎる (eat too much)
  //   - Used after i-adjective stem: 高すぎる (too expensive)
  //   - Must be recognized as legitimate Ichidan verb
  // P5-3: Added exception for でき (from できる - to be able)
  bool is_valid_hiragana_stem = isInArray(stem, inflection::kValidHiraganaStemExceptions);
  if (type == VerbType::Ichidan && stem_len >= core::kTwoJapaneseCharBytes &&
      isPureHiragana(stem) && !is_valid_hiragana_stem) {
    float pen = GET_OPT(penalty_pure_hiragana_stem, inflection::kPenaltyPureHiraganaStem);
    base -= pen;
    logConfidenceAdjustment(-pen, "ichidan_pure_hiragana_stem");
  }

  // GodanRa validation: single-hiragana stems are typically Ichidan, not GodanRa
  // Verbs like みる (to see), きる (to cut/wear), にる (to boil) are Ichidan
  // Godan Ra verbs usually have at least 2 chars in stem (帰る, 走る, 取る)
  // Apply penalty to GodanRa interpretation for single-hiragana stems
  if (type == VerbType::GodanRa && stem_len == core::kJapaneseCharBytes && !endsWithKanji(stem)) {
    // Single hiragana stem (み, き, に, etc.) - likely Ichidan, not GodanRa
    base -= inflection::kPenaltyGodanRaSingleHiragana;
    logConfidenceAdjustment(-inflection::kPenaltyGodanRaSingleHiragana, "godan_ra_single_hiragana");
  }

  // In kVerbKatei (conditional) context, stems ending in i-row hiragana suggest Ichidan
  // Examples: 起き(る), 生き(る), 過ぎ(る) - Ichidan verbs with i-row stems
  // vs. 走(る), 取(る) - GodanRa verbs where stem is typically kanji-only
  // The i-row ending indicates the character is part of the Ichidan stem
  if (required_conn == conn::kVerbKatei && stem_len >= core::kTwoJapaneseCharBytes) {
    bool has_irow_ending = endsWithChar(stem, kRenyokeiEndings, kRenyokeiCount);
    if (has_irow_ending) {
      if (type == VerbType::Ichidan) {
        base += inflection::kBonusIchidanKateiIRow;
        logConfidenceAdjustment(inflection::kBonusIchidanKateiIRow, "ichidan_katei_i_row");
      } else if (type == VerbType::GodanRa) {
        base -= inflection::kPenaltyGodanRaKateiIRow;
        logConfidenceAdjustment(-inflection::kPenaltyGodanRaKateiIRow, "godan_ra_katei_i_row");
      }
    }
  }

  // GodanTa stems cannot end with onbin markers (っ, ん, い)
  // GodanTa verbs like 持つ, 立つ have stems like 持, 立
  // The っ is the onbin FORM, not part of the stem
  // E.g., 行っ + てた → 行っつ (wrong) - 行っ is onbin of 行く (GodanKa), not GodanTa
  if (type == VerbType::GodanTa && stem_len >= core::kJapaneseCharBytes) {
    std::string_view last_char = stem.substr(stem_len - core::kJapaneseCharBytes);
    if (last_char == "っ" || last_char == "ん" || last_char == "い") {
      base -= inflection::kPenaltyGodanTaOnbinStemInvalid;
      logConfidenceAdjustment(-inflection::kPenaltyGodanTaOnbinStemInvalid, "godan_ta_onbin_stem_invalid");
    }
    // GodanTa uses った for te-form onbin, not てた
    // 見てた should be Ichidan 見る, not GodanTa 見つ
    // GodanTa te-form: 持つ → 持った → 持ってた
    // If aux starts with て (not っ), it's wrong for GodanTa
    if (required_conn == conn::kVerbRenyokei && aux_total_len > 0) {
      base -= inflection::kPenaltyGodanTaTeAuxInvalid;
      logConfidenceAdjustment(-inflection::kPenaltyGodanTaTeAuxInvalid, "godan_ta_te_aux_invalid");
    }
  }

  // GodanWa disambiguation for っ-onbin patterns with all-kanji stems
  // Three verb types share っ-onbin: GodanWa (買う), GodanRa (取る), GodanTa (持つ)
  // For multi-kanji stems (2+ kanji), GodanWa is much more common:
  //   - 手伝う, 買い求う, 争う, 戦う, 追い払う (all GodanWa)
  //   - GodanRa verbs rarely have 2+ kanji stems
  //   - GodanTa verbs rarely have 2+ kanji stems
  // Single-kanji stems are ambiguous: 取る (GodanRa), 持つ (GodanTa), 買う (GodanWa)
  // Hiragana stems like いらっしゃ (→ いらっしゃる GodanRa) should NOT be affected
  if (required_conn == conn::kVerbOnbinkei && stem_len >= core::kTwoJapaneseCharBytes && isAllKanji(stem)) {
    if (type == VerbType::GodanWa) {
      base += inflection::kBonusGodanWaMultiKanji;
      logConfidenceAdjustment(inflection::kBonusGodanWaMultiKanji, "godan_wa_multi_kanji");
    } else if (type == VerbType::GodanRa || type == VerbType::GodanTa) {
      base -= inflection::kPenaltyGodanRaTaMultiKanji;
      logConfidenceAdjustment(-inflection::kPenaltyGodanRaTaMultiKanji, "godan_ra_ta_multi_kanji");
    }
  }

  // Kuru validation: only 来る/くる conjugates as Kuru
  // Valid Kuru stems:
  // - "来" (kanji form: 来なかった → 来る)
  // - "" (empty, when suffix is こ/き: こなかった → くる)
  if (type == VerbType::Kuru) {
    if (stem != inflection::kKuruKanji && !stem.empty()) {
      // Any stem other than 来 or empty is invalid for Kuru
      base -= inflection::kPenaltyKuruInvalidStem;
      logConfidenceAdjustment(-inflection::kPenaltyKuruInvalidStem, "kuru_invalid_stem");
    }
  }

  // Suru/Kuru imperative boost: しろ, せよ, こい have empty stems
  // These must win over competing Ichidan/Godan interpretations
  // (しろ vs しる, こい vs こう)
  if (stem.empty() && required_conn == conn::kVerbMeireikei &&
      (type == VerbType::Suru || type == VerbType::Kuru)) {
    base += inflection::kBonusSuruKuruImperative;
    logConfidenceAdjustment(inflection::kBonusSuruKuruImperative, "suru_kuru_imperative");
  }

  // Ichidan validation: reject base forms that would be irregular verbs
  // くる (来る) is カ変, not 一段. Stem く + る = くる is INVALID for Ichidan.
  // する is サ変, not 一段. Stem す + る = する is INVALID for Ichidan.
  // こる is not a valid verb - こ is Kuru mizenkei suffix, not Ichidan stem.
  // E.g., くなかった should NOT be parsed as Ichidan く + なかった = くる
  // E.g., こなかった should NOT be parsed as Ichidan こ + なかった = こる
  if (type == VerbType::Ichidan && stem_len == core::kJapaneseCharBytes) {
    if (isInArray(stem, inflection::kInvalidIchidanSingleStems)) {
      float pen = GET_OPT(penalty_ichidan_irregular_stem, inflection::kPenaltyIchidanIrregularStem);
      base -= pen;
      logConfidenceAdjustment(-pen, "ichidan_irregular_stem");
    }
  }

  // Ichidan single-hiragana particle stem penalty
  // In mizenkei context, single-hiragana stems that are common particles
  // should be heavily penalized. E.g., もない = も(PARTICLE) + ない(AUX),
  // NOT もる(VERB) + ない. Common particles: も, は, が, を, に, へ, と, で, よ, ね, わ, な
  if (type == VerbType::Ichidan && stem_len == core::kJapaneseCharBytes &&
      required_conn == conn::kVerbMizenkei && !endsWithKanji(stem)) {
    // Check if stem is a common particle
    if (isInArray(stem, inflection::kParticleStemList)) {
      float pen = GET_OPT(penalty_ichidan_single_hiragana_particle, inflection::kPenaltyIchidanSingleHiraganaParticleStem);
      base -= pen;
      logConfidenceAdjustment(-pen, "ichidan_single_hiragana_particle_stem");
    }
  }

  // Particle + な stem penalty for GodanWa
  // E.g., もない → もなう is not a real verb. The pattern is も(PARTICLE) + ない(AUX).
  // Stems like もな, はな, がな where first char is a particle are very suspicious
  // for GodanWa verbs. Apply strong penalty.
  if (type == VerbType::GodanWa && stem_len == core::kTwoJapaneseCharBytes && !containsKanji(stem)) {
    std::string_view first = stem.substr(0, core::kJapaneseCharBytes);
    std::string_view second = stem.substr(core::kJapaneseCharBytes);
    // If first char is a common particle and second is な, this is likely
    // a misparse of PARTICLE + ない(adjective/aux)
    if (second == "な" && isInArray(first, inflection::kParticleNaStemPrefixes)) {
      base -= inflection::kPenaltyGodanWaParticleNaStem;
      logConfidenceAdjustment(-inflection::kPenaltyGodanWaParticleNaStem, "godan_wa_particle_na_stem");
    }
  }

  // Single-hiragana stem penalty for Godan verbs (non-Ra/Wa)
  // Single-char hiragana stems like ま(む), む(ぐ) are almost never real verbs
  // Real single-char verbs (み, き, に for ichidan) are handled separately
  // Exception: GodanRa has separate handling (godan_ra_single_hiragana)
  // Exception: い(く) is a valid GodanKa verb (行く)
  // GodanWa with single hiragana stem (ら→らう, ま→まう) are typically not real verbs
  // Real GodanWa verbs like 買う, 舞う use kanji stems
  bool is_godan_non_ra = (type == VerbType::GodanKa || type == VerbType::GodanGa ||
                          type == VerbType::GodanSa || type == VerbType::GodanTa ||
                          type == VerbType::GodanNa || type == VerbType::GodanBa ||
                          type == VerbType::GodanMa || type == VerbType::GodanWa);
  if (is_godan_non_ra && stem_len == core::kJapaneseCharBytes && !containsKanji(stem)) {
    // Exception: い(く) = 行く is valid
    if (!(type == VerbType::GodanKa && stem == "い")) {
      float pen = GET_OPT(penalty_godan_single_hiragana_stem, inflection::kPenaltyGodanSingleHiraganaStem);
      base -= pen;
      logConfidenceAdjustment(-pen, "godan_single_hiragana_stem");
    }
  }

  // Godan (Ma/Ga/Na/Ba) pure hiragana multi-char stem penalty
  // These types rarely have legitimate hiragana-only verbs:
  // - GodanMa: 読む, 飲む - always in kanji; no coined verbs
  // - GodanGa: 泳ぐ, 注ぐ - always in kanji; no coined verbs
  // - GodanNa: only 死ぬ exists
  // - GodanBa: 飛ぶ, 遊ぶ - always in kanji
  // GodanKa/Sa/Ta excluded - いく, なくす, もつ are common in hiragana
  bool is_godan_hiragana_rare = (type == VerbType::GodanMa || type == VerbType::GodanGa ||
                                  type == VerbType::GodanNa || type == VerbType::GodanBa);
  if (is_godan_hiragana_rare && stem_len >= core::kTwoJapaneseCharBytes &&
      isPureHiragana(stem)) {
    float pen = GET_OPT(penalty_godan_non_ra_pure_hiragana, inflection::kPenaltyGodanNonRaPureHiraganaStem);
    base -= pen;
    logConfidenceAdjustment(-pen, "godan_hiragana_rare_stem");
  }

  // GodanNa (ナ行五段) is extremely rare - only 死ぬ exists in modern Japanese
  // In ん-onbin context, penalize GodanNa to prefer GodanMa/GodanBa
  // E.g., 跳んだ → 跳ぶ (GodanBa) is correct, not 跳ぬ (GodanNa)
  // E.g., 読んだ → 読む (GodanMa) is correct, not 読ぬ (GodanNa)
  // Exception: 死ぬ (to die) is the only GodanNa verb - BOOST 死 stem instead
  // This applies to single-kanji stems in onbinkei context
  if (type == VerbType::GodanNa && required_conn == conn::kVerbOnbinkei) {
    // Exception for 死ぬ - the only GodanNa verb in modern Japanese
    // Boost 死 stem to beat GodanMa/GodanBa interpretations
    if (stem == "死") {
      base += inflection::scale::kMinorBonus;
      logConfidenceAdjustment(inflection::scale::kMinorBonus, "godan_na_shinu_boost");
    } else {
      base -= inflection::kPenaltyGodanNaRare;
      logConfidenceAdjustment(-inflection::kPenaltyGodanNaRare, "godan_na_rare");
    }
  }

  // I-adjective validation: single-kanji stems are very rare
  // Most I-adjectives have multi-character stems (美しい, 高い, 長い)
  // Single-kanji stems like 書い (from mismatched 書く) are usually wrong
  if (type == VerbType::IAdjective && stem_len == core::kJapaneseCharBytes) {
    float pen = GET_OPT(penalty_i_adj_single_kanji, inflection::kPenaltyIAdjSingleKanji);
    base -= pen;
    logConfidenceAdjustment(-pen, "i_adj_single_kanji");
  }

  // I-adjective stems containing verb+auxiliary patterns are not real adjectives
  // E.g., 食べすぎてしま (from 食べすぎてしまい) is verb+auxiliary, not adjective
  // Pattern: stem contains てしま/でしま (te-form + shimau) → verb compound
  // Pattern: stem contains ている/でいる (te-form + iru) → verb compound
  // Pattern: stem contains てお/でお (te-form + oku) → verb compound
  if (type == VerbType::IAdjective && stem_len >= core::kFourJapaneseCharBytes) {
    // Check for common auxiliary patterns in the stem
    bool has_aux_pattern =
        stem.find("てしま") != std::string_view::npos ||
        stem.find("でしま") != std::string_view::npos ||
        stem.find("ている") != std::string_view::npos ||
        stem.find("でいる") != std::string_view::npos ||
        stem.find("ておい") != std::string_view::npos ||
        stem.find("でおい") != std::string_view::npos ||
        stem.find("てき") != std::string_view::npos ||  // てくる pattern
        stem.find("でき") != std::string_view::npos;
    if (has_aux_pattern) {
      float pen = GET_OPT(penalty_i_adj_verb_aux_pattern, inflection::kPenaltyIAdjVerbAuxPattern);
      base -= pen;
      logConfidenceAdjustment(-pen, "i_adj_verb_aux_pattern");
      // Note: This penalty may be clamped by the floor at return.
      // Additional penalty is applied in scorer.cpp for lattice cost.
    }
  }

  // I-adjective stems ending with "し" are very common (難しい, 美しい, 楽しい, 苦しい)
  // When followed by すぎる/やすい/にくい auxiliaries, boost confidence
  // This helps disambiguate 難しすぎる (難しい + すぎる) vs 難す (Godan-Sa)
  if (type == VerbType::IAdjective && stem_len >= core::kTwoJapaneseCharBytes && aux_count >= 1) {
    std::string_view last = stem.substr(stem_len - core::kJapaneseCharBytes);
    if (last == "し") {
      base += inflection::kBonusIAdjShiiStem;
      logConfidenceAdjustment(inflection::kBonusIAdjShiiStem, "i_adj_shii_stem");
    }
  }

  // Boost for verb renyokei + やすい/にくい compound adjective patterns
  // E.g., 読みやすい (easy to read), 使いにくい (hard to use)
  // The stem will be verb_renyokei + やす/にく (e.g., 読みやす, 使いにく)
  if (type == VerbType::IAdjective && stem_len >= core::kThreeJapaneseCharBytes) {
    std::string_view last6 = stem.substr(stem_len - core::kTwoJapaneseCharBytes);
    if (last6 == "やす" || last6 == "にく") {
      // Check if the part before やす/にく ends with verb renyokei marker
      std::string_view before = stem.substr(0, stem_len - core::kTwoJapaneseCharBytes);
      // Use centralized renyokei marker check (i-row for godan, e-row for ichidan)
      if (endsWithRenyokeiMarker(before)) {
        float bon = GET_OPT(bonus_i_adj_compound_yasui_nikui, inflection::kBonusIAdjCompoundYasuiNikui);
        base += bon;
        logConfidenceAdjustment(bon, "i_adj_compound_yasui_nikui");
      }
    }
  }

  // I-adjective stems consisting only of 3+ kanji are extremely rare
  // Such stems are usually サ変名詞 (検討, 勉強, 準備) being misanalyzed
  // Real i-adjectives have patterns like: 美しい, 楽しい (kanji + hiragana)
  // This prevents "検討いたす" from being parsed as "検討い" + "たす"
  // Exception: 2-kanji stems (6 bytes) can be valid adjectives:
  //   面白い (おもしろい), 可愛い (かわいい), 美味い (うまい)
  if (type == VerbType::IAdjective && stem_len >= core::kThreeJapaneseCharBytes && isAllKanji(stem)) {
    base -= inflection::kPenaltyIAdjAllKanji;
    logConfidenceAdjustment(-inflection::kPenaltyIAdjAllKanji, "i_adj_all_kanji");
  }

  // I-adjective stems ending with e-row hiragana are extremely rare
  // E-row endings (食べ, 見え, 教え) are typical of ichidan verb stems
  // This prevents "食べそう" from being parsed as i-adjective "食べい"
  if (type == VerbType::IAdjective && endsWithERow(stem)) {
    float pen = GET_OPT(penalty_i_adj_e_row_stem, inflection::kPenaltyIAdjERowStem);
    base -= pen;
    logConfidenceAdjustment(-pen, "i_adj_e_row_stem");
  }

  // I-adjective stems ending with "るらし" or "いらし" are likely verb/adj + rashii pattern
  // E.g., 帰るらし + い → should be 帰る + らしい (conjecture auxiliary)
  // E.g., 帰りたいらし + い → should be 帰りたい + らしい
  // This penalty helps split the compound correctly
  if (type == VerbType::IAdjective && stem_len >= core::kThreeJapaneseCharBytes) {
    std::string_view last9 = stem.substr(stem_len - core::kThreeJapaneseCharBytes);
    if (last9 == "るらし" || last9 == "いらし") {
      float pen = GET_OPT(penalty_i_adj_verb_rashii_pattern, inflection::kPenaltyIAdjVerbRashiiPattern);
      base -= pen;
      logConfidenceAdjustment(-pen, "i_adj_verb_rashii_pattern");
    }
  }

  // I-adjective stems ending with "づ" are invalid
  // "づ" endings are verb onbin patterns (基づ + いて → 基づいて from 基づく)
  // No real i-adjective has a stem ending in づ
  if (type == VerbType::IAdjective && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last = stem.substr(stem_len - core::kJapaneseCharBytes);
    if (last == "づ") {
      base -= inflection::kPenaltyIAdjZuStemInvalid;
      logConfidenceAdjustment(-inflection::kPenaltyIAdjZuStemInvalid, "i_adj_zu_stem_invalid");
    }
  }

  // I-adjective stems ending with a-row hiragana (な, ま, か, etc.) are suspicious
  // These are typically verb mizenkei forms + ない (食べな, 読ま, 書か)
  // This prevents "食べなければ" from being parsed as i-adjective "食べない"
  // Real i-adjectives with ない: 危ない (あぶな), 少ない (すくな)
  // But these have specific patterns, not random verb stem + な
  if (type == VerbType::IAdjective && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last = stem.substr(stem_len - core::kJapaneseCharBytes);
    if (last == "な" || last == "ま" || last == "か" || last == "が" ||
        last == "さ" || last == "た" || last == "ば" || last == "ら" ||
        last == "わ") {
      // 2-character pure hiragana stems ending in ら are typically verb mizenkei
      // E.g., やら (from やる) + さ + れた = やらされた (causative-passive)
      // Only penalize ら endings - other a-row endings may be valid i-adj stems
      // E.g., やば (やばい), なさ (なさい with そう) are valid i-adjectives
      if (stem_len == core::kTwoJapaneseCharBytes && isPureHiragana(stem) &&
          last == "ら") {
        base -= inflection::kPenaltyIAdjMizenkeiPattern;
        logConfidenceAdjustment(-inflection::kPenaltyIAdjMizenkeiPattern, "i_adj_2char_ra_stem");
      }
      // Check if there's a hiragana before the a-row ending (verb+mizenkei pattern)
      // E.g., 食べ + な → 食べな (ichidan verb pattern)
      //       行 + か + な → 行かな (godan verb mizenkei + な)
      // vs. 危 + な → あぶな (real adjective stem)
      else if (stem_len >= core::kThreeJapaneseCharBytes) {
        std::string_view prev = stem.substr(stem_len - core::kTwoJapaneseCharBytes, core::kJapaneseCharBytes);
        // If previous char is hiragana, this looks like verb mizenkei
        // Include all rows: a-row (godan mizenkei), e-row (ichidan), i-row, etc.
        if (prev == "べ" || prev == "め" || prev == "せ" || prev == "け" ||
            prev == "て" || prev == "ね" || prev == "れ" || prev == "え" ||
            prev == "げ" || prev == "ぜ" || prev == "で" || prev == "ぺ" ||
            prev == "み" || prev == "き" || prev == "し" || prev == "ち" ||
            prev == "に" || prev == "ひ" || prev == "り" || prev == "い" ||
            prev == "ぎ" || prev == "じ" || prev == "ぢ" || prev == "び" ||
            prev == "ぴ" ||
            // a-row: godan verb mizenkei markers (書か, 読ま, 行か, etc.)
            prev == "か" || prev == "が" || prev == "さ" || prev == "ざ" ||
            prev == "た" || prev == "だ" || prev == "な" || prev == "ば" ||
            prev == "ぱ" || prev == "ま" || prev == "ら" || prev == "わ" ||
            prev == "あ" || prev == "は") {
          base -= inflection::kPenaltyIAdjMizenkeiPattern;
          logConfidenceAdjustment(-inflection::kPenaltyIAdjMizenkeiPattern, "i_adj_mizenkei_pattern");
        }
      }
    }
  }

  // I-adjective stems that look like godan verb renyokei (kanji + i-row)
  // Pattern: 書き, 読み, 飲み (2 chars = 6 bytes, ends with i-row hiragana)
  // These are typical godan verb stems, not i-adjective stems
  // This prevents "書きすぎる" from being parsed as i-adjective "書きい"
  if (type == VerbType::IAdjective && stem_len == core::kTwoJapaneseCharBytes) {
    std::string_view last = stem.substr(core::kJapaneseCharBytes);  // Last 3 bytes = 1 hiragana
    // き: Apply penalty for godan renyokei pattern (書き, 聞き, etc.)
    //     Exception: 大きい is a real adjective - stem is exactly "大き"
    // し: Excluded - common in real i-adj stems like 美し, 楽し (handled elsewhere)
    if (last == "き") {
      std::string_view first = stem.substr(0, core::kJapaneseCharBytes);
      // Only 大き is a valid adjective stem ending in き
      if (first != "大" && endsWithKanji(first)) {
        base -= inflection::kPenaltyIAdjGodanRenyokeiPattern;
        logConfidenceAdjustment(-inflection::kPenaltyIAdjGodanRenyokeiPattern, "i_adj_godan_renyokei_ki");
      }
    } else if (last == "ぎ" || last == "ち" ||
               last == "に" || last == "び" || last == "み" || last == "り" ||
               last == "い") {
      // Check if first char is kanji (typical verb renyokei pattern)
      if (endsWithKanji(stem.substr(0, core::kJapaneseCharBytes))) {
        base -= inflection::kPenaltyIAdjGodanRenyokeiPattern;
        logConfidenceAdjustment(-inflection::kPenaltyIAdjGodanRenyokeiPattern, "i_adj_godan_renyokei_pattern");
      }
    }
    // Single-kanji + な stems are usually verb negatives, not adjectives
    // E.g., 見なければ → 見ない (verb negative), not adjective
    //       来なければ → 来ない (verb negative), not adjective
    // Exceptions: 少ない, 危ない are true adjectives (finite, small set)
    // Also penalize hiragana + な (しな, こな = suru/kuru negative)
    if (last == "な") {
      std::string_view first = stem.substr(0, core::kJapaneseCharBytes);
      if (!endsWithKanji(first)) {
        // Hiragana + な (verb mizenkei like しな, こな)
        base -= inflection::kPenaltyIAdjVerbNegativeNa;
        logConfidenceAdjustment(-inflection::kPenaltyIAdjVerbNegativeNa, "i_adj_verb_negative_na_hiragana");
      } else if (first != "少" && first != "危") {
        // Single kanji + な that's NOT a known adjective stem
        // Most are verb negatives (見な, 出な, 来な, 寝な, etc.)
        base -= inflection::kPenaltyIAdjVerbNegativeNa;
        logConfidenceAdjustment(-inflection::kPenaltyIAdjVerbNegativeNa, "i_adj_verb_negative_na_kanji");
      }
    }
  }

  // Godan verb stems in onbinkei context should not end with a-row hiragana
  // a-row endings (か, が, さ, etc.) are mizenkei forms, not onbinkei
  // This prevents "美しかった" from being parsed as verb "美しかる"
  // Exception: GodanSa has no phonetic change (音便) - し is the renyokei form
  // used in て-form context. Stems like いた (from いたす) or はな (from はなす)
  // can legitimately end with any hiragana, including a-row characters.
  // Exception: GodanRa with わ-ending stems (終わる, 変わる, 代わる, etc.)
  // These verbs have わ as part of the stem: 終わ + った = 終わった
  if (required_conn == conn::kVerbOnbinkei && stem_len >= core::kTwoJapaneseCharBytes &&
      type != VerbType::GodanSa) {
    std::string_view last = stem.substr(stem_len - core::kJapaneseCharBytes);
    // Skip penalty for GodanRa with わ ending - legitimate pattern for 終わる etc.
    bool is_godan_ra_wa = (type == VerbType::GodanRa && last == "わ");
    if (!is_godan_ra_wa &&
        (last == "か" || last == "が" || last == "さ" || last == "た" ||
         last == "な" || last == "ば" || last == "ま" || last == "ら" ||
         last == "わ")) {
      // Stems ending in a-row are suspicious for onbinkei context
      base -= inflection::kPenaltyOnbinkeiARowStem;
      logConfidenceAdjustment(-inflection::kPenaltyOnbinkeiARowStem, "onbinkei_a_row_stem");
    }
  }

  // Penalty for Godan with e-row stem ending in onbinkei context
  // Pattern like 伝えいた matches GodanKa (伝え + いた → 伝えく)
  // But stems ending in e-row are almost always Ichidan verb renyokei forms
  // Real Godan onbin: 書いた (書く), 飲んだ (飲む) - stems end in kanji
  // This prevents "伝えいた" from being parsed as GodanKa "伝えく"
  if (required_conn == conn::kVerbOnbinkei && stem_len >= core::kTwoJapaneseCharBytes &&
      endsWithERow(stem) && type != VerbType::Ichidan) {
    // E-row endings are ichidan stems, not godan
    // 伝え, 食べ, 見せ are all ichidan renyokei forms
    base -= inflection::kPenaltyOnbinkeiERowNonIchidan;
    logConfidenceAdjustment(-inflection::kPenaltyOnbinkeiERowNonIchidan, "onbinkei_e_row_non_ichidan");
  }

  // Single-kanji Godan stems in onbinkei context need careful handling
  // GodanKa/GodanGa have い音便: 書く→書いて (aux=いて), 泳ぐ→泳いで (aux=いで)
  // Ichidan have no 音便: 用いる→用いて (aux=て)
  // When stem is single kanji and aux is just て/た (3 bytes), it's likely
  // the input is actually an Ichidan verb like 用いる (stem=用い, aux=て)
  // When aux is いて/いた/いで/いだ (6 bytes), it's legitimate GodanKa/GodanGa
  // This prevents "用いて" (stem=用, aux=いて) from being parsed as GodanKa "用く"
  // But allows "書いて" (stem=書, aux=いて) to be correctly parsed as GodanKa "書く"
  // Note: aux_total_len includes all auxiliary suffixes, not just the first one
  // For simple te-form: aux_total_len is 6 for いて, 3 for て

  // Multi-kanji stems (2+ kanji only) are almost always サ変名詞
  // Such stems should only be parsed as Suru verbs, not Godan or Ichidan
  // This prevents "検討いた" from being parsed as GodanKa "検討く"
  // Exception: kVerbKatei (conditional form like 頑張れば) is less ambiguous
  // because the ば-form has a distinct pattern that サ変名詞 doesn't have
  // Exception: っ-onbin verbs (GodanWa/Ra/Ta) are legitimate with 2-kanji stems
  //   - 手伝う → 手伝って (GodanWa) is valid, not サ変
  //   - But 検討 + いて could be サ変 (検討している) or GodanKa (検討く - wrong)
  // Exception: GodanSa verbs with 2-kanji stems exist (目指す, 見逃す, etc.)
  //   - These are NOT サ変名詞, they are true Godan verbs ending in す
  // Skip IAdjective - it has separate handling at line 246-254
  if (stem_len >= core::kTwoJapaneseCharBytes && isAllKanji(stem) && type != VerbType::Suru &&
      type != VerbType::IAdjective) {
    // Skip penalty for っ-onbin verbs (GodanWa/Ra/Ta) in onbinkei context
    // These are legitimate Godan verbs, not サ変名詞 misanalyses
    bool is_tsu_onbin_type = (type == VerbType::GodanWa ||
                              type == VerbType::GodanRa ||
                              type == VerbType::GodanTa);
    // GodanSa verbs like 目指す, 見逃す have 2-kanji stems
    // These should NOT be penalized as サ変名詞
    if (type == VerbType::GodanSa) {
      // GodanSa with 2-kanji stem: use lighter penalty (same as kVerbKatei)
      // This allows 目指す, 見逃す to compete fairly with サ変 interpretations
      base -= inflection::kPenaltyAllKanjiNonSuruKatei;
      logConfidenceAdjustment(-inflection::kPenaltyAllKanjiNonSuruKatei, "all_kanji_godan_sa");
    } else if (required_conn == conn::kVerbOnbinkei && is_tsu_onbin_type) {
      // No penalty for っ-onbin patterns - these are legitimate Godan verbs
    } else if (required_conn == conn::kVerbKatei) {
      // Lighter penalty for conditional form - 頑張れば, 滑れば are valid Godan
      base -= inflection::kPenaltyAllKanjiNonSuruKatei;
      logConfidenceAdjustment(-inflection::kPenaltyAllKanjiNonSuruKatei, "all_kanji_non_suru_katei");
    } else if (required_conn == conn::kVerbRenyokei && aux_total_len >= core::kTwoJapaneseCharBytes) {
      // Lighter penalty for polite form (renyokei + ます/います)
      // E.g., 手伝います, 書きます - clearly verb conjugations
      // kTwoJapaneseCharBytes covers います/ます patterns
      base -= inflection::kPenaltyAllKanjiNonSuruKatei;
      logConfidenceAdjustment(-inflection::kPenaltyAllKanjiNonSuruKatei, "all_kanji_non_suru_renyokei_masu");
    } else if (type == VerbType::Ichidan) {
      // Lighter penalty for Ichidan verbs with kanji stems
      // Unlike Godan, Ichidan verbs commonly have kanji-only stems: 出来る, 居る
      // E.g., 出来まい should recognize 出来る (Ichidan), not 出来する (Suru)
      base -= inflection::kPenaltyAllKanjiNonSuruKatei;
      logConfidenceAdjustment(-inflection::kPenaltyAllKanjiNonSuruKatei, "all_kanji_non_suru_ichidan");
    } else {
      base -= inflection::kPenaltyAllKanjiNonSuruOther;
      logConfidenceAdjustment(-inflection::kPenaltyAllKanjiNonSuruOther, "all_kanji_non_suru_other");
    }
  }

  // Godan potential form boost: 書けない → 書く is more likely than 書ける
  // Potential forms of Godan verbs behave like Ichidan, creating ambiguity
  // Only boost when:
  // 1. stem length is 1 char (3 bytes) - typical for potential forms
  // 2. Auxiliary chain has more than just る (aux_total_len > 3)
  // 3. Single auxiliary (aux_count == 1) - compound patterns like てもらう are
  //    more likely Ichidan て-form, not Godan potential
  // This prevents false matches like 食べる → 食ぶ potential (should be Ichidan)
  // Pattern "Xえる" or "Xべる" is much more likely Ichidan than Godan potential base
  if (required_conn == conn::kVerbPotential && stem_len == core::kJapaneseCharBytes &&
      aux_total_len > core::kJapaneseCharBytes && aux_count == 1) {
    if (type != VerbType::Ichidan && type != VerbType::Suru &&
        type != VerbType::Kuru) {
      base += inflection::kBonusGodanPotential;
      logConfidenceAdjustment(inflection::kBonusGodanPotential, "godan_potential");
    }
  }

  // Penalty for GodanBa potential interpretation
  // GodanBa verbs (飛ぶ, 呼ぶ, 遊ぶ, etc.) are rare compared to Ichidan verbs ending in べる
  // (食べる, 調べる, 比べる, etc.). When we see stem + べ in potential context,
  // it's much more likely to be Ichidan than GodanBa potential.
  // Example: 食べなくなった → 食べる (Ichidan) not 食ぶ (non-existent GodanBa)
  if (required_conn == conn::kVerbPotential && type == VerbType::GodanBa) {
    base -= inflection::kPenaltyGodanBaPotential;
    logConfidenceAdjustment(-inflection::kPenaltyGodanBaPotential, "godan_ba_potential");
  }

  // Penalty for Godan potential with single-kanji stem in compound patterns
  // For simple patterns like "書けない" (aux_count=1), Godan potential is often correct
  // For compound patterns like "食べてもらった" (aux_count>=2), Ichidan is usually correct
  // The べ/え in "食べ" is part of the Ichidan stem, not a potential suffix
  // Penalty scales with aux_count to handle very long compound patterns
  if (required_conn == conn::kVerbPotential && stem_len == core::kJapaneseCharBytes && aux_count >= 2) {
    if (type != VerbType::Ichidan && type != VerbType::Suru &&
        type != VerbType::Kuru) {
      // Scale penalty with compound depth
      float penalty = inflection::kPenaltyPotentialCompoundBase +
                      inflection::kPenaltyPotentialCompoundPerAux *
                          static_cast<float>(aux_count - 1);
      float capped_penalty = std::min(penalty, inflection::kPenaltyPotentialCompoundMax);
      base -= capped_penalty;
      logConfidenceAdjustment(-capped_penalty, "potential_compound");
    }
  }

  // Penalty for short te-form only matches (て/で alone) with noun-like stems
  // When the only auxiliary is "て" or "で" (3 bytes), it's often a particle, not verb conjugation
  // Pattern: 幸いで → 幸いる (WRONG) vs 幸い + で (particle)
  // But: 食べて → 食べる (CORRECT), やって → やる (CORRECT) are valid
  // Only apply to stems ending in "い" which are typically na-adjectives
  if (type == VerbType::Ichidan && required_conn == conn::kVerbOnbinkei &&
      aux_count == 1 && aux_total_len == core::kJapaneseCharBytes && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last = stem.substr(stem_len - core::kJapaneseCharBytes);

    // Stems ending in "い" are likely na-adjectives (幸い, 厄介, etc.)
    // These should be parsed as noun + particle, not verb conjugation
    if (last == "い") {
      base -= inflection::kPenaltyTeFormNaAdjective;
      logConfidenceAdjustment(-inflection::kPenaltyTeFormNaAdjective, "te_form_na_adjective");
    }
  }

  // Penalty for Ichidan stems that look like noun + い pattern in mizenkei context
  // 間違いない → 間違い(NOUN) + ない(AUX), not 間違い + ない = 間違いる(VERB)
  // 違いない → 違い(NOUN) + ない(AUX), not 違いる(VERB)
  // Pattern: stem ends with kanji + い, often a noun form of a verb
  // Common noun patterns: 間違い (from 間違う), 違い (from 違う), 誤り(異なり)...
  // These should be analyzed as NOUN + ない, not Ichidan verb conjugation
  if (type == VerbType::Ichidan && required_conn == conn::kVerbMizenkei &&
      stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last = stem.substr(stem_len - core::kJapaneseCharBytes);
    if (last == "い") {
      // Check if the character before い is kanji (common noun pattern)
      std::string_view prev = stem.substr(stem_len - core::kTwoJapaneseCharBytes, core::kJapaneseCharBytes);
      if (endsWithKanji(prev)) {
        // This is likely kanji + い noun pattern, not Ichidan verb
        // 間違い, 違い, 争い, 戦い etc. are all nouns
        base -= inflection::kPenaltyIchidanNounIMizenkei;
        logConfidenceAdjustment(-inflection::kPenaltyIchidanNounIMizenkei, "ichidan_noun_i_mizenkei");
      }
    }
  }

  // Reject Suru stems ending with onbin markers (っ, ん, い)
  // E.g., "読んする" is not valid - 読ん is Godan onbin form, not suru stem
  // Suru verbs don't have onbin forms; the し renyokei is used instead
  // This check applies regardless of kanji/hiragana ending
  if (type == VerbType::Suru && stem_len >= core::kTwoJapaneseCharBytes &&
      required_conn == conn::kVerbOnbinkei) {
    std::string_view last_char = stem.substr(stem_len - core::kJapaneseCharBytes);
    if (last_char == "っ" || last_char == "ん" || last_char == "い") {
      base -= inflection::kPenaltySuruOnbinStemInvalid;
      logConfidenceAdjustment(-inflection::kPenaltySuruOnbinStemInvalid, "suru_onbin_stem_invalid");
    }
  }

  // Reject Suru negative directly attached to kanji stem (問題ない → 問題する + ない)
  // True suru negatives have し between noun and ない: 勉強しない, not *勉強ない
  // Pattern like 問題ない should be analyzed as 問題(NOUN) + ない(ADJ), not suru negative
  //
  // Detection: Compare suffix_len (verb ending + auxiliaries) with aux_total_len
  // - 勉強しない: suffix="しない" (9 bytes), aux_len=6, verb_ending=し (3 bytes)
  //   suffix_len > aux_total_len means verb ending is present (valid)
  // - 問題ない: suffix="ない" (6 bytes), aux_len=6, verb_ending="" (0 bytes)
  //   suffix_len == aux_total_len means empty verb ending (invalid for mizenkei+ない)
  if (type == VerbType::Suru && required_conn == conn::kVerbMizenkei && isAllKanji(stem)) {
    // Check if verb ending is empty (suffix_len == aux_total_len)
    // Empty verb ending means direct noun + ない attachment, which is invalid
    // Valid patterns: しない, しなかった, しなくて, しなければ (all have し verb ending)
    // Valid patterns with empty suffix: された, させた (さ/せ is part of aux pattern)
    //
    // aux_len == 6 (ない) or 12 (なかった) with empty verb suffix is invalid
    // aux_len >= 9 with empty suffix is likely valid (された=9, させた=9, etc.)
    bool has_empty_verb_suffix = (suffix_len == aux_total_len);
    bool is_direct_nai_pattern = has_empty_verb_suffix &&
        (aux_total_len == 6 || aux_total_len == 12);  // ない or なかった
    if (is_direct_nai_pattern) {
      base -= inflection::kPenaltySuruDirectNai;
      logConfidenceAdjustment(-inflection::kPenaltySuruDirectNai, "suru_direct_nai");
    }
  }

  // Suru vs GodanSa disambiguation
  // Multi-kanji stems strongly suggest サ変 verb (勉強する, 準備する)
  // Single-kanji stems (出す, 消す) are typically GodanSa
  // Stems ending in hiragana/katakana suggest Godan verb (話す, 返す)
  if (endsWithKanji(stem)) {
    // In kVerbBase context, Suru with short suffix (す only = 3 bytes) is suspicious
    // Suru base form is normally "する" (suffix="する", 6 bytes)
    // The "す" suffix pattern is for すべき (classical) only
    // When stem + す = 目指す, this should be GodanSa, not Suru
    // Penalize Suru with 3-byte suffix in kVerbBase context
    if (type == VerbType::Suru && required_conn == conn::kVerbBase &&
        suffix_len == core::kJapaneseCharBytes) {
      // Strong penalty: Suru verbs don't end in just す in base form
      // 勉強する (not 勉強す), 準備する (not 準備す)
      base -= inflection::kPenaltyGodanSaTwoKanji;  // Reuse existing constant
      logConfidenceAdjustment(-inflection::kPenaltyGodanSaTwoKanji, "suru_short_suffix_base");
    }

    bool is_shi_context = (required_conn == conn::kVerbRenyokei ||
                           required_conn == conn::kVerbOnbinkei);
    if (is_shi_context) {
      // Only apply Suru boost for 2-kanji stems (6 bytes)
      // Single-kanji stems (3 bytes) like 出す, 消す are GodanSa
      // Longer stems (9+ bytes) might be verb compounds (考え直す)
      if (stem_len == core::kTwoJapaneseCharBytes) {
        if (type == VerbType::Suru) {
          float bon = GET_OPT(bonus_suru_two_kanji, inflection::kBonusSuruTwoKanji);
          base += bon;
          logConfidenceAdjustment(bon, "suru_two_kanji");
        } else if (type == VerbType::GodanSa) {
          float pen = GET_OPT(penalty_godan_sa_two_kanji, inflection::kPenaltyGodanSaTwoKanji);
          base -= pen;
          logConfidenceAdjustment(-pen, "godan_sa_two_kanji");
        }
      } else if (stem_len >= core::kThreeJapaneseCharBytes) {
        // Longer stems (3+ kanji) might be verb compounds - reduce boost
        if (type == VerbType::Suru) {
          base += inflection::kBonusSuruLongStem;
          logConfidenceAdjustment(inflection::kBonusSuruLongStem, "suru_long_stem");
        }
      } else if (stem_len == core::kJapaneseCharBytes) {
        // Single-kanji stem: prefer GodanSa (出す, 消す, etc.)
        if (type == VerbType::GodanSa) {
          float bon = GET_OPT(bonus_godan_sa_single_kanji, inflection::kBonusGodanSaSingleKanji);
          base += bon;
          logConfidenceAdjustment(bon, "godan_sa_single_kanji");
        } else if (type == VerbType::Suru) {
          float pen = GET_OPT(penalty_suru_single_kanji, inflection::kPenaltySuruSingleKanji);
          base -= pen;
          logConfidenceAdjustment(-pen, "suru_single_kanji");
        }
      }
    }
    // In mizenkei context for single-kanji, also boost GodanSa
    if (required_conn == conn::kVerbMizenkei && stem_len == core::kJapaneseCharBytes) {
      if (type == VerbType::GodanSa) {
        float bon = GET_OPT(bonus_godan_sa_single_kanji, inflection::kBonusGodanSaSingleKanji);
        base += bon;
        logConfidenceAdjustment(bon, "godan_sa_single_kanji_mizenkei");
      }
    }

    // Reject Suru stems containing te-form markers (て/で)
    // E.g., "基づいて処理" should be 基づいて(verb) + 処理(noun), not a single noun
    // The て/で in the middle of a stem indicates a verb te-form followed by noun
    // This applies to any context, not just shi-context
    if (type == VerbType::Suru && stem_len >= core::kThreeJapaneseCharBytes) {
      if (stem.find("て") != std::string_view::npos ||
          stem.find("で") != std::string_view::npos) {
        base -= inflection::kPenaltySuruTeFormStemInvalid;
        logConfidenceAdjustment(-inflection::kPenaltySuruTeFormStemInvalid, "suru_te_form_stem_invalid");
      }
    }
  }

  // Floor confidence at kConfidenceFloor to allow heavy penalties to differentiate
  // grammatically invalid patterns (e.g., e-row adjective stems like 食べい)
  // from valid patterns. The 0.5 threshold in adjective_candidates.cpp
  // will reject candidates below 0.5.
  float ceiling = GET_OPT(confidence_ceiling, inflection::kConfidenceCeiling);
  float floor = GET_OPT(confidence_floor, inflection::kConfidenceFloor);
  float result = std::min(ceiling, std::max(floor, base));
  SUZUME_DEBUG_LOG("[INFL_SCORE] → confidence=" << result << "\n");
  return result;
}

#undef GET_OPT

}  // namespace suzume::grammar
