/**
 * @file verb_candidates_kanji.cpp
 * @brief Kanji-based verb candidate generation (generateVerbCandidates)
 *
 * Handles verb candidate generation for kanji+hiragana patterns.
 * Split from verb_candidates.cpp for maintainability.
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
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis {

namespace vh = verb_helpers;
using namespace kanji_verb_detail;

namespace {

bool hasNiSugiNegativeTail(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos + 3 < codepoints.size() && codepoints[pos] == U'に' && codepoints[pos + 1] == U'す' &&
         codepoints[pos + 2] == U'ぎ' && vh::naiNegativeFollowsAt(codepoints, pos + 3);
}

bool isVerifiedFiniteVerb(const dictionary::DictionaryManager* dict_manager, const grammar::Inflection& inflection,
                          const grammar::InflectionCandidate& candidate) {
  if (vh::isVerbInDictionary(dict_manager, candidate.base_form)) {
    return true;
  }
  if (candidate.verb_type == grammar::VerbType::Ichidan) {
    return vh::isVerifiedVerbBase(dict_manager, inflection, candidate.base_form,
                                  candidate::verb_cost::kConstructedVerbMinConfidence, false);
  }
  return grammar::isGodanVerbType(candidate.verb_type) &&
         vh::isVerifiedVerbBase(dict_manager, inflection, candidate.base_form,
                                candidate::verb_cost::kConstructedVerbMinConfidence, true);
}

// A multi-okurigana span before the excessive auxiliary is not sufficient
// evidence for one verb when it decomposes into an i-adjective renyokei and a
// complete following predicate (高く+なり+すぎる).  Both sides must be
// independently licensed, and an attested full verb lemma always wins, so
// native lexical verbs containing the same kana are unaffected.
bool hasAdjectiveRenyokeiPredicateBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                           const grammar::Inflection& inflection,
                                           const dictionary::DictionaryManager* dict_manager,
                                           std::string_view full_base_form) {
  if (dict_manager == nullptr || end_pos <= start_pos + 2 ||
      dict_manager->lookupExact(full_base_form, core::PartOfSpeech::Verb) != nullptr) {
    return false;
  }

  for (size_t predicate_start = start_pos + 2; predicate_start < end_pos; ++predicate_start) {
    if (codepoints[predicate_start - 1] != U'く') {
      continue;
    }

    const std::string adjective_surface = extractSubstring(codepoints, start_pos, predicate_start);
    bool adjective_verified = false;
    for (const auto& analysis : inflection.analyze(adjective_surface)) {
      if (analysis.verb_type != grammar::VerbType::IAdjective) {
        continue;
      }
      const bool dictionary_base =
          dict_manager->lookupExact(analysis.base_form, core::PartOfSpeech::Adjective) != nullptr;
      if (dictionary_base || analysis.confidence >= candidate::kIAdjConfMin) {
        adjective_verified = true;
        break;
      }
    }
    if (!adjective_verified) {
      continue;
    }

    const std::string predicate_surface = extractSubstring(codepoints, predicate_start, end_pos);
    if (grammar::isSuruRenyokeiSurface(predicate_surface)) {
      return true;
    }
    const char32_t predicate_ending = codepoints[end_pos - 1];
    if (grammar::isIRowCodepoint(predicate_ending)) {
      const std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(predicate_ending);
      const std::string predicate_base = normalize::concat(utf8::dropLastChar(predicate_surface), base_suffix);
      if (vh::isVerifiedVerbBase(dict_manager, inflection, predicate_base,
                                 candidate::verb_cost::kConstructedVerbMinConfidence, true)) {
        return true;
      }
    } else if (grammar::isERowCodepoint(predicate_ending)) {
      if (vh::isVerifiedVerbBase(dict_manager, inflection, predicate_surface + "る",
                                 candidate::verb_cost::kConstructedVerbMinConfidence, false)) {
        return true;
      }
    }
  }
  return false;
}

// A finite predicate plus a case particle remains two morphemes before the
// excessive construction (読む+に+過ぎない).  The final に must not be reused
// as the i-row ending of a fabricated multi-okurigana verb (読むぬ).  Native
// continuatives such as 死に+すぎる have no complete predicate to the left of
// the final mora and therefore remain eligible.
bool hasFinitePredicateCaseParticleTail(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                        const grammar::Inflection& inflection,
                                        const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1) {
    return false;
  }
  const auto* particle =
      lookupEntryInRange(*dict_manager, codepoints, end_pos - 1, end_pos, core::PartOfSpeech::Particle);
  if (particle == nullptr || particle->extended_pos != core::ExtendedPOS::ParticleCase) {
    return false;
  }

  const std::string predicate_surface = extractSubstring(codepoints, start_pos, end_pos - 1);
  if (dict_manager->lookupExact(predicate_surface, core::PartOfSpeech::Verb) != nullptr) {
    return true;
  }
  for (const auto& analysis : inflection.analyze(predicate_surface)) {
    if (isVerifiedFiniteVerb(dict_manager, inflection, analysis)) {
      return true;
    }
  }
  const auto predicate_codepoints = normalize::toCodepoints(predicate_surface);
  return predicate_codepoints.size() >= 3 && predicate_codepoints.back() == U'る' &&
         grammar::isERowCodepoint(predicate_codepoints[predicate_codepoints.size() - 2]);
}

bool hasNominalizedNounParticleContinuation(const std::vector<char32_t>& codepoints, size_t end_pos,
                                            const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos >= codepoints.size() || codepoints[end_pos] == U'て' ||
      codepoints[end_pos] == U'で') {
    return false;
  }

  return hasNominalForcingParticleContinuation(codepoints, end_pos, dict_manager);
}

bool hasDictionaryAdjectiveTail(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return false;
  }
  return hasDictionaryEntryEndingAt(*dict_manager, codepoints, start_pos + 1, end_pos,
                                    partOfSpeechMask(core::PartOfSpeech::Adjective));
}

// A particle-like first okurigana mora is normally a reliable noun boundary
// (本+の, 紙+を).  It must not terminate candidate generation, however, when
// the complete hiragana run independently proves a verb whose stem includes
// that mora (遠のく, 悔やんだ).  Require the whole run and the exact
// kanji+one-mora stem so shorter particle-delimited prefixes cannot license a
// fabricated verb.
bool hasCompleteParticleInitialVerbEvidence(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                            size_t hiragana_end, const grammar::Inflection& inflection,
                                            const dictionary::DictionaryManager* dict_manager,
                                            const VerbCandidateOptions& verb_opts) {
  if (hiragana_end <= kanji_end + 1) {
    return false;
  }

  const std::string surface = extractSubstring(codepoints, start_pos, hiragana_end);
  const std::string expected_stem = extractSubstring(codepoints, start_pos, kanji_end + 1);
  const bool has_conjunctive_initial =
      vh::hasConjunctiveParticleDictionaryEntry(dict_manager, normalize::encodeUtf8(codepoints[kanji_end]));
  for (const auto& candidate : inflection.analyze(surface)) {
    const bool has_mixed_godan_ka_stem = has_conjunctive_initial && candidate.verb_type == grammar::VerbType::GodanKa &&
                                         utf8::startsWith(candidate.stem, expected_stem) &&
                                         normalize::utf8Length(candidate.stem) > normalize::utf8Length(expected_stem);
    // A complete negative form can prove a longer Godan mizenkei whose first
    // okurigana mora is particle-homographic (懐かしま+なかった). Require the
    // observed suffix to contain the row-correct mizenkei ending followed by
    // the closed ない paradigm; this does not admit adjective/Sahen stems such
    // as 冷たく+なかった or 食べたく+なかった.
    if (grammar::isGodanVerbType(candidate.verb_type) && utf8::startsWith(candidate.stem, expected_stem) &&
        candidate.stem != expected_stem && candidate.confidence > verb_opts.confidence_standard &&
        candidate.suffix.size() > core::kJapaneseCharBytes) {
      const char32_t mizenkei_ending = utf8::decodeFirstChar(candidate.suffix);
      const std::string_view negative_suffix(candidate.suffix.data() + core::kJapaneseCharBytes,
                                             candidate.suffix.size() - core::kJapaneseCharBytes);
      if (grammar::verbTypeFromARowCodepoint(mizenkei_ending) == candidate.verb_type &&
          (utf8::startsWith(negative_suffix, "ない") || utf8::startsWith(negative_suffix, "なかった") ||
           utf8::startsWith(negative_suffix, "なく") || utf8::startsWith(negative_suffix, "なけれ"))) {
        return true;
      }
    }
    const float min_confidence = has_mixed_godan_ka_stem ? (utf8::startsWith(candidate.suffix, "いた") ||
                                                                    utf8::startsWith(candidate.suffix, "いて")
                                                                ? verb_opts.confidence_past_te
                                                                : verb_opts.confidence_low)
                                                         : verb_opts.confidence_standard;
    if (candidate.verb_type == grammar::VerbType::IAdjective ||
        (candidate.stem != expected_stem && !has_mixed_godan_ka_stem) || candidate.confidence <= min_confidence) {
      continue;
    }
    const bool complete_terminal = candidate.base_form == surface;
    bool complete_inflection = false;
    switch (candidate.verb_type) {
      case grammar::VerbType::GodanKa:
        complete_inflection = utf8::startsWith(candidate.suffix, "いて") ||
                              utf8::startsWith(candidate.suffix, "いた") ||
                              utf8::startsWith(candidate.suffix, "きます");
        break;
      case grammar::VerbType::GodanGa:
        complete_inflection = utf8::startsWith(candidate.suffix, "いで") || utf8::startsWith(candidate.suffix, "いだ");
        break;
      case grammar::VerbType::GodanTa:
      case grammar::VerbType::GodanRa:
      case grammar::VerbType::GodanWa:
        complete_inflection = utf8::startsWith(candidate.suffix, "って") || utf8::startsWith(candidate.suffix, "った");
        break;
      case grammar::VerbType::GodanNa:
      case grammar::VerbType::GodanBa:
      case grammar::VerbType::GodanMa:
        complete_inflection = utf8::startsWith(candidate.suffix, "んで") || utf8::startsWith(candidate.suffix, "んだ");
        break;
      case grammar::VerbType::GodanSa:
        complete_inflection = utf8::startsWith(candidate.suffix, "して") || utf8::startsWith(candidate.suffix, "した");
        break;
      default:
        break;
    }
    if (complete_terminal || complete_inflection) {
      return true;
    }
  }
  return false;
}

void appendNiSugiPredicateCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  // The limiting construction V終止形+に+すぎない keeps the finite predicate
  // intact (見るにすぎない, 遅れるにすぎない). It is distinct from the
  // excessive construction V連用形+すぎる, so recognize its particle-delimited
  // tail before the latter's renyokei-specific path is considered.
  for (size_t tail_pos = start_pos + 1; tail_pos < hiragana_end; ++tail_pos) {
    if (!hasNiSugiNegativeTail(codepoints, tail_pos)) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, tail_pos);
    for (const auto& inflected : inflection.analyze(surface)) {
      if (inflected.base_form != surface || inflected.verb_type == grammar::VerbType::IAdjective ||
          !isVerifiedFiniteVerb(dict_manager, inflection, inflected)) {
        continue;
      }
      candidates.push_back(makeVerbCandidate(surface, start_pos, tail_pos, candidate::verb_cost::kStrongBonus,
                                             inflected.base_form, grammar::verbTypeToConjType(inflected.verb_type),
                                             true, CandidateOrigin::VerbKanji, inflected.confidence, "finite_ni_sugi",
                                             core::ExtendedPOS::VerbShuushikei));
      return;
    }
  }
}

void appendNiLimitedIchidanCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates) {
  // In the limiting predicate Nに+V連用形+ない, a dictionary-verified
  // Ichidan renyokei must remain available over a homographic Godan reading
  // (本に過ぎない). The preceding case particle and following negative make
  // this a grammatical construction rather than a surface-specific exception.
  if (start_pos == 0 || codepoints[start_pos - 1] != U'に') {
    return;
  }
  for (size_t end_pos = start_pos + 1; end_pos < hiragana_end; ++end_pos) {
    if (!vh::naiNegativeFollowsAt(codepoints, end_pos)) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    // The bare renyokei can be ambiguous (過ぎ → 過ぐ), while its negative
    // continuation supplies the reliable Ichidan evidence (過ぎない → 過ぎる).
    // Analyze that full inflected form, then emit only its stem as the token.
    const std::string negative_surface = extractSubstring(codepoints, start_pos, hiragana_end);
    for (const auto& inflected : inflection.analyze(negative_surface)) {
      // Only a homograph can be resolved here, so the Ichidan base has to be an
      // attested lemma. Without that check the construction fabricates a verb
      // out of any nominal that happens to precede the negative (別に問題ない
      // read as 問題る), which the following ない alone cannot rule out.
      if (inflected.stem != surface || inflected.verb_type != grammar::VerbType::Ichidan ||
          inflected.confidence < candidate::verb_cost::kConstructedVerbMinConfidence ||
          !vh::isVerbInDictionary(dict_manager, inflected.base_form)) {
        continue;
      }
      candidates.push_back(makeVerbCandidate(surface, start_pos, end_pos, candidate::verb_cost::kStrongBonus,
                                             inflected.base_form, dictionary::ConjugationType::Ichidan, true,
                                             CandidateOrigin::VerbKanji, inflected.confidence, "ni_limited_ichidan",
                                             core::ExtendedPOS::VerbRenyokei));
      return;
    }
  }
}

}  // namespace

void generateVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                            const std::vector<normalize::CharType>& char_types, const grammar::Inflection& inflection,
                            const dictionary::DictionaryManager* dict_manager, const VerbCandidateOptions& verb_opts,
                            std::vector<UnknownCandidate>& candidates) {
  // The buffer is shared with the other generators for this position, so every
  // whole-buffer step below must stay inside the range this generator appended.
  const size_t candidate_start = candidates.size();

  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // Find kanji portion (typically 1-2 characters for verbs)
  size_t kanji_end = vh::findCharRegionEnd(char_types, start_pos, 3, normalize::CharType::Kanji);

  if (kanji_end == start_pos) {
    return;
  }

  // Look for hiragana after kanji
  if (kanji_end >= char_types.size() || char_types[kanji_end] != normalize::CharType::Hiragana) {
    return;
  }

  // Sokuonbin-prefixed verb stem: single kanji + っ + kanji run (突っ走, 引っ掻).
  // The sokuon is the contracted renyokei of a prefixing verb (突き -> 突っ); the
  // shape kanji+っ+kanji cannot be represented by the plain "kanji run + okurigana"
  // stem model, so the stem is extended to span the embedded verb when it is a
  // dictionary-verified verb showing conjugation evidence (shuushikei or an aux
  // suffix). The dictionary gate + evidence gate exclude same-shape nominal/
  // adjectival compounds (真っ盛り: 盛る not in dict; 手っ取り早い: 取り is a bare
  // renyokei, nominal). Once extended, all downstream conjugation/aux-split logic
  // treats 突っ走 exactly like any kanji stem.
  // True once the stem has been extended over a dictionary-verified embedded verb;
  // the resulting compound (突っ走る) is itself absent from the dictionary, so this
  // flag lets the confidence-gate and cost logic below treat it as verified.
  bool sokuonbin_stem_verified = false;
  // Correct lemma for the extended compound: sokuon prefix + embedded verb base
  // (吹っ + 飛ぶ = 吹っ飛ぶ). Forced explicitly at emission because the hatsuonbin
  // ん of the full surface is onbin-ambiguous (吹っ飛ん could derive 吹っ飛む), and
  // the embedded analysis is the authoritative source of the base form.
  std::string sokuonbin_lemma;
  grammar::VerbType sokuonbin_verb_type = grammar::VerbType::Unknown;
  if (dict_manager != nullptr && kanji_end == start_pos + 1 && codepoints[kanji_end] == core::hiragana::kSmallTsu &&
      kanji_end + 1 < char_types.size() && char_types[kanji_end + 1] == normalize::CharType::Kanji) {
    size_t kanji2_end = vh::findCharRegionEnd(char_types, kanji_end + 1, 3, normalize::CharType::Kanji);
    // Skip hatsuonbin (ん) continuations: 吹っ飛んだ's ん is onbin-ambiguous (ぶ/む/ぬ)
    // and 漢っ漢+ん compounds are already resolved by the dedicated
    // sokuon_kanji_hatsuonbin suffix candidate with the correct base. The verbs this
    // probe is needed for (走る っ-onbin, 掻く い-onbin) never use ん-onbin.
    if (kanji2_end < char_types.size() && char_types[kanji2_end] == normalize::CharType::Hiragana &&
        codepoints[kanji2_end] != U'ん') {
      size_t probe_end = vh::findCharRegionEnd(char_types, kanji2_end, 12, normalize::CharType::Hiragana);
      std::string embedded = extractSubstring(codepoints, kanji_end + 1, probe_end);
      for (const auto& res : inflection.analyze(embedded)) {
        // Shuushikei (surface == base, 走る) or a multi-character conjugation suffix
        // (った/いた/ります/ろう…) is verbal evidence. A bare renyokei — a single
        // i-row suffix (取り, 盛り) — is nominal and must not pass; standalone
        // renyokei of true compounds is handled by the compound-join path instead.
        bool conjugation_evidence = (embedded == res.base_form) || normalize::utf8Length(res.suffix) >= 2;
        if (conjugation_evidence && vh::isVerbInDictionary(dict_manager, res.base_form)) {
          sokuonbin_lemma = extractSubstring(codepoints, start_pos, kanji_end + 1) + res.base_form;
          sokuonbin_verb_type = res.verb_type;
          kanji_end = kanji2_end;  // stem now spans 突っ走 / 引っ掻; downstream logic unchanged
          sokuonbin_stem_verified = true;
          break;
        }
      }
    }
  }

  // Check if first hiragana is a particle that can NEVER be part of a verb
  // E.g., "領収書を" - を is a particle, not part of a verb
  // Note about が and に:
  // - が can be part of verbs: 上がる, 下がる, 受かる, etc.
  // - が can be mizenkei: 泳がれる (泳ぐ → 泳が + れる)
  // Check if the hiragana after kanji is a particle (not a verb conjugation)
  // e.g., 金がない → 金 + が + ない, not 金ぐ
  // Note about か: excluded - can be part of verb conjugation (書かない, 動かす)
  char32_t first_hiragana = codepoints[kanji_end];
  // Scan the complete okurigana before applying the first-mora particle
  // guard. A later closed excessive suffix can prove that an initially
  // particle-like mora belongs inside a longer continuative (積もり+すぎる).
  const size_t hiragana_end = vh::findCharRegionEnd(char_types, kanji_end, 12, normalize::CharType::Hiragana);
  bool has_excessive_renyokei_tail = false;
  for (size_t pos = kanji_end + 1; pos + 1 < hiragana_end; ++pos) {
    if (codepoints[pos] == U'す' && codepoints[pos + 1] == U'ぎ' &&
        (grammar::isIRowCodepoint(codepoints[pos - 1]) || grammar::isERowCodepoint(codepoints[pos - 1]))) {
      has_excessive_renyokei_tail = true;
      break;
    }
  }
  bool has_following_renyokei_auxiliary = false;
  if (dict_manager != nullptr && hiragana_end < char_types.size() &&
      char_types[hiragana_end] == normalize::CharType::Kanji && hiragana_end > kanji_end + 1 &&
      (grammar::isIRowCodepoint(codepoints[hiragana_end - 1]) ||
       grammar::isERowCodepoint(codepoints[hiragana_end - 1]))) {
    size_t predicate_end = hiragana_end;
    while (predicate_end < char_types.size() && predicate_end - hiragana_end < 6 &&
           (char_types[predicate_end] == normalize::CharType::Kanji ||
            char_types[predicate_end] == normalize::CharType::Hiragana)) {
      ++predicate_end;
    }
    const std::string predicate_probe = extractSubstring(codepoints, hiragana_end, predicate_end);
    for (const auto& result : dict_manager->lookup(predicate_probe, 0)) {
      if (result.entry == nullptr) {
        continue;
      }
      const auto extended_pos = result.entry->extended_pos;
      if (extended_pos == core::ExtendedPOS::AuxAspectHajimeru || extended_pos == core::ExtendedPOS::AuxExcessive ||
          extended_pos == core::ExtendedPOS::AuxInability ||
          (extended_pos == core::ExtendedPOS::AuxAspectShimau &&
           !grammar::isTeFormCompletiveAuxiliaryLemma(result.entry->lemma))) {
        has_following_renyokei_auxiliary = true;
        break;
      }
    }
  }
  // や is usually the enumerating particle, but kanji + やす is the regular
  // Godan-sa shape of verbs such as 増やす and 費やす.  The final す supplies
  // the inflectional evidence that distinguishes it from the particle.
  const bool is_yasu_godan_shape =
      first_hiragana == U'や' && kanji_end + 1 < codepoints.size() && codepoints[kanji_end + 1] == U'す';
  const bool has_complete_particle_initial_verb = hasCompleteParticleInitialVerbEvidence(
      codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, verb_opts);

  // Historical-kana spelling of the wa-row Godan paradigm (思ふ, 思ひけり,
  // 思へど).  Its row kana は/へ are also the topic and direction particles, so
  // the generator runs ahead of the particle gate below: each cell carries its
  // own evidence from the closed class that follows it, which is exactly what
  // that gate has no way to see.
  appendClassicalHaRowCandidates(codepoints, start_pos, kanji_end, hiragana_end, dict_manager, candidates);

  // ク語法 nominalizes the same kind of stem (言わく, 思わく). Its irrealis kana
  // is also an a-row cell the gate below rejects outright, so the generator runs
  // ahead of it and brings its own dictionary evidence.
  const size_t ku_nominalization_end =
      appendKuNominalizationCandidates(codepoints, start_pos, kanji_end, hiragana_end, dict_manager, candidates);

  if (normalize::isNeverVerbStemAfterKanji(first_hiragana) && !is_yasu_godan_shape && !has_excessive_renyokei_tail &&
      !has_following_renyokei_auxiliary && !has_complete_particle_initial_verb) {
    // Exception 1: A-row hiragana followed by れべき may be mizenkei pattern
    // e.g., 泳がれべき = 泳が (mizenkei) + れべき (passive + classical obligation)
    // Exception 2: A-row hiragana followed by れ is godan passive renyokei
    // e.g., 言われ = 言わ (mizenkei) + れ (passive renyokei of 言われる)
    // Exception 3: が followed by る is godan-ra verb pattern
    // e.g., 上がる, 下がる, 受かる - these are common godan-ra verbs
    // For patterns like 金がない, the が should remain NOUN + PARTICLE + ADJ
    bool is_verb_pattern = false;
    if (grammar::isARowCodepoint(first_hiragana)) {
      size_t next_pos = kanji_end + 1;
      if (next_pos < codepoints.size()) {
        char32_t next_char = codepoints[next_pos];
        if (next_char == U'れ') {
          // A-row + れ pattern: could be passive verb stem (言われ, 書かれ, etc.)
          is_verb_pattern = true;
        } else if (first_hiragana == U'が') {
          // が + る/ら/り/っ pattern: could be godan-ra verb (上がる, 下がる, 受かる)
          // Also handle conjugations: がら(mizenkei), がり(renyokei), がっ(onbin)
          // が + せ/さ/ず: godan-ga verb mizenkei patterns
          // E.g., 脱がせる, 脱がさない, 脱がず
          // が+な: only allow if kanji+ぐ is a known godan-ga verb
          // E.g., 脱がない (脱ぐ exists) vs 金がない (金ぐ doesn't exist)
          if (next_char == U'る' || next_char == U'ら' || next_char == U'り' || next_char == U'っ' ||
              next_char == U'れ' || next_char == U'せ' || next_char == U'さ' || next_char == U'ず') {
            is_verb_pattern = true;
          }
          // が+な: verify kanji+ぐ exists as godan-ga verb in dictionary
          if (!is_verb_pattern && next_char == U'な' && dict_manager != nullptr) {
            std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
            std::string gu_form = kanji_stem + "ぐ";
            if (vh::isVerbInDictionary(dict_manager, gu_form)) {
              is_verb_pattern = true;
            }
          }
        }
      }
    }
    if (!is_verb_pattern) {
      return;  // Not a verb - these particles follow nouns
    }
  }

  // Find hiragana portion (max 12 for conjugation + aux)
  // Note: We no longer break at particle-like characters here.
  // The inflection module will determine if the full string is a valid
  // conjugated verb. This allows patterns like "飲みながら" (nomi-nagara)
  // where "が" is part of the auxiliary "ながら", not a standalone particle.
  // Need at least some hiragana for a conjugated verb
  if (hiragana_end <= kanji_end) {
    return;
  }

  // Penalize verb candidates that start in the middle of a kanji run when
  // the preceding kanji and the candidate's first kanji form an exact
  // dictionary word. E.g., in 作画崩壊した a verb candidate 壊し starting at
  // 壊 would split the dictionary word 崩壊 (prefer 作画崩壊+し+た).
  // This mirrors the SPLIT_NV boundary guard in split_candidates.cpp, which
  // cannot cover paths assembled from independent noun and verb candidates.
  float mid_compound_penalty = 0.0F;
  if (start_pos > 0 && dict_manager != nullptr && char_types[start_pos - 1] == normalize::CharType::Kanji) {
    std::string boundary_pair =
        normalize::encodeUtf8(codepoints[start_pos - 1]) + normalize::encodeUtf8(codepoints[start_pos]);
    if (dict_manager->lookupExact(boundary_pair) != nullptr) {
      mid_compound_penalty = bigram_cost::kMinor;
      SUZUME_DEBUG_LOG("[COST_ADJ] verb candidates at pos " << start_pos << " +" << mid_compound_penalty
                                                            << " (boundary pair \"" << boundary_pair
                                                            << "\" is dict word)\n");
    }
  }

  // Detect a kanji verb renyokei followed by the excessive auxiliary すぎ;
  // 書きすぎる is compositional 書き + すぎる, not a single lexical verb.
  // Pattern: kanji + (き/ぎ/し/ち/に/び/み/り/い) + すぎ...
  std::string hira_part = extractSubstring(codepoints, kanji_end, hiragana_end);
  size_t sugi_pos = hiragana_end;
  for (size_t pos = kanji_end; pos + 1 < hiragana_end; ++pos) {
    if (codepoints[pos] == U'す' && codepoints[pos + 1] == U'ぎ') {
      sugi_pos = pos;
      break;
    }
  }
  const bool is_sugi_pattern = sugi_pos < hiragana_end;

  appendNiSugiPredicateCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);
  appendNiLimitedIchidanCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);

  // A closed excessive auxiliary owns the boundary immediately after a bare
  // kanji nominal/adjectival base (遠+すぎる, 最高+すぎる). With no okurigana
  // before すぎ there is no possible continuative material for the whole-span
  // verb generator to absorb; retaining that fallback only fabricates verbs
  // such as 遠すぎる→遠る. The left token's lexical POS remains independent.
  if (is_sugi_pattern && kanji_end == sugi_pos) {
    if (mid_compound_penalty != 0) {
      for (auto& cand : candidates) {
        cand.cost += mid_compound_penalty;
      }
    }
    return;
  }

  // A closed auxiliary that selects a verb continuative licenses the complete
  // multi-okurigana V1 (積もり+始める). Reconstruct from the final mora of the
  // whole span; an arbitrary following verb or noun is deliberately not
  // evidence, so nominal sequences such as 祭り+会場 stay outside this rule.
  if (has_following_renyokei_auxiliary &&
      !hasFinitePredicateCaseParticleTail(codepoints, start_pos, hiragana_end, inflection, dict_manager)) {
    const char32_t ending = codepoints[hiragana_end - 1];
    const std::string surface = extractSubstring(codepoints, start_pos, hiragana_end);
    if (grammar::isIRowCodepoint(ending)) {
      const std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(ending);
      if (!base_suffix.empty()) {
        const std::string base_form = normalize::concat(utf8::dropLastChar(surface), base_suffix);
        const grammar::VerbType verb_type = grammar::verbTypeFromIRowCodepoint(ending);
        candidates.push_back(makeVerbCandidate(surface, start_pos, hiragana_end, candidate::verb_cost::kStrongBonus,
                                               base_form, grammar::verbTypeToConjType(verb_type), true,
                                               CandidateOrigin::VerbKanji,
                                               candidate::verb_cost::kConstructedVerbMinConfidence,
                                               "extended_okurigana_renyokei", core::ExtendedPOS::VerbRenyokei));
      }
    } else if (grammar::isERowCodepoint(ending)) {
      // A span whose okurigana closes on the voice auxiliary (a-row + れ) is a
      // verb plus its passive, not a longer lexical V1: 使わ+れ+始める, never
      // 使われ+始める. A dictionary headword spelled the same way (生まれる)
      // keeps the whole span, which is what tells the two apart.
      const bool ends_on_passive = ending == U'れ' && hiragana_end >= kanji_end + 2 &&
                                   grammar::isARowCodepoint(codepoints[hiragana_end - 2]) &&
                                   !vh::isVerbInDictionary(dict_manager, surface + "る");
      if (!ends_on_passive) {
        candidates.push_back(makeVerbCandidate(surface, start_pos, hiragana_end, candidate::verb_cost::kStrongBonus,
                                               surface + "る", dictionary::ConjugationType::Ichidan, true,
                                               CandidateOrigin::VerbKanji,
                                               candidate::verb_cost::kConstructedVerbMinConfidence,
                                               "extended_okurigana_ichidan_renyokei", core::ExtendedPOS::VerbRenyokei));
      }
    }
  }

  // Generate verb renyokei candidates when followed by すぎ
  // E.g., 書きすぎた → 書き (renyokei of 書く) + すぎ + た (Godan)
  //       食べすぎた → 食べ (renyokei of 食べる) + すぎ + た (Ichidan)
  if (is_sugi_pattern && kanji_end < sugi_pos) {
    const char32_t renyokei_ending = codepoints[sugi_pos - 1];

    // Pattern 1: Godan verb renyokei (kanji + I-row hiragana + すぎ)
    // き→GodanKa, ぎ→GodanGa, し→GodanSa, ち→GodanTa, に→GodanNa,
    // び→GodanBa, み→GodanMa, り→GodanRa
    if (grammar::isIRowCodepoint(renyokei_ending)) {
      // Verify this is followed by すぎ
      if (sugi_pos + 1 < codepoints.size() && codepoints[sugi_pos] == U'す' && codepoints[sugi_pos + 1] == U'ぎ') {
        // Determine verb type from I-row ending
        grammar::VerbType verb_type = grammar::verbTypeFromIRowCodepoint(renyokei_ending);
        if (verb_type != grammar::VerbType::Unknown) {
          // Get base suffix (e.g., き → く for GodanKa)
          std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(renyokei_ending);
          if (!base_suffix.empty()) {
            // Replace the final i-row mora of the entire continuative, not
            // merely the first okurigana after the kanji run.  This covers
            // arbitrary-length okurigana (積もり+すぎる) as well as 書き.
            std::string surface = extractSubstring(codepoints, start_pos, sugi_pos);
            std::string base_form = normalize::concat(utf8::dropLastChar(surface), base_suffix);

            // Verify the base form is a valid verb
            // The closed excessive follower is itself inflectional evidence.
            // Multi-mora okurigana cannot be validated reliably by the
            // single-stem inflection probe (積もる is seen only as もる), so the
            // final i-row shape plus すぎ licenses the productive continuative.
            const bool has_extended_okurigana = sugi_pos > kanji_end + 1;
            const bool crosses_adjective_predicate =
                has_extended_okurigana && hasAdjectiveRenyokeiPredicateBoundary(codepoints, start_pos, sugi_pos,
                                                                                inflection, dict_manager, base_form);
            bool is_valid_verb = (has_extended_okurigana && !crosses_adjective_predicate) ||
                                 vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                                        candidate::verb_cost::kConstructedVerbMinConfidence, true);

            if (is_valid_verb) {
              size_t renyokei_end = sugi_pos;
              // Negative cost to beat compound NOUN path
              // Compound NOUNs like 書きすぎた get cost ~1.0, so we need much lower
              constexpr float kCost = candidate::verb_cost::kStrongBonus;
              SUZUME_DEBUG_VERBOSE_BLOCK {
                SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " godan_renyokei_sugi lemma=" << base_form
                                    << " cost=" << kCost << "\n";
              }
              candidates.push_back(makeVerbCandidate(
                  surface, start_pos, renyokei_end, kCost, base_form, grammar::verbTypeToConjType(verb_type), true,
                  CandidateOrigin::VerbKanji, 0.9F, "godan_renyokei_sugi", core::ExtendedPOS::VerbRenyokei));
            }
          }
        }
      }
    }

    // Pattern 2: Ichidan verb renyokei (kanji + E-row hiragana + すぎ)
    // E.g., 食べすぎた → 食べ (renyokei of 食べる) + すぎ + た
    //       見せすぎる → 見せ (renyokei of 見せる) + すぎる
    if (grammar::isERowCodepoint(renyokei_ending)) {
      // Verify this is followed by すぎ
      if (sugi_pos + 1 < codepoints.size() && codepoints[sugi_pos] == U'す' && codepoints[sugi_pos + 1] == U'ぎ') {
        // The full span before すぎ is the ichidan continuative, including
        // every okurigana mora (諦め+すぎる, not 諦+めすぎる).
        std::string ichidan_stem = extractSubstring(codepoints, start_pos, sugi_pos);
        std::string base_form = ichidan_stem + "る";

        // Verify the base form is a valid ichidan verb
        const bool has_extended_okurigana = sugi_pos > kanji_end + 1;
        const bool crosses_adjective_predicate =
            has_extended_okurigana &&
            hasAdjectiveRenyokeiPredicateBoundary(codepoints, start_pos, sugi_pos, inflection, dict_manager, base_form);
        bool is_valid_verb = (has_extended_okurigana && !crosses_adjective_predicate) ||
                             vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                                    candidate::verb_cost::kConstructedVerbMinConfidence, false);

        if (is_valid_verb) {
          size_t renyokei_end = sugi_pos;
          std::string surface = ichidan_stem;
          // Negative cost to beat compound NOUN path
          constexpr float kCost = candidate::verb_cost::kStrongBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " ichidan_renyokei_sugi lemma=" << base_form
                                << " cost=" << kCost << "\n";
          }
          candidates.push_back(makeVerbCandidate(surface, start_pos, renyokei_end, kCost, base_form,
                                                 dictionary::ConjugationType::Ichidan, true, CandidateOrigin::VerbKanji,
                                                 0.9F, "ichidan_renyokei_sugi", core::ExtendedPOS::VerbRenyokei));
        }
      }
    }

    // Early return to skip generating full verb forms containing すぎ
    // Prefer the grammatical renyokei + すぎ + auxiliary path.
    if (mid_compound_penalty != 0.0F) {
      for (auto& cand : candidates) {
        cand.cost += mid_compound_penalty;
      }
    }
    return;
  }

  // Godan mizenkei pattern: single-kanji + A-row + れ/せ (passive/causative)
  appendGodanMizenkeiPassiveCausativeCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection,
                                                dict_manager, candidates);

  // Contracted sa-row mizenkei: kanji + しゃ + れ/せ/し
  appendSaRowContractedMizenkeiCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, candidates);

  // Godan mizenkei pattern: kanji + A-row hiragana + ず (classical negative)
  appendGodanMizenkeiZuCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, candidates);

  appendAnalyzedKanjiVerbCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, verb_opts,
                                    sokuonbin_stem_verified, sokuonbin_lemma, candidates);

  // Try Ichidan renyokei pattern: kanji + e-row/i-row hiragana (+ shuushikei / multi-char stem)
  appendIchidanRenyokeiCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, verb_opts,
                                  candidates);

  // Try Godan-Sa renyokei stem pattern: kanji + hiragana ending in し
  appendGodanSaRenyokeiCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, verb_opts,
                                  candidates);

  // Try Ichidan verb kateikei (conditional) + volitional stem patterns
  appendIchidanKateikeiVolitionalCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                                            candidates);

  // Keep a productive Godan causative in its inflected verb unit when the
  // following ending confirms the renyokei or conditional form.
  appendCausativeRenyokeiCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, verb_opts,
                                    candidates);

  // Try Godan passive renyokei pattern: kanji + a-row + れ
  appendGodanPassiveRenyokeiCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                                       verb_opts, candidates);

  // NOTE: Ichidan passive forms (食べられる, 見られる) should split MeCab-style:
  //   食べられる → 食べ + られる (stem + passive auxiliary)
  //   見られる → 見 + られる
  // The ichidan stem candidates are generated in the section below
  // and the られる auxiliary is matched from entries.cpp.
  // We do NOT generate single-token passive candidates here to ensure split wins.

  // Generate Ichidan stem candidates for passive/potential auxiliary patterns (られ+X)
  appendIchidanStemRareCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager, candidates);

  // Generate single-kanji Ichidan verb candidates for auxiliary patterns
  appendSingleKanjiIchidanCandidates(codepoints, start_pos, kanji_end, hiragana_end, dict_manager, candidates);

  appendKanjiMizenkeiStemCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                                    candidates);
  appendKanjiOnbinCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                             sokuonbin_stem_verified, sokuonbin_lemma, sokuonbin_verb_type, candidates);
  appendVerifiedTailGodanTaCompoundCandidates(codepoints, start_pos, kanji_end, dict_manager, candidates);

  // Add emphatic variants (来た → 来たっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints, candidate_start);

  // An inflection-validated renyokei immediately followed by a particle can
  // head a deverbal noun phrase (鳴らしを, 書きを). Keep that noun reading
  // alongside the verbal candidate so a coincident auxiliary entry cannot
  // split the stem internally. The particle and lexical-continuation guards
  // keep finite predicates and derivational suffixes unaffected.
  std::vector<UnknownCandidate> nominalized_candidates;
  for (size_t idx = candidate_start; idx < candidates.size(); ++idx) {
    const UnknownCandidate& cand = candidates[idx];
    // A renyokei reading beginning at the second character of a kanji run is
    // not a standalone deverbal-noun host.  Keeping that fabricated reading
    // lets a two-kanji sahen stem split internally before a binding particle
    // (確認しさえ -> 確 / 認し / さえ).  The complete kanji run plus its
    // renyokei remains available as the productive analysis.
    const bool starts_inside_kanji_run = cand.start > 0 && normalize::isKanjiCodepoint(codepoints[cand.start - 1]) &&
                                         normalize::isKanjiCodepoint(codepoints[cand.start]);
    if (cand.pos != core::PartOfSpeech::Verb || cand.origin != core::CandidateOrigin::VerbKanji ||
        cand.extended_pos != core::ExtendedPOS::VerbRenyokei ||
        (!cand.lemma_verified && cand.conj_type != dictionary::ConjugationType::GodanSa) || starts_inside_kanji_run ||
        hasDictionaryAdjectiveTail(codepoints, cand.start, cand.end, dict_manager) ||
        vh::isBoundSuffixAfterNominalHost(dict_manager, codepoints, cand.start, cand.surface) ||
        !hasNominalizedNounParticleContinuation(codepoints, cand.end, dict_manager)) {
      continue;
    }
    nominalized_candidates.push_back(makeNounCandidate(
        cand.surface, cand.start, cand.end,
        candidate::kVerifiedRenyokeiNominalCandidateCost + candidate::kNominalizedNounParticleBonus, true,
        core::CandidateOrigin::NominalizedNoun, core::ExtendedPOS::NounVerbal, "verified_renyokei_nominal"));
  }
  candidates.insert(candidates.end(), nominalized_candidates.begin(), nominalized_candidates.end());

  // Do not let an unverified inflection hypothesis replace an exact
  // dictionary function word or deverbal noun.  Kana-final dictionary forms
  // such as 概ね, 答え and 同じ otherwise invite fabricated ichidan/godan
  // lemmas solely because their final kana resembles a renyokei marker.
  // Verified lexical verb forms remain available for genuine homographs.
  candidates.erase(
      std::remove_if(candidates.begin() + static_cast<std::ptrdiff_t>(candidate_start), candidates.end(),
                     [&](const UnknownCandidate& cand) {
                       const bool has_auxiliary_continuation =
                           cand.end < codepoints.size() &&
                           (codepoints[cand.end] == U'た' || codepoints[cand.end] == U'て');
                       const bool has_excessive_auxiliary_continuation =
                           dict_manager != nullptr && cand.end + 2 <= codepoints.size() && [&] {
                             const auto* next = lookupEntryInRange(*dict_manager, codepoints, cand.end, cand.end + 2);
                             return next != nullptr && next->extended_pos == core::ExtendedPOS::AuxExcessive;
                           }();
                       const bool has_passive_auxiliary_continuation = cand.end + 1 < codepoints.size() &&
                                                                       codepoints[cand.end] == U'ら' &&
                                                                       codepoints[cand.end + 1] == U'れ';
                       const bool has_comma_clause_continuation =
                           vh::isCommaClauseChainingRenyokei(codepoints, cand.start, cand.end, dict_manager);
                       return cand.pos == core::PartOfSpeech::Verb && !cand.lemma_verified &&
                              ((cand.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                grammar::endsWithRenyokeiMarker(cand.surface) && !has_auxiliary_continuation &&
                                !has_excessive_auxiliary_continuation && !has_passive_auxiliary_continuation &&
                                !has_comma_clause_continuation) ||
                               (cand.extended_pos == core::ExtendedPOS::VerbShuushikei &&
                                cand.surface.compare(cand.lemma) == 0)) &&
                              vh::hasNonVerbDictionaryEntry(dict_manager, cand.surface);
                     }),
      candidates.end());

  // Once the dictionary licenses a ク語法 reading of a span, an unverified verb
  // hypothesis over the same characters is a fabrication: it has to invent a
  // paradigm whose terminal is the nominalizer (思わく as a godan-ka 終止形).
  // Left standing it collects the continuative bonuses a real predicate earns —
  // 終止形 before a formal noun is worth more than the nominal boundary — and
  // wins on connections it has no claim to.
  if (ku_nominalization_end > start_pos) {
    candidates.erase(std::remove_if(candidates.begin() + static_cast<std::ptrdiff_t>(candidate_start), candidates.end(),
                                    [&](const UnknownCandidate& cand) {
                                      return cand.pos == core::PartOfSpeech::Verb && !cand.lemma_verified &&
                                             cand.start == start_pos && cand.end == ku_nominalization_end;
                                    }),
                     candidates.end());
  }

  // A case-marked argument or quantified focus phrase followed by a bare
  // continuative and comma is strong clause-level evidence. Candidate
  // generation already preserves unknown verbs in this context; discount the
  // verbal path as well so noun/adjective homographs do not win merely through
  // cheaper local connections.
  for (size_t idx = candidate_start; idx < candidates.size(); ++idx) {
    UnknownCandidate& cand = candidates[idx];
    if (cand.pos == core::PartOfSpeech::Verb && cand.extended_pos == core::ExtendedPOS::VerbRenyokei &&
        vh::isCommaClauseChainingRenyokei(codepoints, cand.start, cand.end, dict_manager)) {
      cand.cost += candidate::verb_cost::kCommaClauseRenyokeiBonus;
    }
  }

  // Apply mid-kanji-run dictionary compound penalty (see comment above)
  if (mid_compound_penalty != 0.0F) {
    for (size_t idx = candidate_start; idx < candidates.size(); ++idx) {
      candidates[idx].cost += mid_compound_penalty;
    }
  }

  // Sort by cost and return best candidates
  vh::sortCandidatesByCost(candidates, candidate_start);

  return;
}

}  // namespace suzume::analysis
