/**
 * @file adjective_candidates_kanji_variants.cpp
 * @brief Post-scan variants for kanji i-adjective candidates
 */

#include <algorithm>
#include <array>
#include <utility>

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/utf8.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

using verb_helpers::addEmphaticVariants;
using verb_helpers::isAdjectiveInDictionary;
using verb_helpers::isVerbInDictionary;

namespace {

// Whether the head of a nominal opens at @p pos. A kanji and a katakana run
// both start a lexical word, and neither spells a cell of any auxiliary, so
// either one proves that the position before it is an adnominal slot.
bool nominalHeadFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos < codepoints.size() &&
         (kana::isKanjiCodepoint(codepoints[pos]) || kana::isKatakanaCodepoint(codepoints[pos]));
}

// Whether the stem ending just before @p shi_pos is the 未然形 of a known godan
// verb, which is what the シク活用 suffix derives an adjective from (喜ば+しい ←
// 喜ぶ, 疑わ+しい ← 疑う, 好ま+しい ← 好む). The derivation is productive, so the
// verb is the evidence the adjective itself cannot supply.
bool derivesShikuFromGodanMizenkei(const std::vector<char32_t>& codepoints, size_t start_pos, size_t shi_pos,
                                   const dictionary::DictionaryManager* dict_manager) {
  if (shi_pos < start_pos + 2) {
    return false;
  }
  const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(codepoints[shi_pos - 1]);
  if (base_suffix.empty()) {
    return false;
  }
  const std::string base = extractSubstring(codepoints, start_pos, shi_pos - 1) + std::string(base_suffix);
  return verb_helpers::isVerbInDictionary(dict_manager, base);
}

}  // namespace

void adj_detail::appendKanjiIAdjPostVariants(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             size_t kanji_end, size_t hiragana_end,
                                             const grammar::Inflection& inflection,
                                             const dictionary::DictionaryManager* dict_manager,
                                             std::vector<UnknownCandidate>& candidates, size_t candidate_start) {
  // Add emphatic variants (すごい → すごいっっ, etc.)
  addEmphaticVariants(candidates, codepoints, candidate_start);

  // Preserve inflection and auxiliary/particle boundaries. Rules remain
  // path-local because the kanji path uses stronger negative splitting and a
  // dictionary-backed ければ disambiguation.
  static constexpr std::array<adj_detail::TrimmedAdjVariantRule, 6> kTrimRules = {{
      {"くない", 2, candidate::kAdjKuSplitBonus, core::ExtendedPOS::AdjRenyokei, 0, "i_adjective_ku_nai"},
      {"くなかった", 4, candidate::kAdjKuSplitBonus, core::ExtendedPOS::AdjRenyokei, 0, "i_adjective_ku_nakatta"},
      {"くなかっ", 3, candidate::kAdjKuSplitBonus, core::ExtendedPOS::AdjRenyokei, 0, "i_adjective_ku_nakatt"},
      {"くて", 1, candidate::kAdjKuSplitBonus, core::ExtendedPOS::AdjRenyokei, 1, "i_adjective_ku_te"},
      {"かった", 1, candidate::kAdjKattSplitBonus, core::ExtendedPOS::AdjKatt, 2, "i_adjective_katt"},
      {"ければ", 1, candidate::kAdjKeSplitBonus, core::ExtendedPOS::AdjKeForm, 3, "i_adjective_kere", false, false,
       true},
  }};
  adj_detail::appendTrimmedAdjVariants(candidates, kTrimRules.data(), kTrimRules.size(), candidate_start, dict_manager);

  // The past た is always a separate auxiliary: an i-adjective past never stands
  // as one かった token (難しかっ|た, 良くなかっ|た). Every span ending in かった
  // produced its trimmed かっ variant above, so drop the merged span itself —
  // it only ever wins over the split when a preceding modifier's connection
  // bonus favors the terminal-form EPOS, which is exactly the wrong parse.
  // Restrict removal to this generator's own [candidate_start, size) sub-range:
  // earlier generators may have shared the buffer and their candidates must not
  // be dropped here.
  candidates.erase(std::remove_if(candidates.begin() + candidate_start, candidates.end(),
                                  [](const UnknownCandidate& cand) { return utf8::endsWith(cand.surface, "かった"); }),
                   candidates.end());

  // The nominalizer っこ carries its own ない-family predicate (負け|っこ|なかっ|た),
  // so an adjective span reaching across the suffix is a fabrication: no
  // adjective paradigm puts a sokuon inside its stem.
  candidates.erase(std::remove_if(candidates.begin() + candidate_start, candidates.end(),
                                  [&codepoints](const UnknownCandidate& cand) {
                                    if (cand.pos != core::PartOfSpeech::Adjective) {
                                      return false;
                                    }
                                    for (size_t pos = cand.start + 1; pos + 2 < cand.end; ++pos) {
                                      if (codepoints[pos] == U'っ' && codepoints[pos + 1] == U'こ' &&
                                          verb_helpers::naiNegativeFollowsAt(codepoints, pos + 2)) {
                                        return true;
                                      }
                                    }
                                    return false;
                                  }),
                   candidates.end());

  // 書か+なく+ない is a verb irrealis followed by the negative auxiliary, not an
  // adjective continuative. What separates it from a genuine stem is where the
  // な sits: on the kanji itself in 少+なく and 危+なく, but behind the irrealis
  // a-row kana that the negative selects in 書か+なく.
  candidates.erase(std::remove_if(candidates.begin() + candidate_start, candidates.end(),
                                  [&codepoints](const UnknownCandidate& cand) {
                                    return cand.pos == core::PartOfSpeech::Adjective && cand.end >= 3 &&
                                           utf8::endsWith(cand.surface, "なく") &&
                                           grammar::isARowCodepoint(codepoints[cand.end - 3]);
                                  }),
                   candidates.end());

  // The conjunctive くて is never an adjective terminal form. Its trimmed
  // continuative candidate is emitted above, so remove the whole-span
  // alternative that would otherwise hide the connective particle.
  candidates.erase(std::remove_if(candidates.begin() + candidate_start, candidates.end(),
                                  [](const UnknownCandidate& cand) {
                                    return cand.pos == core::PartOfSpeech::Adjective &&
                                           utf8::endsWith(cand.surface, "くて");
                                  }),
                   candidates.end());

  // Add mizenkei (かろ) candidates for the conjectural pattern: stem + かろ + う
  // (高かろう, 美しかろう). Shared with the pure-hiragana generator.
  appendIAdjKaroCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, candidates);
  appendIAdjOnbinRenyokeiCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                                    candidates);
  appendIAdjClassicalTerminalCandidates(codepoints, start_pos, kanji_end, hiragana_end, dict_manager, candidates);
  appendIAdjKaraZuCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, candidates);

  // Add classical attributive (文語連体形) き candidates: stem + き + 体言
  // I-adjective 連体形 in classical Japanese: 美しい → 美しき(花), 古い → 古き(良き時代)
  // Inflection analysis does not produce this form, and the surface Xき is
  // homographic with godan-ka verb 連用形 (書き ← 書く), so generate only when
  // the lexical signal is decisive: the reconstructed base (stem + い) is a
  // known dictionary adjective. The lemma normalizes to the modern base form.
  if (dict_manager != nullptr) {
    for (size_t ki_pos = kanji_end; ki_pos < hiragana_end; ++ki_pos) {
      if (codepoints[ki_pos] != U'き') {
        continue;
      }
      std::string ki_stem = extractSubstring(codepoints, start_pos, ki_pos);
      std::string ki_lemma = ki_stem + "い";
      // The シク活用 subclass derives productively off a godan 未然形 (喜ばしい ←
      // 喜ぶ, 疑わしい ← 疑う), so its members cannot be enumerated; the base verb
      // carries the evidence instead. Adnominal position settles the competing
      // reading: it spells the classical past き, whose 終止形 cannot modify the
      // nominal that follows, and whose 連体形 is spelled し.
      const bool productive_shiku_attributive =
          ki_pos > start_pos && codepoints[ki_pos - 1] == U'し' && nominalHeadFollowsAt(codepoints, ki_pos + 1) &&
          derivesShikuFromGodanMizenkei(codepoints, start_pos, ki_pos - 1, dict_manager);
      if (!productive_shiku_attributive && !isAdjectiveInDictionary(dict_manager, ki_lemma)) {
        continue;
      }
      // If stem + く is a real godan-ka verb, Xき is its 連用形 (行き, 焼き),
      // not the classical adjective form — leave it to the verb paths.
      if (isVerbInDictionary(dict_manager, ki_stem + "く")) {
        continue;
      }
      // If the surface itself is a dictionary entry (好き, 大好き), the
      // dictionary interpretation wins — do not shadow it.
      std::string ki_surface = extractSubstring(codepoints, start_pos, ki_pos + 1);
      if (verb_helpers::hasNonVerbDictionaryEntry(dict_manager, ki_surface) ||
          isVerbInDictionary(dict_manager, ki_surface)) {
        continue;
      }
      UnknownCandidate ki_cand;
      ki_cand.surface = ki_surface;
      ki_cand.start = start_pos;
      ki_cand.end = ki_pos + 1;
      ki_cand.pos = core::PartOfSpeech::Adjective;
      ki_cand.lemma = ki_lemma;
      // Dictionary-verified adjective: make the 連体形 win over fake verb
      // interpretations (godan-ka 美しく etc.), mirroring the ke-form handling.
      ki_cand.cost = candidate::verb_cost::kStrongBonus;
      ki_cand.has_suffix = true;  // Conjugated form (連体形)
      // Attributive form connects like the basic form (ADJ + 体言)
      ki_cand.extended_pos = core::ExtendedPOS::AdjBasic;
#ifdef SUZUME_DEBUG_INFO
      ki_cand.origin = CandidateOrigin::AdjectiveI;
      ki_cand.confidence = 0.8F;
      ki_cand.pattern = "i_adjective_classical_ki";
#endif
      candidates.push_back(std::move(ki_cand));
    }
  }
}

}  // namespace suzume::analysis
