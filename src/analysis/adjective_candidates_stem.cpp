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
#include "grammar/honorific_verbs.h"
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
  // A bound derivational suffix verb takes the adjective stem as its host
  // (偉+ぶる, 偉+ぶっ+た) just as it takes a noun; any of its cells opens it.
  for (const size_t cell_chars : {2U, 3U}) {
    const size_t cell_bytes = cell_chars * core::kJapaneseCharBytes;
    const std::string_view cell = remaining.substr(0, cell_bytes);
    // A cell that is also a nominal suffix (久し+ぶり) stays with the suffix reading.
    if (remaining.size() >= cell_bytes && grammar::spellsBoundDerivationalSuffixCell(cell) &&
        !verb_helpers::hasDictionaryEntry(dict_manager, cell, core::PartOfSpeech::Suffix)) {
      followers.push_back(cell);
    }
  }
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
          const bool base_is_dict_adjective = isAdjectiveInDictionary(dict_manager, base_form);

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
          if (ext_okurigana == "し" && sahen_sized_stem && !base_is_dict_adjective) {
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
              !base_is_dict_adjective) {
            continue;
          }

          // A verb continuative plus a closed continuation is a predicate chain,
          // not a stem of an unlisted adjective (来+やがる, 食べ+やがる).
          if (!base_is_dict_adjective &&
              verb_helpers::startsWithVerbContinuative(dict_manager, normalize::toCodepoints(stem), 0,
                                                       normalize::utf8Length(stem))) {
            continue;
          }

          // A pattern opening on し whose stem plus しい is a listed adjective
          // has taken the し from the adjective's own ending (恥ずか+しそう is
          // 恥ずかし+そう, the stem of 恥ずかしい).
          if (utf8::startsWith(pattern, "し") && !base_is_dict_adjective &&
              isAdjectiveInDictionary(dict_manager, normalize::concat(stem, "しい"))) {
            continue;
          }

          // A single mora other than the shiku し that sits between a kanji and
          // what follows is a particle of the phrase, not okurigana of an
          // unattested adjective (花+や+さくら, not the stem of 花やい).
          if (ext_okurigana.size() == core::kJapaneseCharBytes && ext_okurigana != "し" && !base_is_dict_adjective) {
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
            const bool verb_terminal_shape = utf8::endsWith(stem, "る") && !base_is_dict_adjective;
            // そ+う is also the volitional of a sa-row derivative, one kanji plus
            // an a-row irrealis (伸ば+そ+う), so an unlisted adjective of that
            // shape needs its own evidence there.
            const auto& sou_analyses = inflection.analyze(normalize::concat(stem, pattern));
            const bool reads_as_sa_row_volitional =
                !base_is_dict_adjective && normalize::utf8Length(stem) == 2 &&
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
            if (!base_is_dict_adjective &&
                verb_helpers::closesOnTerminalAuxiliaryAndConjunctive(dict_manager, codepoints, stem_end)) {
              continue;
            }
            // The okurigana scan runs past a case particle and reaches the next
            // word's kana (水 + を + くみ read as the stem of the non-word 水をくい).
            // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
            if (verb_helpers::embedsCaseParticle(dict_manager, codepoints, start_pos, stem_end,
                                                 /*include_genitive_and_wa=*/true)) {
              continue;
            }
            if (!base_is_dict_adjective && opensAuxiliaryInsideStem(dict_manager, codepoints, start_pos, stem_end)) {
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

}  // namespace suzume::analysis
