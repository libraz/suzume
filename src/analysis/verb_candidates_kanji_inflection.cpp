/**
 * @file verb_candidates_kanji_inflection.cpp
 * @brief General inflection-analyzed kanji verb candidates
 */

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_kanji_internal.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis::kanji_verb_detail {
namespace vh = verb_helpers;

namespace {

bool hasInternalPredicateBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                  size_t end_pos, const grammar::Inflection& inflection) {
  if (kanji_end <= start_pos + 1) {
    return false;
  }
  for (size_t predicate_start = start_pos + 1; predicate_start < kanji_end; ++predicate_start) {
    // A bare single-kanji ichidan V1 is a productive compound-verb stem
    // (見+極める, 見+送る), not a nominal host.  Its compound candidate is
    // generated independently, but the complete finite edge must remain
    // available here as well.
    if (predicate_start == start_pos + 1 && vh::isSingleKanjiIchidan(codepoints[start_pos])) {
      continue;
    }
    const std::string predicate = extractSubstring(codepoints, predicate_start, end_pos);
    for (const auto& analysis : inflection.analyze(predicate)) {
      if (analysis.verb_type != grammar::VerbType::IAdjective && analysis.base_form == predicate &&
          analysis.confidence >= candidate::kIAdjConfMin) {
        return true;
      }
    }
  }
  return false;
}

// Whether the okurigana a mixed-script stem absorbs already closes a predicate
// of its own. The okurigana of a k-row verb belongs to that verb (羽+ばた+く),
// but the same stem search will just as happily swallow the continuative of an
// embedded predicate: 登録+し is サ変, so a stem reaching past it reconstructs
// the non-word 登録しとく out of three morphemes. サ変 is the paradigm that makes
// this possible — its continuative is one mora and it attaches to any nominal
// kanji run — so that is where the boundary has to be respected.
bool absorbsInnerPredicate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end, size_t stem_end,
                           const grammar::Inflection& inflection) {
  for (size_t inner_end = kanji_end + 1; inner_end <= stem_end; ++inner_end) {
    const std::string inner = extractSubstring(codepoints, start_pos, inner_end);
    for (const auto& analysis : inflection.analyze(inner)) {
      if (analysis.verb_type == grammar::VerbType::Suru && analysis.confidence >= candidate::kIAdjConfMin) {
        return true;
      }
    }
  }
  return false;
}

bool hasAttestedDeverbalNominalization(const grammar::InflectionCandidate& candidate,
                                       const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return false;
  }
  std::string renyokei = candidate.stem;
  if (const auto* godan_row = grammar::Conjugation::getGodanRow(candidate.verb_type); godan_row != nullptr) {
    renyokei += normalize::encodeUtf8(godan_row->i_row);
  }
  return dict_manager->lookupExact(renyokei, core::PartOfSpeech::Noun) != nullptr;
}

}  // namespace

void appendAnalyzedKanjiVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       const VerbCandidateOptions& verb_opts, bool sokuonbin_stem_verified,
                                       const std::string& sokuonbin_lemma, std::vector<UnknownCandidate>& candidates) {
  // A completed reduplicative noun provides a reliable predicate boundary:
  // 月々支払う, 人々集まる.  Permit a conservatively scored compound verb in
  // this context even when its open-class inflection confidence only reaches
  // the dictionary threshold.
  const bool follows_reduplicated_noun = start_pos >= 2 && normalize::isIterationMark(codepoints[start_pos - 1]) &&
                                         normalize::isKanjiCodepoint(codepoints[start_pos - 2]);
  const bool has_conjunctive_initial =
      vh::hasConjunctiveParticleDictionaryEntry(dict_manager, normalize::encodeUtf8(codepoints[kanji_end]));

  // Try different stem lengths. Most verbs use a kanji-only stem, while
  // ichidan verbs add one hiragana and productive mixed-script k-row stems can
  // contain a longer okurigana portion (羽ばた+く/いた/きます).
  for (size_t stem_end = kanji_end; stem_end < hiragana_end; ++stem_end) {
    // Try different ending lengths, starting from longest
    for (size_t end_pos = hiragana_end; end_pos > stem_end; --end_pos) {
      std::string surface = extractSubstring(codepoints, start_pos, end_pos);

      if (surface.empty()) {
        continue;
      }

      // Check for particle/copula patterns that should NOT be treated as verbs
      // Kanji + particle or copula (で, に, を, が, は, も, へ, と, や, か, の, etc.)
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, end_pos);
      if (normalize::isParticleOrCopula(hiragana_part)) {
        continue;  // Skip particle/copula patterns
      }

      // Skip patterns where hiragana part is a known suffix in dictionary
      // (e.g., たち, さん, ら, etc.) - let NOUN+suffix split win instead
      // For multi-kanji stems (2+ kanji), skip any suffix pattern
      // For single-kanji stems, only skip Suffix POS entries (さん, 様, etc.)
      // This allows verb renyokei like 立ち (立つ) while blocking 姉さん
      // Note: Only skip for OTHER (suffixes), not VERB (する is a verb, not suffix)
      // Exception: さ followed by れ/せ is godan-sa mizenkei + passive/causative,
      // not nominalization suffix (騙される, 話される, 殺させる)
      bool is_suffix_pattern = false;
      if (dict_manager != nullptr) {
        auto suffix_results = dict_manager->lookup(hiragana_part, 0);
        for (const auto& result : suffix_results) {
          if (result.entry != nullptr && result.entry->surface == hiragana_part) {
            // For single-kanji stems, only skip if POS is Suffix (honorifics like さん)
            // For multi-kanji stems, skip any suffix pattern
            bool is_suffix_pos = (result.entry->pos == core::PartOfSpeech::Suffix);
            bool is_multi_kanji = (kanji_end - start_pos >= 2);
            if (is_suffix_pos || (is_multi_kanji && (result.entry->extended_pos == core::ExtendedPOS::Suffix ||
                                                     result.entry->pos == core::PartOfSpeech::Other))) {
              // Exception: さ + れ/せ is godan-sa mizenkei + passive/causative
              if (hiragana_part == "さ" && end_pos < codepoints.size()) {
                char32_t next_char = codepoints[end_pos];
                if (next_char == U'れ' || next_char == U'せ') {
                  break;  // Not a suffix - godan-sa verb pattern
                }
              }
              // This hiragana part is a registered suffix - skip verb candidate
              is_suffix_pattern = true;
              break;
            }
          }
        }
      }
      if (is_suffix_pattern) {
        continue;
      }

      // Skip patterns that contain ください (polite request auxiliary)
      // e.g., 待ちください → 待ち + ください, not 待ちく + ださい
      // This prevents false compound verb analysis like 待ちく (待つ+来る renyokei)
      if (hiragana_part.find("ください") != std::string::npos || hiragana_part.find("くださ") != std::string::npos) {
        continue;  // Skip - let VERB + ください split win
      }

      // Skip patterns that extend past te-form boundary into auxiliaries
      // e.g., 履いてない → 履い + て + ない, not a single verb
      //        着ている → 着 + て + いる, not a single verb
      //        飲んでいた → 飲ん + で + いた, not a single verb
      // Detect: onbin ending (い/っ/ん) + て/で + auxiliary content (ない/いる/いた/ある/しまう etc.)
      {
        auto te_pos = hiragana_part.find("て");
        if (te_pos == std::string::npos) {
          te_pos = hiragana_part.find("で");
        }
        if (te_pos != std::string::npos && te_pos >= core::kJapaneseCharBytes) {
          // Check if there's auxiliary content after て/で
          std::string after_te = hiragana_part.substr(te_pos + core::kJapaneseCharBytes);
          if (!after_te.empty()) {
            // Check if char before て/で is onbin ending (い/っ/ん) or
            // godan-sa renyokei (し) — e.g., 過ごしてみた → 過ごし+て+み+た
            std::string_view before_te(hiragana_part.data() + te_pos - core::kJapaneseCharBytes,
                                       core::kJapaneseCharBytes);
            if (before_te == "い" || before_te == "っ" || before_te == "ん" || before_te == "し") {
              continue;  // Skip - let verb + て + auxiliary split win
            }
          }
        }
      }

      // Skip patterns ending with く when followed by ださ (part of ください /
      // くださる / くださいます): 待ちく + ださい should stay 待ち + ください.
      // Only the ださ onset identifies that auxiliary. A bare だ after the く is
      // the copula opening its own predicate (届く + だろう), and treating it as
      // evidence for ください suppressed the k-row terminal of every kanji verb
      // that a copula follows.
      {
        size_t hira_size = hiragana_part.size();
        if (hira_size >= core::kJapaneseCharBytes) {
          std::string_view last_char_view(hiragana_part.data() + hira_size - core::kJapaneseCharBytes,
                                          core::kJapaneseCharBytes);
          if (last_char_view == "く" && end_pos < codepoints.size()) {
            std::string remaining = extractSubstring(codepoints, end_pos, std::min(end_pos + 3, codepoints.size()));
            if (remaining.compare(0, 6, "ださ") == 0) {
              continue;  // Skip - likely part of ください pattern
            }
          }
        }
      }

      // Skip patterns that end with particles (noun renyokei + particle)
      // e.g., 切りに (切り + に), 飲みに (飲み + に), 行きに (行き + に)
      // These are nominalized verb stems followed by particles, not verb forms
      size_t hp_size = hiragana_part.size();
      if (hp_size >= core::kTwoJapaneseCharBytes) {  // At least 2 hiragana
        // Get last hiragana character (particle candidate)
        char32_t last_char = codepoints[end_pos - 1];
        if (normalize::isParticleCodepoint(last_char)) {
          // Check if the preceding part could be a valid verb renyokei
          // Renyokei typically ends in い/り/き/ぎ/し/み/び/ち/に
          char32_t second_last_char = codepoints[end_pos - 2];
          if (second_last_char == U'い' || second_last_char == U'り' || second_last_char == U'き' ||
              second_last_char == U'ぎ' || second_last_char == U'し' || second_last_char == U'み' ||
              second_last_char == U'び' || second_last_char == U'ち') {
            continue;  // Skip - likely nominalized noun + particle
          }
        }
      }

      // Check if this looks like a conjugated verb
      // Get all inflection candidates, not just the best one
      // This handles cases where the best candidate has wrong stem but a lower-ranked
      // candidate has the correct stem (e.g., 見なければ where 見なける wins over 見る)
      const auto& inflection_results = inflection.analyze(surface);
      std::string expected_stem = extractSubstring(codepoints, start_pos, stem_end);

      // Find a candidate with matching stem and sufficient confidence
      // Prefer dictionary-verified candidates when multiple have similar confidence
      // This handles ambiguous っ-onbin patterns like 待って (待つ/待る/待う)
      grammar::InflectionCandidate best;
      best.confidence = candidate::kNoConfidence;
      grammar::InflectionCandidate dict_verified_best;
      dict_verified_best.confidence = candidate::kNoConfidence;

      for (const auto& cand : inflection_results) {
        // Skip candidates from のだ/んだ stripping — these should be split tokens
        if (cand.has_explanatory_suffix)
          continue;

        // Use lower threshold for ichidan verbs with i-row stems (感じる, 信じる)
        // These get ichidan_kanji_i_row_stem penalty which reduces confidence
        // But NOT for e-row stems (て/で), which are often te-form splits
        // Also NOT for single-kanji + い patterns (人い → 人 + いる, not a verb)
        // Single-kanji + い patterns (人い) are excluded: almost always NOUN + いる,
        // not a single verb. Valid ichidan stems are multi-char (感じ, 信じ, etc.).
        bool is_i_row_ichidan = cand.verb_type == grammar::VerbType::Ichidan && vh::isValidIRowIchidanStem(cand.stem);
        const bool has_mixed_godan_ka_stem =
            has_conjunctive_initial && stem_end > kanji_end + 1 && cand.verb_type == grammar::VerbType::GodanKa;
        float conf_threshold = (is_i_row_ichidan || sokuonbin_stem_verified)
                                   ? verb_opts.confidence_ichidan_dict
                                   : (has_mixed_godan_ka_stem ? (utf8::startsWith(cand.suffix, "いた") ||
                                                                         utf8::startsWith(cand.suffix, "いて")
                                                                     ? verb_opts.confidence_past_te
                                                                     : verb_opts.confidence_low)
                                                              : verb_opts.confidence_standard);
        bool is_multi_kanji_godan_wa_renyokei = cand.verb_type == grammar::VerbType::GodanWa &&
                                                utf8::endsWith(surface, "い") &&
                                                normalize::utf8Length(cand.stem) >= 2 && end_pos < codepoints.size() &&
                                                normalize::isKanjiCodepoint(codepoints[end_pos]);
        if (cand.stem == expected_stem && (stem_end <= kanji_end + 1 || has_mixed_godan_ka_stem) &&
            (cand.confidence > conf_threshold ||
             (follows_reduplicated_noun && cand.confidence >= verb_opts.confidence_ichidan_dict) ||
             (is_multi_kanji_godan_wa_renyokei && cand.confidence >= verb_opts.confidence_ichidan_dict)) &&
            cand.verb_type != grammar::VerbType::IAdjective) {
          // Check whether this candidate's base form exists in the dictionary as a
          // verb. The lookup is by surface, so disambiguation among っ-onbin types
          // (GodanRa/Ta/Wa/Ka) comes from each candidate carrying its own base_form
          // (e.g. 経る vs 経つ), not from a type-aware lookup.
          bool in_dict = vh::isVerbInDictionary(dict_manager, cand.base_form);

          if (in_dict) {
            // Prefer dictionary-verified candidates
            if (cand.confidence > dict_verified_best.confidence) {
              dict_verified_best = cand;
            }
          }
          if (cand.confidence > best.confidence) {
            best = cand;
          }
        }
      }

      // Use dictionary-verified candidate if available
      // Dictionary verification trumps confidence penalties from hiragana stems
      bool is_dict_verified = dict_verified_best.confidence > candidate::kNoConfidence;
      if (is_dict_verified) {
        best = dict_verified_best;
      }
      // A sokuonbin compound (突っ走る) is absent from the dictionary but was built
      // over a verified embedded verb; treat it as verified for the proceed gate so
      // its bare 終止形/意志形 (conf ~0.45) is not dropped by the standard threshold.
      is_dict_verified = is_dict_verified || sokuonbin_stem_verified;

      // An unregistered whole-span verb must not absorb a nominal host when the
      // remainder is already a complete predicate (花+咲く, 山+登る). Registered
      // lexical compounds and verbs attested through a deverbal nominalization
      // retain their whole-span reading, and the independently generated inner
      // verb edge supplies the productive noun + predicate path.
      if (!is_dict_verified && !follows_reduplicated_noun && !hasAttestedDeverbalNominalization(best, dict_manager) &&
          hasInternalPredicateBoundary(codepoints, start_pos, kanji_end, end_pos, inflection)) {
        continue;
      }

      // A mixed-script k-row stem carries its lexical okurigana before the
      // い-onbin.  The ordinary onbin generator assumes a kanji-only stem, so
      // emit the same grammatical boundary here from the complete いた/いて
      // cell (羽ばたい+た/て).
      const bool has_mixed_godan_ka_stem =
          has_conjunctive_initial && stem_end > kanji_end + 1 && best.verb_type == grammar::VerbType::GodanKa;
      if (has_mixed_godan_ka_stem && (best.suffix == "いた" || best.suffix == "いて") &&
          !absorbsInnerPredicate(codepoints, start_pos, kanji_end, stem_end, inflection)) {
        const size_t onbin_end = end_pos - 1;
        const std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);
        auto onbin_candidate =
            makeVerbCandidate(onbin_surface, start_pos, onbin_end, candidate::verb_cost::kStandardBonus, best.base_form,
                              grammar::verbTypeToConjType(best.verb_type), true, CandidateOrigin::VerbKanji,
                              best.confidence, "mixed_godan_ka_onbin", core::ExtendedPOS::VerbOnbinkei);
        onbin_candidate.lemma_verified = true;
        candidates.push_back(std::move(onbin_candidate));
      }

      appendSelectedKanjiVerbCandidate(codepoints, start_pos, kanji_end, stem_end, end_pos, surface, hiragana_part,
                                       best, is_dict_verified, follows_reduplicated_noun, inflection, dict_manager,
                                       verb_opts, sokuonbin_stem_verified, sokuonbin_lemma, candidates);
    }
  }
}

}  // namespace suzume::analysis::kanji_verb_detail
