/**
 * @file verb_candidates_kanji_onbin_extended.cpp
 * @brief Extended kanji sokuonbin candidate patterns
 */

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_kanji_internal.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/conjugation.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/utf8.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis::kanji_verb_detail {
namespace vh = verb_helpers;

// The 促音便 base is lexically ラ/ワ/タ-ambiguous from the っ surface alone, so
// default to ラ行 (閉まる/走る) but prefer a dictionary-verified ワ行 base (向かう
// over the non-word 向かる) when the dictionary carries it. Shared by the
// trailing-っ (extended_sokuonbin) and mid-surface (te_aux_sokuonbin) paths.
struct SokuonbinBase {
  std::string base;
  grammar::VerbType type;
};
SokuonbinBase resolveSokuonbinBase(const dictionary::DictionaryManager* dict_manager, const std::string& stem) {
  std::string godan_wa_base = stem + "う";
  if (vh::isVerbInDictionary(dict_manager, godan_wa_base)) {
    return {godan_wa_base, grammar::VerbType::GodanWa};
  }
  return {stem + "る", grammar::VerbType::GodanRa};
}

bool isGodanTerminalEnding(char32_t codepoint) {
  for (const auto& [verb_type, row] : grammar::Conjugation::getGodanRows()) {
    static_cast<void>(verb_type);
    if (row.base_vowel == codepoint) {
      return true;
    }
  }
  return false;
}

bool hasStandaloneVerbTail(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                           size_t tail_start, size_t tail_end) {
  if (dict_manager == nullptr || tail_start >= tail_end) {
    return false;
  }
  return vh::isVerbInDictionary(dict_manager, extractSubstring(codepoints, tail_start, tail_end));
}

bool hasClosedAuxiliaryTail(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                            size_t tail_start, size_t tail_end) {
  return dict_manager != nullptr && tail_start < tail_end &&
         lookupEntryInRange(*dict_manager, codepoints, tail_start, tail_end, core::PartOfSpeech::Auxiliary) != nullptr;
}

/**
 * @brief Whether the past auxiliary already closes a verified predicate before っ.
 *
 * 食べ+た+って and 書い+た+って put the concessive particle って behind a finished
 * past form, so the っ scanned here belongs to the particle rather than to a
 * second onbin of one long verb. A genuine stem that merely ends in た before its
 * own onbin (隔たっ) has no verified predicate in front of that kana.
 * @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
 */
bool pastAuxiliaryClosesPredicateBefore(const grammar::Inflection& inflection,
                                        const dictionary::DictionaryManager* dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start_pos, size_t onbin_pos) {
  // A one-codepoint predicate is a bare kanji, which is the shape of a stem
  // whose own okurigana happens to be た (隔+たっ+て); require the okurigana to
  // be present so only a complete predicate counts.
  if (dict_manager == nullptr || onbin_pos < start_pos + 3) {
    return false;
  }
  const char32_t past = codepoints[onbin_pos - 1];
  if (past != U'た' && past != U'だ') {
    return false;
  }
  // Keep the past auxiliary in the probe.  The boundary under examination is
  // immediately before the quotative っ, so `届いたっ` must be recognized as
  // the complete predicate `届いた` followed by `って`, rather than asking an
  // inflection analyser to infer a predicate from the bare continuative
  // `届い`.
  const std::string predicate = extractSubstring(codepoints, start_pos, onbin_pos);
  if (vh::isVerbInDictionary(dict_manager, predicate)) {
    return true;
  }
  for (const auto& result : inflection.analyze(predicate)) {
    if (result.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
      return true;
    }
  }
  return false;
}

bool hasVerifiedInternalOnbinPredicate(const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       const std::vector<char32_t>& codepoints, size_t tail_start, size_t onbin_pos,
                                       size_t tense_end) {
  if (dict_manager == nullptr || tail_start >= onbin_pos) {
    return false;
  }
  for (size_t boundary = tail_start + 1; boundary < onbin_pos; ++boundary) {
    const std::string tail = extractSubstring(codepoints, boundary, tense_end);
    for (const auto& result : inflection.analyze(tail)) {
      if (result.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence &&
          vh::isVerbInDictionary(dict_manager, result.base_form)) {
        return true;
      }
    }
  }
  return false;
}
// Fallback verification for a 促音便 base when it is not in the dictionary:
// accept only a GodanRa analysis of the complete closed past form whose
// reconstructed base is exactly the candidate base.  This extends the
// existing one-okurigana fallback to the same bounded two-okurigana shape,
// without turning arbitrary long auxiliary chains into one predicate.
bool sokuonbinInflVerified(const grammar::Inflection& inflection, const std::string& onbin_surface,
                           const std::string& potential_base, size_t hiragana_before_onbin) {
  if (hiragana_before_onbin == 0) {
    return false;
  }
  for (const auto& result : inflection.analyze(onbin_surface + "た")) {
    if (result.verb_type == grammar::VerbType::GodanRa && result.base_form == potential_base &&
        result.confidence >= candidate::verb_cost::kKanjiSokuonbinMinConfidence) {
      return true;
    }
  }
  return false;
}
void appendExtendedSokuonbinCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates) {
  // Generate Godan sokuonbin (っ) candidates for extended patterns
  // E.g., 閉まった → 閉まっ (onbin of 閉まる) + た (auxiliary)
  //       始まった → 始まっ (onbin of 始まる) + た (auxiliary)
  //       決まった → 決まっ (onbin of 決まる) + た (auxiliary)
  // Key patterns:
  // - kanji + hiragana + っ + た/て: GodanRa verbs with multi-char stem
  // Constraints:
  // - Hiragana portion (before っ) should be 1-2 chars (まった, まりった)
  // - Base form must exist in dictionary (prevents false positives)
  // - Require at least kanji + 2 hiragana (e.g., 閉まっ = 閉 + ま + っ)
  if (hiragana_end - kanji_end >= 2) {
    // Check for pattern ending in っ + た/て
    // Look for っ at position hiragana_end - 2 (second to last)
    char32_t second_last = codepoints[hiragana_end - 2];
    char32_t last_char = codepoints[hiragana_end - 1];
    bool is_sokuonbin_te_ta = (second_last == U'っ' && (last_char == U'た' || last_char == U'て'));
    // Hiragana between kanji and っ should be 1-2 chars. Longer spans can
    // contain productive auxiliary or compound boundaries and are handled by
    // their dedicated generators.
    // hiragana_end - kanji_end = total hiragana chars (including っ and た/て)
    // So hiragana before っ = (hiragana_end - kanji_end) - 2
    size_t hiragana_before_onbin = (hiragana_end - kanji_end) - 2;
    bool reasonable_length = (hiragana_before_onbin >= 1 && hiragana_before_onbin <= 2);
    if (is_sokuonbin_te_ta && reasonable_length && hiragana_end - kanji_end >= 3) {
      // We have kanji + 1-2 hiragana + っ + た/て
      // Generate candidate for kanji + hiragana + っ (without the た/て)
      size_t onbin_end = hiragana_end - 1;  // Position after っ
      std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);

      // Skip if hiragana portion is なかっ (negative past pattern: なかっ+た)
      // This prevents false positives like 来なかった → 来なかっ+た (来なかる doesn't exist)
      // The correct split is 来 + なかっ + た (kuru + negative aux + past)
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, onbin_end);
      const bool crosses_completed_past =
          pastAuxiliaryClosesPredicateBefore(inflection, dict_manager, codepoints, start_pos, onbin_end - 1);
      if (hasClosedAuxiliaryTail(dict_manager, codepoints, kanji_end, onbin_end) ||
          hasVerifiedInternalOnbinPredicate(inflection, dict_manager, codepoints, kanji_end, onbin_end - 1,
                                            hiragana_end)) {
        // Preserve an already complete auxiliary or embedded dictionary verb
        // boundary (見+たがっ, 早く+いっ) instead of absorbing it into X...る.
      } else if (hiragana_part == "なかっ") {
        // This is negative past, not extended sokuonbin - skip
      } else if (hiragana_part == "であっ") {
        // This is copula である pattern (重要であった = 重要 + で + あっ + た)
        // Skip candidate generation to allow proper copula splitting
      } else if (utf8::startsWith(hiragana_part, "といっ")) {
        // Skip と+いっ pattern - this is particle と + verb いう
        // E.g., 友人といった = 友人 + と + いっ + た, not 友人といる
      } else if (hiragana_part == "くなっ") {
        // Skip く+なっ pattern - this is i-adjective adverbial + なる verb
        // E.g., 良くなった = 良く + なっ + た, not 良くなる as single verb
        // MeCab splits: 高くなった → 高く + なっ + た
      } else {
        // Skip godan終止形 + っ + て pattern - this is verb + って(quotative)
        // E.g., 行くって = 行く + って, 食べるって = 食べる + って
        // Godan 終止形 endings: く, す, つ, う, ぐ, ぶ, む, ぬ, る
        // The sokuonbin of godan verbs drops the ending (行く→行っ), not adds っ (行くっ is invalid)
        // Check if hiragana_part ends with godan終止形 + っ
        bool is_quotative_pattern = false;
        if (last_char == U'て' && hiragana_part.size() >= 6 /* at least 2 chars: Xっ */) {
          // Get the character before っ (second to last in hiragana_part)
          // hiragana_part ends with っ (which is at onbin_end - 1)
          // The char before っ is at position onbin_end - 2
          char32_t char_before_sokuon = codepoints[onbin_end - 2];
          is_quotative_pattern =
              (char_before_sokuon == U'く' || char_before_sokuon == U'す' || char_before_sokuon == U'つ' ||
               char_before_sokuon == U'う' || char_before_sokuon == U'ぐ' || char_before_sokuon == U'ぶ' ||
               char_before_sokuon == U'む' || char_before_sokuon == U'ぬ' || char_before_sokuon == U'る');
        }
        if (is_quotative_pattern) {
          // Skip: this is likely quotative って, not extended sokuonbin
        } else {
          // Build potential base form and verify it exists in dictionary or inflection
          // This prevents false positives like 食べてしまる
          std::string stem = extractSubstring(codepoints, start_pos, onbin_end - 1);
          const SokuonbinBase sokuon = resolveSokuonbinBase(dict_manager, stem);
          const std::string& potential_base = sokuon.base;
          const grammar::VerbType onbin_verb_type = sokuon.type;

          // Skip if hiragana before っ is だ (copula pattern)
          // E.g., 本だった = 本 + だっ + た (noun + copula), not 本だる (verb)
          // But 閉まった = 閉まっ + た (verb 閉まる) is valid
          char32_t char_before_sokuon = codepoints[hiragana_end - 3];
          if (char_before_sokuon == U'だ') {
            // This is a copula pattern (NOUN + だった), not a verb
            // Skip candidate generation
          } else {
            // Check dictionary first
            bool in_dict = vh::isVerbInDictionary(dict_manager, potential_base);

            // Fallback: exact full-form inflection evidence for productive
            // verbs absent from the dictionary.  Initial particle-like morae
            // have already been rejected by generateVerbCandidates(), so a
            // phrase such as N+が+V cannot enter through this path.
            // 〜かっ is shared by Godan-ra sokuonbin and i-adjective past.
            // Without lexical evidence the row is not recoverable, so do not
            // let a mechanical verb analysis steal the adjective path.
            const bool ambiguous_katt = char_before_sokuon == U'か';
            bool infl_verified =
                !in_dict && !ambiguous_katt &&
                sokuonbinInflVerified(inflection, onbin_surface, potential_base, hiragana_before_onbin);
            const bool standalone_verb_tail = hasStandaloneVerbTail(dict_manager, codepoints, kanji_end, onbin_end);

            // Skip if this is an i-adjective katt-form (美しかっ → 美しい, 高かっ → 高い)
            // The stem ends with か, so remove か and add い to get adjective base form
            // E.g., stem="美しか" → adj_base="美しい"
            bool is_adj_katt_form = false;
            if (stem.size() >= core::kTwoJapaneseCharBytes && utf8::endsWith(stem, "か")) {
              std::string adj_base = stem.substr(0, stem.size() - core::kJapaneseCharBytes) + "い";
              if (vh::isAdjectiveInDictionary(dict_manager, adj_base)) {
                is_adj_katt_form = true;
                SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] " << onbin_surface << " skip: i-adj \"" << adj_base
                                                        << "\" in dict\n");
              }
            }

            if (!is_adj_katt_form && (in_dict || infl_verified)) {
              // Verified - generate candidate
              float cost = candidate::verb_cost::kModerateBonus;
              if (crosses_completed_past) {
                // A completed predicate before the quotative って is stronger
                // evidence than a fabricated long-verb analysis. Keep the
                // candidate available, but price it out of that boundary.
                cost += candidate::verb_cost::kGeneratedSpanParticlePenalty;
              }
              SUZUME_DEBUG_VERBOSE_BLOCK {
                SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface << " extended_sokuonbin lemma=" << potential_base
                                    << (in_dict ? " [dict]" : " [infl]") << " cost=" << cost << "\n";
              }
              auto candidate =
                  makeVerbCandidate(onbin_surface, start_pos, onbin_end, cost, potential_base,
                                    grammar::verbTypeToConjType(onbin_verb_type), true, CandidateOrigin::VerbKanji,
                                    0.9F, "extended_sokuonbin", core::ExtendedPOS::VerbOnbinkei);
              // A standalone dictionary verb tail supplies a grammatical
              // boundary, so an inflection-only compound must remain
              // unverified and receive the generic false-positive penalty.
              candidate.lemma_verified =
                  in_dict || (infl_verified && kanji_end == start_pos + 1 && !standalone_verb_tail);
              candidates.push_back(std::move(candidate));
            }
          }  // end else (not copula だ pattern)
        }  // end else (not quotative って pattern)
      }  // end else (not なかっ pattern)
    }
  }

  // Generate sokuonbin (っ) candidates from surfaces containing て+auxiliary chains
  // E.g., 挙がっている → 挙がっ (onbin of 挙がる) + て + いる
  //       集まってくる → 集まっ (onbin of 集まる) + て + くる
  // This handles patterns where っ+て/で is followed by auxiliary verbs,
  // which the basic/extended sokuonbin sections miss (they only handle endings)
  if (hiragana_end - kanji_end >= 3) {
    // Scan for っ in hiragana portion (not at the very end - that's handled above)
    for (size_t pos = kanji_end; pos + 2 < hiragana_end; ++pos) {
      if (codepoints[pos] != U'っ')
        continue;
      char32_t after_sokuon = codepoints[pos + 1];
      if (after_sokuon != U'て' && after_sokuon != U'で')
        continue;
      // Found っ+て/で NOT at end of surface - check if followed by auxiliary
      size_t hiragana_before_onbin = pos - kanji_end;
      if (hiragana_before_onbin < 1 || hiragana_before_onbin > 2)
        continue;

      size_t onbin_end = pos + 1;  // Position after っ
      std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);
      std::string stem = extractSubstring(codepoints, start_pos, pos);
      const SokuonbinBase sokuon = resolveSokuonbinBase(dict_manager, stem);
      const std::string& potential_base = sokuon.base;
      const grammar::VerbType onbin_verb_type = sokuon.type;

      // Check hiragana part for known false patterns
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, onbin_end);
      // 書い+た+って: the っ scanned here belongs to the concessive particle, not
      // to an onbin stem, whenever a complete auxiliary already sits on the
      // stem's own onbin kana.
      // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
      if (hasClosedAuxiliaryTail(dict_manager, codepoints, kanji_end, onbin_end) ||
          vh::embedsAuxiliaryOnOnbinStem(codepoints, kanji_end, pos, dict_manager) ||
          pastAuxiliaryClosesPredicateBefore(inflection, dict_manager, codepoints, start_pos, pos) ||
          hasVerifiedInternalOnbinPredicate(inflection, dict_manager, codepoints, kanji_end, pos, pos + 2) ||
          hiragana_part == "なかっ" || hiragana_part == "であっ" || utf8::startsWith(hiragana_part, "といっ") ||
          hiragana_part == "くなっ") {
        continue;
      }

      // A terminal predicate followed by って is a colloquial quotation
      // (読む+っていう), rather than a te-form that continues into an auxiliary.
      if (after_sokuon == U'て' && isGodanTerminalEnding(codepoints[pos - 1])) {
        continue;
      }

      bool in_dict_check = vh::isVerbInDictionary(dict_manager, potential_base);
      bool infl_verified =
          !in_dict_check && sokuonbinInflVerified(inflection, onbin_surface, potential_base, hiragana_before_onbin);
      const bool standalone_verb_tail = hasStandaloneVerbTail(dict_manager, codepoints, kanji_end, onbin_end);

      if (in_dict_check || infl_verified) {
        constexpr float kTeAuxSokuonbinCost = candidate::verb_cost::kModerateBonus;
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface << " te_aux_sokuonbin lemma=" << potential_base
                              << (in_dict_check ? " [dict]" : " [infl]") << " cost=" << kTeAuxSokuonbinCost << "\n";
        }
        auto candidate =
            makeVerbCandidate(onbin_surface, start_pos, onbin_end, kTeAuxSokuonbinCost, potential_base,
                              grammar::verbTypeToConjType(onbin_verb_type), true, CandidateOrigin::VerbKanji, 0.9F,
                              "te_aux_sokuonbin", core::ExtendedPOS::VerbOnbinkei);
        candidate.lemma_verified =
            in_dict_check || (infl_verified && kanji_end == start_pos + 1 && !standalone_verb_tail);
        candidates.push_back(std::move(candidate));
      }
      break;  // Only process first っ+て/で occurrence
    }
  }
}

}  // namespace suzume::analysis::kanji_verb_detail
