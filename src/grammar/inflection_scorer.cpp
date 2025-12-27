/**
 * @file inflection_scorer.cpp
 * @brief Confidence scoring for inflection analysis candidates
 */

#include "inflection_scorer.h"

#include <algorithm>

#include "char_patterns.h"
#include "connection.h"
#include "inflection_scorer_constants.h"

namespace suzume::grammar {

float calculateConfidence(VerbType type, std::string_view stem,
                          size_t aux_total_len, size_t aux_count,
                          uint16_t required_conn) {
  float base = inflection::kBaseConfidence;
  size_t stem_len = stem.size();

  // Stem length penalties/bonuses
  // Very long stems are suspicious (likely wrong analysis)
  if (stem_len >= 12) {
    base -= inflection::kPenaltyStemVeryLong;
  } else if (stem_len >= 9) {
    base -= inflection::kPenaltyStemLong;
  } else if (stem_len >= 6) {
    // 2-char stems (6 bytes) are common
    base += inflection::kBonusStemTwoChar;
  } else if (stem_len >= 3) {
    // 1-char stems (3 bytes) are possible but less common
    base += inflection::kBonusStemOneChar;
  }

  // Longer auxiliary chain = higher confidence (matched more grammar)
  base += static_cast<float>(aux_total_len) * inflection::kBonusAuxLengthPerByte;

  // Ichidan validation based on connection context
  if (type == VerbType::Ichidan) {
    if (endsWithERow(stem)) {
      // E-row endings (食べ, 見せ, etc.) are very common for Ichidan
      // But 2-char stems with e-row ending (書け, 読め) could be Godan potential
      // Apply penalty when stem looks like a common Godan potential form:
      //   - け (ka-row): 書く, 聞く, 行く, etc. - very common
      //   - め (ma-row): 読む, 飲む, etc. - common
      //   - せ (sa-row): 話す, 出す, etc. - common
      //   - れ (ra-row): 取る, 乗る, etc. - common
      // But NOT:
      //   - べ (ba-row): 食べる is Ichidan, 飛ぶ → 飛べ is less common
      //   - え (wa-row): Many Ichidan verbs end in え (考える, 答える, 見える)
      //   - げ (ga-row): 泳ぐ, 急ぐ, etc. - moderately common
      //   - Others: て, ね, へ - less common as potential forms
      bool is_common_potential_ending = false;
      if (stem_len >= 3) {
        std::string_view last_char = stem.substr(stem_len - 3);
        is_common_potential_ending = (last_char == "け" || last_char == "め" ||
                                      last_char == "せ" || last_char == "れ" ||
                                      last_char == "げ");
      }
      // Apply penalty only when:
      // 1. Stem is 2 chars (kanji + e-row hiragana)
      // 2. In a context where Godan potential interpretation is possible
      // 3. The e-row ending is a common Godan potential form (け, め, せ, れ)
      // Note: kVerbBase is included because pure potential forms like 読める
      // are parsed as Ichidan with る base ending, but should prefer Godan potential
      bool is_potential_context =
          required_conn == conn::kVerbRenyokei ||
          required_conn == conn::kVerbMizenkei ||
          required_conn == conn::kVerbBase;
      if (stem_len == 6 && is_potential_context &&
          endsWithKanji(stem.substr(0, 3)) && is_common_potential_ending) {
        // 読め could be Ichidan 読める or Godan potential of 読む
        // Prefer Godan potential interpretation (読む is more common than treating 読める as Ichidan)
        // Strong penalty to overcome the 0.95 cap tie
        base -= inflection::kPenaltyIchidanPotentialAmbiguity;
      } else {
        base += inflection::kBonusIchidanERow;
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
        base -= inflection::kPenaltyIchidanLooksGodan;
      }

      // Ichidan stem ending in い (kanji + い) in renyokei context is suspicious
      // Pattern: 行い + ます → 行いる (wrong) vs 行 + います → 行う (correct)
      // Stems like 行い (kanji + い) are more likely GodanWa renyokei than Ichidan
      // But 用い (from 用いる) is a valid Ichidan stem
      // Apply penalty only when stem is exactly 2 chars (6 bytes): kanji + い
      // Longer stems like 用い are more likely valid Ichidan
      if (required_conn == conn::kVerbRenyokei && stem_len == 6) {
        std::string_view last = stem.substr(3);  // Last 3 bytes = 1 hiragana
        if (last == "い" && endsWithKanji(stem.substr(0, 3))) {
          // Stem is exactly kanji + い, likely GodanWa renyokei
          base -= inflection::kPenaltyIchidanLooksGodan;
        }
      }
    }

    // Single-kanji Ichidan stems are rare but valid (見る, 着る, etc.)
    // Problem: 殺されて can be parsed as 殺 + されて (wrong) or 殺さ + れて (correct)
    // The させられた/させられて patterns (15 bytes) are legitimate Ichidan causative-passive
    // When aux_count == 1 and aux_total_len == 15, it's likely させられた (correct)
    // When aux_count >= 2 (e.g., きた + されて), it's likely wrong
    if (stem_len == 3 && endsWithKanji(stem)) {
      if (aux_count == 1 && aux_total_len >= 12) {
        // Single long aux match like させられた (15 bytes) or させられる (15 bytes)
        // This is legitimate Ichidan causative-passive (見させられた → 見る)
        base += inflection::kBonusIchidanCausativePassive;
      } else {
        // Multiple aux matches or short single match
        // Likely wrong match via される pattern (殺されて → 殺る WRONG)
        // Apply penalty to prefer the れて path (殺されて → 殺さ + れて → 殺す)
        base -= inflection::kPenaltyIchidanSingleKanjiMultiAux;
      }
    }
  }

  // GodanRa validation: single-hiragana stems are typically Ichidan, not GodanRa
  // Verbs like みる (to see), きる (to cut/wear), にる (to boil) are Ichidan
  // Godan Ra verbs usually have at least 2 chars in stem (帰る, 走る, 取る)
  // Apply penalty to GodanRa interpretation for single-hiragana stems
  if (type == VerbType::GodanRa && stem_len == 3 && !endsWithKanji(stem)) {
    // Single hiragana stem (み, き, に, etc.) - likely Ichidan, not GodanRa
    base -= inflection::kPenaltyGodanRaSingleHiragana;
  }

  // In kVerbKatei (conditional) context, stems ending in i-row hiragana suggest Ichidan
  // Examples: 起き(る), 生き(る), 過ぎ(る) - Ichidan verbs with i-row stems
  // vs. 走(る), 取(る) - GodanRa verbs where stem is typically kanji-only
  // The i-row ending indicates the character is part of the Ichidan stem
  if (required_conn == conn::kVerbKatei && stem_len >= 6) {
    bool has_irow_ending = endsWithChar(stem, kRenyokeiEndings, kRenyokeiCount);
    if (has_irow_ending) {
      if (type == VerbType::Ichidan) {
        base += inflection::kBonusIchidanKateiIRow;
      } else if (type == VerbType::GodanRa) {
        base -= inflection::kPenaltyGodanRaKateiIRow;
      }
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
  if (required_conn == conn::kVerbOnbinkei && stem_len >= 6 && isAllKanji(stem)) {
    if (type == VerbType::GodanWa) {
      base += inflection::kBonusGodanWaMultiKanji;
    } else if (type == VerbType::GodanRa || type == VerbType::GodanTa) {
      base -= inflection::kPenaltyGodanRaTaMultiKanji;
    }
  }

  // Kuru validation: only 来る/くる conjugates as Kuru
  // Valid Kuru stems:
  // - "来" (kanji form: 来なかった → 来る)
  // - "" (empty, when suffix is こ/き: こなかった → くる)
  if (type == VerbType::Kuru) {
    if (stem != "来" && !stem.empty()) {
      // Any stem other than 来 or empty is invalid for Kuru
      base -= inflection::kPenaltyKuruInvalidStem;
    }
  }

  // Suru/Kuru imperative boost: しろ, せよ, こい have empty stems
  // These must win over competing Ichidan/Godan interpretations
  // (しろ vs しる, こい vs こう)
  if (stem.empty() && required_conn == conn::kVerbMeireikei &&
      (type == VerbType::Suru || type == VerbType::Kuru)) {
    base += inflection::kBonusSuruKuruImperative;
  }

  // Ichidan validation: reject base forms that would be irregular verbs
  // くる (来る) is カ変, not 一段. Stem く + る = くる is INVALID for Ichidan.
  // する is サ変, not 一段. Stem す + る = する is INVALID for Ichidan.
  // こる is not a valid verb - こ is Kuru mizenkei suffix, not Ichidan stem.
  // E.g., くなかった should NOT be parsed as Ichidan く + なかった = くる
  // E.g., こなかった should NOT be parsed as Ichidan こ + なかった = こる
  if (type == VerbType::Ichidan && stem_len == 3) {
    if (stem == "く" || stem == "す" || stem == "こ") {
      base -= inflection::kPenaltyIchidanIrregularStem;
    }
  }

  // I-adjective validation: single-kanji stems are very rare
  // Most I-adjectives have multi-character stems (美しい, 高い, 長い)
  // Single-kanji stems like 書い (from mismatched 書く) are usually wrong
  if (type == VerbType::IAdjective && stem_len == 3) {
    base -= inflection::kPenaltyIAdjSingleKanji;
  }

  // I-adjective stems containing verb+auxiliary patterns are not real adjectives
  // E.g., 食べすぎてしま (from 食べすぎてしまい) is verb+auxiliary, not adjective
  // Pattern: stem contains てしま/でしま (te-form + shimau) → verb compound
  // Pattern: stem contains ている/でいる (te-form + iru) → verb compound
  // Pattern: stem contains てお/でお (te-form + oku) → verb compound
  if (type == VerbType::IAdjective && stem_len >= 12) {
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
      base -= inflection::kPenaltyIAdjVerbAuxPattern;
      // Note: This penalty may be clamped by the floor at return.
      // Additional penalty is applied in scorer.cpp for lattice cost.
    }
  }

  // I-adjective stems ending with "し" are very common (難しい, 美しい, 楽しい, 苦しい)
  // When followed by すぎる/やすい/にくい auxiliaries, boost confidence
  // This helps disambiguate 難しすぎる (難しい + すぎる) vs 難す (Godan-Sa)
  if (type == VerbType::IAdjective && stem_len >= 6 && aux_count >= 1) {
    std::string_view last = stem.substr(stem_len - 3);
    if (last == "し") {
      base += inflection::kBonusIAdjShiiStem;
    }
  }

  // Boost for verb renyokei + やすい/にくい compound adjective patterns
  // E.g., 読みやすい (easy to read), 使いにくい (hard to use)
  // The stem will be verb_renyokei + やす/にく (e.g., 読みやす, 使いにく)
  if (type == VerbType::IAdjective && stem_len >= 9) {
    std::string_view last6 = stem.substr(stem_len - 6);
    if (last6 == "やす" || last6 == "にく") {
      // Check if the part before やす/にく ends with verb renyokei marker
      std::string_view before = stem.substr(0, stem_len - 6);
      if (!before.empty() && before.size() >= 3) {
        std::string_view last3 = before.substr(before.size() - 3);
        // i-row (godan renyokei) and e-row (ichidan renyokei)
        if (last3 == "み" || last3 == "き" || last3 == "ぎ" ||
            last3 == "し" || last3 == "ち" || last3 == "び" ||
            last3 == "り" || last3 == "い" || last3 == "に" ||
            last3 == "べ" || last3 == "め" || last3 == "せ" ||
            last3 == "け" || last3 == "げ" || last3 == "て" ||
            last3 == "ね" || last3 == "れ" || last3 == "え") {
          base += inflection::kBonusIAdjCompoundYasuiNikui;
        }
      }
    }
  }

  // I-adjective stems consisting only of 3+ kanji are extremely rare
  // Such stems are usually サ変名詞 (検討, 勉強, 準備) being misanalyzed
  // Real i-adjectives have patterns like: 美しい, 楽しい (kanji + hiragana)
  // This prevents "検討いたす" from being parsed as "検討い" + "たす"
  // Exception: 2-kanji stems (6 bytes) can be valid adjectives:
  //   面白い (おもしろい), 可愛い (かわいい), 美味い (うまい)
  if (type == VerbType::IAdjective && stem_len >= 9 && isAllKanji(stem)) {
    base -= inflection::kPenaltyIAdjAllKanji;
  }

  // I-adjective stems ending with e-row hiragana are extremely rare
  // E-row endings (食べ, 見え, 教え) are typical of ichidan verb stems
  // This prevents "食べそう" from being parsed as i-adjective "食べい"
  if (type == VerbType::IAdjective && endsWithERow(stem)) {
    base -= inflection::kPenaltyIAdjERowStem;
  }

  // I-adjective stems ending with a-row hiragana (な, ま, か, etc.) are suspicious
  // These are typically verb mizenkei forms + ない (食べな, 読ま, 書か)
  // This prevents "食べなければ" from being parsed as i-adjective "食べない"
  // Real i-adjectives with ない: 危ない (あぶな), 少ない (すくな)
  // But these have specific patterns, not random verb stem + な
  if (type == VerbType::IAdjective && stem_len >= 6) {
    std::string_view last = stem.substr(stem_len - 3);
    if (last == "な" || last == "ま" || last == "か" || last == "が" ||
        last == "さ" || last == "た" || last == "ば" || last == "ら" ||
        last == "わ") {
      // Check if there's a hiragana before the a-row ending (verb+mizenkei pattern)
      // E.g., 食べ + な → 食べな (ichidan verb pattern)
      //       行 + か + な → 行かな (godan verb mizenkei + な)
      // vs. 危 + な → あぶな (real adjective stem)
      if (stem_len >= 9) {
        std::string_view prev = stem.substr(stem_len - 6, 3);
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
        }
      }
    }
  }

  // I-adjective stems that look like godan verb renyokei (kanji + i-row)
  // Pattern: 書き, 読み, 飲み (2 chars = 6 bytes, ends with i-row hiragana)
  // These are typical godan verb stems, not i-adjective stems
  // This prevents "書きそう" from being parsed as i-adjective "書きい"
  // Apply when the stem could be confused with verb renyokei
  if (type == VerbType::IAdjective && stem_len == 6) {
    std::string_view last = stem.substr(3);  // Last 3 bytes = 1 hiragana
    if (last == "き" || last == "ぎ" || last == "ち" ||
        last == "に" || last == "び" || last == "み" || last == "り" ||
        last == "い") {
      // Check if first char is kanji (typical verb renyokei pattern)
      // But exclude し which is common in real i-adj stems like 美し, 楽し
      if (endsWithKanji(stem.substr(0, 3))) {
        base -= inflection::kPenaltyIAdjGodanRenyokeiPattern;
      }
    }
    // Single-kanji + な stems are usually verb negatives, not adjectives
    // E.g., 見なければ → 見ない (verb negative), not adjective
    //       来なければ → 来ない (verb negative), not adjective
    // Exceptions: 少ない, 危ない are true adjectives (finite, small set)
    // Also penalize hiragana + な (しな, こな = suru/kuru negative)
    if (last == "な") {
      std::string_view first = stem.substr(0, 3);
      if (!endsWithKanji(first)) {
        // Hiragana + な (verb mizenkei like しな, こな)
        base -= inflection::kPenaltyIAdjVerbNegativeNa;
      } else if (first != "少" && first != "危") {
        // Single kanji + な that's NOT a known adjective stem
        // Most are verb negatives (見な, 出な, 来な, 寝な, etc.)
        base -= inflection::kPenaltyIAdjVerbNegativeNa;
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
  if (required_conn == conn::kVerbOnbinkei && stem_len >= 6 &&
      type != VerbType::GodanSa) {
    std::string_view last = stem.substr(stem_len - 3);
    // Skip penalty for GodanRa with わ ending - legitimate pattern for 終わる etc.
    bool is_godan_ra_wa = (type == VerbType::GodanRa && last == "わ");
    if (!is_godan_ra_wa &&
        (last == "か" || last == "が" || last == "さ" || last == "た" ||
         last == "な" || last == "ば" || last == "ま" || last == "ら" ||
         last == "わ")) {
      // Stems ending in a-row are suspicious for onbinkei context
      base -= inflection::kPenaltyOnbinkeiARowStem;
    }
  }

  // Penalty for Godan with e-row stem ending in onbinkei context
  // Pattern like 伝えいた matches GodanKa (伝え + いた → 伝えく)
  // But stems ending in e-row are almost always Ichidan verb renyokei forms
  // Real Godan onbin: 書いた (書く), 飲んだ (飲む) - stems end in kanji
  // This prevents "伝えいた" from being parsed as GodanKa "伝えく"
  if (required_conn == conn::kVerbOnbinkei && stem_len >= 6 &&
      endsWithERow(stem) && type != VerbType::Ichidan) {
    // E-row endings are ichidan stems, not godan
    // 伝え, 食べ, 見せ are all ichidan renyokei forms
    base -= inflection::kPenaltyOnbinkeiERowNonIchidan;
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
  // Skip IAdjective - it has separate handling at line 246-254
  if (stem_len >= 6 && isAllKanji(stem) && type != VerbType::Suru &&
      type != VerbType::IAdjective) {
    // Skip penalty for っ-onbin verbs (GodanWa/Ra/Ta) in onbinkei context
    // These are legitimate Godan verbs, not サ変名詞 misanalyses
    bool is_tsu_onbin_type = (type == VerbType::GodanWa ||
                              type == VerbType::GodanRa ||
                              type == VerbType::GodanTa);
    if (required_conn == conn::kVerbOnbinkei && is_tsu_onbin_type) {
      // No penalty for っ-onbin patterns - these are legitimate Godan verbs
    } else if (required_conn == conn::kVerbKatei) {
      // Lighter penalty for conditional form - 頑張れば, 滑れば are valid Godan
      base -= inflection::kPenaltyAllKanjiNonSuruKatei;
    } else {
      base -= inflection::kPenaltyAllKanjiNonSuruOther;
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
  if (required_conn == conn::kVerbPotential && stem_len == 3 &&
      aux_total_len > 3 && aux_count == 1) {
    if (type != VerbType::Ichidan && type != VerbType::Suru &&
        type != VerbType::Kuru) {
      base += inflection::kBonusGodanPotential;
    }
  }

  // Penalty for GodanBa potential interpretation
  // GodanBa verbs (飛ぶ, 呼ぶ, 遊ぶ, etc.) are rare compared to Ichidan verbs ending in べる
  // (食べる, 調べる, 比べる, etc.). When we see stem + べ in potential context,
  // it's much more likely to be Ichidan than GodanBa potential.
  // Example: 食べなくなった → 食べる (Ichidan) not 食ぶ (non-existent GodanBa)
  if (required_conn == conn::kVerbPotential && type == VerbType::GodanBa) {
    base -= inflection::kPenaltyGodanBaPotential;
  }

  // Penalty for Godan potential with single-kanji stem in compound patterns
  // For simple patterns like "書けない" (aux_count=1), Godan potential is often correct
  // For compound patterns like "食べてもらった" (aux_count>=2), Ichidan is usually correct
  // The べ/え in "食べ" is part of the Ichidan stem, not a potential suffix
  // Penalty scales with aux_count to handle very long compound patterns
  if (required_conn == conn::kVerbPotential && stem_len == 3 && aux_count >= 2) {
    if (type != VerbType::Ichidan && type != VerbType::Suru &&
        type != VerbType::Kuru) {
      // Scale penalty with compound depth
      float penalty = inflection::kPenaltyPotentialCompoundBase +
                      inflection::kPenaltyPotentialCompoundPerAux *
                          static_cast<float>(aux_count - 1);
      base -= std::min(penalty, inflection::kPenaltyPotentialCompoundMax);
    }
  }

  // Penalty for short te-form only matches (て/で alone) with noun-like stems
  // When the only auxiliary is "て" or "で" (3 bytes), it's often a particle, not verb conjugation
  // Pattern: 幸いで → 幸いる (WRONG) vs 幸い + で (particle)
  // But: 食べて → 食べる (CORRECT), やって → やる (CORRECT) are valid
  // Only apply to stems ending in "い" which are typically na-adjectives
  if (type == VerbType::Ichidan && required_conn == conn::kVerbOnbinkei &&
      aux_count == 1 && aux_total_len == 3 && stem_len >= 6) {
    std::string_view last = stem.substr(stem_len - 3);

    // Stems ending in "い" are likely na-adjectives (幸い, 厄介, etc.)
    // These should be parsed as noun + particle, not verb conjugation
    if (last == "い") {
      base -= inflection::kPenaltyTeFormNaAdjective;
    }
  }

  // Suru vs GodanSa disambiguation
  // Multi-kanji stems strongly suggest サ変 verb (勉強する, 準備する)
  // Single-kanji stems (出す, 消す) are typically GodanSa
  // Stems ending in hiragana/katakana suggest Godan verb (話す, 返す)
  if (endsWithKanji(stem)) {
    bool is_shi_context = (required_conn == conn::kVerbRenyokei ||
                           required_conn == conn::kVerbOnbinkei);
    if (is_shi_context) {
      // Only apply Suru boost for 2-kanji stems (6 bytes)
      // Single-kanji stems (3 bytes) like 出す, 消す are GodanSa
      // Longer stems (9+ bytes) might be verb compounds (考え直す)
      if (stem_len == 6) {
        if (type == VerbType::Suru) {
          base += inflection::kBonusSuruTwoKanji;
        } else if (type == VerbType::GodanSa) {
          base -= inflection::kPenaltyGodanSaTwoKanji;
        }
      } else if (stem_len >= 9) {
        // Longer stems (3+ kanji) might be verb compounds - reduce boost
        if (type == VerbType::Suru) {
          base += inflection::kBonusSuruLongStem;
        }
      } else if (stem_len == 3) {
        // Single-kanji stem: prefer GodanSa (出す, 消す, etc.)
        if (type == VerbType::GodanSa) {
          base += inflection::kBonusGodanSaSingleKanji;
        } else if (type == VerbType::Suru) {
          base -= inflection::kPenaltySuruSingleKanji;
        }
      }
    }
    // In mizenkei context for single-kanji, also boost GodanSa
    if (required_conn == conn::kVerbMizenkei && stem_len == 3) {
      if (type == VerbType::GodanSa) {
        base += inflection::kBonusGodanSaSingleKanji;
      }
    }
  }

  // Floor confidence at kConfidenceFloor to allow heavy penalties to differentiate
  // grammatically invalid patterns (e.g., e-row adjective stems like 食べい)
  // from valid patterns. The 0.5 threshold in adjective_candidates.cpp
  // will reject candidates below 0.5.
  return std::min(inflection::kConfidenceCeiling,
                  std::max(inflection::kConfidenceFloor, base));
}

}  // namespace suzume::grammar
