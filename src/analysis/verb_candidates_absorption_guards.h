/**
 * @file verb_candidates_absorption_guards.h
 * @brief Fabricated closed-class absorption guards
 *
 * Guards that reject a verb/adjective candidate whose surface swallows an
 * adjacent particle or auxiliary; the family note below classifies them by
 * where the closed-class element sits.
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_ABSORPTION_GUARDS_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_ABSORPTION_GUARDS_H_

#include <array>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/inflection.h"

namespace suzume::analysis::verb_helpers {

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
  ClassicalAuxiliaryTail,
};

enum class GuardOrigin {
  HiraganaInflection,
  HiraganaDerived,
  KanjiFinalization,
  KanjiMizenkei,
  KanjiRenyokei,
  KanjiAdjective,
  KanjiCompoundAdjective,
};

struct GuardWiring {
  GuardMember member;
  GuardOrigin origin;
  std::string_view origin_name;
};

inline constexpr std::array<GuardWiring, 10> kGuardWiring = {{
    {GuardMember::EmbedTeAuxiliary, GuardOrigin::HiraganaInflection, "hiragana_inflection"},
    {GuardMember::EmbedTeAuxiliary, GuardOrigin::KanjiFinalization, "kanji_finalization"},
    {GuardMember::EmbedTeAuxiliary, GuardOrigin::KanjiMizenkei, "kanji_mizenkei"},
    {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::HiraganaInflection, "hiragana_inflection"},
    {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::HiraganaDerived, "hiragana_derived"},
    {GuardMember::EmbedTeMiruAuxiliary, GuardOrigin::KanjiFinalization, "kanji_finalization"},
    {GuardMember::FocusParticleHead, GuardOrigin::KanjiAdjective, "kanji_adjective"},
    {GuardMember::FocusParticleHead, GuardOrigin::KanjiCompoundAdjective, "kanji_compound_adjective"},
    {GuardMember::ClassicalAuxiliaryTail, GuardOrigin::KanjiFinalization, "kanji_finalization"},
    {GuardMember::ClassicalAuxiliaryTail, GuardOrigin::KanjiRenyokei, "kanji_renyokei"},
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
 * @param include_genitive_and_wa Also count the genitive の and the topic は
 *        as argument boundaries, for spans whose kanji host makes a word-internal
 *        occurrence of either implausible (彼のはやさし)
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool embedsCaseParticle(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start_pos, size_t end_pos, bool include_genitive_and_wa = false);

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
 * @brief The auxiliary endsWithAuxiliaryAfterOkurigana() found, or nullptr
 *
 * For callers that also need to know which paradigm closes the span.
 */
const dictionary::DictionaryEntry* auxiliaryClosingAfterOkurigana(const dictionary::DictionaryManager* dict_manager,
                                                                  const std::vector<char32_t>& codepoints,
                                                                  size_t okurigana_start, size_t end_pos);

/**
 * @brief Whether the auxiliary closing the span after its okurigana is the polite copula
 *
 * No verb's own cell spells です/でし: a coined hiragana verb ending in one is a
 * nominal plus the copula (ごはん+です, ごはん+でし+た).
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool closesOnPoliteCopula(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                          size_t okurigana_start, size_t end_pos);

/**
 * @brief Whether the span is a registered verb continuative plus the polite ます
 *
 * A coined hiragana verb spelled that way (し+ます as one godan-sa verb) is the
 * registered verb and its auxiliary, not a word of its own.
 * @see fabricated closed-class absorption guards (top of this header)
 */
/**
 * @brief Whether codepoints ending at @p stem_end close on a registered
 * auxiliary in its terminal form plus a one-mora conjunctive particle
 *
 * Such a span is a predicate chain (やり+たい+し), never an adjective stem,
 * whichever script opens it. A non-terminal cell (おい of おく) says nothing
 * (おいし+さ).
 * @see fabricated closed-class absorption guards (top of this header)
 */
bool closesOnTerminalAuxiliaryAndConjunctive(const dictionary::DictionaryManager* dict_manager,
                                             const std::vector<char32_t>& codepoints, size_t stem_end);

/**
 * @brief Whether the span from @p start_pos to @p stem_end is a registered verb
 * in its terminal form plus a one-mora conjunctive particle, standing right
 * after the case particle that gives it its argument (好きに+なる+し)
 *
 * The argument slot is the evidence: elsewhere the same kana can open an
 * adjective stem (くる+し+さ).
 */
bool closesOnTerminalVerbAndConjunctive(const dictionary::DictionaryManager* dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start_pos, size_t stem_end);

bool spellsContinuativeBeforePolite(const dictionary::DictionaryManager* dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

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
/**
 * @brief Whether a registered imperative/terminal verb cell ends at `end_pos`
 * and covers `start_pos`.
 *
 * The cell must start before `start_pos` (the span opens inside it), or, when
 * the span's own base is unattested, may start at `start_pos` itself.
 */
bool closedPredicateEndsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                           size_t start_pos, size_t end_pos, bool span_lemma_attested);

bool opensOnClosedClassWordTail(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

/**
 * @brief Whether a coined verb opens on the one-mora particle marking a kanji
 *        or adverb host.
 *
 * Behind a kanji host or a registered adverb a one-mora particle marks that
 * argument, so a coined verb opening on it loses to the same verb read after
 * it whenever the remainder reconstructs at least as well (駅+で+しらべる, not
 * でしらべる; ゆっくり+と+なぞる).
 *
 * @param own_confidence Best inflection confidence of the candidate span
 */
bool coinedVerbOpensOnArgumentParticle(const dictionary::DictionaryManager* dict_manager,
                                       const grammar::Inflection& inflection, const std::vector<char32_t>& codepoints,
                                       size_t start_pos, size_t end_pos, float own_confidence);

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
 * @brief True when an internal て opens a dictionary verb cell ending at @p end_pos.
 *
 * A conditional cell built on a te-form subsidiary (やって+みれ+ば) is the
 * te-form plus that verb, never one conditional stem.
 */
bool embedsTeFormVerbCell(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                          size_t start_pos, size_t end_pos);

}  // namespace suzume::analysis::verb_helpers

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_ABSORPTION_GUARDS_H_
