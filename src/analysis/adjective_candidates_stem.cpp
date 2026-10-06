/**
 * @file adjective_candidates_stem.cpp
 * @brief I-adjective stem candidate generation
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
#include "grammar/auxiliaries.h"
#include "grammar/char_patterns.h"
#include "grammar/connection.h"
#include "grammar/patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

using verb_helpers::addEmphaticVariants;
using verb_helpers::isAdjectiveInDictionary;
using verb_helpers::isEmphaticChar;
using verb_helpers::isVerbInDictionary;

using adj_detail::makeIAdjCandidate;
using adj_detail::makeIAdjCellCandidate;
using adj_detail::makeIAdjStemCandidate;

namespace {

bool hasNaAdjectiveStemEvidence(const std::string& stem, const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager != nullptr) {
    const auto* entry = dict_manager->lookupExact(stem, core::PartOfSpeech::Adjective);
    if (entry != nullptr && entry->extended_pos == core::ExtendedPOS::AdjNaAdj) {
      return true;
    }
  }
  // -やか is a productive na-adjective head (軽やか、華やか、健やか).
  // Do not extend this to -らか: 柔らか+さ is the stem of 柔らかい.
  return utf8::endsWith(stem, "やか");
}

std::vector<std::string_view> iAdjectiveStemFollowers(std::string_view hiragana, size_t byte_pos,
                                                      const dictionary::DictionaryManager* dict_manager) {
  std::vector<std::string_view> followers;
  std::array<bool, 3> seen_auxiliary{};
  if (dict_manager != nullptr) {
    for (const auto& result : dict_manager->lookup(hiragana, byte_pos)) {
      if (result.entry == nullptr) {
        continue;
      }
      size_t auxiliary_index = seen_auxiliary.size();
      switch (result.entry->extended_pos) {
        case core::ExtendedPOS::AuxGaru:
          auxiliary_index = 0;
          break;
        case core::ExtendedPOS::AuxExcessive:
          auxiliary_index = 1;
          break;
        case core::ExtendedPOS::AuxAppearanceSou:
          auxiliary_index = 2;
          break;
        default:
          break;
      }
      if (auxiliary_index < seen_auxiliary.size() && !seen_auxiliary[auxiliary_index]) {
        followers.push_back(result.entry->surface);
        seen_auxiliary[auxiliary_index] = true;
      }
    }
  }

  const std::string_view remaining = hiragana.substr(byte_pos);
  static constexpr std::array<std::string_view, 3> kDerivedSuffixes = {"さ", "み", "げ"};
  for (const std::string_view suffix : kDerivedSuffixes) {
    if (utf8::startsWith(remaining, suffix)) {
      followers.push_back(suffix);
    }
  }
  return followers;
}

bool hasInternalNominalDerivationalBoundary(const std::string& stem,
                                            const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || isAdjectiveInDictionary(dict_manager, stem + "い")) {
    return false;
  }

  const auto stem_codepoints = normalize::toCodepoints(stem);
  for (size_t boundary = 1; boundary < stem_codepoints.size(); ++boundary) {
    const std::string left = extractSubstring(stem_codepoints, 0, boundary);
    if (isAdjectiveInDictionary(dict_manager, left + "い")) {
      continue;
    }

    const std::string right = extractSubstring(stem_codepoints, boundary, stem_codepoints.size());
    const auto* adjective = dict_manager->lookupExact(right, core::PartOfSpeech::Adjective);
    if (adjective != nullptr && adjective->extended_pos == core::ExtendedPOS::AdjStem) {
      return true;
    }
    const auto* auxiliary = dict_manager->lookupExact(right, core::PartOfSpeech::Auxiliary);
    if (auxiliary != nullptr && core::isConjectureRashiiType(auxiliary->extended_pos)) {
      return true;
    }
  }
  return false;
}

bool isPossibleUnknownIAdjectiveStem(const std::string& stem, const std::string& base_form,
                                     const dictionary::DictionaryManager* dict_manager) {
  if (isAdjectiveInDictionary(dict_manager, base_form)) {
    return true;
  }

  // Native i-adjectives do not form an unknown base by appending い to an
  // e-row okurigana (静け+い).  This is a conjugational shape constraint, not
  // a lexical whitelist; kanji-final stems and the a/i/u/o rows remain open.
  const char32_t stem_last = utf8::decodeFirstChar(utf8::lastChar(stem));
  if (kana::isERowCodepoint(stem_last)) {
    return false;
  }

  // く is the continuative ending of the i-adjective paradigm, so a span ending
  // in it is already an inflected form and cannot also be a stem awaiting い
  // (面倒く+い, 古く+い). The compound-adjective suffix that follows it (くさい,
  // くない) keeps its own boundary. Attested bases ending in く (にくい) are
  // dictionary entries and returned above.
  if (stem_last == U'く') {
    return false;
  }

  // Do not absorb a complete dictionary-verified verb continuative into a
  // fabricated adjective stem (語り+ぐ+い, 読み+やす+い).  Productive derived
  // adjectives retain their own morpheme boundary and are emitted elsewhere.
  const auto stem_codepoints = normalize::toCodepoints(stem);
  return !verb_helpers::startsWithVerbContinuative(dict_manager, stem_codepoints, 0, stem_codepoints.size());
}

// A derived i-adjective can contain a complete predicate plus a productive
// auxiliary chain (書い+て+ほしい, 読み+やすい, 書き+にくい).  When that parse
// has a dictionary-verified predicate base and multiple inflectional
// morphemes, its internal boundaries are stronger evidence than the competing
// hypothesis that the entire surface is one unknown adjective stem.
bool hasVerifiedPredicateDerivedAdjective(const std::string& base_form, const grammar::Inflection& inflection,
                                          const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return false;
  }
  for (const auto& analysis : inflection.analyze(base_form)) {
    if (analysis.verb_type == grammar::VerbType::IAdjective || !utf8::endsWith(analysis.suffix, "い")) {
      continue;
    }
    bool has_derived_adjective_attachment = false;
    for (const auto& auxiliary : grammar::getAuxiliaries()) {
      if (!auxiliary.surface.empty() && utf8::endsWith(analysis.suffix, auxiliary.surface) &&
          (auxiliary.required_conn == grammar::conn::kVerbRenyokei ||
           auxiliary.required_conn == grammar::conn::kAuxOutTe)) {
        has_derived_adjective_attachment = true;
        break;
      }
    }
    if (!has_derived_adjective_attachment) {
      continue;
    }
    const bool productive_predicate =
        analysis.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence &&
        (analysis.verb_type == grammar::VerbType::Ichidan || grammar::isGodanVerbType(analysis.verb_type));
    if (isVerbInDictionary(dict_manager, analysis.base_form) || productive_predicate) {
      return true;
    }
  }
  return false;
}

float productiveIAdjectiveStemConfidence(const std::string& stem, const std::string& base_form,
                                         const grammar::Inflection& inflection,
                                         const dictionary::DictionaryManager* dict_manager) {
  if (isAdjectiveInDictionary(dict_manager, base_form)) {
    return candidate::kDictionaryOriginConfidence;
  }

  // A generated stem cannot end by swallowing a completed grammatical
  // boundary from its left context (問題+を、彼+は、資料+の).  Restrict this to
  // case/topic/nominalizing particles: sentence-final か is homographic with
  // the genuine i-adjective stem in 暖か+さ and must remain available.
  if (dict_manager != nullptr) {
    const auto* particle = dict_manager->lookupExact(utf8::lastChar(stem), core::PartOfSpeech::Particle);
    if (particle != nullptr && (particle->extended_pos == core::ExtendedPOS::ParticleCase ||
                                particle->extended_pos == core::ExtendedPOS::ParticleTopic ||
                                particle->extended_pos == core::ExtendedPOS::ParticleNo)) {
      return candidate::kNoOriginConfidence;
    }
  }

  const float confidence = adj_detail::firstConfidenceAtLeast(
      inflection.analyze(base_form), grammar::VerbType::IAdjective, candidate::kCompoundAdjConfMin);
  if (confidence == candidate::kNoOriginConfidence) {
    return candidate::kNoOriginConfidence;
  }

  // An i-row ending can instead be a productive godan continuative.  Lexical
  // evidence for that verb wins (書き/話し/積もり), while stems without a
  // competing predicate retain the adjective analysis.
  const char32_t final_codepoint = utf8::decodeFirstChar(utf8::lastChar(stem));
  const std::string_view godan_suffix = grammar::godanBaseSuffixFromIRow(final_codepoint);
  if (!godan_suffix.empty()) {
    const std::string verb_base = normalize::concat(utf8::dropLastChar(stem), godan_suffix);
    if (isVerbInDictionary(dict_manager, verb_base)) {
      return candidate::kNoOriginConfidence;
    }
    // Outside the productive -しい class, an unregistered i-row ending is
    // much stronger evidence for a godan continuative (積もり/書き) than for
    // a fictitious adjective ending in りい/きい.  Established forms such as
    // 大きい are accepted by the dictionary branch above.
    if (final_codepoint != U'し') {
      return candidate::kNoOriginConfidence;
    }
  }

  const auto stem_codepoints = normalize::toCodepoints(stem);
  if (!stem_codepoints.empty() && normalize::isKanjiCodepoint(stem_codepoints.back())) {
    // An all-kanji compound adjective can inherit a productive adjectival head
    // (心+細い).  Requiring the final head prevents arbitrary nouns such as
    // 子供 from becoming fictitious 子供い adjectives before げ/さ.
    const std::string head_base = normalize::concat(utf8::lastChar(stem), "い");
    return isAdjectiveInDictionary(dict_manager, head_base) ? confidence : candidate::kNoOriginConfidence;
  }
  // A compound-forming kana head is inherited the same way (照れ+くさい,
  // 面倒+くさい), provided a host stands in front of it.
  // A head the dictionary lists as an auxiliary makes no adjective: its cells
  // are that auxiliary's own (本+らし+げ, as 本+らし+さ), unless the whole
  // adjective is listed, which the first check above already accepted.
  for (size_t head_start = 1; head_start < stem_codepoints.size(); ++head_start) {
    const std::string head_base = extractSubstring(stem_codepoints, head_start, stem_codepoints.size());
    if (verb_helpers::hasDictionaryEntry(dict_manager, head_base + "い", core::PartOfSpeech::Auxiliary)) {
      return candidate::kNoOriginConfidence;
    }
    if (adj_detail::isCompoundFormingAdjective(head_base + "い")) {
      return confidence;
    }
  }

  // One-kanji+るい is a productive shape already recognized for complete
  // adjectives (明るい/明るく), whose generic inflection confidence is low
  // because of the homographic godan analysis.
  const bool single_kanji_rui_stem =
      stem_codepoints.size() == 2 && normalize::isKanjiCodepoint(stem_codepoints[0]) && stem_codepoints[1] == U'る';
  if (single_kanji_rui_stem || utf8::endsWith(stem, "し") || confidence >= candidate::kIAdjConfMin) {
    return confidence;
  }
  return candidate::kNoOriginConfidence;
}

// A stem whose okurigana ends inside a multi-mora auxiliary cell, one that
// begins after at least one host mora and runs past the stem, has swallowed the
// head of that auxiliary (食べや+がっ is 食べ+やがっ, not a stem 食べや).
bool opensAuxiliaryInsideStem(const dictionary::DictionaryManager* dict_manager,
                              const std::vector<char32_t>& codepoints, size_t start_pos, size_t stem_end) {
  if (dict_manager == nullptr) {
    return false;
  }
  constexpr size_t kLongestAuxiliaryCell = 4;
  for (size_t cell_start = start_pos + 1; cell_start < stem_end; ++cell_start) {
    if (!kana::isHiraganaCodepoint(codepoints[cell_start])) {
      continue;
    }
    const size_t last_end = std::min(codepoints.size(), cell_start + kLongestAuxiliaryCell);
    for (size_t cell_end = std::max(stem_end + 1, cell_start + 2); cell_end <= last_end; ++cell_end) {
      if (dict_manager->lookupExact(extractSubstring(codepoints, cell_start, cell_end),
                                    core::PartOfSpeech::Auxiliary) != nullptr) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

void generateAdjectiveStemCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const std::vector<normalize::CharType>& char_types,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  // Must start with kanji
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // A one-kanji adjective stem before the kanji spelling of the excessive
  // 過ぎる (高+過ぎる) takes the same stem reading as before すぎる; the kanji
  // run scan below would otherwise read 高過 as one stem.
  if (start_pos + 2 < codepoints.size() && codepoints[start_pos + 1] == U'過' && codepoints[start_pos + 2] == U'ぎ') {
    const std::string stem = extractSubstring(codepoints, start_pos, start_pos + 1);
    const std::string base_form = stem + "い";
    if (isAdjectiveInDictionary(dict_manager, base_form)) {
      const float cost = candidate::confidenceScaledCost(
          candidate::kAdjStemBaseCost, candidate::kDictFallbackAdjConfidence, candidate::kAdjStemConfScale);
      candidates.push_back(makeIAdjStemCandidate(stem, start_pos, start_pos + 1, base_form, cost,
                                                 CandidateOrigin::AdjectiveI, candidate::kDictFallbackAdjConfidence,
                                                 "adj_stem_before_kanji_excessive"));
    }
  }

  // Kanji portion (1-2 characters for adjective stem) followed by hiragana
  size_t kanji_end = findCharRegionEndBeforeHiragana(char_types, start_pos, 2, normalize::CharType::Kanji);
  if (kanji_end == start_pos) {
    return;
  }

  // Find hiragana ending with し + auxiliary pattern (そう, すぎ, etc.)
  size_t hiragana_end = findCharRegionEnd(char_types, kanji_end, 8, normalize::CharType::Hiragana);

  if (hiragana_end <= kanji_end) {
    return;
  }

  const std::string hiragana_part = extractSubstring(codepoints, kanji_end, hiragana_end);
  const std::string kanji_part = extractSubstring(codepoints, start_pos, kanji_end);
  SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM] pos=" << start_pos << " kanji=\"" << kanji_part << "\" hiragana=\""
                                             << hiragana_part << "\"\n");

  // =============================================================================
  // Pattern 1: Regular i-adjective stem + すぎる/がる/さ/そう (ガル接続)
  // =============================================================================
  // MeCab handles regular i-adjectives (高い, 尊い, 寒い) differently from しい-adjectives.
  // For patterns like 高すぎる, MeCab splits as: 高(ADJ, ガル接続) + すぎる(VERB)
  // The adjective stem is just the kanji portion (without い).
  //
  // Patterns handled:
  // - 高すぎる → 高 (ADJ stem) + すぎる (VERB)
  // - 尊すぎて → 尊 (ADJ stem) + すぎ (VERB) + て (PARTICLE)
  // - 高がる → 高 (ADJ stem) + がる (VERB)
  // - 高さ → 高 (ADJ stem) + さ (NOUN/SUFFIX)
  // - 高そう → 高 (ADJ stem) + そう (AUX)
  // AuxGaru/AuxExcessive/AuxAppearanceSou are discovered from their canonical
  // dictionary EPOS paradigm. Productive nominal suffixes remain structural.
  for (const std::string_view pattern : iAdjectiveStemFollowers(hiragana_part, 0, dict_manager)) {
    if (utf8::startsWith(hiragana_part, pattern)) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   pattern=\"" << pattern << "\" matched, hiragana=\"" << hiragana_part
                                                         << "\"\n");

      // Check for サ変 passive/causative pattern: さ + れ/せ
      // E.g., 処理される, 勉強させる - these are NOT adjective nominalization
      if (pattern == "さ" && hiragana_part.size() > 3) {
        const std::string_view after_sa = std::string_view(hiragana_part).substr(3);  // Skip さ (3 bytes)
        if (utf8::startsWithAny(after_sa, {"れ", "せ"})) {
          SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   skip: サ変 passive/causative (さ+" << after_sa.substr(0, 3) << ")\n");
          continue;  // Skip - this is likely サ変 passive/causative, not adjective
        }
      }

      // Check if hiragana_part is a suffix in dictionary (さん, さま, etc.)
      // E.g., 姉さん = 姉 + さん (NOUN + SUFFIX), not 姉 + さ (ADJ stem + nominalization)
      // EXCEPT: "さ" alone is valid for adjective nominalization (高さ, 明るさ, 優しさ)
      if (hiragana_part != "さ" &&
          verb_helpers::hasDictionaryEntry(dict_manager, hiragana_part, core::PartOfSpeech::Suffix)) {
        SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   skip: suffix \"" << hiragana_part << "\" in dict\n");
        continue;  // Skip - hiragana_part is a dictionary suffix
      }

      // Check for compound adjective pattern: み + やすい/にくい/がたい
      // E.g., 読みやすい, 使いにくい - these are verb renyokei + auxiliary adjective
      // NOT kanji stem + み nominalization
      if (pattern == "み" && verb_helpers::isCompoundAdjectivePattern(hiragana_part)) {
        SUZUME_DEBUG_VERBOSE_BLOCK {
          // Extract the compound suffix for detailed logging
          const char* compound_suffix = "compound";
          if (hiragana_part.find("やすい") != std::string::npos || hiragana_part.find("やすく") != std::string::npos) {
            compound_suffix = "やすい";
          } else if (hiragana_part.find("にくい") != std::string::npos ||
                     hiragana_part.find("にくく") != std::string::npos) {
            compound_suffix = "にくい";
          } else if (hiragana_part.find("がたい") != std::string::npos ||
                     hiragana_part.find("がたく") != std::string::npos) {
            compound_suffix = "がたい";
          }
          SUZUME_DEBUG_STREAM << "[ADJ_STEM]   skip: compound adjective (contains \"" << compound_suffix << "\")\n";
        }
        continue;  // Skip - this is likely verb + やすい/にくい, not adjective + み
      }

      // For 1-char patterns (み, さ), skip if the hiragana portion starts with
      // a known dictionary word of 2+ chars. This prevents splitting known words.
      // E.g., 像+みんな → みんな is PRON, so み is not nominalization suffix
      // A registered particle opening right after the pattern keeps it, though:
      // there the pattern closes a word and the longer entry only spells
      // across that boundary (高さ+も, not さも).
      const bool particle_follows_pattern =
          dict_manager != nullptr && hiragana_part.size() > pattern.size() &&
          lookupEntryInRange(*dict_manager, codepoints, kanji_end + normalize::utf8Length(pattern),
                             kanji_end + normalize::utf8Length(pattern) + 1, core::PartOfSpeech::Particle) != nullptr;
      if (pattern.size() <= 3 && hiragana_part.size() > pattern.size() && dict_manager && !particle_follows_pattern) {
        auto hira_results = dict_manager->lookup(hiragana_part, 0);
        bool has_longer_dict_word = false;
        for (const auto& result : hira_results) {
          if (result.entry && result.entry->surface.size() > 3) {
            has_longer_dict_word = true;
            break;
          }
        }
        if (has_longer_dict_word) {
          SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   skip: hiragana starts with dict word\n");
          continue;
        }
      }

      // Found potential i-adjective stem + garu-connection pattern
      // The stem is just the kanji portion (e.g., 高, 尊, 寒)
      const std::string& stem = kanji_part;
      const std::string base_form = stem + "い";  // e.g., 高 → 高い

      // Validate that stem + い is a real i-adjective
      // Use lower threshold (0.35) for garu-connection patterns because:
      // - Single-kanji adjectives like 高い get lower confidence (0.42)
      // - The presence of すぎる/がる/さ strongly indicates adjective interpretation
      // A single-kanji stem is validated by inflection shape alone too easily: 上い
      // (conf 0.42) looks like an i-adjective but is really the godan verb stem of
      // 上がる. Require dictionary confirmation for single-kanji stems (real ones —
      // 寒い, 高い, 痛い — are all registered), while multi-kanji/extended stems
      // (恥ずかしい) keep the inflection path.
      const bool single_kanji_stem = (kanji_end - start_pos == 1);
      const bool in_dictionary = isAdjectiveInDictionary(dict_manager, base_form);
      float adj_confidence =
          (single_kanji_stem && !in_dictionary)
              ? candidate::kNoOriginConfidence
              : adj_detail::firstConfidenceAtLeast(inflection.analyze(base_form), grammar::VerbType::IAdjective,
                                                   candidate::kGaruAdjConfMin);

      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   base=\"" << base_form << "\" conf=" << adj_confidence << "\n");

      // Dictionary fallback: if inflection analysis gives low confidence but
      // the adjective exists in the dictionary, accept it.
      // E.g., 可愛い has conf=0 from inflection (all-kanji stem) but is in L2 dict.
      if (adj_confidence == candidate::kNoOriginConfidence) {
        if (!in_dictionary) {
          continue;
        }
        adj_confidence = candidate::kDictFallbackAdjConfidence;
        SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   dict fallback: \"" << base_form << "\" found in dictionary\n");
      }

      // Check for false positives: single-kanji stems that are also verb renyokei
      // E.g., 落ちすぎ could be 落ち(verb renyokei) + すぎ(verb)
      // We should prefer the verb renyokei interpretation if kanji+ちる/きる/etc. is a verb
      if (single_kanji_stem) {
        // Check if stem + る, stem + す, etc. forms a verb
        bool is_likely_verb_stem = false;
        for (const auto& suffix : {"ちる", "きる", "ぎる", "しる", "びる", "みる", "りる"}) {
          std::string verb_form = stem + suffix;
          if (isVerbInDictionary(dict_manager, verb_form)) {
            SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   skip: ichidan verb \"" << verb_form << "\" exists in dict\n");
            is_likely_verb_stem = true;
            break;
          }
        }
        if (is_likely_verb_stem) {
          continue;  // Skip - likely verb renyokei, not adjective stem
        }
      }

      // Skip adjective stem when the full kanji+hiragana surface is a known verb
      // E.g., 下さい(=ください) is a verb, not adjective stem 下 + nominalization さ + い
      std::string full_surface = extractSubstring(codepoints, start_pos, hiragana_end);
      if (isVerbInDictionary(dict_manager, full_surface)) {
        SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   skip: full surface \"" << full_surface << "\" is dict verb\n");
        continue;
      }

      // Low cost to compete with single-token verb path (高すぎる as VERB/ADJ)
      // Use strong negative cost to prefer ADJ_stem + すぎる split over compound
      // Need: stem + connection(0.5) + すぎる(0.4) < compound(0.35)
      // Required: stem < 0.35 - 0.5 - 0.4 = -0.55
      float cost =
          candidate::confidenceScaledCost(candidate::kAdjStemBaseCost, adj_confidence, candidate::kAdjStemConfScale);
      SUZUME_DEBUG_LOG("[ADJ_STEM]   ✓ candidate stem=\"" << stem << "\" cost=" << cost << "\n");
      candidates.push_back(makeIAdjStemCandidate(stem, start_pos, kanji_end, base_form, cost,
                                                 CandidateOrigin::AdjectiveI, adj_confidence, "adj_stem_garu_conn"));
      // Don't break - allow multiple patterns to generate candidates
    }
  }

  // =============================================================================
  // Pattern 1b: Extended adjective stem + garu-connection
  // =============================================================================
  // For adjectives like 恥ずかしい where the stem has extended okurigana.
  // E.g., 恥ずかしがってる → 恥ずかし (ADJ stem) + がっ + てる
  // E.g., 恥ずかしすぎる → 恥ずかし (ADJ stem) + すぎる
  //
  // Scan hiragana_part for garu patterns at non-zero positions.
  // If kanji + hiragana_prefix + い is a dict adjective, generate stem candidate.
  if (hiragana_part.size() >= 6) {  // Need at least 2 hiragana chars (prefix + pattern)
    // Search at each hiragana boundary and query the canonical follower
    // paradigms there. This includes the AuxGaru mizenkei がら.
    bool found_ext_garu = false;
    for (size_t byte_pos = 3; byte_pos < hiragana_part.size() && !found_ext_garu; byte_pos += 3) {
      for (const std::string_view pattern : iAdjectiveStemFollowers(hiragana_part, byte_pos, dict_manager)) {
        if (utf8::startsWith(std::string_view(hiragana_part).substr(byte_pos), pattern)) {
          // Found pattern at byte_pos within hiragana_part
          const std::string ext_okurigana = hiragana_part.substr(0, byte_pos);
          const std::string stem = kanji_part + ext_okurigana;
          const std::string base_form = stem + "い";

          // A productive kanji suffix followed by the independent Sahen
          // continuative has a complete nominal analysis (簡素+化+し+すぎ).
          // Do not reinterpret that suffix plus し as an unattested extended
          // i-adjective stem (化しい).  Require actual material to the left of
          // the suffix, either in this kanji span or immediately before it.
          bool has_nominal_sahen_suffix_boundary = false;
          if (ext_okurigana == "し") {
            for (const auto& suffix_entry : getSuffixEntries()) {
              if (!utf8::endsWith(kanji_part, suffix_entry.suffix)) {
                continue;
              }
              const bool has_local_base = kanji_part.size() > suffix_entry.suffix.size();
              const bool has_left_kanji = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]);
              if (has_local_base || has_left_kanji) {
                has_nominal_sahen_suffix_boundary = true;
                break;
              }
            }
          }
          // A multi-kanji stem before し is a サ変 verbal noun plus its
          // continuative (確認+し+すぎる), not an adjective stem. Only an
          // attested い-adjective of that spelling keeps the stem reading
          // (美味し+すぎる).
          const bool sahen_sized_stem = normalize::utf8Length(kanji_part) >= 2 ||
                                        (start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]));
          if (ext_okurigana == "し" && sahen_sized_stem && !isAdjectiveInDictionary(dict_manager, base_form)) {
            has_nominal_sahen_suffix_boundary = true;
          }
          if (has_nominal_sahen_suffix_boundary) {
            continue;
          }
          // A multi-mora okurigana that is itself an auxiliary cell is that
          // auxiliary on a compound noun (子供+らし+げ, as 子供+らし+さ), unless the
          // whole adjective is attested (男らしい). A single kanji is left alone:
          // it is usually a bound stem (誇らし+げ).
          if (ext_okurigana.size() >= core::kTwoJapaneseCharBytes && normalize::utf8Length(kanji_part) >= 2 &&
              dict_manager != nullptr &&
              dict_manager->lookupExact(ext_okurigana, core::PartOfSpeech::Auxiliary) != nullptr &&
              !isAdjectiveInDictionary(dict_manager, base_form)) {
            continue;
          }

          // A verb continuative plus a closed continuation is a predicate chain,
          // not a stem of an unlisted adjective (来+やがる, 食べ+やがる).
          if (!isAdjectiveInDictionary(dict_manager, base_form) &&
              verb_helpers::startsWithVerbContinuative(dict_manager, normalize::toCodepoints(stem), 0,
                                                       normalize::utf8Length(stem))) {
            continue;
          }

          if (pattern == "さ") {
            if (!isPossibleUnknownIAdjectiveStem(stem, base_form, dict_manager) ||
                hasNaAdjectiveStemEvidence(stem, dict_manager) ||
                hasInternalNominalDerivationalBoundary(stem, dict_manager)) {
              continue;
            }
            // The nominalizer derives a noun, and a noun does not host the
            // passive: a さ that れ follows is the irrealis of a godan-sa verb
            // instead (励ま+さ+れ+ぬ, 心動か+さ+れ+ぬ). Single-kanji stems never
            // reached this branch, which is why only the two-mora okurigana
            // spellings broke.
            const size_t nominalizer_end = kanji_end + (byte_pos / core::kJapaneseCharBytes) + 1;
            if (nominalizer_end < codepoints.size() && codepoints[nominalizer_end] == U'れ') {
              continue;
            }
            // The さ must be the nominalizer, not the first mora of a longer
            // closed class beginning with it (飲む+さかい).
            // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
            if (dict_manager != nullptr && hiragana_part.size() > byte_pos + pattern.size()) {
              const std::string_view closed_tail = std::string_view(hiragana_part).substr(byte_pos);
              if (dict_manager->lookupExact(closed_tail, core::PartOfSpeech::Particle) != nullptr) {
                continue;
              }
            }
          }
          if (pattern == "そう" && dict_manager != nullptr) {
            const auto* adjective = dict_manager->lookupExact(stem, core::PartOfSpeech::Adjective);
            const bool is_complete_na_adjective =
                adjective != nullptr && adjective->extended_pos == core::ExtendedPOS::AdjNaAdj;
            // A stem closed by る is a verb terminal before hearsay そう (食べる+そう)
            // unless the adjective it builds is attested (明る+そう).
            const bool verb_terminal_shape =
                utf8::endsWith(stem, "る") && !isAdjectiveInDictionary(dict_manager, base_form);
            // そ+う is also the volitional of a sa-row derivative, one kanji plus
            // an a-row irrealis (伸ば+そ+う), so an unlisted adjective of that
            // shape needs its own evidence there.
            const auto& sou_analyses = inflection.analyze(normalize::concat(stem, pattern));
            const bool reads_as_sa_row_volitional =
                !isAdjectiveInDictionary(dict_manager, base_form) && normalize::utf8Length(stem) == 2 &&
                normalize::isKanjiCodepoint(utf8::decodeFirstChar(stem)) &&
                kana::isARowCodepoint(utf8::decodeLastChar(stem)) &&
                std::any_of(sou_analyses.begin(), sou_analyses.end(), [&](const auto& analysis) {
                  return analysis.verb_type == grammar::VerbType::GodanSa &&
                         analysis.base_form == normalize::concat(stem, "す");
                });
            if (verb_terminal_shape || reads_as_sa_row_volitional || isVerbInDictionary(dict_manager, stem) ||
                is_complete_na_adjective || hasVerifiedPredicateDerivedAdjective(base_form, inflection, dict_manager) ||
                verb_helpers::startsWithVerbContinuative(dict_manager, normalize::toCodepoints(stem), 0,
                                                         normalize::utf8Length(stem))) {
              continue;
            }
          }

          const float adjective_confidence =
              productiveIAdjectiveStemConfidence(stem, base_form, inflection, dict_manager);
          const bool is_verified_adjective = adjective_confidence != candidate::kNoOriginConfidence;
          if (is_verified_adjective) {
            // Count hiragana chars in okurigana for stem_end calculation
            const size_t stem_end = kanji_end + byte_pos / 3;

            // Nor may it end on a terminal auxiliary and a conjunctive particle
            // (食べ+たい+し+さ): that is a predicate chain.
            if (!isAdjectiveInDictionary(dict_manager, base_form) &&
                verb_helpers::closesOnTerminalAuxiliaryAndConjunctive(dict_manager, codepoints, stem_end)) {
              continue;
            }
            // The okurigana scan runs past a case particle and reaches the next
            // word's kana (水 + を + くみ read as the stem of the non-word 水をくい).
            // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
            if (verb_helpers::embedsCaseParticle(dict_manager, codepoints, start_pos, stem_end)) {
              continue;
            }
            if (!isAdjectiveInDictionary(dict_manager, base_form) &&
                opensAuxiliaryInsideStem(dict_manager, codepoints, start_pos, stem_end)) {
              continue;
            }

            const float cost = candidate::kAdjStemExtCost;
            SUZUME_DEBUG_LOG("[ADJ_STEM]   ✓ ext_garu candidate stem=\""
                             << stem << "\" base=\"" << base_form << "\" pattern=\"" << pattern << "\" cost=" << cost
                             << "\n");
            candidates.push_back(makeIAdjStemCandidate(stem, start_pos, stem_end, base_form, cost,
                                                       CandidateOrigin::AdjectiveI, adjective_confidence,
                                                       "adj_stem_ext_garu"));
            found_ext_garu = true;  // Found a match, skip remaining patterns
            break;
          }
        }
      }
    }
  }

  // Check for しそう, しすぎ patterns (adjective stem + auxiliary)
  // The stem ends with し, and is followed by そう/すぎる/etc.
  // E.g., 難しそう → 難し (stem) + そう
  // E.g., 美しすぎる → 美し (stem) + すぎる
  for (const std::string_view pattern : adj_detail::kIAdjStemAuxPatterns) {
    if (utf8::startsWith(hiragana_part, pattern)) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   shii pattern=\"" << pattern << "\" matched\n");

      // Found adjective stem + auxiliary pattern
      // The stem is: kanji + し
      const size_t stem_end = kanji_end + 1;  // kanji + し (one hiragana)

      const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
      const std::string base_form = stem + "い";  // e.g., 難し → 難しい

      // Validate that this looks like a real adjective
      const auto& adj_results = inflection.analyze(base_form);
      const float adj_confidence =
          adj_detail::firstConfidenceAtLeast(adj_results, grammar::VerbType::IAdjective, candidate::kIAdjConfMin);
      const bool is_valid_adjective = adj_confidence != 0.0F;

      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   base=\"" << base_form << "\" is_valid=" << is_valid_adjective
                                                      << " conf=" << adj_confidence << "\n");

      if (!is_valid_adjective) {
        continue;
      }

      // Also check that this is NOT a verb renyokei (話し from 話す)
      // by comparing adjective vs verb confidence
      // The verb form would be: kanji_stem + す (e.g., 話 + す = 話す)
      const std::string verb_form = kanji_part + "す";  // e.g., 話す (not 話しす)
      const auto& verb_results = inflection.analyze(verb_form);
      const float verb_confidence =
          adj_detail::maxConfidenceFor(verb_results, {grammar::VerbType::GodanSa, grammar::VerbType::Suru});

      // Check if the verb form (kanji + す) is in the dictionary
      // If it is, this is likely a verb renyokei, not an adjective stem
      // E.g., 話す is in dictionary → 話し is verb renyokei, not adjective
      // E.g., 難す is NOT in dictionary → 難し could be adjective stem
      const bool is_dict_verb = isVerbInDictionary(dict_manager, verb_form);
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   verb_form=\"" << verb_form << "\" is_dict_verb=" << is_dict_verb << "\n");
      if (is_dict_verb) {
        SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   skip: verb in dictionary\n");
        continue;  // Skip - this is a dictionary verb renyokei
      }

      // Check if the adjective form (kanji + し + い) is in the dictionary
      // If it is, we trust the dictionary entry over confidence comparison
      // E.g., 美味しい is in dictionary → 美味し is adjective stem (skip conf check)
      // E.g., 難しい is in dictionary → 難し is adjective stem (skip conf check)
      const bool is_dict_adjective = isAdjectiveInDictionary(dict_manager, base_form);
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   is_dict_adj=" << is_dict_adjective << "\n");

      // A single-kanji Xしい reconstruction is too permissive without lexical
      // evidence (化し+すぎ must not invent 化しい). Established adjectives
      // such as 難しい and 美しい are dictionary-verified and remain covered.
      if (!is_dict_adjective && normalize::utf8Length(kanji_part) == 1) {
        continue;
      }

      // Confidence-based fallback when adjective is not in dictionary
      // Only generate adjective stem if adjective confidence is SIGNIFICANTLY higher
      // than verb confidence. This prevents generating stems for verb renyokei
      // patterns like 話し (from 話す) where both get similar confidence.
      if (!is_dict_adjective) {
        float diff = adj_confidence - verb_confidence;
        SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   conf_diff=" << diff << " (adj=" << adj_confidence
                                                           << " verb=" << verb_confidence
                                                           << " threshold=" << candidate::kAdjVerbConfDiffMin << ")\n");
        if (diff < candidate::kAdjVerbConfDiffMin) {
          SUZUME_DEBUG_LOG_VERBOSE("[ADJ_STEM]   skip: conf_diff < threshold\n");
          continue;
        }
      }

      // Low cost to compete with VERB path and single-token conjugated forms
      // Dictionary adjectives get a strong bonus for the stem + auxiliary path.
      // (美味しそう → 美味し + そう)
      // Need stronger negative cost like garu-connection pattern
      float cost = is_dict_adjective ? candidate::kAdjStemDictionaryCost
                                     : candidate::confidenceScaledCost(candidate::kAdjStemBaseCost, adj_confidence,
                                                                       candidate::kAdjStemConfScale);
      SUZUME_DEBUG_LOG("[ADJ_STEM]   ✓ candidate stem=\"" << stem << "\" cost=" << cost << "\n");
      candidates.push_back(makeIAdjStemCandidate(stem, start_pos, stem_end, base_form, cost,
                                                 CandidateOrigin::AdjectiveI, adj_confidence, "adj_stem_shii"));
      break;  // Only one stem candidate per pattern
    }
  }

  // A dictionary i-adjective may attach the productive appearance suffix げ
  // directly to its kanji stem (心細い → 心細+げ).  Unlike the しい/さ paths
  // below, this construction has no hiragana from the adjective itself, so
  // reconstruct the base from the complete kanji run.  Dictionary validation
  // keeps unrelated noun+げ sequences on the ordinary nominal path.
  if (hiragana_part.size() >= core::kJapaneseCharBytes && utf8::startsWith(hiragana_part, "げ")) {
    const std::string base_form = kanji_part + "い";
    const bool starts_inside_kanji_run = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]);
    const float adjective_confidence =
        (hasNaAdjectiveStemEvidence(kanji_part, dict_manager) || scorer::startsWithNegationPrefix(kanji_part))
            ? candidate::kNoOriginConfidence
            : productiveIAdjectiveStemConfidence(kanji_part, base_form, inflection, dict_manager);
    if (!starts_inside_kanji_run && adjective_confidence != candidate::kNoOriginConfidence) {
      candidates.push_back(makeIAdjStemCandidate(kanji_part, start_pos, kanji_end, base_form,
                                                 candidate::kAdjStemDictionaryCost, CandidateOrigin::AdjectiveI,
                                                 adjective_confidence, "adj_stem_kanji_ge"));
    }
  }

  // =============================================================================
  // Pattern 4: Extended adjective stem (kanji + multi-char hiragana)
  // =============================================================================
  // For adjectives like 懐かしい where the okurigana extends beyond しい.
  // E.g., 懐かしアニメ → 懐かし (ADJ stem) + アニメ (NOUN)
  // E.g., 勇ましい → 勇まし (stem) used in adnominal form
  //
  // Check if kanji + full hiragana_part + い is a dictionary adjective.
  // Only applies when hiragana_part is 2+ chars (Pattern 2 handles 1-char "し").
  if (hiragana_part.size() >= 6) {  // 2+ hiragana chars (6+ bytes)
    const std::string stem = kanji_part + hiragana_part;
    const std::string base_form = stem + "い";
    if (isAdjectiveInDictionary(dict_manager, base_form)) {
      const float cost = candidate::kAdjStemExtCost;
      SUZUME_DEBUG_LOG("[ADJ_STEM]   ✓ ext_adj candidate stem=\"" << stem << "\" base=\"" << base_form
                                                                  << "\" cost=" << cost << "\n");
      candidates.push_back(makeIAdjStemCandidate(stem, start_pos, hiragana_end, base_form, cost,
                                                 CandidateOrigin::AdjectiveI, 1.0F, "adj_stem_ext_adj"));
    }
  }

  // The suffix げ forms a noun that the supplementary ない takes as its
  // subject, on an adjective stem (危なげ+ない) and on a kanji compound
  // (大人げ+ない) alike; elsewhere it stays stem+げ (寂し+げ+な). A compound
  // whose last kanji spells an ichidan verb with げ keeps that verb (引上げ).
  for (size_t ge_pos = kanji_end; ge_pos + 2 < hiragana_end; ++ge_pos) {
    if (codepoints[ge_pos] != U'げ' || !verb_helpers::naiNegativeFollowsAt(codepoints, ge_pos + 1)) {
      continue;
    }
    const std::string stem = extractSubstring(codepoints, start_pos, ge_pos);
    const bool kanji_compound_host = ge_pos == kanji_end && kanji_end >= start_pos + 2 &&
                                     !verb_helpers::isVerbInDictionary(dict_manager, stem + "げる") &&
                                     !verb_helpers::isVerbInDictionary(
                                         dict_manager, extractSubstring(codepoints, kanji_end - 1, kanji_end) + "げる");
    if (kanji_compound_host || isAdjectiveInDictionary(dict_manager, stem + "い")) {
      candidates.push_back(makeNounCandidate(stem + "げ", start_pos, ge_pos + 1, candidate::kAdjStemExtCost, false,
                                             CandidateOrigin::AdjectiveI, core::ExtendedPOS::Noun));
    }
    break;
  }
}

namespace {

bool isModernIAdjective(const std::string& lemma, const grammar::Inflection& inflection,
                        const dictionary::DictionaryManager* dict_manager) {
  if (isAdjectiveInDictionary(dict_manager, lemma)) {
    return true;
  }
  const float minimum_confidence =
      grammar::containsKanji(lemma) ? candidate::kCompoundAdjConfMin : candidate::kHiraAdjConfMin;
  return adj_detail::firstConfidenceAtLeast(inflection.analyze(lemma), grammar::VerbType::IAdjective,
                                            minimum_confidence) != candidate::kNoOriginConfidence;
}

// The modern base of a ku-paradigm stem that ends in け, when that base is a
// na-adjective spelling the same stem with か (静け -> 静か). Empty when no such
// adjective is registered, which is the ordinary case for a verb stem in け.
std::string classicalKuStemNaAdjectiveBase(const std::string& stem, const dictionary::DictionaryManager* dict_manager) {
  if (!utf8::endsWith(stem, "け")) {
    return {};
  }
  const std::string base = normalize::concat(utf8::dropLastChar(stem), "か");
  return isAdjectiveInDictionary(dict_manager, base) ? base : std::string{};
}

}  // namespace

void appendIAdjClassicalTerminalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                           size_t scan_end, const dictionary::DictionaryManager* dict_manager,
                                           std::vector<UnknownCandidate>& candidates) {
  for (size_t shi_pos = scan_start; shi_pos < scan_end; ++shi_pos) {
    if (shi_pos <= start_pos || codepoints[shi_pos] != U'し') {
      continue;
    }
    // The terminal closes its clause. Any hiragana behind it continues some
    // other paradigm — the modern adjective's own (美し+かった), the sahen
    // predicate's (確認し+て) — and that reading owns the mora. A prolonged
    // sound mark is the same continuation spelled colloquially: しー is how
    // the modern ending しい is written when the vowel is drawn out, so the
    // mora belongs to that adjective (おいしーー is おいしい elongated, not a
    // literary terminal followed by emphasis).
    // A quotation or the copula is the exception: each takes a complete clause
    // or a quoted phrase, and the terminal is exactly that (暗し+という,
    // 暗し+だ, 高し+と).
    const auto takes_closed_terminal = [&](size_t pos) {
      if (codepoints[pos] == core::hiragana::kTo) {
        return true;
      }
      constexpr size_t kQuoteProbe = 4;
      if (dict_manager != nullptr) {
        const size_t probe_end = std::min(codepoints.size(), pos + kQuoteProbe);
        for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, pos, probe_end)) {
          if (match.entry != nullptr && (match.entry->extended_pos == core::ExtendedPOS::ParticleQuote ||
                                         match.entry->extended_pos == core::ExtendedPOS::DeterminerQuotative)) {
            return true;
          }
        }
      }
      return verb_helpers::auxiliaryFollowsAt(dict_manager, codepoints, pos,
                                              [](const dictionary::DictionaryEntry& entry) {
                                                return entry.extended_pos == core::ExtendedPOS::AuxCopulaDa ||
                                                       entry.extended_pos == core::ExtendedPOS::AuxCopulaDesu;
                                              });
    };
    if (shi_pos + 1 < codepoints.size() &&
        ((normalize::classifyChar(codepoints[shi_pos + 1]) == normalize::CharType::Hiragana &&
          !takes_closed_terminal(shi_pos + 1)) ||
         normalize::isProlongedSoundMark(codepoints[shi_pos + 1]))) {
      continue;
    }
    const std::string stem = extractSubstring(codepoints, start_pos, shi_pos);
    const std::string surface = extractSubstring(codepoints, start_pos, shi_pos + 1);
    // A registered su-row base makes the same spelling that row's continuative
    // (話し from 話す), which is the incomparably more frequent reading.
    if (verb_helpers::isVerbInDictionary(dict_manager, stem + "す") ||
        verb_helpers::hasNonVerbDictionaryEntry(dict_manager, surface)) {
      continue;
    }
    // The ku paradigm spells its terminal by adding し to the stem the modern
    // base keeps (高い -> 高し); the shiku paradigm already ends in that mora and
    // spells the terminal with the stem itself (欲しい -> 欲し). Only a dictionary
    // base licenses the reading: the analyzer endorses an i-adjective shape for
    // any run ending in し, which would fabricate one per kanji run.
    std::string lemma = surface + "い";
    if (!verb_helpers::isAdjectiveInDictionary(dict_manager, lemma)) {
      lemma = stem + "い";
      if (!verb_helpers::isAdjectiveInDictionary(dict_manager, lemma)) {
        // Part of the ku paradigm survives into the modern language as a
        // na-adjective rather than an i-adjective, and there the classical stem
        // replaces that adjective's final か with け (静か -> 静け+し, 明らか ->
        // 明らけ+し). Probing the か spelling recovers the base the surface no
        // longer carries; without it the terminal falls back on an invented
        // ichidan verb over the same stem.
        lemma = classicalKuStemNaAdjectiveBase(stem, dict_manager);
        if (lemma.empty()) {
          continue;
        }
      }
    }
    candidates.push_back(makeIAdjCellCandidate(surface, start_pos, shi_pos + 1, lemma, core::ExtendedPOS::AdjBasic,
                                               candidate::verb_cost::kStrongBonus, CandidateOrigin::AdjectiveI,
                                               candidate::kIAdjKaroConfidence, "i_adjective_classical_shi"));
  }
}

void appendIAdjOnbinRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                       size_t scan_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates) {
  // The polite continuative replaces the く of an i-adjective's renyokei with
  // う, and the mora in front of it carries the glide that vowel change
  // produces: an i-row kana grows its ゅ digraph (よろしく → よろしゅう) while a
  // kanji stem keeps its spelling and only loses the く (高く → 高う). The cell
  // is missing from the paradigm, so the run gets cut at the glide instead.
  //
  // Only those two shapes are admitted. A bare kana stem plus う would accept
  // any two morae whose first plus い happens to be an adjective, which is what
  // the formal noun よう spells.
  for (size_t u_pos = scan_start; u_pos < scan_end; ++u_pos) {
    if (codepoints[u_pos] != U'う' || u_pos <= start_pos) {
      continue;
    }
    size_t stem_end = u_pos;
    bool glide_shape = false;
    if (codepoints[u_pos - 1] == U'ゅ') {
      if (u_pos < start_pos + 2 || !kana::isIRowCodepoint(codepoints[u_pos - 2])) {
        continue;
      }
      stem_end = u_pos - 1;
      glide_shape = true;
    } else if (!normalize::isKanjiCodepoint(codepoints[u_pos - 1])) {
      continue;
    }
    if (stem_end <= start_pos) {
      continue;
    }
    const std::string lemma = extractSubstring(codepoints, start_pos, stem_end) + "い";
    // The glide is itself the evidence: nothing else produces し+ゅ+う. Without
    // it the shape is just a kanji stem plus う, which every wa-row Godan
    // terminal also spells (思う, 使う), so that side needs the dictionary.
    if (glide_shape ? !isModernIAdjective(lemma, inflection, dict_manager)
                    : !isAdjectiveInDictionary(dict_manager, lemma)) {
      continue;
    }
    candidates.push_back(makeIAdjCellCandidate(extractSubstring(codepoints, start_pos, u_pos + 1), start_pos, u_pos + 1,
                                               lemma, core::ExtendedPOS::AdjRenyokei,
                                               candidate::verb_cost::kStrongBonus, CandidateOrigin::AdjectiveI,
                                               candidate::kIAdjKaroConfidence, "i_adjective_onbin_renyokei"));
  }
}

void appendIAdjKaroCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                              size_t scan_end, const grammar::Inflection& inflection,
                              const dictionary::DictionaryManager* dict_manager,
                              std::vector<UnknownCandidate>& candidates) {
  for (size_t karo_pos = scan_start; karo_pos + 1 < scan_end; ++karo_pos) {
    if (karo_pos <= start_pos) {
      continue;  // The stem before かろ must be non-empty
    }
    if (codepoints[karo_pos] != U'か' || codepoints[karo_pos + 1] != U'ろ') {
      continue;
    }
    // Require a following う (推量): Xかろ+う. Without う, Xかろ is far more likely
    // a verb form, so leave it to the verb candidate paths.
    if (karo_pos + 2 >= codepoints.size() || codepoints[karo_pos + 2] != U'う') {
      continue;
    }
    std::string lemma = extractSubstring(codepoints, start_pos, karo_pos) + "い";
    // ない is both the adjective 無い and the negative auxiliary; in the かろ form
    // (〜ではなかろうか) the auxiliary reading dominates, so leave なかろ to the
    // auxiliary path rather than tagging it Adjective.
    //
    // The same ambiguity survives one morpheme to the left: an irrealis stem
    // plus the auxiliary has exactly the shape of a lexical ない-adjective, and
    // the inflection analyzer scores 知らない like 少ない. Attestation is the
    // only thing that separates them, so a base ending in ない has to come from
    // the dictionary rather than from the analyzer's shape guess.
    if (lemma == "ない" || (utf8::endsWith(lemma, "ない") && !isAdjectiveInDictionary(dict_manager, lemma))) {
      continue;
    }
    if (adj_detail::hasDictionaryVerbAnalysis(analysesInRange(inflection, codepoints, start_pos, karo_pos + 3),
                                              dict_manager)) {
      continue;
    }
    // Decisive lexical signal: the reconstructed base is a dictionary adjective,
    // or the inflection analyzer recognizes it as an i-adjective. This rejects the
    // verb-volitional homograph (分かろう → 分か+い is not an adjective).
    if (!isModernIAdjective(lemma, inflection, dict_manager)) {
      continue;
    }
    // Verified adjective: make the 未然形 win over fake verb interpretations
    // (ichidan Xかる etc.), mirroring the ke-form handling. AdjMizenkei feeds the
    // AdjMizenkei→AuxVolitional bigram.
    candidates.push_back(makeIAdjCellCandidate(extractSubstring(codepoints, start_pos, karo_pos + 2), start_pos,
                                               karo_pos + 2, lemma, core::ExtendedPOS::AdjMizenkei,
                                               candidate::verb_cost::kStrongBonus, CandidateOrigin::AdjectiveI,
                                               candidate::kIAdjKaroConfidence, "i_adjective_karo"));
  }
}

namespace {

// The supplementary (カリ) conjugation follows the ラ変 pattern, so the kana
// after か identifies the cell by its vowel row. Anything else is not a カリ form.
core::ExtendedPOS classicalKariCell(char32_t after_ka) {
  switch (after_ka) {
    case U'ら':
      return core::ExtendedPOS::AdjMizenkei;
    case U'り':
      return core::ExtendedPOS::AdjRenyokei;
    // The attributive and terminal cells share one form for i-adjectives.
    // The paradigm has no 已然形 (the plain conjugation supplies けれ).
    case U'る':
      return core::ExtendedPOS::AdjBasic;
    // The 命令形 closes a clause exactly as the terminal cell does, so it shares
    // that ExtendedPOS. It needs a licensing environment of its own rather than
    // the classical auxiliary the other cells take, because かれ is
    // overwhelmingly a godan irrealis plus the passive れ (書か+れ).
    case U'れ':
      return core::ExtendedPOS::AdjBasic;
    default:
      return core::ExtendedPOS::Unknown;
  }
}

bool classicalConjunctiveFollowsAt(const std::vector<char32_t>& codepoints, size_t pos,
                                   const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kClassicalTailProbeChars = 3;
  return hasDictionaryEntryFrom(dict_manager, codepoints, pos, 1, kClassicalTailProbeChars,
                                core::PartOfSpeech::Particle, [](const dictionary::DictionaryEntry& entry) {
                                  // The optative/imperative cell can be followed by the quotative と.
                                  // Its dictionary entry is often labelled as a case particle even though
                                  // this construction is a clausal connective (高かれ+と願う).
                                  return entry.extended_pos == core::ExtendedPOS::ParticleConj ||
                                         entry.extended_pos == core::ExtendedPOS::ParticleQuote ||
                                         (entry.extended_pos == core::ExtendedPOS::ParticleCase &&
                                          grammar::isSingleHiragana(entry.surface, core::hiragana::kTo));
                                });
}

// The imperative cell also stands in the paired concessive, where two of them are
// juxtaposed with nothing in between (遅かれ早かれ, 多かれ少なかれ). Neither member
// carries the conjunctive particle that licenses the cell everywhere else: the
// first hands the clause straight to its partner and the second to the predicate,
// so what identifies the construction is the adjacency itself. Each member
// therefore looks for its partner on the side the other one is on. Both halves
// still have to pass the adjective evidence the cell needs on its own, which is
// what keeps an ordinary verb ending in the same two kana out (道が分かれ、).
bool pairedImperativeCellAdjacent(const std::vector<char32_t>& codepoints, size_t start_pos, size_t cell_end) {
  // The partner's stem is a word of its own, so a few codepoints reach its cell.
  constexpr size_t kPairedStemProbeChars = 4;
  if (start_pos >= 2 && codepoints[start_pos - 2] == U'か' && codepoints[start_pos - 1] == U'れ') {
    return true;
  }
  const size_t probe_end = std::min(codepoints.size(), cell_end + kPairedStemProbeChars);
  for (size_t pos = cell_end; pos + 1 < probe_end; ++pos) {
    if (codepoints[pos] == U'か' && codepoints[pos + 1] == U'れ') {
      return true;
    }
  }
  return false;
}

bool classicalClauseEndsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return true;
  }
  switch (codepoints[pos]) {
    case U'。':
    case U'、':
    case U'！':
    case U'？':
    case U'」':
      return true;
    default:
      return false;
  }
}

// The supplementary conjugation exists only to carry the classical auxiliaries
// the plain paradigm cannot take, so require one to start at the given position.
// Gating on the classical auxiliary class rather than on auxiliaries in general
// keeps the colloquial homographs out (や, registered as a copula variant, must
// not license 最初から as an adjective). Probes the longest form so multi-kana
// auxiliaries (けり, べし) count alongside single-kana ones (ず, き).
bool classicalAuxiliaryFollowsAt(const std::vector<char32_t>& codepoints, size_t pos, size_t scan_end,
                                 const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  // A spelling may carry more than one auxiliary registration, and the classical
  // one is not always the first: the counterfactual's せ shares its mora with the
  // causative. The question here is whether any auxiliary of the classical class
  // starts at this position, so the probe walks every entry the span opens rather
  // than the single one an exact lookup answers with.
  const size_t probe_end = std::min({codepoints.size(), scan_end + 1, pos + 3});
  for (const auto& result : lookupResultsInRange(*dict_manager, codepoints, pos, probe_end)) {
    const auto* entry = result.entry;
    if (entry == nullptr || entry->pos != core::PartOfSpeech::Auxiliary) {
      continue;
    }
    // The negative conjecture takes a terminal exactly as the affirmative one
    // does, so the cell it selects is the same (高かる+まじ next to 高かる+らむ).
    if (core::isClassicalAuxiliaryType(entry->extended_pos) ||
        entry->extended_pos == core::ExtendedPOS::AuxNegativeMai) {
      return true;
    }
  }
  return false;
}

}  // namespace

void appendIAdjKaraZuCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                size_t scan_end, const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager,
                                std::vector<UnknownCandidate>& candidates) {
  for (size_t kara_pos = scan_start; kara_pos + 1 < scan_end; ++kara_pos) {
    if (kara_pos <= start_pos || codepoints[kara_pos] != U'か') {
      continue;
    }
    // The supplementary (カリ) conjugation of an i-adjective inflects on the ラ変
    // pattern: 未然 から, 連用 かり, 連体 かる, 已然/命令 かれ. Select the cell from
    // the row of the kana after か instead of enumerating the forms.
    const core::ExtendedPOS cell = classicalKariCell(codepoints[kara_pos + 1]);
    if (cell == core::ExtendedPOS::Unknown) {
      continue;
    }
    // Only a closed-class follower licenses the supplementary conjugation; it
    // exists precisely to carry auxiliaries the plain paradigm cannot
    // (大きから+ず, 高かり+けり, 冷たかる+べし). Without one, the same kana are an
    // ordinary noun or godan verb (明かり, 見つかる).
    //
    const size_t cell_end = kara_pos + 2;
    // The 已然/命令 cell is selected by a conjunctive particle, including the
    // concessive ど as well as the optative quotative と.  Either continuation
    // rules out the homographic passive; without a closed particle the cell is
    // not emitted. The continuative かり may also close a literary clause.
    const bool is_kare = codepoints[kara_pos + 1] == U'れ';
    const bool follows_conjunctive = is_kare && (classicalConjunctiveFollowsAt(codepoints, cell_end, dict_manager) ||
                                                 pairedImperativeCellAdjacent(codepoints, start_pos, cell_end));
    const bool terminal_renyokei = codepoints[kara_pos + 1] == U'り' && classicalClauseEndsAt(codepoints, cell_end);
    const bool licensed = follows_conjunctive || terminal_renyokei ||
                          (!is_kare && classicalAuxiliaryFollowsAt(codepoints, cell_end, scan_end, dict_manager));
    if (!licensed) {
      continue;
    }
    std::string lemma = extractSubstring(codepoints, start_pos, kara_pos) + "い";
    // A シク adjective carries its し in the stem the supplementary conjugation
    // attaches to, and the modern base keeps that mora only sometimes
    // (美し+から -> 美しい, 悪し+から -> 悪い). Fall back to the stem without it,
    // the same two-step probe the classical terminal uses.
    if (!isAdjectiveInDictionary(dict_manager, lemma) && codepoints[kara_pos - 1] == U'し' &&
        kara_pos - 1 > start_pos) {
      const std::string shiku_lemma = extractSubstring(codepoints, start_pos, kara_pos - 1) + "い";
      if (isAdjectiveInDictionary(dict_manager, shiku_lemma)) {
        lemma = shiku_lemma;
      }
    }
    // The かれ cell is also the passive auxiliary after a Godan-ka irrealis
    // (書か+れ+ども).  The inflection engine intentionally recognizes broad
    // i-adjective-shaped runs, which is not enough to distinguish that path.
    // A classical カリ reading therefore needs adjective evidence; genuine
    // bases such as 美しい、高い、多い are L2-backed while the passive remains a
    // regular productive verb chain. The open シク class cannot be listed
    // exhaustively, so a productively formed -しい terminal counts as the same
    // evidence: its し is the stem mora the supplementary conjugation attaches
    // to, which the passive's a-row irrealis can never supply.
    // The requirement holds for the other cells too, even though a classical
    // auxiliary already licenses them: accepting a merely well-formed base there
    // lets the run start one word early and coin a compound out of whatever
    // stands in front of it (山+高かり as 山高い, 言ふ+べかり as 言ふべい), which is
    // the compound-stem ambiguity the lexicon is what settles.
    if (!isAdjectiveInDictionary(dict_manager, lemma) &&
        !verb_helpers::isProductiveShiiAdjectiveTerminal(lemma, inflection)) {
      continue;
    }
    // A productive stem stays inside one word: a conjunctive て/で after a kana
    // closes a te-form clause, and what follows it starts the next predicate
    // (実行され+て+しかる+べき, not a coined 実行されてしい).
    if (!isAdjectiveInDictionary(dict_manager, lemma)) {
      bool embeds_te_form = false;
      for (size_t te_pos = start_pos + 1; te_pos < kara_pos && !embeds_te_form; ++te_pos) {
        embeds_te_form = (codepoints[te_pos] == core::hiragana::kTe || codepoints[te_pos] == U'で') &&
                         kana::isHiraganaCodepoint(codepoints[te_pos - 1]);
      }
      if (embeds_te_form) {
        continue;
      }
    }
    // A カリ form is also an ordinary godan-ra inflection plus a classical
    // auxiliary. A complete analysis whose reconstructed verb lemma is
    // dictionary-attested is stronger than the weak, generic i-adjective
    // hypothesis (分からぬ -> 分かる, not fictitious 分い).
    // The other cells need their auxiliary inside the probe for the verb
    // analysis to complete (分から+ぬ -> 分かる). The 命令形 is followed by a
    // particle instead, which no verb analysis spans, so the probe stops at the
    // cell itself — that is what still recognizes 分かれ as 分かれる before と.
    const size_t probe_end = is_kare ? cell_end : std::min(kara_pos + 3, scan_end);
    if (adj_detail::hasDictionaryVerbAnalysis(analysesInRange(inflection, codepoints, start_pos, probe_end),
                                              dict_manager)) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, kara_pos + 2);
    if (dict_manager != nullptr && dict_manager->lookupExact(surface, core::PartOfSpeech::Auxiliary) != nullptr) {
      continue;
    }
    candidates.push_back(makeIAdjCellCandidate(surface, start_pos, kara_pos + 2, lemma, cell,
                                               candidate::verb_cost::kStrongBonus, CandidateOrigin::AdjectiveI,
                                               candidate::kIAdjKaroConfidence, "i_adjective_kari"));
  }
}

}  // namespace suzume::analysis
