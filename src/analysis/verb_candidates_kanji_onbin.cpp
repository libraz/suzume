/**
 * @file verb_candidates_kanji_onbin.cpp
 * @brief Kanji verb onbin candidate patterns
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

bool followsQuantityHead(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos < 2) {
    return false;
  }
  const char32_t previous = codepoints[start_pos - 1];
  const char32_t before_previous = codepoints[start_pos - 2];
  // Numeral+counter and productive quantity nouns ending in 数 both close
  // before the following predicate (二件|残っ, 複数|残っ).
  return (normalize::isCounterKanji(previous) && normalize::isNumeralCodepoint(before_previous)) ||
         (previous == U'数' && normalize::isKanjiCodepoint(before_previous));
}

void appendVerifiedTailGodanTaCompoundCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                 size_t kanji_end, const dictionary::DictionaryManager* dict_manager,
                                                 std::vector<UnknownCandidate>& candidates) {
  // A kanji prefix binding to a known one-kanji GodanTa verb carries every cell
  // of the compound's paradigm (先立つ, 先立ち, 先立って) off the tail's
  // conjugation class. Whether the prefix binds at all is lexical, though:
  // 波立つ is a word and 音立つ is a subject plus its predicate, and the two
  // prefixes are ordinary nouns with no grammatical difference at this position.
  // Without lexical evidence for the compound itself the pair is a subject and
  // a predicate, so a base form the dictionary does not carry stays split.
  if (dict_manager == nullptr || kanji_end != start_pos + 2 || kanji_end >= codepoints.size()) {
    return;
  }
  std::string stem = extractSubstring(codepoints, start_pos, kanji_end);
  std::string tail_base = extractSubstring(codepoints, kanji_end - 1, kanji_end) + "つ";
  if (!vh::isVerbInDictionary(dict_manager, tail_base) || !vh::isVerbInDictionary(dict_manager, stem + "つ")) {
    return;
  }

  char32_t ending = codepoints[kanji_end];
  size_t end_pos = kanji_end;
  core::ExtendedPOS extended_pos = core::ExtendedPOS::Unknown;
  const char* pattern = nullptr;
  if (ending == U'つ') {
    end_pos = kanji_end + 1;
    extended_pos = core::ExtendedPOS::VerbShuushikei;
    pattern = "tail_godan_ta_shuushikei";
  } else if (ending == U'ち') {
    end_pos = kanji_end + 1;
    extended_pos = core::ExtendedPOS::VerbRenyokei;
    pattern = "tail_godan_ta_renyokei";
  } else if (ending == U'っ' && kanji_end + 1 < codepoints.size() &&
             (codepoints[kanji_end + 1] == U'て' || codepoints[kanji_end + 1] == U'た')) {
    end_pos = kanji_end + 1;
    extended_pos = core::ExtendedPOS::VerbOnbinkei;
    pattern = "tail_godan_ta_sokuonbin";
  }
  if (pattern == nullptr) {
    return;
  }
  std::string surface = extractSubstring(codepoints, start_pos, end_pos);
  std::string base_form = stem + "つ";
  candidates.push_back(makeVerbCandidate(surface, start_pos, end_pos, candidate::kVerifiedTailCompoundVerbBonus,
                                         base_form, grammar::verbTypeToConjType(grammar::VerbType::GodanTa), true,
                                         CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence, pattern,
                                         extended_pos));
}

// Inflection-analysis fallback for a kanji 音便 stem whose base was not found in
// the dictionary: pick the highest-confidence (≥0.5) inflection of full_surface
// whose base_form/type matches one of the onbin candidate types. Shared by the
// い/ん extended-onbin and the て/だ hatsuonbin paths.
struct OnbinInflMatch {
  grammar::VerbType type{grammar::VerbType::Unknown};
  std::string base_form;
};
OnbinInflMatch bestOnbinInflMatch(const grammar::Inflection& inflection, const std::string& full_surface,
                                  const std::string& kanji_stem, grammar::GodanOnbinRange onbin_types) {
  OnbinInflMatch match;
  float best_conf = 0.0F;
  for (const auto& result : inflection.analyze(full_surface)) {
    if (result.confidence >= 0.5F && result.confidence > best_conf) {
      for (const auto& [verb_type, base_suffix] : onbin_types) {
        std::string base_form = normalize::concat(kanji_stem, base_suffix);
        if (result.base_form == base_form && result.verb_type == verb_type) {
          match.type = verb_type;
          match.base_form = base_form;
          best_conf = result.confidence;
          break;
        }
      }
    }
  }
  return match;
}

void appendKanjiOnbinCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                size_t hiragana_end, const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager, bool sokuonbin_stem_verified,
                                const std::string& sokuonbin_lemma, grammar::VerbType sokuonbin_verb_type,
                                std::vector<UnknownCandidate>& candidates) {
  // Generate Godan onbin stem candidates for contraction auxiliary patterns
  // E.g., 読んでる → 読ん (onbin of 読む) + でる (ている contraction)
  //       書いとく → 書い (onbin of 書く) + とく (ておく contraction)
  // Key patterns:
  // - kanji + ん + (ど/じ/で): GodanMa/GodanBa/GodanNa verbs (読んでる, 飛んどく)
  // - kanji + い + (と/ち): GodanKa/GodanGa verbs (書いとく, 泳いちゃう)
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // Check for hatsuonbin (ん), ikuon (い), or the lexical GodanWa
    // u-onbin (問うた) pattern.
    bool is_hatsuonbin = (first_hira == U'ん');
    bool is_ikuon = (first_hira == U'い');
    bool is_uonbin = (first_hira == U'う');
    if ((is_hatsuonbin || is_ikuon || is_uonbin) && kanji_end + 1 < hiragana_end) {
      char32_t next_char = codepoints[kanji_end + 1];
      bool is_contraction_pattern = false;
      size_t inflection_end = hiragana_end;
      if (is_hatsuonbin) {
        // ん + ど (どく/どいた) or じ (じゃう/じゃった) or で (でる/でた/でて)
        is_contraction_pattern = (next_char == U'ど' || next_char == U'じ' || next_char == U'で');
        // ん + で closes the euphonic cell on its own, and what follows is the
        // contracted aspect auxiliary. Asking the analyzer to read the whole run
        // as one verb hands it a shape no paradigm has, so where the base is not
        // in the dictionary it answers with a fabricated Ichidan lemma instead of
        // the Godan row the cell actually spells (混んでる → 混る, not 混む). The
        // ikuon branch below already stops at its own closed cell for the same
        // reason.
        if (next_char == U'で') {
          inflection_end = kanji_end + 2;
        }
      } else if (is_ikuon) {
        // い + と (とく/といた) or ち (ちゃう/ちゃった). The exact
        // two-kana い+た/だ run is also a closed past form. In that case the
        // full-form inflection analysis supplies the Godan-ka/ga class even
        // when the open-class lemma is absent from the dictionary, allowing
        // the grammatical 音便形 + 過去 auxiliary boundary to enter the
        // lattice (続い+た).
        const bool is_past = next_char == U'た' || next_char == U'だ';
        const bool is_exact_past_run = kanji_end + 2 == hiragana_end && is_past;
        const bool is_past_before_quotative = is_past && kanji_end + 3 < hiragana_end &&
                                              codepoints[kanji_end + 2] == U'っ' && codepoints[kanji_end + 3] == U'て';
        if (is_exact_past_run || is_past_before_quotative) {
          // The quote belongs after the completed past form.  Validate the
          // closed い+た/だ cell itself, rather than asking inflection to
          // interpret the following って as part of the verb.
          inflection_end = kanji_end + 2;
        }
        is_contraction_pattern =
            next_char == U'と' || next_char == U'ち' || is_exact_past_run || is_past_before_quotative;
      } else {
        // う音便 is a closed lexical GodanWa subclass, so it occurs only in
        // the simple past/te cells and only for an attested subclass stem.
        const std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
        is_contraction_pattern = grammar::isUOnbinStem(kanji_stem) && (next_char == U'た' || next_char == U'て');
      }
      if (is_contraction_pattern) {
        // Determine candidate verb types based on onbin type
        // Uses centralized GodanRow data instead of manual enumeration
        std::string_view onbin_str = is_hatsuonbin ? "ん" : (is_ikuon ? "い" : "う");
        const auto& candidates_to_try = vh::getGodanTypesByOnbin(onbin_str);
        // Get the kanji stem
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
        // First, check dictionary for ALL verb types before falling back to inflection
        // This ensures dictionary-verified verbs take precedence
        // Phase 1: Dictionary check
        auto onbin_match = vh::firstGodanOnbinDictBase(dict_manager, kanji_stem, onbin_str);
        grammar::VerbType matched_verb_type = onbin_match.verb_type;
        std::string matched_base_form = std::move(onbin_match.base_form);
        // Inflection analysis fallback (dictionary lookup above found nothing)
        if (matched_verb_type == grammar::VerbType::Unknown && kanji_end > start_pos) {
          std::string full_surface = extractSubstring(codepoints, start_pos, inflection_end);
          OnbinInflMatch infl = bestOnbinInflMatch(inflection, full_surface, kanji_stem, candidates_to_try);
          if (infl.type != grammar::VerbType::Unknown) {
            matched_verb_type = infl.type;
            matched_base_form = std::move(infl.base_form);
          }
        }
        if (matched_verb_type == grammar::VerbType::Unknown) {
          // No valid verb found
        } else {
          // Found valid verb - generate onbin stem candidate
          std::string onbin_surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
          constexpr float kOnbinCost = candidate::verb_cost::kStandardBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                                << " kanji_onbin_contraction lemma=" << matched_base_form << " cost=" << kOnbinCost
                                << "\n";
          }
          const char* pattern = is_hatsuonbin ? "kanji_hatsuonbin" : (is_ikuon ? "kanji_ikuon" : "kanji_uonbin");
          auto candidate =
              makeVerbCandidate(onbin_surface, start_pos, kanji_end + 1, kOnbinCost, matched_base_form,
                                grammar::verbTypeToConjType(matched_verb_type), true, CandidateOrigin::VerbKanji, 0.9F,
                                pattern, core::ExtendedPOS::VerbOnbinkei);
          candidate.lemma_verified = onbin_match.matched;
          candidates.push_back(std::move(candidate));
        }
      }
    }
  }

  // Generate Godan sokuonbin (っ) candidates for basic te/ta-form splitting
  // E.g., 言って → 言っ (onbin of 言う) + て (particle)
  //       言った → 言っ (onbin of 言う) + た (auxiliary)
  //       待って → 待っ (onbin of 待つ) + て (particle)
  //       買って → 買っ (onbin of 買う) + て (particle)
  // Key patterns:
  // - kanji + っ + て/た/たら/たり: GodanRa/GodanTa/GodanWa verbs
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // Check for sokuonbin (っ) pattern
    if (first_hira == U'っ' && kanji_end + 1 < hiragana_end) {
      char32_t next_char = codepoints[kanji_end + 1];
      // Basic te/ta form patterns (て, た, たら, たり), ちゃう (ち), and とく (と) contractions
      bool is_te_ta_pattern = (next_char == U'て' || next_char == U'た' || next_char == U'ち' || next_char == U'と');
      if (is_te_ta_pattern) {
        const auto& sokuonbin_types = vh::getGodanTypesByOnbin("っ");
        // Get the kanji stem
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);

#ifdef SUZUME_DEBUG
        // TRACE: Collect all candidates for logging (debug builds only)
        std::string onbin_surface_for_log = extractSubstring(codepoints, start_pos, kanji_end + 1);
        struct SokuonbinCandidate {
          grammar::VerbType type;
          std::string base_form;
          bool dict_match;
        };
        std::vector<SokuonbinCandidate> all_sokuonbin_candidates;
#endif

        // First, check dictionary for ALL verb types
        grammar::VerbType matched_verb_type = grammar::VerbType::Unknown;
        std::string matched_base_form;
        bool matched_via_dict = false;
        for (const auto& [verb_type, base_suffix] : sokuonbin_types) {
          std::string base_form = normalize::concat(kanji_stem, base_suffix);
          bool dict_match = vh::isVerbInDictionary(dict_manager, base_form);
#ifdef SUZUME_DEBUG
          all_sokuonbin_candidates.push_back({verb_type, base_form, dict_match});
#endif
          if (dict_match && matched_verb_type == grammar::VerbType::Unknown) {
            matched_verb_type = verb_type;
            matched_base_form = base_form;
            matched_via_dict = true;
          }
        }
        // Sokuonbin compound (突っ走る) whose stem was verified via its embedded verb:
        // the compound itself is absent from the dictionary, so emit the onbin stem
        // (突っ走っ) here with the embedded verb's type/base so た/て split off exactly
        // like a plain verb (走った → 走っ + た), rather than the whole form winning.
        if (matched_verb_type == grammar::VerbType::Unknown && sokuonbin_stem_verified &&
            sokuonbin_verb_type != grammar::VerbType::Unknown && !sokuonbin_lemma.empty()) {
          matched_verb_type = sokuonbin_verb_type;
          matched_base_form = sokuonbin_lemma;
          matched_via_dict = true;
        }
        // Phase 2: Inflection analysis fallback
        // Try progressively shorter surfaces to handle cases where hiragana_end
        // includes particles (e.g., "使っているが" vs "使っている")
        // Skip if kanji stem starts with a dictionary noun (e.g., 昨日買っ → skip)
        // This prevents false compound verb candidates like "昨日買う"
        bool starts_with_dict_noun = false;
        bool remainder_is_dict_verb = false;
        if (dict_manager != nullptr && kanji_end - start_pos >= 2) {
          // Check if any prefix of kanji_stem is a dictionary noun
          for (size_t prefix_len = 1; prefix_len < kanji_end - start_pos; ++prefix_len) {
            std::string prefix = extractSubstring(codepoints, start_pos, start_pos + prefix_len);
            if (verb_helpers::isNounInDictionary(dict_manager, prefix)) {
              starts_with_dict_noun = true;
              SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << kanji_stem << "\" starts with dict noun \"" << prefix
                                                        << "\"\n");
              break;
            }
          }
          // Also check: if removing single-kanji prefix leaves a valid dict verb
          // E.g., 本買う → 本 + 買う, where 買う is a dict verb
          // This handles patterns like 本買った, 服買った, 車買った
          if (!starts_with_dict_noun && kanji_end - start_pos == 2) {
            // Get the second kanji + verb ending
            std::string remainder_stem = extractSubstring(codepoints, start_pos + 1, kanji_end);
            for (const auto& [verb_type, base_suffix] : sokuonbin_types) {
              std::string remainder_base = normalize::concat(remainder_stem, base_suffix);
              if (vh::isVerbInDictionary(dict_manager, remainder_base)) {
                remainder_is_dict_verb = true;
                SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << kanji_stem << "\" remainder \"" << remainder_base
                                                          << "\" is dict verb\n");
                break;
              }
            }
          }
        }
        if (matched_verb_type == grammar::VerbType::Unknown && !starts_with_dict_noun && !remainder_is_dict_verb) {
          // A one-kanji stem picked out of the middle of a dictionary word
          // splits that word and invents a verb from its tail (事故った as
          // 事 + 故る). Only the analyzer's guess licenses this branch, so it is
          // no evidence against a registered headword.
          if (kanji_stem.size() == core::kJapaneseCharBytes &&
              !vh::splitsDictionaryKanjiWord(dict_manager, codepoints, start_pos, kanji_end)) {
            // Single-kanji stem: use inflection analysis of the longer surface
            // (kanji + っ + following chars) to find verb type.
            // Common verbs like 残る, 立つ, 打つ may not be in L2 dictionary.
            // Try surfaces of increasing length to get inflection result.
            for (size_t try_end = kanji_end + 2; try_end <= codepoints.size() && try_end <= kanji_end + 4; ++try_end) {
              std::string try_surface = extractSubstring(codepoints, start_pos, try_end);
              auto infl_result = inflection.analyze(try_surface);
              if (!infl_result.empty()) {
                const auto& best = infl_result[0];
                if (best.confidence >= 0.6F) {
                  for (const auto& [verb_type, base_suffix] : sokuonbin_types) {
                    if (best.verb_type == verb_type) {
                      matched_verb_type = verb_type;
                      matched_base_form = normalize::concat(kanji_stem, base_suffix);
                      SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] \"" << kanji_stem << "\" single-kanji sokuonbin → "
                                                                << matched_base_form
                                                                << " (infl, conf=" << best.confidence << ")\n");
                      break;
                    }
                  }
                  if (matched_verb_type != grammar::VerbType::Unknown)
                    break;
                }
              }
            }
          } else {
            SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << kanji_stem << "\" skip non-dict sokuonbin\n");
          }
        }

        // A multi-kanji stem can itself be an unregistered open-class verb.
        // A dictionary noun prefix alone is not proof of a boundary (戸 is a
        // noun but 戸惑う is one predicate); the decisive counter-evidence is
        // a dictionary verb in the remainder, as in 本+買った.  When no such
        // remainder exists, admit only an exact full-form inflection match and
        // keep it unverified/neutral so stronger lexical boundaries still win.
        if (matched_verb_type == grammar::VerbType::Unknown && kanji_end > start_pos + 1 && !remainder_is_dict_verb) {
          const std::string full_surface = extractSubstring(codepoints, start_pos, hiragana_end);
          const OnbinInflMatch infl = bestOnbinInflMatch(inflection, full_surface, kanji_stem, sokuonbin_types);
          if (infl.type != grammar::VerbType::Unknown) {
            matched_verb_type = infl.type;
            matched_base_form = infl.base_form;
          }
        }

#ifdef SUZUME_DEBUG
        // TRACE: Log all sokuonbin candidates
        SUZUME_DEBUG_TRACE_BLOCK {
          SUZUME_DEBUG_STREAM << "[SOKUONBIN_CANDIDATES] \"" << onbin_surface_for_log << "\":\n";
          constexpr float kSokuonbinCost = candidate::verb_cost::kStandardBonus;
          for (const auto& cand : all_sokuonbin_candidates) {
            bool is_selected = (cand.type == matched_verb_type);
            SUZUME_DEBUG_STREAM << "  - " << cand.base_form << " (" << grammar::verbTypeToString(cand.type) << "): "
                                << "dict_match=" << (cand.dict_match ? "YES" : "NO")
                                << ", score=" << (cand.dict_match ? kSokuonbinCost : 0.0F)
                                << (is_selected ? "" : " (skipped)") << "\n";
          }
          if (matched_verb_type != grammar::VerbType::Unknown) {
            SUZUME_DEBUG_STREAM << "  → Selected: " << matched_base_form << " ("
                                << grammar::verbTypeToString(matched_verb_type) << ")\n";
          } else {
            SUZUME_DEBUG_STREAM << "  → No match found\n";
          }
        }
#endif

        // A non-dictionary sokuonbin candidate that begins inside a kanji run
        // must not take its っ from the head of a dictionary particle. The run
        // is a compound nominal that the particle marks (資料 + って, 確認 +
        // って), so the fabricated predicate splits the nominal and steals the
        // particle's first mora in one move (資 + 料っ + て). A dictionary-backed
        // base keeps its ordinary te-form reading (見 + 合って).
        // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
        bool sokuon_heads_dictionary_particle = false;
        if (!matched_via_dict && dict_manager != nullptr && start_pos > 0 &&
            normalize::isKanjiCodepoint(codepoints[start_pos - 1]) && !followsQuantityHead(codepoints, start_pos) &&
            kanji_end + 1 < codepoints.size()) {
          constexpr size_t kParticleProbe = 3;
          const size_t max_particle_end = std::min(codepoints.size(), kanji_end + kParticleProbe);
          for (size_t particle_end = kanji_end + 2; particle_end <= max_particle_end; ++particle_end) {
            if (lookupEntryInRange(*dict_manager, codepoints, kanji_end, particle_end, core::PartOfSpeech::Particle) !=
                nullptr) {
              sokuon_heads_dictionary_particle = true;
              SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << kanji_stem << "\" sokuon heads a dictionary particle\n");
              break;
            }
          }
        }

        if (matched_verb_type != grammar::VerbType::Unknown && !sokuon_heads_dictionary_particle) {
          // Found valid verb - generate sokuonbin stem candidate
          // Dict-matched verbs get bonus (-0.5) to beat unsplit forms
          // Inflection-only matches get neutral cost (0) to avoid false positives
          // like 像っ (from 像る which is not a real verb)
          std::string onbin_surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
          // Dict-matched verbs get bonus (-0.5) to beat unsplit forms
          // Inflection-only matches (2-kanji stems only) get neutral cost
          const float sokuonbin_cost = matched_via_dict ? candidate::verb_cost::kStandardBonus : bigram_cost::kNeutral;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface << " kanji_sokuonbin lemma=" << matched_base_form
                                << " cost=" << sokuonbin_cost << (matched_via_dict ? " (dict)" : " (infl)") << "\n";
          }
          auto candidate =
              makeVerbCandidate(onbin_surface, start_pos, kanji_end + 1, sokuonbin_cost, matched_base_form,
                                grammar::verbTypeToConjType(matched_verb_type), true, CandidateOrigin::VerbKanji, 0.9F,
                                "kanji_sokuonbin", core::ExtendedPOS::VerbOnbinkei);
          // The non-dictionary fallback reaches here only for a one-kanji
          // stem whose complete sokuonbin form was validated by inflection.
          // Preserve that evidence, as the extended and te-auxiliary paths do,
          // so an ordinary verb does not lose to a fabricated noun boundary.
          candidate.lemma_verified = matched_via_dict || kanji_end == start_pos + 1;
          candidates.push_back(std::move(candidate));
        }
      }
    }
  }

  appendExtendedSokuonbinCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                                    candidates);

  // Generate Godan hatsuonbin (ん) candidates for basic te/ta-form splitting
  // E.g., 読んだ → 読ん (onbin of 読む) + だ (auxiliary)
  //       読んで → 読ん (onbin of 読む) + で (particle)
  //       飛んだ → 飛ん (onbin of 飛ぶ) + だ (auxiliary)
  //       死んだ → 死ん (onbin of 死ぬ) + だ (auxiliary)
  // Key patterns:
  // - kanji + ん + で/だ: GodanMa/GodanBa/GodanNa verbs
  if (kanji_end < hiragana_end) {
    char32_t first_hira = codepoints[kanji_end];
    // Check for hatsuonbin (ん) pattern
    if (first_hira == U'ん' && kanji_end + 1 < hiragana_end) {
      char32_t next_char = codepoints[kanji_end + 1];
      // Basic te/ta form patterns (で, だ)
      bool is_de_da_pattern = (next_char == U'で' || next_char == U'だ');
      if (is_de_da_pattern) {
        const auto& hatsuonbin_types = vh::getGodanTypesByOnbin("ん");
        // Get the kanji stem
        std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);

        // First, check dictionary for ALL verb types
        auto hatsuonbin_match = vh::firstGodanOnbinDictBase(dict_manager, kanji_stem, "ん");
        grammar::VerbType matched_verb_type = hatsuonbin_match.verb_type;
        std::string matched_base_form = std::move(hatsuonbin_match.base_form);
        // Inflection analysis fallback (dictionary lookup above found nothing)
        if (matched_verb_type == grammar::VerbType::Unknown) {
          std::string full_surface = extractSubstring(codepoints, start_pos, hiragana_end);
          OnbinInflMatch infl = bestOnbinInflMatch(inflection, full_surface, kanji_stem, hatsuonbin_types);
          if (infl.type != grammar::VerbType::Unknown) {
            matched_verb_type = infl.type;
            matched_base_form = std::move(infl.base_form);
          }
        }

        if (matched_verb_type != grammar::VerbType::Unknown) {
          // Found valid verb - generate hatsuonbin stem candidate
          std::string onbin_surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
          constexpr float kHatsuonbinCost = candidate::verb_cost::kStandardBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface << " kanji_hatsuonbin lemma=" << matched_base_form
                                << " cost=" << kHatsuonbinCost << "\n";
          }
          auto candidate =
              makeVerbCandidate(onbin_surface, start_pos, kanji_end + 1, kHatsuonbinCost, matched_base_form,
                                grammar::verbTypeToConjType(matched_verb_type), true, CandidateOrigin::VerbKanji, 0.9F,
                                "kanji_hatsuonbin", core::ExtendedPOS::VerbOnbinkei);
          // Both branches above prove the complete Xん+で/だ paradigm: either
          // the base lemma is in the dictionary or full-form inflection
          // reconstructs a matching nasal-euphonic row. Preserve that evidence
          // on this first candidate too; otherwise later dedup can discard the
          // already-verified duplicate emitted by the extended handler.
          candidate.lemma_verified = true;
          candidates.push_back(std::move(candidate));
        }
      }
    }
  }

  // Generate hatsuonbin candidates for multi-hiragana okurigana and standalone ん
  // Covers cases NOT handled by the de/da handler above:
  // - Multi-hira okurigana: 汗ばんだ → 汗ばん (onbin of 汗ばむ) + だ
  // - Standalone single ん: 死ん (end of token, no following で/だ)
  if (kanji_end < hiragana_end) {
    for (size_t n_pos = kanji_end; n_pos < hiragana_end; ++n_pos) {
      if (codepoints[n_pos] != U'ん')
        continue;

      bool at_end = (n_pos + 1 >= hiragana_end);
      bool followed_by_de_da = (!at_end) && (codepoints[n_pos + 1] == U'で' || codepoints[n_pos + 1] == U'だ');

      // Skip: n_pos == kanji_end && !at_end is already handled by de/da handler above
      if (n_pos == kanji_end && !at_end)
        continue;
      // Only valid: at end of hiragana region, or followed by で/だ
      if (!at_end && !followed_by_de_da)
        continue;

      std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
      std::string hira_stem = (n_pos > kanji_end) ? extractSubstring(codepoints, kanji_end, n_pos) : "";
      const std::string lexical_stem = kanji_stem + hira_stem;

      // The kana between the kanji and the euphony is the verb's own okurigana,
      // and a nasal euphony belongs to a Godan verb ending in む/ぶ/ぬ, whose
      // okurigana never spells the connective (汗ば+ん, 苦し+ん, 慈し+ん). Where
      // it does, the run is a continuative that has already handed its clause on
      // and the euphony belongs to whatever follows: 見+て+くん, not a euphonic
      // cell of the non-word 見てくむ. The kanji-only stem is unaffected, and so
      // is the case where the onbin form settles first (書い+て+く+ん+だ).
      const bool okurigana_spells_connective =
          std::any_of(codepoints.begin() + static_cast<std::ptrdiff_t>(kanji_end),
                      codepoints.begin() + static_cast<std::ptrdiff_t>(n_pos),
                      [](char32_t kana) { return kana == U'て' || kana == U'で'; });
      if (okurigana_spells_connective) {
        break;
      }

      // A complete predicate followed by the nominalizer ん and copula だ is
      // explanatory (食べる+ん+だ, 高い+ん+だ), not a nasal-euphonic verb.
      // Genuine hatsuonbin has an incomplete stem before ん (読+ん+だ,
      // 汗ば+ん+だ), so exact predicate evidence separates the two shapes.
      constexpr PartOfSpeechMask kPredicateMask =
          partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Adjective);
      const bool exact_predicate =
          dict_manager != nullptr && hasExactPartOfSpeech(*dict_manager, lexical_stem, kPredicateMask);
      const auto& predicate_analyses = inflection.analyze(lexical_stem);
      const bool analyzed_complete_predicate =
          std::any_of(predicate_analyses.begin(), predicate_analyses.end(), [&](const auto& analysis) {
            return analysis.base_form == lexical_stem && analysis.verb_type != grammar::VerbType::Unknown &&
                   analysis.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
          });
      const bool explanatory_n_da =
          followed_by_de_da && codepoints[n_pos + 1] == U'だ' && (exact_predicate || analyzed_complete_predicate);
      if (explanatory_n_da) {
        break;
      }

      auto n_onbin_match = vh::firstGodanOnbinDictBase(dict_manager, lexical_stem, "ん");
      grammar::VerbType matched_type = n_onbin_match.verb_type;
      std::string matched_base = std::move(n_onbin_match.base_form);
      // A nasal euphony replaces the continuative's own final mora (読み → 読ん,
      // 汗ばみ → 汗ばん), so the kana in front of it is never the a-row. That row
      // spells the irrealis, and an irrealis before ん is the contracted negative
      // ぬ — an auxiliary of its own, whose boundary the analysis has to keep
      // (待た+ん, 知ら+ん, 分から+ん, 読ま+ん). The euphonic reading hands its
      // clause on to て/で/た/だ, so this only has to be settled where the kana
      // run ends: with で or だ behind it the euphony is already proven.
      if (at_end && n_pos > kanji_end && grammar::isARowCodepoint(codepoints[n_pos - 1])) {
        break;
      }
      if (matched_type == grammar::VerbType::Unknown && followed_by_de_da) {
        const std::string full_surface = extractSubstring(codepoints, start_pos, n_pos + 2);
        OnbinInflMatch infl =
            bestOnbinInflMatch(inflection, full_surface, lexical_stem, vh::getGodanTypesByOnbin("ん"));
        matched_type = infl.type;
        matched_base = std::move(infl.base_form);
      }
      if (matched_type != grammar::VerbType::Unknown) {
        std::string onbin_surface = extractSubstring(codepoints, start_pos, n_pos + 1);
        constexpr float kHatsuonbinCost = candidate::verb_cost::kStandardBonus;
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface
                              << " kanji_hatsuonbin_standalone lemma=" << matched_base << " cost=" << kHatsuonbinCost
                              << "\n";
        }
        auto candidate = makeVerbCandidate(onbin_surface, start_pos, n_pos + 1, kHatsuonbinCost, matched_base,
                                           grammar::verbTypeToConjType(matched_type), true, CandidateOrigin::VerbKanji,
                                           0.9F, "kanji_hatsuonbin", core::ExtendedPOS::VerbOnbinkei);
        candidate.lemma_verified = n_onbin_match.matched || followed_by_de_da;
        candidates.push_back(std::move(candidate));
      }
      break;  // Only process first ん in the region
    }
  }
}

}  // namespace suzume::analysis::kanji_verb_detail
