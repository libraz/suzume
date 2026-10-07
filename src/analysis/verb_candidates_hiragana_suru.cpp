/**
 * @file verb_candidates_hiragana_suru.cpp
 * @brief Kanji-Sahen hiragana subsidiary-verb candidates
 */

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/join_compound_verb_internal.h"
#include "analysis/tokenizer_utils.h"
#include "analysis/verb_candidates_dictionary_probes.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis::hiragana_verb_detail {
namespace vh = verb_helpers;

void appendSuruSubsidiaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const dictionary::DictionaryManager* dict_manager,
                                    std::vector<UnknownCandidate>& candidates) {
  if (dict_manager == nullptr || start_pos == 0 || codepoints[start_pos] != U'し' ||
      !normalize::isKanjiCodepoint(codepoints[start_pos - 1])) {
    return;
  }

  // A Sahen noun plus する has a distinct continuative stem before a
  // subsidiary verb.  Keep the し with that subsidiary only when the stem to
  // its left is not independently a lexical godan-sa verb; this preserves
  // ordinary V連用形+V2 boundaries such as 話し+切れ.
  size_t sahen_start = start_pos;
  while (sahen_start > 0 && normalize::isKanjiCodepoint(codepoints[sahen_start - 1])) {
    --sahen_start;
  }
  if (start_pos - sahen_start < 2) {
    return;
  }
  const std::string sahen_stem = extractSubstring(codepoints, sahen_start, start_pos);
  if (vh::isVerbInDictionary(dict_manager, sahen_stem + "す")) {
    return;
  }
  // The contiguous kanji sequence can include the single-kanji V1 of an
  // established compound after a preceding noun (ご報告+申し上げる).  Check
  // that immediate V1 before treating the whole sequence as a Sahen stem.
  const std::string final_kanji = extractSubstring(codepoints, start_pos - 1, start_pos);
  if (vh::isVerbInDictionary(dict_manager, final_kanji + "す")) {
    return;
  }

  const auto matchesAt = [&](size_t pos, std::string_view form) {
    return !form.empty() && pos + normalize::utf8Length(form) <= codepoints.size() &&
           extractSubstring(codepoints, pos, pos + normalize::utf8Length(form)) == form;
  };
  for (const auto& subsidiary : compound_verb_detail::subsidiaryVerbs()) {
    if (!subsidiary.joins_suru) {
      continue;
    }
    const std::string_view reading = subsidiary.reading == nullptr ? std::string_view{} : subsidiary.reading;
    const auto conjugation =
        compound_verb_detail::compoundConjugationType(subsidiary.verb_type, subsidiary.base_ending);
    const auto addCandidate = [&](size_t end_pos, std::string_view lemma_base, core::ExtendedPOS epos,
                                  const char* origin) {
      const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
      const float restricted_auxiliary_bonus =
          subsidiary.joins_general ? bigram_cost::kNeutral : bigram_cost::kTripleVeryStrongBonus;
      candidates.push_back(makeVerbCandidate(
          surface, start_pos, end_pos,
          candidate::verb_cost::kStrongBonus + bigram_cost::kVeryStrongBonus + restricted_auxiliary_bonus,
          normalize::concat("し", lemma_base), conjugation, true, CandidateOrigin::VerbHiragana,
          candidate::kHighOriginConfidence, origin, epos));
    };

    const size_t v2_start = start_pos + 1;
    if (matchesAt(v2_start, subsidiary.surface)) {
      addCandidate(v2_start + normalize::utf8Length(subsidiary.surface), subsidiary.surface,
                   core::ExtendedPOS::VerbShuushikei, "suru_subsidiary_base");
    } else if (!reading.empty() && matchesAt(v2_start, reading)) {
      addCandidate(v2_start + normalize::utf8Length(reading), reading, core::ExtendedPOS::VerbShuushikei,
                   "suru_subsidiary_base");
    }

    const auto tryConjugatedForm = [&](const std::string& form, std::string_view lemma_base, core::ExtendedPOS epos,
                                       const char* origin) {
      if (matchesAt(v2_start, form)) {
        const size_t end_pos = v2_start + normalize::utf8Length(form);
        if (end_pos >= codepoints.size()) {
          return;
        }
        const char32_t follower = codepoints[end_pos];
        const bool is_allowed =
            (epos == core::ExtendedPOS::VerbRenyokei &&
             (follower == U'た' || follower == U'て' || follower == U'で' || follower == U'ま' || follower == U'な' ||
              follower == U'ず')) ||
            // The irrealis stem selects the whole voice
            // paradigm, not only the negatives: せる and
            // れる attach to it exactly as ない and ず do
            // (し直さ+せる, し直さ+れる).
            (epos == core::ExtendedPOS::VerbMizenkei &&
             (follower == U'な' || follower == U'ず' || follower == U'ら' || follower == U'せ' || follower == U'れ')) ||
            (epos == core::ExtendedPOS::VerbKateikei && follower == U'ば');
        if (is_allowed) {
          addCandidate(end_pos, lemma_base, epos, origin);
        }
      }
    };
    tryConjugatedForm(compound_verb_detail::generateKanjiRenyokei(subsidiary.surface, reading, subsidiary.verb_type),
                      subsidiary.surface, core::ExtendedPOS::VerbRenyokei, "suru_subsidiary_renyokei");
    tryConjugatedForm(compound_verb_detail::generateRenyokei(reading, "", subsidiary.verb_type), reading,
                      core::ExtendedPOS::VerbRenyokei, "suru_subsidiary_renyokei");
    tryConjugatedForm(compound_verb_detail::generateMizenkei(subsidiary.surface, "", subsidiary.verb_type),
                      subsidiary.surface, core::ExtendedPOS::VerbMizenkei, "suru_subsidiary_mizenkei");
    tryConjugatedForm(compound_verb_detail::generateMizenkei(reading, "", subsidiary.verb_type), reading,
                      core::ExtendedPOS::VerbMizenkei, "suru_subsidiary_mizenkei");
    tryConjugatedForm(compound_verb_detail::generateKateikei(subsidiary.surface, "", subsidiary.verb_type),
                      subsidiary.surface, core::ExtendedPOS::VerbKateikei, "suru_subsidiary_kateikei");
    tryConjugatedForm(compound_verb_detail::generateKateikei(reading, "", subsidiary.verb_type), reading,
                      core::ExtendedPOS::VerbKateikei, "suru_subsidiary_kateikei");

    if (subsidiary.verb_type == compound_verb_detail::V2VerbType::Godan) {
      if (compound_verb_detail::getTeFormType(subsidiary.base_ending) == compound_verb_detail::TeFormType::Renyokei) {
        continue;
      }
      const auto tryTeStem = [&](const std::pair<std::string, bool>& te_stem, std::string_view lemma_base) {
        const auto& [form, uses_de] = te_stem;
        if (matchesAt(v2_start, form)) {
          const size_t end_pos = v2_start + normalize::utf8Length(form);
          if (end_pos < codepoints.size() &&
              (uses_de ? (codepoints[end_pos] == U'で' || codepoints[end_pos] == U'だ')
                       : (codepoints[end_pos] == U'て' || codepoints[end_pos] == U'た'))) {
            addCandidate(end_pos, lemma_base, core::ExtendedPOS::VerbOnbinkei, "suru_subsidiary_te_form");
          }
        }
      };
      tryTeStem(compound_verb_detail::generateTeFormStem(subsidiary.surface, "", subsidiary.verb_type,
                                                         subsidiary.base_ending),
                subsidiary.surface);
      tryTeStem(compound_verb_detail::generateTeFormStem(reading, "", subsidiary.verb_type, subsidiary.base_ending),
                reading);
    }
  }
}
}  // namespace suzume::analysis::hiragana_verb_detail
