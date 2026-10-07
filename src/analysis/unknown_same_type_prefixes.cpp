/**
 * @file unknown_same_type_prefixes.cpp
 * @brief Per-length candidates of a same-type run
 *
 * Emits one candidate for every prefix of the run that survives the boundary
 * filters: a prefix whose end or interior crosses a word boundary proven by
 * the dictionary or by grammar is skipped, and the rest are priced by shape.
 */

#include <algorithm>
#include <array>
#include <string_view>

#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/unknown.h"
#include "analysis/unknown_same_type_internal.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis {

namespace {

using same_type_detail::isRightBoundaryParticle;
using same_type_detail::opensOnParticleBeforePronoun;

// Whether [start, end) is an opaque run that closes on a registered kanji
// pronoun after a registered noun (結局+皆). A pronoun is a phrase of its own, so
// no unregistered run spans it and the noun in front of it. A left side that is
// no registered word (暴+君) leaves the run alone.
bool closesOnPronounAfterRegisteredNoun(const dictionary::DictionaryManager& dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  for (size_t pronoun_start = start + 2; pronoun_start < end; ++pronoun_start) {
    if (lookupEntryInRange(dict_manager, codepoints, pronoun_start, end, core::PartOfSpeech::Pronoun) != nullptr &&
        lookupEntryInRange(dict_manager, codepoints, start, pronoun_start, core::PartOfSpeech::Noun) != nullptr) {
      return true;
    }
  }
  return false;
}

// Whether a registered multi-kanji interjection opens or closes [start, end)
// beside more kanji (前略+失礼). An interjection stands outside the clause it
// opens or closes, so no unregistered run spans it and its neighbour.
bool spansInterjectionBoundary(const dictionary::DictionaryManager& dict_manager,
                               const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  const auto registered_interjection = [&](size_t from, size_t to) {
    return to > from + 1 &&
           lookupEntryInRange(dict_manager, codepoints, from, to, core::PartOfSpeech::Interjection) != nullptr;
  };
  for (size_t split = start + 2; split + 1 < end; ++split) {
    if (registered_interjection(start, split)) {
      return true;
    }
  }
  for (size_t split = start + 1; split + 2 < end; ++split) {
    if (registered_interjection(split, end)) {
      return true;
    }
  }
  return false;
}

// Whether [start, end) splits into registered auxiliaries, the last of which
// may run past end.
bool spellsAuxiliaryChain(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                          size_t start, size_t end) {
  std::vector<bool> reachable(end - start + 1, false);
  reachable[0] = true;
  for (size_t from = start; from < end; ++from) {
    if (!reachable[from - start]) {
      continue;
    }
    for (size_t to = from + 1; to <= end; ++to) {
      if (!reachable[to - start] &&
          lookupEntryInRange(dict_manager, codepoints, from, to, core::PartOfSpeech::Auxiliary) != nullptr) {
        reachable[to - start] = true;
      }
    }
  }
  if (reachable[end - start]) {
    return true;
  }
  // A run cut short inside an auxiliary (ま of ます) duplicates it as well.
  constexpr size_t kMaxAuxiliaryChars = 8;
  for (size_t from = start; from < end; ++from) {
    if (reachable[from - start] && hasDictionaryEntryFrom(&dict_manager, codepoints, from, end - from + 1,
                                                          kMaxAuxiliaryChars, core::PartOfSpeech::Auxiliary, nullptr)) {
      return true;
    }
  }
  return false;
}

bool startsClosedNativeNumber(const std::vector<char32_t>& codepoints, size_t pos) {
  constexpr std::array<std::u32string_view, 10> kClosedNativeNumbers{{U"ひとつ", U"ふたつ", U"みっつ", U"よっつ",
                                                                      U"いつつ", U"むっつ", U"ななつ", U"やっつ",
                                                                      U"ここのつ", U"とおのつ"}};
  return std::any_of(kClosedNativeNumbers.begin(), kClosedNativeNumbers.end(), [&](std::u32string_view word) {
    return pos + word.size() <= codepoints.size() && std::equal(word.begin(), word.end(), codepoints.begin() + pos);
  });
}

// A generated hiragana noun cannot consist solely of two or more closed
// particles. The dynamic program preserves a complete multi-mora particle as
// one grammatical unit while rejecting accidental noun rescue paths such as
// へ+と. Lexical dictionary readings remain separate candidates.
bool decomposesIntoMultipleParticles(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                     const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1) {
    return false;
  }
  return maximalSegmentCount(*dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Particle) >= 2;
}

bool isFollowedByNominalParticle(const std::vector<char32_t>& codepoints, size_t end_pos,
                                 const dictionary::DictionaryManager* dict_manager) {
  // Longest nominal-selecting particle in the closed class is three codepoints.
  constexpr size_t kParticleProbe = 3;
  return hasDictionaryEntryFrom(
      dict_manager, codepoints, end_pos, 1, kParticleProbe, core::PartOfSpeech::Particle,
      [](const dictionary::DictionaryEntry& entry) { return isNominalForcingParticle(entry.extended_pos); });
}

// A one-kanji formal noun followed by an attributive na-adjective is a word
// boundary (この時妙なもの, その事不思議な結末). The whole kanji run determines
// this boundary so that shorter fabricated prefixes such as 時不 do not evade it.
bool hasFormalNounNaAdjectiveBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                      normalize::CharType start_type,
                                      const dictionary::DictionaryManager* dict_manager) {
  if (start_type != normalize::CharType::Kanji || kanji_end <= start_pos + 1 || kanji_end >= codepoints.size() ||
      codepoints[kanji_end] != U'な' || verb_helpers::opensNonCopularParticleAt(dict_manager, codepoints, kanji_end)) {
    return false;
  }
  if (kanji_end + 1 < codepoints.size() && codepoints[kanji_end + 1] == U'ら') {
    return false;
  }

  std::string first_char;
  normalize::encodeUtf8(codepoints[start_pos], first_char);
  return normalize::isFormalNounSurface(first_char);
}

// A hiragana run may begin on the okurigana of the preceding kanji verb
// continuative. That ownership is evidence against reusing the mora as the head
// of a longer particle-final unknown word before a nominal selector
// (組み|ひも|を, not 組|みひも|を). Both productive paradigms are reconstructed
// from their final mora.
bool startsAtDictionaryVerbContinuative(const std::vector<char32_t>& codepoints,
                                        const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                        const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0 || start_pos >= codepoints.size() ||
      char_types[start_pos - 1] != normalize::CharType::Kanji) {
    return false;
  }
  return verb_helpers::namesDictionaryVerbContinuative(dict_manager, codepoints, start_pos);
}

// A same-type run may begin at the final okurigana of a dictionary predicate of
// @p pos that opens on the preceding kanji. For a terminal verb (書く+だべ), a
// following closed auxiliary belongs to that verb and cannot be absorbed into
// the run; for an adjective (静か+な), the copular cell stays available instead
// of promoting the tail to an unknown noun. The probe is limited to the local
// candidate window used by this generator.
bool startsAfterDictionaryPredicate(const std::vector<char32_t>& codepoints,
                                    const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                    size_t run_end, const dictionary::DictionaryManager* dict_manager,
                                    core::PartOfSpeech pos) {
  if (dict_manager == nullptr || start_pos == 0 || start_pos >= run_end ||
      char_types[start_pos - 1] != normalize::CharType::Kanji) {
    return false;
  }
  return hasDictionaryEntryFrom(dict_manager, codepoints, start_pos - 1, 2, run_end - start_pos + 1, pos, nullptr);
}

// Whether the kanji just before @p kana_start heads a predicate whose
// okurigana starts at @p kana_start (外|飲み|たい, 地|固まる, 駅|遠い). A
// registered verb gives the proof directly; otherwise the inflection analyzer
// must read the kanji plus a prefix of the kana run as a verb or i-adjective
// on that one-kanji stem.
bool kanjiHeadsPredicateAt(const std::vector<char32_t>& codepoints, const std::vector<normalize::CharType>& char_types,
                           size_t kana_start, const grammar::Inflection& inflection,
                           const dictionary::DictionaryManager& dict_manager) {
  constexpr size_t kPredicateProbe = 4;
  size_t kana_end = kana_start;
  while (kana_end < codepoints.size() && kana_end < kana_start + kPredicateProbe &&
         char_types[kana_end] == normalize::CharType::Hiragana) {
    ++kana_end;
  }
  const auto opening_entry = [&](size_t length, core::PartOfSpeech pos) {
    return lookupEntryInRange(dict_manager, codepoints, kana_start, kana_start + length, pos);
  };
  // A registered content word or interjection of three kana or more opening
  // on the kana leaves the kanji to the run (助言|いただく, 質問|うざい,
  // 学生|いかに, 合格|おめでとう). The word may run past the predicate probe.
  // Two-kana verbs are weighed after the direct proofs below, because they
  // are also spelled by the okurigana of a predicate (遠+いけ+ど, 鳴+かむ).
  constexpr size_t kOpeningContentWordMinLength = 3;
  constexpr size_t kOpeningContentWordProbe = 8;
  size_t word_probe_end = kana_start;
  while (word_probe_end < codepoints.size() && word_probe_end < kana_start + kOpeningContentWordProbe &&
         char_types[word_probe_end] == normalize::CharType::Hiragana) {
    ++word_probe_end;
  }
  for (const auto& match : lookupResultsInRange(dict_manager, codepoints, kana_start, word_probe_end)) {
    if (match.entry != nullptr && match.length >= kOpeningContentWordMinLength &&
        (match.entry->pos == core::PartOfSpeech::Verb || match.entry->pos == core::PartOfSpeech::Adjective ||
         match.entry->pos == core::PartOfSpeech::Adverb || match.entry->pos == core::PartOfSpeech::Interjection)) {
      return false;
    }
  }
  const char32_t okurigana = codepoints[kana_start];
  const std::string head = normalize::encodeUtf8(codepoints[kana_start - 1]);
  if (verb_helpers::hasDictionaryGodanBaseFromIRow(&dict_manager, head, okurigana) ||
      (kana::isERowCodepoint(okurigana) &&
       verb_helpers::isVerbInDictionary(&dict_manager,
                                        normalize::concat(head, normalize::encodeUtf8(okurigana), "る"))) ||
      (okurigana == U'い' && verb_helpers::isAdjectiveInDictionary(&dict_manager, normalize::concat(head, "い")))) {
    return true;
  }
  // A word a noun takes opening on the kana makes the run plus that word a
  // reading of its own, so nothing is proven from the right: a non-final
  // particle (が, より, って), an auxiliary the noun licenses (だ, らしい), a
  // bound suffix (たち, ぶる, っぽ) or a cell of する (研究+さ+れる).
  // An auxiliary of two kana or more needs only to be one a noun is not
  // barred from (確認+たる, 対応+たし); a one-kana one is spelled by okurigana
  // too often (吹+き) and must be one a noun selects.
  for (const auto& match : lookupResultsInRange(dict_manager, codepoints, kana_start, kana_end)) {
    if (match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Verb && match.entry->lemma == "する") {
      return false;
    }
  }
  for (size_t length = 1; kana_start + length <= kana_end; ++length) {
    const auto* particle = opening_entry(length, core::PartOfSpeech::Particle);
    const auto* auxiliary = opening_entry(length, core::PartOfSpeech::Auxiliary);
    const auto* adjective = opening_entry(length, core::PartOfSpeech::Adjective);
    const float noun_to_auxiliary = auxiliary != nullptr
                                        ? BigramTable::getCost(core::ExtendedPOS::Noun, auxiliary->extended_pos)
                                        : bigram_cost::kNeutral;
    const bool noun_takes_auxiliary =
        auxiliary != nullptr &&
        (noun_to_auxiliary < bigram_cost::kNeutral || (length >= 2 && noun_to_auxiliary <= bigram_cost::kNeutral));
    if ((particle != nullptr && particle->extended_pos != core::ExtendedPOS::ParticleFinal) || noun_takes_auxiliary ||
        opening_entry(length, core::PartOfSpeech::Suffix) != nullptr ||
        (adjective != nullptr && length >= 2 && adjective->extended_pos == core::ExtendedPOS::AdjStem) ||
        grammar::spellsBoundDerivationalSuffixCell(
            normalize::encodeRange(codepoints, kana_start, kana_start + length))) {
      return false;
    }
  }
  // An a-row kana followed by an auxiliary that selects the irrealis is a
  // Godan irrealis cell on the kanji (鳴か+む, 咲か+む), unless the kana opens
  // a function word of its own (確認+まじ).
  const bool opens_two_kana_function_word =
      kana_start + 2 <= kana_end && (opening_entry(2, core::PartOfSpeech::Auxiliary) != nullptr ||
                                     opening_entry(2, core::PartOfSpeech::Particle) != nullptr);
  if (kana::isARowCodepoint(okurigana) && !opens_two_kana_function_word && kana_start + 1 < kana_end) {
    for (size_t aux_end = kana_start + 2; aux_end <= kana_end; ++aux_end) {
      const auto* auxiliary =
          lookupEntryInRange(dict_manager, codepoints, kana_start + 1, aux_end, core::PartOfSpeech::Auxiliary);
      if (auxiliary != nullptr &&
          BigramTable::getCost(core::ExtendedPOS::VerbMizenkei, auxiliary->extended_pos) < bigram_cost::kNeutral &&
          BigramTable::getCost(core::ExtendedPOS::Noun, auxiliary->extended_pos) > bigram_cost::kNeutral) {
        return true;
      }
    }
  }
  // A registered two-kana verb opening on the kana is the predicate of a
  // particle-dropped noun (会社|いく, 部屋|いる).
  if (kana_start + 2 <= kana_end && opening_entry(2, core::PartOfSpeech::Verb) != nullptr) {
    return false;
  }
  // Of the inferred readings of the kanji plus each prefix of the kana, any
  // that is a bound derivational suffix verb on the kanji (肉+めく) makes the
  // kanji that suffix's host, so none of them proves a head.
  // A sa-hen reading makes the kanji a verbal noun, and one whose stem the
  // dictionary conjugates in another row (味わ+く against 味わう) is refuted by
  // that entry.
  // A reading that leaves the run to resume on the ら column proves nothing:
  // no native word starts there (瞬+ため leaves らった), as the opaque-fragment
  // rule in overlapsRegisteredWord holds.
  bool proven = false;
  for (size_t reading_end = kana_start + 1; reading_end <= kana_end; ++reading_end) {
    if (reading_end < codepoints.size() && char_types[reading_end] == normalize::CharType::Hiragana &&
        kana::isRaColumnCodepoint(codepoints[reading_end])) {
      continue;
    }
    for (const auto& analysis : inflection.analyze(normalize::encodeRange(codepoints, kana_start - 1, reading_end))) {
      if (!utf8::startsWith(analysis.stem, head) || analysis.verb_type == grammar::VerbType::Unknown ||
          analysis.confidence < candidate::verb_cost::kConstructedVerbMinConfidence) {
        continue;
      }
      if (grammar::isBoundDerivationalSuffixVerbLemma(std::string_view(analysis.base_form).substr(head.size()))) {
        return false;
      }
      proven =
          proven || (analysis.verb_type != grammar::VerbType::Suru &&
                     !verb_helpers::stemHasDictionaryVerbOnOtherRow(&dict_manager, analysis.stem, analysis.base_form));
    }
  }
  return proven;
}

// Inputs shared by every prefix length of one same-type run.
struct PrefixScanContext {
  const std::vector<char32_t>& codepoints;
  const std::vector<normalize::CharType>& char_types;
  size_t start_pos;
  normalize::CharType start_type;
  bool started_with_particle;
  size_t crossed_particle_pos;
  const dictionary::DictionaryManager* dict_manager;
  const grammar::Inflection& inflection;
  bool starts_at_dictionary_verb_continuative;
  bool starts_after_dictionary_adjective;
};

// The nominal readings that select a prefix before it is priced.
struct PrefixShape {
  bool closes_particle_bracketed_hiragana_noun{false};
  bool brackets_medial_particle_crossing{false};
  bool selects_past_tari_collision_noun{false};
  bool precedes_closed_native_number{false};
  bool closes_genitive_negative_noun{false};
};

bool crossesVerbOkuriganaBoundary(const PrefixScanContext& ctx, size_t len, size_t candidate_end) {
  const auto& codepoints = ctx.codepoints;
  if (ctx.starts_at_dictionary_verb_continuative && len > 1 &&
      normalize::isParticleCodepoint(codepoints[candidate_end - 1]) && candidate_end < codepoints.size() &&
      isRightBoundaryParticle(codepoints[candidate_end])) {
    return true;
  }
  // A kana run beginning on the okurigana of a dictionary verb cannot
  // absorb a following closed auxiliary.  The first mora belongs to the
  // preceding continuative/terminal verb (書く+だべ, 書く+き); retaining an
  // opaque same-type edge would erase both independently licensed
  // boundaries.
  if (ctx.starts_at_dictionary_verb_continuative && len > 1 && ctx.dict_manager != nullptr) {
    bool embeds_closed_auxiliary = false;
    for (size_t split = ctx.start_pos + 1; split < candidate_end; ++split) {
      if (lookupEntryInRange(*ctx.dict_manager, codepoints, split, candidate_end, core::PartOfSpeech::Auxiliary) !=
          nullptr) {
        embeds_closed_auxiliary = true;
        break;
      }
    }
    if (embeds_closed_auxiliary) {
      return true;
    }
  }
  return false;
}

PrefixShape classifyPrefixShape(const PrefixScanContext& ctx, size_t len, size_t candidate_end,
                                const std::string& surface) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const normalize::CharType start_type = ctx.start_type;
  const size_t crossed_particle_pos = ctx.crossed_particle_pos;
  const bool crossed_particle_is_bracketed = start_type == normalize::CharType::Hiragana &&
                                             crossed_particle_pos != SIZE_MAX && candidate_end < codepoints.size() &&
                                             isRightBoundaryParticle(codepoints[candidate_end]);
  const bool closes_particle_bracketed_hiragana_noun =
      crossed_particle_is_bracketed && candidate_end == crossed_particle_pos + 1;
  // The same bracket one mora further in. A run that spells a particle
  // character word-internally and is closed by a real particle sits in a
  // nominal slot either way, so it takes the nominal category rather than
  // the opaque one and its length penalty (おとな + に, ひとつ + を).
  const bool brackets_medial_particle_crossing =
      crossed_particle_is_bracketed && !ctx.started_with_particle && candidate_end > crossed_particle_pos + 1;
  // After a past auxiliary, a following hiragana noun beginning with り
  // must remain available as a nominal host. Otherwise the preceding た
  // absorbs its first mora as the listing particle たり (買っ+た+りんご).
  const bool closes_past_tari_collision_noun =
      start_type == normalize::CharType::Hiragana && start_pos > 0 && len >= 2 && codepoints[start_pos - 1] == U'た' &&
      codepoints[start_pos] == U'り' &&
      (candidate_end == codepoints.size() ||
       (candidate_end < codepoints.size() && isRightBoundaryParticle(codepoints[candidate_end])));
  const bool has_verb_tail_after_ri = closes_past_tari_collision_noun && ctx.dict_manager != nullptr &&
                                      lookupEntryInRange(*ctx.dict_manager, codepoints, start_pos + 1, candidate_end,
                                                         core::PartOfSpeech::Verb) != nullptr;
  const bool has_inflected_verb_reading =
      closes_past_tari_collision_noun && std::any_of(ctx.inflection.analyze(surface).begin(),
                                                     ctx.inflection.analyze(surface).end(), [](const auto& analysis) {
                                                       return analysis.verb_type != grammar::VerbType::Unknown &&
                                                              analysis.verb_type != grammar::VerbType::IAdjective &&
                                                              !analysis.morphemes.empty() &&
                                                              analysis.confidence > candidate::kNoOriginConfidence;
                                                     });
  const bool has_contracted_progressive_tail =
      closes_past_tari_collision_noun && len >= 2 &&
      grammar::isContractedProgressiveSurface(extractSubstring(codepoints, candidate_end - 2, candidate_end));
  const bool selects_past_tari_collision_noun = closes_past_tari_collision_noun && !has_verb_tail_after_ri &&
                                                !has_inflected_verb_reading && !has_contracted_progressive_tail;
  const bool precedes_closed_native_number =
      start_type == normalize::CharType::Hiragana && startsClosedNativeNumber(codepoints, candidate_end);
  // What licenses the nominal reading is the genitive の with the negative
  // predicate behind it, so the negative has to be resolved from its own
  // paradigm rather than from its opening mora. That mora also opens the
  // conditional なら, which follows the nominalizer instead and leaves a
  // predicate in front of it — reading the run as a nominal there cuts an
  // ordinary verb in half (食べる|の|なら, not 食|べる|の|なら).
  const bool closes_genitive_negative_noun =
      start_type == normalize::CharType::Hiragana && candidate_end - start_pos >= 2 &&
      !ctx.starts_at_dictionary_verb_continuative && !ctx.starts_after_dictionary_adjective &&
      candidate_end + 1 < codepoints.size() && codepoints[candidate_end] == U'の' &&
      verb_helpers::naiNegativeFollowsAt(codepoints, candidate_end + 1);
  return {closes_particle_bracketed_hiragana_noun, brackets_medial_particle_crossing, selects_past_tari_collision_noun,
          precedes_closed_native_number, closes_genitive_negative_noun};
}

// Prices a kanji run against the words it would absorb; false when no run of
// this length is offered.
bool priceKanjiRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, float& cost) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const normalize::CharType start_type = ctx.start_type;
  // Penalize kanji sequences ending with honorific/title suffixes (様, 氏)
  // to encourage NOUN + SUFFIX separation (e.g., 田中様 → 田中 + 様)
  // 的 is not listed: the plain kanji-run cost already keeps 目的/知的 whole
  // while 論理+的 splits.
  // The host length carries no information here, unlike for the plural
  // honorific 方 below: 様 attaches to a bare surname, and a surname is an
  // open class that is routinely one kanji (辻様, 林様, 森様), so the
  // penalty has to reach a two-kanji run. The lexicalized compounds that
  // share the shape (異様, 同様, 王様, 神様, 殿様, 奥様) are a closed set and
  // are held in the dictionary instead.
  // 君 and 殿 are the same titles, but each also closes many two-kanji words
  // (主君, 暴君, 宮殿, 沈殿) that are not a closed set, so only a host of two
  // or more kanji is split off (佐藤君, 先生殿).
  if (start_type == normalize::CharType::Kanji && len >= 2) {
    char32_t last_char = codepoints[candidate_end - 1];
    const bool closes_title =
        last_char == U'様' || last_char == U'氏' || (len >= 3 && (last_char == U'君' || last_char == U'殿'));
    if (closes_title) {
      cost += 4.0F;  // Strong penalty to prefer NOUN + SUFFIX path
    }
  }

  // Penalize kanji sequences starting with the prefix kanji 御.
  // 御 is an L1 PREFIX entry and should split off as a productive prefix
  // (御 + 尽力, 御 + 挨拶, 御 + 協力). The +2.0 penalty makes the PREFIX path
  // win over any 2+ char kanji_seq starting with 御. Lexicalized 御-X nouns
  // (御者, 御所, 御曹司) come from the dictionary and get a separate bonus
  // in scorer.cpp to beat the prefix path.
  if (start_type == normalize::CharType::Kanji && len >= 2 && codepoints[start_pos] == U'御') {
    cost += 2.0F;
  }

  // Skip kanji sequences starting with iteration mark (々)
  // 々 always attaches to the preceding kanji (人々, 時々)
  // It can never start a word
  if (start_type == normalize::CharType::Kanji && normalize::isIterationMark(codepoints[start_pos])) {
    return false;
  }

  // Penalize kanji sequences that extend past iteration mark (々), except
  // for a complete double-reduplication X々Y々 (津々浦々, 様々). The latter
  // is a productive four-character compound shape and must retain its
  // whole-word candidate; a lone completed pair followed by another kanji
  // (時々妙) is still a boundary.
  if (start_type == normalize::CharType::Kanji && len >= 3) {
    bool is_double_reduplication = len == 4 && normalize::isIterationMark(codepoints[start_pos + 1]) &&
                                   normalize::isIterationMark(codepoints[start_pos + 3]);
    for (size_t i = start_pos + 1; i < candidate_end - 1; ++i) {
      if (!is_double_reduplication && normalize::isIterationMark(codepoints[i])) {
        // Found 々 in the middle - penalize extending past it
        cost += 5.0F;
        break;
      }
    }
  }

  // Penalize kanji sequences with interrogative kanji (何, 誰, 幾) at NON-initial position
  // e.g., 今何 should be split as 今 + 何, not kept as one compound
  // But 何日, 何人 (interrogative + counter) should stay together
  // Interrogatives are standalone words unless they're at the start (counter pattern)
  if (start_type == normalize::CharType::Kanji && len >= 2) {
    for (size_t i = start_pos + 1; i < candidate_end; ++i) {  // Skip first char
      if (isInterrogativeKanji(codepoints[i])) {
        // Heavy penalty to force split
        cost += 3.0F;
        break;
      }
    }
  }

  // A temporal prefix that is itself a standalone noun (今) heads a temporal
  // compound, never an arbitrary one: 今週/今回/今後 continue it, an ordinary
  // noun does not (今|紙, 今|本, 今|大会). Without this the generic run cost
  // (shorter than the sum of its parts) glues the adverbial 今 to whatever
  // object follows. The sibling prefixes are not standalone nouns — 先 is a
  // suffix and 来 a verb stem — so 先方/来客 keep their run.
  if (start_type == normalize::CharType::Kanji && len >= 2 && ctx.dict_manager != nullptr &&
      isPrefixLikeKanji(codepoints[start_pos]) &&
      !normalize::continuesTemporalNounCompound(codepoints[start_pos], codepoints[start_pos + 1]) &&
      !(len == 2 && normalize::isStyleSuffixKanji(codepoints[start_pos + 1]) &&
        (candidate_end >= codepoints.size() || ctx.char_types[candidate_end] != normalize::CharType::Kanji)) &&
      (start_pos == 0 || ctx.char_types[start_pos - 1] != normalize::CharType::Kanji)) {
    const auto* head =
        lookupEntryInRange(*ctx.dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Noun);
    if (head != nullptr && head->extended_pos == core::ExtendedPOS::Noun) {
      return false;
    }
  }
  return true;
}

// Prices a hiragana run that spells a particle; false when no run of this
// length is offered.
bool priceHiraganaRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, const PrefixShape& shape,
                      float& cost) {
  const auto& codepoints = ctx.codepoints;
  const auto* dict_manager = ctx.dict_manager;
  const size_t crossed_particle_pos = ctx.crossed_particle_pos;
  // Penalize hiragana candidates that include an internal particle
  // character (see scan loop above). The penalty keeps particle splits
  // preferred when the prefix is a plausible word (ここ+で beats ここで),
  // while letting genuine nouns spanning a particle char (こども, ひとつ)
  // win when the split leaves an implausible fragment (こど+も, ひ+と+つ).
  // - 2-char candidates (single char + particle char) are skipped unless
  //   another nominal particle closes the run; that bracket proves the
  //   particle-like mora is word-internal (ひも+を)
  // - Particle-final candidates (こども) get a minor penalty
  // - Medial crossing (one char after the particle, e.g. ひとつ) is less
  //   plausible and gets a strong penalty; genuine words still win
  //   because their split path needs multiple unknown 1-char fragments
  if (ctx.start_type == normalize::CharType::Hiragana && !ctx.started_with_particle &&
      candidate_end > crossed_particle_pos) {
    if (len < 3 && !shape.closes_particle_bracketed_hiragana_noun) {
      return false;
    }
    // The bracket above is also what separates a native noun from a real
    // particle boundary here: without it the strong penalty made the noun
    // lose to a chain of one-character fragments whose own reading strands
    // the run's head (おとな|に|なる, not お|と|なに|なる).
    cost += (candidate_end > crossed_particle_pos + 1 && !shape.brackets_medial_particle_crossing)
                ? scorer::scale::kStrong
                : scorer::scale::kMinor;
    // The mora allowed past the crossed particle exists for native nouns
    // that spell a particle word-internally (こども, ひとつ). It must not be
    // taken from the front of a bound word instead: an auxiliary or a
    // particle attaches leftward, so its own left edge is a fixed boundary
    // and the run has to stop there (見|る|が|ごとく, not 見|るがご|とく).
    // An unbound word heading the same position proves nothing, since a
    // native noun may simply spell it (おとな + になる, not お + と + なに).
    constexpr size_t kStraddledWordProbe = 4;
    if (candidate_end > crossed_particle_pos + 1 && dict_manager != nullptr &&
        hasDictionaryEntryFrom(dict_manager, codepoints, candidate_end - 1, 2, kStraddledWordProbe,
                               core::PartOfSpeech::Unknown, [](const dictionary::DictionaryEntry& entry) {
                                 return entry.pos == core::PartOfSpeech::Auxiliary ||
                                        entry.pos == core::PartOfSpeech::Particle;
                               })) {
      return false;
    }
  }

  // The conditional particle ば cannot close an open nominal run. A listed
  // noun such as ことば retains its dictionary edge, while an unverified
  // hiragana fallback must leave the particle available at the boundary.
  if (ctx.start_type == normalize::CharType::Hiragana && !ctx.started_with_particle && dict_manager != nullptr &&
      candidate_end > ctx.start_pos + 1) {
    const auto* final_particle =
        lookupEntryInRange(*dict_manager, codepoints, candidate_end - 1, candidate_end, core::PartOfSpeech::Particle);
    if (final_particle != nullptr && final_particle->extended_pos == core::ExtendedPOS::ParticleConj &&
        codepoints[candidate_end - 1] == U'ば') {
      return false;
    }
  }
  return true;
}

void appendInterjectionNominalCandidate(const PrefixScanContext& ctx, size_t len, size_t candidate_end,
                                        const std::string& surface, std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  // A closed interjection cannot host the nominalizing suffix さ. When
  // that shape occurs, retain a complete unknown noun candidate as the
  // only grammatical lexical reading (うわ+さ -> うわさ), rather than
  // letting the suffix expose the interjection as an independent token.
  if (ctx.start_type == normalize::CharType::Hiragana && !ctx.started_with_particle && len >= 3 &&
      codepoints[candidate_end - 1] == U'さ' && dict_manager != nullptr) {
    const bool follows_prefix = start_pos > 0 && lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos,
                                                                    core::PartOfSpeech::Prefix) != nullptr;
    const auto* interjection =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, candidate_end - 1, core::PartOfSpeech::Interjection);
    if (interjection != nullptr && !follows_prefix) {
      auto noun_candidate = makeCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Noun,
                                          candidate::kSelectedNominalShortHeadCost,
                                          /*has_suffix=*/true, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
      noun_candidate.pattern = "interjection_nominal_boundary";
#endif
      candidates.push_back(std::move(noun_candidate));
    }
  }
}

// Penalize hiragana sequences starting with particle characters
// These could be nouns (はし, はな, にく, にゃんこ) but are less likely than
// the particle interpretation, unless the particle path has connection penalties
bool priceParticleStartedRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, float& cost,
                             bool& has_suffix) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  if (ctx.started_with_particle) {
    if (len == 1) {
      return false;  // Single-char particle-start never forms a noun alone
    }
    // Check if this is a reduplicated pattern (same character repeated)
    // Reduplicated hiragana like はは (母), ちち (父) are likely real words
    bool is_reduplicated = (len == 2 && codepoints[start_pos] == codepoints[start_pos + 1]);
    if (is_reduplicated) {
      // Small bonus for reduplicated patterns - they're often real words
      cost -= 0.5F;
    } else if (len == 2) {
      // 2-char: light penalty — bigram penalties on unnatural particle chains
      // provide enough discouragement for false splits (は+し vs はし)
      cost += 0.5F;
    } else if (len == 3) {
      // 3-char: moderate penalty (にある, によれ are likely particle chains)
      cost += 0.8F;
    } else {
      // 4+ char: heavier penalty scaling with length
      // but still generated so words like にゃんこ have a chance
      cost += 1.0F + static_cast<float>(len - 3) * 0.5F;
    }
    // Mark as has_suffix to skip exceeds_dict_length penalty in tokenizer
    has_suffix = true;
  }
  if (ctx.started_with_particle && !isFollowedByNominalParticle(codepoints, candidate_end, ctx.dict_manager) &&
      decomposesIntoMultipleParticles(codepoints, start_pos, candidate_end, ctx.dict_manager)) {
    return false;
  }
  return true;
}

bool crossesKanjiWordBoundary(const PrefixScanContext& ctx, size_t len, size_t candidate_end,
                              const std::string& surface) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const normalize::CharType start_type = ctx.start_type;
  const auto* dict_manager = ctx.dict_manager;
  // A kanji run must not end on the kanji that heads a bound suffix: the
  // suffix owns that character together with its okurigana (画面|越し,
  // 条件|付き), so a run reaching into it is a boundary error.
  if (start_type == normalize::CharType::Kanji && dict_manager != nullptr && candidate_end < codepoints.size() &&
      ctx.char_types[candidate_end] == normalize::CharType::Hiragana) {
    constexpr size_t kSuffixProbe = 3;
    const size_t probe_end = std::min(codepoints.size(), candidate_end + kSuffixProbe);
    bool heads_bound_suffix = false;
    for (size_t suffix_end = candidate_end + 1; suffix_end <= probe_end; ++suffix_end) {
      if (lookupEntryInRange(*dict_manager, codepoints, candidate_end - 1, suffix_end, core::PartOfSpeech::Suffix) !=
          nullptr) {
        heads_bound_suffix = true;
        break;
      }
    }
    if (heads_bound_suffix && len > 1) {
      return true;
    }
  }
  // A sahen nominal is a predicate head: the light verb behind it takes the
  // whole clause, so a listed noun in front of it is that clause's modifier,
  // not the left half of a compound (直接|確認させられる). The listing is what
  // separates the two readings — a head with no lexical identity of its own
  // has nothing to lose by being absorbed (早期発見する stays one nominal).
  if (start_type == normalize::CharType::Kanji && len >= 3 && dict_manager != nullptr &&
      candidate_end < codepoints.size() && ctx.char_types[candidate_end] == normalize::CharType::Hiragana &&
      (codepoints[candidate_end] == U'す' || codepoints[candidate_end] == U'し' ||
       codepoints[candidate_end] == U'さ')) {
    bool listed_modifier_head = false;
    for (size_t split = start_pos + 1; split < candidate_end && !listed_modifier_head; ++split) {
      listed_modifier_head =
          lookupEntryInRange(*dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Noun) != nullptr;
    }
    if (listed_modifier_head) {
      return true;
    }
  }
  // A kanji run must not close on a prefix that scopes rightward. Such a
  // kanji modifies what comes after it, so absorbing it into the noun behind
  // it fabricates a compound out of two separate words (仕事|超|忙しい, not
  // 仕事超|忙しい). The prefix keeps its own one-kanji candidate.
  if (start_type == normalize::CharType::Kanji && len > 1 &&
      grammar::isLeftBranchingPrefixKanji(codepoints[candidate_end - 1])) {
    return true;
  }
  // A quantity head cannot be joined to the one-kanji stem of a following
  // sokuonbin predicate. The sequence is a noun phrase plus a verb
  // (二件|残っ, 複数|残っ), never an unknown compound noun ending at the
  // predicate stem. This also leaves the っ available to the verb edge.
  if (start_type == normalize::CharType::Kanji && len >= 3 && candidate_end < codepoints.size() &&
      codepoints[candidate_end] == U'っ') {
    const size_t head_end = candidate_end - 1;
    const bool numeral_counter_head = head_end >= start_pos + 2 &&
                                      normalize::isCounterKanji(codepoints[head_end - 1]) &&
                                      normalize::isNumeralCodepoint(codepoints[head_end - 2]);
    const bool quantity_noun_head = head_end >= start_pos + 2 && codepoints[head_end - 1] == U'数' &&
                                    normalize::isKanjiCodepoint(codepoints[head_end - 2]);
    if (numeral_counter_head || quantity_noun_head) {
      return true;
    }
  }
  if (start_type == normalize::CharType::Kanji && len > 2 && dict_manager != nullptr &&
      dict_manager->lookupExact(surface) == nullptr &&
      spansInterjectionBoundary(*dict_manager, codepoints, start_pos, candidate_end)) {
    return true;
  }
  if (start_type == normalize::CharType::Kanji && len > 1 && dict_manager != nullptr &&
      dict_manager->lookupExact(surface) == nullptr &&
      closesOnPronounAfterRegisteredNoun(*dict_manager, codepoints, start_pos, candidate_end)) {
    return true;
  }
  // The mirror boundary: a kanji run must not open with a registered
  // multi-kanji formal noun (以来|問題, 途中|経過). A formal noun is a bound
  // right-hand element, so the material after it starts a new phrase rather
  // than continuing one compound. One-kanji formal nouns are excluded — 内,
  // 中, 手, 先 head ordinary kango (内容, 中止, 手法, 先方).
  if (start_type == normalize::CharType::Kanji && dict_manager != nullptr && len > 2) {
    bool opens_with_formal_noun = false;
    for (size_t head_end = start_pos + 2; head_end < candidate_end; ++head_end) {
      const auto* entry = lookupEntryInRange(*dict_manager, codepoints, start_pos, head_end, core::PartOfSpeech::Noun);
      if (entry != nullptr && entry->extended_pos == core::ExtendedPOS::NounFormal) {
        opens_with_formal_noun = true;
        break;
      }
    }
    if (opens_with_formal_noun) {
      return true;
    }
  }
  return false;
}

bool crossesHiraganaWordBoundary(const PrefixScanContext& ctx, size_t len, size_t candidate_end) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const normalize::CharType start_type = ctx.start_type;
  const auto* dict_manager = ctx.dict_manager;
  // A formal noun is a bound right-hand element carrying its own edge, so
  // an opaque hiragana run must not close on one. Closing there hides the
  // boundary in front of it and buries a closed-class morpheme inside a
  // fabricated noun (見|た|こと|の|ない, not 見|たこと|の|ない). A run that
  // *is* the formal noun keeps its candidate, since nothing precedes it
  // inside the span.
  if (start_type == normalize::CharType::Hiragana && !ctx.started_with_particle && len > 1 && dict_manager != nullptr) {
    bool closes_on_formal_noun = false;
    for (size_t split = start_pos + 1; split < candidate_end; ++split) {
      const auto* tail = lookupEntryInRange(*dict_manager, codepoints, split, candidate_end, core::PartOfSpeech::Noun);
      if (tail != nullptr && tail->extended_pos == core::ExtendedPOS::NounFormal) {
        closes_on_formal_noun = true;
        break;
      }
    }
    if (closes_on_formal_noun) {
      return true;
    }
  }
  // A hiragana fallback run has no analysis of its own, so it must not
  // cross a case particle that closes a registered word: that boundary is
  // proven (見る|が|ごとし, not 見|るが|ごとし). A run whose kana merely
  // spell a particle keeps its candidate, because nothing ends in front of
  // it (ひがし, たまご).
  if (start_type == normalize::CharType::Hiragana && len > 1 && dict_manager != nullptr) {
    bool crosses_proven_particle = false;
    for (size_t particle_pos = start_pos; particle_pos < candidate_end; ++particle_pos) {
      const auto* particle =
          lookupEntryInRange(*dict_manager, codepoints, particle_pos, particle_pos + 1, core::PartOfSpeech::Particle);
      if (particle == nullptr || particle->extended_pos != core::ExtendedPOS::ParticleCase) {
        continue;
      }
      const size_t scan_start = dictionaryLookbehindStart(particle_pos);
      if (hasDictionaryEntryEndingAt(*dict_manager, codepoints, scan_start, particle_pos,
                                     partOfSpeechMask(core::PartOfSpeech::Verb) |
                                         partOfSpeechMask(core::PartOfSpeech::Adjective) |
                                         partOfSpeechMask(core::PartOfSpeech::Noun))) {
        crosses_proven_particle = true;
        break;
      }
    }
    if (crosses_proven_particle) {
      return true;
    }
  }
  // An auxiliary is licensed by the morpheme in front of it, so an opaque
  // hiragana run must not open on one whose licenser is right there: that
  // boundary is proven from the left (追わ|れ|た|ねずみ, not 追わ|れ|たねずみ).
  // The connection table decides what counts as a licenser, so a run whose
  // opening kana merely spell an auxiliary after an unrelated morpheme
  // (ゆで|たまご) keeps its candidate.
  if (start_type == normalize::CharType::Hiragana && !ctx.started_with_particle && len > 1 && dict_manager != nullptr) {
    const auto* opening =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Auxiliary);
    bool opens_on_licensed_auxiliary = false;
    for (size_t licenser_start = dictionaryLookbehindStart(start_pos); opening != nullptr && licenser_start < start_pos;
         ++licenser_start) {
      const auto* licenser =
          lookupEntryInRange(*dict_manager, codepoints, licenser_start, start_pos, core::PartOfSpeech::Auxiliary);
      if (licenser != nullptr &&
          BigramTable::getCost(licenser->extended_pos, opening->extended_pos) < bigram_cost::kNeutral) {
        opens_on_licensed_auxiliary = true;
        break;
      }
    }
    if (opens_on_licensed_auxiliary) {
      return true;
    }
  }
  return false;
}

bool overlapsRegisteredWord(const PrefixScanContext& ctx, size_t len, size_t candidate_end, const std::string& surface,
                            core::PartOfSpeech pos) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const normalize::CharType start_type = ctx.start_type;
  const auto* dict_manager = ctx.dict_manager;
  // A kanji fallback run must not end on a kanji that heads a predicate
  // whose okurigana is the very next character: that boundary is proven
  // from the right (外|飲み|たい, not 外飲|みたい; 地|固まる, not 地固|まる),
  // and the kana spelled by the stolen mora plus what follows it are cheap
  // enough to pay for the fabrication. A shorter run ending before that
  // kanji keeps its candidate, which is what supplies 外 here, and a
  // one-kanji run is exempt because there is no fabrication to reject
  // (夢|みたい).
  if (start_type == normalize::CharType::Kanji && len > 1 && dict_manager != nullptr &&
      candidate_end < codepoints.size() && ctx.char_types[candidate_end] == normalize::CharType::Hiragana &&
      dict_manager->lookupExact(surface) == nullptr &&
      kanjiHeadsPredicateAt(codepoints, ctx.char_types, candidate_end, ctx.inflection, *dict_manager)) {
    return true;
  }
  if (start_type == normalize::CharType::Hiragana &&
      opensOnParticleBeforePronoun(dict_manager, codepoints, start_pos, candidate_end)) {
    return true;
  }
  // A kana run that spells a registered suffix exactly (ごと, たび) already has
  // that entry; an unknown noun over the same span only lets it be read as
  // the head a determiner selects (こと+ある+ごと).
  if (start_type == normalize::CharType::Hiragana && pos == core::PartOfSpeech::Noun && dict_manager != nullptr &&
      dict_manager->lookupExact(surface, core::PartOfSpeech::Suffix) != nullptr) {
    return true;
  }
  // Likewise a run spelled wholly by a chain of registered auxiliaries
  // (ます, ませ+ん): an opaque duplicate only bypasses the connection that
  // licenses the chain (だけ+ます). The same holds for an auxiliary closed by
  // its particles (だ+よ+ね).
  if (start_type == normalize::CharType::Hiragana && dict_manager != nullptr &&
      (len > 1 || dict_manager->lookupExact(surface, core::PartOfSpeech::Auxiliary) == nullptr) &&
      (spellsAuxiliaryChain(*dict_manager, codepoints, start_pos, candidate_end) ||
       hasAuxiliaryParticleDecomposition(codepoints, start_pos, candidate_end, dict_manager))) {
    return true;
  }
  // An opaque kana fragment does not open on the ら column inside a kana
  // run: no native word starts there, so the run began inside a word
  // (あつく+るしい); the one-mora fallback still covers the span.
  if (start_type == normalize::CharType::Hiragana && pos == core::PartOfSpeech::Other && len > 1 && start_pos > 0 &&
      ctx.char_types[start_pos - 1] == normalize::CharType::Hiragana &&
      kana::isRaColumnCodepoint(codepoints[start_pos])) {
    return true;
  }
  return false;
}

void appendPluralHonorificSuffix(const PrefixScanContext& ctx, size_t len, size_t candidate_end,
                                 const std::string& surface, std::vector<UnknownCandidate>& candidates) {
  const size_t start_pos = ctx.start_pos;
  // Emit a standalone SUFFIX candidate for plural-honorific 方 when it sits
  // at the tail of a kanji_seq (i.e., preceded by another kanji). Enables
  // splits like 皆様(NOUN) + 方(SUFFIX) for 皆様方. Restricting to "prev is
  // kanji" avoids false splits like その方(NOUN), 北の方(NOUN) where 方 is
  // a standalone noun, not a plural-honorific suffix.
  // The host has to be a term of address, which is at least two kanji long
  // (先生方, 皆様方, 奥様方, 客様方). Unlike 様, the plural honorific does
  // not attach to a bare surname, so nothing open-class reaches a
  // one-kanji host; what does is a lexicalized compound noun (彼方, 行方,
  // 味方, 両方, 先方, 目方), where the honorific reading never applies.
  constexpr size_t kMinPluralHonorificHost = 2;
  size_t honorific_host_start = start_pos;
  while (honorific_host_start > 0 && ctx.char_types[honorific_host_start - 1] == normalize::CharType::Kanji) {
    --honorific_host_start;
  }
  if (ctx.start_type == normalize::CharType::Kanji && len == 1 && ctx.codepoints[start_pos] == U'方' &&
      start_pos - honorific_host_start >= kMinPluralHonorificHost) {
    auto suffix_cand = makeCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Suffix, 0.5F,
                                     /*has_suffix=*/true, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
    suffix_cand.confidence = 1.0F;
    suffix_cand.pattern = "tail_suffix_方";
#endif
    candidates.push_back(suffix_cand);
  }
}

}  // namespace

void UnknownWordGenerator::appendSameTypePrefixCandidates(const std::vector<char32_t>& codepoints,
                                                          const std::vector<normalize::CharType>& char_types,
                                                          const SameTypeRun& run,
                                                          std::vector<UnknownCandidate>& candidates) const {
  const size_t start_pos = run.start_pos;
  const normalize::CharType start_type = run.start_type;
  const bool starts_non_word_run = run.starts_non_word_run;
  const bool started_with_particle = run.started_with_particle;
  const size_t end_pos = run.end_pos;

  // Generate candidates for different lengths
  const bool has_formal_noun_na_adjective_boundary =
      hasFormalNounNaAdjectiveBoundary(codepoints, start_pos, end_pos, start_type, dict_manager_);
  const size_t first_candidate_length = starts_non_word_run ? end_pos - start_pos : 1;
  const bool starts_at_dictionary_verb_continuative =
      start_type == normalize::CharType::Hiragana &&
      (startsAtDictionaryVerbContinuative(codepoints, char_types, start_pos, dict_manager_) ||
       startsAfterDictionaryPredicate(codepoints, char_types, start_pos, end_pos, dict_manager_,
                                      core::PartOfSpeech::Verb));
  const bool starts_after_dictionary_adjective =
      start_type == normalize::CharType::Hiragana &&
      startsAfterDictionaryPredicate(codepoints, char_types, start_pos, end_pos, dict_manager_,
                                     core::PartOfSpeech::Adjective);
  const PrefixScanContext ctx{codepoints,
                              char_types,
                              start_pos,
                              start_type,
                              started_with_particle,
                              run.crossed_particle_pos,
                              dict_manager_,
                              inflection_,
                              starts_at_dictionary_verb_continuative,
                              starts_after_dictionary_adjective};
  for (size_t len = first_candidate_length; len <= end_pos - start_pos; ++len) {
    size_t candidate_end = start_pos + len;
    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    if (!surface.empty()) {
      if (crossesVerbOkuriganaBoundary(ctx, len, candidate_end)) {
        continue;
      }
      const PrefixShape shape = classifyPrefixShape(ctx, len, candidate_end, surface);
      // Particle-start hiragana sequences are potential nouns (はし, はな, にく)
      // Use NOUN POS instead of OTHER to avoid exceeds_dict_length penalty
      core::PartOfSpeech pos = (started_with_particle || shape.closes_particle_bracketed_hiragana_noun ||
                                shape.brackets_medial_particle_crossing || shape.selects_past_tari_collision_noun ||
                                shape.precedes_closed_native_number || shape.closes_genitive_negative_noun)
                                   ? core::PartOfSpeech::Noun
                                   : getPosForType(start_type);
      // A noun never ends on the small っ: it geminates onto the next mora,
      // so the run stops inside a word (は+とっさ, not はとっ).
      if (pos == core::PartOfSpeech::Noun && start_type == normalize::CharType::Hiragana &&
          codepoints[candidate_end - 1] == core::hiragana::kSmallTsu) {
        continue;
      }
      float cost = getCostForType(start_type, len);
      if (shape.selects_past_tari_collision_noun) {
        cost = candidate::kSelectedNominalShortHeadCost;
      }
      if (shape.precedes_closed_native_number || shape.closes_genitive_negative_noun) {
        cost = candidate::kSelectedNominalShortHeadCost + bigram_cost::kDoubleVeryStrongBonus;
      }
      if (has_formal_noun_na_adjective_boundary && len >= 2) {
        cost += candidate::kFormalNounNaAdjectiveBoundaryPenalty;
      }
      if (!priceKanjiRun(ctx, len, candidate_end, cost) || !priceHiraganaRun(ctx, len, candidate_end, shape, cost)) {
        continue;
      }
      appendInterjectionNominalCandidate(ctx, len, candidate_end, surface, candidates);
      bool has_suffix = shape.selects_past_tari_collision_noun;
      if (!priceParticleStartedRun(ctx, len, candidate_end, cost, has_suffix) ||
          crossesKanjiWordBoundary(ctx, len, candidate_end, surface) ||
          crossesHiraganaWordBoundary(ctx, len, candidate_end) ||
          overlapsRegisteredWord(ctx, len, candidate_end, surface, pos)) {
        continue;
      }
      // A run of Latin letters or digits read as a nominal is one whose script
      // sets it apart from the Japanese around it, and that difference decides
      // one connection: nothing on the Japanese side modifies it attributively.
      const bool names_foreign_nominal =
          pos == core::PartOfSpeech::Noun &&
          (start_type == normalize::CharType::Alphabet || start_type == normalize::CharType::Digit);
      // A pictograph keeps its own output class but scores as the phrase
      // boundary it marks (歌っ+てる+わ+よ+♪), like the punctuation it stands in for.
      core::ExtendedPOS scoring_epos = core::ExtendedPOS::Unknown;
      if (names_foreign_nominal) {
        scoring_epos = core::ExtendedPOS::NounForeign;
      } else if (pos == core::PartOfSpeech::Other && normalize::isPhraseClosingPictograph(codepoints[start_pos])) {
        scoring_epos = core::ExtendedPOS::Symbol;
      }
      auto cand = makeCandidate(surface, start_pos, candidate_end, pos, cost, has_suffix, CandidateOrigin::SameType,
                                scoring_epos);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = started_with_particle ? 0.7F : 1.0F;
      switch (start_type) {
        case normalize::CharType::Kanji:
          cand.pattern = "kanji_seq";
          break;
        case normalize::CharType::Katakana:
          cand.pattern = "kata_seq";
          break;
        case normalize::CharType::Hiragana:
          cand.pattern = started_with_particle ? "hira_noun_seq" : "hira_seq";
          break;
        case normalize::CharType::Alphabet:
          cand.pattern = "alpha_seq";
          break;
        case normalize::CharType::Digit:
          cand.pattern = "digit_seq";
          break;
        default:
          cand.pattern = "other_seq";
          break;
      }
#endif
      candidates.push_back(cand);

      appendPluralHonorificSuffix(ctx, len, candidate_end, surface, candidates);
    }
  }
}

}  // namespace suzume::analysis
