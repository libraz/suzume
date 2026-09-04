/**
 * @file verb_candidates_kanji_derived.cpp
 * @brief Derived kanji verb inflection candidate patterns
 */

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
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

/**
 * @brief Whether a classical predicate tail follows the given position.
 *
 * The classical past/perfect/copular auxiliaries and the concessive conjunction
 * all attach to an auxiliary's own inflection, so a voice boundary in front of
 * them stays open (飲ま+れ+けり, 書か+れ+ども) exactly as it does before べき and
 * the causative and negative chains.
 */
bool classicalPredicateTailFollowsAt(const std::vector<char32_t>& codepoints, size_t pos,
                                     const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return false;
  }
  const size_t probe_end = std::min(codepoints.size(), pos + 3);
  for (size_t end = pos + 1; end <= probe_end; ++end) {
    const std::string tail = extractSubstring(codepoints, pos, end);
    const auto* auxiliary = dict_manager->lookupExact(tail, core::PartOfSpeech::Auxiliary);
    if (auxiliary != nullptr && core::isClassicalAuxiliaryType(auxiliary->extended_pos)) {
      return true;
    }
    const auto* particle = dict_manager->lookupExact(tail, core::PartOfSpeech::Particle);
    if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleConj) {
      return true;
    }
  }
  return false;
}

bool hasAttestedInternalGodanConditional(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                         size_t particle_pos, const grammar::InflectionCandidate& whole,
                                         const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Whether the binding particle that selects the 已然形 opens this clause.
 *
 * Of the binding particles only こそ takes the 已然形 as its 結び; ぞ and なむ
 * take the attributive, and the modern members select no cell at all. The
 * particle is therefore identified individually rather than by its class, and
 * the search stops at a clause boundary so a こそ from an earlier clause cannot
 * license a cell it does not govern.
 */
bool bindingParticleKosoPrecedes(const std::vector<char32_t>& codepoints, size_t start_pos) {
  constexpr size_t kKosoLength = 2;
  for (size_t pos = start_pos; pos > 0; --pos) {
    const char32_t codepoint = codepoints[pos - 1];
    if (normalize::classifyChar(codepoint) == normalize::CharType::Symbol) {
      return false;
    }
    if (pos >= kKosoLength && codepoints[pos - kKosoLength] == U'こ' && codepoint == U'そ') {
      return true;
    }
  }
  return false;
}

/**
 * @brief Emit the godan 已然形/仮定形 cell ending just before @p cell_end.
 *
 * The row a bare e-row mora belongs to is not recoverable from it: analyzing
 * 定まれ alone yields the ichidan 定まれる, and only 定まれば identifies the godan
 * 定まる. The conditional particle is therefore appended for the analysis even
 * where the text has none, which is what lets the clause-final cell reuse the
 * conditional's own row identification and guards rather than a second copy of
 * them.
 */
bool appendGodanIzenkeiCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                 size_t cell_end, const grammar::Inflection& inflection,
                                 const dictionary::DictionaryManager* dict_manager,
                                 std::vector<UnknownCandidate>& candidates) {
  // A negative conditional spells its own e-row mora (走らなければ), so the
  // cell the particle keys on belongs to the negative auxiliary rather than to
  // the host verb.
  constexpr size_t kNakereLength = 3;
  for (size_t negative_pos = kanji_end; negative_pos + kNakereLength <= cell_end; ++negative_pos) {
    if (negative_pos > start_pos && vh::naiConditionalFollowsAt(codepoints, negative_pos) &&
        (grammar::isARowCodepoint(codepoints[negative_pos - 1]) ||
         grammar::isERowCodepoint(codepoints[negative_pos - 1]))) {
      return false;
    }
  }
  const std::string cell_surface = extractSubstring(codepoints, start_pos, cell_end);
  const std::string full_surface = cell_surface + "ば";
  const auto& analyses = inflection.analyze(full_surface);
  if (analyses.empty()) {
    return false;
  }
  const auto& best = analyses.front();
  // An auxiliary carries its own izenkei before ば (担わ+ざれ+ば,
  // 過ぎ+たれ+ば), so the e-mora the conditional keys on belongs to the
  // auxiliary rather than to the host verb. Reading the whole span as one
  // conditional fabricates a lemma out of that auxiliary (過ぎたる, 担わざる).
  // Resolve the tail from the auxiliary inventory instead of naming one cell,
  // so the whole closed class is covered at once. A dictionary-attested
  // lexical verb such as ござる retains its genuine ござれ+ば paradigm.
  // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
  if (vh::endsWithAuxiliaryAfterOkurigana(dict_manager, codepoints, kanji_end, cell_end) &&
      !vh::isVerbInDictionary(dict_manager, best.base_form)) {
    return false;
  }
  // That guard only sees an auxiliary beginning after at least one okurigana
  // mora, which is the shape a genuine verb has. When the whole tail is the
  // auxiliary there is no okurigana left for a stem to own, so the kanji run is
  // a nominal predicate and the e-row mora is the copula's own cell
  // (重要+なれ, 大切+なれ, not the non-word 重要なる).
  if (dict_manager != nullptr && dict_manager->lookupExact(extractSubstring(codepoints, kanji_end, cell_end),
                                                           core::PartOfSpeech::Auxiliary) != nullptr) {
    return false;
  }
  const auto* godan_row = grammar::Conjugation::getGodanRow(best.verb_type);
  if (best.confidence < candidate::verb_cost::kConstructedVerbMinConfidence || godan_row == nullptr ||
      godan_row->e_row != codepoints[cell_end - 1] || best.base_form == full_surface) {
    return false;
  }
  if (hasAttestedInternalGodanConditional(codepoints, start_pos, kanji_end, cell_end, best, dict_manager)) {
    return false;
  }
  // 書い+とけ+ば: the ておく contraction leaves no て for the te-form guards.
  // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
  if (vh::embedsAuxiliaryOnOnbinStem(codepoints, kanji_end, cell_end, dict_manager)) {
    return false;
  }
  auto conditional =
      makeVerbCandidate(cell_surface, start_pos, cell_end, candidate::verb_cost::kStrongBonus, best.base_form,
                        grammar::verbTypeToConjType(best.verb_type), true, CandidateOrigin::VerbKanji, best.confidence,
                        "godan_kateikei", core::ExtendedPOS::VerbKateikei);
  conditional.lemma_verified = vh::isVerbInDictionary(dict_manager, best.base_form);
  candidates.push_back(std::move(conditional));
  return true;
}

bool hasAttestedInternalGodanConditional(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                         size_t particle_pos, const grammar::InflectionCandidate& whole,
                                         const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || vh::isVerbInDictionary(dict_manager, whole.base_form) || kanji_end <= start_pos + 1) {
    return false;
  }
  const std::string base_suffix = vh::baseFormSuffix(whole.verb_type);
  if (base_suffix.empty()) {
    return false;
  }
  // Only adjacent kanji can hide a particleless noun + predicate boundary
  // here.  Mixed-script compounds have already exposed their V1/V2 boundary
  // to the compound-verb generator and must not be reopened by this guard.
  for (size_t predicate_start = start_pos + 1; predicate_start < kanji_end; ++predicate_start) {
    std::string internal_base = extractSubstring(codepoints, predicate_start, particle_pos - 1);
    internal_base += base_suffix;
    if (vh::isVerbInDictionary(dict_manager, internal_base)) {
      return true;
    }
  }
  return false;
}

}  // namespace

// Try Ichidan verb kateikei (conditional) + volitional stem patterns.
// Kateikei: renyokei + れ + ば (食べれば → 食べれ + ば).
// Volitional: renyokei + よ + う (食べよう → 食べよ + う).
// MeCab splits these; generate the stem candidate for the split.
void appendIchidanKateikeiVolitionalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                               size_t kanji_end, size_t hiragana_end,
                                               const grammar::Inflection& inflection,
                                               const dictionary::DictionaryManager* dict_manager,
                                               std::vector<UnknownCandidate>& candidates) {
  // Literary ～んずる verbs (軽んずる, 重んずる) use a productive zuru
  // terminal that is not the ordinary noun+する paradigm.  Preserve the
  // complete finite form instead of reopening ん as a nominalizer particle.
  if (kanji_end + 3 <= codepoints.size() && codepoints[kanji_end] == U'ん' && codepoints[kanji_end + 1] == U'ず' &&
      codepoints[kanji_end + 2] == U'る') {
    const size_t zuru_end = kanji_end + 3;
    const std::string zuru_surface = extractSubstring(codepoints, start_pos, zuru_end);
    auto zuru_candidate =
        makeVerbCandidate(zuru_surface, start_pos, zuru_end, candidate::verb_cost::kStrongBonus, zuru_surface,
                          dictionary::ConjugationType::Suru, true, CandidateOrigin::VerbKanji,
                          candidate::kVerifiedConfidence, "literary_zuru_terminal", core::ExtendedPOS::VerbShuushikei);
    zuru_candidate.lemma_verified = true;
    candidates.push_back(std::move(zuru_candidate));
  }

  // Productive Godan volitional: o-row mizenkei + う (飾ろ+う, 書こ+う).
  // The full inflected surface identifies the Godan row even when the open-
  // class lemma is absent from L2.  Select only the best confident analysis
  // and require its row's actual o-row mora, so an i-adjective conjectural
  // form (高かろう) or an ordinary dictionary-form verb is not manufactured
  // into this path.
  if (kanji_end + 1 < codepoints.size() && grammar::isORowCodepoint(codepoints[kanji_end]) &&
      codepoints[kanji_end + 1] == U'う') {
    const size_t full_end = kanji_end + 2;
    const std::string full_surface = extractSubstring(codepoints, start_pos, full_end);
    const auto& analyses = inflection.analyze(full_surface);
    if (!analyses.empty()) {
      const std::string stem_surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
      const grammar::InflectionCandidate* selected = nullptr;
      bool selected_from_dictionary = false;

      // The dictionary compiler already expands every L2 Godan lemma to its
      // o-row mizenkei. Match that exact surface/EPOS/lemma relation against
      // every inflection analysis instead of trusting analyses.front(), which
      // can choose a shorter internal stem for multi-kanji verbs.
      if (dict_manager != nullptr) {
        for (const auto& result : dict_manager->lookup(stem_surface, 0)) {
          if (result.entry == nullptr || result.entry->surface.compare(stem_surface) != 0 ||
              result.entry->pos != core::PartOfSpeech::Verb ||
              result.entry->extended_pos != core::ExtendedPOS::VerbMizenkei) {
            continue;
          }
          for (const auto& analysis : analyses) {
            const auto* row = grammar::Conjugation::getGodanRow(analysis.verb_type);
            if (row != nullptr && row->o_row == codepoints[kanji_end] && analysis.base_form == result.entry->lemma &&
                (selected == nullptr || analysis.confidence > selected->confidence)) {
              selected = &analysis;
              selected_from_dictionary = true;
            }
          }
        }
      }

      if (selected == nullptr) {
        selected = &analyses.front();
      }
      const auto& best = *selected;
      const auto* godan_row = grammar::Conjugation::getGodanRow(best.verb_type);
      if ((selected_from_dictionary || best.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) &&
          godan_row != nullptr && godan_row->o_row == codepoints[kanji_end] && best.base_form != full_surface) {
        const size_t stem_end = kanji_end + 1;
        auto volitional =
            makeVerbCandidate(stem_surface, start_pos, stem_end, candidate::verb_cost::kStrongBonus, best.base_form,
                              grammar::verbTypeToConjType(best.verb_type), true, CandidateOrigin::VerbKanji,
                              best.confidence, "godan_volitional", core::ExtendedPOS::VerbMizenkei);
        volitional.lemma_verified = true;
        candidates.push_back(std::move(volitional));
      }
    }
  }

  // Productive Godan conditional: e-row 仮定形 + ば (伸ばせ+ば,
  // くぐれ+ば).  The full span supplies the verb row and lemma even when the
  // open-class base is absent from L2; keep the closed particle as its own
  // search unit.  Checking the selected row's e-mora excludes the identical
  // surface shape of i-adjective conditionals (高けれ+ば).
  for (size_t particle_pos = kanji_end + 1; particle_pos < hiragana_end; ++particle_pos) {
    if (codepoints[particle_pos] == U'ば' && appendGodanIzenkeiCandidate(codepoints, start_pos, kanji_end, particle_pos,
                                                                         inflection, dict_manager, candidates)) {
      break;
    }
  }

  // 係り結び: こそ takes the 已然形 as its 結び, and that cell closes the clause
  // with nothing after it (心こそ定まれ, 花こそ散りぬれ). It is spelled exactly
  // like the conditional's own cell, so the same row identification applies —
  // only the licensing context differs, because a bare e-row mora at the end of
  // a run is far more often a te-form or a continuative (出して, 食べて) than an
  // izenkei. こそ is what rules those out: it is the one binding particle whose
  // 結び is this cell, and the ぞ/なむ pair takes the attributive instead.
  //
  // The cell is emitted as VerbKateikei rather than under an ExtendedPOS of its
  // own: the godan 已然形 and 仮定形 are one cell of one paradigm, and every
  // connection rule that already reasons about it applies here unchanged.
  if (hiragana_end > kanji_end && grammar::isERowCodepoint(codepoints[hiragana_end - 1]) &&
      (hiragana_end == codepoints.size() ||
       normalize::classifyChar(codepoints[hiragana_end]) == normalize::CharType::Symbol) &&
      bindingParticleKosoPrecedes(codepoints, start_pos)) {
    appendGodanIzenkeiCandidate(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, candidates);
  }

  // Dictionary-backed single-kanji する verbs form 仮定形 with すれ+ば
  // (反する→反すれ+ば).  The generic analyzer can otherwise detach the
  // lexical kanji and select the standalone する paradigm.
  if (kanji_end == start_pos + 1 && kanji_end + 2 < codepoints.size() && codepoints[kanji_end] == U'す' &&
      codepoints[kanji_end + 1] == U'れ' && codepoints[kanji_end + 2] == U'ば') {
    const std::string suru_base = extractSubstring(codepoints, start_pos, kanji_end) + "する";
    if (vh::isVerbInDictionary(dict_manager, suru_base)) {
      const size_t kateikei_end = kanji_end + 2;
      auto suru_candidate =
          makeVerbCandidate(extractSubstring(codepoints, start_pos, kateikei_end), start_pos, kateikei_end,
                            candidate::verb_cost::kStrongBonus, suru_base, dictionary::ConjugationType::Suru, true,
                            CandidateOrigin::VerbKanji, candidate::kVerifiedConfidence,
                            "verified_single_kanji_suru_kateikei", core::ExtendedPOS::VerbKateikei);
      suru_candidate.lemma_verified = true;
      candidates.push_back(std::move(suru_candidate));
    }
  }

  // A Godan causative is itself an Ichidan-form predicate. Its conditional
  // surface is stem + a-row + せれ + ば (遊ばせれば), so preserve the full
  // conditional stem instead of splitting the causative auxiliary midway.
  if (kanji_end + 3 < codepoints.size() && grammar::isARowCodepoint(codepoints[kanji_end]) &&
      codepoints[kanji_end + 1] == U'せ' && codepoints[kanji_end + 2] == U'れ' && codepoints[kanji_end + 3] == U'ば') {
    size_t kateikei_end = kanji_end + 3;
    std::string surface = extractSubstring(codepoints, start_pos, kateikei_end);
    std::string causative_stem = extractSubstring(codepoints, start_pos, kanji_end + 2);
    std::string base_form = causative_stem + "る";
    float confidence =
        getIchidanConfidence(inflection.analyze(surface), candidate::verb_cost::kIchidanKateikeiMinConfidence);
    if (confidence >= candidate::verb_cost::kIchidanKateikeiMinConfidence &&
        vh::isVerbInDictionary(dict_manager, base_form)) {
      auto candidate =
          makeVerbCandidate(surface, start_pos, kateikei_end, candidate::verb_cost::kStrongBonus, base_form,
                            dictionary::ConjugationType::Ichidan, true, CandidateOrigin::VerbKanji, confidence,
                            "causative_kateikei", core::ExtendedPOS::VerbKateikei);
      candidate.lemma_verified = true;
      candidates.push_back(std::move(candidate));
    }
  }

  // A single-kanji ichidan stem can attach directly to よう (見よう, 着よう).
  // Unlike 食べよう, there is no e-row renyokei kana before よ, so emit the
  // mizenkei stem separately after inflection confirms the full form.
  if (kanji_end == start_pos + 1 && codepoints[start_pos] != U'来' && kanji_end + 1 < codepoints.size() &&
      codepoints[kanji_end] == U'よ' && codepoints[kanji_end + 1] == U'う') {
    std::string full_surface = extractSubstring(codepoints, start_pos, kanji_end + 2);
    float confidence =
        getIchidanConfidence(inflection.analyze(full_surface), candidate::verb_cost::kIchidanKateikeiMinConfidence);
    if (confidence >= candidate::verb_cost::kIchidanKateikeiMinConfidence) {
      std::string stem = extractSubstring(codepoints, start_pos, kanji_end);
      candidates.push_back(makeVerbCandidate(stem, start_pos, kanji_end, candidate::verb_cost::kStrongBonus,
                                             stem + "る", dictionary::ConjugationType::Ichidan, true,
                                             CandidateOrigin::VerbKanji, confidence, "single_kanji_volitional",
                                             core::ExtendedPOS::VerbMizenkei));
    }
  }

  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // Check if first hiragana is e-row or i-row (ichidan renyokei ending)
    if (grammar::isERowCodepoint(first_hira) || grammar::isIRowCodepoint(first_hira)) {
      size_t renyokei_end = kanji_end + 1;  // kanji + e/i-row
      // Check for れ + ば pattern after renyokei
      if (renyokei_end + 1 < codepoints.size() && codepoints[renyokei_end] == U'れ' &&
          codepoints[renyokei_end + 1] == U'ば') {
        // E.g., 食べ + れ + ば → 食べれ is kateikei
        size_t kateikei_end = renyokei_end + 1;  // renyokei + れ
        std::string surface = extractSubstring(codepoints, start_pos, kateikei_end);
        std::string renyokei_surface = extractSubstring(codepoints, start_pos, renyokei_end);
        std::string base_form = renyokei_surface + "る";  // 食べ + る = 食べる

        // Disambiguate i-adjective 仮定形 from ichidan verb 仮定形 for the ければ case.
        // "高ければ"(高い) and "受ければ"(受ける) are grammatically indistinguishable by
        // inflection rules alone (both yield a plausible ichidan base 高ける/受ける).
        // The distinguishing signal is lexical: when kanji-stem + い is a known
        // i-adjective, this is the adjective 仮定形 (高い→高けれ+ば), not a verb.
        // Suppress the fake ichidan verb candidate so the i-adjective ke-form wins.
        bool is_iadj_kateikei = false;
        if (renyokei_end > start_pos && codepoints[renyokei_end - 1] == U'け' && dict_manager != nullptr) {
          std::string adj_base = extractSubstring(codepoints, start_pos, renyokei_end - 1) + "い";
          if (vh::isAdjectiveInDictionary(dict_manager, adj_base)) {
            SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" ichidan_kateikei: " << adj_base
                                                      << " is i-adjective (prefer ADJ 仮定形)\n");
            is_iadj_kateikei = true;
          }
        }

        // Verify using inflection analysis on the kateikei form
        const auto& all_candidates = inflection.analyze(surface);
        float ichidan_confidence = getIchidanConfidence(all_candidates, 0.3F);

        if (!is_iadj_kateikei && ichidan_confidence >= 0.3F) {
          // Negative cost to beat the split path 語幹+れ(受身)+ば
          constexpr float kKateikeiCost = candidate::verb_cost::kStrongBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " ichidan_kateikei lemma=" << base_form
                                << " conf=" << ichidan_confidence << " cost=" << kKateikeiCost << "\n";
          }
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, kateikei_end, kKateikeiCost, base_form, dictionary::ConjugationType::Ichidan, true,
              CandidateOrigin::VerbKanji, ichidan_confidence, "ichidan_kateikei", core::ExtendedPOS::VerbKateikei));
        }
      }

      // Ichidan verbs form both the volitional stem (食べよ+う) and
      // the literary imperative (食べよ) from renyokei + よ.
      if (renyokei_end < codepoints.size() && codepoints[renyokei_end] == U'よ') {
        // A dictionary-backed Godan e-row stem before final よ is an
        // imperative (書け+よ), not the mizenkei of an invented Ichidan
        // potential (書ける).  The e-row-to-base mapping is a closed
        // conjugation table, and dictionary verification keeps this from
        // guessing at open-class spellings.
        const std::string_view godan_base_suffix = grammar::godanBaseSuffixFromERow(codepoints[renyokei_end - 1]);
        const bool has_verified_godan_imperative =
            !godan_base_suffix.empty() && dict_manager != nullptr &&
            vh::isVerbInDictionary(dict_manager, extractSubstring(codepoints, start_pos, renyokei_end - 1) +
                                                     std::string(godan_base_suffix));
        const bool is_volitional = renyokei_end + 1 < codepoints.size() && codepoints[renyokei_end + 1] == U'う';
        const size_t you_end = renyokei_end + 2;
        bool has_formal_method_continuation = false;
        if (is_volitional && dict_manager != nullptr && you_end <= codepoints.size()) {
          const auto* formal_you =
              lookupEntryInRange(*dict_manager, codepoints, renyokei_end, you_end, core::PartOfSpeech::Noun);
          const bool no_way_continuation = formal_you != nullptr &&
                                           formal_you->extended_pos == core::ExtendedPOS::NounFormal &&
                                           you_end < codepoints.size() && codepoints[you_end] == U'が' &&
                                           vh::naiNegativeFollowsAt(codepoints, you_end + 1);
          bool nominal_case_continuation = false;
          if (formal_you != nullptr && formal_you->extended_pos == core::ExtendedPOS::NounFormal &&
              you_end < codepoints.size() && codepoints[you_end] == U'に') {
            const size_t probe_end = std::min(codepoints.size(), you_end + static_cast<size_t>(5));
            const std::string probe = extractSubstring(codepoints, you_end, probe_end);
            for (const auto& following : dict_manager->lookup(probe, 0)) {
              if (following.entry != nullptr && following.length > 1 &&
                  following.entry->extended_pos == core::ExtendedPOS::ParticleCase) {
                nominal_case_continuation = true;
                break;
              }
            }
          }
          has_formal_method_continuation = no_way_continuation || nominal_case_continuation;
        }
        // Skip suru-verb pattern: 漢字 + し + よう
        // Suru-verbs (勉強しよう, 説明しよう) should be split as: 漢字|しよ|う
        // Check if renyokei ends with し preceded by kanji
        bool is_suru_pattern = false;
        if (renyokei_end > start_pos && codepoints[renyokei_end - 1] == U'し' && renyokei_end - 1 > start_pos) {
          // Check if there's at least one kanji before し
          bool has_kanji_before = false;
          for (size_t i = start_pos; i < renyokei_end - 1; ++i) {
            if (normalize::isKanjiCodepoint(codepoints[i])) {
              has_kanji_before = true;
              break;
            }
          }
          is_suru_pattern = has_kanji_before;
        }

        // The Godan reading wins for a bare e-row + よ imperative (書けよ),
        // but a following う closes the distinct Ichidan volitional pattern
        // (書けよ+う).  Do not let the imperative guard hide that productive
        // volitional candidate.
        if ((!has_verified_godan_imperative || is_volitional) && !is_suru_pattern && !has_formal_method_continuation) {
          // E.g., 食べ + よ + う → 食べよ is volitional stem;
          //       食べ + よ → 食べよ is the literary imperative.
          size_t volitional_end = renyokei_end + 1;  // renyokei + よ
          std::string surface = extractSubstring(codepoints, start_pos, volitional_end);
          std::string renyokei_surface = extractSubstring(codepoints, start_pos, renyokei_end);
          std::string base_form = renyokei_surface + "る";  // 食べ + る = 食べる

          // Check if renyokei looks like an adjective (kanji+い pattern)
          // E.g., 良い, 高い, 赤い - these are adjectives, not ichidan verb stems
          // Require higher confidence to avoid false volitional candidates
          // like 良いよ(う) being parsed as volitional of non-existent 良いる
          bool could_be_adjective = false;
          if (renyokei_end > start_pos + 1 && codepoints[renyokei_end - 1] == U'い') {
            // Check if chars before い are all kanji
            bool all_kanji_before_i = true;
            for (size_t k = start_pos; k < renyokei_end - 1; ++k) {
              if (!normalize::isKanjiCodepoint(codepoints[k])) {
                all_kanji_before_i = false;
                break;
              }
            }
            could_be_adjective = all_kanji_before_i;
          }

          // Verify using inflection analysis
          const auto& all_candidates = inflection.analyze(renyokei_surface + "よう");
          float min_confidence = could_be_adjective ? 0.5F : 0.3F;
          float ichidan_confidence = getIchidanConfidence(all_candidates, min_confidence);

          if (ichidan_confidence >= min_confidence) {
            // Negative cost to beat the renyokei + final-particle path.
            constexpr float kVolitionalCost = candidate::verb_cost::kStrongBonus;
            SUZUME_DEBUG_VERBOSE_BLOCK {
              SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                                  << (is_volitional ? " ichidan_volitional lemma=" : " ichidan_imperative lemma=")
                                  << base_form << " conf=" << ichidan_confidence << " cost=" << kVolitionalCost << "\n";
            }
            candidates.push_back(
                makeVerbCandidate(surface, start_pos, volitional_end, kVolitionalCost, base_form,
                                  dictionary::ConjugationType::Ichidan, true, CandidateOrigin::VerbKanji,
                                  ichidan_confidence, is_volitional ? "ichidan_volitional" : "ichidan_imperative",
                                  is_volitional ? core::ExtendedPOS::VerbMizenkei : core::ExtendedPOS::VerbMeireikei));
          }
        }
      }
    }
  }
}

// Try Causative verb renyokei pattern: kanji + ら + せ
// Causative verbs from Godan verbs follow this pattern:
//   知る → 知らせる (causative, Ichidan verb)
//   乗る → 乗らせる (causative, Ichidan verb)
//   終わる → 終わらせる (causative, Ichidan verb)
// The renyokei of these causative verbs ends with せ (e-row):
//   知らせ (renyokei of 知らせる), connects to ます, られる, て, た, etc.
// Pattern: kanji + ら + せ (followed by られ for causative-passive)
void appendCausativeRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       const VerbCandidateOptions& verb_opts,
                                       std::vector<UnknownCandidate>& candidates) {
  if (kanji_end + 2 <= hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    char32_t second_hira = codepoints[kanji_end + 1];
    // ら + せ pattern (causative renyokei)
    if (first_hira == U'ら' && second_hira == U'せ') {
      std::string original_base = extractSubstring(codepoints, start_pos, kanji_end) + "る";
      // When the underlying Godan verb is attested, preserve the productive
      // mizenkei + causative-auxiliary boundary. The fallback below exists for
      // an otherwise unavailable predicate analysis, not to replace it.
      if (vh::isVerbInDictionary(dict_manager, original_base)) {
        return;
      }
      // Generate causative renyokei when followed by valid ichidan verb endings
      // or causative-passive (られ). This covers:
      //   眠らせた (past), 眠らせて (te-form), 眠らせない (negative),
      //   眠らせます (polite), 眠らせられ (passive)
      bool followed_by_valid = false;
      if (kanji_end + 2 < codepoints.size()) {
        char32_t next_cp = codepoints[kanji_end + 2];
        followed_by_valid = (next_cp == U'ら' || next_cp == U'た' || next_cp == U'て' || next_cp == U'な' ||
                             next_cp == U'ま' || next_cp == U'ず' || next_cp == U'ば');
      }
      // Also allow at end of input (bare renyokei: 眠らせ)
      if (kanji_end + 2 >= codepoints.size()) {
        followed_by_valid = true;
      }
      if (followed_by_valid) {
        size_t renyokei_end = kanji_end + 2;  // kanji + ら + せ
        std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);

        // The causative base form is surface + る (e.g., 知らせ → 知らせる)
        std::string causative_base = surface + "る";

        // Verify this is a valid ichidan verb
        const auto& all_candidates = inflection.analyze(causative_base);
        float ichidan_confidence =
            getIchidanConfidence(all_candidates, candidate::verb_cost::kIchidanDefaultMinConfidence);

        if (ichidan_confidence >= 0.4F) {
          float base_cost = candidate::confidenceScaledCost(verb_opts.bonus_ichidan, ichidan_confidence,
                                                            verb_opts.confidence_cost_scale_small);
          SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] " << surface << " causative_renyokei lemma=" << causative_base
                                                  << " conf=" << ichidan_confidence << " cost=" << base_cost << "\n");
          candidates.push_back(makeVerbCandidate(surface, start_pos, renyokei_end, base_cost, causative_base,
                                                 grammar::verbTypeToConjType(grammar::VerbType::Ichidan), true,
                                                 CandidateOrigin::VerbKanji, ichidan_confidence, "causative_renyokei"));
        }
      }
    }
  }
}

// Try Godan passive renyokei pattern: kanji + a-row + れ
// Godan passive verbs (受身形) follow this pattern:
//   言う → 言われる (passive, Ichidan verb)
//   書く → 書かれる (passive, Ichidan verb)
//   読む → 読まれる (passive, Ichidan verb)
// The renyokei of these passive verbs ends with れ (e-row):
//   言われ (renyokei of 言われる), connects to ます, ない, て, た, etc.
// Pattern: kanji + a-row hiragana + れ
void appendGodanPassiveRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                          size_t hiragana_end, const grammar::Inflection& inflection,
                                          const dictionary::DictionaryManager* dict_manager,
                                          const VerbCandidateOptions& verb_opts,
                                          std::vector<UnknownCandidate>& candidates) {
  if (kanji_end + 1 < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    char32_t second_hira = codepoints[kanji_end + 1];
    // A-row + れ pattern (godan passive renyokei)
    if (grammar::isARowCodepoint(first_hira) && second_hira == U'れ') {
      // Skip suru-verb passive pattern: kanji + さ + れ
      // e.g., 処理される should be 処理(noun) + される(aux), not godan passive
      // Also skip single kanji + さ + れ as these are typically not real verbs
      // e.g., 強される is not a verb (強い is adjective, 強 is noun)
      std::string kanji_check = extractSubstring(codepoints, start_pos, kanji_end);
      bool is_suru_passive_pattern = (first_hira == U'さ' && grammar::isAllKanji(kanji_check));
      if (is_suru_passive_pattern) {
        // Skip - this should be handled as noun + される auxiliary
        // Continue to next pattern
      } else {
        size_t renyokei_end = kanji_end + 2;  // kanji + a-row + れ
        std::string surface = extractSubstring(codepoints, start_pos, renyokei_end);

        // Check if this is a valid passive verb stem
        // The passive base form is surface + る (e.g., 言われ → 言われる)
        std::string passive_base = surface + "る";

        // Skip if passive_base is already a known ichidan verb in dictionary.
        // E.g., 生まれる is a standalone ichidan verb, not passive of 生む.
        // The dictionary entry provides the correct candidate with proper lemma.
        if (vh::isVerbInDictionary(dict_manager, passive_base)) {
          // Fall through to end of block - dict entry handles this
        } else {
          // Compute the original base verb lemma by converting A-row to U-row
          // e.g., 言われる: 言 + わ + れる → 言 + う = 言う
          std::string kanji_part = extractSubstring(codepoints, start_pos, kanji_end);
          std::string_view u_row_suffix = grammar::godanBaseSuffixFromARow(first_hira);
          std::string base_lemma = normalize::concat(kanji_part, u_row_suffix);

          // Use analyze() to get all interpretations, not just the best one
          // The best overall interpretation might be Godan (言う + れる), but
          // there should also be an Ichidan interpretation (言われる as verb)
          const auto& all_candidates = inflection.analyze(passive_base);
          float ichidan_confidence =
              getIchidanConfidence(all_candidates, candidate::verb_cost::kIchidanDefaultMinConfidence);

          // Passive verbs are Ichidan conjugation (言われる conjugates like 食べる)
          if (ichidan_confidence >= 0.4F) {
            // Check if followed by べき (classical obligation)
            // For 書かれべき pattern, we want 書か + れべき, not 書かれ + べき
            bool is_beki_pattern = false;
            if (renyokei_end < codepoints.size()) {
              char32_t next_char = codepoints[renyokei_end];
              if (next_char == U'べ') {
                is_beki_pattern = true;
              }
            }

            // Calculate base cost for passive candidates
            // Add a penalty so the grammatical split path (縛ら+れ) can compete.
            // Without this, the merged form (縛られ) has too low a cost (-0.16)
            // and always beats the split path (縛ら(0.1) + れ(aux))
            float base_cost = candidate::confidenceScaledCost(verb_opts.bonus_ichidan, ichidan_confidence,
                                                              verb_opts.confidence_cost_scale_small) +
                              bigram_cost::kMinor;

            // A passive stem before a causative remains split as mizenkei +
            // passive + causative (書か+れ+させる); it is not a lexical
            // renyokei followed directly by させる. Preserve the same rule as
            // the existing classical べき boundary.
            const bool is_passive_causative_chain = vh::causativeSaseFollowsAt(codepoints, renyokei_end);
            // A following ない-family form must also keep the productive
            // voice boundary (読ま+れ+なく, 書か+れ+ない). Lexical Ichidan
            // verbs such as 生まれる use the dictionary/Ichidan path above
            // and do not reach this Godan-passive fallback.
            const bool is_passive_negative_chain = vh::naiNegativeFollowsAt(codepoints, renyokei_end);
            // So must the polite auxiliary, for the same reason: it selects a
            // continuative, and the continuative it selects is the passive
            // auxiliary's own (読ま+れ+まし+た). Leaving it out split the same
            // chain two ways depending on which cell of ます followed.
            const bool is_passive_polite_chain = vh::masuAuxFollowsAt(codepoints, renyokei_end);
            const bool is_classical_predicate_chain =
                classicalPredicateTailFollowsAt(codepoints, renyokei_end, dict_manager);
            // A subsidiary verb behind the passive keeps the boundary for the
            // same reason (使わ+れ+続ける, 使わ+れ+始める): the passive is an
            // auxiliary, so it never heads a lexical compound. Every cell it
            // does host is kana, which is what separates the two cases.
            const bool is_passive_subsidiary_chain = vh::lexicalWordFollowsAt(codepoints, renyokei_end);
            if (!is_beki_pattern && !is_passive_causative_chain && !is_passive_negative_chain &&
                !is_passive_polite_chain && !is_classical_predicate_chain && !is_passive_subsidiary_chain) {
              candidates.push_back(makeVerbCandidate(
                  surface, start_pos, renyokei_end, base_cost, base_lemma, dictionary::ConjugationType::Ichidan, false,
                  CandidateOrigin::VerbKanji, ichidan_confidence, "godan_passive_renyokei"));
            }

            // NOTE: Passive verb conjugated forms (言われる, 言われた, etc.) are NOT generated
            // as single tokens. MeCab splits them as: 言わ + れ + た
            // The renyokei form (言われ) generated above connects to auxiliary た/て/ない/etc.
          }
        }  // end else (not dict ichidan verb)
      }  // end else (not suru passive pattern)
    }
  }
}

}  // namespace suzume::analysis::kanji_verb_detail
