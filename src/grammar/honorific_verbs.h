/**
 * @file honorific_verbs.h
 * @brief Closed-class honorific, benefactive, modal, and aspectual subsidiary verb data
 *
 * Predicates for closed-class honorific (敬語), benefactive (授受), modal
 * (可能否定 かねる), and aspectual (相 始める・終わる) subsidiary verbs. Their static
 * tables have one owner in the grammar implementation; callers use these
 * predicates instead of scattered surface literals.
 */

#ifndef SUZUME_GRAMMAR_HONORIFIC_VERBS_H_
#define SUZUME_GRAMMAR_HONORIFIC_VERBS_H_

#include <string_view>

namespace suzume::grammar {

/**
 * @brief Check whether a surface is the renyokei of a humble/honorific verb
 * @param surface Candidate token surface (UTF-8)
 * @return true if the surface is a renyokei of いたす, くださる, or いただく
 */
bool isHumbleHonorificRenyokei(std::string_view surface);

/** Return true for a humble/honorific subsidiary verb lemma. */
bool isHumbleHonorificLemma(std::string_view lemma);

/** Return true for a potential benefactive subsidiary verb lemma. */
bool isPotentialBenefactiveLemma(std::string_view lemma);

/** Return true for an ordinary te-form benefactive subsidiary verb lemma. */
bool isBenefactiveLemma(std::string_view lemma);

/**
 * @brief Check whether a surface is the renyokei of any honorific or
 *        benefactive subsidiary verb (いたす・くださる・いただく・もらう・あげる)
 * @param surface Candidate token surface (UTF-8)
 * @return true if the surface belongs to the closed subsidiary verb set
 */
bool isSubsidiaryHonorificRenyokei(std::string_view surface);

/**
 * @brief Check whether a surface is the renyokei/mizenkei of the modal
 *        subsidiary verb かねる
 * @param surface Candidate token surface (UTF-8)
 * @return true if the surface is a conjugated stem of かねる
 */
bool isModalSubsidiaryRenyokei(std::string_view surface);

/**
 * @brief Check whether a lemma is an aspectual subsidiary verb (始める・終わる・終える・過ぎる)
 * @param lemma Dictionary form of the candidate V2 (UTF-8, kanji or reading)
 * @return true if the lemma marks phase rather than lexical content
 *
 * These verbs describe the phase of the preceding predicate instead of adding
 * lexical content, so a compound built on them stays two search units
 * (読み|終わる, 食べ|始める). Callers that would otherwise join an arbitrary V2
 * consult this predicate to keep the finite and nominalized forms consistent.
 */
bool isAspectualSubsidiaryLemma(std::string_view lemma);

/**
 * @brief Check whether a text begins with an honorific/humble subsidiary verb
 * @param surface Text following a candidate continuative stem (UTF-8)
 * @return true if くださる, いただく, or いたす opens the text
 *
 * These verbs attach only to a verb renyokei (お確かめ+ください,
 * ご確認+いただく, 確認+いたします), so their presence is grammatical evidence
 * for the verbal reading of the preceding stem in exactly the way the ます
 * family is. なさる is deliberately absent: callers already treat its な
 * opening as a continuation.
 */
bool startsHonorificSubsidiaryVerb(std::string_view surface);

/**
 * @brief Check whether a lemma is a bound derivational suffix verb (ばる・がかる)
 * @param lemma Dictionary form of the candidate verb (UTF-8)
 * @return true if the verb only exists attached to a nominal host
 *
 * These conjugate as ordinary Godan verbs but have no independent use: there is
 * no verb ばる, only 形式ばる / 四角ばる. A dictionary entry is still the right
 * home for the conjugation table, so callers use this predicate to require the
 * host — otherwise the entry wins spans it has no claim on, and because the
 * stem is a single mora those spans are common (ばったり → ばっ + たり).
 */
bool isBoundDerivationalSuffixVerbLemma(std::string_view lemma);

/**
 * @brief Check whether an okurigana run spells a cell of one of those verbs
 * @param okurigana Hiragana run following a kanji (UTF-8)
 * @return true if the run is exactly one inflected form of a bound suffix verb
 *
 * The cells are conjugated from the same lemmas rather than listed, and the
 * match is on the whole run: a kanji run followed by one of them is a nominal
 * host plus the suffix, never a verb of its own (嘘 + じみ). Requiring the whole
 * run keeps an ordinary verb whose okurigana merely opens with the same kana
 * (羽ばたく is not 羽 + ばる).
 */
bool spellsBoundDerivationalSuffixCell(std::string_view okurigana);

/**
 * @brief Check whether a surface is a personal-address suffix (さん・ちゃん・たん…)
 * @param surface Candidate token surface (UTF-8)
 * @return true if the suffix names or addresses a person
 *
 * These suffixes take a personal name or a kinship term as their host, so they
 * cannot attach to an inflected predicate. The class matters because two of
 * them are homographic with predicate material: 〜たん is also 〜た+ん (past plus
 * nominalizer) and 〜さん also opens the polite 〜さんです, and without a host
 * check the suffix reading takes a continuative stem with it (確認し+たん).
 */
bool isPersonalAddressSuffix(std::string_view surface);

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_HONORIFIC_VERBS_H_
