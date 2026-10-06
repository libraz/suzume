/**
 * @file adjective_candidates_na.cpp
 * @brief Kanji na-adjective candidate generation
 */

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "scorer_constants.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

using adj_detail::makeNaAdjCandidate;

namespace {

// Whether a dictionary adjective closing the run stands apart from what is in
// front of it: one bound kanji (超|重要, 最|簡単) or a quantity phrase, which
// its counter closes (何部|必要). A full noun compounds with it (交通安全).
bool hasIndependentAdjectiveHost(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                 const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1) {
    return false;
  }
  if (lookupEntryInRange(*dict_manager, codepoints, start_pos + 1, end_pos, core::PartOfSpeech::Adjective) != nullptr) {
    return true;
  }
  const bool opens_quantity_phrase =
      normalize::isNumeralCodepoint(codepoints[start_pos]) || normalize::isQuantityPrefixKanji(codepoints[start_pos]);
  return opens_quantity_phrase && hasDictionaryEntryEndingAt(*dict_manager, codepoints, start_pos + 1, end_pos,
                                                             partOfSpeechMask(core::PartOfSpeech::Adjective));
}

// The な that would be the attributive copula may instead be the first mora of
// a longer closed-class word (なら, ない, など, なし). When the dictionary can
// name one starting there, that word is the reading and the material before it
// is not a stem this rule invented an adjective for.
//
// The two callers ask for different classes, and the difference is
// grammatical rather than incidental. An auxiliary or a particle attaches to a
// stem, so 壮大+なる and 遺憾+ながら still have a na-adjective stem in front of
// them; only a mixed run needs protecting there, because its own boundary is
// what is in question. A closed-class *predicate* forms its own phrase
// instead, so it leaves an ordinary nominal to its left (勝利+なし), and so
// does a final particle, which closes the sentence on it (東京+なう).
bool startsLongerClosedForm(const std::vector<char32_t>& codepoints, size_t na_pos,
                            const dictionary::DictionaryManager* dict_manager, PartOfSpeechMask pos_mask,
                            bool include_final_particle) {
  if (dict_manager == nullptr) {
    return false;
  }
  constexpr size_t kMaxClosedFormLength = 3;
  const size_t probe_end = std::min(codepoints.size(), na_pos + kMaxClosedFormLength);
  for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, na_pos, probe_end)) {
    if (match.entry != nullptr && match.length > 1 &&
        ((pos_mask & partOfSpeechMask(match.entry->pos)) != 0 ||
         (include_final_particle && match.entry->extended_pos == core::ExtendedPOS::ParticleFinal))) {
      return true;
    }
  }
  return false;
}

constexpr PartOfSpeechMask kClosedFormAfterMixedStem =
    partOfSpeechMask(core::PartOfSpeech::Adjective) | partOfSpeechMask(core::PartOfSpeech::Auxiliary) |
    partOfSpeechMask(core::PartOfSpeech::Particle) | partOfSpeechMask(core::PartOfSpeech::Suffix);
constexpr PartOfSpeechMask kClosedFormAfterKanjiStem = partOfSpeechMask(core::PartOfSpeech::Adjective);

void generateHiraganaNariNaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                               const std::vector<normalize::CharType>& char_types,
                                               std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Hiragana) {
    return;
  }

  // -やか/-らか are productive na-adjective endings.  Before a na-adjective
  // continuation, the whole hiragana stem is an adjective (すこやかなる,
  // あきらかに), not a sequence of short verb candidates.
  // Keep the bounded scan local to one adjective-sized word so an earlier
  // hiragana adverb cannot be absorbed into the stem.
  // Every character of the stem is checked, the ones inside the minimum length
  // too: a kanji there makes the run a kanji word's okurigana (を+明らか).
  constexpr size_t kMinHiraganaNaAdjectiveLength = 3;
  constexpr size_t kMaxHiraganaNaAdjectiveLength = 6;
  for (size_t stem_end = start_pos + 1;
       stem_end <= codepoints.size() && stem_end - start_pos <= kMaxHiraganaNaAdjectiveLength; ++stem_end) {
    if (char_types[stem_end - 1] != normalize::CharType::Hiragana) {
      break;
    }
    if (stem_end - start_pos < kMinHiraganaNaAdjectiveLength) {
      continue;
    }
    if (stem_end >= codepoints.size()) {
      break;
    }
    const bool has_classical_attributive =
        stem_end + 2 <= codepoints.size() && extractSubstring(codepoints, stem_end, stem_end + 2) == "なる";
    const bool has_na_adjective_continuation = codepoints[stem_end] == U'に' || codepoints[stem_end] == U'な' ||
                                               codepoints[stem_end] == U'だ' || codepoints[stem_end] == U'で' ||
                                               codepoints[stem_end] == U'さ';
    if (!has_classical_attributive && !has_na_adjective_continuation) {
      continue;
    }
    const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
    // The classical attributive なる only ever attaches to a nominal adjective
    // stem, and the productive shape of that stem is a -か ending (しずか,
    // ゆたか, おごそか).  Before なる the wider -か shape is safe; before the
    // modern continuations it is not, because 〜かな/〜かに also occur inside
    // ordinary verb inflection, so those keep the narrow -やか/-らか test.
    const bool has_na_adjective_stem_shape =
        utf8::endsWithAny(stem, {"やか", "らか"}) || (has_classical_attributive && utf8::endsWithAny(stem, {"か"}));
    if (!has_na_adjective_stem_shape) {
      continue;
    }
    candidates.push_back(makeNaAdjCandidate(stem, start_pos, stem_end, candidate::kNaAdjYakaCost, true,
                                            CandidateOrigin::AdjectiveNa, candidate::kHiraganaNaAdjNariConfidence,
                                            "hira_na_adj_yaka_raka_nari"));
    return;
  }
}

// A bare hiragana stem before the attributive copula (うぶ+な+人, ふつう+な+ん+よ)
// is a nominal adjective: a verb terminal takes the prohibitive な there, which
// never heads a noun or the explanatory ん/の. The な must be followed by such
// a head, which keeps なんて/なんか/なら and the prohibitive な+よ out.
void generateHiraganaAttributiveNaStemCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                 const std::vector<normalize::CharType>& char_types,
                                                 const dictionary::DictionaryManager* dict_manager,
                                                 std::vector<UnknownCandidate>& candidates) {
  constexpr size_t kMaxStemLength = 4;
  for (size_t stem_end = start_pos + 2; stem_end < codepoints.size() && stem_end - start_pos <= kMaxStemLength;
       ++stem_end) {
    if (char_types[stem_end - 1] != normalize::CharType::Hiragana) {
      return;
    }
    if (codepoints[stem_end] != U'な' || stem_end + 1 >= codepoints.size()) {
      continue;
    }
    const size_t head = stem_end + 1;
    const bool nominal_head =
        char_types[head] == normalize::CharType::Kanji || char_types[head] == normalize::CharType::Katakana;
    const bool explanatory_head =
        (codepoints[head] == U'ん' || codepoints[head] == U'の') && head + 1 < codepoints.size() &&
        (codepoints[head + 1] == U'だ' || codepoints[head + 1] == U'で' || codepoints[head + 1] == U'じ' ||
         codepoints[head + 1] == U'よ' || codepoints[head + 1] == U'ね');
    if (!nominal_head && !explanatory_head) {
      continue;
    }
    if (dict_manager == nullptr) {
      return;
    }
    // A registered word is not re-read as a coined stem, and neither is a
    // registered word plus a particle (それ+は+なんで) or a bound suffix
    // (らし+げ+な), nor a particle plus a registered word (が+これ+なんです).
    const auto stem_matches = lookupResultsInRange(*dict_manager, codepoints, start_pos, stem_end);
    const bool registered_stem = std::any_of(stem_matches.begin(), stem_matches.end(), [&](const auto& match) {
      return match.entry != nullptr && match.length == stem_end - start_pos;
    });
    bool splits_at_particle = false;
    for (size_t split = start_pos + 1; split < stem_end && !splits_at_particle; ++split) {
      const bool word_then_particle =
          lookupEntryInRange(*dict_manager, codepoints, start_pos, split) != nullptr &&
          (lookupEntryInRange(*dict_manager, codepoints, split, stem_end, core::PartOfSpeech::Particle) != nullptr ||
           lookupEntryInRange(*dict_manager, codepoints, split, stem_end, core::PartOfSpeech::Suffix) != nullptr);
      const bool particle_then_word =
          lookupEntryInRange(*dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Particle) != nullptr &&
          lookupEntryInRange(*dict_manager, codepoints, split, stem_end) != nullptr;
      splits_at_particle = word_then_particle || particle_then_word;
    }
    if (registered_stem || splits_at_particle) {
      return;
    }
    const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
    candidates.push_back(makeNaAdjCandidate(stem, start_pos, stem_end, candidate::kNaAdjYakaCost, true,
                                            CandidateOrigin::AdjectiveNa, candidate::kHiraganaNaAdjNariConfidence,
                                            "hira_na_adj_attributive"));
    return;
  }
}

}  // namespace

void generateNaAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                   const std::vector<normalize::CharType>& char_types,
                                   const UnknownOptions& /*options*/, const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= char_types.size()) {
    return;
  }
  if (char_types[start_pos] == normalize::CharType::Hiragana) {
    generateHiraganaNariNaAdjectiveCandidates(codepoints, start_pos, char_types, candidates);
    generateHiraganaAttributiveNaStemCandidates(codepoints, start_pos, char_types, dict_manager, candidates);
    return;
  }
  if (char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // The attributive copula can license a productive compound stem longer than
  // three kanji (非現実的な, 再利用可能な).  Keep the scan bounded to one
  // content-word-sized run, but do not truncate the very evidence needed to
  // recognize the right boundary.
  constexpr size_t kMaxNaAdjKanjiLength = 6;
  const size_t kanji_end = findCharRegionEnd(char_types, start_pos, kMaxNaAdjKanjiLength, normalize::CharType::Kanji);
  const size_t kanji_len = kanji_end - start_pos;

  // Pattern 0: Kanji(1) + やか/らか + na-adjective inflection. These productive
  // derivatives can be followed by attributive な, adverbial に, or a copula
  // form (e.g., 華やかな, 明らかになる, 安らかだった).
  if (kanji_len == 1 && kanji_end < char_types.size() && char_types[kanji_end] == normalize::CharType::Hiragana) {
    size_t stem_end = kanji_end + 2;
    if (stem_end < codepoints.size()) {
      std::string stem_suffix = extractSubstring(codepoints, kanji_end, stem_end);
      bool is_yaka_pattern = utf8::equalsAny(stem_suffix, {"やか", "らか"});
      bool has_na_adj_continuation = codepoints[stem_end] == U'な' || codepoints[stem_end] == U'に' ||
                                     codepoints[stem_end] == U'だ' || codepoints[stem_end] == U'で' ||
                                     codepoints[stem_end] == U'さ';
      if (is_yaka_pattern && has_na_adj_continuation) {
        std::string stem = extractSubstring(codepoints, start_pos, stem_end);
        candidates.push_back(makeNaAdjCandidate(stem, start_pos, stem_end, candidate::kNaAdjYakaCost, true,
                                                CandidateOrigin::AdjectiveNa, candidate::kHiraganaNaAdjNariConfidence,
                                                "na_adj_yaka_raka"));
        return;
      }
    }
  }

  // A mixed kanji-hiragana stem immediately followed by attributive な has
  // the same grammatical evidence as a kanji-only stem.  Preserve the maximum
  // stem (気まぐれ+な, 気まま+な), while rejecting an internal particle such
  // as 山+の+よう+な.  Bare copula だ is deliberately excluded because it
  // cannot distinguish an ordinary nominal predicate from a na-adjective.
  if (kanji_end < char_types.size() && char_types[kanji_end] == normalize::CharType::Hiragana) {
    // The productive plural suffix ら and the na-adjective stem ending ら are
    // homographic before the copula (彼らだ / 平らだ).  A pronoun host licenses
    // the suffix reading; otherwise the explicit copula supplies the missing
    // predicative evidence for the mixed na-adjective stem.
    if (kanji_end + 1 < codepoints.size() && codepoints[kanji_end] == U'ら' && codepoints[kanji_end + 1] == U'だ') {
      const auto* pronoun = dict_manager == nullptr ? nullptr
                                                    : lookupEntryInRange(*dict_manager, codepoints, start_pos,
                                                                         kanji_end, core::PartOfSpeech::Pronoun);
      const auto* copula =
          dict_manager == nullptr ? nullptr : dict_manager->lookupExact("だ", core::PartOfSpeech::Auxiliary);
      if (pronoun == nullptr && copula != nullptr && copula->extended_pos == core::ExtendedPOS::AuxCopulaDa) {
        const size_t stem_end = kanji_end + 1;
        const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
        candidates.push_back(makeNaAdjCandidate(stem, start_pos, stem_end, candidate::kNaAdjStemCost, true,
                                                CandidateOrigin::AdjectiveNa, candidate::kNaAdjPredicateConfidence,
                                                "mixed_ra_na_adjective_predicate"));
      }
    }
    constexpr size_t kMaxMixedNaAdjHiraganaLength = 4;
    size_t stem_end = kanji_end;
    bool has_internal_particle = false;
    while (stem_end < codepoints.size() && stem_end - kanji_end < kMaxMixedNaAdjHiraganaLength &&
           char_types[stem_end] == normalize::CharType::Hiragana) {
      if (codepoints[stem_end] == U'な') {
        const bool starts_longer_closed_form =
            startsLongerClosedForm(codepoints, stem_end, dict_manager, kClosedFormAfterMixedStem, false);
        const bool is_bare_attributive = stem_end > kanji_end && !starts_longer_closed_form &&
                                         (stem_end + 1 >= codepoints.size() ||
                                          (codepoints[stem_end + 1] != U'ら' && codepoints[stem_end + 1] != U'の' &&
                                           codepoints[stem_end + 1] != U'い' && codepoints[stem_end + 1] != U'く' &&
                                           codepoints[stem_end + 1] != U'か' && codepoints[stem_end + 1] != U'さ'));
        bool contains_closed_suffix = false;
        bool starts_closed_tail = false;
        if (dict_manager != nullptr) {
          for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, kanji_end, stem_end)) {
            if (match.entry != nullptr &&
                (match.entry->pos == core::PartOfSpeech::Auxiliary || match.entry->pos == core::PartOfSpeech::Suffix ||
                 match.entry->pos == core::PartOfSpeech::Particle)) {
              contains_closed_suffix = contains_closed_suffix || match.length == stem_end - kanji_end;
              starts_closed_tail = starts_closed_tail || match.entry->extended_pos == core::ExtendedPOS::AuxCopulaDa ||
                                   match.entry->extended_pos == core::ExtendedPOS::AuxCopulaDesu;
            }
          }
          // An auxiliary stem plus a nominal suffix on a compound noun is that
          // chain, not a stem (子供+らし+げ+な), as the i-adjective stem path holds.
          for (size_t aux_end = kanji_end + 2; aux_end < stem_end && kanji_end >= start_pos + 2; ++aux_end) {
            contains_closed_suffix =
                contains_closed_suffix || (lookupEntryInRange(*dict_manager, codepoints, kanji_end, aux_end,
                                                              core::PartOfSpeech::Auxiliary) != nullptr &&
                                           lookupEntryInRange(*dict_manager, codepoints, aux_end, stem_end,
                                                              core::PartOfSpeech::Suffix) != nullptr);
          }
        }
        const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
        // A te-form chain is not a nominal stem (食べ+て+み+な): the な after
        // it is the imperative particle.
        bool crosses_te_form = false;
        for (size_t tail = kanji_end; tail < stem_end; ++tail) {
          crosses_te_form = crosses_te_form || codepoints[tail] == U'て' || codepoints[tail] == U'で';
        }
        const bool is_exact_verb_stem =
            dict_manager != nullptr && dict_manager->lookupExact(stem, core::PartOfSpeech::Verb) != nullptr;
        bool contains_passive_boundary = false;
        if (dict_manager != nullptr) {
          for (size_t auxiliary_start = kanji_end + 1; auxiliary_start < stem_end; ++auxiliary_start) {
            if (!kana::isARowCodepoint(codepoints[auxiliary_start - 1])) {
              continue;
            }
            const auto* auxiliary =
                lookupEntryInRange(*dict_manager, codepoints, auxiliary_start, stem_end, core::PartOfSpeech::Auxiliary);
            if (auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxPassive) {
              contains_passive_boundary = true;
              break;
            }
          }
        }
        const bool starts_naru_after_ku =
            stem_end > start_pos && codepoints[stem_end - 1] == U'く' && stem_end + 1 < codepoints.size() &&
            utf8::equalsAny(extractSubstring(codepoints, stem_end + 1, stem_end + 2), {"る", "っ", "り", "れ", "ろ"});
        // A 形容動詞 stem is nominal, and no nominal closes on the verbal
        // ending る — the ichidan and サ変 terminal cells do (食べる, 見る,
        // 絶望する), and the な after one is the prohibitive final particle,
        // not the attributive copula. Without this the reading depends on
        // whether the base happens to be listed: 走るな and 忘れるな parse
        // correctly only because their verbs are, while 食べるな and 見るな
        // become an invented adjective plus a copula. The stems this rule has
        // to leave alone end in か, ら or や (静かな, 平らな, 気さくな), none
        // of which is a verbal ending.
        // A final particle between the predicate and な is not part of any stem
        // either (来るよ+な, 来たよ+な, 来るわ+な): the predicate it closes is
        // checked through it, where any u-row terminal or た/だ ends one.
        const auto* final_particle =
            dict_manager == nullptr || stem_end - 1 <= kanji_end
                ? nullptr
                : lookupEntryInRange(*dict_manager, codepoints, stem_end - 1, stem_end, core::PartOfSpeech::Particle);
        const bool after_final_particle =
            final_particle != nullptr && final_particle->extended_pos == core::ExtendedPOS::ParticleFinal;
        const size_t predicate_end = after_final_particle ? stem_end - 1 : stem_end;
        const char32_t predicate_tail = codepoints[predicate_end - 1];
        // The terminal る contracts to ん before the prohibitive な (忘れん+な);
        // only a bare kanji keeps ん as a stem ending (盛んな).
        const bool closes_on_verbal_ru =
            predicate_tail == U'る' || (predicate_tail == U'ん' && predicate_end - 1 > kanji_end) ||
            (after_final_particle &&
             (kana::isURowCodepoint(predicate_tail) || predicate_tail == U'た' || predicate_tail == U'だ'));
        if (is_bare_attributive && !has_internal_particle && !contains_closed_suffix && !starts_closed_tail &&
            !is_exact_verb_stem && !crosses_te_form && !contains_passive_boundary && !starts_naru_after_ku &&
            !closes_on_verbal_ru) {
          if (!normalize::isFormalNounSurface(normalize::encodeUtf8(codepoints[start_pos]))) {
            candidates.push_back(makeNaAdjCandidate(stem, start_pos, stem_end, candidate::kNaAdjStemCost, true,
                                                    CandidateOrigin::AdjectiveNa, candidate::kNaAdjPredicateConfidence,
                                                    "mixed_na_adjective_stem"));
          }
        }
        break;
      }
      has_internal_particle = has_internal_particle || normalize::isParticleCodepoint(codepoints[stem_end]);
      ++stem_end;
    }
  }

  // Productive kanji-only patterns need at least two kanji.  A bare copula
  // does not distinguish a one-kanji na-adjective from an ordinary nominal
  // predicate, so generating an adjective there would turn 本だ, 水だ, and
  // other common noun predicates into adjectives.  Mixed stems such as
  // 平らだ are handled by the preceding kanji+hiragana rule.
  // The attributive な before a nominal head is different: a noun modifies
  // through の, so 変+な+やつ and 急+な+話 can only be the adjective.
  if (kanji_len < 2) {
    const size_t head = kanji_end + 1;
    const auto nominal_word_starts = [&]() {
      constexpr size_t kHeadProbeChars = 3;
      // A formal noun takes a nominal predicate's な as well (本+な+わけ).
      return hasDictionaryEntryFrom(dict_manager, codepoints, head, 1, kHeadProbeChars, core::PartOfSpeech::Noun,
                                    [](const dictionary::DictionaryEntry& entry) {
                                      return entry.extended_pos != core::ExtendedPOS::NounFormal;
                                    }) ||
             hasDictionaryEntryFrom(dict_manager, codepoints, head, 1, kHeadProbeChars, core::PartOfSpeech::Pronoun,
                                    nullptr);
    };
    // Inside a longer kanji run (直接的, 再利用可能) the kanji is a suffix, not a stem.
    // A na-adjective-forming suffix (なし崩し+的) attaches rather than heads.
    const std::string one_kanji = extractSubstring(codepoints, start_pos, kanji_end);
    const auto& na_suffixes = getNaAdjSuffixes();
    const bool opens_kanji_run = (start_pos == 0 || char_types[start_pos - 1] != normalize::CharType::Kanji) &&
                                 std::none_of(na_suffixes.begin(), na_suffixes.end(), [&](const auto& suffix) {
                                   return std::string_view(suffix) == one_kanji;
                                 });
    const bool attributive_before_nominal =
        dict_manager != nullptr && opens_kanji_run && head < codepoints.size() && codepoints[kanji_end] == U'な' &&
        !startsLongerClosedForm(codepoints, kanji_end, dict_manager, kClosedFormAfterMixedStem, true) &&
        (char_types[head] == normalize::CharType::Kanji || char_types[head] == normalize::CharType::Katakana ||
         nominal_word_starts());
    if (attributive_before_nominal) {
      candidates.push_back(makeNaAdjCandidate(one_kanji, start_pos, kanji_end, candidate::kNaAdjYakaCost, true,
                                              CandidateOrigin::AdjectiveNa, candidate::kHiraganaNaAdjNariConfidence,
                                              "na_adjective_one_kanji"));
    }
    return;
  }

  const std::string kanji_seq = extractSubstring(codepoints, start_pos, kanji_end);

  // Pattern 1: Check for na-adjective suffixes (的)
  // Keep X+的 as one tokenizer search unit while preserving its na-adjective
  // class.  A bare noun path remains available for contexts that do not
  // license the derived adjective.
  for (const auto& suffix : getNaAdjSuffixes()) {
    if (utf8::endsWith(kanji_seq, suffix)) {
      candidates.push_back(makeNaAdjCandidate(kanji_seq, start_pos, kanji_end, candidate::kNaAdjTekiCost, true,
                                              CandidateOrigin::AdjectiveNa, 1.0F, "na_adjective_teki"));
      break;
    }
  }

  // Pattern 2: Check for kanji compound + na-adjective continuation (e.g., 獰猛な, 変だ).
  // A bare copula cannot license an arbitrary multi-kanji unknown: nominal
  // predicates such as 学生だ are much more common, and the noun candidate is
  // the grammatically neutral analysis. The one-kanji ambiguity remains
  // useful for open-class predicates such as 変だ.
  // A bare な licenses an attributive na-adjective stem, but なら does not:
  // nouns and na-adjectives both take conditional なら, so generating an
  // adjective for every unknown kanji compound would destroy that ambiguity.
  // The classical copula なり and the nominalizer なの are ambiguous in the same
  // way and are excluded for the same reason: 体言+なり is the nominal predicate
  // and stem+なり is the classical adjective's terminal form, so the mora after
  // な decides nothing and the neutral nominal reading stands.
  // The mixed-stem rule above asks the dictionary the same question rather
  // than listing the morae, and the kanji-only rule needs it for the literary
  // predicate なし: 勝利なしとは is 勝利 + なし, not a stem plus the copula.
  const bool followed_by_na =
      kanji_end < codepoints.size() && codepoints[kanji_end] == U'な' &&
      !startsLongerClosedForm(codepoints, kanji_end, dict_manager, kClosedFormAfterKanjiStem, true) &&
      (kanji_end + 1 >= codepoints.size() ||
       (codepoints[kanji_end + 1] != U'ら' && codepoints[kanji_end + 1] != U'の' &&
        codepoints[kanji_end + 1] != U'り'));
  const bool followed_by_sou =
      kanji_end + 1 < codepoints.size() && codepoints[kanji_end] == U'そ' && codepoints[kanji_end + 1] == U'う';
  // A dictionary adjective after one bound kanji is the predicate head, so
  // that prefix must not be swallowed by the generic "all kanji before な"
  // fallback (超|重要な, 最|簡単な). Productive negation compounds are
  // licensed as new adjective units by their prefix semantics (不十分な), and
  // keep the existing compound reading.
  const bool has_independent_adjective_host =
      hasIndependentAdjectiveHost(codepoints, start_pos, kanji_end, dict_manager);
  const bool is_productive_negation_compound = scorer::startsWithNegationPrefix(kanji_seq);
  // A noun in front of a dictionary na-adjective head compounds with it, and
  // that attested head also licenses the predicative copula (利用可能だ).
  const bool has_dictionary_head = dict_manager != nullptr && kanji_end >= start_pos + 3 &&
                                   hasDictionaryEntryEndingAt(*dict_manager, codepoints, start_pos + 2, kanji_end,
                                                              partOfSpeechMask(core::PartOfSpeech::Adjective));
  const bool followed_by_da = kanji_end < codepoints.size() && codepoints[kanji_end] == U'だ';
  if ((followed_by_na || followed_by_sou || (has_dictionary_head && followed_by_da)) &&
      (!has_independent_adjective_host || is_productive_negation_compound)) {
    // Skip if first character is a formal noun (形式名詞)
    // e.g., 時妙な should be 時+妙な, not 時妙(ADJ)+な
    // Formal nouns (時, 事, 所, etc.) are standalone grammatical words
    if (normalize::isFormalNounSurface(normalize::encodeUtf8(codepoints[start_pos]))) {
      return;
    }

    // Skip if kanji ends with 的 - MeCab splits as NOUN + 的(SUFFIX) + な
    // e.g., 論理的な should be 論理+的+な, not 論理的+な
    if (codepoints[kanji_end - 1] == U'的') {
      return;
    }

    // Skip if な is followed by く/い/か — these indicate ない (auxiliary/adjective)
    // attached to the preceding noun, not a な-adjective stem.
    // Examples:
    //   私心なく → 私心 + ない連用 (not 私心(ADJ_NA) + く)
    //   仕方ない → 仕方 + ない (not 仕方(ADJ_NA) + い)
    //   関係なかった → 関係 + なかっ (か triggers naかった past form)
    // Real な-adjectives followed by these forms (静かなく) are not standard Japanese.
    if (followed_by_na && kanji_end + 1 < codepoints.size() &&
        (codepoints[kanji_end + 1] == U'く' || codepoints[kanji_end + 1] == U'い' ||
         codepoints[kanji_end + 1] == U'か')) {
      return;
    }

    // Found kanji compound + な - potential na-adjective stem
    // Cost similar to dictionary na-adjectives but with small penalty for unknown
    const float cost = has_dictionary_head ? candidate::kNaAdjHeadedCompoundCost : candidate::kNaAdjStemCost;
    candidates.push_back(makeNaAdjCandidate(kanji_seq, start_pos, kanji_end, cost, true, CandidateOrigin::AdjectiveNa,
                                            0.8F, "na_adjective_stem"));
  }
}

}  // namespace suzume::analysis
