/**
 * @file verb_candidates_helpers.h
 * @brief Internal helpers for verb candidate generation
 *
 * This file contains shared helper functions used by verb candidate generators.
 * These helpers are internal to the analysis module and should not be exposed
 * in the public API.
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_HELPERS_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_HELPERS_H_

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "analysis/dictionary_probe.h"
#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "tokenizer_utils.h"
#include "unknown.h"

namespace suzume::analysis::verb_helpers {

// =============================================================================
// Single-kanji Ichidan verbs (単漢字一段動詞)
// =============================================================================

/**
 * @brief Check if character is a known single-kanji ichidan verb
 *
 * Common single-kanji Ichidan verbs:
 * 見(みる), 居(いる), 着(きる), 寝(ねる), 煮(にる), 似(にる)
 * 経(へる), 干(ひる), 射(いる), 得(える/うる), 出(でる), 鋳(いる)
 */
bool isSingleKanjiIchidan(char32_t c);

/** Return true for a one-kanji stem that takes the polite auxiliary directly. */
bool isSingleKanjiPoliteStem(char32_t c);

/**
 * @brief Check if a surface form is exactly one single-kanji Ichidan verb
 *
 * True when the surface consists of exactly one codepoint and that codepoint
 * is a known single-kanji Ichidan verb (see isSingleKanjiIchidan).
 */
bool isSingleKanjiIchidanSurface(std::string_view surface);

// =============================================================================
// Dictionary Lookup Helpers
// =============================================================================

/**
 * @brief Generic dictionary entry lookup by part of speech
 * @param dict_manager Dictionary manager (may be null)
 * @param surface Surface form to lookup
 * @param pos Part of speech to match
 * @return true if an exact-match entry with the specified POS exists
 */
bool hasDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface,
                        core::PartOfSpeech pos);

// Detect a grammatical chain boundary inside a larger fabricated verb candidate:
// either productive て/で between verified verbs (なっ+て+なら), or classical
// negative ず before a verified continuation (あら+ず+し).  The full candidate
// is discarded only when both lexical sides are dictionary-backed.
bool hasInternalVerbChainBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                  const grammar::Inflection& inflection,
                                  const dictionary::DictionaryManager* dict_manager);

/**
 * @brief Check if a base form exists in dictionary as a verb
 */
bool isVerbInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view base_form);

/**
 * @brief Check if a base form exists in dictionary as an adjective
 */
bool isAdjectiveInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view base_form);

/**
 * @brief Check if a terminal is a productively formed -しい i-adjective
 *
 * The inflection analyzer can reinterpret the same bytes as the continuative of
 * a hypothetical ワ行 verb (恐しい -> 恐しう), so both the grammatical suffix and
 * an independently generated i-adjective analysis are required. This is the
 * rule-side counterpart of @ref isAdjectiveInDictionary for the open シク class,
 * whose members cannot all be listed.
 */
bool isProductiveShiiAdjectiveTerminal(std::string_view surface, const grammar::Inflection& inflection);

/**
 * @brief Check if a surface exists in dictionary as a noun (exact match)
 *
 * Reports a hit only for an entry whose surface equals @p surface, so a shorter
 * dictionary prefix (e.g. a single-kanji noun) does not spuriously match a
 * longer verb candidate.
 */
bool isNounInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view surface);

/**
 * @brief Check if a surface exists in dictionary as a noun or adjective (exact match)
 *
 * Reports a hit only for an entry whose surface equals @p surface (see
 * isNounInDictionary for the exact-match rationale).
 */
bool isNounOrAdjectiveInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view surface);

/**
 * @brief Check if a surface has a non-verb entry in dictionary
 */
bool hasNonVerbDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface);

/**
 * @brief Check whether a continuative is a registered suffix bound to a nominal host
 *
 * A continuative that is also a closed derivational suffix (会社+帰り, 条件+付き)
 * is the bound reading whenever it is written directly onto a kanji or katakana
 * host. In that position the productive deverbal-noun re-reading must not be
 * offered, or it outbids the suffix and erases the morpheme boundary. Free
 * occurrences (帰りが遅い, 家に帰り) have no such host and keep the noun reading.
 */
bool isBoundSuffixAfterNominalHost(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t start_pos, std::string_view surface);

/**
 * @brief Check whether a kanji or katakana host is written directly before a position
 *
 * This is the environment a bound morpheme requires. Callers use it to withhold
 * a candidate whose morpheme cannot stand on its own, whether the reading comes
 * from a dictionary entry or from the inflection analyzer.
 */
bool hasNominalHostBefore(const std::vector<char32_t>& codepoints, size_t start_pos);

/**
 * @brief Check if a surface has a particle entry in dictionary
 *
 * Used to detect compound particles (について, によって, として, etc.)
 */
bool hasParticleDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface);

// Exact one-token case-particle lookup.  Conjunctive particles such as ば are
// valid syllables inside inflected lexical stems and must not trigger the
// noun+case-particle+する guard.
bool hasCaseParticleDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface);

// Exact one-token conjunctive-particle lookup. Used when a particle-homographic
// mora is licensed inside a longer, fully inflected lexical stem.
bool hasConjunctiveParticleDictionaryEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface);

// True when a complete case-particle entry ends exactly at @p pos.
bool followsCaseParticle(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                         size_t pos);

/**
 * @brief Whether a case-marked argument ends at @p pos, through any focus particles
 *
 * A focus particle stacks on top of the case marking without changing the
 * argument structure, so the predicate evidence a case particle supplies
 * survives it (半数に+も+達した reads like 半数に+達した).
 */
bool followsCaseMarkedArgument(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t pos);

// A bare continuative can chain clauses before the literal Japanese comma
// when a non-quotative case particle or quantified focus phrase licenses a
// predicate on its left.
bool isCommaClauseChainingRenyokei(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                   const dictionary::DictionaryManager* dict_manager);

// True when start_pos is strictly inside a dictionary particle. Candidate
// generators use this to avoid manufacturing a verb from the tail of a
// compound particle (から + やり直す, not か + らやり直す).
bool startsInsideDictionaryParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const dictionary::DictionaryManager* dict_manager);

// Returns true when start_pos is interior to the polite copula です. An
// unknown-word predicate cannot reopen that interior boundary (です -> で+す).
bool startsInsideDictionaryAuxiliary(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const dictionary::DictionaryManager* dict_manager);

// Detect a multi-mora particle beginning exactly at @p start_pos. Such a
// closed-class prefix cannot be the first half of a productive compound verb.
bool startsWithMultiMoraDictionaryParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           const dictionary::DictionaryManager* dict_manager);

// True when start_pos is strictly inside a contiguous kanji run immediately
// followed by し. Such an internal position cannot begin a separate lexical
// candidate: the complete run is a productive verbal noun (提出+し) or the
// kanji portion of a lexical verb stem (見直し).
bool startsInsideKanjiRunBeforeShi(const std::vector<char32_t>& codepoints, size_t start_pos);

// =============================================================================
// Fabricated closed-class absorption guards
// =============================================================================
// The table below is executable documentation: each guarded candidate origin
// names the guard it applies before calling the corresponding helper. The
// integration test asserts every listed origin, so a new generator cannot
// silently inherit an incomplete copy of this family.
enum class GuardMember {
  EmbedTeAuxiliary,
  EmbedTeMiruAuxiliary,
  FocusParticleHead,
};

enum class GuardOrigin {
  HiraganaInflection,
  HiraganaDerived,
  KanjiFinalization,
  KanjiMizenkei,
  KanjiAdjective,
  KanjiCompoundAdjective,
};

struct GuardWiring {
  GuardMember member;
  GuardOrigin origin;
  std::string_view origin_name;
};

inline constexpr std::array<GuardWiring, 8> kGuardWiring = {{
    {GuardMember::EmbedTeAuxiliary, GuardOrigin::HiraganaInflection, "hiragana_inflection"},
    {GuardMember::EmbedTeAuxiliary, GuardOrigin::KanjiFinalization, "kanji_finalization"},
    {GuardMember::EmbedTeAuxiliary, GuardOrigin::KanjiMizenkei, "kanji_mizenkei"},
    {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::HiraganaInflection, "hiragana_inflection"},
    {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::HiraganaDerived, "hiragana_derived"},
    {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::KanjiFinalization, "kanji_finalization"},
    {GuardMember::FocusParticleHead, GuardOrigin::KanjiAdjective, "kanji_adjective"},
    {GuardMember::FocusParticleHead, GuardOrigin::KanjiCompoundAdjective, "kanji_compound_adjective"},
}};

constexpr bool guardIsWired(GuardMember member, GuardOrigin origin) {
  for (const GuardWiring& wiring : kGuardWiring) {
    if (wiring.member == member && wiring.origin == origin) {
      return true;
    }
  }
  return false;
}

// A recurring defect this family defends against: a verb/adjective candidate
// generator builds a NON-dictionary conjugation whose surface swallows an
// adjacent closed-class morpheme, because that morpheme's kana coincide with an
// inflectional ending. The 係助詞 しか・さえ・すら end in an a-row か/え
// that matches a godan mizenkei; a て/で-form + 補助動詞 みる has an internal
// てみ/でみ that matches an ichidan stem. Unchecked, these fabricated tokens
// (水しく for 水しか, 金さう for 金さえ, やってみる for やっ+て+み) outscore the
// correct split.
//
// The guards reject such fabrications and fall into three shapes by where the
// closed-class element sits relative to the fabricated verb:
//   - Tail  (T): the run ends in [word] + particle (+ negative). Helpers:
//                endsWithParticleTailOfPos, endsWithFocusParticleTail (副助詞 ‖
//                係助詞), and hiragana_verb_detail::endsWithParticleAfterVerb
//                (verb-prefix + 副助詞). Plus an inline 副助詞 head check in the
//                kanji adjective path.
//   - Embed (E): an internal て/で + 補助動詞 must split the run. Helpers:
//                embedsTeFormMiruAuxiliary (て/で + みる), embedsTeFormAuxiliary
//                (ていく / benefactive-request). Plus inline てくれ/てもら/てあげ
//                and で + auxiliary-chain checks in the onbin paths — see the
//                per-site comments there for why each set differs from the
//                helper's pattern list (ている/ておく are deliberately absent).
//                Also embedsCaseParticle (格助詞 strictly inside the run), which
//                is what the adjective paths need: a case particle marks an
//                argument boundary, so 水 + を + くみ cannot be one word.
//   - Head  (H): a leading 副助詞 ‖ 係助詞 opens the hiragana portion of an
//                adjective. Helper: startsWithFocusParticleHead, used by both
//                the plain and the compound kanji adjective path.
//
// A real verb/adjective that genuinely embeds these kana (押さえる, 起こす) is
// protected by its dictionary base form where such a lexical candidate exists.
// The candidate generators that can emit an exact dictionary surface apply an
// explicit `!in_dict` exemption; purely rule-derived origins have no such
// surface candidate to exempt. `kGuardWiring` is the authoritative list of
// those origins and is asserted by verb_guard_family_test.
// =============================================================================

/**
 * @brief Check if a span ends in a dictionary particle of the given POS
 *
 * True when the span [start_pos, end_pos) ends in a dictionary-registered
 * particle of @p particle_pos, optionally followed by the negative
 * ない / なかっ / なかった or the copula inflection だ / だっ. Detects
 * candidates fabricated by absorbing [word] + particle (+ auxiliary) into a
 * single token: the 副助詞 しか ends in
 * the a-row mora か, which coincides with the godan-ka mizenkei/onbin ending,
 * so a non-word verb conjugation can absorb noun + しか(…ない) (水しかない read
 * as a form of the non-word 水しく). The particle must be 2+ codepoints so the
 * single mora か of a genuine godan-ka mizenkei (行かない) can never match, and
 * a non-empty prefix must remain before the particle.
 */
bool endsWithParticleTailOfPos(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                               core::ExtendedPOS particle_pos);

/**
 * @brief Check if a span ends in a focus particle (副助詞 or 係助詞) tail
 *
 * Convenience wrapper over endsWithParticleTailOfPos covering both focus
 * particle classes: 副助詞 (しか, だけ, ばかり, ...) and 係助詞 (さえ, こそ,
 * すら, ...). Both attach after a noun and may be followed by an auxiliary,
 * so a candidate spanning [word] + focus particle (+ auxiliary) is never a
 * single word (お金さえない = お金 + さえ + ない, never a form of the non-word
 * 金さう).
 */
bool endsWithFocusParticleTail(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Check if the hiragana portion of a candidate opens with a focus particle
 *
 * True when a dictionary focus particle (副助詞 or 係助詞) of 2+ codepoints
 * starts at @p hiragana_start inside [hiragana_start, end_pos). A particle
 * there is not adjective okurigana: the run is [noun] + particle and the kana
 * that look like an inflectional ending belong to the following word (水とか +
 * いう absorbed into the non-word adjective 水とかい, 水しか + ない into 水しかい).
 * The particle must be 2+ codepoints so a one-mora coincidence cannot match,
 * and a following っ waives the check because an adjective past keeps it.
 */
bool startsWithFocusParticleHead(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t hiragana_start, size_t end_pos);

/**
 * @brief Check if a candidate span swallows a case particle
 *
 * True when a dictionary case particle (格助詞) sits strictly inside
 * [start_pos, end_pos), with a non-empty prefix and suffix around it. A case
 * particle marks an argument boundary, so no single lexical word can span one:
 * a candidate that does was assembled out of [noun] + particle + [predicate]
 * (さきに食べとく read as one adjective, 水をく as the stem of the non-word 水をくい).
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool embedsCaseParticle(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start_pos, size_t end_pos);

/**
 * @brief Check if a span ends in a one-mora case particle written onto a continuative
 *
 * embedsCaseParticle needs material on both sides of the particle, so it cannot
 * see the argument boundary when the fabricated candidate stops on the particle
 * itself: 変わりが is proposed as the irrealis of the non-word 変わりぐ, absorbing
 * the nominative that marks 変わり as a subject. The multi-mora tail guard cannot
 * reach it either, because が is a single mora and would then also match the
 * genuine irrealis of a godan-ka verb.
 *
 * The host supplies the missing evidence instead: an i-row mora before the
 * particle is what nominalizes a godan stem, and the analyzer must read the host
 * as the continuative of some other base form. A real godan-ga irrealis has its
 * own stem there (和らが, 揺るが), so nothing lexical is suppressed.
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool endsWithCaseParticleAfterContinuative(const dictionary::DictionaryManager* dict_manager,
                                           const grammar::Inflection& inflection,
                                           const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Check if a candidate's conjugation ending is itself a classical auxiliary
 *
 * True when @p surface is @p stem plus a remainder that the dictionary knows as
 * a classical auxiliary. The monograde and カ変 paradigms both stand on a bare
 * stem, so every kana past it is supposed to be the conjugation ending — and a
 * classical auxiliary is never one of those. It selects a cell and carries its
 * own token, so a candidate spelling one has absorbed it: 来ぬ is 来 + ぬ and
 * 食べたり is 食べ + たり, never a cell of 来る or 食べる.
 *
 * The godan paradigms are the reason this is keyed on the verb type at the call
 * site rather than on the surface: 死ぬ and 読む end in the same kana as ぬ and
 * む, but there the kana is their own terminal ending.
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool spellsClassicalAuxiliaryEnding(const dictionary::DictionaryManager* dict_manager, std::string_view surface,
                                    std::string_view stem);

/**
 * @brief Check if a span ends in a multi-mora auxiliary written after okurigana
 *
 * True when a dictionary auxiliary of 2+ codepoints closes [.., end_pos) and at
 * least one okurigana mora of the host precedes it from @p okurigana_start. An
 * auxiliary selects a conjugated cell of the word in front of it, so a candidate
 * reaching across one was assembled out of [verb] + auxiliary: 過ぎたれ is 過ぎ
 * plus the izenkei たれ, not a cell of the non-word 過ぎたる.
 *
 * Requiring okurigana before the auxiliary is what keeps the nominal hosts out:
 * 重要なれ attaches なれ straight to the kanji run, and the copula there is not
 * absorbing a verb stem. The 2+ codepoint floor is the usual one — ぬ, き, り
 * and the rest of the one-mora closed class are also ordinary verb endings.
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool endsWithAuxiliaryAfterOkurigana(const dictionary::DictionaryManager* dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t okurigana_start, size_t end_pos);

/**
 * @brief Length of a multi-mora negative auxiliary written at a position
 *
 * Returns the codepoint length of the longest dictionary auxiliary starting at
 * @p pos whose extended POS is one of the negative classes, and 0 when none is
 * there. The irrealis of a godan verb has no use of its own — it exists because
 * a negative auxiliary selects it — so the auxiliary is what tells a generator
 * where the cell ends. Reading the paradigm out of the dictionary keeps every
 * one of its cells (ない, なかっ, なけれ, ざり, ざる) on a single rule instead of a
 * list that grows one cell at a time and leaves the rest of the paradigm to
 * fabricated readings.
 *
 * One-mora members (ぬ, ず, ん, ね, じ) are excluded on the same ground the rest
 * of this family excludes one-mora particles: after an a-row mora they are
 * indistinguishable from an ordinary word ending (数 read as か + ず), so they
 * need the extra conditions their own generators carry.
 */
size_t negativeAuxiliaryLengthAt(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check if a candidate span opens on the tail of an earlier closed-class word
 *
 * True when a dictionary auxiliary or particle begins before @p start_pos and
 * ends strictly inside [start_pos, end_pos). The candidate has then taken that
 * word's tail and joined it to what follows: しょう in 高いでしょうから is the
 * last two morae of the polite copula でしょ plus the volitional う, read as the
 * dictionary form of the non-word しょう. Unlike the head shape above, the
 * closed-class element is not contained in the candidate at all — only its end
 * is — which is why it cannot be found by scanning the candidate's own span.
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool opensOnClosedClassWordTail(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

// True when a fabricated verb candidate starts with an exact auxiliary entry
// and absorbs that auxiliary's negative inflection (過ぎない → 過ぎ + ない).
// The check is POS-based: lexical verbs with the same surface are unaffected.
bool hasAuxiliaryNegativeBoundary(const dictionary::DictionaryManager* dict_manager,
                                  const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Check whether a complete auxiliary heads a candidate span
 *
 * An auxiliary predicates over something already complete, so it is never the
 * head of a lexical word: whatever follows it belongs to a separate token. A
 * span that covers a whole auxiliary and keeps going has therefore crossed a
 * morpheme boundary, and the kana behind the auxiliary is being read as
 * okurigana of a word that does not exist (如く + あら analyzed as the irrealis
 * of the non-word 如くある). The auxiliary must end strictly inside the span:
 * one that ends with it is the auxiliary itself, spelled as its own cell.
 *
 * Only an inflected cell counts. An auxiliary in its base form is a headword
 * like any other and is routinely homographic with an ordinary word or with the
 * opening morae of one (ある of あるいて, たい of たいらな), so finding one there
 * says nothing (an empty lemma is the dictionary's shorthand for "same as the
 * surface", so it marks a base form too). A cell whose lemma differs exists only inside
 * that paradigm — なかっ is not a word, it is the past stem of ない — so meeting
 * one at the head of a span is evidence the span reaches into a closed
 * paradigm. The 2+ codepoint floor is the one the rest of this family carries:
 * one mora is spelled like the opening mora of any number of words (す of
 * すいた, た of たどっ).
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool opensOnCompleteAuxiliary(const dictionary::DictionaryManager* dict_manager,
                              const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

// True when a dictionary formal noun starts at @p pos. This lets candidate
// generation preserve the boundary after a predicate's inflecting auxiliary.
bool formalNounFollowsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                         size_t pos);

/**
 * @brief Look up a verb's lemma from the dictionary
 *
 * Returns the lemma of the first verb entry whose surface exactly matches
 * @p surface and whose lemma is non-empty. Falls back to @p fallback when the
 * dictionary is null or no matching verb entry exists.
 */
std::string lookupVerbLemma(const dictionary::DictionaryManager* dict_manager, std::string_view surface,
                            std::string_view fallback);

/**
 * @brief Verify a constructed base form as a real verb
 *
 * Accepts the base form when it is a dictionary verb, or when inflection
 * analysis recognizes it with confidence strictly above @p min_confidence as
 * a Godan verb (@p require_godan true) or an Ichidan verb (@p require_godan
 * false).
 */
bool isVerifiedVerbBase(const dictionary::DictionaryManager* dict_manager, const grammar::Inflection& inflection,
                        std::string_view base_form, float min_confidence, bool require_godan);

// =============================================================================
// Candidate Sorting
// =============================================================================

/**
 * @brief Sort candidates by cost (lowest cost first)
 */
void sortCandidatesByCost(std::vector<UnknownCandidate>& candidates, size_t first_index = 0);

// =============================================================================
// Emphatic Pattern Helpers (口語強調パターン)
// =============================================================================

/**
 * @brief Check if character is an emphatic suffix character
 *
 * Emphatic characters: っ, ッ, ー, ぁぃぅぇぉ, ァィゥェォ
 */
bool isEmphaticChar(char32_t c);

/**
 * @brief Get the vowel character (あいうえお) for a hiragana's ending vowel
 *
 * Maps any hiragana to its vowel row character.
 * Returns 0 for characters without vowels (ん, っ) or non-hiragana.
 */
char32_t getHiraganaVowel(char32_t c);

/**
 * @brief A matched emphatic suffix and the input position after it.
 */
struct EmphaticSuffixMatch {
  std::string suffix;
  size_t end = 0;
  size_t standard_char_count = 0;
  size_t repeated_vowel_count = 0;

  [[nodiscard]] bool empty() const { return suffix.empty(); }
};

/**
 * @brief Context-specific treatment of a sokuon before て/た.
 */
enum class SokuonOnsetPolicy {
  Candidate,        // Generated full-form candidate: release っ from って/った.
  DictionaryEntry,  // Dictionary stem: preserve productive onbin (あらっ+て/た).
};

/**
 * @brief Match standard emphatic marks and repeated final vowels after a candidate.
 */
EmphaticSuffixMatch matchEmphaticSuffix(const std::vector<char32_t>& codepoints, size_t base_end,
                                        core::PartOfSpeech base_pos,
                                        SokuonOnsetPolicy policy = SokuonOnsetPolicy::Candidate);

/**
 * @brief Return the cost adjustment for a matched emphatic suffix.
 */
float emphaticCostAdjustment(const EmphaticSuffixMatch& match);

/**
 * @brief True when a single-verb candidate surface embeds a て/で-form followed
 *        by a subsidiary or aspect verb that would otherwise merge into one verb.
 *
 * The 〜ていく directional aspect ends in く, so a candidate like 食べていく is
 * mis-generated as a lone godan-ka verb and must be split (食べ+て+いく), unlike
 * 食べている where the plain split already wins. The benefactive/request verbs
 * (てもらう/てくれ/てあげ/てほしい) likewise split (助けてもらう → 助け+て+もらう).
 * Continuation 〜ている/ておく is intentionally NOT matched here: it would also
 * catch verbs whose renyokei ends in て (慌て+ている, 捨て+ておく) and strand the
 * stem. The completed-state construction 〜てある is different: it is always a
 * te-form followed by the existential subsidiary, including after such stems.
 */
bool embedsTeFormAuxiliary(std::string_view surface);

/**
 * @brief True when a candidate span embeds a te-form て/で immediately followed
 *        by み past its first codepoint.
 *
 * An internal て/で inside a verb surface is always a conjugation boundary (the
 * te-form particle or its voiced onbin form), and a following み is the onset of
 * the subsidiary verb みる, so the span is [te-form] + みる, never a single
 * conjugated verb (食べてみれば = 食べ + て + みれ + ば, やってみ = やっ + て +
 * み). No real verb embeds てみ/でみ inside one conjugated form. The codepoint
 * at @p start_pos is exempt: a candidate that merely begins with て/で (てみ
 * itself, で-leading runs) is a different shape and is left untouched.
 */
bool embedsTeFormMiruAuxiliary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief True when a dictionary auxiliary stands directly on an onbin kana
 *        inside the span.
 *
 * The te-form guards above look for a て/で that the contraction and the past
 * auxiliary simply do not leave behind: 書い+とけ+ば and 書い+た+って both put a
 * complete auxiliary straight onto the onbin stem, and the run then reads as one
 * fabricated verb (書いとける, 書いたる). The onbin kana in front of the auxiliary
 * is the boundary evidence the surface still carries.
 */
bool embedsAuxiliaryOnOnbinStem(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                const dictionary::DictionaryManager* dict_manager);

/**
 * @brief True when a dictionary auxiliary accepted by @p accept starts at @p pos.
 *
 * Callers select the grammatical class they need rather than a spelling, so the
 * probe stays a category decision (see the ExtendedPOS predicates in types.h).
 */
bool auxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        size_t pos, EntryAccept accept);

/**
 * @brief True when a literary (文語) auxiliary starts at @p pos.
 *
 * Narrower than predicateAuxiliaryFollowsAt: only the auxiliaries the inflection
 * analyzer never emits as part of a modern paradigm qualify (see
 * core::isClassicalAuxiliaryType). Candidate generators that normally demand a
 * dictionary base form use this as the missing lexical evidence — a continuative
 * or irrealis stem is the only thing these auxiliaries can stand on, so the
 * ambiguity the dictionary gate was protecting against (連用形 み against the
 * auxiliary みたい) cannot arise in front of one.
 */
bool classicalAuxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief True when a dictionary auxiliary that selects a predicate starts at
 *        @p pos.
 *
 * Such an auxiliary attaches to an inflected verb form, so the span in front of
 * it is verbal and the only open question is where the verb begins (花散り+ぬ,
 * 見送り+けむ). The copula is deliberately excluded: it follows a deverbal noun
 * just as readily (足取り+だっ+た), so it carries no such evidence.
 */
bool predicateAuxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Extend candidates with emphatic suffix variants
 *
 * For each verb/adjective candidate, checks if input continues with emphatic
 * characters and creates an extended variant.
 */
void addEmphaticVariants(std::vector<UnknownCandidate>& candidates, const std::vector<char32_t>& codepoints,
                         size_t first_index = 0);

// =============================================================================
// Pattern Skip Helpers
// =============================================================================

/**
 * @brief Check if surface ends with ます auxiliary patterns
 *
 * Returns true if pattern should be skipped (to allow auxiliary split)
 */
bool shouldSkipMasuAuxPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check if surface ends with そう auxiliary patterns
 */
bool shouldSkipSouPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check if surface contains compound adjective patterns (にくい/やすい/がたい)
 */
bool isCompoundAdjectivePattern(std::string_view surface);

/**
 * @brief Check if surface contains adj renyokei + なる conjugation pattern
 *
 * Matches: くなっ, くなり, くなる, くなれ anywhere in the string.
 * Used to skip/penalize false candidates that absorb adj く-form + なる.
 */
bool containsKuNaruPattern(std::string_view surface);

/**
 * @brief Detect a fully spelled-out reduplicated 〜しい adjective head at @p start_pos
 *
 * 畳語 i-adjectives whose doubled stem is written out instead of using the
 * iteration mark: a repeated two-character unit (XYXY) followed by し and an
 * i-adjective inflection onset (い/く/か/け), e.g. 馬鹿馬鹿しい, バカバカしく,
 * ばかばかしかった. The halves are compared by codepoint, so one rule covers
 * kanji and both kana scripts. The iteration-mark spelling (若々しい) needs no
 * special handling because 々 keeps the stem within the regular 2-kanji path.
 *
 * @param codepoints Full input codepoints
 * @param start_pos Index of the first character of the doubled unit
 * @return true if positions [start_pos, start_pos+5] form the reduplicated head
 */
bool isReduplicatedShiiAdjectiveHead(const std::vector<char32_t>& codepoints, size_t start_pos);

/**
 * @brief Get Godan VerbTypes that use a specific onbin pattern
 *
 * Onbin patterns:
 * - "い" (ikuon) → GodanKa, GodanGa
 * - "っ" (sokuon) → GodanKa (行く irregular), GodanRa, GodanTa, GodanWa
 * - "ん" (hatsuonbin) → GodanNa, GodanBa, GodanMa
 * - "" (none) → GodanSa
 *
 * @param onbin Onbin pattern to match ("い", "っ", "ん", or "")
 * @return Reference to a shared immutable table of (VerbType, base_suffix) pairs
 */
grammar::GodanOnbinRange getGodanTypesByOnbin(std::string_view onbin);

/**
 * @brief Result of matching an onbin stem against the dictionary's godan verbs.
 *
 * @c base_suffix points into the immutable getGodanTypesByOnbin() table and is
 * valid for the program's lifetime. When @c matched is false, @c verb_type is
 * Unknown, @c base_form is empty, and @c base_suffix is empty.
 */
struct GodanOnbinDictMatch {
  grammar::VerbType verb_type = grammar::VerbType::Unknown;
  std::string base_form;         // stem + base_suffix
  std::string_view base_suffix;  // the matched suffix from the table
  bool matched = false;
};

/**
 * @brief First (verb_type, stem+base_suffix) pair for @p onbin whose base form
 *        is a dictionary verb, in getGodanTypesByOnbin() table order.
 *
 * Reproduces the phase-1 "check every godan candidate, keep the first dictionary
 * hit" scan shared by the onbin candidate generators.
 *
 * @param dict_manager Dictionary manager (may be null → no match)
 * @param stem         Verb stem to which each table suffix is appended
 * @param onbin        Onbin pattern ("い", "っ", "ん", or "")
 * @return The first dictionary-verified match, or an unmatched result
 */
GodanOnbinDictMatch firstGodanOnbinDictBase(const dictionary::DictionaryManager* dict_manager, std::string_view stem,
                                            std::string_view onbin);

/**
 * @brief Check if surface contains passive/potential auxiliary patterns
 */
bool shouldSkipPassiveAuxPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check whether the codepoint after passive れ continues an auxiliary chain
 *
 * Matches the passive/potential continuation set after れ (or られ):
 * る/た/て immediately, the closed ない-family paradigm via
 * naiNegativeFollowsAt(), ま (れます, れました), and the conditional
 * れ+ば. With @p strict_masu the ま branch additionally requires a following
 * す or せ (れます/れません), excluding bare ま.
 *
 * @param codepoints Full input codepoints
 * @param pos_after_re Index of the codepoint immediately after れ
 * @param strict_masu Require す/せ after ま
 */
bool isPassiveAuxContinuation(const std::vector<char32_t>& codepoints, size_t pos_after_re, bool strict_masu);

/**
 * @brief Whether れ begins the passive auxiliary's conditional cell れれ+ば.
 *
 * The first れ is the passive auxiliary stem; the second is its 仮定形.
 */
bool isPassiveAuxConditionalAt(const std::vector<char32_t>& codepoints, size_t passive_re_pos);

/** @brief Whether the passive auxiliary starting at @p passive_re_pos consumes the surface remainder. */
bool isCompletePassiveAuxiliaryAt(const std::vector<char32_t>& codepoints, size_t passive_re_pos);

/** @brief Whether the causative auxiliary starting at @p causative_se_pos consumes the surface remainder. */
bool isCompleteCausativeAuxiliaryAt(const std::vector<char32_t>& codepoints, size_t causative_se_pos);

/**
 * @brief Check if surface contains causative auxiliary patterns
 */
bool shouldSkipCausativeAuxPattern(std::string_view surface, grammar::VerbType verb_type);

/**
 * @brief Check if surface matches suru-verb auxiliary patterns
 *
 * Detects サ変名詞 + する-auxiliary chains (勉強して, 対応される, 実行させた)
 * via connection-based inflection analysis: the hiragana tail after the kanji
 * run must analyze as a conjugation of する with an auxiliary chain attached.
 * Returns true if the pattern should be skipped (to allow the noun + する-aux
 * split to win).
 */
bool shouldSkipSuruVerbAuxPattern(std::string_view surface, size_t kanji_count, const grammar::Inflection& inflection);

// =============================================================================
// Auxiliary Pattern Penalty Checks (for verb candidate cost adjustment)
// =============================================================================

/**
 * @brief Check if surface contains te-form + auxiliary verb patterns
 * Uses kTeFormAuxPenaltyPatterns from scorer_constants.h
 */
bool containsTeFormAuxPattern(std::string_view surface);

/**
 * @brief Check if surface contains causative auxiliary patterns (contains-based)
 * Uses kCausativeAuxPenaltyPatterns from scorer_constants.h
 * Unlike shouldSkipCausativeAuxPattern, this uses contains() not endsWith()
 */
bool containsCausativeAuxPattern(std::string_view surface);

// A passive followed by a causative is always an auxiliary chain (書かれさせる),
// unlike an ordinary lexical compound that merely ends in せる.
bool containsPassiveCausativeAuxPattern(std::string_view surface);

// =============================================================================
// Inflection Analysis Helpers
// =============================================================================

/**
 * @brief Check whether a polite-auxiliary (ます family) follows at @p pos.
 *
 * ます / まし / ませ attach only to a verb renyokei, never to a bare noun, so
 * this licenses the verb reading of a noun/renyokei homograph (感じます).
 * Matches ま followed by す (ます), し (ました), or せ (ません).
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading ま
 */
bool masuAuxFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

// Returns the length of a complete finite ます inflection beginning at pos,
// or zero when the following characters do not form one.  The caller decides
// whether the form is at a clause boundary.
size_t finiteMasuFormLengthAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether an ichidan causative auxiliary (させ family) follows at @p pos.
 *
 * The causative させる attaches only to a verb mizenkei, never to a bare noun,
 * so — like the ます family — it licenses the verb reading of a noun/renyokei
 * homograph (感じさせる → 感じ(VERB) + させる, not 感じ(NOUN) + さ + せる).
 * Matches さ followed by せ (させる/させた/させ...).
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading さ
 */
bool causativeSaseFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a character can start a する-auxiliary after renyokei し.
 *
 * Covers the continuations of する in renyokei position:
 * ちゃ (contracted しちゃう), て/た (して/した), な (しない), ま (します),
 * よ (しよう), ろ (imperative しろ), そ (しそう), と/か/つ (しとく/しかける/しつつ).
 * Used to tell a renyokei し + する-auxiliary chain apart from a nominalized
 * noun or a false godan-sa stem that would absorb the し.
 */
bool isSuruAuxiliaryStarter(char32_t next_char);

/**
 * @brief Check whether a ない-family negative begins at @p pos.
 *
 * Matches the negative auxiliary ない and its conjugated/contracted onsets:
 * ない, なかっ(た), なく(て), なけれ(ば), なけりゃ, なきゃ. A bare な followed by
 * anything else (なる, なさい, ...) does not match, so callers can use this as
 * an unambiguous "negation follows" gate after a verb mizenkei.
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading な
 */
size_t naiNegativeFormLengthAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a ない-family negative begins at @p pos.
 *
 * Boolean facade over naiNegativeFormLengthAt() for callers that only need
 * boundary evidence rather than the exact closed-paradigm span.
 */
bool naiNegativeFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a candidate at @p pos would start inside a kanji run.
 *
 * A kanji run with no boundary in front of it is one word. Carving its last
 * character out as a verb splits that word (提出 into 提 + 出), so a generator
 * that proposes a single-kanji predicate has to know it is mid-run.
 *
 * @param codepoints Full input codepoints
 * @param pos Index the candidate would start at
 */
bool startsInsideKanjiRun(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a span crosses a case particle that a predicate follows.
 *
 * A one-mora case particle inside a hiragana candidate is only incidental while
 * nothing on its far side is a word of its own. Once a dictionary predicate
 * stands there the particle reading is real, and the span is a phrase rather
 * than one open-class verb (み+が+ある, not みがある).
 *
 * @param dict_manager Dictionary, may be null
 * @param codepoints Full input codepoints
 * @param start_pos Span start
 * @param end_pos Span end, exclusive
 */
bool crossesCaseParticleBeforePredicate(const dictionary::DictionaryManager* dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Check whether a candidate starting at @p pos would split a known word.
 *
 * Stricter than startsInsideKanjiRun(): the run this candidate starts inside
 * has to be a dictionary word for the split to be a real loss (事故った, not
 * 複数+残った). Use it where the run is otherwise free to be several words.
 *
 * @param dict_manager Dictionary, may be null
 * @param codepoints Full input codepoints
 * @param pos Index the candidate would start at
 * @param end_pos Index the containing kanji run ends at
 */
bool splitsDictionaryKanjiWord(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t pos, size_t end_pos);

/**
 * @brief Check whether a lexical word begins at @p pos rather than an auxiliary.
 *
 * Every cell that a voice auxiliary hosts is written in kana (た, て, ない,
 * ます, 続ける's own ける is preceded by its kanji). A kanji at the boundary
 * therefore starts a new lexical word, so the auxiliary before it has to keep
 * its own token instead of being absorbed into a fabricated stem.
 *
 * @param codepoints Full input codepoints
 * @param pos Index just past the auxiliary
 */
bool lexicalWordFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether a span runs across the nominalizer っこ
 *
 * A ない-family predicate behind っこ proves the suffix, and with it a morpheme
 * boundary in front of its sokuon (食べ|られ|っこ|ない). Any span reaching across
 * that boundary — or stopping between the suffix's two morae — is built on
 * material the construction has already spoken for.
 */
bool crossesKkoNominalizer(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Whether a candidate starting here opens inside the suffix がまし〜.
 *
 * The suffix derives an i-adjective from a nominal (未練がましい, 恩着せがましさ)
 * and is bound: no word begins at its が, ま or し. An adjective cell must
 * follow the stem, which separates it from the nominal まし after the subject
 * marker (こちらの方がましだ).
 *
 * @param codepoints Full input codepoints
 * @param pos Candidate start position
 */
bool startsInsideGaMashiiSuffix(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Check whether the conditional negative auxiliary なけれ begins at @p pos.
 *
 * This is the irrealis-stem continuation used by 〜なければ.  Keeping it
 * separate from the broader ない-family gate lets candidate generators apply
 * the conditional's stronger boundary evidence without changing ordinary or
 * contracted negative forms.
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading な
 */
bool naiConditionalFollowsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether a classical past/perfect form can end at this position.
 *
 * The 連体形 either modifies a nominal (読みし人, 見しこと) or closes the clause,
 * and the 已然形 takes a conjunctive particle. Requiring one of those keeps the
 * far more frequent readings of the same spellings intact — the サ変 continuative
 * for し, and the ordinary stem-internal kana for the one-mora perfect (見つける
 * is not 見 + つ).
 *
 * The 已然形 has one further position, the clause-final predicate slot, which
 * this function cannot judge because the evidence for it is the continuative in
 * front rather than anything that follows; see @ref clauseEndsAt for the caller
 * that pairs the two.
 *
 * @param dict_manager Dictionary used to probe the follower
 * @param codepoints Full input codepoints
 * @param end_pos Index just past the classical form
 * @param is_izenkei Whether the form is the 已然形 cell
 */
bool classicalPastEnvironmentFollows(const dictionary::DictionaryManager& dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t end_pos, bool is_izenkei);

/**
 * @brief Whether a clause ends at @p pos.
 *
 * True at the end of the input and in front of the punctuation that closes a
 * clause. Forms whose paradigm cell is finite need this on their right-hand
 * side, so it is shared rather than restated per generator.
 *
 * @param codepoints Full input codepoints
 * @param pos Index just past the form being judged
 */
bool clauseEndsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether a case particle begins at @p pos.
 *
 * A 連体形 nominalizes as well as modifies, and the nominal it forms fills an
 * argument slot, so a case particle marks it (告げぬべかりし+に, 読みし+を). The
 * case particle is what separates that reading from the neighbours the same
 * kana spells: a conjunctive particle takes an inflected form instead
 * (注意すべく+し+て), and a final particle closes the clause behind the finite
 * cell the paradigm already covers (できな+さ+そう+だ). The probe stays within the
 * width of the longest function word so it cannot reach into the next clause.
 *
 * @param dict_manager Dictionary used to probe the follower
 * @param codepoints Full input codepoints
 * @param pos Index just past the form being judged
 */
bool caseParticleFollowsAt(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                           size_t pos);

/**
 * @brief Check whether the いただく paradigm begins at @p pos.
 *
 * The receptive humble auxiliary いただく conjugates as いただ + ka-row kana
 * or the onbin い: いただか(ない), いただき, いただく, いただけ(ば/ます),
 * いただこ(う), いただい(た/て). A candidate that ends by absorbing this
 * leading い steals the auxiliary's onset (ご覧いただき → 覧い+ただき,
 * お使いいただく → 使+いい+ただく), so generators use this gate to keep the
 * い with いただく.
 *
 * @param codepoints Full input codepoints
 * @param pos Index expected to hold the leading い
 */
bool itadakuParadigmStartsAt(const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Best inflection candidate per verb class (Ichidan / Suru / Godan)
 *
 * Members left unmatched keep confidence 0.0 and are otherwise
 * default-constructed.
 */
struct VerbClassBests {
  grammar::InflectionCandidate ichidan;
  grammar::InflectionCandidate suru;
  grammar::InflectionCandidate godan;
};

/**
 * @brief Scan inflection candidates for the best Ichidan, Suru, and Godan entries
 *
 * Candidates matched via のだ/んだ stripping (has_explanatory_suffix) are
 * ignored. Ties keep the earlier candidate (strict > comparison).
 */
VerbClassBests bestByVerbClass(const std::vector<grammar::InflectionCandidate>& candidates);

// =============================================================================
// Verb Type / Stem Analysis Helpers
// =============================================================================

/**
 * @brief Get the terminal-form (終止形) okurigana suffix for a verb type
 *
 * Returns the dictionary-form ending: Ichidan yields "る", Godan types yield
 * their base vowel (GodanKa -> "く", GodanSa -> "す", ...). Returns an empty
 * string for verb types without a Godan terminal ending (Suru, Kuru,
 * IAdjective, Unknown).
 */
std::string baseFormSuffix(grammar::VerbType verb_type);

/**
 * @brief Check whether a stem is a valid i-row Ichidan verb stem
 *
 * A valid i-row Ichidan stem ends in an i-row hiragana, has at least two
 * characters, and is not the single-kanji + い pattern (人い -> 人 + いる),
 * which is almost always NOUN + いる rather than an Ichidan verb.
 */
bool isValidIRowIchidanStem(std::string_view stem);

// =============================================================================
// 係り結び (binding particle and the clause-final cell it selects)
// =============================================================================

/**
 * @brief The clause-final cell a binding particle demands of its 結び.
 */
enum class KakariMusubi : uint8_t {
  None,       ///< No governing binding particle, or one that selects no cell.
  Izenkei,    ///< こそ closes its clause on the 已然形.
  Rentaikei,  ///< ぞ and なむ close theirs on the 連体形.
};

/**
 * @brief Which cell the binding particle governing @p clause_pos demands.
 *
 * 係り結び is the one long-distance agreement in the language: the particle sits
 * arbitrarily far from the form it selects, so a rule about a clause-final cell
 * cannot read the requirement off the adjacent token. The search therefore runs
 * leftward from @p clause_pos and stops at a clause boundary, since a particle
 * in an earlier clause governs nothing here.
 *
 * The binding particles do not agree on a cell — こそ takes the 已然形 while ぞ
 * and なむ take the 連体形, and the modern members (さえ, すら, しか, しも) select
 * none — so the class alone is not the answer and the individual particle is
 * resolved here once for every caller.
 */
KakariMusubi governingKakariMusubi(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t clause_pos);

/**
 * @brief Whether a registered classical auxiliary closes [.., @p end_pos).
 *
 * True when a proper suffix of the span is a dictionary auxiliary of one of the
 * classical types, so at least one mora of the span precedes it. Unlike
 * endsWithAuxiliaryAfterOkurigana this admits a one-mora head, because the head
 * being a whole word rather than okurigana is exactly the case it is asked
 * about: に+ける is the perfect's continuative plus けり, not a cell of にける.
 */
bool endsWithClassicalAuxiliary(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

// =============================================================================
// Character Region Detection
// =============================================================================

// Delegate to shared implementation in tokenizer_utils.h
using ::suzume::analysis::findCharRegionEnd;

}  // namespace suzume::analysis::verb_helpers

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_HELPERS_H_
