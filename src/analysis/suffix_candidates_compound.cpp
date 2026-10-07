/**
 * @file suffix_candidates_compound.cpp
 * @brief Suffix-based unknown word candidate generation
 */

#include <algorithm>
#include <array>

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/honorific_verbs.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "suffix_candidates_compound_internal.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

// End of the hiragana run at @p start, at most four kana and stopping before a
// particle-like kana. The terminal copula だ opens a predicate over the noun
// in front (久しぶり+だ, 宝くじ+だ, 本+だ), and a final particle closing the
// clause ends it once past the first kana (久しぶり+ね).
size_t scanCompoundHiraganaEnd(const std::vector<char32_t>& codepoints,
                               const std::vector<normalize::CharType>& char_types, size_t start,
                               const dictionary::DictionaryManager* dict_manager) {
  size_t end = start;
  while (end < char_types.size() && end - start < 4 && char_types[end] == normalize::CharType::Hiragana &&
         !normalize::isParticleCodepoint(codepoints[end])) {
    if (dict_manager != nullptr) {
      const auto* copula = lookupEntryInRange(*dict_manager, codepoints, end, end + 1, core::PartOfSpeech::Auxiliary);
      const bool terminal_copula = copula != nullptr && copula->extended_pos == core::ExtendedPOS::AuxCopulaDa &&
                                   copula->lemma == extractSubstring(codepoints, end, end + 1);
      const auto* final_particle =
          lookupEntryInRange(*dict_manager, codepoints, end, end + 1, core::PartOfSpeech::Particle);
      const bool closes_clause_after =
          end + 1 >= codepoints.size() || char_types[end + 1] != normalize::CharType::Hiragana;
      if (terminal_copula ||
          (end > start && final_particle != nullptr &&
           final_particle->extended_pos == core::ExtendedPOS::ParticleFinal && closes_clause_after)) {
        break;
      }
    }
    ++end;
  }
  return end;
}

bool hasNominalPhraseSelectorAt(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t pos) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  const auto* particle = lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Particle);
  if (particle != nullptr && (particle->extended_pos == core::ExtendedPOS::ParticleCase ||
                              particle->extended_pos == core::ExtendedPOS::ParticleTopic ||
                              particle->extended_pos == core::ExtendedPOS::ParticleNo)) {
    return true;
  }
  const auto* auxiliary = lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Auxiliary);
  return auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxCopulaDa;
}

// A particle-like kana may be the final mora of an open-class content noun. If
// the mixed span stops immediately before it and a real nominal selector follows
// it, treating the stranded mora as a particle creates an impossible particle
// stack (組みひ|も|を). Reject that cut so the lattice can instead place the
// content-word boundary to its left (組み|ひも|を).
bool strandsParticleLikeMoraBeforeNominalSelector(const dictionary::DictionaryManager* dict_manager,
                                                  const std::vector<char32_t>& codepoints, size_t candidate_end) {
  return candidate_end + 1 < codepoints.size() && normalize::isParticleCodepoint(codepoints[candidate_end]) &&
         hasNominalPhraseSelectorAt(dict_manager, codepoints, candidate_end + 1);
}

// A particle and a determiner are both fixed forms of a closed class: neither
// derives from anything or inflects, so neither has an interior a word boundary
// could fall on.
bool isUninflectedClosedClass(const dictionary::DictionaryEntry& entry) {
  return entry.pos == core::PartOfSpeech::Particle || entry.pos == core::PartOfSpeech::Determiner;
}

/**
 * @brief Whether the span boundary falls inside a fixed closed-class word.
 *
 * A compound noun cannot end part-way through one: 読まれども is 読ま + れ +
 * ども, so a span reaching only 読まれど has cut the concessive conjunction in
 * half and owes its score to that cut. A determiner is cut the same way when it
 * follows a predicate, because its first mora completes a plausible mixed-script
 * span and its second is a particle on its own (走るそ|の for 走る + その).
 */
bool boundarySplitsClosedClassWord(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t kanji_end, size_t end_pos) {
  if (dict_manager == nullptr || end_pos >= codepoints.size()) {
    return false;
  }
  const size_t scan_start = (end_pos > kanji_end + 1) ? end_pos - 2 : kanji_end;
  const size_t probe_end = std::min(codepoints.size(), end_pos + 2);
  for (size_t start = scan_start; start < end_pos; ++start) {
    if (hasDictionaryEntryFrom(dict_manager, codepoints, start, end_pos + 1 - start, probe_end - start,
                               core::PartOfSpeech::Unknown, &isUninflectedClosedClass)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Whether the hiragana portion opens with a multi-mora conjunctive particle.
 *
 * A 接続助詞 attaches to a predicate, so a kanji run immediately followed by one
 * is a verb or adjective stem, not the head of a compound noun (見+ちゃ+だめ,
 * 読ん+じゃ+だめ). This is the head-side counterpart of the て/で tail check
 * below; single-mora members are left out because their kana are also ordinary
 * word-internal morae (手しごと, 雨やどり).
 * @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
 */
bool startsWithConjunctiveParticle(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t kanji_end, size_t end_pos) {
  if (dict_manager == nullptr || end_pos <= kanji_end) {
    return false;
  }
  constexpr size_t kMinimumParticleLength = 2;
  constexpr size_t kParticleProbe = 4;
  return hasDictionaryEntryFrom(
      dict_manager, codepoints, kanji_end, kMinimumParticleLength, std::min(kParticleProbe, end_pos - kanji_end),
      core::PartOfSpeech::Particle,
      [](const dictionary::DictionaryEntry& entry) { return entry.extended_pos == core::ExtendedPOS::ParticleConj; });
}

}  // namespace

void generateKanjiHiraganaCompoundCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const dictionary::DictionaryManager* dict_manager,
                                             std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // Skip if this kanji is preceded by another kanji - it's likely the tail end
  // of a longer kanji compound, not the start of a new kanji+hiragana word.
  // E.g., in 魔法少女まどか, skip generating 女まど at pos=3.
  // Dictionary entries (玉ねぎ etc.) are handled separately as dict candidates.
  if (start_pos > 0 && char_types[start_pos - 1] == normalize::CharType::Kanji) {
    return;
  }

  // A non-quantity nominal stem plus the closed comparison bound 以上/以下
  // forms one search unit (必要以上, 期待以下). Numeral+counter phrases retain
  // their compositional boundary (三名|以上, 百倍|以下), which is owned by the
  // counter generator. A following nominal selector proves the right edge.
  const size_t kanji_run_end = findCharRegionEnd(char_types, start_pos, char_types.size(), normalize::CharType::Kanji);
  if (kanji_run_end >= start_pos + 4 && codepoints[kanji_run_end - 2] == U'以' &&
      (codepoints[kanji_run_end - 1] == U'上' || codepoints[kanji_run_end - 1] == U'下') &&
      !normalize::isNumeralCodepoint(codepoints[kanji_run_end - 3]) &&
      !normalize::isCounterKanji(codepoints[kanji_run_end - 3]) &&
      hasNominalPhraseSelectorAt(dict_manager, codepoints, kanji_run_end)) {
    const std::string surface = extractSubstring(codepoints, start_pos, kanji_run_end);
    auto comparison = makeCandidate(surface, start_pos, kanji_run_end, core::PartOfSpeech::Noun,
                                    candidate::kComparisonCompoundNounCost, false, CandidateOrigin::SuffixPattern);
    comparison.lemma = surface;
    SUZUME_DEBUG_CANDIDATE(comparison, candidate::kDictionaryOriginConfidence, "comparison_bound_compound");
    candidates.push_back(std::move(comparison));
    return;
  }

  // Kanji portion is 1 character only for compound nouns
  const size_t kanji_end = start_pos + 1;

  // -がかり and -がけ are nominal suffixes after a noun or a verb
  // continuative (手がかり, 通りがかり, 通りがけ, 一日がけ).  The
  // nominal-phrase-particle gate distinguishes these closed nominal constructions
  // from an ordinary subject marker followed by unrelated hiragana, while
  // keeping the complete compound as one search unit in a noun phrase.
  // After a bare nominal host, がけ is a suffix of its own (声+がけ, as 一日+がけ);
  // a verbal-noun predicate (する, the request/humble verbs) may close it too.
  // A continuative host keeps the deverbal compound whole (通りがけ, 出がけ).
  constexpr std::string_view kGakari = "がかり";
  constexpr std::string_view kGake = "がけ";
  const auto verbal_noun_predicate_at = [&](size_t pos) {
    if (dict_manager == nullptr || pos >= codepoints.size()) {
      return false;
    }
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, pos, codepoints.size())) {
      if (match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Verb &&
          (grammar::isSuruBaseForm(match.entry->lemma) || grammar::isHumbleHonorificLemma(match.entry->lemma))) {
        return true;
      }
    }
    return false;
  };
  // An ichidan stem written as the bare kanji (出 of 出る) is a continuative
  // host, and a host that heads a lexical がける verb (心がける) is that
  // verb's deverbal noun.
  const std::string gake_host = extractSubstring(codepoints, start_pos, kanji_run_end);
  const bool nominal_host =
      dict_manager != nullptr &&
      lookupEntryInRange(*dict_manager, codepoints, start_pos, kanji_run_end, core::PartOfSpeech::Verb) == nullptr &&
      !(kanji_run_end == start_pos + 1 && verb_helpers::isSingleKanjiIchidan(codepoints[start_pos])) &&
      !verb_helpers::isVerbInDictionary(dict_manager, gake_host + "る") &&
      !verb_helpers::isVerbInDictionary(dict_manager, gake_host + "がける");
  for (size_t suffix_start = kanji_run_end;
       suffix_start < codepoints.size() && char_types[suffix_start] == normalize::CharType::Hiragana; ++suffix_start) {
    for (std::string_view suffix : {kGakari, kGake}) {
      const size_t suffix_end = suffix_start + normalize::utf8Length(suffix);
      if (suffix_end > codepoints.size() || extractSubstring(codepoints, suffix_start, suffix_end) != suffix) {
        continue;
      }
      if (suffix == kGake && suffix_start == kanji_run_end && nominal_host &&
          (hasNominalPhraseSelectorAt(dict_manager, codepoints, suffix_end) || verbal_noun_predicate_at(suffix_end))) {
        auto gake = makeCandidate(std::string(kGake), suffix_start, suffix_end, core::PartOfSpeech::Suffix,
                                  candidate::kCounterExtentSuffixCost, true, CandidateOrigin::SuffixPattern,
                                  core::ExtendedPOS::Suffix);
        gake.lemma = std::string(kGake);
        SUZUME_DEBUG_CANDIDATE_PATTERN(gake, "nominal_gake_suffix");
        candidates.push_back(std::move(gake));
        return;
      }
      if (!hasNominalPhraseSelectorAt(dict_manager, codepoints, suffix_end)) {
        continue;
      }
      const std::string surface = extractSubstring(codepoints, start_pos, suffix_end);
      auto candidate = makeCandidate(surface, start_pos, suffix_end, core::PartOfSpeech::Noun,
                                     candidate::kDerivedSuffixCompoundNounCost, false, CandidateOrigin::SuffixPattern);
      candidate.lemma = surface;
      SUZUME_DEBUG_CANDIDATE(candidate, candidate::kDictionaryOriginConfidence, "nominal_gakari_gake");
      candidates.push_back(std::move(candidate));
      return;
    }
  }

  // Find hiragana portion (2-4 characters)
  if (kanji_end >= char_types.size() || char_types[kanji_end] != normalize::CharType::Hiragana) {
    return;
  }
  const size_t hiragana_end = scanCompoundHiraganaEnd(codepoints, char_types, kanji_end, dict_manager);
  const size_t hiragana_len = hiragana_end - kanji_end;
  const char32_t first_hira = codepoints[kanji_end];

  // A kanji numeral followed by つ is already a complete native counter
  // (一つ, 二つ). Do not extend it into an invented kanji-hiragana compound
  // when another hiragana word follows (一つ|ひとつ), because the counter
  // generator emits the natural boundary separately.
  if (normalize::isNumeralCodepoint(codepoints[start_pos]) && first_hira == U'つ') {
    return;
  }

  // Handle sokuon (っ) pattern FIRST, before the hiragana_len check
  // Pattern: 漢字 + っ + (漢字 or 平仮名) - e.g., 横っ面, 取っ手, 引っ込む
  // These are valid compound words where hiragana portion may be just 1 char (っ)
  if (first_hira == U'っ') {
    // Need at least one more character after っ
    size_t sokuon_pos = kanji_end;  // Position of っ
    if (sokuon_pos + 1 < char_types.size()) {
      normalize::CharType next_type = char_types[sokuon_pos + 1];

      if (next_type == normalize::CharType::Kanji) {
        // Pattern: 漢字 + っ + 漢字 (e.g., 横っ面, 取っ手)
        size_t kanji2_end = findCharRegionEnd(char_types, sokuon_pos + 1, 3, normalize::CharType::Kanji);

        // Generate candidates for each length
        for (size_t end_pos = sokuon_pos + 2; end_pos <= kanji2_end; ++end_pos) {
          auto cand = makeCandidate(codepoints, start_pos, end_pos, core::PartOfSpeech::Noun,
                                    candidate::kInfixCompoundNounCost, false, CandidateOrigin::KanjiHiraganaCompound);
          SUZUME_DEBUG_CANDIDATE(cand, 0.9F, "kanji_sokuon_kanji");
          candidates.push_back(cand);
        }

        // Check for hatsuonbin verb: 漢字+っ+漢字+ん (e.g., 吹っ飛ん from 吹っ飛ぶ)
        // When the second kanji is followed by ん, check if kanji2+ぶ/む/ぬ is in dict
        if (kanji2_end < codepoints.size() && codepoints[kanji2_end] == U'ん' && dict_manager != nullptr) {
          std::string kanji2_stem = extractSubstring(codepoints, sokuon_pos + 1, kanji2_end);

          auto hatsuonbin_match = verb_helpers::firstGodanOnbinDictBase(dict_manager, kanji2_stem, "ん");
          if (hatsuonbin_match.matched) {
            size_t onbin_end = kanji2_end + 1;  // Include ん
            std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);
            constexpr float kHatsuonbinCost = -0.5F;
            auto cand = makeCandidate(onbin_surface, start_pos, onbin_end, core::PartOfSpeech::Verb, kHatsuonbinCost,
                                      false, CandidateOrigin::KanjiHiraganaCompound);
            // Full base form includes the first kanji + っ
            std::string full_kanji = extractSubstring(codepoints, start_pos, kanji2_end);
            cand.lemma = normalize::concat(full_kanji, hatsuonbin_match.base_suffix);
            cand.conj_type = grammar::verbTypeToConjType(hatsuonbin_match.verb_type);
            cand.extended_pos = core::ExtendedPOS::VerbOnbinkei;
            SUZUME_DEBUG_CANDIDATE(cand, 0.9F, "sokuon_kanji_hatsuonbin");
            SUZUME_DEBUG_LOG("[SUFFIX_CAND] " << onbin_surface << " sokuon_kanji_hatsuonbin lemma=" << cand.lemma
                                              << " cost=" << kHatsuonbinCost << "\n");
            candidates.push_back(cand);
          }
        }
      } else if (next_type == normalize::CharType::Hiragana) {
        // Pattern: 漢字 + っ + 平仮名 (e.g., 引っ込む, 突っ走る)
        // BUT skip if っ is followed by た/て (verb conjugation endings)
        // e.g., 減った, 勝って are verb forms, not compound nouns
        char32_t next_hira = codepoints[sokuon_pos + 1];
        if (next_hira == U'た' || next_hira == U'て') {
          return;  // Skip - this is a verb conjugation, not a compound noun
        }
        const size_t hira2_end = scanCompoundHiraganaEnd(codepoints, char_types, sokuon_pos + 1, dict_manager);

        if (hira2_end > sokuon_pos + 1) {
          // A registered adjective beginning at the sokuon is a productive
          // suffix boundary (e.g. noun + っぽ + さ). Do not fabricate a
          // single compound noun across it; the dictionary candidates retain
          // the suffix inflection and any following nominalizer.
          if (dict_manager != nullptr) {
            // The suffix's inflected cells may run past the compound scan,
            // which stops before an auxiliary-like tail (油っこ+かった).
            const size_t hiragana_run_end = findCharRegionEnd(char_types, sokuon_pos, codepoints.size() - sokuon_pos,
                                                              normalize::CharType::Hiragana);
            for (const auto& entry : lookupResultsInRange(*dict_manager, codepoints, sokuon_pos, hiragana_run_end)) {
              // A registered derivational suffix opening at the sokuon (色+っけ)
              // keeps its own boundary, as the adjectival suffixes below do.
              if (entry.entry != nullptr && entry.entry->pos == core::PartOfSpeech::Suffix) {
                return;
              }
              if (entry.entry != nullptr && entry.entry->pos == core::PartOfSpeech::Adjective) {
                // っぽい and っこい: a sokuon, one mora, and the adjective ending.
                const std::string& suffix_lemma = entry.entry->lemma;
                const bool sokuon_suffix =
                    utf8::startsWith(suffix_lemma, "っ") && adj_detail::isCompoundFormingAdjective(suffix_lemma);
                if (!sokuon_suffix && sokuon_pos + entry.length > hira2_end) {
                  continue;
                }
                const size_t suffix_stem_end = sokuon_pos + 2;
                const bool stem_before_inflection =
                    sokuon_suffix && entry.entry->extended_pos == core::ExtendedPOS::AdjStem &&
                    suffix_stem_end < hiragana_run_end && codepoints[suffix_stem_end] != U'さ';
                if (stem_before_inflection) {
                  continue;
                }
                // A single-kanji nominal/adjectival host forms one search unit
                // with the productive suffix (安っぽい, 水っぽい, 油っこい).
                // Longer nominal hosts retain the noun + suffix boundary
                // (子供 + っぽい), while verb continuatives are handled by the
                // dedicated productive path below.
                const bool precedes_nominalizer =
                    suffix_stem_end < hiragana_run_end && codepoints[suffix_stem_end] == U'さ';
                if (sokuon_suffix && !precedes_nominalizer) {
                  const size_t derived_end = sokuon_pos + entry.length;
                  auto adjective = makeCandidate(codepoints, start_pos, derived_end, core::PartOfSpeech::Adjective,
                                                 candidate::kProductivePpoiAdjCost, false,
                                                 CandidateOrigin::KanjiHiraganaCompound, entry.entry->extended_pos);
                  adjective.lemma = extractSubstring(codepoints, start_pos, sokuon_pos) + suffix_lemma;
                  adjective.conj_type = dictionary::ConjugationType::IAdjective;
                  candidates.push_back(std::move(adjective));
                  return;
                }
                const std::string base = extractSubstring(codepoints, start_pos, sokuon_pos);
                // An i-adjective stem productively forms the suffix.  Keep its
                // stem before the following nominalizer (安っぽ+さ), while a
                // nominal base such as 男 retains the ordinary noun+suffix
                // boundary.  The dictionary gate is on the adjective base,
                // not on individual derived words.
                if (sokuon_suffix && suffix_stem_end <= codepoints.size() &&
                    extractSubstring(codepoints, sokuon_pos, suffix_stem_end) + "い" == suffix_lemma) {
                  if (dict_manager->lookupExact(base + "い", core::PartOfSpeech::Adjective) != nullptr) {
                    auto stem = makeCandidate(codepoints, start_pos, suffix_stem_end, core::PartOfSpeech::Adjective,
                                              candidate::kCompoundAdjBaseCost, true,
                                              CandidateOrigin::KanjiHiraganaCompound, core::ExtendedPOS::AdjStem);
                    stem.lemma = base + suffix_lemma;
                    stem.conj_type = dictionary::ConjugationType::IAdjective;
                    candidates.push_back(std::move(stem));
                  }
                }
                return;
              }
            }
          }

          auto cand = makeCandidate(codepoints, start_pos, hira2_end, core::PartOfSpeech::Noun, 1.0F, false,
                                    CandidateOrigin::KanjiHiraganaCompound);
          SUZUME_DEBUG_CANDIDATE(cand, 0.7F, "kanji_sokuon_hira");
          candidates.push_back(cand);
        }
      }
    }
    // Return after handling sokuon - don't continue to normal hiragana logic
    return;
  }

  // Pattern: 単漢字 + ん + 単漢字 — the moraic nasal infixed inside one lexical
  // compound (真ん前, 真ん丸, 赤ん坊), the phonological sibling of the っ
  // pattern above. The contracted genitive の spells the same mora (店+ん+中),
  // so this only adds a candidate: where both flanking kanji are attested
  // nouns their own dictionary edges keep the split cheaper.
  if (first_hira == U'ん' && kanji_end + 1 < char_types.size() &&
      char_types[kanji_end + 1] == normalize::CharType::Kanji) {
    const size_t end_pos = kanji_end + 2;
    const bool second_kanji_is_single =
        end_pos >= char_types.size() || char_types[end_pos] != normalize::CharType::Kanji;
    if (second_kanji_is_single) {
      // An attributive copula right after the compound identifies it as a
      // na-adjective stem rather than a plain noun (真ん丸+な+月).
      const bool has_attributive_copula = end_pos < codepoints.size() && codepoints[end_pos] == U'な';
      auto cand = makeCandidate(codepoints, start_pos, end_pos,
                                has_attributive_copula ? core::PartOfSpeech::Adjective : core::PartOfSpeech::Noun,
                                candidate::kInfixCompoundNounCost, false, CandidateOrigin::KanjiHiraganaCompound);
      if (has_attributive_copula) {
        cand.extended_pos = core::ExtendedPOS::AdjNaAdj;
      }
      SUZUME_DEBUG_CANDIDATE(cand, 0.9F, "kanji_hatsuon_kanji");
      candidates.push_back(cand);
    }
    return;
  }

  // A single さ after a kanji nominal has no reading of its own here. The
  // nominalizer derives a noun from an adjective stem and cannot take a plain
  // nominal host; the final particle さ only occurs clause-finally. When a
  // nominal-selecting particle follows, both are excluded by elimination and
  // the mixed-script span is one lexical compound (逆さに映る, but 今さ、…
  // keeps the final particle and 高さ keeps the nominalizer).
  if (hiragana_len == 1 && first_hira == U'さ' &&
      !isAdjectiveNominalizationSa(dict_manager, codepoints, start_pos, hiragana_end) &&
      hasNominalPhraseSelectorAt(dict_manager, codepoints, hiragana_end)) {
    auto cand = makeCandidate(codepoints, start_pos, hiragana_end, core::PartOfSpeech::Noun,
                              candidate::kInfixCompoundNounCost, false, CandidateOrigin::KanjiHiraganaNominalCompound);
    SUZUME_DEBUG_CANDIDATE(cand, candidate::kHighOriginConfidence, "kanji_nominalizer_sa_compound");
    candidates.push_back(cand);
    return;
  }

  if (hiragana_len < 2) {
    return;
  }
  const char32_t second_hira = codepoints[kanji_end + 1];

  // A kanji verb continuative stem productively combines with the resemblance
  // suffix っぽい to form one i-adjective search unit (忘れっぽい, 飽きっぽい).
  // This is morphology, not a per-word lexicon: i-row marks Godan
  // continuative stems and e-row marks Ichidan continuative stems.
  const std::string hiragana_candidate = extractSubstring(codepoints, kanji_end, hiragana_end);
  if (utf8::endsWith(hiragana_candidate, "っぽい") &&
      (kana::isIRowCodepoint(first_hira) || kana::isERowCodepoint(first_hira))) {
    const std::string derived = extractSubstring(codepoints, start_pos, hiragana_end);
    auto adjective = makeCandidate(derived, start_pos, hiragana_end, core::PartOfSpeech::Adjective,
                                   candidate::kProductivePpoiAdjCost, false, CandidateOrigin::KanjiHiraganaCompound,
                                   core::ExtendedPOS::AdjBasic);
    adjective.lemma = derived;
    adjective.conj_type = dictionary::ConjugationType::IAdjective;
    candidates.push_back(std::move(adjective));
    return;
  }

  // Skip small kana at start - morphologically invalid
  // EXCEPTION: っ (sokuon) can appear in compound patterns like 横っ面, 取っ手, 引っ込む
  // These are valid words where kanji + っ + (kanji or hiragana) forms a compound
  if (first_hira == U'ゃ' || first_hira == U'ゅ' || first_hira == U'ょ' || first_hira == U'ぁ' || first_hira == U'ぃ' ||
      first_hira == U'ぅ' || first_hira == U'ぇ' || first_hira == U'ぉ') {
    return;
  }

  // Skip patterns ending with ん - likely honorific suffixes
  // e.g., さん, くん, ちゃん, たん should split as NOUN + SUFFIX
  // This is a grammatical pattern: hiragana ending with ん after single kanji
  // is typically an honorific suffix, not a compound noun
  if (codepoints[hiragana_end - 1] == U'ん') {
    return;
  }

  // Check if pattern looks like a grammatical suffix
  // These get high cost to let verb/adjective candidates win
  bool looks_like_aux = false;

  // te/ta form, copula patterns
  if (second_hira == U'て' || second_hira == U'た' || second_hira == U'で' || second_hira == U'だ') {
    looks_like_aux = true;
  }
  // ます, ない, れる, せる, だった, だろう, なら, なかった
  constexpr std::array<std::array<char32_t, 2>, 8> kAuxKanaPairs{{{U'ま', U'す'},
                                                                  {U'な', U'い'},
                                                                  {U'れ', U'る'},
                                                                  {U'せ', U'る'},
                                                                  {U'だ', U'っ'},
                                                                  {U'だ', U'ろ'},
                                                                  {U'な', U'ら'},
                                                                  {U'な', U'か'}}};
  if (std::any_of(kAuxKanaPairs.begin(), kAuxKanaPairs.end(), [&](const std::array<char32_t, 2>& pair) {
        return first_hira == pair[0] && second_hira == pair[1];
      })) {
    looks_like_aux = true;
  }
  // An o-row irrealis followed by the volitional う (走ろ+う, 食べよ+う) is a
  // predicate, not the inside of a compound noun.
  if (kana::isORowCodepoint(first_hira) && verb_helpers::volitionalEndingFollowsAt(codepoints, kanji_end + 1)) {
    looks_like_aux = true;
  }
  // Godan verb shuushikei (終止形) pattern
  // e.g., 休む, 行く, 泳ぐ, 話す, 立つ, 死ぬ, 飛ぶ, 取る
  // If first hiragana is a godan verb ending, kanji+first hiragana likely forms
  // a complete verb, and the rest starts a new word
  // 休むこと → 休む(VERB) + こと(NOUN), not 休むこ(NOUN) + と(PARTICLE)
  const bool is_godan_shuushikei = grammar::isModernGodanTerminalKana(first_hira);
  if (is_godan_shuushikei) {
    // The 終止形 split hypothesis (kanji+first_hira is a complete verb, the rest starts
    // a new word) is only sound when the stranded remainder is lexically realizable.
    // When exactly one hiragana would be orphaned (hiragana_len == 2), require that a
    // dictionary word can start there; otherwise the "verb" reading strands junk (宝く|じ)
    // and we must keep the kanji+hiragana noun (宝くじ) whole. Standalone single hiragana
    // are a closed class (final particles よ/ね/な, copula, …) all in L1, and formal-noun
    // continuations (こと) are caught by scanning across the particle break — so 休むこと,
    // 飲むな, 帰るね, 行くよ still split as before.
    const size_t orphan_pos = kanji_end + 1;
    const bool orphan_split_viable =
        hiragana_len != 2 || dict_manager == nullptr ||
        lookupResultsHavePartOfSpeech(
            lookupResultsInRange(*dict_manager, codepoints, orphan_pos,
                                 findCharRegionEnd(char_types, orphan_pos, 3, normalize::CharType::Hiragana)),
            partOfSpeechMask(core::PartOfSpeech::Particle));
    if (orphan_split_viable) {
      looks_like_aux = true;
    }
  }
  // Renyokei + そう/たい/ます
  // For godan verbs: し,み,き,ぎ,ち,り,い,び (i-row)
  // For ichidan verbs: べ,め,け,せ,て,ね,れ,え (e-row) - these are verb stems
  const bool is_renyokei = (first_hira == U'し' || first_hira == U'み' || first_hira == U'き' || first_hira == U'ぎ' ||
                            first_hira == U'ち' || first_hira == U'り' || first_hira == U'い' || first_hira == U'び');
  const bool is_ichidan_stem = kana::isERowCodepoint(first_hira);
  if ((is_renyokei || is_ichidan_stem) && (second_hira == U'そ' || second_hira == U'た' || second_hira == U'ま')) {
    looks_like_aux = true;
  }
  // Negative + 様態 そう (なさそう): the negative auxiliary ない nominalized as
  // なさ, carrying 様態 そう. Attaches to a verb stem (見なさそう = 見 + なさそう,
  // 食べなさそう = 食べ + なさそう) and is never a compound noun. This is the
  // negative counterpart of the renyokei + そう handling above, so let the
  // verb + な + さ + そう decomposition win instead of merging into one noun.
  if (hiragana_len >= 3 && hiragana_candidate.find("なさそ") != std::string::npos) {
    looks_like_aux = true;
  }
  // Renyokei + なさい (polite imperative)
  // e.g., 書きなさい, 起きなさい - these should split as verb + なさい
  const char32_t last_hira = codepoints[hiragana_end - 1];
  const char32_t before_last_hira = codepoints[hiragana_end - 2];
  if ((is_renyokei || is_ichidan_stem) && hiragana_len >= 4 && before_last_hira == U'さ' && last_hira == U'い') {
    looks_like_aux = true;
  }
  // Renyokei + べき (classical auxiliary)
  // e.g., 読むべき, 食べるべき - these should split as verb + べき
  if (hiragana_len >= 3 && before_last_hira == U'べ' && last_hira == U'き') {
    looks_like_aux = true;
  }
  // Patterns containing くださ (part of ください auxiliary)
  // e.g., 待ちくださ, 行きくださ - these should be verb + ください
  if (hiragana_len >= 3 && hiragana_candidate.find("くださ") != std::string::npos) {
    looks_like_aux = true;
  }

  // Ichidan verb pattern (e-row + る, or the imperative ろ: 慣れ+ろ)
  const bool is_e_row =
      (first_hira == U'え' || first_hira == U'け' || first_hira == U'げ' || first_hira == U'せ' ||
       first_hira == U'て' || first_hira == U'ね' || first_hira == U'べ' || first_hira == U'め' || first_hira == U'れ');
  if (is_e_row && (second_hira == U'る' || second_hira == U'ろ')) {
    looks_like_aux = true;
  }

  // Patterns ending with る
  if (last_hira == U'る') {
    looks_like_aux = true;
  }

  // Patterns ending with るそう (verb dictionary form + hearsay そう)
  // e.g., 食べるそう, 降るそう - these are verb終止形 + そう(hearsay), not compound nouns
  // Valid i-adj+そう like 美味しそう are handled separately (don't have る before そう)
  // The same holds for godan-ku/sa terminals (行くそう, 話すそう, するそう).
  if (hiragana_len >= 3 && last_hira == U'う' && before_last_hira == U'そ') {
    const char32_t terminal = codepoints[hiragana_end - 3];
    if (terminal == U'る' || terminal == U'く' || terminal == U'す') {
      looks_like_aux = true;
    }
  }

  // Patterns ending with て/で (verb te-form)
  // e.g., 基づいて, 考えて - these are verb conjugations, not compound nouns
  if (last_hira == U'て' || last_hira == U'で') {
    looks_like_aux = true;
  }

  // The past auxiliary た selects the same continuative the connective て does,
  // so a span ending in it is a finished predicate rather than a compound noun
  // (嘘じみた, 夢じみた). This is the past-tense half of the check above; the
  // fixed nouns that happen to end in the same mora (花かるた) keep an ordinary
  // candidate, only priced as the auxiliary-shaped span it looks like.
  if (last_hira == U'た') {
    looks_like_aux = true;
  }

  // Patterns opening with a conjunctive particle (見ちゃだめ, 読んじゃだめ)
  if (startsWithConjunctiveParticle(dict_manager, codepoints, kanji_end, hiragana_end)) {
    looks_like_aux = true;
  }

  // Patterns ending with お (prefix marker)
  // e.g., 一つお should be 一つ + お(PREFIX), not 一つお(NOUN)
  // お is very commonly used as honorific prefix, so it should not be absorbed
  // into compound nouns
  if (last_hira == U'お') {
    looks_like_aux = true;
  }

  // X+さ is an adjective nominalization when X is a verified adjective stem;
  // keep that morpheme boundary instead of treating the mixed-script span as
  // an opaque noun merely because a case particle follows it.
  if (isAdjectiveNominalizationSa(dict_manager, codepoints, start_pos, hiragana_end)) {
    looks_like_aux = true;
  }

  // An auxiliary stem plus a nominal suffix is that chain on the noun before it
  // (本+らし+げ, as 本+らし+さ), the same reading the adjective-stem paths take,
  // unless the adjective it spells is listed (誇らし+げ).
  for (size_t aux_end = kanji_end + 2; dict_manager != nullptr && aux_end < hiragana_end && !looks_like_aux;
       ++aux_end) {
    looks_like_aux =
        lookupEntryInRange(*dict_manager, codepoints, kanji_end, aux_end, core::PartOfSpeech::Auxiliary) != nullptr &&
        lookupEntryInRange(*dict_manager, codepoints, aux_end, hiragana_end, core::PartOfSpeech::Suffix) != nullptr &&
        !verb_helpers::isAdjectiveInDictionary(dict_manager, extractSubstring(codepoints, start_pos, aux_end) + "い");
  }

  // The same chain behind a host that carries one okurigana mora of its own
  // (頼り+な+げ): the negative adjective stem plus the nominal suffix.
  const size_t stem_end = kanji_end + 2;
  if (dict_manager != nullptr && !looks_like_aux && stem_end < hiragana_end) {
    const bool names_stem = lookupEntryInRange(*dict_manager, codepoints, kanji_end + 1, stem_end,
                                               core::PartOfSpeech::Auxiliary) != nullptr ||
                            lookupEntryInRange(*dict_manager, codepoints, kanji_end + 1, stem_end,
                                               core::PartOfSpeech::Adjective) != nullptr;
    // The stem may carry the nominalizer さ before the suffix (頼り+な+さ+げ).
    const size_t suffix_start = codepoints[stem_end] == U'さ' ? stem_end + 1 : stem_end;
    looks_like_aux =
        names_stem && suffix_start < hiragana_end &&
        lookupEntryInRange(*dict_manager, codepoints, suffix_start, hiragana_end, core::PartOfSpeech::Suffix) !=
            nullptr &&
        !verb_helpers::isAdjectiveInDictionary(dict_manager, extractSubstring(codepoints, start_pos, stem_end) + "い");
  }

  // A derivational suffix opening with a sokuon keeps its boundary after the
  // host's okurigana (飾り+っけ, as 色+っけ), so no single noun spans it.
  for (size_t suffix_pos = kanji_end + 1; dict_manager != nullptr && suffix_pos + 1 < hiragana_end; ++suffix_pos) {
    if (codepoints[suffix_pos] == U'っ' && lookupEntryInRange(*dict_manager, codepoints, suffix_pos, hiragana_end,
                                                              core::PartOfSpeech::Suffix) != nullptr) {
      return;
    }
  }

  // Skip NOUN generation for pure auxiliary patterns
  // These should always be verb stem + auxiliary, never a compound noun
  // e.g., 寝ます should be 寝(VERB) + ます(AUX), not 寝ます(NOUN)
  // ます, ない - pure polite/negative auxiliaries
  if (hiragana_len == 2 && ((first_hira == core::hiragana::kMa && second_hira == core::hiragana::kSu) ||
                            (first_hira == core::hiragana::kNa && second_hira == core::hiragana::kI))) {
    return;
  }

  // Check if the hiragana portion is a known dictionary word (exact match)
  // If so, skip compound generation to let the split path win
  // E.g., 火だるま: if だるま is in dictionary, don't generate compound
  // Only skip for exact matches - partial matches (like た in たまり) don't count
  if (dict_manager != nullptr && lookupEntryInRange(*dict_manager, codepoints, kanji_end, hiragana_end) != nullptr) {
    // This allows split like 火+だるま to win.
    return;
  }

  // Skip compound generation if the full surface is a known verb in dictionary
  // E.g., 下さい is dict verb (くださる), not compound noun
  if (verb_helpers::isVerbInDictionary(dict_manager, extractSubstring(codepoints, start_pos, hiragana_end))) {
    return;
  }

  // Skip when the hiragana portion ends in a focus particle (副助詞/係助詞)
  // tail, optionally followed by ない: 金さえない is noun + 係助詞 さえ + ない,
  // never a single compound noun. A hiragana portion that IS exactly a
  // particle (先ほど, 中ほど) was already skipped by the exact-dictionary-word
  // check above, so this only rejects particle + negative absorption blobs.
  // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
  if (verb_helpers::endsWithFocusParticleTail(dict_manager, codepoints, start_pos, hiragana_end)) {
    return;  // Skip - noun + focus particle split should win
  }

  if (strandsParticleLikeMoraBeforeNominalSelector(dict_manager, codepoints, hiragana_end)) {
    return;
  }

  // Skip when the span boundary cuts a fixed closed-class word in half
  // (読まれど|も for 読ま + れ + ども, 走るそ|の for 走る + その).
  if (boundarySplitsClosedClassWord(dict_manager, codepoints, kanji_end, hiragana_end)) {
    return;
  }

  // Generate candidate with cost based on pattern
  const float cost = looks_like_aux ? 3.5F : 1.0F;
  // A clause that ends on the span is the same nominal frame a following case
  // particle provides: nothing there can be a predicate ending, so whatever
  // occupies the position is a nominal (草むら。 alongside 草むらに). Without
  // this the identical compound would be priced as an unverified run purely
  // because the sentence stopped.
  const bool ends_clause = hiragana_end >= char_types.size() || char_types[hiragana_end] == normalize::CharType::Symbol;
  const bool nominal_context =
      !looks_like_aux && (ends_clause || hasNominalPhraseSelectorAt(dict_manager, codepoints, hiragana_end));
  auto cand = makeCandidate(
      codepoints, start_pos, hiragana_end, core::PartOfSpeech::Noun, cost, false,
      nominal_context ? CandidateOrigin::KanjiHiraganaNominalCompound : CandidateOrigin::KanjiHiraganaCompound);
  SUZUME_DEBUG_CANDIDATE(cand, looks_like_aux ? 0.3F : 0.8F,
                         looks_like_aux ? "aux_like" : (nominal_context ? "nominal_compound" : "compound"));
  candidates.push_back(cand);
}

}  // namespace suzume::analysis
