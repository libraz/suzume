/**
 * @file verb_candidates_kanji_mizenkei.cpp
 * @brief Kanji verb mizenkei candidate patterns
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

void appendGodanMizenkeiPassiveCausativeCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                   size_t kanji_end, size_t hiragana_end,
                                                   const grammar::Inflection& inflection,
                                                   const dictionary::DictionaryManager* dict_manager,
                                                   std::vector<UnknownCandidate>& candidates) {
  if (kanji_end - start_pos == 1 && kanji_end < hiragana_end && grammar::isARowCodepoint(codepoints[kanji_end])) {
    char32_t a_row = codepoints[kanji_end];
    size_t after_a_pos = kanji_end + 1;
    if (after_a_pos < codepoints.size()) {
      char32_t after_a = codepoints[after_a_pos];
      // A-row + れ (passive) or A-row + せ (causative)
      if (after_a == U'れ' || after_a == U'せ') {
        grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(a_row);
        std::string_view base_suffix = grammar::godanBaseSuffixFromARow(a_row);
        if (verb_type != grammar::VerbType::Unknown && !base_suffix.empty()) {
          std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
          std::string base_form = normalize::concat(kanji_stem, base_suffix);
          std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);

          // A closed-class irregular form in L1 is authoritative over this
          // productive Godan fallback.  In particular, do not fabricate a
          // Godan lemma for an irregular verb's causative connection form.
          if (dict_manager != nullptr) {
            const dictionary::DictionaryEntry* exact = dict_manager->lookupExact(surface);
            if (exact != nullptr && exact->pos == core::PartOfSpeech::Verb &&
                exact->extended_pos == core::ExtendedPOS::VerbMizenkei) {
              return;
            }
          }

          // Verify via inflection analysis of base form
          const auto& results = inflection.analyze(base_form);
          bool is_valid = false;
          for (const auto& cand : results) {
            if (cand.verb_type == verb_type && cand.confidence >= 0.4F) {
              is_valid = true;
              break;
            }
          }

          if (is_valid) {
            // Skip if kanji+A-row+る is a known godan-ra verb in dictionary
            // (potential form conflict). E.g., 泊まれる = potential of
            // 泊まる (godan-ra), not passive of 泊む (godan-ma).
            // 囲まれる = passive of 囲む is OK because 囲まる is not
            // in the dictionary.
            //
            // Also skip if kanji+A-row+れる is a known ichidan verb in
            // dictionary. E.g., 生まれる is ichidan, not passive of 生む.
            // Without this check, 生ま(mizenkei)+れ(passive) would
            // incorrectly win over the dictionary ichidan entry.
            bool has_competing_verb = false;
            if (after_a == U'れ') {
              std::string ra_form = surface + "る";
              has_competing_verb = vh::isVerbInDictionary(dict_manager, ra_form);
              if (!has_competing_verb) {
                std::string ichidan_form = surface + "れる";
                has_competing_verb = vh::isVerbInDictionary(dict_manager, ichidan_form);
              }
            }

            if (!has_competing_verb) {
              constexpr float kCost = candidate::verb_cost::kWeakPenalty;
              SUZUME_DEBUG_LOG("[VERB_CAND] " << surface << " godan_mizenkei_passive lemma=" << base_form
                                              << " cost=" << kCost << "\n");
              candidates.push_back(makeVerbCandidate(
                  surface, start_pos, kanji_end + 1, kCost, base_form, grammar::verbTypeToConjType(verb_type), true,
                  CandidateOrigin::VerbKanji, 0.8F, "godan_mizenkei_passive", core::ExtendedPOS::VerbMizenkei));
            }
          }
        }
      }
    }
  }
}

// Contracted sa-row mizenkei: kanji + しゃ + れ/せ/し
// Colloquial contraction さ→しゃ in passive/causative/emphatic negation
// E.g., 殺しゃれる → 殺しゃ (contracted mizenkei of 殺す) + れる (passive)
//       話しゃれる → 話しゃ (contracted mizenkei of 話す) + れる (passive)
//       出しゃしない → 出しゃ (contracted) + し + ない (emphatic neg)
// Only single-kanji stems (same constraint as godan-sa mizenkei above)
void appendSaRowContractedMizenkeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             size_t kanji_end, size_t hiragana_end,
                                             const grammar::Inflection& inflection,
                                             std::vector<UnknownCandidate>& candidates) {
  if (kanji_end - start_pos == 1 && kanji_end + 1 < hiragana_end && codepoints[kanji_end] == U'し' &&
      codepoints[kanji_end + 1] == U'ゃ') {
    size_t after_sha = kanji_end + 2;
    if (after_sha < codepoints.size()) {
      char32_t after = codepoints[after_sha];
      // しゃ + れ (passive) or しゃ + せ (causative) or しゃ + し (emphatic)
      if (after == U'れ' || after == U'せ' || after == U'し') {
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = kanji_stem + "す";
        std::string surface = kanji_stem + "しゃ";

        const auto& sa_results = inflection.analyze(base_form);
        bool is_valid_godan_sa = false;
        for (const auto& cand : sa_results) {
          if (cand.verb_type == grammar::VerbType::GodanSa && cand.confidence >= 0.4F) {
            is_valid_godan_sa = true;
            break;
          }
        }

        if (is_valid_godan_sa) {
          constexpr float kCost = candidate::verb_cost::kWeakPenalty;
          SUZUME_DEBUG_LOG("[VERB_CAND] " << surface << " godan_sa_contracted_mizenkei lemma=" << base_form
                                          << " cost=" << kCost << "\n");
          candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end + 2, kCost, base_form,
                                                 grammar::verbTypeToConjType(grammar::VerbType::GodanSa), false,
                                                 CandidateOrigin::VerbKanji, 0.8F, "godan_sa_contracted_mizenkei",
                                                 core::ExtendedPOS::VerbMizenkei));
        }
      }
    }
  }
}

// Godan mizenkei pattern: kanji + mizenkei ending + ず/ざる/ざれ
// (classical negative)
// E.g., 抜かずに → 抜か (mizenkei of 抜く) + ず + に
//       行かずに → 行か (mizenkei of 行く) + ず + に
//       書かずに → 書か (mizenkei of 書く) + ず + に
//       欠かさず → 欠かさ (mizenkei of 欠かす) + ず
// The main loop skips short A-row hiragana as particles, so generate the
// complete mizenkei candidate explicitly when followed by a classical
// negative auxiliary.
void appendGodanMizenkeiZuCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                     size_t hiragana_end, const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  size_t negative_pos = kanji_end;
  while (negative_pos < hiragana_end) {
    const bool is_zu = codepoints[negative_pos] == U'ず';
    const bool is_zaru_or_zare = codepoints[negative_pos] == U'ざ' && negative_pos + 1 < hiragana_end &&
                                 (codepoints[negative_pos + 1] == U'る' || codepoints[negative_pos + 1] == U'れ');
    if (is_zu || is_zaru_or_zare) {
      break;
    }
    ++negative_pos;
  }
  if (negative_pos < hiragana_end && negative_pos > kanji_end) {
    const bool is_single_kanji_stem = kanji_end - start_pos == 1;
    char32_t mizenkei_ending = codepoints[negative_pos - 1];
    if (grammar::isARowCodepoint(mizenkei_ending)) {
      grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(mizenkei_ending);
      if (verb_type != grammar::VerbType::Unknown) {
        std::string_view base_suffix = grammar::godanBaseSuffixFromARow(mizenkei_ending);
        if (!base_suffix.empty()) {
          std::string surface = extractSubstring(codepoints, start_pos, negative_pos);
          std::string base_form = surface.substr(0, surface.size() - core::kJapaneseCharBytes);
          base_form += base_suffix;

          // A closed particle inside the proposed stem marks a morpheme
          // boundary (静けさ|のみ|なら|ず).  It cannot be evidence for an
          // unknown Godan mizenkei candidate.  Inspect the finite particle
          // lexicon rather than enumerating particle surfaces.
          bool contains_internal_particle = false;
          if (dict_manager != nullptr) {
            for (size_t particle_start = start_pos + 1; particle_start + 1 < negative_pos; ++particle_start) {
              for (size_t particle_end = particle_start + 2; particle_end < negative_pos; ++particle_end) {
                const std::string particle_surface = extractSubstring(codepoints, particle_start, particle_end);
                if (dict_manager->lookupExact(particle_surface, core::PartOfSpeech::Particle) != nullptr) {
                  contains_internal_particle = true;
                  break;
                }
              }
              if (contains_internal_particle) {
                break;
              }
            }
          }
          // Verify via dictionary or inflection analysis of conjugated form
          const bool dictionary_verified =
              !contains_internal_particle && vh::isVerbInDictionary(dict_manager, base_form);
          bool is_valid = dictionary_verified;
          if (!contains_internal_particle && !is_valid && is_single_kanji_stem) {
            // Analyze mizenkei+ない form (standard negative) for better confidence
            // Base form alone may not be recognized. Multi-kanji stems require
            // dictionary evidence so a preceding noun cannot be absorbed.
            std::string neg_form = surface + "ない";
            const auto& infl_results = inflection.analyze(neg_form);
            for (const auto& cand : infl_results) {
              if (cand.base_form == base_form && cand.verb_type == verb_type && cand.confidence >= 0.3F) {
                is_valid = true;
                break;
              }
            }
          }

          if (is_valid) {
            // A lexicalized verb+ず entry (思わず) wins unless the following に
            // explicitly creates the productive ずに auxiliary construction.
            const bool followed_by_zu = codepoints[negative_pos] == U'ず';
            bool dict_has_zu_form = false;
            if (followed_by_zu && dict_manager != nullptr) {
              std::string zu_form = surface + "ず";
              std::string zuni_form = surface + "ずに";
              dict_has_zu_form =
                  dict_manager->lookupExact(zu_form) != nullptr || dict_manager->lookupExact(zuni_form) != nullptr;
            }
            const bool followed_by_case_ni =
                followed_by_zu && negative_pos + 1 < codepoints.size() && codepoints[negative_pos + 1] == U'に';
            if (!dict_has_zu_form || followed_by_case_ni) {
              constexpr float kCost = candidate::verb_cost::kWeakPenalty;
              SUZUME_DEBUG_LOG("[VERB_CAND] " << surface << " godan_mizenkei_zu lemma=" << base_form
                                              << " cost=" << kCost << "\n");
              auto candidate = makeVerbCandidate(
                  surface, start_pos, negative_pos, kCost, base_form, grammar::verbTypeToConjType(verb_type), true,
                  CandidateOrigin::VerbKanji, 0.8F, "godan_mizenkei_zu", core::ExtendedPOS::VerbMizenkei);
              candidate.lemma_verified = dictionary_verified;
              candidates.push_back(std::move(candidate));
            }
          }
        }
      }
    }
  }
}

// Try Ichidan renyokei pattern: kanji + e-row/i-row hiragana
// 下一段 (shimo-ichidan): e-row ending (食べ, 見せ, 教え)
// 上一段 (kami-ichidan): i-row ending (感じ, 見, 居)
// These are standalone verb forms that connect to ます, ましょう, etc.
// The stem IS the entire surface (no conjugation suffix)

void appendKanjiMizenkeiStemCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates) {
  // The colloquial negative contracts the Godan-ra irrealis and ない as
  // stem+ん+ない (分かん+ない ← 分かる).  The existing mizenkei scan keys on
  // the visible a-row mora, which this contraction has removed, so rebuild
  // the ra-row terminal from the stem immediately before ん instead.  The
  // contraction is on the irrealis, not on the auxiliary, so every cell of
  // ない takes it (分かん+なかっ+た, 分かん+なけれ+ば) — pinning the terminal
  // cell here left the rest of the paradigm cut at the ん.
  for (size_t n_pos = kanji_end + 1; n_pos + 2 < hiragana_end; ++n_pos) {
    if (codepoints[n_pos] != U'ん' || !vh::naiNegativeFollowsAt(codepoints, n_pos + 1)) {
      continue;
    }
    const std::string stem = extractSubstring(codepoints, start_pos, n_pos);
    const std::string base_form = stem + "る";
    if (!vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                candidate::verb_cost::kConstructedVerbMinConfidence, true)) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, n_pos + 1);
    candidates.push_back(makeVerbCandidate(surface, start_pos, n_pos + 1, candidate::verb_cost::kStandardBonus,
                                           base_form, dictionary::ConjugationType::GodanRa, true,
                                           CandidateOrigin::VerbKanji, candidate::kVerifiedConfidence,
                                           "kanji_n_onbin_nai", core::ExtendedPOS::VerbMizenkei));
    break;
  }

  // A passive may follow a Godan stem with more than one okurigana mora
  // (明かさ+れる).  Locate the A-row mora immediately before an explicit れ,
  // then validate the complete observed inflection.  Requiring that れ fixes
  // the ambiguity with lexical forms such as 知らせる, where the A-row mora is
  // followed by せ rather than a passive auxiliary.
  const std::string kanji_prefix = extractSubstring(codepoints, start_pos, kanji_end);
  const bool is_multi_kanji_nominal = kanji_end - start_pos >= 2 && grammar::isAllKanji(kanji_prefix);
  if (!is_multi_kanji_nominal) {
    for (size_t mizenkei_end = kanji_end + 2; mizenkei_end < hiragana_end; ++mizenkei_end) {
      const char32_t mizenkei_ending = codepoints[mizenkei_end - 1];
      if (codepoints[mizenkei_end] != U'れ' || !grammar::isARowCodepoint(mizenkei_ending) ||
          !vh::isPassiveAuxContinuation(codepoints, mizenkei_end + 1, /*strict_masu=*/true)) {
        continue;
      }
      const grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(mizenkei_ending);
      const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(mizenkei_ending);
      if (verb_type == grammar::VerbType::Unknown || base_suffix.empty()) {
        continue;
      }

      // Do not absorb a dictionary-verified causative-passive chain into a
      // lexical Godan-ra/sa proposal.  聞か+せ+られ and 書か+さ+れ retain
      // their auxiliary boundaries; 明かさ+れ remains eligible because the
      // competing shorter base 明く is not attested.
      size_t underlying_a_row_pos = codepoints.size();
      if (mizenkei_ending == U'ら' && mizenkei_end >= kanji_end + 3 && codepoints[mizenkei_end - 2] == U'せ' &&
          grammar::isARowCodepoint(codepoints[mizenkei_end - 3])) {
        underlying_a_row_pos = mizenkei_end - 3;
      } else if (mizenkei_ending == U'さ' && mizenkei_end >= kanji_end + 2 &&
                 grammar::isARowCodepoint(codepoints[mizenkei_end - 2])) {
        underlying_a_row_pos = mizenkei_end - 2;
      }
      if (underlying_a_row_pos < codepoints.size()) {
        const std::string underlying_surface = extractSubstring(codepoints, start_pos, underlying_a_row_pos + 1);
        const dictionary::DictionaryEntry* underlying_exact =
            dict_manager == nullptr ? nullptr : dict_manager->lookupExact(underlying_surface);
        if (underlying_exact != nullptr && underlying_exact->pos == core::PartOfSpeech::Verb &&
            underlying_exact->extended_pos == core::ExtendedPOS::VerbMizenkei) {
          continue;
        }
        const std::string_view underlying_suffix = grammar::godanBaseSuffixFromARow(codepoints[underlying_a_row_pos]);
        const std::string underlying_base =
            normalize::concat(extractSubstring(codepoints, start_pos, underlying_a_row_pos), underlying_suffix);
        if (!underlying_suffix.empty() && vh::isVerbInDictionary(dict_manager, underlying_base)) {
          continue;
        }
      }

      const std::string surface = extractSubstring(codepoints, start_pos, mizenkei_end);
      const std::string stem = extractSubstring(codepoints, start_pos, mizenkei_end - 1);
      const std::string base_form = normalize::concat(stem, base_suffix);
      bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
      if (!is_valid_verb) {
        // Validate exactly one closed auxiliary inflection after れ.  Cutting
        // れなかった at れなか loses its base-form evidence, while consuming
        // beyond れて into a following aspect chain (れていない) crosses a
        // morpheme boundary.
        const size_t continuation_pos = mizenkei_end + 1;
        size_t observed_end = mizenkei_end + 2;  // れる / れた / れて
        const size_t negative_length = vh::naiNegativeFormLengthAt(codepoints, continuation_pos);
        if (negative_length != 0) {
          observed_end = continuation_pos + negative_length;
        } else if (codepoints[continuation_pos] == U'ま' && observed_end < hiragana_end) {
          ++observed_end;  // Preserve the existing れまし validation span.
        }
        observed_end = std::min(observed_end, hiragana_end);
        const std::string observed_form = extractSubstring(codepoints, start_pos, observed_end);
        for (const auto& inflection_candidate : inflection.analyze(observed_form)) {
          if (inflection_candidate.verb_type == verb_type && inflection_candidate.base_form == base_form &&
              inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
            is_valid_verb = true;
            break;
          }
        }
      }

      const std::string competing_ichidan = surface + "れる";
      if (is_valid_verb && !vh::isVerbInDictionary(dict_manager, competing_ichidan)) {
        candidates.push_back(makeVerbCandidate(surface, start_pos, mizenkei_end, candidate::verb_cost::kStrongBonus,
                                               base_form, grammar::verbTypeToConjType(verb_type), true,
                                               CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                               "godan_mizenkei_passive_multi", core::ExtendedPOS::VerbMizenkei));
      }
      break;
    }
  }

  // A godan potential verb inflects as Ichidan. In the negative adverbial
  // pattern 読めなく/書けなく, its e-row stem must therefore be available as
  // the mizenkei of 読める/書ける, rather than only as the conditional form of
  // 読む/書く. Validate the underlying godan verb so ordinary Ichidan stems
  // such as 食べなく do not acquire a fabricated potential reading.
  if (kanji_end - start_pos == 1 && kanji_end + 2 < hiragana_end && grammar::isERowCodepoint(codepoints[kanji_end]) &&
      codepoints[kanji_end + 1] == U'な' && codepoints[kanji_end + 2] == U'く') {
    const std::string_view base_suffix = grammar::godanBaseSuffixFromERow(codepoints[kanji_end]);
    if (!base_suffix.empty()) {
      const std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
      const std::string base_form = normalize::concat(kanji_stem, base_suffix);
      if (vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                 candidate::verb_cost::kConstructedVerbMinConfidence, true)) {
        const std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
        const std::string potential_lemma = surface + "る";
        candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end + 1, candidate::verb_cost::kWeakPenalty,
                                               potential_lemma, dictionary::ConjugationType::Ichidan, true,
                                               CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                               "godan_potential_negative", core::ExtendedPOS::VerbMizenkei));
      }
    }
  }

  // Generate Godan mizenkei stem candidates for auxiliary separation
  // E.g., 書か (from 書く), 読ま (from 読む), 話さ (from 話す)
  // These connect to passive (れる), causative (せる), negative (ない)
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // A-row hiragana: あ, か, さ, た, な, ま, ら, わ, が, ざ, だ, ば, ぱ
    if (grammar::isARowCodepoint(first_hira)) {
      size_t mizenkei_end = kanji_end + 1;
      // Check if followed by passive/causative auxiliary pattern
      if (mizenkei_end < hiragana_end) {
        char32_t next_char = codepoints[mizenkei_end];
        // Generate mizenkei candidates for:
        // 1. Classical べき patterns: 書かれべき, 読まれべき
        // 2. Classical negation ぬ: 揃わぬ, 知らぬ, 行かぬ
        // 3. Passive patterns: 書か+れる, 言わ+れ+た
        bool is_beki_pattern = false;
        bool is_nu_pattern = false;
        bool is_passive_pattern = false;
        if (next_char == U'れ') {
          if (mizenkei_end + 2 < codepoints.size() && codepoints[mizenkei_end + 1] == U'べ' &&
              codepoints[mizenkei_end + 2] == U'き') {
            // Check for れべき pattern
            is_beki_pattern = true;
          } else {
            // Check for passive patterns: れる, れた, れて, れない, れます
            // E.g., 言われる → 言わ (mizenkei) + れる (passive AUX)
            // Strict ま-branch: bare ま requires a following す/せ (れます/れません).
            is_passive_pattern = vh::isPassiveAuxContinuation(codepoints, mizenkei_end + 1, /*strict_masu=*/true);
          }
        }
        // Check for classical negation ぬ pattern
        // E.g., 揃わぬ → 揃わ (mizenkei) + ぬ (AUX)
        if (next_char == U'ぬ') {
          is_nu_pattern = true;
        }
        // The literary conjectural む selects the irrealis just as ぬ does
        // (成ら+む, 咲か+む). Its modern siblings う / よう take the o-row
        // irrealis instead, so at this a-row position an AuxVolitional entry
        // can only be the literary one — the dictionary category decides,
        // not the spelling. Without the boundary the run reads as one
        // fabricated Godan-ma verb whose lemma is its own surface (成らむ).
        // Restricted to a stem whose whole kanji run is one character, for the
        // same reason GodanSa is above: a longer run is the nominal host that
        // the complete auxiliaries らむ / けむ take (確認+らむ), and there the
        // a-row mora is their onset rather than okurigana. Requiring the run to
        // be complete — not merely one character measured from an interior
        // position — is what separates 成+らむ from 確認+らむ, whose second
        // kanji looks identical on its own.
        const bool stem_is_lone_kanji =
            kanji_end - start_pos == 1 && (start_pos == 0 || !normalize::isKanjiCodepoint(codepoints[start_pos - 1]));
        const bool is_classical_conjecture_pattern =
            stem_is_lone_kanji &&
            vh::auxiliaryFollowsAt(dict_manager, codepoints, mizenkei_end,
                                   [](const dictionary::DictionaryEntry& entry) {
                                     return entry.extended_pos == core::ExtendedPOS::AuxVolitional;
                                   });
        // Check for colloquial contracted negative ん pattern
        // E.g., 行かん → 行か (mizenkei) + ん (contracted negative AUX)
        //       言わん → 言わ (mizenkei) + ん
        // Skip single-kanji + さ + ん pattern (honorific さん suffix)
        // E.g., 姉さん should be 姉 + さん (noun + suffix), not 姉さ + ん (verb + AUX)
        bool is_n_pattern = false;
        if (next_char == U'ん') {
          // Skip if single kanji + さ (potential さん honorific)
          bool is_honorific_san = (kanji_end == start_pos + 1 && first_hira == U'さ');
          // The contracted negative closes the predicate, so the past
          // auxiliary cannot follow it: 〜んだ attaches to an attributive, not
          // to an irrealis. In that environment the ん belongs to the verb as
          // its ma/ba/na-row 音便 (黄ばん+だ, not 黄ば+ん+だ).
          const bool past_auxiliary_follows =
              mizenkei_end + 1 < codepoints.size() && codepoints[mizenkei_end + 1] == U'だ';
          if (!is_honorific_san && !past_auxiliary_follows) {
            is_n_pattern = true;
          }
        }
        // Check for standard negative ない pattern
        // E.g., 行かない → 行か (mizenkei) + ない (negative AUX)
        //       書かない → 書か (mizenkei) + ない
        bool is_nai_pattern = false;
        if (next_char == U'な' && mizenkei_end + 1 < codepoints.size() && codepoints[mizenkei_end + 1] == U'い') {
          is_nai_pattern = true;
        }
        // Check for the past-negative auxiliary stem なかっ.
        // E.g., 書かなかった → 書か (mizenkei) + なかっ (negative past AUX) + た
        //       行かなかった → 行か (mizenkei) + なかっ + た
        bool is_nakatt_pattern = false;
        if (next_char == U'な' && mizenkei_end + 3 < codepoints.size() && codepoints[mizenkei_end + 1] == U'か' &&
            codepoints[mizenkei_end + 2] == U'っ') {
          is_nakatt_pattern = true;
        }
        // Check for the negative adverbial なく (ない's 連用形).
        // E.g., 行かなくて → 行か (mizenkei) + なく (negative adj) + て
        //       食べなくなる counterpart is handled elsewhere; here we split the godan
        //       mizenkei so なく does not get absorbed into a spurious verb form.
        bool is_naku_pattern = false;
        if (next_char == U'な' && mizenkei_end + 1 < codepoints.size() && codepoints[mizenkei_end + 1] == U'く') {
          is_naku_pattern = true;
        }
        // Check for the causative auxiliary せ.
        // E.g., 聞かせられた → 聞か (mizenkei) + せ (causative AUX) + られ + た
        //       書かせる → 書か (mizenkei) + せる (causative AUX)
        bool is_causative_pattern = false;
        bool is_shortened_causative_passive = false;
        if (next_char == U'せ') {
          // A lexical Ichidan verb can share the surface of a productive
          // causative (知らせる, 合わせる).  Its dictionary entry is evidence
          // that the whole form is one search unit; otherwise the ordinary
          // Godan mizenkei + causative auxiliary boundary is productive.
          const std::string mizenkei_surface = extractSubstring(codepoints, start_pos, mizenkei_end);
          const bool has_lexical_causative = vh::isVerbInDictionary(dict_manager, mizenkei_surface + "せる");
          // せ followed by られ, る, た, て, etc.
          if (mizenkei_end + 1 < codepoints.size()) {
            char32_t after_se = codepoints[mizenkei_end + 1];
            // Causative-passive chains: せられる and the shortened される.
            // A bare せる/せた/せて remains the lexical causative verb
            // (知らせる, 眠らせた), rather than being split again.
            if (after_se == U'ら') {
              is_causative_pattern = true;
            }
            // Shortened causative-passive: 負わ+さ+れる,
            // 読ま+さ+れた. The さ is the causative auxiliary and the
            // following れ starts the passive auxiliary, not an inflection
            // of an independent lexical verb.
            else if (after_se == U'れ' &&
                     vh::isPassiveAuxContinuation(codepoints, mizenkei_end + 2, /*strict_masu=*/true)) {
              is_causative_pattern = true;
              is_shortened_causative_passive = true;
            } else if (after_se == U'れ' &&
                       vh::isPassiveAuxContinuation(codepoints, mizenkei_end + 2, /*strict_masu=*/true)) {
              is_causative_pattern = true;
            }
            // Bare causative inflection remains productive unless a lexical
            // verb with the same full dictionary form is attested.
            else if (!has_lexical_causative && (after_se == U'る' || after_se == U'た' || after_se == U'て' ||
                                                (after_se == U'な' && mizenkei_end + 2 < codepoints.size() &&
                                                 codepoints[mizenkei_end + 2] == U'い'))) {
              is_causative_pattern = true;
            }
          }
        }
        if (is_beki_pattern || is_nu_pattern || is_n_pattern || is_nai_pattern || is_nakatt_pattern ||
            is_naku_pattern || is_passive_pattern || is_causative_pattern || is_classical_conjecture_pattern) {
          // Derive VerbType from the A-row ending (e.g., か → GodanKa)
          grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(first_hira);
          if (verb_type != grammar::VerbType::Unknown) {
            // Skip GodanSa mizenkei for all-kanji stems (likely サ変名詞 + される)
            // E.g., 装飾さ should be 装飾 + される, not 装飾す mizenkei
            bool is_suru_verb_pattern = false;
            if (verb_type == grammar::VerbType::GodanSa) {
              std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
              // Count the kanji run in codepoints, not bytes: the run is carved
              // out of `codepoints`, so its length is already the character count.
              const size_t kanji_count = kanji_end - start_pos;
              if (grammar::isAllKanji(kanji_stem) && kanji_count >= 2) {
                // This is likely a Suru verb pattern (2+ kanji followed by される)
                // The connection rules will handle 装飾 + される instead
                is_suru_verb_pattern = true;
              }
              // Skip single-kanji GodanSa + causative pattern (likely ichidan verb + させ)
              // E.g., 見させられた = 見 + させ + られ + た (ichidan 見る + causative)
              //       Not: 見さ + せ + られ + た (godan 見す doesn't exist)
              // Real godan-sa verbs (話す, 出す, 消す) have multi-char stems (話さ, 出さ, 消さ)
              if (is_causative_pattern && kanji_count == 1) {
                is_suru_verb_pattern = true;  // Skip generation
              }
            }
            if (is_suru_verb_pattern) {
              // Skip mizenkei generation for Suru verb patterns
            } else {
              // Get base suffix (e.g., か → く for GodanKa)
              std::string_view base_suffix = grammar::godanBaseSuffixFromARow(first_hira);
              if (!base_suffix.empty()) {
                // Construct base form: stem + base_suffix (e.g., 書 + く = 書く)
                std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
                std::string base_form = normalize::concat(kanji_stem, base_suffix);

                // Verify the base form is a valid verb
                // First check dictionary, then fall back to inflection analysis
                // IMPORTANT: For passive pattern, require dictionary check only for
                // most verb rows. The inflection analyzer is too permissive and will
                // accept patterns like 泊む (from 泊まれる) which don't exist.
                // EXCEPTIONS that allow inflection fallback:
                // - WA-row (わ行): passive (奪われる) doesn't conflict with potential
                // - RA-row (ら行): Xらる is not a valid modern verb, so Xられる
                //   is always passive of Xる (e.g., 縛られる = passive of 縛る)
                // - SA-row (さ行): Xさ+れる is the productive passive of
                //   an open-class Godan-sa verb. The all-kanji sahen guard
                //   above retains the nominal + される analysis where needed.
                bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
                // The shortened causative-passive is surface-ambiguous with
                // a lexical Godan-sa passive (明かさ+れる). Require an
                // attested base for this shortened path so the productive
                // open-class SA-row analysis remains available without
                // fabricating an unrelated underlying verb.
                const bool is_base_dict_verb = is_valid_verb;
                // For passive pattern, allow inflection fallback for WA-, RA-, and SA-row.
                bool allow_inflection_fallback =
                    !is_passive_pattern || first_hira == U'わ' || first_hira == U'ら' || first_hira == U'さ';
                if (!is_valid_verb && allow_inflection_fallback) {
                  // For non-passive patterns (ない, ぬ, etc.), allow inflection fallback
                  // For WA-row passive, also allow with higher confidence threshold
                  float threshold = is_passive_pattern ? candidate::verb_cost::kConstructedVerbPassiveMinConfidence
                                                       : candidate::verb_cost::kConstructedVerbMinConfidence;
                  is_valid_verb = vh::isVerifiedVerbBase(dict_manager, inflection, base_form, threshold, true);
                }

                // A bare open-class Godan-sa base can be too short for the
                // generic analyzer to rank confidently, while its explicit
                // passive chain supplies the missing inflectional evidence.
                // Validate that complete observed form before rejecting the
                // productive mizenkei candidate; this remains type- and
                // lemma-checked rather than accepting an arbitrary kanji+さ.
                if (!is_valid_verb && is_passive_pattern && first_hira == U'さ') {
                  const std::string observed_form = extractSubstring(codepoints, start_pos, hiragana_end);
                  for (const auto& inflection_candidate : inflection.analyze(observed_form)) {
                    if (inflection_candidate.verb_type == verb_type && inflection_candidate.base_form == base_form &&
                        inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
                      is_valid_verb = true;
                      break;
                    }
                  }
                }

                // Skip irregular verb 来る for passive — its passive is 来+られる, not 来ら+れる
                if (is_valid_verb && is_passive_pattern && base_form == "来る") {
                  is_valid_verb = false;
                }

                if (is_valid_verb && is_shortened_causative_passive && !is_base_dict_verb) {
                  is_valid_verb = false;
                }

                // Skip godan mizenkei passive when the surface + れる is a known
                // ichidan verb in the dictionary. E.g., 囚われる is ichidan,
                // not passive of 囚う. The dictionary entry provides the correct
                // candidate with proper lemma.
                if (is_valid_verb && is_passive_pattern) {
                  std::string ichidan_form = extractSubstring(codepoints, start_pos, mizenkei_end) + "れる";
                  if (vh::isVerbInDictionary(dict_manager, ichidan_form)) {
                    is_valid_verb = false;
                  }
                }

                // The classical conjectural attaches to an irrealis, so the
                // a-row mora in front of it has to belong to a verb of its own.
                // A productive ma-row derived verb ends in exactly that shape
                // (黄ばむ, 汗ばむ) while its apparent stem is not a verb, so an
                // unattested base plus a complete terminal reading of the whole
                // span is the derived verb and not an irrealis. An attested base
                // keeps the classical reading (咲か+む).
                if (is_valid_verb && is_classical_conjecture_pattern && !is_base_dict_verb &&
                    mizenkei_end < codepoints.size()) {
                  const std::string whole = extractSubstring(codepoints, start_pos, mizenkei_end + 1);
                  for (const auto& analysis : inflection.analyze(whole)) {
                    if (grammar::isGodanVerbType(analysis.verb_type) && analysis.base_form == whole &&
                        analysis.morphemes.empty() &&
                        analysis.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
                      is_valid_verb = false;
                      break;
                    }
                  }
                }

                if (is_valid_verb) {
                  std::string surface = extractSubstring(codepoints, start_pos, mizenkei_end);
                  // Cost varies by pattern:
                  // - ぬ pattern: negative cost (-0.5F) to beat combined verb form
                  //   揃わぬ(VERB) gets ~-0.1 total, so split needs lower cost
                  // - ん pattern: negative cost (-0.5F) for contracted negative
                  //   行かん(VERB) should split to 行か + ん
                  // - ない pattern: negative cost (-0.5F) for standard negative
                  //   行かない(VERB) should split to 行か + ない
                  // - passive pattern: negative cost (-0.5F) for the mizenkei boundary
                  //   言われる(VERB) gets ~0.15, so split (言わ+れる) needs lower cost
                  // - べき pattern: moderate cost (0.2F) for classical obligation
                  float cost = 0.2F;  // default for beki
                  if (is_nu_pattern || is_n_pattern || is_nai_pattern) {
                    cost = -0.5F;
                  } else if (is_passive_pattern) {
                    cost = -0.5F;
                  }
                  const char* debug_pattern = is_nu_pattern                     ? "nu"
                                              : is_n_pattern                    ? "n"
                                              : is_nai_pattern                  ? "nai"
                                              : is_passive_pattern              ? "passive"
                                              : is_classical_conjecture_pattern ? "mu"
                                                                                : "beki";
                  SUZUME_DEBUG_VERBOSE_BLOCK {
                    SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " godan_mizenkei lemma=" << base_form
                                        << " cost=" << cost << " pattern=" << debug_pattern << "\n";
                  }
                  const char* info_pattern = is_nu_pattern                     ? "godan_mizenkei_nu"
                                             : is_n_pattern                    ? "godan_mizenkei_n"
                                             : is_nai_pattern                  ? "godan_mizenkei_nai"
                                             : is_nakatt_pattern               ? "godan_mizenkei_nakatt"
                                             : is_passive_pattern              ? "godan_mizenkei_passive"
                                             : is_classical_conjecture_pattern ? "godan_mizenkei_mu"
                                                                               : "godan_mizenkei";
                  // Use explicit VerbMizenkei EPOS for negative/passive patterns to enable bigram connection
                  core::ExtendedPOS epos =
                      (is_nu_pattern || is_n_pattern || is_nai_pattern || is_nakatt_pattern || is_passive_pattern ||
                       is_causative_pattern || is_classical_conjecture_pattern)
                          ? core::ExtendedPOS::VerbMizenkei
                          : core::ExtendedPOS::Unknown;
                  candidates.push_back(makeVerbCandidate(surface, start_pos, mizenkei_end, cost, base_form,
                                                         grammar::verbTypeToConjType(verb_type), true,
                                                         CandidateOrigin::VerbKanji, 0.9F, info_pattern, epos));
                }
              }
            }  // else (not Suru verb pattern)
          }
        }
      }
    }
  }

  // Generate mizenkei candidates for verbs with multiple okurigana + negative patterns
  // E.g., 分からない → 分から (mizenkei of 分かる) + ない
  //       分からなかった → 分から (mizenkei of 分かる) + なかっ + た
  //       始まらない → 始まら (mizenkei of 始まる) + ない
  // These are Godan verbs where the okurigana includes 2+ hiragana before the A-row ending
  if (kanji_end < hiragana_end && hiragana_end >= kanji_end + 3) {
    const bool has_multi_kanji_stem = kanji_end - start_pos >= 2;
    bool follows_case_particle = false;
    if (has_multi_kanji_stem && start_pos > 0 && dict_manager != nullptr) {
      const std::string preceding_surface = extractSubstring(codepoints, start_pos - 1, start_pos);
      const auto* preceding_entry = dict_manager->lookupExact(preceding_surface, core::PartOfSpeech::Particle);
      follows_case_particle =
          preceding_entry != nullptr && preceding_entry->extended_pos == core::ExtendedPOS::ParticleCase;
    }
    // Look for A-row hiragana + negative patterns (ない, なかっ, or ん)
    const size_t scan_start = has_multi_kanji_stem && follows_case_particle ? kanji_end : kanji_end + 1;
    for (size_t scan_pos = scan_start; scan_pos < hiragana_end - 1; ++scan_pos) {
      char32_t cur_char = codepoints[scan_pos];
      char32_t next_char = codepoints[scan_pos + 1];
      // Check if cur_char is A-row and followed by negative pattern
      bool is_nai_pattern = grammar::isARowCodepoint(cur_char) && next_char == U'な' &&
                            scan_pos + 2 < codepoints.size() && codepoints[scan_pos + 2] == U'い';
      bool is_nakatt_pattern = grammar::isARowCodepoint(cur_char) && next_char == U'な' &&
                               scan_pos + 3 < codepoints.size() && codepoints[scan_pos + 2] == U'か' &&
                               codepoints[scan_pos + 3] == U'っ';
      // Check for contracted negative ん pattern (分からん, 始まらん)
      // ん must be at the end of the string (hiragana_end == scan_pos + 2)
      bool is_n_pattern = grammar::isARowCodepoint(cur_char) && next_char == U'ん' && scan_pos + 2 == hiragana_end;
      // Check for classical negative ぬ pattern (分からぬ, 変わらぬ)
      bool is_nu_pattern = grammar::isARowCodepoint(cur_char) && next_char == U'ぬ';
      if (is_nai_pattern || is_nakatt_pattern || is_n_pattern || is_nu_pattern) {
        // Found A-row + negative pattern at scan_pos
        // The mizenkei would be from start_pos to scan_pos + 1
        size_t multi_miz_end = scan_pos + 1;
        grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(cur_char);
        if (verb_type != grammar::VerbType::Unknown) {
          // Construct the base form
          // E.g., 分から → 分かる (replace A-row ending with U-row)
          std::string_view base_suffix = grammar::godanBaseSuffixFromARow(cur_char);
          if (!base_suffix.empty()) {
            std::string stem = extractSubstring(codepoints, start_pos, scan_pos);
            std::string base_form = normalize::concat(stem, base_suffix);
            std::string surface = extractSubstring(codepoints, start_pos, multi_miz_end);
            // An internal te-form followed by a subsidiary/aspect verb is a
            // grammatical boundary, not the irrealis of one lexical verb
            // (描いていかない → 描い + て + いか + ない).
            // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
            if (vh::guardIsWired(vh::GuardMember::EmbedTeAuxiliary, vh::GuardOrigin::KanjiMizenkei) &&
                vh::embedsTeFormAuxiliary(surface)) {
              continue;
            }
            // The scan for the irrealis mora starts inside the okurigana and
            // runs to the end of the kana region, so it reaches past the word
            // and into the next phrase. A case particle in between marks an
            // argument boundary, which no single predicate spans: 資料 + を +
            // しら is read as the irrealis of the non-word 料をしる. Okurigana
            // that merely spells a case particle stays exempt, because the
            // irrealis mora closes the span immediately after it and the guard
            // requires kana on both sides of the particle (落と+さ+ない).
            // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
            if (vh::embedsCaseParticle(dict_manager, codepoints, start_pos, multi_miz_end)) {
              continue;
            }
            // Verify this is a valid verb
            bool is_valid_verb = vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                                        candidate::verb_cost::kConstructedVerbMinConfidence, true);
            if (!is_valid_verb) {
              const std::string observed_form = extractSubstring(codepoints, start_pos, hiragana_end);
              for (const auto& inflected : inflection.analyze(observed_form)) {
                if (inflected.verb_type == verb_type && inflected.base_form == base_form &&
                    inflected.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
                  is_valid_verb = true;
                  break;
                }
              }
            }
            // Reject a fabricated mizenkei that merely absorbs a trailing
            // binding particle (係助詞): 水すらない is noun + すら + ない, never
            // the mizenkei of a non-word godan-ra verb 水する. Only すら ends in
            // an a-row mora among binding particles, and no genuine godan verb
            // ends in 〜する, so this cannot suppress a real conjugation.
            // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
            if (is_valid_verb && !vh::isVerbInDictionary(dict_manager, base_form) &&
                vh::endsWithParticleTailOfPos(dict_manager, codepoints, start_pos, multi_miz_end,
                                              core::ExtendedPOS::ParticleBinding)) {
              SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << extractSubstring(codepoints, start_pos, multi_miz_end)
                                                << "\" fabricated mizenkei absorbing binding particle\n");
              is_valid_verb = false;
            }
            if (is_valid_verb) {
              constexpr float kCost = candidate::verb_cost::kStandardBonus;  // Same as other negative patterns
              const char* pattern = is_nakatt_pattern ? "multi_mizenkei_nakatt"
                                    : is_n_pattern    ? "multi_mizenkei_n"
                                    : is_nu_pattern   ? "multi_mizenkei_nu"
                                                      : "multi_mizenkei_nai";
              SUZUME_DEBUG_VERBOSE_BLOCK {
                SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " " << pattern << " lemma=" << base_form
                                    << " cost=" << kCost << "\n";
              }
              candidates.push_back(makeVerbCandidate(
                  surface, start_pos, multi_miz_end, kCost, base_form, grammar::verbTypeToConjType(verb_type), true,
                  CandidateOrigin::VerbKanji, 0.9F, pattern, core::ExtendedPOS::VerbMizenkei));
            }
          }
        }
        break;  // Only generate one candidate per position
      }
    }
  }
}

}  // namespace suzume::analysis::kanji_verb_detail
