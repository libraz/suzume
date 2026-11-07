/**
 * @file suffix_candidates_humble_nominal.cpp
 * @brief Nominal candidates inside the humble お/ご + stem + する/いたす frame
 */
#include <algorithm>
#include <array>

#include "adjective_candidates_internal.h"
#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "join_compound_verb_internal.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis {

void generateHumbleNominalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  if (start_pos == 0 || start_pos >= codepoints.size()) {
    return;
  }
  if (!grammar::isHonorificPrefix(extractSubstring(codepoints, start_pos - 1, start_pos))) {
    return;
  }
  // A one-mora stem carries no continuative evidence of its own and would turn
  // the opening kana of a lexical word into a nominal (おいしい).
  constexpr size_t kMinStemLength = 2;
  // Long enough for a compound continuative (問い合わせ) without scanning past
  // the frame.
  constexpr size_t kMaxStemLength = 5;
  const size_t max_end = lookaheadEnd(codepoints.size(), start_pos, kMaxStemLength);
  for (size_t end_pos = start_pos + kMinStemLength; end_pos <= max_end; ++end_pos) {
    if (end_pos >= codepoints.size()) {
      break;
    }
    // する closes the frame in its dictionary form or through its continuative
    // し, which carries the polite and past chains (お伝えします, おかけした).
    // Its humble form いたす closes it in every cell (おかけいたします).
    const char32_t suru_head = codepoints[end_pos];
    const bool closes_with_itasu =
        suru_head == U'い' && end_pos + 2 < codepoints.size() && codepoints[end_pos + 1] == U'た' &&
        utf8::equalsAny(extractSubstring(codepoints, end_pos + 2, end_pos + 3), {"さ", "し", "す", "せ", "そ"});
    const bool closes_frame =
        suru_head == U'し' || closes_with_itasu ||
        (suru_head == U'す' && end_pos + 1 < codepoints.size() && codepoints[end_pos + 1] == U'る');
    if (!closes_frame) {
      continue;
    }
    // A kanji-headed continuative already carries its own deverbal-noun and
    // verbal candidates inside this frame, so only the pure-hiragana stem is
    // left with nothing to oppose the fabricated verb.
    const std::string stem = extractSubstring(codepoints, start_pos, end_pos);
    if (!grammar::isPureHiragana(stem)) {
      continue;
    }
    // ご prefixes a Sino-Japanese verbal noun spelled in kana (ご+あんない+し+
    // ます), which carries no continuative shape to check.
    if (grammar::isSinoHonorificPrefix(extractSubstring(codepoints, start_pos - 1, start_pos))) {
      // A registered word opening on the ご mora owns it (ござい+ます, ごろごろ).
      const bool prefix_is_word_head =
          dict_manager != nullptr &&
          lookupResultsHaveLongerPartOfSpeech(
              lookupResultsInRange(*dict_manager, codepoints, start_pos - 1, end_pos + 1),
              ~(partOfSpeechMask(core::PartOfSpeech::Prefix) | partOfSpeechMask(core::PartOfSpeech::Suffix)), 1);
      // So does a godan-sa verb whose continuative is the frame's し (ごまかし+ます).
      bool spans_godan_sa_continuative = false;
      if (suru_head == U'し') {
        for (const auto& cand : analysesInRange(inflection, codepoints, start_pos - 1, end_pos + 1)) {
          spans_godan_sa_continuative =
              spans_godan_sa_continuative || (cand.verb_type == grammar::VerbType::GodanSa &&
                                              cand.confidence > candidate::kHumbleNominalStemMinConfidence);
        }
      }
      // A case particle before する is the Nにする/Nとする frame, not the
      // noun's last mora (ごはん+に+します).
      const auto* closing_particle =
          dict_manager != nullptr
              ? lookupEntryInRange(*dict_manager, codepoints, end_pos - 1, end_pos, core::PartOfSpeech::Particle)
              : nullptr;
      const bool closes_on_case_particle =
          closing_particle != nullptr && closing_particle->extended_pos == core::ExtendedPOS::ParticleCase;
      if (prefix_is_word_head || spans_godan_sa_continuative || closes_on_case_particle) {
        continue;
      }
      SUZUME_DEBUG_LOG_VERBOSE("[NOMINAL_CAND] \"" << stem << "\" kana verbal noun after ご\n");
      // The prefix and the する frame prove both boundaries of the head.
      auto cand = makeCandidate(stem, start_pos, end_pos, core::PartOfSpeech::Noun,
                                candidate::kHumbleNominalCandidateBonus, true, CandidateOrigin::SelectedNominalHead);
      SUZUME_DEBUG_CANDIDATE_PATTERN(cand, "humble_kana_verbal_noun");
      candidates.push_back(cand);
      continue;
    }
    // A continuative ends in an e-row or i-row mora. Without this the a-row
    // tail of an ordinary lexical word (おこがましい, ございました) reads as a
    // stem, because the inflection analyzer reconstructs a nominal ichidan
    // paradigm for any kana run at its floor confidence.
    const char32_t stem_end = codepoints[end_pos - 1];
    if (!kana::isIOrERowCodepoint(stem_end)) {
      continue;
    }
    const auto& stem_analysis = inflection.analyze(stem);
    const std::string ichidan_base = stem + "る";
    const grammar::VerbType godan_type = grammar::verbTypeFromIRowCodepoint(codepoints[end_pos - 1]);
    std::string godan_base;
    if (godan_type != grammar::VerbType::Unknown) {
      godan_base = extractSubstring(codepoints, start_pos, end_pos - 1) +
                   std::string(grammar::godanBaseSuffixFromIRow(codepoints[end_pos - 1]));
    }
    bool is_continuative = false;
    for (const auto& cand : stem_analysis) {
      // An e-row stem already has the ichidan continuative's shape, so its
      // ichidan reading needs no confidence floor (お+つたえ+し+ます).
      const bool ichidan_shaped = cand.verb_type == grammar::VerbType::Ichidan && kana::isERowCodepoint(stem_end);
      if (!ichidan_shaped && cand.confidence <= candidate::kHumbleNominalStemMinConfidence) {
        continue;
      }
      if ((cand.verb_type == grammar::VerbType::Ichidan && cand.base_form == ichidan_base) ||
          (cand.verb_type == godan_type && !godan_base.empty() && cand.base_form == godan_base)) {
        is_continuative = true;
        break;
      }
    }
    if (!is_continuative) {
      continue;
    }
    auto cand = makeCandidate(stem, start_pos, end_pos, core::PartOfSpeech::Noun,
                              candidate::kHumbleNominalCandidateBonus, true, CandidateOrigin::NominalizedNoun);
    SUZUME_DEBUG_CANDIDATE_PATTERN(cand, "humble_nominal");
    candidates.push_back(cand);
  }
}

}  // namespace suzume::analysis
