/**
 * @file adjective_candidates_na.cpp
 * @brief Kanji na-adjective candidate generation
 */

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
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
using verb_helpers::findCharRegionEnd;

namespace {

bool hasIndependentAdjectiveHost(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                 const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1) {
    return false;
  }
  return hasDictionaryEntryEndingAt(*dict_manager, codepoints, start_pos + 1, end_pos,
                                    partOfSpeechMask(core::PartOfSpeech::Adjective));
}

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
  constexpr size_t kMaxHiraganaNaAdjectiveLength = 6;
  for (size_t stem_end = start_pos + 3;
       stem_end <= codepoints.size() && stem_end - start_pos <= kMaxHiraganaNaAdjectiveLength; ++stem_end) {
    if (char_types[stem_end - 1] != normalize::CharType::Hiragana) {
      break;
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
  return;
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
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, kMaxNaAdjKanjiLength, normalize::CharType::Kanji);

  size_t kanji_len = kanji_end - start_pos;

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
        bool starts_longer_closed_form = false;
        if (dict_manager != nullptr) {
          const std::string continuation =
              extractSubstring(codepoints, stem_end, std::min(codepoints.size(), stem_end + static_cast<size_t>(3)));
          for (const auto& match : dict_manager->lookup(continuation, 0)) {
            if (match.entry != nullptr && match.length > 1 &&
                (match.entry->pos == core::PartOfSpeech::Adjective ||
                 match.entry->pos == core::PartOfSpeech::Auxiliary ||
                 match.entry->pos == core::PartOfSpeech::Particle || match.entry->pos == core::PartOfSpeech::Suffix)) {
              starts_longer_closed_form = true;
              break;
            }
          }
        }
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
        }
        const std::string stem = extractSubstring(codepoints, start_pos, stem_end);
        const bool is_exact_verb_stem =
            dict_manager != nullptr && dict_manager->lookupExact(stem, core::PartOfSpeech::Verb) != nullptr;
        bool contains_passive_boundary = false;
        if (dict_manager != nullptr) {
          for (size_t auxiliary_start = kanji_end + 1; auxiliary_start < stem_end; ++auxiliary_start) {
            if (!grammar::isARowCodepoint(codepoints[auxiliary_start - 1])) {
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
        const bool closes_on_verbal_ru = stem_end > start_pos && codepoints[stem_end - 1] == U'る';
        if (is_bare_attributive && !has_internal_particle && !contains_closed_suffix && !starts_closed_tail &&
            !is_exact_verb_stem && !contains_passive_boundary && !starts_naru_after_ku && !closes_on_verbal_ru) {
          std::string first_char_str;
          normalize::encodeUtf8(codepoints[start_pos], first_char_str);
          if (!normalize::isFormalNounSurface(first_char_str)) {
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
  if (kanji_len < 2) {
    return;
  }

  std::string kanji_seq = extractSubstring(codepoints, start_pos, kanji_end);

  // Pattern 1: Check for na-adjective suffixes (的)
  // Keep X+的 as one tokenizer search unit while preserving its na-adjective
  // class.  A bare noun path remains available for contexts that do not
  // license the derived adjective.
  for (const auto& suffix : getNaAdjSuffixes()) {
    if (kanji_seq.size() >= suffix.size()) {
      std::string_view kanji_suffix(kanji_seq.data() + kanji_seq.size() - suffix.size(), suffix.size());
      if (kanji_suffix == suffix) {
        candidates.push_back(makeNaAdjCandidate(kanji_seq, start_pos, kanji_end, candidate::kNaAdjTekiCost, true,
                                                CandidateOrigin::AdjectiveNa, 1.0F, "na_adjective_teki"));
        break;
      }
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
  const bool followed_by_na = kanji_end < codepoints.size() && codepoints[kanji_end] == U'な' &&
                              (kanji_end + 1 >= codepoints.size() ||
                               (codepoints[kanji_end + 1] != U'ら' && codepoints[kanji_end + 1] != U'の' &&
                                codepoints[kanji_end + 1] != U'り'));
  const bool followed_by_sou =
      kanji_end + 1 < codepoints.size() && codepoints[kanji_end] == U'そ' && codepoints[kanji_end + 1] == U'う';
  // Productive X+可能 is a capability noun compound whose following な is
  // the attributive copula (再利用可能な, 使用可能な).  Preserve an exact
  // lexical adjective such as 不可能, but do not let the generic "all-kanji
  // before な" fallback reclassify every open left-hand compound as AdjNa.
  const bool is_unlexicalized_capability_compound =
      kanji_len > 2 && utf8::endsWith(kanji_seq, "可能") &&
      (dict_manager == nullptr || dict_manager->lookupExact(kanji_seq, core::PartOfSpeech::Adjective) == nullptr);
  // An internal dictionary adjective is the predicate head, so a preceding
  // productive prefix must not be swallowed by the generic "all kanji before
  // な" fallback (超|重要な, 最|簡単な). Productive negation compounds are
  // licensed as new adjective units by their prefix semantics (不十分な), and
  // keep the existing compound reading.
  const bool has_independent_adjective_host =
      hasIndependentAdjectiveHost(codepoints, start_pos, kanji_end, dict_manager);
  const bool is_productive_negation_compound = scorer::startsWithNegationPrefix(kanji_seq);
  if (is_unlexicalized_capability_compound) {
    auto capability_noun = makeCandidate(kanji_seq, start_pos, kanji_end, core::PartOfSpeech::Noun,
                                         candidate::kNaAdjStemCost, true, CandidateOrigin::SuffixPattern);
    capability_noun.lemma = kanji_seq;
#ifdef SUZUME_DEBUG_INFO
    capability_noun.confidence = candidate::kDictionaryOriginConfidence;
    capability_noun.pattern = "capability_noun_compound";
#endif
    candidates.push_back(std::move(capability_noun));
  }
  if ((followed_by_na || followed_by_sou) && !is_unlexicalized_capability_compound &&
      (!has_independent_adjective_host || is_productive_negation_compound)) {
    // Skip if first character is a formal noun (形式名詞)
    // e.g., 時妙な should be 時+妙な, not 時妙(ADJ)+な
    // Formal nouns (時, 事, 所, etc.) are standalone grammatical words
    std::string first_char_str;
    normalize::encodeUtf8(codepoints[start_pos], first_char_str);
    if (normalize::isFormalNounSurface(first_char_str)) {
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
    if (followed_by_na && kanji_end + 1 < codepoints.size()) {
      char32_t after_na = codepoints[kanji_end + 1];
      if (after_na == U'く' || after_na == U'い' || after_na == U'か') {
        return;
      }
    }

    // Found kanji compound + な - potential na-adjective stem
    // Cost similar to dictionary na-adjectives but with small penalty for unknown
    candidates.push_back(makeNaAdjCandidate(kanji_seq, start_pos, kanji_end, candidate::kNaAdjStemCost, true,
                                            CandidateOrigin::AdjectiveNa, 0.8F, "na_adjective_stem"));
  }

  return;
}

}  // namespace suzume::analysis
