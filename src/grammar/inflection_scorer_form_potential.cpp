#include <algorithm>

#include "char_patterns.h"
#include "connection.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "inflection_scorer_constants.h"
#include "inflection_scorer_internal.h"

#define GET_OPT(field, default_val) \
  (opts ? InflectionScorerOptions::getOrDefault(opts->field, default_val) : default_val)

namespace suzume::grammar::inflection_score_detail {

float scoreAdjectiveAndForm(float base, const InflectionScoreContext& context) {
  SUZUME_UNPACK_INFLECTION_CONTEXT(context);

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
    bool has_aux_pattern = utf8::containsAny(stem, {
                                                       "てしま", "でしま",  // te-form + shimau
                                                       "ている", "でいる",  // te-form + iru
                                                       "ておい", "でおい",  // te-form + oku
                                                       "てき", "でき"       // te-form + kuru
                                                   });
    if (has_aux_pattern) {
      float pen = GET_OPT(penalty_i_adj_verb_aux_pattern, inflection::kPenaltyIAdjVerbAuxPattern);
      base -= pen;
      logConfidenceAdjustment(-pen, "i_adj_verb_aux_pattern");
      // Note: This penalty may be clamped by the floor at return.
      // Additional penalty is applied in scorer.cpp for lattice cost.
    }
  }

  // I-adjective 2-char stems containing subject/object markers are invalid
  // E.g., のが, のを, がお, をか are not valid adjective stems (particle combinations)
  // Note: Longer stems CAN contain が/を in real adjectives (おこがましい, etc.)
  // so we only apply this penalty to short 2-char stems
  if (type == VerbType::IAdjective && stem_len == core::kTwoJapaneseCharBytes) {
    std::string_view first = stem.substr(0, core::kJapaneseCharBytes);
    std::string_view second = stem.substr(core::kJapaneseCharBytes);
    if (utf8::equalsAny(first, {"が", "を"}) || utf8::equalsAny(second, {"が", "を"})) {
      base -= inflection::kPenaltyIAdjEmbeddedParticle;
      logConfidenceAdjustment(-inflection::kPenaltyIAdjEmbeddedParticle, "i_adj_embedded_particle");
    }
  }

  // I-adjective stems ending with "し" are very common (難しい, 美しい, 楽しい, 苦しい)
  // When followed by すぎる/やすい/にくい auxiliaries, boost confidence
  // This helps disambiguate 難しすぎる (難しい + すぎる) vs 難す (Godan-Sa)
  if (type == VerbType::IAdjective && stem_len >= core::kTwoJapaneseCharBytes && aux_count >= 1) {
    std::string_view last = utf8::lastChar(stem);
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
    if (utf8::equalsAny(last6, {"やす", "にく"})) {
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

  // I-adjective 2-kanji stems are rare - most are misanalyzed nouns
  // Words like 勘違い, 間違い end with い but are nouns from verb nominalization
  // Valid 2-kanji adjective stems: 面白 (おもしろい)
  // This prevents "勘違い" from being parsed as adjective
  if (type == VerbType::IAdjective && stem_len == core::kTwoJapaneseCharBytes && isAllKanji(stem)) {
    // Check if in valid exceptions list
    bool is_valid_two_kanji = equalsAny(stem, inflection::kValidTwoKanjiIAdjStems);
    if (!is_valid_two_kanji) {
      base -= inflection::kPenaltyIAdjTwoKanjiStem;
      logConfidenceAdjustment(-inflection::kPenaltyIAdjTwoKanjiStem, "i_adj_two_kanji_stem");
    }
  }

  // I-adjective stems consisting only of 3+ kanji are extremely rare
  // Such stems are usually サ変名詞 (検討, 勉強, 準備) being misanalyzed
  // Real i-adjectives have patterns like: 美しい, 楽しい (kanji + hiragana)
  // This prevents "検討いたす" from being parsed as "検討い" + "たす"
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

  // I-adjective stems ending with る are invalid - verb dictionary form pattern
  // E.g., するそう → するい (invalid), 食べるそう → 食べるい (invalid)
  //       降るそう → 降るい (invalid)
  // These are verb終止形 + そう(hearsay), not i-adjectives
  // Real i-adjectives never have stems ending in る
  if (type == VerbType::IAdjective && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last = utf8::lastChar(stem);
    if (last == "る") {
      float pen = GET_OPT(penalty_i_adj_ru_stem_invalid, inflection::kPenaltyIAdjRuStemInvalid);
      base -= pen;
      logConfidenceAdjustment(-pen, "i_adj_ru_stem_invalid");
    }
  }

  // I-adjective stems ending with "るらし" or "いらし" are likely verb/adj + rashii pattern
  // E.g., 帰るらし + い → should be 帰る + らしい (conjecture auxiliary)
  // E.g., 帰りたいらし + い → should be 帰りたい + らしい
  // This penalty helps split the compound correctly
  if (type == VerbType::IAdjective && stem_len >= core::kThreeJapaneseCharBytes) {
    std::string_view last9 = stem.substr(stem_len - core::kThreeJapaneseCharBytes);
    if (utf8::equalsAny(last9, {"るらし", "いらし"})) {
      float pen = GET_OPT(penalty_i_adj_verb_rashii_pattern, inflection::kPenaltyIAdjVerbRashiiPattern);
      base -= pen;
      logConfidenceAdjustment(-pen, "i_adj_verb_rashii_pattern");
    }
  }

  // I-adjective stems ending with "づ" are invalid
  // "づ" endings are verb onbin patterns (基づ + いて → 基づいて from 基づく)
  // No real i-adjective has a stem ending in づ
  if (type == VerbType::IAdjective && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last = utf8::lastChar(stem);
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
    std::string_view last = utf8::lastChar(stem);
    if (endsWithChar(stem, kMizenkeiEndings, kMizenkeiCount)) {
      // 2-character pure hiragana stems ending in ら are typically verb mizenkei
      // E.g., やら (from やる) + さ + れた = やらされた (causative-passive)
      // Only penalize ら endings - other a-row endings may be valid i-adj stems
      // E.g., やば (やばい), なさ (なさい with そう) are valid i-adjectives
      // Exception: つら (辛い), きら (嫌い) are valid i-adjective stems
      // A bare terminal い/く has no verb counterpart (no irrealis takes い),
      // so it cannot be that pattern (えらい, からい).
      const bool bare_adjective_ending = aux_total_len <= core::kJapaneseCharBytes;
      if (stem_len == core::kTwoJapaneseCharBytes && isPureHiragana(stem) && last == "ら" && !bare_adjective_ending &&
          !equalsAny(stem, inflection::kValidIAdjRaStemExceptions)) {
        base -= inflection::kPenaltyIAdjMizenkeiPattern;
        logConfidenceAdjustment(-inflection::kPenaltyIAdjMizenkeiPattern, "i_adj_2char_ra_stem");
      }
      // Check if there's a hiragana before the a-row ending (verb+mizenkei pattern)
      // E.g., 食べ + な → 食べな (ichidan verb pattern)
      //       行 + か + な → 行かな (godan verb mizenkei + な)
      // vs. 危 + な → あぶな (real adjective stem)
      else if (stem_len >= core::kThreeJapaneseCharBytes) {
        const char32_t previous =
            utf8::decodeFirstChar(stem.substr(stem_len - core::kTwoJapaneseCharBytes, core::kJapaneseCharBytes));
        // A/i/e-row kana cover Godan mizenkei/renyokei and Ichidan stems.
        // Use the canonical vowel predicates so や and へ cannot drift out of
        // a hand-maintained surface list. 促音 is the independent onbin marker.
        if (isARowCodepoint(previous) || isIRowCodepoint(previous) || isERowCodepoint(previous) || previous == U'っ') {
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
    std::string_view last = utf8::lastChar(stem);  // Last 3 bytes = 1 hiragana
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
    } else if (utf8::equalsAny(last, {"ぎ", "ち", "に", "び", "み", "り", "い"})) {
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
  // Exception: わ is part of the stem both for GodanRa verbs such as 終わる
  // and GodanWa verbs such as 味わう: 終わ+った, 味わ+った.
  if (required_conn == conn::kVerbOnbinkei && stem_len >= core::kTwoJapaneseCharBytes && type != VerbType::GodanSa) {
    std::string_view last = utf8::lastChar(stem);
    const bool is_godan_wa_stem = (type == VerbType::GodanRa || type == VerbType::GodanWa) && last == "わ";
    if (!is_godan_wa_stem && endsWithChar(stem, kMizenkeiEndings, kMizenkeiCount)) {
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
  if (required_conn == conn::kVerbOnbinkei && stem_len >= core::kTwoJapaneseCharBytes && endsWithERow(stem) &&
      type != VerbType::Ichidan) {
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
    bool is_tsu_onbin_type = (type == VerbType::GodanWa || type == VerbType::GodanRa || type == VerbType::GodanTa);
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

  return base;
}

float scorePotentialAndSuru(float base, const InflectionScoreContext& context) {
  SUZUME_UNPACK_INFLECTION_CONTEXT(context);

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
    if (type != VerbType::Ichidan && type != VerbType::Suru && type != VerbType::Kuru) {
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
    if (type != VerbType::Ichidan && type != VerbType::Suru && type != VerbType::Kuru) {
      // Scale penalty with compound depth
      float penalty = inflection::kPenaltyPotentialCompoundBase +
                      inflection::kPenaltyPotentialCompoundPerAux * static_cast<float>(aux_count - 1);
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
  if (type == VerbType::Ichidan && required_conn == conn::kVerbOnbinkei && aux_count == 1 &&
      aux_total_len == core::kJapaneseCharBytes && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last = utf8::lastChar(stem);

    // Stems ending in "い" are likely na-adjectives (幸い, 厄介, etc.)
    // These should be parsed as noun + particle, not verb conjugation.
    // Exception: kanji + い kami-ichidan verbs (率いた, 報いた, 老いた) are real
    // te/ta-forms of 率いる/報いる/老いる, guarded by the shared exception set.
    if (last == "い" && !inflection::isValidKanjiIStemException(stem)) {
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
  if (type == VerbType::Ichidan && required_conn == conn::kVerbMizenkei && stem_len >= core::kTwoJapaneseCharBytes) {
    std::string_view last = utf8::lastChar(stem);
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
  if (type == VerbType::Suru && stem_len >= core::kTwoJapaneseCharBytes && required_conn == conn::kVerbOnbinkei) {
    std::string_view last_char = utf8::lastChar(stem);
    if (utf8::equalsAny(last_char, {"っ", "ん", "い"})) {
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
    bool is_direct_nai_pattern =
        has_empty_verb_suffix && (aux_total_len == 6 || aux_total_len == 12);  // ない or なかった
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
    if (type == VerbType::Suru && required_conn == conn::kVerbBase && suffix_len == core::kJapaneseCharBytes) {
      // Strong penalty: Suru verbs don't end in just す in base form
      // 勉強する (not 勉強す), 準備する (not 準備す)
      base -= inflection::kPenaltyGodanSaTwoKanji;  // Reuse existing constant
      logConfidenceAdjustment(-inflection::kPenaltyGodanSaTwoKanji, "suru_short_suffix_base");
    }

    bool is_shi_context = (required_conn == conn::kVerbRenyokei || required_conn == conn::kVerbOnbinkei);
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
      if (utf8::containsAny(stem, {"て", "で"})) {
        base -= inflection::kPenaltySuruTeFormStemInvalid;
        logConfidenceAdjustment(-inflection::kPenaltySuruTeFormStemInvalid, "suru_te_form_stem_invalid");
      }
    }
  }
  return base;
}

}  // namespace suzume::grammar::inflection_score_detail

#undef GET_OPT
