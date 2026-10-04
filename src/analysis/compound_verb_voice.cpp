/**
 * @file compound_verb_voice.cpp
 * @brief Voice auxiliary tails that remain separate from lexical compound verbs
 */

#include "join_compound_verb_internal.h"

namespace suzume::analysis::compound_verb_detail {

bool addPassiveContinuativeTailCandidates(core::Lattice& lattice, const std::vector<char32_t>& codepoints,
                                          size_t kanji_end) {
  // A voice auxiliary followed by the continuative subsidiary keeps both of its
  // boundaries: れ is an auxiliary and 続ける an independent verb, so no search
  // unit spans them. What the position needs is evidence that the subsidiary
  // starts here, or 使われ続ける collapses into one fabricated verb. The gate
  // distinguishes this from lexical 〜れ続ける verbs such as 汚れ続ける: only a
  // preceding mizenkei (a-row) or the ichidan passive られ licenses it.
  for (size_t passive_pos = kanji_end; passive_pos + 3 < codepoints.size(); ++passive_pos) {
    if (codepoints[passive_pos] != U'れ' || codepoints[passive_pos + 1] != U'続' ||
        codepoints[passive_pos + 2] != U'け') {
      continue;
    }
    const char32_t tail_ending = codepoints[passive_pos + 3];
    if (tail_ending != U'る' && tail_ending != U'た' && tail_ending != U'て') {
      continue;
    }

    const bool follows_godan_mizenkei = passive_pos > kanji_end && kana::isARowCodepoint(codepoints[passive_pos - 1]);
    const bool follows_ichidan_passive = passive_pos == kanji_end + 1 && codepoints[kanji_end] == U'ら';
    // Causative-passive chains retain their voice boundaries, but the
    // passive-continuative tail itself remains one search unit: サ変/一段
    // + させ + られ続ける. The exact three-mora sequence is grammatical
    // evidence; it does not admit an arbitrary られ+続ける join.
    const bool follows_causative_passive = passive_pos >= kanji_end + 3 && codepoints[passive_pos - 3] == U'さ' &&
                                           codepoints[passive_pos - 2] == U'せ' && codepoints[passive_pos - 1] == U'ら';
    if (!follows_godan_mizenkei && !follows_ichidan_passive && !follows_causative_passive) {
      continue;
    }

    // The subsidiary alone is the candidate; the voice auxiliary in front of it
    // is already a dictionary edge. In past and te forms, expose the renyokei
    // and let the regular た/て auxiliary candidate supply the inflectional
    // boundary.
    const size_t subsidiary_start = passive_pos + 1;
    const size_t subsidiary_end = passive_pos + (tail_ending == U'る' ? 4 : 3);
    const std::string subsidiary_surface = extractSubstring(codepoints, subsidiary_start, subsidiary_end);
    lattice.addEdge(subsidiary_surface, static_cast<uint32_t>(subsidiary_start), static_cast<uint32_t>(subsidiary_end),
                    core::PartOfSpeech::Verb, candidate::kVerifiedTailCompoundVerbBonus, 0, "続ける",
                    dictionary::ConjugationType::Ichidan, core::CandidateOrigin::VerbCompound,
                    candidate::kNoOriginConfidence, "passive_continuative_subsidiary",
                    tail_ending == U'る' ? core::ExtendedPOS::VerbShuushikei : core::ExtendedPOS::VerbRenyokei,
                    "passive_continuative_subsidiary");
    return true;
  }

  return false;
}

}  // namespace suzume::analysis::compound_verb_detail

namespace suzume::analysis {

bool subsidiaryVerbContinuativeAt(const std::vector<char32_t>& codepoints, size_t pos) {
  for (const auto& subsidiary : compound_verb_detail::subsidiaryVerbs()) {
    if (subsidiary.reading == nullptr) {
      continue;
    }
    const std::string continuative =
        compound_verb_detail::generateRenyokei(subsidiary.reading, "", subsidiary.verb_type);
    const size_t length = normalize::utf8Length(continuative);
    if (!continuative.empty() && pos + length <= codepoints.size() &&
        extractSubstring(codepoints, pos, pos + length) == continuative) {
      return true;
    }
  }
  return false;
}

}  // namespace suzume::analysis
