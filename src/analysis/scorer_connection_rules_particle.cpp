#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "analysis/scorer.h"
#include "analysis/scorer_connection_rules.h"
#include "analysis/scorer_connection_rules_internal.h"
#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/types.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace cost = suzume::analysis::bigram_cost;
namespace sc = suzume::analysis::scorer;

// Surface-based adjustments use cost:: namespace directly from bigram_cost.
// See bigram_table.h and scorer_constants.h for constant values.

namespace suzume::analysis::connection_rules {

namespace {

char32_t firstCodepoint(std::string_view surface) {
  size_t byte_pos = 0;
  return normalize::decodeUtf8(surface, byte_pos);
}

}  // namespace

// Surface-based connection rules, extracted from connectionCost for readability.
// Each helper accumulates the `surface_bonus +=` contributions of a thematically
// related group of rules and returns their sum. Helpers are self-contained: they
// recompute any needed locals from prev/next and never read caller state. Because
// every contribution is additive, the order among these helpers does not affect the
// total; call sites are kept at their original positions for readability.

// Short pure-hiragana verb false-split penalties after particles/OTHER, plus
// determiner→noun and case-particle→final-particle guards.
float computeParticleDeterminerBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};  // value-init to 0

  // A formal noun followed by a multi-mora case particle is a complete
  // adpositional phrase (こと+に関して). The independent adverbial reading
  // ending in the particle's first mora cannot own that construction, so keep
  // the closed particle attached to its formal-noun host without changing
  // unrelated compound particles such as conditional ったら.
  if (prev.extended_pos == core::ExtendedPOS::NounFormal && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      normalize::utf8Length(next.surface) >= 3) {
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusFormalParticleBinding);
  }

  // An object marker followed by a continuative verb strongly licenses a
  // predicate (本を買いに行く, 本を読み始める). This left-context evidence
  // offsets the general renyokei-before-case-particle nominalization bias while
  // leaving standalone nominal forms such as 読みを/香りを untouched.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && grammar::isAccusativeParticleWoSurface(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.fromDictionary()) {
    SUZUME_CONNECTION_ADD(bonus, cost::kExtraStrongBonus);
  }

  // A multi-mora compound case particle finishes an adpositional phrase; it
  // cannot directly host the aspectual いる. If this
  // sequence is intended as a predicate, the competing analysis retains the
  // internal case-particle + verb-te boundary (目を+通し+て+いる).  Keep this
  // surface-sensitive because single case particles may validly precede an
  // independent verb with either lemma.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && normalize::utf8Length(prev.surface) > 1 &&
      next.lemma == "いる") {
    SUZUME_CONNECTION_ADD(bonus, sc::kPenaltyClosedClassBoundary);
  }

  // Penalty for single-char case particle → very short inflected
  // pure-hiragana verb pattern. Two-kana base forms are deliberately exempt:
  // object + きく/やく/けす is a basic feature-derived verb boundary.
  // The risky false splits here are short stems such as が+おさ.
  // Single-char particles: が, を, に, へ, と, で, から, etc.
  // Only penalize very short verbs (2 chars or less) to avoid affecting なくし, etc.
  // Exception: "い" (いる renyokei) has specific bonus rule below for PART_格→い pattern
  // Generated stems are emitted only after a following inflection has
  // validated the reconstruction. They are therefore not the short
  // unconstrained stems this guard targets (混雑を+さけ+ない/て/た), even
  // though their surfaces are two morae. An irrealis is that condition in its
  // own right: the cell has no independent use, so a candidate carrying it was
  // built because the auxiliary that selects it stands right after (資料を+しら
  // +ない). Naming the ichidan generator alone left its godan sibling out, and
  // the missing exemption pushed the parse into any reading that avoids this
  // edge — a fabricated verb opening on the case particle itself (にしら).
  const bool is_validated_ichidan_inflection = next.extended_pos == core::ExtendedPOS::VerbMizenkei ||
                                               (next.origin == core::CandidateOrigin::VerbHiraganaInflectedRenyokei &&
                                                next.extended_pos == core::ExtendedPOS::VerbRenyokei);
  const bool is_resolved_i_onbin = next.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
                                   next.origin == core::CandidateOrigin::VerbHiragana &&
                                   utf8::endsWith(next.surface, "い");
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      prev.surface.size() <= core::kJapaneseCharBytes &&  // Single hiragana char (3 bytes in UTF-8)
      next.pos == core::PartOfSpeech::Verb && !next.fromDictionary() && grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= 6 &&  // 2 chars or less (6 bytes in UTF-8)
      next.extended_pos != core::ExtendedPOS::VerbShuushikei &&
      next.surface != "い" &&  // Exclude い - has specific rule
      !is_validated_ichidan_inflection && !is_resolved_i_onbin) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Penalty for は (topic) → short pure-hiragana verb pattern
  // E.g., は+し in はしがかかる should be はし (noun), not は+し (topic + する連用形)
  // Only applies to は — other topic particles (も, こそ) naturally precede し
  // (何もしない, 誰もしない are common patterns)
  // Exception: い (renyokei of いる) - valid in ずにはいられない pattern
  // Exception: し (renyokei of する) - valid in emphatic negation ありはしない pattern
  if (prev.extended_pos == core::ExtendedPOS::ParticleTopic && prev.surface == "は" &&
      next.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= 3 &&                       // 1 char only (3 bytes in UTF-8)
      next.surface != "い" &&                           // い+られ is valid (いる potential)
      !grammar::isSuruRenyokeiSurface(next.surface)) {  // し+ない is valid (emphatic negation)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryRare);
  }

  // Penalty for pure-hiragana OTHER → single-char VerbRenyokei
  // E.g., ふんど+し should be ふんどし (one word), not noun+する連用形
  // Pure hiragana unknown sequences split before し/き/etc. are usually wrong
  // Does not apply when prev is a known particle/aux (those have specific EPOS)
  if (prev.pos == core::PartOfSpeech::Other && grammar::isPureHiragana(prev.surface) &&
      prev.surface.size() >= 6 &&                                                          // 2+ hiragana chars
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface.size() <= 3) {  // Single char (し, き, etc.)
    SUZUME_CONNECTION_ADD(bonus, cost::kUncommon);
  }

  // An unknown hiragana fragment cannot directly introduce an onbin verb.
  // Such a path is an over-segmentation of one inflected word (よろこんで),
  // whereas ordinary adverbial modifiers have their own lexical categories.
  if (prev.pos == core::PartOfSpeech::Other && grammar::isPureHiragana(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbOnbinkei) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // Penalty for NOUN → single-hiragana OTHER
  // A single hiragana character classified as OTHER after a kanji NOUN is almost
  // always a misparse: the hiragana should be part of a verb (先+生きのこる) or
  // okurigana (読み+残す), not a standalone unknown token
  // E.g., 先生+き(OTHER) should lose to 先+生きのこる
  // Needs a very high penalty to overcome prefix compound bonus advantages
  if (prev.pos == core::PartOfSpeech::Noun && grammar::containsKanji(prev.surface) &&
      next.pos == core::PartOfSpeech::Other && next.surface.size() == 3 &&  // Single char = 3 bytes UTF-8
      grammar::isPureHiragana(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // The classical determiner 斯かる always needs a preceding particle, topic
  // marker, or clause boundary (は+かかる, として+かかる, かかる事態 at clause
  // start). A bare noun, a duration closing suffix and a degree particle all
  // introduce the intransitive verb 掛かる/罹る instead, taking a duration or
  // quantity host without a case particle (3週間かかる, 三時間ほどかかる), so the
  // homographic determiner cannot fill that slot. The ParticleCase→Determiner
  // bigram penalty covers only noun+particle+かかる (壁にかかる).
  const bool duration_host_before_kakaru = prev.pos == core::PartOfSpeech::Noun ||
                                           (prev.extended_pos == core::ExtendedPOS::Suffix && prev.surface == "間") ||
                                           prev.extended_pos == core::ExtendedPOS::ParticleAdverbial;
  if (duration_host_before_kakaru && grammar::isDurationPredicateKakaru(next.surface) &&
      next.extended_pos == core::ExtendedPOS::Determiner) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A quotative determiner is licensed by the nominal it modifies, and that
  // nominal stands to its right. The bonuses that attach it to a preceding
  // predicate read only the left side, so without this condition the determiner
  // absorbs と+いう in front of anything at all (行く+という+なら+止め+ない).
  // Every continuation that cannot head a noun phrase is the compositional
  // quotation instead: quotative と, the verb いう, and its own continuation
  // (と+いう+なら, と+いう+より). The heads named here are the ones an
  // attributive can select — a nominal, the pro-form の, a further determiner or
  // prefix stacked in front of the noun, and the two adjective forms that are
  // attributive; a continuative or a stem is not a word in this position.
  const bool quotative_determiner_head =
      core::isNounType(next.extended_pos) || core::isPronounType(next.extended_pos) ||
      next.extended_pos == core::ExtendedPOS::ParticleNo || next.pos == core::PartOfSpeech::Determiner ||
      next.pos == core::PartOfSpeech::Prefix || next.extended_pos == core::ExtendedPOS::AdjBasic ||
      next.extended_pos == core::ExtendedPOS::AdjNaAdj;
  // An attributive with nothing to modify is not a possible reading rather than
  // an unlikely one, and the penalty also has to outweigh the closed-class
  // bonus the determiner's own lexical cost already received on the assumption
  // that a head follows it.
  if (prev.extended_pos == core::ExtendedPOS::DeterminerQuotative && !quotative_determiner_head) {
    SUZUME_CONNECTION_ADD(bonus, cost::kProhibitive);
  }

  // Penalty for DET → non-dict single-kanji NOUN
  // The DET→NOUN bigram bonus (-2.5) is too strong for unknown single-kanji tokens,
  // causing splits like こんな+伸+びる instead of こんな+伸びる
  // Valid DET+NOUN patterns (こんな+事, あんな+人) use dict nouns or multi-char nouns
  if (prev.pos == core::PartOfSpeech::Determiner && next.pos == core::PartOfSpeech::Noun && !next.fromDictionary() &&
      grammar::containsKanji(next.surface) && suzume::normalize::utf8Length(next.surface) == 1) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // Penalty for DET → non-dict kanji+hiragana NOUN (nominalized verb pattern)
  // The DET→NOUN bonus (-2.5) makes heuristic candidates like "先生き" (NOUN)
  // too attractive, preventing correct splits like 先+生きのこる
  // Valid DET+NOUN uses dict nouns or pure-kanji nouns; nominalized forms
  // (kanji + 1 trailing hiragana, e.g., 先生き, 出来事み) are rare after DET
  if (prev.pos == core::PartOfSpeech::Determiner && next.pos == core::PartOfSpeech::Noun && !next.fromDictionary()) {
    size_t char_len = suzume::normalize::utf8Length(next.surface);
    if (char_len >= 3 && grammar::containsKanji(next.surface) && !grammar::isAllKanji(next.surface)) {
      // Check whether the final two characters are kanji + hiragana.
      const std::string_view before_last = utf8::dropLastChar(next.surface);
      if (kana::isHiraganaCodepoint(utf8::decodeLastChar(next.surface)) && !before_last.empty() &&
          normalize::isKanjiCodepoint(utf8::decodeLastChar(before_last))) {
        SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
      }
    }
  }

  // とも has exactly two hosts: a counted quantity, where it is the universal
  // quantifier (二人とも), and a negative/volitional auxiliary or an adjective
  // adverbial form, where it is the concessive conjunctive particle (読まずとも,
  // 届こうとも, 少なくとも, 答えなくとも). Both negative auxiliaries fill that
  // slot: the classical ぬ through its 連用形 ず, the modern ない through なく.
  // Anywhere else the surface is the case particle と plus も, or the opening of
  // a longer lexical word (ともだち, もっとも, 何とも).
  //
  // ば/ど/ども are the same kind of rule one particle over: they complete the
  // hypothetical slot of a predicate and have no other host, so a focus particle
  // cannot precede them either, offering no inflected stem. The
  // ParticleAdverbial→ParticleConj bonus is meant for the conditional なら
  // (だけ+なら, ほど+なら) and would otherwise buy the fabricated だけ+ど over the
  // copula plus けど (〜んだけど).
  const bool tomo_particle = next.pos == core::PartOfSpeech::Particle && utf8::equalsAny(next.surface, {"とも"});
  const bool quantifier_host = tomo_particle && prev.origin == core::CandidateOrigin::Counter;
  const bool unlicensed_tomo = tomo_particle && prev.extended_pos != core::ExtendedPOS::AuxNegativeNu &&
                               prev.extended_pos != core::ExtendedPOS::AuxNegativeNai &&
                               prev.extended_pos != core::ExtendedPOS::AuxVolitional &&
                               prev.extended_pos != core::ExtendedPOS::AdjRenyokei;
  // A particle offers no inflected stem, and the passive fills the conditional
  // ば slot with its own cell れれ/られれ.  The concessive ど/ども, however,
  // attaches to the passive continuative itself (書か+れ+ども), just like
  // ながら; do not reject that productive classical construction.
  const bool unlicensed_hypothetical_host =
      prev.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
      prev.extended_pos == core::ExtendedPOS::ParticleBinding ||
      (prev.extended_pos == core::ExtendedPOS::AuxPassive && !grammar::spellsHypotheticalAuxiliaryCell(prev.surface) &&
       utf8::equalsAny(next.surface, {"ば"}));
  const bool unlicensed_hypothetical = next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                       grammar::isHypotheticalSelectingConjunctiveParticle(next.surface) &&
                                       unlicensed_hypothetical_host;
  if (quantifier_host || unlicensed_tomo || unlicensed_hypothetical) {
    SUZUME_CONNECTION_ADD(bonus, quantifier_host ? cost::kExtremeBonus : cost::kAlmostNever);
  }

  return bonus;
}

// A compound particle built on a predicate continuative still hosts the polite
// auxiliary (に関し+まし+て, につき+まし+て). The POS-level bigram bars every
// particle from that slot because a bare particle offers no stem for ます, and
// the homographic readings of まし — the na-adjective and the opening of a
// ましい-derived adjective — are what a bare particle actually selects. The
// continuative row it ends in is the distinguishing evidence, and a bare
// particle is a single mora that never carries it.
float computeCompoundParticlePoliteBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  if (next.extended_pos != core::ExtendedPOS::AuxTenseMasu || prev.extended_pos != core::ExtendedPOS::ParticleCase ||
      normalize::utf8Length(prev.surface) < 3 || !grammar::isIRowCodepoint(utf8::decodeLastChar(prev.surface))) {
    return cost::kNeutral;
  }
  return -cost::kSevere;
}

// The copula である is the auxiliary で plus ある, so a conjunctive particle that
// merely ends in で cannot govern ある: its で would have to be the particle's
// own tail and the copula at once. The reading that fits is the formal noun
// plus the copula (確認したのであって = の + で + あっ + て). A bare で is
// exempt because there it really is the copula.
float computeConjunctiveParticleCopulaPenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // Match the cells of ある by surface as well as by lemma: when this edge is
  // barred the analyzer falls back to an unknown token over the same span, and
  // a lemma-only test would let that fallback carry the very path being ruled
  // out.
  const bool governs_aru =
      next.lemma == "ある" || utf8::equalsAny(next.surface, {"ある", "あっ", "あり", "あれ", "あろ", "あら"});
  if (prev.extended_pos != core::ExtendedPOS::ParticleConj || normalize::utf8Length(prev.surface) < 2 ||
      !utf8::endsWith(prev.surface, "で") || !governs_aru) {
    return cost::kNeutral;
  }
  return cost::kProhibitive;
}

// An adverb ending in the adverbializing に opens the clause it modifies, so a
// finished predicate cannot introduce one: without a boundary marker the same
// spelling is a nominal plus the case particle, which is what an attributive or
// a past auxiliary actually selects (確認した+ことに+なる is 確認した+こと+に+なる,
// while clause-initial ことに夜風が冷たかった keeps the adverb).
float computeAdverbialNiAfterPredicatePenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  const bool finished_predicate = prev.extended_pos == core::ExtendedPOS::VerbShuushikei ||
                                  prev.extended_pos == core::ExtendedPOS::AuxTenseTa ||
                                  prev.extended_pos == core::ExtendedPOS::AdjBasic;
  if (!finished_predicate || next.pos != core::PartOfSpeech::Adverb || !next.fromDictionary() ||
      !utf8::endsWith(next.surface, "に")) {
    return cost::kNeutral;
  }
  return cost::kProhibitive;
}

// Prefix/adverb→short-verb, symbol→particle/aux/furigana, and で+も copula rules.
float computePrefixSymbolBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};  // value-init to 0

  // Penalty for PREFIX → short pure-hiragana verb pattern
  // E.g., お+い in において should not happen (お is prefix, い is not a verb here)
  // Valid お+verb patterns: お待ち, お願い (longer, often with kanji)
  // A closed-class honorific verb is the only dictionary-confirmed exception
  // to this short-prefix guard (お+はす). Other one- or two-mora hypotheses,
  // including the L1 い stem, remain too ambiguous to license directly.
  const bool is_honorific_prefix_verb = prev.extended_pos == core::ExtendedPOS::Prefix &&
                                        grammar::isHonorificPrefix(prev.surface) && next.fromDictionary() &&
                                        grammar::isHumbleHonorificLemma(next.lemma);
  if (prev.pos == core::PartOfSpeech::Prefix && next.pos == core::PartOfSpeech::Verb && !is_honorific_prefix_verb &&
      grammar::isPureHiragana(next.surface) && next.surface.size() <= 6) {  // 2 chars or less
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Penalty for PREFIX → non-dictionary pure-hiragana verb pattern (3 chars)
  // E.g., お+はよう in おはよう - はよう is not a real verb
  // Valid patterns like お+待ち have kanji, お+召し would be in dictionary
  if (prev.pos == core::PartOfSpeech::Prefix && next.pos == core::PartOfSpeech::Verb && !next.fromDictionary() &&
      grammar::isPureHiragana(next.surface) && next.surface.size() == 9) {  // Exactly 3 chars (9 bytes)
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Penalty for ADV → short pure-hiragana verb renyokei pattern
  // E.g., はなはだ+し should not happen (はなはだしい is an adjective)
  // Valid ADV+verb patterns: ゆっくり+歩く (verb is longer/has kanji)
  // This prevents split like はなはだ+し+い when はなはだしい exists in dict
  // Exception: dictionary verbs like ね(寝る), み(見る), で(出る) are valid, and
  // so is the カ変 continuative. Its one mora is a cell of a closed irregular
  // paradigm rather than a stem hypothesized from the adverb's tail, so it
  // carries the same lexical weight as a listed entry (ぐっと+き+た).
  if (prev.pos == core::PartOfSpeech::Adverb && isSingleHiraganaVerbRenyokei(next) && !next.fromDictionary() &&
      next.conj_type != dictionary::ConjugationType::Kuru) {
    // This rule formerly contributed kVeryRare in two call sites. Preserve
    // that effective magnitude while owning the rule here only.
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryRare + cost::kVeryRare);
  }

  // Penalty for opening bracket → PARTICLE pattern (furigana in parentheses).
  // E.g., 東京（とうきょう） should not split と+う+きょう. Punctuation
  // and closing brackets are clause boundaries and may legitimately be
  // followed by a particle, so they must not receive this penalty.
  if (prev.pos == core::PartOfSpeech::Symbol && next.pos == core::PartOfSpeech::Particle &&
      normalize::isOpeningBracket(firstCodepoint(prev.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Bonus for SYMBOL → long pure-hiragana OTHER (furigana pattern)
  // E.g., 東京（とうきょう） - the hiragana in parentheses is reading/furigana
  // Long hiragana sequences after symbols should stay as single tokens
  if (prev.pos == core::PartOfSpeech::Symbol && next.pos == core::PartOfSpeech::Other &&
      grammar::isPureHiragana(next.surface) && next.surface.size() >= 12) {  // 4+ chars (12 bytes in UTF-8)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Penalty for SYMBOL → short hiragana → AUX pattern (furigana), gated to an
  // opening bracket only. Emoji, closing brackets, and other symbols are a soft
  // boundary that still license a following copula: 天気😀です, 犬🐕でした,
  // 本(重要)です, 評価◎です must keep です/でした whole rather than splitting で|す.
  if (prev.pos == core::PartOfSpeech::Symbol && next.pos == core::PartOfSpeech::Auxiliary &&
      normalize::isOpeningBracket(firstCodepoint(prev.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryRare);
  }

  // Penalty for AuxCopulaDa(で) + ParticleTopic(も) pattern
  // This prevents 雨+で+も split when 雨+でも (副助詞) is correct
  // But allows 何+で+も split (で=copula連用形, も=係助詞)
  // The difference: 何(Pronoun) vs 雨(Noun) - Pronoun should split
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic && next.surface == "も") {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryRare);
  }

  return bonus;
}

}  // namespace suzume::analysis::connection_rules
