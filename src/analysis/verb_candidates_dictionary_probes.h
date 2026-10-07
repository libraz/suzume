/**
 * @file verb_candidates_dictionary_probes.h
 * @brief Dictionary lookups used by verb candidate generation
 *
 * Exact-match probes by part of speech, verb/adjective base-form checks,
 * quantity and counter-run boundaries, and the particle/auxiliary entries a
 * candidate span starts inside of or is followed by.
 */

#ifndef SUZUME_ANALYSIS_VERB_CANDIDATES_DICTIONARY_PROBES_H_
#define SUZUME_ANALYSIS_VERB_CANDIDATES_DICTIONARY_PROBES_H_

#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "analysis/dictionary_probe.h"
#include "core/types.h"
#include "dictionary/dictionary.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"

namespace suzume::analysis::verb_helpers {

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
 * @brief Whether @p base_form is listed as a verb's own dictionary form
 *
 * Unlike isVerbInDictionary, a conjugated cell the dictionary expands from
 * another verb does not count: 書ける is listed as the potential of 書く,
 * not as an ichidan verb in its own right.
 */
bool isVerbBaseFormInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view base_form);

/**
 * @brief Whether an ichidan stem ending at @p stem_end is a stranded potential
 *
 * True when the dictionary lists @p ichidan_base only as another verb's
 * potential (書ける of 書く) and no kana follows the stem for it to carry. A
 * potential stem needs its auxiliary or particle (書け+ない); without one the
 * e-row is the listed verb's own imperative/hypothetical cell (書け+今).
 */
bool isStrandedPotentialStem(const dictionary::DictionaryManager* dict_manager, std::string_view ichidan_base,
                             const std::vector<char32_t>& codepoints, size_t stem_end);

/**
 * @brief Check if a base form exists in dictionary as an adjective
 */
bool isAdjectiveInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view base_form);

/**
 * @brief The span forms of the two lookups above
 *
 * They own the conversion from the codepoint range to the surface, which
 * otherwise inlines the UTF-8 encode loop and the temporary's teardown into
 * every caller. Callers that already hold the base form keep using the view
 * forms.
 */
bool isVerbInDictionary(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start, size_t end);
bool isAdjectiveInDictionary(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                             size_t start, size_t end);

/**
 * @brief Whether @p stem plus the godan terminal of @p i_row_kana is a dictionary verb
 *
 * Reads @p i_row_kana as a godan continuative ending (書+き → 書く); a kana
 * outside the godan i-row never matches.
 */
bool hasDictionaryGodanBaseFromIRow(const dictionary::DictionaryManager* dict_manager, std::string_view stem,
                                    char32_t i_row_kana);

/**
 * @brief Whether codepoints[stem_start, okurigana_pos) plus the okurigana there
 * is the continuative of a dictionary verb
 *
 * Both live paradigms are inverted by rule: a godan continuative replaces the
 * dictionary form's u-row mora with the i-row one (書き → 書く), and an ichidan
 * continuative is the dictionary form without its る (上げ → 上げる,
 * 落ち → 落ちる). The ichidan reading needs a monograde stem-final kana, so a
 * godan okurigana mora (分+か of 分かる) is not read as a continuative.
 */
bool namesDictionaryVerbContinuative(const dictionary::DictionaryManager* dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t stem_start, size_t okurigana_pos);

/** @brief namesDictionaryVerbContinuative over the one kanji before @p okurigana_pos */
inline bool namesDictionaryVerbContinuative(const dictionary::DictionaryManager* dict_manager,
                                            const std::vector<char32_t>& codepoints, size_t okurigana_pos) {
  return okurigana_pos > 0 &&
         namesDictionaryVerbContinuative(dict_manager, codepoints, okurigana_pos - 1, okurigana_pos);
}

/**
 * @brief Whether the kanji at @p pos is a one-kanji dictionary suffix closing a
 * quantity phrase (三割+強, 二時間+弱): it ends the noun run, and relational
 * 前/後 are excluded since they stand alone (三日|後)
 */
bool isQuantityClosingSuffixAt(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t pos);

/**
 * @brief Whether the kanji at @p pos after a quantity opens a word of its own:
 * relational 前/後 or a listed pronoun (三日|後, 5年|彼)
 */
bool opensWordAfterQuantity(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                            size_t pos);

/**
 * @brief End of the counter kanji run that starts at @p pos and stops before @p limit
 *
 * Every leading counter kanji belongs to the counter (時間, 段階) except one
 * that counts only straight after the numeral, which opens the next word
 * (5日|連続, but 3連勝).
 */
size_t counterKanjiRunEnd(const std::vector<char32_t>& codepoints, size_t pos, size_t limit);

/**
 * @brief Whether the word on the kanji run before @p run_end may continue past it
 *
 * A numeral kanji there may still belong to the word (100円+均一). Otherwise
 * grammar::mayBeOkuriganaAt() answers from the kana alone, and a registered
 * particle of two or more morae starting there settles it unless the last
 * kanji opens a registered predicate through it (1万人超+まで). A one-mora
 * particle stays ambiguous with okurigana (三枚+重ねる).
 */
bool kanjiRunMayContinueAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                           size_t run_end);

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
 * @brief Whether the analyzer reads @p surface as an i-adjective terminal
 *
 * Before the nominalizer ん or the quotative っ only a finished predicate stands
 * (つらい+ん+だ, えぐい+って); a noun cannot take either directly, so the
 * terminal reading needs no lexical support there.
 */
bool readsAsIAdjectiveTerminal(std::string_view surface, const grammar::Inflection& inflection);

/**
 * @brief Whether some analysis of @p surface yields @p base_form with @p verb_type
 *
 * @param min_confidence Lowest accepted confidence; the default accepts any,
 *        since penalties can push a confidence below zero
 */
bool readsAsBaseForm(const grammar::Inflection& inflection, std::string_view surface, std::string_view base_form,
                     grammar::VerbType verb_type, float min_confidence = std::numeric_limits<float>::lowest());

/**
 * @brief Check if a surface exists in dictionary as a noun (exact match)
 *
 * Reports a hit only for an entry whose surface equals @p surface, so a shorter
 * dictionary prefix (e.g. a single-kanji noun) does not spuriously match a
 * longer verb candidate.
 */
bool isNounInDictionary(const dictionary::DictionaryManager* dict_manager, std::string_view surface);

/**
 * @brief Whether the dictionary attests @p stem as a Godan-ra verb via its ら irrealis.
 *
 * A bare kanji stem is also an Ichidan stem (見る) or a nasal-onbin stem (読む);
 * only a registered stem+ら whose lemma is stem+る proves the Godan-ra row that
 * the colloquial る→ん contraction needs (帰ん+ない, 帰+ん+だ).
 */
bool attestsGodanRaIrrealis(const dictionary::DictionaryManager* dict_manager, std::string_view stem);

/**
 * @brief Whether the dictionary has a verb on @p stem other than @p base_form.
 *
 * An inferred reading whose stem the dictionary already conjugates in another
 * row (味わ+く against the registered 味わう) is refuted by that entry.
 */
bool stemHasDictionaryVerbOnOtherRow(const dictionary::DictionaryManager* dict_manager, std::string_view stem,
                                     std::string_view base_form);

/**
 * @brief Whether [@p start_pos, @p end_pos) ends on the irrealis of a registered Godan verb.
 *
 * The last kana must be the a-row cell of a Godan row and the stem before it
 * plus that row's terminal must be a dictionary verb (知ら ← 知る, 分から ← 分かる).
 * A ん after such a cell is the contracted negative, not the る→ん contraction
 * of a verb whose stem happens to end in that kana (分か+ん ← 分かる).
 */
bool endsOnRegisteredGodanIrrealis(const dictionary::DictionaryManager* dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos);

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

// True when the mora ending at @p pos is a one-mora particle of @p particle_pos.
bool oneMoraParticleEndsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                           size_t pos, core::ExtendedPOS particle_pos);

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

// True when start_pos is strictly inside a registered i-adjective spelled out
// in full. The terminal い belongs to that adjective, so a coined verb must not
// start on it and carry the boundary past the adjective's end
// (楽しい + って, not 楽 + し + いっ + て).
bool startsInsideDictionaryIAdjective(const std::vector<char32_t>& codepoints, size_t start_pos,
                                      const dictionary::DictionaryManager* dict_manager);

// Detect a multi-mora particle beginning exactly at @p start_pos. Such a
// closed-class prefix cannot be the first half of a productive compound verb.
bool startsWithMultiMoraDictionaryParticle(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           const dictionary::DictionaryManager* dict_manager);

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

/**
 * @brief True when a dictionary auxiliary accepted by @p accept starts at @p pos.
 *
 * Callers select the grammatical class they need rather than a spelling, so the
 * probe stays a category decision (see the ExtendedPOS predicates in types.h).
 */
bool auxiliaryFollowsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        size_t pos, EntryAccept accept);

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

}  // namespace suzume::analysis::verb_helpers

#endif  // SUZUME_ANALYSIS_VERB_CANDIDATES_DICTIONARY_PROBES_H_
