/**
 * @file adjective_candidates_katakana.cpp
 * @brief Katakana i-adjective candidate generation (エモい, ウザい)
 */

#include <algorithm>
#include <array>

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_absorption_guards.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"
#include "verb_candidates_emphatic.h"

namespace suzume::analysis {

using verb_helpers::addEmphaticVariants;
using verb_helpers::isAdjectiveInDictionary;
using verb_helpers::isEmphaticChar;
using verb_helpers::isVerbInDictionary;

using adj_detail::addReduplicatedShiiAdjective;
using adj_detail::appendHiraganaPrefixedKanjiIAdjCandidates;
using adj_detail::dropCoinedAdjectivesOverDictionaryAdjective;
using adj_detail::makeIAdjCandidate;
using adj_detail::makeIAdjCellCandidate;
using adj_detail::makeIAdjStemCandidate;
using adj_detail::makeNaAdjCandidate;

void generateKatakanaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                         const std::vector<normalize::CharType>& char_types,
                                         const grammar::Inflection& inflection,
                                         const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates) {
  const size_t candidate_start = candidates.size();

  // Only process katakana-starting positions
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Katakana) {
    return;
  }

  // Find katakana portion (1-6 characters for slang adjective stems)
  // e.g., エモ, キモ, ウザ, ダサ, etc.
  size_t kata_end = findCharRegionEnd(char_types, start_pos, 6, normalize::CharType::Katakana);

  // Need at least 1 katakana character
  if (kata_end == start_pos) {
    return;
  }

  // Fully spelled-out reduplicated 〜しい adjective (バカバカしい): the doubled katakana
  // stem is otherwise pre-empted by the aa_doubled ADV candidate, and its しい ending
  // starts with し, which the ending gate below rejects. Emit the whole-word adjective
  // here; the trim loops after the main loop derive the …しく split.
  addReduplicatedShiiAdjective(candidates, codepoints, start_pos, char_types, inflection, CandidateOrigin::AdjectiveI);

  // The main loop only runs for a katakana stem followed by a valid i-adjective ending
  // start. When it does not apply (e.g. the reduplicated しい handled above), fall through
  // to the shared emphatic/trim/sort tail so the emitted candidate still gets its splits.
  if (kata_end < char_types.size() && char_types[kata_end] == normalize::CharType::Hiragana) {
    // I-adjective endings: い, か(った), く(ない/て), け(れば), さ(そう), そ(う) etc.
    char32_t first_hira = codepoints[kata_end];
    size_t kata_len = kata_end - start_pos;
    bool valid_ending_start = (first_hira == U'い' || first_hira == U'か' || first_hira == U'く' ||
                               first_hira == U'け' || first_hira == U'さ' || first_hira == U'そ');
    // For さ (nominalization), restrict to short katakana stems (2 chars max)
    // Valid: エモさ, キモさ, ウザさ, ダサさ (2-char stems)
    // Invalid: レイプさ (3-char stem, レイプい doesn't exist)
    if (valid_ending_start && !(first_hira == U'さ' && kata_len > 2)) {
      // Find hiragana portion (up to 8 chars for conjugation endings)
      size_t hira_end = findCharRegionEnd(char_types, kata_end, 8, normalize::CharType::Hiragana);

      // Try different ending lengths, starting from longest
      for (size_t end_pos = hira_end; end_pos > kata_end; --end_pos) {
        const std::string surface = extractSubstring(codepoints, start_pos, end_pos);

        // Check all candidates for IAdjective
        const auto& all_candidates = inflection.analyze(surface);
        for (const auto& cand : all_candidates) {
          // Require confidence >= 0.5 for i-adjectives
          if (cand.confidence >= candidate::kIAdjConfMin && cand.verb_type == grammar::VerbType::IAdjective) {
            // Lower cost than pure katakana noun to prefer adjective reading
            // Cost: 0.2-0.35 based on confidence (lower = better)
            float cost = candidate::confidenceScaledCost(candidate::kKanjiAdjBaseCost, cand.confidence,
                                                         candidate::kKanjiAdjConfScale);
            // The nominalizer さ is a suffix on the bare stem (エモ+さ), as on the
            // kana and kanji sides, not a cell of the adjective.
            if (end_pos == kata_end + 1 && first_hira == U'さ') {
              candidates.push_back(makeIAdjStemCandidate(extractSubstring(codepoints, start_pos, kata_end), start_pos,
                                                         kata_end, cand.base_form, cost, CandidateOrigin::AdjectiveI,
                                                         cand.confidence, "i_adjective_kata_stem_sa"));
              break;
            }
            auto adj_cand = makeIAdjCandidate(surface, start_pos, end_pos, cand.base_form, cost,
                                              CandidateOrigin::AdjectiveI, cand.confidence, "i_adjective_kata");
            // Skip exceeds_dict_length penalty - this is a morphologically recognized pattern
            adj_cand.has_suffix = true;
            candidates.push_back(std::move(adj_cand));
            break;  // Only add one adjective candidate per surface
          }
        }
      }
    }
  }

  // Add emphatic variants (エグい → エグいっ, etc.)
  addEmphaticVariants(candidates, codepoints, candidate_start);

  // Preserve the same negative-family boundaries across scripts.
  static constexpr std::array<adj_detail::TrimmedAdjVariantRule, 7> kKatakanaTrimRules = {{
      {"かった", 1, candidate::kAdjKattSplitBonus, core::ExtendedPOS::AdjKatt, 0, "i_adjective_kata_katt"},
      {"くて", 1, candidate::kAdjKuSplitBonus, core::ExtendedPOS::AdjRenyokei, 1, "i_adjective_kata_ku_te"},
      {"くない", 2, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 2, "i_adjective_kata_ku_nai"},
      {"くなかった", 4, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 2,
       "i_adjective_kata_ku_nakatta"},
      {"くなかっ", 3, candidate::kAdjKuSplitBonusWeak, core::ExtendedPOS::AdjRenyokei, 2, "i_adjective_kata_ku_nakatt"},
      {"ければ", 1, candidate::kAdjKeSplitBonus, core::ExtendedPOS::AdjKeForm, 3, "i_adjective_kata_kere"},
      {"そう", 2, candidate::kAdjStemSplitBonus, core::ExtendedPOS::AdjStem, 4, "i_adjective_kata_stem_sou", false,
       true},
  }};
  adj_detail::appendTrimmedAdjVariants(candidates, kKatakanaTrimRules.data(), kKatakanaTrimRules.size(),
                                       candidate_start);

  dropCoinedAdjectivesOverDictionaryAdjective(dict_manager, codepoints, candidates, candidate_start);

  // Sort by cost
  verb_helpers::sortCandidatesByCost(candidates, candidate_start);
}

}  // namespace suzume::analysis
