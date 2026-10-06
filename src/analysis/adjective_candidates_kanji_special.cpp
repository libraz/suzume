/**
 * @file adjective_candidates_kanji_special.cpp
 * @brief Specialized kanji i-adjective candidate patterns
 */

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "tokenizer_utils.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis::adj_detail {

bool appendKanjiIAdjSpecialCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                      size_t hiragana_end, const std::vector<normalize::CharType>& char_types,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates) {
  // The derivational suffix め attaches to an i-adjective stem and yields a
  // degree-modified na-adjective (大きめだ, 長めな). Verify the reconstructed
  // stem + い as an adjective so ordinary nouns such as 初め are not admitted.
  for (size_t stem_end = kanji_end; stem_end < hiragana_end; ++stem_end) {
    if (codepoints[stem_end] != U'め' || stem_end + 1 >= codepoints.size() ||
        (codepoints[stem_end + 1] != U'な' && codepoints[stem_end + 1] != U'だ')) {
      continue;
    }
    const std::string adjective_base = extractSubstring(codepoints, start_pos, stem_end) + "い";
    const float adjective_confidence =
        maxConfidenceFor(inflection.analyze(adjective_base), {grammar::VerbType::IAdjective});
    if (verb_helpers::isAdjectiveInDictionary(dict_manager, adjective_base) ||
        adjective_confidence >= candidate::kIAdjConfMin) {
      const size_t suffix_end = stem_end + 1;
      const std::string surface = extractSubstring(codepoints, start_pos, suffix_end);
      candidates.push_back(makeNaAdjCandidate(surface, start_pos, suffix_end, candidate::kNaAdjStemCost, true,
                                              CandidateOrigin::AdjectiveNa, adjective_confidence,
                                              "i_adjective_degree_me"));
      break;
    }
  }

  // A kanji verb stem followed by すぎ is a verb-plus-auxiliary construction,
  // not a single adjective.
  // Pattern: kanji + (き/ぎ/し/ち/に/び/み/り/い) + すぎ...
  if (utf8::contains(extractSubstring(codepoints, kanji_end, hiragana_end), "すぎ")) {
    return true;  // Skip the main scan and force the split path.
  }

  // Special handling for single-kanji + い patterns (高い, 辛い, 甘い, etc.)
  // These are common i-adjectives that may not be recognized by inflection analysis
  // due to penalty_i_adj_single_kanji reducing confidence below threshold.
  // Generate candidate directly without relying on inflection analysis.
  // Also handles in-context cases like 甘いもの where hiragana_end extends past い.
  // Skip if already registered as NOUN in dictionary (e.g. 勢い) to avoid POS conflict.
  // Skip if い is followed by て/た/だ/で/や (verb onbin context, not adjective)
  // e.g., 届いて(verb te-form), 泳いだ(verb ta-form), 泳いで(godan-ga te-form),
  //        使いやすい(verb renyokei)
  // Exception: で followed by す (part of です) is NOT verb context
  //   良いです = ADJ + AUX, not VERB onbin
  if (kanji_end == start_pos + 1 && codepoints[kanji_end] == U'い' &&
      !adj_detail::isVerbOnbinContextAfterI(codepoints, kanji_end + 1)) {
    const size_t adj_end = kanji_end + 1;
    const std::string surface = extractSubstring(codepoints, start_pos, adj_end);
    // A surface that is itself a dictionary verb conjugation (来い = 来る 命令形,
    // or a godan-wa renyokei like 買い) is not an adjective — 来 is a verb stem,
    // unlike a genuine single-kanji adjective stem (濃い, 良い).
    if (verb_helpers::isNounInDictionary(dict_manager, surface)) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SINGLE] \"" << surface << "\" is dict NOUN, skipping ADJ candidate\n");
    } else if (verb_helpers::hasDictionaryEntry(dict_manager, surface, core::PartOfSpeech::Verb)) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SINGLE] \"" << surface << "\" is dict VERB, skipping ADJ candidate\n");
    } else {
      // Use moderate cost to compete with verb candidates (尊う has cost ~0.5)
      // Lower cost wins, so 0.35 should beat verb candidates
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SINGLE] \"" << surface << "\" cost=" << candidate::kSingleKanjiICost << "\n");
      candidates.push_back(makeIAdjCandidate(surface, start_pos, adj_end, surface, candidate::kSingleKanjiICost,
                                             CandidateOrigin::AdjectiveI, candidate::kIAdjConfMin, "single_kanji_i"));
    }
  }

  // Special handling for single-kanji + く patterns (甘く, 辛く, 暗く, etc.)
  // Only generate ADJ renyokei candidate when followed by adjective-renyokei
  // continuations (て/ない/なっ/なる/も), which disambiguate from godan-ka verbs.
  // Without this context check, 歩く/叩く etc. would get false ADJ candidates.
  if (kanji_end == start_pos + 1 && codepoints[kanji_end] == U'く') {
    const size_t adj_end = kanji_end + 1;
    bool is_adj_context = false;
    if (adj_end < codepoints.size()) {
      const char32_t next = codepoints[adj_end];
      // なら immediately after a terminal godan-ka form is the conditional
      // particle (届く+なら), not evidence that the preceding く is an
      // adjective continuative.  Keep the change-of-state sequence only when
      // なら is itself followed by a negative form (高く+なら+ない/ぬ/ず/ん).
      const bool starts_nara = next == U'な' && adj_end + 1 < codepoints.size() && codepoints[adj_end + 1] == U'ら';
      const bool nara_is_negative_change = starts_nara && adj_end + 2 < codepoints.size() &&
                                           (codepoints[adj_end + 2] == U'な' || codepoints[adj_end + 2] == U'ぬ' ||
                                            codepoints[adj_end + 2] == U'ず' || codepoints[adj_end + 2] == U'ん');
      // A bare も is ambiguous with the first character of the formal noun
      // もの (動くもの). Treat it as adjective evidence only when it opens a
      // negative continuation such as 高くもない.
      const bool is_mo_negative = next == U'も' && adj_end + 1 < codepoints.size() && codepoints[adj_end + 1] == U'な';
      // な opens the negative or the change of state (高くない, 高くなる, 高くなかった,
      // 高くなさそう) only when a cell of ない/なる follows it; a final な or one
      // before a sentence-final particle is the prohibitive (驚くなよ).
      const bool opens_nai_or_naru =
          next == U'な' && adj_end + 1 < codepoints.size() &&
          (codepoints[adj_end + 1] == U'い' || codepoints[adj_end + 1] == U'く' || codepoints[adj_end + 1] == U'か' ||
           codepoints[adj_end + 1] == U'け' || codepoints[adj_end + 1] == U'さ' || codepoints[adj_end + 1] == U'っ' ||
           codepoints[adj_end + 1] == U'る' || codepoints[adj_end + 1] == U'れ');
      is_adj_context =
          next == U'て' || (opens_nai_or_naru && (!starts_nara || nara_is_negative_change)) || is_mo_negative;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, adj_end);
    const std::string lemma = extractSubstring(codepoints, start_pos, kanji_end) + "い";
    const bool counter_conditioned_adjective = start_pos > 0 && normalize::isCounterKanji(codepoints[start_pos - 1]) &&
                                               hasKanjiSuruPredicateAt(codepoints, char_types, adj_end);
    if (is_adj_context || counter_conditioned_adjective) {
      const float cost =
          counter_conditioned_adjective ? candidate::kCounterConditionedKuAdjectiveCost : candidate::kSingleKanjiKuCost;
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SINGLE_KU] \"" << surface << "\" cost=" << cost << "\n");
      candidates.push_back(makeIAdjCandidate(surface, start_pos, adj_end, lemma, cost, CandidateOrigin::AdjectiveI,
                                             candidate::kIAdjConfMin, "single_kanji_ku"));
    }
  }

  // The past cell かっ+た of the same one-kanji stem (違かった). A ka-row
  // verb owns the spelling when the dictionary attests it (分かった, 助かった).
  if (kanji_end == start_pos + 1 && kanji_end + 2 < codepoints.size() &&
      extractSubstring(codepoints, kanji_end, kanji_end + 3) == "かった") {
    const std::string kanji = extractSubstring(codepoints, start_pos, kanji_end);
    if (!verb_helpers::hasDictionaryEntry(dict_manager, kanji + "かる", core::PartOfSpeech::Verb) &&
        !verb_helpers::hasDictionaryEntry(dict_manager, kanji + "く", core::PartOfSpeech::Verb)) {
      const size_t adj_end = kanji_end + 2;
      candidates.push_back(makeIAdjCandidate(extractSubstring(codepoints, start_pos, adj_end), start_pos, adj_end,
                                             kanji + "い", candidate::kSingleKanjiKuCost, CandidateOrigin::AdjectiveI,
                                             candidate::kIAdjConfMin, "single_kanji_katt"));
    }
  }

  // A one-kanji stem followed by るい/るく is a productive i-adjective
  // shape (明るい, 明るく). The inflection engine can prefer a homographic
  // godan analysis here, so retain the adjective candidate independently.
  if (kanji_end == start_pos + 1 && kanji_end + 1 < codepoints.size() && codepoints[kanji_end] == U'る' &&
      (codepoints[kanji_end + 1] == U'い' || codepoints[kanji_end + 1] == U'く' ||
       (kanji_end + 3 < codepoints.size() && codepoints[kanji_end + 1] == U'か' && codepoints[kanji_end + 2] == U'っ' &&
        codepoints[kanji_end + 3] == U'た'))) {
    const bool is_past = codepoints[kanji_end + 1] == U'か';
    const size_t adj_end = kanji_end + (is_past ? 3 : 2);
    const std::string surface = extractSubstring(codepoints, start_pos, adj_end);
    const std::string lemma = extractSubstring(codepoints, start_pos, kanji_end) + "るい";
    const std::string verb_lemma = extractSubstring(codepoints, start_pos, kanji_end) + "る";
    if (!verb_helpers::isVerbInDictionary(dict_manager, verb_lemma) ||
        verb_helpers::isAdjectiveInDictionary(dict_manager, lemma)) {
      auto adjective = makeIAdjCandidate(surface, start_pos, adj_end, lemma,
                                         is_past ? candidate::kAdjKattSplitBonus : candidate::kSingleKanjiICost,
                                         CandidateOrigin::AdjectiveI, candidate::kIAdjConfMin, "single_kanji_rui");
      if (is_past) {
        adjective.extended_pos = core::ExtendedPOS::AdjKatt;
        adjective.has_suffix = true;
      }
      candidates.push_back(std::move(adjective));
    }
  }
  return false;
}

}  // namespace suzume::analysis::adj_detail
