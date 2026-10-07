#include <algorithm>

#include "char_patterns.h"
#include "connection.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "inflection_scorer_constants.h"
#include "inflection_scorer_internal.h"

namespace suzume::grammar::inflection_score_detail {

float scoreStemAndIchidan(float base, const InflectionScoreContext& context) {
  SUZUME_UNPACK_INFLECTION_CONTEXT(context);

  // Stem length penalties/bonuses
  // Very long stems are suspicious (likely wrong analysis)
  if (stem_len >= core::kFourJapaneseCharBytes) {
    applyPenalty(base, GET_OPT(penalty_stem_very_long, inflection::kPenaltyStemVeryLong), "stem_very_long");
  } else if (stem_len >= core::kThreeJapaneseCharBytes) {
    applyPenalty(base, GET_OPT(penalty_stem_long, inflection::kPenaltyStemLong), "stem_long");
  } else if (stem_len >= core::kTwoJapaneseCharBytes) {
    // 2-char stems (6 bytes) are common
    applyBonus(base, GET_OPT(bonus_stem_two_char, inflection::kBonusStemTwoChar), "stem_two_char");
  } else if (stem_len >= core::kJapaneseCharBytes) {
    // 1-char stems (3 bytes) are possible but less common
    applyBonus(base, inflection::kBonusStemOneChar, "stem_one_char");
  }

  // Small kana (拗音) cannot start a verb stem
  // ょ, ゃ, ゅ, ぁ, ぃ, ぅ, ぇ, ぉ, っ are always part of compound sounds
  // E.g., きょう is valid, but ょう alone cannot be a word
  if (stem_len >= core::kJapaneseCharBytes) {
    std::string_view first_char = stem.substr(0, core::kJapaneseCharBytes);
    if (isSmallKana(first_char)) {
      // Heavily penalize - this is grammatically impossible
      applyPenalty(base, inflection::kPenaltySmallKanaStemInvalid, "small_kana_stem_invalid");
    }
    // ん cannot start a verb stem in Japanese
    // E.g., んじゃする is impossible - should be ん + じゃない
    if (first_char == "ん") {
      applyPenalty(base, inflection::kPenaltyNStartStemInvalid, "n_start_stem_invalid");
    }
  }

  // Longer auxiliary chain = higher confidence (matched more grammar)
  float aux_per_byte = GET_OPT(bonus_aux_length_per_byte, inflection::kBonusAuxLengthPerByte);
  applyBonus(base, static_cast<float>(aux_total_len) * aux_per_byte, "aux_length");

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
        !equalsAny(stem, inflection::kValidHiraganaStemExceptions)) {
      applyPenalty(base, inflection::kPenaltyIchidanOnbinInvalid, "ichidan_onbin_invalid");
    }

    // Ichidan stems cannot end with onbin markers (っ, ん, い)
    // These are Godan onbin forms: 行っ(く), 読ん(む), 書い(く)
    // If Ichidan stem ends with these, it's a false match
    // E.g., 行っ + てた → 行っる (wrong) - should be 行く
    // Exception for specific kanji + い ichidan stems (用い, 率い, 報い)
    if (stem_len >= core::kJapaneseCharBytes) {
      std::string_view last_char = utf8::lastChar(stem);
      if (utf8::equalsAny(last_char, {"っ", "ん"})) {
        // っ and ん are always onbin markers
        applyPenalty(base, inflection::kPenaltyIchidanOnbinMarkerStemInvalid, "ichidan_onbin_marker_stem_invalid");
      } else if (last_char == "い") {
        // い can be onbin marker OR part of legitimate ichidan stem
        // Valid い-ending ichidan stems:
        // - Single-char い (from いる - to exist/be)
        // - 用い (用いる - to use), 率い (率いる - to lead), 報い (報いる - to repay)
        bool is_valid_i_stem = (stem == "い" ||  // いる
                                inflection::isValidKanjiIStemException(stem));
        if (!is_valid_i_stem) {
          applyPenalty(base, inflection::kPenaltyIchidanOnbinMarkerStemInvalid, "ichidan_onbin_marker_stem_invalid");
        }
      }
    }

    // Ichidan volitional requires e-row stem ending (食べよう, 見せよう)
    // If stem ends with godan base endings (く, す, etc.), it's likely wrong
    // E.g., 続く + よう → 続くる (wrong) - should be 続こう
    if (required_conn == conn::kVerbVolitional && stem_len >= core::kJapaneseCharBytes) {
      std::string_view last_char = utf8::lastChar(stem);
      bool is_godan_base_ending = utf8::equalsAny(last_char, {"く", "す", "ぐ", "つ", "ぬ", "む", "ぶ", "う"});
      if (is_godan_base_ending) {
        applyPenalty(base, inflection::kPenaltyIchidanVolitionalGodanStem, "ichidan_volitional_godan_stem");
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
      // Each use below keeps its own stem length guard.
      const std::string_view last_char = utf8::lastChar(stem);
      const std::string_view stem_head =
          stem.substr(0, stem_len >= core::kJapaneseCharBytes ? stem_len - core::kJapaneseCharBytes : 0);
      bool is_common_potential_ending = false;
      bool is_copula_de_pattern = false;
      if (stem_len >= core::kJapaneseCharBytes) {
        is_common_potential_ending = utf8::equalsAny(last_char, {"け", "め", "せ", "れ", "げ"});
        // All-kanji + で patterns are usually copula, not verb stems
        // e.g., 嫌でない = 嫌 + で + ない, 公園でる is not a real verb
        // Valid Ichidan verbs ending in で are rare: 茹でる, 出でる (archaic)
        // These have single-kanji stems (茹, 出), not multi-kanji stems
        if (last_char == "で" && stem_len >= core::kTwoJapaneseCharBytes) {
          if (isAllKanji(stem_head)) {
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
      // Pure potential forms (書ける, 読める) are treated as independent Ichidan verbs
      // when aux_count==0. This is a deliberate design choice.
      // With auxiliaries (書けない, 読めなかった), they correctly trace back to Godan.
      bool is_potential_context = required_conn == conn::kVerbRenyokei || required_conn == conn::kVerbMizenkei ||
                                  (required_conn == conn::kVerbBase && aux_count > 0);
      // Ichidan stems ending in て are suspicious as base forms
      // "来て" as Ichidan stem → "来てる" is wrong; it's actually 来る te-form
      // Exception: 捨てる, 棄てる have legitimate て-ending stems
      // Apply penalty when aux_count == 0 (analyzing as base/dictionary form)
      bool is_te_stem_in_base_context = false;
      if (stem_len >= core::kTwoJapaneseCharBytes && aux_count == 0) {
        if (last_char == "て") {
          // Check if this is a known exception (捨て, 棄て)
          if (!equalsAny(stem_head, inflection::kTeEndingStemExceptionKanji)) {
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
        if (last_char == "せ") {
          // Only apply to 2+ kanji stems (勉強, 検討, etc.), not single kanji (話, 見)
          if (isAllKanji(stem_head) && stem_head.size() >= core::kTwoJapaneseCharBytes) {
            // Multi-kanji + せ: likely suru-verb imperative, not Ichidan
            is_suru_imperative_pattern = true;
          }
        }
      }

      if (is_te_stem_in_base_context) {
        // Strong penalty: て-ending as base form is usually wrong
        // e.g., 来てる, 食べてる should be analyzed as て-form, not Ichidan base
        applyPenalty(base, inflection::kPenaltyIchidanTeStemBaseInvalid, "ichidan_te_stem_base_invalid");
      } else if (is_copula_de_pattern) {
        // Strong penalty: kanji + で is almost always copula (だ/です), not Ichidan
        // e.g., 公園で = NOUN + copula, 嫌でない = 嫌 + で + ない
        applyPenalty(base, inflection::kPenaltyIchidanCopulaDePattern, "ichidan_copula_de_pattern");
      } else if (is_suru_imperative_pattern) {
        // Strong penalty: kanji+ + せ is suru-verb imperative, not Ichidan
        // e.g., 勉強せ = 勉強する imperative, not 勉強せる
        applyPenalty(base, inflection::kPenaltyIchidanSuruImperativeSePattern, "ichidan_suru_imperative_se_pattern");
      } else if (stem_len == core::kTwoJapaneseCharBytes && is_potential_context &&
                 endsWithKanji(stem.substr(0, core::kJapaneseCharBytes)) && is_common_potential_ending) {
        // 読め could be Ichidan 読める or Godan potential of 読む
        // Prefer Godan potential interpretation (読む is more common than treating 読める as Ichidan)
        // Strong penalty to overcome the 0.95 cap tie
        applyPenalty(base, GET_OPT(penalty_ichidan_potential_ambiguity, inflection::kPenaltyIchidanPotentialAmbiguity),
                     "ichidan_potential_ambiguity");
      } else {
        applyBonus(base, GET_OPT(bonus_ichidan_e_row, inflection::kBonusIchidanERow), "ichidan_e_row");
      }
    } else {
      // Check for context-specific Godan patterns
      bool looks_godan = false;

      // Onbin context: stems ending in い, っ, ん suggest Godan
      if (required_conn == conn::kVerbOnbinkei) {
        looks_godan = endsWithOnbin(stem);
      }
      // Mizenkei context: stems ending in a-row suggest Godan
      else if (required_conn == conn::kVerbMizenkei) {
        looks_godan = endsWithChar(stem, kMizenkeiEndings, kMizenkeiCount);
      }
      // Renyokei context: stems ending in i-row suggest Godan
      else if (required_conn == conn::kVerbRenyokei) {
        looks_godan = endsWithChar(stem, kRenyokeiEndings, kRenyokeiCount);
      }

      // Skip the "looks godan" penalty for kanji + い kami-ichidan verbs
      // (率いる, 老いる, 悔いる, 報いる, 強いる, 用いる). Their renyokei/onbin stem
      // ends in い, which resembles GodanKa い-onbin (率いた looks like 率く), but
      // the correct lemma is the いる form. The same exception set guards the
      // onbin-marker path above, so both paths agree via one source of truth.
      if (looks_godan && !inflection::isValidKanjiIStemException(stem)) {
        // Stem matches Godan conjugation pattern for this context
        applyPenalty(base, GET_OPT(penalty_ichidan_looks_godan, inflection::kPenaltyIchidanLooksGodan),
                     "ichidan_looks_godan");
      }

      // Ichidan verb stems can only end in i-row or e-row hiragana (見, 起き, 食べ, 分かれ).
      // Any other final hiragana — a-row (分か), u-row (読む), o-row, or ん — is a Godan
      // conjugation shape, so an Ichidan analysis is grammatically impossible. This blocks
      // both 読む→読むる and the 分か+れた fake-ichidan 分かる that would otherwise be
      // "verified" against the real Godan-ra 分かる by the type-blind dictionary lookup.
      // Applies even in kVerbBase context (aux_count == 0): the shape is invalid regardless.
      // (E-row stems are handled in the branch above; only i-row remains as a valid ending.)
      if (stem_len >= core::kJapaneseCharBytes) {
        char32_t last_cp = utf8::decodeLastChar(stem);
        if (kana::isHiraganaCodepoint(last_cp) && !kana::isIRowCodepoint(last_cp) && !kana::isERowCodepoint(last_cp)) {
          // Strong penalty - this pattern is grammatically impossible for Ichidan
          applyPenalty(base, inflection::kPenaltyIchidanInvalidRowStem, "ichidan_invalid_row_stem");
        }
      }

      // Ichidan stem ending in い (kanji + い) in renyokei context is suspicious
      // Pattern: 行い + ます → 行いる (wrong) vs 行 + います → 行う (correct)
      // Pattern: 手伝い + ます → 手伝いる (wrong) vs 手伝 + います → 手伝う (correct)
      // Stems like 行い, 手伝い (kanji + い) are more likely Godan renyokei than Ichidan
      // Exception: kanji + い kami-ichidan verbs (用い, 率い, 報い, 老い, 悔い, 強い)
      // are valid Ichidan renyokei stems; the shared exception set guards them so
      // the lemmatizer resolves 率い → 率いる rather than a spurious godan lemma.
      // Apply strong penalty when stem ends with kanji + い in renyokei context
      if (required_conn == conn::kVerbRenyokei && stem_len >= core::kTwoJapaneseCharBytes &&
          !inflection::isValidKanjiIStemException(stem)) {
        std::string_view last3 = utf8::lastChar(stem);  // Last 3 bytes = い
        std::string_view prev3 =
            stem.substr(stem_len - core::kTwoJapaneseCharBytes, core::kJapaneseCharBytes);  // Previous char
        if (last3 == "い" && endsWithKanji(prev3)) {
          // Stem ends with kanji + い, likely Godan renyokei misanalysis
          // Use stronger penalty than generic "looks godan"
          applyPenalty(base, GET_OPT(penalty_ichidan_kanji_i, inflection::kPenaltyIchidanKanjiI),
                       "ichidan_kanji_i_renyokei");
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
        applyBonus(base, inflection::kBonusIchidanCausativePassive, "ichidan_causative_passive");
      } else if (aux_count == 1 && aux_total_len == core::kJapaneseCharBytes) {
        // Simple te-form: て/た (3 bytes only)
        // These are common for 見る, 着る, 寝る, etc.
        // BUT: Ichidan te-form uses て/た, NOT で
        // で is Godan onbin te-form (読んで from 読む), not Ichidan
        // If we're in onbinkei context with Ichidan, apply strong penalty
        if (required_conn == conn::kVerbOnbinkei) {
          // Ichidan verbs don't have onbin - this is wrong analysis
          // e.g., 侍で should NOT be analyzed as Ichidan stem + で (te-form)
          applyPenalty(base, inflection::kPenaltyIchidanSingleKanjiOnbinInvalid, "ichidan_single_kanji_onbin_invalid");
        }
      } else if (aux_count == 1 && aux_total_len == core::kTwoJapaneseCharBytes &&
                 required_conn == conn::kVerbRenyokei) {
        // 2-char aux with renyokei connection: とく, ちゃう, てる, etc.
        // Valid colloquial patterns for Ichidan (見とく → 見る + とく)
        // No penalty - these are legitimate contractions
        // NOTE: 3-char patterns like すぎた should still get penalty (高い + すぎた, not 高る + すぎた)
      } else if (aux_count == 1 && aux_total_len == core::kTwoJapaneseCharBytes &&
                 required_conn == conn::kVerbMizenkei) {
        // 2-char aux with mizenkei connection: ない, ぬ, etc.
        // Valid negative forms for single-kanji Ichidan (見ない → 見る + ない)
        // No penalty - these are legitimate conjugations
      } else {
        // Multiple aux matches or longer single match (like せる, されて)
        // Likely wrong match via potential/passive pattern
        // Apply penalty to prefer the Godan interpretation
        applyPenalty(base, inflection::kPenaltyIchidanSingleKanjiMultiAux, "ichidan_single_kanji_multi_aux");
      }
    }
  }

  // Ichidan with kanji+i-row hiragana stem pattern validation
  // Stems like 人い, 玉い are unnatural for Ichidan verbs
  // Real Ichidan verbs have e-row stems (食べ, 見え, 出来) not i-row
  // Kanji + i-row patterns are likely NOUN + verb (いる) misanalysis
  // E.g., 人いる = 人 + いる (not 人い + る)
  // Exception for specific kanji + い stems (用い, 率い, 報い) - valid kami-ichidan
  if (type == VerbType::Ichidan && stem_len == core::kTwoJapaneseCharBytes && aux_count == 0) {
    // 6 bytes = 2 chars (kanji + hiragana)
    // Check if pattern is kanji + i-row hiragana
    char32_t first_cp = utf8::decodeFirstChar(stem);
    char32_t second_cp = utf8::decode3ByteUtf8At(stem, core::kJapaneseCharBytes);
    bool first_is_kanji = kana::isKanjiCodepoint(first_cp);
    bool is_i_row = kana::isIRowCodepoint(second_cp);
    // Exception: specific known kanji + い stems are valid
    bool is_known_kanji_i_stem = inflection::isValidKanjiIStemException(stem);
    if (first_is_kanji && is_i_row && !is_known_kanji_i_stem) {
      applyPenalty(base, GET_OPT(penalty_ichidan_kanji_hiragana_stem, inflection::kPenaltyIchidanKanjiHiraganaStem),
                   "ichidan_kanji_i_row_stem");
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
  // Also でき (from できる - to be able)
  bool is_valid_hiragana_stem = equalsAny(stem, inflection::kValidHiraganaStemExceptions);
  if (type == VerbType::Ichidan && stem_len >= core::kTwoJapaneseCharBytes && isPureHiragana(stem) &&
      !is_valid_hiragana_stem) {
    applyPenalty(base, GET_OPT(penalty_pure_hiragana_stem, inflection::kPenaltyPureHiraganaStem),
                 "ichidan_pure_hiragana_stem");
  }

  return base;
}

float scoreGodan(float base, const InflectionScoreContext& context) {
  SUZUME_UNPACK_INFLECTION_CONTEXT(context);

  // GodanRa validation: single-hiragana stems are typically Ichidan, not GodanRa
  // Verbs like みる (to see), きる (to cut/wear), にる (to boil) are Ichidan
  // Godan Ra verbs usually have at least 2 chars in stem (帰る, 走る, 取る)
  // Apply penalty to GodanRa interpretation for single-hiragana stems
  if (type == VerbType::GodanRa && stem_len == core::kJapaneseCharBytes && !endsWithKanji(stem)) {
    // Single hiragana stem (み, き, に, etc.) - likely Ichidan, not GodanRa
    applyPenalty(base, inflection::kPenaltyGodanRaSingleHiragana, "godan_ra_single_hiragana");
  }

  // In kVerbKatei (conditional) context, stems ending in i-row hiragana suggest Ichidan
  // Examples: 起き(る), 生き(る), 過ぎ(る) - Ichidan verbs with i-row stems
  // vs. 走(る), 取(る) - GodanRa verbs where stem is typically kanji-only
  // The i-row ending indicates the character is part of the Ichidan stem
  if (required_conn == conn::kVerbKatei && stem_len >= core::kTwoJapaneseCharBytes) {
    bool has_irow_ending = endsWithChar(stem, kRenyokeiEndings, kRenyokeiCount);
    if (has_irow_ending) {
      if (type == VerbType::Ichidan) {
        applyBonus(base, inflection::kBonusIchidanKateiIRow, "ichidan_katei_i_row");
      } else if (type == VerbType::GodanRa) {
        applyPenalty(base, inflection::kPenaltyGodanRaKateiIRow, "godan_ra_katei_i_row");
      }
    }
  }

  // GodanTa stems cannot end with onbin markers (っ, ん, い)
  // GodanTa verbs like 持つ, 立つ have stems like 持, 立
  // The っ is the onbin FORM, not part of the stem
  // E.g., 行っ + てた → 行っつ (wrong) - 行っ is onbin of 行く (GodanKa), not GodanTa
  if (type == VerbType::GodanTa && stem_len >= core::kJapaneseCharBytes) {
    std::string_view last_char = utf8::lastChar(stem);
    if (utf8::equalsAny(last_char, {"っ", "ん", "い"})) {
      applyPenalty(base, inflection::kPenaltyGodanTaOnbinStemInvalid, "godan_ta_onbin_stem_invalid");
    }
    // GodanTa uses った for te-form onbin, not てた.
    // 見てた should be Ichidan 見る, not GodanTa 見つ
    // GodanTa te-form: 持つ → 持った → 持ってた
    // Only penalize when the first auxiliary actually starts with て/で.
    // Other valid renyokei auxiliaries (持ち+ます/たい) must not be affected.
    if (required_conn == conn::kVerbRenyokei &&
        (utf8::startsWith(first_aux, "て") || utf8::startsWith(first_aux, "で"))) {
      applyPenalty(base, inflection::kPenaltyGodanTaRenyokeiTeDeAuxInvalid, "godan_ta_renyokei_te_de_aux_invalid");
    }
  }

  // GodanRa disambiguation for っ-onbin patterns with single-kanji stems
  // Three verb types share っ-onbin: GodanWa (買う), GodanRa (取る), GodanTa (持つ)
  // A small GodanRa preference is useful only for a single-kanji stem; longer
  // stems are left neutral so dictionary evidence decides the verb class.
  if (required_conn == conn::kVerbOnbinkei && isAllKanji(stem)) {
    if (type == VerbType::GodanRa && stem_len == core::kJapaneseCharBytes) {
      applyBonus(base, inflection::scale::kMinorBonus, "godan_ra_single_kanji");
    }
  }

  // GodanWa vs GodanRa disambiguation for っ-onbin with hiragana stems ending in しゃ/さ
  // Honorific verbs like いらっしゃる, おっしゃる, くださる, なさる are GodanRa
  // These stems end in しゃ or さ, which is NOT typical for GodanWa verbs
  // GodanWa verbs like 買う, 舞う, 行う have stems ending in kanji or other hiragana
  if (required_conn == conn::kVerbOnbinkei && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last6 = stem.substr(stem_len - core::kTwoJapaneseCharBytes);
    std::string_view last3 = utf8::lastChar(stem);
    bool ends_with_sha = utf8::equalsAny(last6, {"しゃ", "しょ", "しゅ"});
    bool ends_with_sa = (last3 == "さ");
    if (ends_with_sha || ends_with_sa) {
      if (type == VerbType::GodanWa) {
        // Strong penalty: stems ending in しゃ/さ are not typical GodanWa
        applyPenalty(base, inflection::scale::kMinor, "godan_wa_sha_sa_stem");
      } else if (type == VerbType::GodanRa) {
        // Boost: honorific verbs are GodanRa
        applyBonus(base, inflection::scale::kMinor, "godan_ra_sha_sa_stem");
      }
    }
  }

  // Kuru validation: only 来る/くる conjugates as Kuru
  // Valid Kuru stems:
  // - "来" (kanji form: 来なかった → 来る)
  // - "" (empty, when suffix is こ/き: こなかった → くる)
  if (type == VerbType::Kuru) {
    if (!isKuruStem(stem)) {
      // Any stem other than 来 or empty is invalid for Kuru
      applyPenalty(base, inflection::kPenaltyKuruInvalidStem, "kuru_invalid_stem");
    }
  }

  // Suru/Kuru imperative boost: しろ, せよ, こい have empty stems
  // These must win over competing Ichidan/Godan interpretations
  // (しろ vs しる, こい vs こう)
  if (stem.empty() && required_conn == conn::kVerbMeireikei && (type == VerbType::Suru || type == VerbType::Kuru)) {
    applyBonus(base, inflection::kBonusSuruKuruImperative, "suru_kuru_imperative");
  }

  // Ichidan validation: reject base forms that would be irregular verbs
  // くる (来る) is カ変, not 一段. Stem く + る = くる is INVALID for Ichidan.
  // する is サ変, not 一段. Stem す + る = する is INVALID for Ichidan.
  // こる is not a valid verb - こ is Kuru mizenkei suffix, not Ichidan stem.
  // E.g., くなかった should NOT be parsed as Ichidan く + なかった = くる
  // E.g., こなかった should NOT be parsed as Ichidan こ + なかった = こる
  if (type == VerbType::Ichidan && stem_len == core::kJapaneseCharBytes) {
    if (equalsAny(stem, inflection::kInvalidIchidanSingleStems)) {
      applyPenalty(base, GET_OPT(penalty_ichidan_irregular_stem, inflection::kPenaltyIchidanIrregularStem),
                   "ichidan_irregular_stem");
    }
  }

  // Ichidan single-hiragana particle stem penalty
  // In mizenkei context, single-hiragana stems that are common particles
  // should be heavily penalized. E.g., もない = も(PARTICLE) + ない(AUX),
  // NOT もる(VERB) + ない. Common particles: も, は, が, を, に, へ, と, で, よ, ね, わ, な
  if (type == VerbType::Ichidan && stem_len == core::kJapaneseCharBytes && required_conn == conn::kVerbMizenkei &&
      !endsWithKanji(stem)) {
    // Check if stem is a common particle
    if (equalsAny(stem, inflection::kParticleStemList)) {
      applyPenalty(
          base,
          GET_OPT(penalty_ichidan_single_hiragana_particle, inflection::kPenaltyIchidanSingleHiraganaParticleStem),
          "ichidan_single_hiragana_particle_stem");
    }
  }

  // Particle + な stem penalty for GodanWa
  // E.g., もない → もなう is not a real verb. The pattern is も(PARTICLE) + ない(AUX).
  // Only the continuative い and the onbin っ spell a particle + ない/なる
  // (に+なっ+た); the other cells keep the kana verb (になう).
  if (type == VerbType::GodanWa && stem_len == core::kTwoJapaneseCharBytes && !containsKanji(stem) &&
      (required_conn == conn::kVerbRenyokei || required_conn == conn::kVerbOnbinkei)) {
    std::string_view first = stem.substr(0, core::kJapaneseCharBytes);
    std::string_view second = stem.substr(core::kJapaneseCharBytes);
    // If first char is a common particle and second is な, this is likely
    // a misparse of PARTICLE + ない(adjective/aux)
    if (second == "な" && equalsAny(first, inflection::kParticleStemList)) {
      applyPenalty(base, inflection::kPenaltyGodanWaParticleNaStem, "godan_wa_particle_na_stem");
    }
  }

  // Single-hiragana stem penalty for Godan verbs (non-Ra/Wa)
  // Single-char hiragana stems like ま(む), む(ぐ) are almost never real verbs
  // Real single-char verbs (み, き, に for ichidan) are handled separately
  // Exception: GodanRa has separate handling (godan_ra_single_hiragana)
  // Exception: い(く) is a valid GodanKa verb (行く)
  // GodanWa with single hiragana stem (ら→らう, ま→まう) are typically not real verbs
  // Real GodanWa verbs like 買う, 舞う use kanji stems
  bool is_godan_non_ra = isGodanVerbType(type) && type != VerbType::GodanRa;
  if (is_godan_non_ra && stem_len == core::kJapaneseCharBytes && !containsKanji(stem)) {
    // Exception: い(く) = 行く is valid
    bool is_iku = (type == VerbType::GodanKa && isIkuStem(stem));
    // A matched タ行五段 連用形 + auxiliary chain is strong grammatical
    // evidence even with a one-kana lexical stem (も+ち+たい, た+ち+ます).
    // Keep this independent of the auxiliary surface: invalid ち+て/で chains
    // receive the dedicated penalty above instead of two unrelated penalties.
    bool is_godan_ta_renyokei_aux = type == VerbType::GodanTa && required_conn == conn::kVerbRenyokei && aux_count > 0;
    // Exception: the bare-う dictionary form of a single-kana GodanWa stem is a
    // systematically real verb (かう/買う, すう/吸う, ぬう/縫う, いう/言う, あう/会う).
    // Scoped to the base form (conn = kVerbBase, suffix = the single mora う, no
    // auxiliaries) so onbin/renyokei shapes (かって, かい) keep the penalty and
    // don't fabricate かう readings elsewhere.
    bool is_godan_wa_base = (type == VerbType::GodanWa && required_conn == conn::kVerbBase && aux_count == 0 &&
                             suffix_len == core::kJapaneseCharBytes);
    if (!is_iku && !is_godan_ta_renyokei_aux && !is_godan_wa_base) {
      applyPenalty(base, GET_OPT(penalty_godan_single_hiragana_stem, inflection::kPenaltyGodanSingleHiraganaStem),
                   "godan_single_hiragana_stem");
    }
  }

  // Godan (Ma/Ga/Na/Ba) pure hiragana multi-char stem penalty
  // These types rarely have legitimate hiragana-only verbs:
  // - GodanMa: 読む, 飲む - always in kanji; no coined verbs
  // - GodanGa: 泳ぐ, 注ぐ - always in kanji; no coined verbs
  // - GodanNa: only 死ぬ exists
  // - GodanBa: 飛ぶ, 遊ぶ - always in kanji
  // GodanKa/Sa/Ta excluded - いく, なくす, もつ are common in hiragana
  bool is_godan_hiragana_rare = (type == VerbType::GodanMa || type == VerbType::GodanGa || type == VerbType::GodanNa ||
                                 type == VerbType::GodanBa);
  if (is_godan_hiragana_rare && stem_len >= core::kTwoJapaneseCharBytes && isPureHiragana(stem)) {
    applyPenalty(base, GET_OPT(penalty_godan_non_ra_pure_hiragana, inflection::kPenaltyGodanNonRaPureHiraganaStem),
                 "godan_hiragana_rare_stem");
  }

  // GodanSa/Suru with pure hiragana long stem (3+ chars) is very rare
  // Valid hiragana GodanSa verbs: なくす, もらす, こぼす (2-char stems)
  // Invalid: おねえす, おにいす (3+ char hiragana stems don't form real verbs)
  // Suru verbs almost never have pure hiragana stems (attach to kanji/katakana nouns)
  // Exception: an a-row final mora is the transitive -asu derivation
  // (うごかす, ちらかす, あまやかす), a productive GodanSa shape.
  bool is_godan_sa_or_suru = (type == VerbType::GodanSa || type == VerbType::Suru);
  const bool is_transitive_asu_stem =
      type == VerbType::GodanSa && !stem.empty() && kana::isARowCodepoint(utf8::decodeLastChar(stem));
  if (is_godan_sa_or_suru && !is_transitive_asu_stem && stem_len >= core::kThreeJapaneseCharBytes &&
      isPureHiragana(stem)) {
    applyPenalty(base, inflection::kPenaltyGodanSaSuruPureHiraganaLongStem, "godan_sa_suru_pure_hiragana_long_stem");
  }

  // Godan verbs with stems ending in て/で are almost always invalid
  // The て/で ending indicates a te-form, not a valid verb stem
  // E.g., してく → して + く is wrong; it's a te-form, not a GodanKu stem
  // Real Godan verbs: 書く (stem 書), 読む (stem 読) - stems never end in て/で
  // Exception: Some Ichidan verbs have て/で in stem (捨てる, 建てる), but those
  // are handled separately. This penalty applies to Godan types only.
  if (is_godan_non_ra && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last_char = utf8::lastChar(stem);
    if (isTeDeSurface(last_char)) {
      applyPenalty(base, GET_OPT(penalty_godan_te_stem, inflection::kPenaltyGodanTeStem), "godan_te_stem_invalid");
    }
  }

  // Suru with 1 or 2 hiragana stem is invalid
  // E.g., えする, あする, みくする - pure hiragana are not valid Suru noun stems
  // Valid Suru stems: 勉強, ダウンロード (kanji or katakana)
  // Note: GodanSa 2-char stems ARE valid (なくす, もらす), so this only applies to Suru
  if (type == VerbType::Suru && stem_len <= core::kTwoJapaneseCharBytes && isPureHiragana(stem)) {
    applyPenalty(base, inflection::kPenaltyGodanSaSuruPureHiraganaLongStem, "suru_short_hiragana_stem");
  }

  // GodanNa (ナ行五段) is extremely rare - only 死ぬ exists in modern Japanese
  // Apply penalty to all GodanNa interpretations except 死ぬ:
  // - In ん-onbin context: penalize to prefer GodanMa/GodanBa (跳んだ→跳ぶ, not 跳ぬ)
  // - In base form context: penalize to prefer VERB+ぬ(AUX) (消えぬ→消え+ぬ, not 消えぬ)
  // Exception: 死ぬ (to die) is the only GodanNa verb - BOOST 死 stem instead
  if (type == VerbType::GodanNa) {
    // Exception for 死ぬ - the only GodanNa verb in modern Japanese
    // Boost 死 stem to beat GodanMa/GodanBa and VERB+ぬ interpretations
    if (stem == "死") {
      applyBonus(base, inflection::scale::kMinorBonus, "godan_na_shinu_boost");
    } else {
      // For base form context (〜ぬ as dictionary form), apply strong penalty
      // to prefer VERB(未然形)+ぬ(AUX) interpretation (文語否定)
      // E.g., 消えぬ → 消え(消える)+ぬ(AUX), not 消えぬ(GodanNa verb)
      // E.g., 揃わぬ → 揃わ(揃う)+ぬ(AUX), not 揃わぬ(GodanNa verb)
      applyPenalty(base, inflection::kPenaltyGodanNaRare, "godan_na_rare");
    }
  }

  // I-adjective validation: single-kanji stems are very rare
  // Most I-adjectives have multi-character stems (美しい, 高い, 長い)
  return base;
}

}  // namespace suzume::grammar::inflection_score_detail
